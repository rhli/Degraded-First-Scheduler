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
package org.apache.hadoop.hdfs;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.EOFException;
import java.io.IOException;
import java.io.DataInput;
import java.io.PrintStream;
import java.nio.ByteBuffer;
import java.net.URI;
import java.net.URISyntaxException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Map;
import java.util.List;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.Future;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorCompletionService;
import java.util.concurrent.CompletionService;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ConcurrentHashMap;
import java.util.StringTokenizer;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileInputStream;
import java.util.Queue;
import java.util.LinkedList;
import java.util.ArrayList;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.Collections;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.permission.FsPermission;
import org.apache.hadoop.fs.ChecksumException;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FilterFileSystem;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSInputStream;
import org.apache.hadoop.hdfs.BlockMissingException;
import org.apache.hadoop.fs.LocalFileSystem;
import org.apache.hadoop.fs.LocatedFileStatus;
import org.apache.hadoop.fs.RemoteIterator;
import org.apache.hadoop.fs.PathFilter;
import org.apache.hadoop.hdfs.DistributedFileSystem;
import org.apache.hadoop.hdfs.protocol.LocatedBlocks;
import org.apache.hadoop.hdfs.protocol.LocatedBlock;
import org.apache.hadoop.util.Progressable;
import org.apache.hadoop.util.StringUtils;
import org.apache.hadoop.util.ReflectionUtils;
import org.apache.hadoop.raid.Decoder;
import org.apache.hadoop.raid.protocol.PolicyInfo.ErasureCodeType;
import org.apache.hadoop.raid.RaidNode;
import org.apache.hadoop.raid.PMDecoder;
import org.apache.hadoop.raid.JRSDecoder;
import org.apache.hadoop.hdfs.protocol.DatanodeInfo;

/**
 * This is an implementation of the Hadoop  RAID Filesystem. This FileSystem 
 * wraps an instance of the DistributedFileSystem.
 * If a file is corrupted, this FileSystem uses the parity blocks to 
 * regenerate the bad block.
 */

public class DistributedBDRFileSystem extends FilterFileSystem {
  public static final int SKIP_BUF_SIZE = 2048;
  public static final int MIN_BLOCKS_FOR_RAIDING = 3;
  public static boolean DEBUG = false;
  public static boolean MEASURE = true;

  static {
    System.loadLibrary("decode_crdr");
    System.loadLibrary("decode_basic");
  }

  // these are alternate locations that can be used for read-only access
  Configuration conf;

  DistributedBDRFileSystem() throws IOException {
  }

  DistributedBDRFileSystem(FileSystem fs) throws IOException {
    super(fs);
  }

  /* Initialize a bdr FileSystem
  */
  public void initialize(URI name, Configuration conf) throws IOException {
    this.conf = conf;
    DEBUG = conf.getBoolean("hdfs.bdr.debug", false);
    MEASURE = conf.getBoolean("hdfs.bdr.measure", true);

    if(DEBUG) LOG.info("initial DistributedBDRFileSystem");

    Class<?> clazz = conf.getClass("fs.raid.underlyingfs.impl",
        DistributedFileSystem.class);
    if (clazz == null) {
      throw new IOException("No FileSystem for fs.raid.underlyingfs.impl.");
    }

    this.fs = (FileSystem)ReflectionUtils.newInstance(clazz, null); 
    super.initialize(name, conf);
  }

  /*
   * Returns the underlying filesystem
   */
  public FileSystem getFileSystem() throws IOException {
    return fs;
  }

  @Override
    public FSDataInputStream open(Path f, int bufferSize) throws IOException {
      // We want to use RAID logic only on instance of DFS.
      if (fs instanceof DistributedFileSystem) {
        DistributedFileSystem underlyingDfs = (DistributedFileSystem) fs;
        LocatedBlocks lbs =
          underlyingDfs.dfs.namenode.getBlockLocations(
              f.toUri().getPath(), 0L, Long.MAX_VALUE);
        // Do we have a minimum number of blocks?
        if (lbs.locatedBlockCount() >= MIN_BLOCKS_FOR_RAIDING) {
          // Use underlying filesystem if the file is under construction.
          if (!lbs.isUnderConstruction()) {
            // Use underlying filesystem if file length is 0.
            final long fileSize = getFileSize(lbs);
            if (fileSize > 0) {
              return new ExtFSDataInputStream(conf, this, f,
                  fileSize, getBlockSize(lbs), bufferSize);
            }
          }
        }
      }
      return fs.open(f, bufferSize);
    }

  // Obtain block size given 3 or more blocks
  private static long getBlockSize(LocatedBlocks lbs) throws IOException {
    List<LocatedBlock> locatedBlocks = lbs.getLocatedBlocks();
    if (locatedBlocks.size() < MIN_BLOCKS_FOR_RAIDING) {
      throw new IOException("Too few blocks " + locatedBlocks.size());
    }
    long bs = -1;
    for (LocatedBlock lb: locatedBlocks) {
      if (lb.getBlockSize() > bs) {
        bs = lb.getBlockSize();
      }
    }
    return bs;
  }

  private static long getFileSize(LocatedBlocks lbs) throws IOException {
    List<LocatedBlock> locatedBlocks = lbs.getLocatedBlocks();
    long fileSize = 0;
    for (LocatedBlock lb: locatedBlocks) {
      fileSize += lb.getBlockSize();
    }
    if (fileSize != lbs.getFileLength()) {
      throw new IOException("lbs.getFileLength() " + lbs.getFileLength() +
          " does not match sum of block sizes " + fileSize);
    }
    return fileSize;
  }


  public void close() throws IOException {
    if (fs != null) {
      try {
        fs.close();
      } catch(IOException ie) {
        //this might already be closed, ignore
      }
    }
    super.close();
  }

  /**
   * Layered filesystem input stream. This input stream tries reading
   * from alternate locations if it encounters read errors in the primary location.
   */
  private static class ExtFSDataInputStream extends FSDataInputStream {

    private static class UnderlyingBlock {
      // File that holds this block. Need not be the same as outer file.
      public Path path;
      // Offset within path where this block starts.
      public long actualFileOffset;
      // Offset within the outer file where this block starts.
      public long originalFileOffset;
      // Length of the block (length <= blk sz of outer file).
      public long length;
      public UnderlyingBlock(Path path, long actualFileOffset,
          long originalFileOffset, long length) {
        this.path = path;
        this.actualFileOffset = actualFileOffset;
        this.originalFileOffset = originalFileOffset;
        this.length = length;
      }
    }

    /**
     * Create an input stream that wraps all the reads/positions/seeking.
     */
    private static class ExtFsInputStream extends FSInputStream {

      native void decodeCrdr(int k, int m, int w, double[] costs, int failDisk,
          int startIdx, int len, ByteBuffer downloads, ByteBuffer decodeMatrix);
      native void decodeBasic(int k, int m, int w, double[] costs, int failDisk,
          int startIdx, int len, ByteBuffer downloads, ByteBuffer decodeMatrix);

      // Extents of "good" underlying data that can be read.
      private UnderlyingBlock[] underlyingBlocks;

      // in a file?
      private long currentOffset;

      private FSDataInputStream[] currentStreams;

      private UnderlyingBlock[] currentBlocks;

      private byte[] oneBytebuff = new byte[1];
      private byte[] skipbuf = new byte[SKIP_BUF_SIZE];

      private DistributedBDRFileSystem lfs;

      private boolean localRecovery;

      private FileSystem recoverFs;

      private Path path;

      private final long fileSize;

      private final long blockSize;

      private final int buffersize;

      private final Configuration conf;

      private final int dataLength;
      private final int parityLength;
      private final int stripeLength;
      private final int stripSize;
      private final int stripeSize;
      private final int dataSize;

      private final long initTime = System.nanoTime();

      private Set<Path> recoveredPaths = new HashSet<Path>();

      private final ExecutorService decodeES;
      private final ExecutorService readES;
      private final CompletionService<Void> decodeCS;
      private final CompletionService<Void> readCS;

      ExtFsInputStream(Configuration conf, DistributedBDRFileSystem lfs,
          Path path, long fileSize, long blockSize,
          int buffersize) throws IOException {
        if(DEBUG) LOG.info("initial ExtFsInputStream");

        //this.dataLength = RaidNode.getStripeLength(conf);
        this.dataLength = conf.getInt("hdfs.bdr.data.length", 0);
        if (dataLength == 0) {
          //never degraded read
          LOG.info("dfs.raid.dataLength is incorrectly defined to be " + 
              dataLength + " Ignoring...");
        }

        this.path = path;
        
        // Construct array of blocks in file.
        this.blockSize = blockSize;
        this.fileSize = fileSize;

        long numBlocks = (this.fileSize % this.blockSize == 0) ?
          this.fileSize / this.blockSize :
          1 + this.fileSize / this.blockSize;
        if(DEBUG) LOG.info("numBlocks "+numBlocks);

        this.underlyingBlocks = new UnderlyingBlock[(int)numBlocks];
        for (int i = 0; i < numBlocks; i++) {
          long actualFileOffset = i * blockSize;
          long originalFileOffset = i * blockSize;
          long length = Math.min(
              blockSize, fileSize - originalFileOffset);
          this.underlyingBlocks[i] = new UnderlyingBlock(
              path, actualFileOffset, originalFileOffset, length);
        }

        this.currentOffset = 0;
        this.buffersize = buffersize;
        this.conf = conf;
        this.lfs = lfs;

        // Recover to Local by default.
        this.localRecovery = conf.getBoolean("fs.raid.recoveryfs.uselocal", true);

        this.stripSize
          = conf.getInt("hdfs.bdr.strip.size", 4);

        this.parityLength
          = conf.getInt("hdfs.bdr.parity.length", 2);

        this.stripeLength = dataLength+parityLength;
        this.stripeSize = stripeLength*stripSize;
        this.dataSize = dataLength*stripSize;

        this.readES = Executors.newFixedThreadPool(stripeLength);
        this.decodeES = Executors.newFixedThreadPool(parityLength*stripSize);
        this.readCS = new ExecutorCompletionService<Void>(readES);
        this.decodeCS = new ExecutorCompletionService<Void>(decodeES);

        if (localRecovery) {
          //LOG.info("Use local filesystem");
          this.recoverFs = FileSystem.getLocal(conf);
        } else {
          this.recoverFs = lfs.fs;
        }

        this.currentBlocks = new UnderlyingBlock[dataSize];
        this.currentStreams = new FSDataInputStream[dataSize];
        
        // Open a stream to the first stripe.
        if(DEBUG) LOG.info("initial ExtFsInputStream, parameter imported");
        openCurrentStreams();
        if(DEBUG) LOG.info("initial ExtFsInputStream, stream open");
      }

      private void closeCurrentStreams() throws IOException {
        if(DEBUG) LOG.info("close current streams start");
        if (currentStreams != null) {
          /* big bug here
          for(FSDataInputStream s: currentStreams){
            if(s != null){
              s.close();
              s = null;
            }
          }
          */
          for(int i = 0; i < currentStreams.length; i++){
            if(currentStreams[i] != null){
              currentStreams[i].close();
              currentStreams[i] = null;
            }
          }
        }
        if(DEBUG) LOG.info("close current streams end");
      }

      private void closeCurrentStream(int idx) throws IOException{
        if(DEBUG) LOG.info("close stream idx: "+idx);
        if(currentStreams != null && currentStreams[idx] != null){
          currentStreams[idx].close();
          currentStreams[idx] = null;
        }
      }

      // open all data block in the stripe
      private void openCurrentStreams() throws IOException {
        if(DEBUG) LOG.info("open current streams start");
        for(int i = 0; i < dataSize; i++){
          if(DEBUG) LOG.info("open current streams start "+i);
          openCurrentStream(i);
          if(DEBUG) LOG.info("open current streams end "+i);
        }
        if(DEBUG) LOG.info("open current streams end");
      }

      @Deprecated
      /**
       * Open a stream to the file containing the current block
       * and seek to the appropriate offset
       */
      private void openCurrentStreams(Set<Integer> idxs) throws IOException {
        //if seek to the filelen + 1, block should be the last block
        if(DEBUG) LOG.info("openCurrentStreams");
        int stripeIdx = (currentOffset < fileSize)?
          (int)(currentOffset/blockSize/dataSize):
          (underlyingBlocks.length - 1)/dataSize;
        if(DEBUG) LOG.info("openCurrentStreams idx stripeIdx "+stripeIdx);
        UnderlyingBlock[] blocks = new UnderlyingBlock[dataSize];
        for(int i = 0; i < dataSize; i++)
          blocks[i] = underlyingBlocks[stripeIdx*dataSize+i];
        // If the current path is the same as we want.
        boolean[] flags = new boolean[dataSize];
        if(DEBUG) LOG.info("openCurrentStreams cur streams testing");
        for(int i: idxs){
          if (currentBlocks[i] == blocks[i] ||
              currentBlocks[i]  != null && currentBlocks[i].path == blocks[i].path){
            if (currentStreams[i] != null) {
              currentBlocks[i] = blocks[i];
              long correctPos = ((int)(currentOffset/blockSize) == stripeIdx*dataSize+i)?
                currentOffset:(stripeIdx*dataSize+i)*blockSize;
              if(currentStreams[i].getPos() != correctPos)
                currentStreams[i].seek(correctPos);
              flags[i] = true;
            }
          }else{
            closeCurrentStream(i);
          }
        }

        boolean flag = true;
        for(int i: idxs){
          if(flags[i] == false){
            flag = false;
            break;
          }
        }
        
        if(DEBUG) LOG.info("openCurrentStreams flag: "+flag);
        if(flag) return;

        if(DEBUG) LOG.info("openCurrentStreams need to open some streams");
        for(int i: idxs)
          if(!flags[i]){
            currentBlocks[i] = blocks[i];
            if (recoveredPaths.contains(currentBlocks[i].path)) {
              currentStreams[i] = recoverFs.open(
                  currentBlocks[i].path, buffersize);
            } else {
              currentStreams[i] = lfs.fs.open(currentBlocks[i].path, buffersize);
            }
            long offset = blocks[i].actualFileOffset +
              ((currentOffset/blockSize == blocks[i].originalFileOffset/blockSize)?
               (currentOffset - blocks[i].originalFileOffset):0);
            currentStreams[i].seek(offset);
          }
        if(DEBUG) LOG.info("openCurrentStreams end");
      }

      private void openCurrentStream(int i) throws IOException {
        int stripeIdx = (currentOffset < fileSize)?
          (int)(currentOffset/blockSize/dataSize):
          (underlyingBlocks.length - 1)/dataSize;
        UnderlyingBlock blocks = underlyingBlocks[stripeIdx*dataSize+i];
        // If the current path is the same as we want.
        if (currentBlocks[i] == blocks ||
            currentBlocks[i] != null && currentBlocks[i].path == blocks.path){
          if(DEBUG) LOG.info("open current streams block matched");
          if (currentStreams[i] != null) {
            if(DEBUG) LOG.info("open current streams streams opened");
            currentBlocks[i] = blocks;
            long correctPos = (((int)(currentOffset/blockSize) == stripeIdx*dataSize+i)?
              currentOffset:
              (stripeIdx*dataSize+i)*blockSize);
            if(DEBUG) LOG.info("open current streams correct position "+correctPos);
            if(DEBUG) LOG.info("open current streams current position "+currentStreams[i].getPos());
            if(currentStreams[i].getPos() != correctPos){
              if(DEBUG) LOG.info("open current streams before seek");
              currentStreams[i].seek(correctPos);
              if(DEBUG) LOG.info("open current streams after seek");
            }
            return;
          }
        }else{
          closeCurrentStream(i);
        }

        currentBlocks[i] = blocks;
        if (recoveredPaths.contains(currentBlocks[i].path)) {
          currentStreams[i] = recoverFs.open(
              currentBlocks[i].path, buffersize);
        } else {
          currentStreams[i] = lfs.fs.open(currentBlocks[i].path, buffersize);
        }
        long offset = blocks.actualFileOffset +
          ((currentOffset/blockSize == blocks.originalFileOffset/blockSize)?
           (currentOffset - blocks.originalFileOffset):0);
        currentStreams[i].seek(offset);
      }

      @Deprecated
      /**
       * Returns the number of bytes available in the current block.
       */
      private int blockAvailable(int i) {
        // This implementation may incorrect.
        return (int) (currentBlocks[i].length -
            (currentOffset - currentBlocks[i].originalFileOffset));
      }


      @Override
        /**
         * an incorrect implementation (but usable)
         */
        public synchronized int available() throws IOException {
          // Application should not assume that any bytes are buffered here.
          // maybe I need to implement this function more carefully
          int available = stripeAvailable();
          return available;
        }

      @Override
        public synchronized  void close() throws IOException {
          closeCurrentStreams();
          super.close();
          for (Path p: recoveredPaths) {
            LOG.info("Deleting recovered block-file " + p);
            recoverFs.delete(p, false);
          }
        }

      @Override
        public boolean markSupported() { return false; }

      @Override
        public void mark(int readLimit) {
        }

      @Override
        public void reset() throws IOException {
        }

      @Override
        public synchronized int read() throws IOException {
          int value = read(oneBytebuff, 0, 1);
          if (value < 0) {
            return value;
          } else {
            return 0xFF & oneBytebuff[0];
          }
        }

      @Override
        public synchronized int read(byte[] b) throws IOException {
          return read(b, 0, b.length);
        }

      /**
       * dump and hardcode implementation at the moment
       */
      private double[] getCosts(){
        double[] costs = new double[stripeLength];
        for(int i=0; i < stripeLength; i++)
          costs[i] = i+1;
        return costs;
      }

      private FSDataInputStream getCurrentStream(){
        int currentBlockIdx = (int)(currentOffset / blockSize) % dataSize;
        return currentStreams[currentBlockIdx];
      }

      @Override
        public synchronized int read(byte[] b, int offset, int len) 
        throws IOException {
        long startTime = 0;
        if(MEASURE) startTime = System.nanoTime();
        if(DEBUG) LOG.info("ExtInputStream read called");
        
        if (currentOffset >= fileSize) {
          return -1;
        }
        //get Location info in the stripe
        int stripeIdx = (int)(currentOffset/blockSize/dataSize);

        //normalize len
        long distToStripeEnd = (stripeIdx+1)*dataSize*blockSize-currentOffset;
        len = (int)Math.min(distToStripeEnd, len);

        /* corrupt info */
        Map<Integer, LocatedBlock> corruptsInStripe =
          new HashMap<Integer, LocatedBlock>();

        /* block locations */
        Map<Integer, DatanodeInfo[] > locations =
          new HashMap<Integer, DatanodeInfo[] >();

        RaidDFSUtil.corruptBlocksInStripe((DistributedFileSystem)lfs.fs, 
            path.toUri().getPath(), 
            currentOffset, dataSize, blockSize, 
            corruptsInStripe, locations);

        if(DEBUG) LOG.info("location size "+locations.size());
        List<Integer> lostIdxs = new ArrayList<Integer>();

        int lostSize = BDRUtil.getCorruptNum(corruptsInStripe, currentOffset, len,
            blockSize, dataSize, lostIdxs);

        if(DEBUG) LOG.info("lost size "+lostSize);
        if(DEBUG) LOG.info("current offset "+currentOffset);

        if(lostSize > 0){

          //fake cost for testing
          double[] costs = getCosts();

          int lostStripIdx = 0;
          //assume only one node has data lost
          for(Integer idx: corruptsInStripe.keySet()){
            if(DEBUG) LOG.info("corrupt idx "+idx);
            lostStripIdx = idx%dataLength;
          }

          int startIdx = (int)(currentOffset/blockSize)%dataSize;
          //need to take care of this
          int numBlkToRead = len/(int)blockSize;

          if(DEBUG) LOG.info("lost blocks "+lostSize);

          ByteBuffer downloads = ByteBuffer.allocateDirect(stripeSize);
          ByteBuffer decodeMatrix = ByteBuffer.allocateDirect(
              stripeSize*lostSize);
          if(DEBUG){
            LOG.info("parityLength: "+parityLength);
            LOG.info("dataLength: "+dataLength);
            LOG.info("stripSize: "+stripSize);
            for(double cost: costs)
              LOG.info("cost "+cost);
            LOG.info("lostStripIdx "+lostStripIdx);
            LOG.info("startIdx "+startIdx);
            LOG.info("len "+len);
            LOG.info("blockSize "+blockSize);
            LOG.info("numBlkToRead "+numBlkToRead);
            LOG.info("downloads length "+downloads.capacity());
            LOG.info("decodeMatrix length "+decodeMatrix.capacity());
          }
          if(conf.getBoolean("hdfs.bdr.use.crdr", true)){
            if(DEBUG) LOG.info("using crdr");
            decodeCrdr(dataLength, parityLength, stripSize,
                costs, lostStripIdx, startIdx, numBlkToRead,
                downloads, decodeMatrix);
          }else{
            if(DEBUG) LOG.info("using basic");
            decodeBasic(dataLength, parityLength, stripSize,
                costs, lostStripIdx, startIdx, numBlkToRead,
                downloads, decodeMatrix);
          }

          if(DEBUG) BDRUtil.printByteBuf(downloads, "downloads", LOG);
          if(DEBUG) BDRUtil.printByteBuf(decodeMatrix, "decode matrix", LOG);

          boolean[][] marker =  BDRUtil.getDownloadMarker(downloads,
              stripeLength, stripSize);
          boolean[][] formattedDM = BDRUtil.getFormattedDM(decodeMatrix, 
              lostSize, stripeLength, stripSize);

          openCurrentStreams();

          Queue[] readQueues = new Queue[stripeLength];

          for(int i = 0; i<readQueues.length; i++){
            readQueues[i] = new LinkedList<ReadTask>();
          }

          /*int[][] degrees = getDegrees(boolean[][] marker. decodeMatrix);*/

          for(int i = 0; i<marker.length; i++){
            for(int j = 0; j < marker[i].length; j++){
              if(marker[i][j]){
                if(DEBUG) LOG.info("marker true: "+i+", "+j);
                int blockIdxInStripe = BDRUtil.getBlockIdxInStripe(i, 
                    j, dataLength, parityLength, stripSize);
                if(DEBUG) LOG.info("block idx: "+blockIdxInStripe);
                ReadTask rt = new ReadTask(blockIdxInStripe, (int)blockSize);
                readQueues[i].offer(rt);
              }
            }
            Collections.shuffle((LinkedList<ReadTask>)readQueues[i]);
            readQueues[i].offer(ReadTask.endOfQueue);
          }

          FSDataInputStream[] ins = new FSDataInputStream[stripeSize];
          for(int i = 0; i<dataSize; i++){
            ins[i] = currentStreams[i];
          }
          for(int i = dataSize; i < stripeSize; i++){
            ins[i] = RaidNode.unRaidParityStream(
                conf, path, RaidNode.pmDestinationPath(conf, lfs.fs),
                stripeIdx, i-dataSize, parityLength*stripSize);
          }

          BlockingQueue[] decodeQueues
            = new BlockingQueue[lostSize];
          for(int i=0; i<lostSize; i++)
            decodeQueues[i]
              = new ArrayBlockingQueue<DataCell>(stripSize);

          for(int i = 0; i<lostSize; i++){
            int lostIdx = lostIdxs.get(i);
            int lostOffset = (int)blockSize*(lostIdx-startIdx);
            DataCell cell = new DataCell(b, lostOffset, (int)blockSize, false);
            if(DEBUG) LOG.info("lost idx "+i+" : "+lostIdx);
            decodeCS.submit(new LostBlockDecoder(
                  decodeQueues[i], (int)blockSize, stripeLength, cell, lostIdx));
          }

          for(int i = 0; i<readQueues.length; i++)
            readES.submit(new BlockReader(ins, readQueues[i],
                  formattedDM, decodeQueues,
                  startIdx, numBlkToRead, b, blockSize));

          if(DEBUG) LOG.info("job submitted");

          try{
            for(int i = 0; i < lostSize; i++){
              decodeCS.take();
            }
          } catch (InterruptedException e) {
            if(DEBUG) LOG.info("InterruptedException");
            Thread.currentThread().interrupt();
          }
          if(DEBUG) LOG.info("task completed");

          int read = (int)Math.min(blockSize*numBlkToRead, len);
          /*
          for(int i = 0; i < numBlkToRead; i++){
            int toRead = (int)Math.min(blockSize, len-read);
            int curBlk = startIdx+i;
            byte[] curData = readBlocks.get(curBlk);
            if(curData==null) LOG.info("curData "+i+" is null");
            System.arraycopy(curData, 0, b, offset+read, toRead);
            read += toRead;
          }
          */
          currentOffset += read;
          if(MEASURE) LOG.info("Degraded read: "+((System.nanoTime()-startTime)/1000000)+" ms");
          return read;
        }

        if(DEBUG) LOG.info("No degraded read");

        openCurrentStreams();

        Queue[] readQueues = new Queue[dataLength];
        for(int i = 0; i<readQueues.length; i++){
          readQueues[i] = new LinkedList<ReadTask>();
        }

        int startIdx = (int)(currentOffset / blockSize) % dataSize;
        long curOffsetInStripe = currentOffset % (dataSize*blockSize);
        int endIdx = (curOffsetInStripe+len >= blockSize*stripeSize)?
          (stripeSize):
          (int)((curOffsetInStripe+len)/blockSize);

        if(DEBUG) LOG.info("Normal Read StartIdx "+startIdx);
        if(DEBUG) LOG.info("Normal Read endIdx "+endIdx);

        for(int i = startIdx; i<endIdx; i++){
          ReadTask rt = new ReadTask(i, (int)blockSize);
          readQueues[i%dataLength].offer(rt);
        }
        for(int i = 0; i<dataLength; i++)
          readQueues[i].offer(ReadTask.endOfQueue);

        for(int i = 0; i<readQueues.length; i++)
          readCS.submit(new BlockReader(currentStreams, readQueues[i], startIdx, endIdx-startIdx, b, blockSize));
        try{
          for(int i = 0; i < dataLength; i++){
            readCS.take();
          }
        } catch (InterruptedException e) {
          LOG.info("InterruptedException");
          Thread.currentThread().interrupt();
        }
        int read = (int)Math.min(blockSize*(endIdx-startIdx), len);
        /*
        for(int i = startIdx; i<endIdx; i++){
          int toRead = (int)Math.min(blockSize, len-read);
          byte[] curData = readBlocks.get(i);
          System.arraycopy(curData, 0, b, offset+read, toRead);
          read += toRead;
        }
        */
        currentOffset += read;
        if(MEASURE) LOG.info("Normal read: "+((System.nanoTime()-startTime)/1000000)+" Interval");
        return read;
      }


      private synchronized int stripeAvailable(){
        int stripeIdx = (int)(currentOffset/blockSize)/stripSize;
        int stripeEndOffset = (int)Math.min(fileSize, 
            (stripeIdx+1)*blockSize*dataSize);
        return (int)(stripeEndOffset-currentOffset);
      }

      class LostBlockDecoder implements Callable<Void>{
        private final BlockingQueue<DataCell> q;
        private final int blockSize;
        private final int numReaders;
        private final DataCell ret;
        private final int idx;
        LostBlockDecoder(BlockingQueue<DataCell> q, int blockSize, 
            int numReaders, DataCell ret, int idx){
          this.q = q;
          this.blockSize = blockSize;
          this.numReaders = numReaders;
          this.ret = ret;
          this.idx = idx;
        }
        public Void call(){
          int count = 0;
          if(DEBUG) LOG.info("num of readers " + numReaders);
          while(count < numReaders){
            DataCell operand = null;
            try{
              operand = q.take();
              if(DEBUG && MEASURE) LOG.info("pop to decode lost block "+idx+" with buf size "
                  +operand.buf.length+" and length "+operand.len);
            }catch(InterruptedException e){
              Thread.currentThread().interrupt();
            }
            if(operand.buf.length == 0){
              count++;
              continue;
            }
            long st = (System.nanoTime()-initTime)/1000000;
            if(MEASURE) LOG.info("Lost block "+idx+" XOR one block start at "+st);
            BDRUtil.byteArrayXor(operand.buf, operand.offset, ret.buf, ret.offset, operand.len);
            long et = (System.nanoTime()-initTime)/1000000;
            if(MEASURE) LOG.info("Lost block "+idx+" XOR one block end at "+et);
            if(MEASURE) LOG.info("Lost block "+idx+" XOR one block spends "+(et-st)+" ms");
          }
          ret.setFinish();
          return null;
        }
      }

      class BlockReader implements Callable <Void>{
        private final FSDataInputStream[] ins;
        private final Queue<ReadTask> q;
        private final boolean[][] decodeMatrix;
        private final BlockingQueue[] decodeQueues;
        private final int startIdx;
        private final int numBlkToRead;
        private final byte[] outBuf;
        private final long blockSize;

        // for normal read
        BlockReader(FSDataInputStream[] ins,
            Queue<ReadTask> q, int startIdx, int numBlkToRead, byte[] buf, long blockSize){
          this(ins, q, null, null, startIdx, numBlkToRead, buf, blockSize);
        }

        BlockReader(FSDataInputStream[] ins,
            Queue<ReadTask> q, boolean[][] decodeMatrix, BlockingQueue[] decodeQueues,
            int startIdx, int numBlkToRead, byte[] buf, long blockSize){
          this.ins = ins;
          this.q = q;
          this.decodeMatrix = decodeMatrix;
          this.decodeQueues = decodeQueues;
          this.startIdx = startIdx;
          this.numBlkToRead = numBlkToRead;
          this.outBuf = buf;
          this.blockSize = blockSize;
        }
        public Void call()
          throws IOException, BlockMissingException, ChecksumException{
          ReadTask curTask = q.poll();
          if(DEBUG) LOG.info("the first read task "+curTask.blockIdxInStripe);
          while(curTask != null && !curTask.finished){
            byte[] buf = null;
            int offset = 0;
            int len = curTask.toRead;
            if(inRange(curTask.blockIdxInStripe)){
              buf = outBuf;
              offset = (int)blockSize*(curTask.blockIdxInStripe-startIdx);
            }else{
              if(DEBUG) LOG.info("Block "+curTask.blockIdxInStripe+" out of range");
              buf = new byte[curTask.toRead];
            }
            if(DEBUG) LOG.info("about to read data "+curTask.blockIdxInStripe);
            long st = (System.nanoTime()-initTime)/1000000;
            if(MEASURE) LOG.info("block "+curTask.blockIdxInStripe+" read start at "+st);
            int ret = ins[curTask.blockIdxInStripe].read(buf, offset, len);
            long et = (System.nanoTime()-initTime)/1000000;
            if(MEASURE) LOG.info("block "+curTask.blockIdxInStripe+" read end at "+et);
            if(MEASURE) LOG.info("block "+curTask.blockIdxInStripe+" read spends "+(et-st)+" ms");
            
            if(DEBUG) LOG.info("about to put in the data "+curTask.blockIdxInStripe);
            if(decodeQueues != null){
              for(int i = 0; i<decodeQueues.length; i++){
                int matrixIdx = BDRUtil.getMatrixIndxFromBlock(
                    curTask.blockIdxInStripe, dataLength, parityLength, stripSize
                    );
                if(DEBUG) LOG.info("Matrix Idx: "+matrixIdx);
                if(decodeMatrix[i][matrixIdx])
                  try{
                    decodeQueues[i].put(new DataCell(buf, offset, len, true));
                    if(DEBUG && MEASURE) LOG.info("put Matrix idx: "+matrixIdx+" for "+i);
                  }catch(InterruptedException e){
                    Thread.currentThread().interrupt();
                  }
              }
            }
            curTask = q.poll();
          }
          if(decodeQueues != null)
            for(BlockingQueue q: decodeQueues)
              try{
                q.put(DataCell.endOfQueue);
              }catch(InterruptedException e){
                Thread.currentThread().interrupt();
              }
          return null;
        }

        private boolean isData(int blockIdxInStripe){
          if(blockIdxInStripe < dataSize) return true;
          return false;
        }
        private boolean inRange(int blockIdxInStripe){
          if(blockIdxInStripe-startIdx >= 0 && blockIdxInStripe-startIdx < numBlkToRead) 
            return true;
          return false;
        }
      }

      static class ReadTask{
        public final int blockIdxInStripe;
        public final int toRead;
        public final boolean finished;

        private ReadTask(boolean finished){
          this.finished = finished;
          blockIdxInStripe=0;
          toRead = 0;
        }

        static public final ReadTask endOfQueue = new ReadTask(true);

        ReadTask(int blockIdxInStripe, int toRead){
          this.blockIdxInStripe = blockIdxInStripe;
          this.toRead = toRead;
          this.finished = false;
        }
      }

      static class DataCell{
        public final byte[] buf;
        public final int offset;
        public final int len;
        private boolean finished;

        DataCell(byte[] buf, int offset, int len, boolean finished){
          this.buf = buf;
          this.offset = offset;
          this.len = len;
          this.finished = finished;
        }

        static public final DataCell endOfQueue = new DataCell(new byte[0], 0, 0, true);

        public void setFinish(){
          this.finished = true;
        }

        public boolean isFinish(){
          return this.finished;
        }
      }

      @Override
        public synchronized int read(long position, byte[] b, int offset, int len) 
        throws IOException {
          long oldPos = currentOffset;
          seek(position);
          try {
            return read(b, offset, len);
          } finally {
            seek(oldPos);
          }
        }

      @Override
        public synchronized long skip(long n) throws IOException {
          long skipped = 0;
          long startPos = getPos();
          while (skipped < n) {
            int toSkip = (int)Math.min(SKIP_BUF_SIZE, n - skipped);
            int val = read(skipbuf, 0, toSkip);
            if (val < 0) {
              break;
            }
            skipped += val;
          }

          long newPos = getPos();
          if (newPos - startPos > n) {
            throw new IOException(
                "skip(" + n + ") went from " + startPos + " to " + newPos);
          }
          if (skipped != newPos - startPos) {
            throw new IOException(
                "skip(" + n + ") went from " + startPos + " to " + newPos +
                " but skipped=" + skipped);
          }
          return skipped;
        }

      @Override
        public synchronized long getPos() throws IOException {

          return currentOffset;
        }

      @Override
        public synchronized void seek(long pos) throws IOException {
          if (pos > fileSize) {
            throw new EOFException("Cannot seek to " + pos + ", file length is " + fileSize);
          }
          if (pos != currentOffset) {
            closeCurrentStreams();
            currentOffset = pos;
            openCurrentStreams();
          }

        }

      @Override
        public boolean seekToNewSource(long targetPos) throws IOException {
          seek(targetPos);
          FSDataInputStream currentStream = getCurrentStream();
          boolean value = currentStream.seekToNewSource(currentStream.getPos());

          return value;
        }

      /**
       * position readable again.
       */
      @Override
      public void readFully(long pos, byte[] b, int offset, int length) 
          throws IOException {
        long oldPos = currentOffset;
        seek(pos);
        try {
          while (true) {
            // This loop retries reading until successful. Unrecoverable errors
            // cause exceptions.
            // currentOffset is changed by read().
            try {
              while (length > 0) {
                int n = read(b, offset, length);
                if (n < 0) {
                  throw new IOException("Premature EOF");
                }
                offset += n;
                length -= n;
              }
              return;
            } catch (BlockMissingException e) {
              LOG.info("Block missing exception");
            } catch (ChecksumException e) {
            }
          }
        } finally {
          seek(oldPos);
        }
      }

      @Override
        public void readFully(long pos, byte[] b) throws IOException {
          readFully(pos, b, 0, b.length);
        }

      private void addRecoveredPath(Path p) {
        recoveredPaths.add(p);
        if (localRecovery) {
          LocalFileSystem localFs = (LocalFileSystem) recoverFs;
          new File(p.toUri().getPath()).deleteOnExit();
          new File(localFs.getChecksumFile(p).toUri().getPath()).deleteOnExit();
        }
      }

      /**
       * disabled
       * Extract good block from RAID
       * @throws IOException if all alternate locations are exhausted
       */

      /**
       * The name of the file system that is immediately below the
       * DistributedBDRFileSystem. This is specified by the
       * configuration parameter called fs.raid.underlyingfs.impl.
       * If this parameter is not specified in the configuration, then
       * the default class DistributedFileSystem is returned.
       * @param conf the configuration object
       * @return the filesystem object immediately below DistributedBDRFileSystem
       * @throws IOException if all alternate locations are exhausted
       */
      private FileSystem getUnderlyingFileSystem(Configuration conf) {
        Class<?> clazz = conf.getClass("fs.raid.underlyingfs.impl", DistributedFileSystem.class);
        FileSystem fs = (FileSystem)ReflectionUtils.newInstance(clazz, conf);
        return fs;
      }
    }

    /**
     * constructor for ext input stream.
     * @param fs the underlying filesystem
     * @param p the path in the underlying file system
     * @param buffersize the size of IO
     * @throws IOException
     */
    public ExtFSDataInputStream(Configuration conf, DistributedBDRFileSystem lfs,
        Path p, long fileSize, long blockSize,
        int buffersize) throws IOException {
      super(new ExtFsInputStream(conf, lfs, p, fileSize, blockSize,
            buffersize));
      if(DEBUG) LOG.info("initial ExtFSDataInputStream");
    }
  }

}
