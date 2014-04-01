/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * pass data to output
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
package org.apache.hadoop.hdfs.server.datanode;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.SocketException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.concurrent.*;

import org.apache.commons.logging.Log;
import org.apache.hadoop.fs.ChecksumException;
import org.apache.hadoop.hdfs.protocol.Block;
import org.apache.hadoop.hdfs.protocol.FSConstants;
import org.apache.hadoop.hdfs.protocol.DataTransferProtocol.PacketHeader;
import org.apache.hadoop.hdfs.util.DataTransferThrottler;
import org.apache.hadoop.io.IOUtils;
import org.apache.hadoop.net.SocketOutputStream;
import org.apache.hadoop.util.DataChecksum;
import org.apache.hadoop.util.StringUtils;

/**
 * Reads a block from the disk and sends it to a recipient.
 */
class IABlockSender implements java.io.Closeable, FSConstants {
    
  native void iaREncode(byte[] data, byte[] code, int bufsize, int mPlusk, int k, int n, int[] fails, ByteBuffer reserve);

  static{
    System.loadLibrary("iarencode");
  }

  private ByteBuffer cache;

  public static final Log LOG = DataNode.LOG;
  static final Log ClientTraceLog = DataNode.ClientTraceLog;

  private Block block; // the block to read from

  /** the replica to read from */
  private final Replica replica;
  /** The visible length of a replica. */
  private final long replicaVisibleLength;

  private static final boolean is32Bit = System.getProperty("sun.arch.data.model").equals("32");

  private InputStream blockIn; // data stream
  private long blockInPosition = -1; // updated while using transferTo().
  private DataInputStream checksumIn; // checksum datastream
  private DataChecksum checksum; // checksum stream
  private long offset; // starting position to read
  private long realOffset;
  private long endOffset; // ending position
  private long realEndOffset; // ending position
  private int bytesPerChecksum; // chunk size
  private int checksumSize; // checksum size
  private boolean corruptChecksumOk; // if need to verify checksum
  private boolean chunkOffsetOK; // if need to send chunk offset
  private long seqno; // sequence number of packet

  private boolean transferToAllowed = true;
  // set once entire requested byte range has been sent to the client
  private boolean blockReadFully; //set when the whole block is read
  private boolean sentEntireByteRange;
  private boolean verifyChecksum; //if true, check is verified while reading
  private DataTransferThrottler throttler;
  private final String clientTraceFmt; // format of client trace log message

  private int iaPacketSize;
  private int k;
  private int m;
  private int n;
  private int[] fails;

  private ByteBuffer reserve = null;

  /**
   * Minimum buffer used while sending data to clients. Used only if
   * transferTo() is enabled. 64KB is not that large. It could be larger, but
   * not sure if there will be much more improvement.
   */
  private static final int MIN_BUFFER_WITH_TRANSFERTO = 64*1024;
  private volatile ChunkChecksum lastChunkChecksum = null;


  IABlockSender(Block block, long startOffset, long length,
      boolean corruptChecksumOk, boolean chunkOffsetOK,
      boolean verifyChecksum, DataNode datanode,
      int k, int m, int n, int[] fails) throws IOException {
    this(block, startOffset, length, corruptChecksumOk, chunkOffsetOK,
        verifyChecksum, datanode, null, k, m, n, fails);
  }

  IABlockSender(Block block, long startOffset, long length,
      boolean corruptChecksumOk, boolean chunkOffsetOK,
      boolean verifyChecksum, DataNode datanode, String clientTraceFmt,
      int k, int m, int n, int[] fails)
    throws IOException {

    if(verifyChecksum)
      LOG.info("verify checksum is true");
    else
      LOG.info("verify checksum is not true");
    /** packet size for ia encoding */
    iaPacketSize = datanode.getConf().getInt("hdfs.raid.packet.size", 1024*1024);
    //LOG.info("iaPacketSize: "+iaPacketSize);
    cache = null;

    this.k = k;
    this.m = m;
    this.n = n;
    this.fails = fails;
    //reserve = datanode.getReserve(k);

    try {
      this.block = block;
      synchronized(datanode.data) { 
        this.replica = datanode.data.getReplica(block.getBlockId());
        if (replica == null) {
          throw new ReplicaNotFoundException(block);
        }
        this.replicaVisibleLength = replica.getVisibleLength();
      }
      long minEndOffset = startOffset + length;
      long realMinEndOffset = minEndOffset * m / n;
      // if this is a write in progress
      ChunkChecksum chunkChecksum = null;
      if (replica instanceof ReplicaBeingWritten) {
        for (int i = 0; i < 30 && replica.getBytesOnDisk() < minEndOffset; i++) {
          try {
            Thread.sleep(100);
          } catch (InterruptedException ie) {
            throw new IOException(ie);
          }
        }

        long currentBytesOnDisk = replica.getBytesOnDisk();
        
        if (currentBytesOnDisk < realMinEndOffset) {
          throw new IOException(String.format(
            "need %d bytes, but only %d bytes available",
            realMinEndOffset,
            currentBytesOnDisk
          ));
        }

        ReplicaInPipeline rip = (ReplicaInPipeline) replica;
        chunkChecksum = rip.getLastChecksumAndDataLen();
      }

      if (replica.getGenerationStamp() < block.getGenerationStamp()) {
        throw new IOException(
            "replica.getGenerationStamp() < block.getGenerationStamp(), block="
            + block + ", replica=" + replica);
      }
      if (replicaVisibleLength < 0) {
        throw new IOException("The replica is not readable, block="
            + block + ", replica=" + replica);
      }
      if (DataNode.LOG.isDebugEnabled()) {
        DataNode.LOG.debug("block=" + block + ", replica=" + replica);
      }
      
      this.chunkOffsetOK = chunkOffsetOK;
      this.corruptChecksumOk = corruptChecksumOk;
      this.verifyChecksum = verifyChecksum;

      // transferToFully() fails on 32 bit platforms for block sizes >= 2GB,
      // use normal transfer in those cases
      // not allow
      this.transferToAllowed = false && datanode.transferToAllowed &&
        (!is32Bit || length < (long) Integer.MAX_VALUE);
      this.clientTraceFmt = clientTraceFmt;

      if ( !corruptChecksumOk || datanode.data.metaFileExists(block) ) {
        /** take care about the BUFFER_SIZE */
        checksumIn = new DataInputStream(
            new BufferedInputStream(datanode.data.getMetaDataInputStream(block),
              BUFFER_SIZE));

        // read and handle the common header here. For now just a version
        BlockMetadataHeader header = BlockMetadataHeader.readHeader(checksumIn);
        short version = header.getVersion();

        if (version != FSDataset.METADATA_VERSION) {
          LOG.warn("Wrong version (" + version + ") for metadata file for "
              + block + " ignoring ...");
        }
        checksum = header.getChecksum();
      } else {
        LOG.warn("Could not find metadata file for " + block);
        // This only decides the buffer size. Use BUFFER_SIZE?
        checksum = DataChecksum.newDataChecksum(DataChecksum.CHECKSUM_NULL,
            16 * 1024);
      }

      /* If bytesPerChecksum is very large, then the metadata file
       * is mostly corrupted. For now just truncate bytesPerchecksum to
       * blockLength.
       */
      bytesPerChecksum = checksum.getBytesPerChecksum();
      if (bytesPerChecksum > 10*1024*1024 && bytesPerChecksum > replicaVisibleLength) {
        checksum = DataChecksum.newDataChecksum(checksum.getChecksumType(),
            Math.max((int)replicaVisibleLength, 10*1024*1024));
        bytesPerChecksum = checksum.getBytesPerChecksum();        
      }
      checksumSize = checksum.getChecksumSize();

      if (length < 0) {
        length = replicaVisibleLength;
      }

      // end is either last byte on disk or the length for which we have a 
      // checksum
      if (chunkChecksum != null) {
        realEndOffset = chunkChecksum.getDataLength();
      } else {
        realEndOffset = replica.getBytesOnDisk();
      }
      endOffset = realEndOffset * n / m;

      long realStartOffset = startOffset * m / n;
      if (realStartOffset < 0 || realStartOffset > realEndOffset
          || (length * m/n + realStartOffset) > realEndOffset) {
        String msg = " Offset " + startOffset + " and length " + length
          + " don't match block " + block + " ( blockLen " + realEndOffset*n/m 
          + " n " + n + " k " + k + " m " + m + " )";
        LOG.warn(datanode.dnRegistration + ":sendBlock() : " + msg);
        throw new IOException(msg);
          }
      //assume ia packet is a multiple of bytesPerChecksum
      //offset = (startOffset - (startOffset % bytesPerChecksum));
      offset = (startOffset - (startOffset % (iaPacketSize * n/m)));
      realOffset = realStartOffset - (realStartOffset % iaPacketSize);

      if (length >= 0) {
        // Make sure endOffset points to end of a <del>checksumed chunk</del> ia packet.
        long tmpLen = startOffset + length;
        /*
           if (tmpLen % bytesPerChecksum != 0) {
           tmpLen += (bytesPerChecksum - tmpLen % bytesPerChecksum);
           }
           */
        if (tmpLen % (iaPacketSize*n/m) != 0) {
          tmpLen += (iaPacketSize*n/m - tmpLen % (iaPacketSize*n/m));
        }
        if (tmpLen < endOffset) {
          // will use on-disk checksum here since the end is a stable chunk
          endOffset = tmpLen;
          realEndOffset = tmpLen * m/n;
        } else if (chunkChecksum != null) {
          //in last chunk which is changing. flag that we need to use in-memory 
          // checksum 
          this.lastChunkChecksum = chunkChecksum;
        }
      }

      // seek to the right offsets
      if (offset > 0) {
        /** */
        long checksumSkip = (realOffset / bytesPerChecksum) * checksumSize;
        // note blockInStream is seeked when created below
        if (checksumSkip > 0) {
          // Should we use seek() for checksum file as well?
          IOUtils.skipFully(checksumIn, checksumSkip);
        }
      }
      seqno = 0;

      if (DataNode.LOG.isDebugEnabled()) {
        DataNode.LOG.debug("replica=" + replica);
      }

      blockIn = datanode.data.getBlockInputStream(block, realOffset); // seek to offset
    } catch (IOException ioe) {
      IOUtils.closeStream(this);
      IOUtils.closeStream(blockIn);
      throw ioe;
    }
  }

  /**
   * close opened files.
   */
  public void close() throws IOException {
    IOException ioe = null;
    // close checksum file
    if(checksumIn!=null) {
      try {
        checksumIn.close();
      } catch (IOException e) {
        ioe = e;
      }
      checksumIn = null;
    }
    // close data file
    if(blockIn!=null) {
      try {
        blockIn.close();
      } catch (IOException e) {
        ioe = e;
      }
      blockIn = null;
    }
    // throw IOException if there is any
    if(ioe!= null) {
      throw ioe;
    }
  }

  /**
   * Converts an IOExcpetion (not subclasses) to SocketException.
   * This is typically done to indicate to upper layers that the error 
   * was a socket error rather than often more serious exceptions like 
   * disk errors.
   */
  private static IOException ioeToSocketException(IOException ioe) {
    if (ioe.getClass().equals(IOException.class)) {
      // "se" could be a new class in stead of SocketException.
      IOException se = new SocketException("Original Exception : " + ioe);
      se.initCause(ioe);
      /* Change the stacktrace so that original trace is not truncated
       * when printed.*/ 
      se.setStackTrace(ioe.getStackTrace());
      return se;
    }
    // otherwise just return the same exception.
    return ioe;
  }

  /**
   * Sends upto maxChunks chunks of data. Used by Encoded Read.
   * 
   * When blockInPosition is >= 0, assumes 'out' is a 
   * {@link SocketOutputStream} and tries 
   * {@link SocketOutputStream#transferToFully(FileChannel, long, int)} to
   * send data (and updates blockInPosition).
   */
  private int sendChunks(ByteBuffer pkt, int maxChunks, OutputStream out, BlockingQueue<ByteBuffer> q)
    throws IOException {
    //LOG.info("anchor Send_packet "+seqno);
    // Sends multiple chunks in one packet with a single write().

    int len = (int) Math.min((endOffset - offset),
                             (((long) bytesPerChecksum) * ((long) maxChunks)));
    int numChunks = (len + bytesPerChecksum - 1)/bytesPerChecksum;
    //boolean lastDataPacket = offset + len == endOffset && len > 0;
    int packetLen = len + numChunks*checksumSize + 4;
    //initial packet
    pkt.clear();

    //header
    PacketHeader header = new PacketHeader(
      packetLen, offset, seqno, (len == 0), len);
    header.putInBuffer(pkt);

    int checksumOff = pkt.position();
    int checksumLen = numChunks * checksumSize;
    byte[] buf = pkt.array();

    int dataOff = checksumOff + checksumLen;
    /*
    LOG.info("real length of the packet " + (dataOff + len) + " maxchunks " + maxChunks
        + " num chunks " + numChunks);
    */
    //read data from the ring buffer. Due to some padding problems, we need a global cache.
    //may have a better design
    if(cache == null)
      try{
        cache = q.take();
      }catch(InterruptedException e){
      }

    int r = cache.remaining();
    int taken = 0;
    while(r < len){
      cache.get(buf, dataOff+taken, r-taken);
      try{
        LOG.info("before taken new package with remaining:"+r);
        cache = q.take();
      }catch(InterruptedException e){
      }
      taken = r;
      r += cache.remaining();
    }

    //LOG.info("dataOff: "+dataOff+" taken: "+taken+" len:"+len);
    cache.get(buf, dataOff+taken, len-taken);

    //create checksum
    for(int i = checksumOff; i < checksumOff + checksumLen; i+=checksumSize){
      checksum.reset();
      int bufOff = (i-checksumOff)/checksumSize*bytesPerChecksum + dataOff;
      checksum.update(buf, bufOff, bytesPerChecksum);
      checksum.writeValue(buf, i, true);
    }
    //LOG.info("anchor Send_packet "+seqno+" Checksum_generated");

    try {
      if (blockInPosition >= 0) {
        //should not be used.
        LOG.warn("encoded read should not used transferTo().");
        //use transferTo(). Checks on out and blockIn are already done. 

        //SocketOutputStream sockOut = (SocketOutputStream)out;
        //first write the packet
        //sockOut.write(buf, 0, dataOff);
        // no need to flush. since we know out is not a buffered stream. 

        //sockOut.transferToFully(((FileInputStream)blockIn).getChannel(), 
        //                        blockInPosition, len);

        //blockInPosition += len;
      } else {
        // normal transfer
        /* LOG.info("send packet with Length: "+len+" Offset: "+offset); */
        out.write(buf, 0, dataOff + len);
      }
    //LOG.info("anchor Send_packet "+seqno+" Sent");
      
    } catch (IOException e) {
      /* Exception while writing to the client. Connection closure from
       * the other end is mostly the case and we do not care much about
       * it. But other things can go wrong, especially in transferTo(),
       * which we do not want to ignore.
       *
       * The message parsing below should not be considered as a good
       * coding example. NEVER do it to drive a program logic. NEVER.
       * It was done here because the NIO throws an IOException for EPIPE.
       */
      String ioem = e.getMessage();
      if (!ioem.startsWith("Broken pipe") && !ioem.startsWith("Connection reset")) {
        LOG.error("BlockSender.sendChunks() exception: ", e);
      }
      throw ioeToSocketException(e);
    }

    if (throttler != null) { // rebalancing so throttle
      throttler.throttle(packetLen);
    }

    return len;
  }
  /**
   * sendBlock() is used to read (and encode) block and its metadata and stream the data to
   * either a client or to another datanode
   * 
   * @param out  stream to which the block is written to
   * @param baseStream optional. if non-null, <code>out</code> is assumed to 
   *        be a wrapper over this stream. This enables optimizations for
   *        sending the data, e.g. 
   *        {@link SocketOutputStream#transferToFully(FileChannel, 
   *        long, int)}.
   * @param throttler for sending data.
   * @return total bytes reads, including crc.
   */
  long sendBlock(DataOutputStream out, OutputStream baseStream, 
                 DataTransferThrottler throttler) throws IOException {
    if( out == null ) {
      throw new IOException( "out stream is null" );
    }
    this.throttler = throttler;
    if(throttler == null)
      LOG.info("throttler is null");
    else
      LOG.info("throttler bandwidth: "+throttler.getBandwidth());


    long initialOffset = offset;
    long totalRead = 0;
    OutputStream streamForSendChunks = out;
    
    final long startTime = ClientTraceLog.isInfoEnabled() ? System.nanoTime() : 0;
    try {
      try {
        checksum.writeHeader(out);
        if ( chunkOffsetOK ) {
          out.writeLong( offset );
        }
        out.flush();
      } catch (IOException e) { //socket error
        throw ioeToSocketException(e);
      }
      
      int maxChunksPerPacket = 1;
      int pktSize = PacketHeader.PKT_HEADER_LEN;
      
      if (transferToAllowed && !verifyChecksum && 
          baseStream instanceof SocketOutputStream && 
          blockIn instanceof FileInputStream) {
        
        //FileChannel fileChannel = ((FileInputStream)blockIn).getChannel();
        
        // blockInPosition also indicates sendChunks() uses transferTo.
        //blockInPosition = fileChannel.position();
        //streamForSendChunks = baseStream;
        
        // assure a mininum buffer size.
        //maxChunksPerPacket = (Math.max(BUFFER_SIZE, 
        //                               MIN_BUFFER_WITH_TRANSFERTO)
        //                      + bytesPerChecksum - 1)/bytesPerChecksum;
        
        // allocate smaller buffer while using transferTo(). 
        //pktSize += checksumSize * maxChunksPerPacket;
      } else {
        maxChunksPerPacket = Math.max(1,
                 (BUFFER_SIZE + bytesPerChecksum - 1)/bytesPerChecksum);
        pktSize += ((bytesPerChecksum + checksumSize) * maxChunksPerPacket);
      }

      //queue for passing data from encode to output
      BlockingQueue<ByteBuffer> q = new ArrayBlockingQueue<ByteBuffer>(64);

      //Encode thread
      IAREncoder encoder = new IAREncoder(q);
      new Thread(encoder).start();
      
      //LOG.info("before allocate buf, we have pktSize " + pktSize + " maxchunksperpacket "
      //    + maxChunksPerPacket + " byteperchecksum " + bytesPerChecksum + " checksum size " +
      //    checksumSize);
      ByteBuffer pktBuf = ByteBuffer.allocate(pktSize);

      //output, send chunks
      while (endOffset > offset) {
        long len = sendChunks(pktBuf, maxChunksPerPacket, 
                              streamForSendChunks, q);
        //LOG.info("Send chunks with len:"+len+" and seq:"+seqno);
        //LOG.info("sendChunks offset:"+offset+" endOffset:"+endOffset);
        offset += len;
        totalRead += len + ((len + bytesPerChecksum - 1)/bytesPerChecksum*
                            checksumSize);
        seqno++;
      }
      try {
        // send an empty packet to mark the end of the block
        sendChunks(pktBuf, maxChunksPerPacket, streamForSendChunks, q);
        out.flush();
        LOG.info("Send last Chunk");
      } catch (IOException e) { //socket error
        LOG.info("IOException in sendChunks");
        throw ioeToSocketException(e);
      }

      sentEntireByteRange = true;
    } finally {
      if (clientTraceFmt != null) {
        final long endTime = System.nanoTime();
        ClientTraceLog.info(String.format(clientTraceFmt, totalRead, initialOffset, endTime - startTime));
      }
      close();
    }

    blockReadFully = initialOffset == 0 && offset >= replicaVisibleLength;

    return totalRead;
  }

  boolean isBlockReadFully() {
    return blockReadFully;
  }

  boolean didSendEntireByteRange() {
    return sentEntireByteRange;
  }

  class IAREncoder implements Runnable{

    private final BlockingQueue<ByteBuffer> queue;
    byte[] checksumBuf = null;
    byte[] buf = null;
    byte[] code = null;
    IAREncoder(BlockingQueue<ByteBuffer> q) {
      queue = q;
      if(checksumSize > 0 && checksumIn != null)
        checksumBuf = new byte[iaPacketSize/bytesPerChecksum*checksumSize];
      buf = new byte[iaPacketSize];
      code = new byte[iaPacketSize*n/m];
    }
    public void run() {
      int iteration = 0;
      while(realEndOffset > realOffset) {
        LOG.info("anchor Encode_stripe "+iteration);
        //LOG.info("New round to encode packet with realOffset: "+realOffset);
        boolean lastDataPacket = realOffset + iaPacketSize == realEndOffset;
        if(checksumIn != null){
          try{
            //read checksum
            checksumIn.readFully(checksumBuf, 0, checksumBuf.length);
          }catch( IOException e ){
            LOG.warn(" Could not read or failed to veirfy checksum for data" +
                " at real offset " + realOffset + " for block " + block + " got : "
                + StringUtils.stringifyException(e) +
                "in ia block sender");
            IOUtils.closeStream(checksumIn);
            checksumIn = null;
            /** need to handle checksum verified failure */
          }
          if(lastDataPacket && lastChunkChecksum != null){
            int start = checksumBuf.length - checksumSize;
            byte[] updatedChecksum = lastChunkChecksum.getChecksum();
            if(updatedChecksum != null){
              System.arraycopy(updatedChecksum, 0, checksumBuf, start, checksumSize);
            }
          }
        }
        //LOG.info("anchor Encode_stripe "+iteration+" Checksum_read");
        try{
          //read data
          IOUtils.readFully(blockIn, buf, 0, iaPacketSize);
        }catch(IOException e){
          LOG.warn(" Could not data" +
              " at real offset " + realOffset + " for block " + block + " got : "
              + StringUtils.stringifyException(e) +
              "in ia block sender");
          IOUtils.closeStream(blockIn);
          blockIn = null;
        }
        //LOG.info("anchor Encode_stripe "+iteration+" Data_read");

        if (verifyChecksum) {
          int dOff = 0;
          int cOff = 0;
          int dLeft = iaPacketSize;

          //LOG.info("verify checksum");

          for (int i = 0; i < iaPacketSize/bytesPerChecksum; i++){
            checksum.reset();
            int dLen = Math.min(dLeft, bytesPerChecksum);
            //potential bug here
            checksum.update(buf, dOff, dLen);
            if(!checksum.compare(checksumBuf, cOff)){
              long failedPos = realOffset + iaPacketSize - dLeft;
              //LOG.warn("checksum failed at " + failedPos);
            }
            dLeft -= dLen;
            dOff += dLen;
            cOff += checksumSize;
          }
        }

        //LOG.info("anchor Encode_stripe "+iteration+" Checksum_checked");

        //encode data
        iaREncode(buf, code, iaPacketSize, m+k, k, n, fails, reserve);
        //LOG.info("anchor Encode_stripe "+iteration+" Encoded");
        //LOG.info("code length: "+code.length);
        try {
          ByteBuffer tmp = ByteBuffer.allocate(code.length);
          tmp.put(code);
          tmp.rewind();
          //pass data to output
          queue.put(tmp);
          tmp = null;
        } catch (InterruptedException ex) {
          LOG.info("Interrupted Exception when putting new code");
        }
        //LOG.info("anchor Encode_stripe "+iteration+" Pushed");
        realOffset += iaPacketSize;
        //LOG.info("anchor Encode_stripe realOffset:"+realOffset+" realEndOffset:"+realEndOffset);
        //if(iteration < 10){
        //  for(int i = 0; i<8; i++){
        //    LOG.info(block.getBlockId()+"iteration: "+iteration+" data byte: " + i + " value: " + buf[i]);

        //  }
        //  for(int i = 0; i<8; i++){
        //    LOG.info(block.getBlockId()+"iteration: "+iteration+" code byte: " + i + " value: " + code[i]);
        //  }
        //}
        iteration++;
      }
    }
  }

  public void setReserve(ByteBuffer r){
    this.reserve = r;
  }
}
