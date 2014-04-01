/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.hadoop.raid;

import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Map;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.regex.Pattern;
import java.util.Random;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.channels.SocketChannel;
import java.util.concurrent.*;

import org.apache.hadoop.util.DataChecksum;
import org.apache.hadoop.hdfs.DistributedFileSystem;
import org.apache.hadoop.hdfs.DistributedRaidFileSystem;
import org.apache.hadoop.hdfs.protocol.Block;
import org.apache.hadoop.hdfs.protocol.DataTransferProtocol;
import org.apache.hadoop.hdfs.protocol.DatanodeInfo;
import org.apache.hadoop.hdfs.protocol.FSConstants;
import org.apache.hadoop.hdfs.protocol.FSConstants.DatanodeReportType;
import org.apache.hadoop.hdfs.protocol.LocatedBlock;
import org.apache.hadoop.hdfs.protocol.LocatedBlocks;
import org.apache.hadoop.hdfs.server.common.HdfsConstants;
import org.apache.hadoop.hdfs.server.datanode.FSDataset;
import org.apache.hadoop.hdfs.server.datanode.RaidBlockSender;
import org.apache.hadoop.io.Text;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.conf.Configured;
import org.apache.hadoop.fs.ChecksumException;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.hdfs.BlockMissingException;
import org.apache.hadoop.hdfs.RaidDFSUtil;
import org.apache.hadoop.util.StringUtils;
import org.apache.hadoop.net.NetUtils;

import org.apache.hadoop.raid.RaidNode;
import org.apache.hadoop.raid.RaidUtils;
import org.apache.hadoop.raid.protocol.PolicyInfo.ErasureCodeType;
import org.apache.hadoop.hdfs.protocol.DatanodeID;


/**
 * contains the core functionality of the block fixer
 *
 * raid.blockfix.interval          - interval between checks for corrupt files
 *
 * raid.blockfix.history.interval  - interval before fixing same file again
 *
 * raid.blockfix.read.timeout      - read time out
 *
 * raid.blockfix.write.timeout     - write time out
 */
public class BlockFixer extends Configured implements Runnable {
  public static final Log LOG = LogFactory.getLog(
                                  "org.apache.hadoop.raid.BlockFixer");

  public static final String BLOCKFIX_INTERVAL = "raid.blockfix.interval";
  public static final String BLOCKFIX_HISTORY_INTERVAL =
    "raid.blockfix.history.interval";
  public static final String BLOCKFIX_READ_TIMEOUT =
    "raid.blockfix.read.timeout";
  public static final String BLOCKFIX_WRITE_TIMEOUT =
    "raid.blockfix.write.timeout";

  public static final long DEFAULT_BLOCKFIX_INTERVAL = 60 * 1000; // 1 min
  public static final long DEFAULT_BLOCKFIX_HISTORY_INTERVAL =
    60 * 60 * 1000; // 60 mins

  private java.util.HashMap<String, java.util.Date> history;
  private long numFilesFixed = 0;

  private String pmPrefix; // the destination path of the pm parity
  private Decoder pmDecoder;
  private Encoder pmEncoder;

  private String iaPrefix; // the destination path of the ia parity
  private Decoder iaDecoder;
  private Encoder iaEncoder;

  private String jrsPrefix; // the destination path of the jrs parity
  private Decoder jrsDecoder;
  private Encoder jrsEncoder;

  // This queue is used to store the information of recovered block that is waiting for sending to the datanode
  private BlockingQueue<PendingBlockInfo> q = new ArrayBlockingQueue<PendingBlockInfo>(20);
  
  int stripeLength;
  int parityLength;

  // interval between checks for corrupt files
  protected long blockFixInterval = DEFAULT_BLOCKFIX_INTERVAL;

  // interval before fixing same file again
  protected long historyInterval = DEFAULT_BLOCKFIX_HISTORY_INTERVAL;

  public volatile boolean running = true;


  public BlockFixer(Configuration conf) throws IOException {
    super(conf);
    history = new java.util.HashMap<String, java.util.Date>();
    blockFixInterval = getConf().getInt(BLOCKFIX_INTERVAL,
                                   (int) blockFixInterval);
    
    stripeLength = RaidNode.getStripeLength(getConf());
    
    /* potentail bug here */
    parityLength = RaidNode.pmParityLength(getConf());
    
    pmPrefix = RaidNode.pmDestinationPath(getConf()).toUri().getPath();
    if (!pmPrefix.endsWith(Path.SEPARATOR)) {
      pmPrefix += Path.SEPARATOR;
    }
    pmDecoder = new PMDecoder(getConf(), stripeLength, parityLength, true);
    pmEncoder = new PMEncoder(getConf(), stripeLength, parityLength);
    
    iaPrefix = RaidNode.iaDestinationPath(getConf()).toUri().getPath();
    if (!iaPrefix.endsWith(Path.SEPARATOR)) {
      iaPrefix += Path.SEPARATOR;
    }
    iaDecoder = new IADecoder(getConf(), stripeLength, parityLength, true);
    iaEncoder = new IAEncoder(getConf(), stripeLength, parityLength);

    jrsPrefix = RaidNode.jrsDestinationPath(getConf()).toUri().getPath();
    if (!jrsPrefix.endsWith(Path.SEPARATOR)) {
      jrsPrefix += Path.SEPARATOR;
    }
    jrsDecoder = new JRSDecoder(getConf(), stripeLength, parityLength, true);
    
    jrsEncoder = new JRSEncoder(getConf(), stripeLength, parityLength);

    DataSender ds = new DataSender(q);
    Thread dst = new Thread(ds);
    dst.start();

  }

  public void run() {
    while (running) {
      try {
        LOG.info("BlockFixer continuing to run...");
        doFix();
      } catch (Exception e) {
        LOG.error(StringUtils.stringifyException(e));
      } catch (Error err) {
        LOG.error("Exiting after encountering " +
                    StringUtils.stringifyException(err));
        throw err;
      }
    }
  }

  public long filesFixed() {
    return numFilesFixed;
  }

  void doFix() throws InterruptedException, IOException {
    LOG.info("in BLockFixer doFix");
    while (running) {
      // Sleep before proceeding to fix files.
      Thread.sleep(blockFixInterval);
      // Purge history older than the history interval.
      purgeHistory();


      // get those files that were corrupted
      List<Path> corruptFiles = getCorruptFiles();
      
      if (corruptFiles.isEmpty()) {
        LOG.info("corruptFiles is empty");
        // If there are no corrupt files, retry after some time.
        continue;
      }
      LOG.info("Found " + corruptFiles.size() + " corrupt files.");

      // put source at the front
      // maybe we don't need to do this
      sortCorruptFiles(corruptFiles);

      // pair up source corrupted path and corrupted parity path
      HashMap<String, ArrayList<Path> > corruptGroups = groupPath(corruptFiles);

      LOG.info("Corrupt's group size: "+corruptGroups.size());

      // fix corrupted file one by one
      for (String pName: corruptGroups.keySet()) {
        LOG.info("process: "+pName);
        if (!running) break;
        try {
          fixFile(corruptGroups.get(pName));
        } catch (IOException ie) {
          LOG.error("Hit error while processing " + pName +
            ": " + StringUtils.stringifyException(ie));
          // Do nothing, move on to the next file.
        }
      }

    }
  }

  /**
   * pair source file and parity file
   */
  private HashMap<String, ArrayList<Path> > groupPath(List<Path> corruptFiles){
    HashMap<String, ArrayList<Path> > ret = new HashMap<String, ArrayList<Path> >();
    for(Path srcPath: corruptFiles){
      // talk the path of the source file as a key
      Path keyPath = srcPath;
      if (RaidNode.isParityHarPartFile(srcPath)) {
        ;//do nothing, har parity is handled separately
      }else if(isPmParityFile(srcPath) || isJrsParityFile(srcPath) || isIaParityFile(srcPath)) {
        keyPath = sourcePathFromParityPath(srcPath);
      }
      String name = keyPath.getName();
      ArrayList<Path> pl = ret.get(name);
      if(pl == null){
        pl = new ArrayList<Path>();
        pl.add(srcPath);
        ret.put(name, pl);
      }else{
        pl.add(srcPath);
      }
    }
    return ret;
  }

  /**
   * fix one pair of files(There may be only one file corrupted)
   * this function acts as a dispatch, it forward the job to the right
   * handler.
   */
  void fixFile(ArrayList< Path > srcPathList) throws IOException {

    // handle har parity file
    for(Path p:srcPathList)
      if(RaidNode.isParityHarPartFile(p)) {
        processCorruptParityHarPartFile(p);
        return;
      }

    Path src = null;
    Path parity = null;

    for(Path p:srcPathList){
      if(isPmParityFile(p) || isJrsParityFile(p) || isIaParityFile(p))
        parity = p;
      else
        src = p;
    }

    // find those 'miss' path
    if(src == null)
      src = sourcePathFromParityPath(parity);
    if(src == null)
      LOG.info("src is null, path ");
    else
      LOG.info("source:"+src);
    if(parity != null)
      LOG.info("parity: "+parity);
    if(RaidNode.pmParityForSource(src, getConf())==null)
      LOG.info("source for pm is null");
    if(parity == null){
      RaidNode.ParityFilePair pair = RaidNode.pmParityForSource(src, getConf());
      if(pair != null) parity = pair.getPath();
    }
    if(parity == null){
      RaidNode.ParityFilePair pair = RaidNode.jrsParityForSource(src, getConf());
      if(pair != null) parity = pair.getPath();
    }
    if(parity == null){
      RaidNode.ParityFilePair pair = RaidNode.iaParityForSource(src, getConf());
      if(pair != null) parity = pair.getPath();
    }
    //RaidNode.ParityFilePair ppair = RaidNode.pmParityForSource(srcPath, getConf());

    // If we have a parity file, process the file and fix it.
    //if (ppair != null) {
    //  processCorruptFile(srcPath, ppair, pmDecoder);
    //}
    if(isJrsParityFile(parity))
      processCorruptFile(src, parity, jrsDecoder);
    if(isPmParityFile(parity))
      processCorruptFile(src, parity, pmDecoder);
    if(isIaParityFile(parity))
      processCorruptFile(src, parity, iaDecoder);

  }

  /**
   * We maintain history of fixed files because a fixed file may appear in
   * the list of corrupt files if we loop around too quickly.
   * This function removes the old items in the history so that we can
   * recognize files that have actually become corrupt since being fixed.
   */
  void purgeHistory() {
    // Default history interval is 1 hour.
    long historyInterval = getConf().getLong(
                             BLOCKFIX_HISTORY_INTERVAL, 3600*1000);
    java.util.Date cutOff = new java.util.Date(
                                   System.currentTimeMillis()-historyInterval);
    List<String> toRemove = new java.util.ArrayList<String>();

    for (String key: history.keySet()) {
      java.util.Date item = history.get(key);
      if (item.before(cutOff)) {
        toRemove.add(key);
      }
    }
    for (String key: toRemove) {
      LOG.info("Removing " + key + " from history");
      history.remove(key);
    }
  }

  /**
   * @return A list of corrupt files as obtained from the namenode
   */
  List<Path> getCorruptFiles() throws IOException {
    DistributedFileSystem dfs = getDFS(new Path("/"));
    //LOG.info("http server: "+getConf().get("dfs.http.address", "empty"));
    String[] nnCorruptFiles = RaidDFSUtil.getCorruptFiles(getConf());
    List<Path> corruptFiles = new LinkedList<Path>();
    for (String file: nnCorruptFiles) {
      if (!history.containsKey(file)) {
        corruptFiles.add(new Path(file));
      }
    }
    RaidUtils.filterTrash(getConf(), corruptFiles);
    return corruptFiles;
  }

  /**
   * Sorts source files ahead of parity files.
   */
  void sortCorruptFiles(List<Path> files) {
    // TODO: We should first fix the files that lose more blocks
    Comparator<Path> comp = new Comparator<Path>() {
      public int compare(Path p1, Path p2) {
        if (isPmParityFile(p2) || isJrsParityFile(p2) || isIaParityFile(p2)) {
          // If p2 is a parity file, p1 is smaller.
          return -1;
        }
        if (isPmParityFile(p1) || isJrsParityFile(p1) || isIaParityFile(p1)) {
          // If p1 is a parity file, p2 is smaller.
          return 1;
        }
        // If both are source files, they are equal.
        return 0;
      }
    };
    Collections.sort(files, comp);
  }

  /**
   * Reads through a corrupt source file fixing corrupt blocks on the way.
   * @param srcPath Path identifying the corrupt source file.
   * @param parityPath Path identifying the corrupt parity file.
   * @param decoder Decoder
   * @throws IOException
   */
  void processCorruptFile(Path srcPath, Path parityPath,
      Decoder decoder) throws IOException {
    LOG.info("Processing corrupt file " + srcPath);

    // map to group corrupt block into stripes
    Map<Integer, Map<Integer, LocatedBlock> > corrupt =
      new HashMap<Integer, Map<Integer, LocatedBlock> >();

    // map to note down the location of blocks
    Map<Integer, List<DatanodeInfo> > locations = 
      new HashMap<Integer, List<DatanodeInfo> >();
    
    DistributedFileSystem srcFs = getDFS(srcPath);
    FileStatus srcStat = srcFs.getFileStatus(srcPath);
    long blockSize = srcStat.getBlockSize();
    long srcFileSize = srcStat.getLen();
    String uriPath = srcPath.toUri().getPath();

    LOG.info("Location size 1:"+locations.size());
    // get source file's corrupt blocks
    LocatedBlocks locatedBlocks =
      RaidDFSUtil.getBlockLocations(srcFs, uriPath, 0, srcFileSize);
    int size = locatedBlocks.getLocatedBlocks().size();
    LOG.info("source file size:"+size);

    RaidDFSUtil.corruptBlocksInFile(srcFs, uriPath, 0, srcFileSize,
        stripeLength, 0, corrupt, locations);

    LOG.info("Location size 2:"+locations.size());

    DistributedFileSystem parityFs = getDFS(parityPath);
    FileStatus parityStat = parityFs.getFileStatus(parityPath);
    long parityFileSize = parityStat.getLen();
    String parityUriPath = parityPath.toUri().getPath();

    // get parity file's corrupt blocks
    RaidDFSUtil.corruptBlocksInFile(parityFs, parityUriPath, 0, parityFileSize,
        parityLength, stripeLength, corrupt, locations);
    LOG.info("Location size 3:"+locations.size());

    int numBlocksFixed = 0;
    for (Integer stripeIdx: corrupt.keySet()) {
      // map store the info of corrupt block in current stripe
      Map<Integer, LocatedBlock> corruptStripe = corrupt.get(stripeIdx);


      File[] localBlockFiles = new File[corruptStripe.size()];
      int idx = 0;
      for(Integer blockIdx: corruptStripe.keySet()){
        Block corruptBlock = corruptStripe.get(blockIdx).getBlock();
        localBlockFiles[idx] = 
          File.createTempFile(corruptBlock.getBlockName(), ".tmp");
        localBlockFiles[idx++].deleteOnExit();
      }

      //try { // useless try
      // employ the decoder to recovery the stripe
      LOG.info("start decoder.recoverStripeToFile");
        decoder.recoverStripeToFile(srcFs, srcPath, parityFs,
          parityPath, blockSize,  corruptStripe, localBlockFiles, stripeIdx);

        // We have a the contents of the block, send them.
        idx = 0;
        for(Integer blockIdx: corruptStripe.keySet()){
          LocatedBlock lb = corruptStripe.get(blockIdx);
          DatanodeInfo datanode = null;
          // choose a data node to send the recovered block
          // try the best to avoid those has hold his stripe mate
          try{
            DatanodeInfo[] tmp = new DatanodeInfo[locations.get(stripeIdx).size()];
            datanode = chooseDatanode((DatanodeInfo[])(locations.get(stripeIdx).toArray(tmp)));
          }catch(IOException e){
            datanode = chooseDatanode(lb.getLocations());
          }
          File f = localBlockFiles[idx++];
          //computeMetdataAndSendFixedBlock(
          //  datanode, f, lb, f.length());
          try{
            LOG.info("put pending block");
            q.put(new PendingBlockInfo(datanode, f, lb));
          }catch(InterruptedException e){
          } 
          numBlocksFixed++;
        }
        LOG.info("Adding " + srcPath + " to history");
        history.put(srcPath.toString(), new java.util.Date());
      //} finally { // apply another design
      //  for(int i = 0; i < localBlockFiles.length; i++)
      //    localBlockFiles[i].delete();
      //}
    }
    LOG.info("Fixed " + numBlocksFixed + " blocks in " + srcPath + " Recovery Task Done");
    // not accurate
    numFilesFixed++;
  }

  class PendingBlockInfo{

    public DatanodeInfo di;
    public File f;
    public LocatedBlock lb;

    PendingBlockInfo(DatanodeInfo di, File f, LocatedBlock lb){
      this.f = f;
      this.di = di;
      this.lb = lb;
    }
  }
  /*
   * Class that takes charged of sending coding data
   */
  class DataSender implements Runnable{
    //signal queue
    BlockingQueue<PendingBlockInfo> q;

    DataSender(BlockingQueue<PendingBlockInfo> q){
      this.q = q;
    }

    public void run(){
      while(running){
        PendingBlockInfo info = null;
        try{
          info = q.take();
        }catch(InterruptedException e){
        }
        if(info != null){
          try{
            LOG.info("about to send fix block, block length: "+info.f.length());
            
            computeMetadataAndSendFixedBlock(
              info.di, info.f, info.lb, info.f.length());
            
            LOG.info("sent fix block");
          }catch(IOException e){
            LOG.info(e);
          }
        }
        info.f.delete();
      }
    }
  }

  /**
   * checks whether file is jrs parity file
   */
  boolean isJrsParityFile(Path p) {
    String pathStr = p.toUri().getPath();
    if (pathStr.contains(RaidNode.HAR_SUFFIX)) {
      return false;
    }
    return pathStr.startsWith(jrsPrefix);
  }

  /**
   * checks whether file is pm parity file
   */
  boolean isPmParityFile(Path p) {
    String pathStr = p.toUri().getPath();
    if (pathStr.contains(RaidNode.HAR_SUFFIX)) {
      return false;
    }
    return pathStr.startsWith(pmPrefix);
  }

  /**
   * checks whether file is ia parity file
   */
  boolean isIaParityFile(Path p) {
    String pathStr = p.toUri().getPath();
    if (pathStr.contains(RaidNode.HAR_SUFFIX)) {
      return false;
    }
    return pathStr.startsWith(iaPrefix);
  }

  /**
   * Returns a DistributedFileSystem hosting the path supplied.
   */
  protected DistributedFileSystem getDFS(Path p) throws IOException {
    // return (DistributedFileSystem) p.getFileSystem(getConf());
    return (DistributedFileSystem) ((DistributedRaidFileSystem)p.getFileSystem(getConf())).getFileSystem();
  }

  /**
   * Choose a datanode (hostname:portnumber). The datanode is chosen at
   * random from the live datanodes.
   * @param locationsToAvoid locations to avoid.
   * @return A string in the format name:port.
   * @throws IOException
   */
  private DatanodeInfo chooseDatanode(DatanodeInfo[] locationsToAvoid)
    throws IOException {
    DistributedFileSystem dfs = getDFS(new Path("/"));
    DatanodeInfo[] live = dfs.getClient().datanodeReport(
                                                 DatanodeReportType.LIVE);
    LOG.info("Choosing a datanode from " + live.length +
      " live nodes while avoiding " + locationsToAvoid.length);
    Random rand = new Random(54321);
    DatanodeInfo chosen = null;
    int maxAttempts = 1000;
    for (int i = 0; i < maxAttempts && chosen == null; i++) {
      int idx = rand.nextInt(live.length);
      chosen = live[idx];
      for (DatanodeInfo avoid: locationsToAvoid) {
        if (chosen.name.equals(avoid.name)) {
          LOG.info("Avoiding " + avoid.name);
          chosen = null;
          break;
        }
      }
    }
    if (chosen == null) {
      throw new IOException("Could not choose datanode");
    }
    LOG.info("Choosing datanode " + chosen.name);
    return chosen;
  }

  /**
   * Reads data from the data stream provided and computes metadata.
   */
  static DataInputStream computeMetadata(
    Configuration conf, InputStream dataStream) throws IOException {
    ByteArrayOutputStream mdOutBase = new ByteArrayOutputStream(1024*1024);
    DataOutputStream mdOut = new DataOutputStream(mdOutBase);

    // First, write out the version.
    mdOut.writeShort(FSDataset.METADATA_VERSION);

    // Create a summer and write out its header.
    int bytesPerChecksum = conf.getInt("io.bytes.per.checksum", 512);
    DataChecksum sum = DataChecksum.newDataChecksum(
                        DataChecksum.CHECKSUM_CRC32,
                        bytesPerChecksum);
    sum.writeHeader(mdOut);

    // Buffer to read in a chunk of data.
    byte[] buf = new byte[bytesPerChecksum];
    // Buffer to store the checksum bytes.
    byte[] chk = new byte[sum.getChecksumSize()];

    // Read data till we reach the end of the input stream.
    int bytesSinceFlush = 0;
    while (true) {
      // Read some bytes.
      int bytesRead = dataStream.read(
        buf, bytesSinceFlush, bytesPerChecksum-bytesSinceFlush);
      if (bytesRead == -1) {
        if (bytesSinceFlush > 0) {
          boolean reset = true;
          sum.writeValue(chk, 0, reset); // This also resets the sum.
          // Write the checksum to the stream.
          mdOut.write(chk, 0, chk.length);
          bytesSinceFlush = 0;
        }
        break;
      }
      // Update the checksum.
      sum.update(buf, bytesSinceFlush, bytesRead);
      bytesSinceFlush += bytesRead;

      // Flush the checksum if necessary.
      if (bytesSinceFlush == bytesPerChecksum) {
        boolean reset = true;
        sum.writeValue(chk, 0, reset); // This also resets the sum.
        // Write the checksum to the stream.
        mdOut.write(chk, 0, chk.length);
        bytesSinceFlush = 0;
      }
    }

    byte[] mdBytes = mdOutBase.toByteArray();
    return new DataInputStream(new ByteArrayInputStream(mdBytes));
  }

  private void computeMetadataAndSendFixedBlock(
    DatanodeInfo datanode,
    File localBlockFile, LocatedBlock block, long blockSize
    ) throws IOException {

    LOG.info("Computing metdata");
    InputStream blockContents = null;
    DataInputStream blockMetadata = null;
    try {
      blockContents = new FileInputStream(localBlockFile);
      blockMetadata = computeMetadata(getConf(), blockContents);
      blockContents.close();
      // Reopen
      blockContents = new FileInputStream(localBlockFile);
      sendFixedBlock(datanode, blockContents, blockMetadata, block, blockSize);
    } finally {
      if (blockContents != null) {
        blockContents.close();
        blockContents = null;
      }
      if (blockMetadata != null) {
        blockMetadata.close();
        blockMetadata = null;
      }
    }
  }

  /**
   * Send a generated block to a datanode.
   * @param datanode Chosen datanode name in host:port form.
   * @param blockContents Stream with the block contents.
   * @param corruptBlock Block identifying the block to be sent.
   * @param blockSize size of the block.
   * @throws IOException
   */
  private void sendFixedBlock(
    DatanodeInfo datanode,
    final InputStream blockContents, DataInputStream metadataIn,
    LocatedBlock block, long blockSize
    ) throws IOException {
    InetSocketAddress target = NetUtils.createSocketAddr(datanode.name);
    Socket sock = SocketChannel.open().socket();

    int readTimeout = getConf().getInt(BLOCKFIX_READ_TIMEOUT,
      HdfsConstants.READ_TIMEOUT);
    NetUtils.connect(sock, target, readTimeout);
    sock.setSoTimeout(readTimeout);

    int writeTimeout = getConf().getInt(BLOCKFIX_WRITE_TIMEOUT,
      HdfsConstants.WRITE_TIMEOUT);

    OutputStream baseStream = NetUtils.getOutputStream(sock, writeTimeout);
    DataOutputStream out = new DataOutputStream(
        new BufferedOutputStream(baseStream, FSConstants.SMALL_BUFFER_SIZE));

    boolean corruptChecksumOk = false;
    boolean chunkOffsetOK = false;
    boolean verifyChecksum = true;
    boolean transferToAllowed = false;

    try {
      LOG.info("Sending block " + block.getBlock() +
          " from " + sock.getLocalSocketAddress().toString() +
          " to " + sock.getRemoteSocketAddress().toString() +
          " " + blockSize + " bytes");
      RaidBlockSender blockSender = new RaidBlockSender(
          block.getBlock(), blockSize, 0, blockSize,
          corruptChecksumOk, chunkOffsetOK, verifyChecksum, transferToAllowed,
          metadataIn, new RaidBlockSender.InputStreamFactory() {
          @Override
          public InputStream createStream(long offset) throws IOException {
            // we are passing 0 as the offset above, so we can safely ignore
            // the offset passed
            return blockContents;
          }
        });

      DatanodeInfo[] nodes = new DatanodeInfo[]{datanode};
      DataTransferProtocol.Sender.opWriteBlock(
        out, block.getBlock(), 1,
        DataTransferProtocol.BlockConstructionStage.PIPELINE_SETUP_CREATE,
        0, blockSize, 0, "", null, nodes, block.getBlockToken());
      LOG.info("anchor Decode_stripe "+block.getBlock().getBlockId()+" Data_sending "+System.nanoTime());
      blockSender.sendBlock(out, baseStream);
      LOG.info("anchor Decode_stripe "+block.getBlock().getBlockId()+" Data_sent "+System.nanoTime());

      LOG.info("Sent block " + block.getBlock() + " to " + datanode.name);
    } finally {
      out.close();
    }
  }

  /**
   * returns the source file corresponding to a parity file
   */
  Path sourcePathFromParityPath(Path parityPath) {
    String parityPathStr = parityPath.toUri().getPath();
    if (parityPathStr.startsWith(jrsPrefix)) {
      // Remove the prefix to get the source file.
      String src = parityPathStr.replaceFirst(jrsPrefix, "/");
      return new Path(src);
    }
    if (parityPathStr.startsWith(pmPrefix)) {
      // Remove the prefix to get the source file.
      String src = parityPathStr.replaceFirst(pmPrefix, "/");
      return new Path(src);
    }
    if (parityPathStr.startsWith(iaPrefix)) {
      // Remove the prefix to get the source file.
      String src = parityPathStr.replaceFirst(iaPrefix, "/");
      return new Path(src);
    }
    return null;
  }

  /**
   * Reads through a parity HAR part file, fixing corrupt blocks on the way.
   * A HAR block can contain many file blocks, as long as the HAR part file
   * block size is a multiple of the file block size.
   * @return true if file has been fixed, false if no fixing 
   * was necessary or possible.
   */
  boolean processCorruptParityHarPartFile(Path partFile)
    throws IOException {
    LOG.info("Processing corrupt file " + partFile);
    // Get some basic information.
    DistributedFileSystem dfs = getDFS(partFile);
    FileStatus partFileStat = dfs.getFileStatus(partFile);
    long partFileSize = partFileStat.getLen();
    long partFileBlockSize = partFileStat.getBlockSize();
    LOG.info(partFile + " has block size " + partFileBlockSize);

    // Find the path to the index file.
    // Parity file HARs are only one level deep, so the index files is at the
    // same level as the part file.
    String harDirectory = partFile.toUri().getPath(); // Temporarily.
    harDirectory =
      harDirectory.substring(0, harDirectory.lastIndexOf(Path.SEPARATOR));
    Path indexFile = new Path(harDirectory + "/" + HarIndex.indexFileName);
    FileStatus indexStat = dfs.getFileStatus(indexFile);
    // Parses through the HAR index file.
    HarIndex harIndex = new HarIndex(dfs.open(indexFile), indexStat.getLen());

    String uriPath = partFile.toUri().getPath();
    int numBlocksFixed = 0;
    List<LocatedBlock> corrupt =
      RaidDFSUtil.corruptBlocksInFile(dfs, uriPath, 0, partFileSize);
    if (corrupt.size() == 0) {
      return false;
    }
    for (LocatedBlock lb: corrupt) {
      Block corruptBlock = lb.getBlock();
      long corruptOffset = lb.getStartOffset();

      File localBlockFile =
        File.createTempFile(corruptBlock.getBlockName(), ".tmp");
      localBlockFile.deleteOnExit();
      processCorruptParityHarPartBlock(dfs, partFile, corruptBlock,
          corruptOffset, partFileStat, harIndex,
          localBlockFile);
      // Now we have recovered the part file block locally, send it.
      try {
        DatanodeInfo datanode = chooseDatanode(lb.getLocations());
        computeMetadataAndSendFixedBlock(datanode, localBlockFile,
            lb, localBlockFile.length());
        numBlocksFixed++;
      } finally {
        localBlockFile.delete();
      }
    }
    LOG.info("Fixed " + numBlocksFixed + " blocks in " + partFile);
    return true;
  }

  /**
   * This fixes a single part file block by recovering in sequence each
   * parity block in the part file block.
   */
  private void processCorruptParityHarPartBlock(FileSystem dfs, Path partFile,
      Block corruptBlock,
      long corruptOffset,
      FileStatus partFileStat,
      HarIndex harIndex,
      File localBlockFile)
    throws IOException {
    String partName = partFile.toUri().getPath(); // Temporarily.
    partName = partName.substring(1 + partName.lastIndexOf(Path.SEPARATOR));

    OutputStream out = new FileOutputStream(localBlockFile);

    try {
      // A HAR part file block could map to several parity files. We need to
      // use all of them to recover this block.
      final long corruptEnd = Math.min(corruptOffset +
          partFileStat.getBlockSize(),
          partFileStat.getLen());
      for (long offset = corruptOffset; offset < corruptEnd; ) {
        HarIndex.IndexEntry entry = harIndex.findEntry(partName, offset);
        if (entry == null) {
          String msg = "Corrupt index file has no matching index entry for " +
            partName + ":" + offset;
          LOG.warn(msg);
          throw new IOException(msg);
        }
        Path parityFile = new Path(entry.fileName);
        Encoder encoder;
        if (isPmParityFile(parityFile)) {
          encoder = pmEncoder;
        } else if (isJrsParityFile(parityFile)) {
          encoder = jrsEncoder;
        } else if (isIaParityFile(parityFile)){
          encoder = iaEncoder;
        } else {
          String msg = "Could not figure out parity file correctly";
          LOG.warn(msg);
          throw new IOException(msg);
        }
        Path srcFile = sourcePathFromParityPath(parityFile);
        FileStatus srcStat = dfs.getFileStatus(srcFile);
        if (srcStat.getModificationTime() != entry.mtime) {
          String msg = "Modification times of " + parityFile + " and " +
            srcFile + " do not match.";
          LOG.warn(msg);
          throw new IOException(msg);
        }
        long corruptOffsetInParity = offset - entry.startOffset;
        LOG.info(partFile + ":" + offset + " maps to " +
            parityFile + ":" + corruptOffsetInParity +
            " and will be recovered from " + srcFile);
        encoder.recoverParityBlockToStream(dfs, srcFile, srcStat.getLen(),
            srcStat.getBlockSize(), parityFile,
            corruptOffsetInParity, out);
        // Finished recovery of one parity block. Since a parity block has the
        // same size as a source block, we can move offset by source block size.
        offset += srcStat.getBlockSize();
        LOG.info("Recovered " + srcStat.getBlockSize() + " part file bytes ");
        if (offset > corruptEnd) {
          String msg =
            "Recovered block spills across part file blocks. Cannot continue.";
          throw new IOException(msg);
        }
      }
    } finally {
      out.close();
    }
  }


}
