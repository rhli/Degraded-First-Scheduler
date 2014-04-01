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

import java.io.OutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.nio.ByteBuffer;
import java.io.RandomAccessFile;
import java.io.File;
import java.util.concurrent.*;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.ChecksumException;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.hdfs.BlockMissingException;
import org.apache.hadoop.hdfs.DistributedFileSystem;
import org.apache.hadoop.hdfs.DistributedRaidFileSystem;
import org.apache.hadoop.hdfs.RaidDFSUtil;
import org.apache.hadoop.hdfs.protocol.*;

public class JRSDecoder extends Decoder {

  static{
    System.loadLibrary("jrsdecoder");
  }

  private int threadNum = 1;

  private BlockingQueue[] q;
  private BlockingQueue[] p;

  public static final Log LOG = LogFactory.getLog(
      "org.apache.hadoop.raid.JRSDecoder");

  private long startTime;
  private long endTime;

  /**
   * @param forRecovery determine the type of this decoder, for recovery or for degraded read 
   *   (someday, i may combine them into the same function)
   */
  public JRSDecoder(
      Configuration conf, int stripeSize, int paritySize, boolean forRecovery) {
    super(conf, stripeSize, paritySize);

    LOG.info("initial decoder: k="+stripeSize+" m="+paritySize+" bufSize:"+bufSize);
    threadNum = conf.getInt("hdfs.raid.decoder.threadnum", 1); 

    //data queue, input to decode
    this.q = new BlockingQueue[threadNum];
    for(int i=0; i<threadNum; i++)
      q[i] = new ArrayBlockingQueue<ByteBuffer>(1024);

    //signal queue, decode to output
    this.p = new BlockingQueue[threadNum];
    for(int i=0; i<threadNum; i++)
      p[i] = new ArrayBlockingQueue<Integer>(65);

    //decode threads
    Thread[] ds = new Thread[threadNum];
    for(int i = 0; i < threadNum; i++){
      if(forRecovery){
        JRSRecoveryDecoder decoder = new JRSRecoveryDecoder(i);
        ds[i] = new Thread(decoder);
      }else{
        JRSDegradedReadDecoder decoder = new JRSDegradedReadDecoder(i);
        ds[i] = new Thread(decoder);
      }
      ds[i].start();
    }

    LOG.info("JRSDecoder 1/1");
      }


  protected void fixErasedBlock(
      FileSystem fs, Path srcFile,
      FileSystem parityFs, Path parityFile,
      long blockSize, Map<Integer, LocatedBlock> corruptStripe,
      File[] lbfs, int stripeIdx) throws IOException {
    FSDataInputStream[] inputs = new FSDataInputStream[stripeSize + paritySize];
    Set<Integer> tmploc = corruptStripe.keySet();
    int[] erasedLocations = new int[tmploc.size()];
    int idx = 0;
    for(Integer loc: tmploc){
      erasedLocations[idx++] = loc;
    }
    Arrays.sort(erasedLocations);

    JRSStreamFactory sf = new JRSStreamFactory(fs, srcFile, parityFs, parityFile, stripeIdx);

    sf.buildStream(inputs, erasedLocations);

    long srcFileSize = fs.getFileStatus(srcFile).getLen();
    long parityFileSize = parityFs.getFileStatus(parityFile).getLen();

    long[] limits = new long[erasedLocations.length];
    for(int i = 0; i<limits.length; i++){
      long remaining  = 0;
      if(erasedLocations[i] < stripeSize)
        remaining = srcFileSize - corruptStripe.get(erasedLocations[i]).getStartOffset();
      else
        remaining = parityFileSize - corruptStripe.get(erasedLocations[i]).getStartOffset();
      limits[i] = Math.min(blockSize, remaining);
    }
    writeFixedBlock(inputs, erasedLocations, corruptStripe, lbfs, limits, sf);
      }

  /**
   * PMStreamFactory is for generating FSDataInputStreams
   * The streams need to be regenerated if the validErasedLocations is changed
   */
  class JRSStreamFactory{
    private FileSystem fs = null;
    private Path srcFile = null;
    private FileSystem parityFs = null;
    private Path parityFile = null;
    private long stripeIdx = -1; 

    public JRSStreamFactory(
        FileSystem fs, Path srcFile, FileSystem parityFs, Path parityFile,
        long stripeIdx){
      this.fs = fs; 
      this.parityFs = parityFs;
      this.srcFile = srcFile;
      this.parityFile = parityFile;
      this.stripeIdx = stripeIdx;
        }

    public void buildStream(FSDataInputStream[] inputs, int[] erasedLocations)
      throws IOException{
      buildStream(inputs, erasedLocations, 0); 
    }

    public void buildStream(FSDataInputStream[] inputs, int[] erasedLocations, int skipInBlock)
      throws IOException{
      FileStatus srcStat = fs.getFileStatus(srcFile);
      FileStatus parityStat = parityFs.getFileStatus(parityFile);
      long blockSize = srcStat.getBlockSize();
      ArrayList<Long> fails = new ArrayList<Long>();
      for(int i = 0; i < erasedLocations.length; i++)
        fails.add((long)(erasedLocations[i]));

      for(int i = 0, j = 0, k = 0; i < inputs.length && k < stripeSize; i++){
        long offset = 0;
        if(i < stripeSize)
          offset = blockSize * (stripeIdx * stripeSize + i) + skipInBlock;
        else
          offset = blockSize * (stripeIdx * paritySize + i - stripeSize) + skipInBlock;
        if(j >= erasedLocations.length || i != erasedLocations[j]){
          if(i < stripeSize){
            if (offset > srcStat.getLen()){
              inputs[i] = new FSDataInputStream(new RaidUtils.ZeroInputStream(
                    offset + blockSize));
            }else{
              inputs[i] = fs.open(
                  srcFile, conf.getInt("io.file.buffer.size", 64 * 1024));
              inputs[i].seek(offset);
            }
          }else{
            inputs[i] = parityFs.open(
                parityFile, conf.getInt("io.file.buffer.size", 64 * 1024));
            inputs[i].seek(offset);
            k++;
          }
        }else{
          inputs[i] = new FSDataInputStream(new RaidUtils.ZeroInputStream(
                offset + blockSize));
          j++;
        }
      }
    }

    public void closeStreams(FSDataInputStream[] inputs)
      throws IOException{
      for(FSDataInputStream s: inputs)
        if(s != null)
          s.close();
    }
  }

  protected void fixErasedBlock(
      FileSystem fs, Path srcFile, FileSystem parityFs, Path parityFile,
      long blockSize, long errorOffset, long limit,
      OutputStream out) throws IOException{
    int stripeIdx = (int)(errorOffset / blockSize) / stripeSize;
    
    JRSStreamFactory sf = new JRSStreamFactory(fs, srcFile, parityFs, parityFile, stripeIdx);
    
    int[] erasedLocations = new int[1];
    erasedLocations[0] = ((int)(errorOffset/blockSize)) % stripeSize;

    FSDataInputStream[] inputs = new FSDataInputStream[stripeSize + paritySize];

    sf.buildStream(inputs, erasedLocations);
    
    byte[] buf = new byte[(int)limit];

    startTime = System.nanoTime();

    writeFixedBlock(inputs, erasedLocations, limit, buf, sf);

    endTime = System.nanoTime();
    LOG.info("anchor Degraded_time "+(-startTime + endTime));
    out.write(buf, 0, (int)limit);
      }

  /**
   * builder
   * keep the information that need by PMDegradedReadDecoder/PMRecoveryDecoder to recovery a stripe
   */
  class DecodePackage{
    public long[] limits;
    public int[] erasedLocations;
    public byte[] outBuf;
    public long limit;
    public File[] lfs;
    public int target;
    ByteBuffer buf;
    DecodePackage(int[] erasedLocations, ByteBuffer buf){
      this.erasedLocations = erasedLocations;
      this.buf = buf;
    }
    DecodePackage limit(long limit){
      this.limit = limit;
      return this;
    }
    DecodePackage limits(long[] limits){
      this.limits = limits;
      return this;
    }
    DecodePackage localFiles(File[] lfs){
      this.lfs = lfs;
      return this;
    }
    DecodePackage outputBuffer(byte[] outBuf){
      this.outBuf = outBuf;
      return this;
    }
    DecodePackage target(int target){
      this.target = target;
      return this;
    }
  }


  void writeFixedBlock(
      FSDataInputStream[] inputs,
      int[] erasedLocations,
      long limit,
      byte[] outBuf,
      JRSStreamFactory sf
      ) throws IOException {

    // Loop while the number of skipped + read bytes is less than the max.
    int seq = 0;

    int target = erasedLocations[0];

    for (long read = 0; read < limit; ) {

      int failNum = erasedLocations.length;
      int bufOffset = bufSize*stripeSize;
      ByteBuffer buf = ByteBuffer.allocate(bufOffset+64);
      buf.putInt(bufOffset, seq);

      boolean important = false;

      //last threadNum# packet checked
      if((limit-read + bufSize - 1)/bufSize <= threadNum){
        important = true;
        buf.put(bufOffset+4, (byte)1);
      }else{
        buf.put(bufOffset+4, (byte)0);
      }
      LOG.info("anchor Decode_stripe "+seq+" Data_reading "+System.nanoTime());
      //read packets
      buf.rewind();
      erasedLocations = readFromInputs(inputs, erasedLocations, buf, sf, seq);
      LOG.info("anchor Decode_stripe "+seq+" Data_read "+System.nanoTime());

      int toRead = (int)Math.min((long)bufSize, limit - read);

      buf.rewind();

      //finding the best ring buffer
      int remain = -1; 
      int chosen = -1; 
      for(int i =0; i<threadNum; i++){
        int rc = q[i].remainingCapacity();
        if(remain < rc){
          remain = rc; 
          chosen = i;
        }
      }
      if(important){
        chosen = (((int)(limit - read) + bufSize - 1)/bufSize - 1)%threadNum;
      }
      DecodePackage dp = (new DecodePackage(erasedLocations, buf))
        .limit(limit).outputBuffer(outBuf).target(target);
      //dispatch
      boolean flag = true;
      while(flag){
        flag = false;
        try{
          q[chosen].put(dp);
        }catch(InterruptedException e){
          flag = true;
        }
      }
      LOG.info("anchor Decode_stripe "+seq+" Data_pushed "+System.nanoTime());

      seq++;
      read += toRead;
    }

    //waiting for the end of the decode
    for(int i=0; i<threadNum; i++){
      boolean flag = true;
      while(flag){
        flag = false;
        try{
          p[i].take();
        }catch(InterruptedException e){
          flag = true;
        }
      }
    }
  }



  void writeFixedBlock(
      FSDataInputStream[] inputs,
      int[] erasedLocations,
      Map<Integer, LocatedBlock> corruptStripe,
      File[] lbfs, long[] limits,
      JRSStreamFactory sf
      ) throws IOException {

    long limit = 0;

    for(int i = 0; i<limits.length; i++)
      if(limit < limits[i])
        limit = limits[i];

    // Loop while the number of skipped + read bytes is less than the max.
    int seq = 0;

    for (long read = 0; read < limit; ) {

      int failNum = erasedLocations.length;
      int bufOffset = bufSize*stripeSize;
      ByteBuffer buf = ByteBuffer.allocate(bufOffset+64);
      buf.putInt(bufOffset, seq);

      boolean important = false;

      //last threadNum# packet checked
      if((limit-read + bufSize - 1)/bufSize <= threadNum){
        important = true;
        buf.put(bufOffset+4, (byte)1);
      }else{
        buf.put(bufOffset+4, (byte)0);
      }
      LOG.info("anchor Decode_stripe "+seq+" Data_reading "+System.nanoTime());
      //read packets
      buf.rewind();
      erasedLocations = readFromInputs(inputs, erasedLocations, buf, sf, seq);
      LOG.info("anchor Decode_stripe "+seq+" Data_read "+System.nanoTime());

      int toRead = (int)Math.min((long)bufSize, limit - read);

      buf.rewind();

      //finding the best ring buffer
      int remain = -1; 
      int chosen = -1; 
      for(int i =0; i<threadNum; i++){
        int rc = q[i].remainingCapacity();
        if(remain < rc){
          remain = rc; 
          chosen = i;
        }
      }
      if(important){
        chosen = (((int)(limit - read) + bufSize - 1)/bufSize - 1)%threadNum;
      }

      DecodePackage dp = (new DecodePackage(erasedLocations, buf))
        .limits(limits).localFiles(lbfs);

      //dispatch
      boolean flag = true;
      while(flag){
        flag = false;
        try{
          q[chosen].put(dp);
        }catch(InterruptedException e){
          flag = true;
        }
      }
      LOG.info("anchor Decode_stripe "+seq+" Data_pushed "+System.nanoTime());

      seq++;
      read += toRead;
    }

    //waiting for the end of the decode
    for(int i=0; i<threadNum; i++){
      boolean flag = true;
      while(flag){
        flag = false;
        try{
          p[i].take();
        }catch(InterruptedException e){    
          flag = true;
        }
      }

    } 

  }

  int[] readFromInputs(
      FSDataInputStream[] inputs,
      int[] erasedLocations,
      ByteBuffer buf,
      JRSStreamFactory sf,
      int seq) throws IOException {
    boolean flag = true;
    while(flag){
      flag = false;
      // For every input, read some data = bufSize
      for (int i = 0, j = 0, k = 0; i < inputs.length && k < stripeSize; i++) {
        if(j >= erasedLocations.length || i != erasedLocations[j]){
          try {
            LOG.info("read input:"+i+" raw bs:"+bufSize+" "+System.nanoTime());
            RaidUtils.readTillEnd(inputs[i], readBufs[i], bufSize, true);
            k++;
            continue;
          } catch (BlockMissingException e) {
            LOG.error("Encountered BlockMissingException in stream " + i);
          } catch (ChecksumException e) {
            LOG.error("Encountered ChecksumException in stream " + i);
          }
      }else{
        j++;
        continue;
      }

      if (erasedLocations.length == paritySize) {
        String msg = "Too many read errors";
        LOG.error(msg);
        throw new IOException(msg);
      }
      
      int[] newErasedLocations = new int[erasedLocations.length + 1];
      for (int t = 0; t < erasedLocations.length; t++) {
        newErasedLocations[t] = erasedLocations[t];
      }
      newErasedLocations[newErasedLocations.length - 1] = i;
      erasedLocations = newErasedLocations;
      Arrays.sort(erasedLocations);

      LOG.info("Rebuild streams");
      sf.closeStreams(inputs);
      sf.buildStream(inputs, erasedLocations, seq*bufSize);
      //reset
      startTime = System.nanoTime();
      flag = true;
      break;
      }
    }

    LOG.info("end read raw bs:"+bufSize+" "+System.nanoTime());

    for (int i = 0, j = 0, k = 0; i < inputs.length && k < stripeSize; i++)
      if(j >= erasedLocations.length || i != erasedLocations[j]){
        buf.put(readBufs[i], 0, bufSize);
        k++;
      }else
        j++;
    LOG.info("end import raw bs:"+bufSize+" "+System.nanoTime());
    return erasedLocations;
      }

  class JRSDegradedReadDecoder implements Runnable{
    private int[] locations = null;
    private final ByteBuffer inBuf;
    private final ByteBuffer outBuf;
    private final int idx;
    private final ByteBuffer temp;

    native void jrsDr(ByteBuffer in, ByteBuffer out, int k, int m, int n,
        int[] locations, int target, int bufSize, ByteBuffer temp, boolean doReconstruct);

    JRSDegradedReadDecoder(int idx){
      inBuf = ByteBuffer.allocateDirect(bufSize*stripeSize+64);
      outBuf = ByteBuffer.allocateDirect(bufSize+64);
      temp = ByteBuffer.allocateDirect(1024);
      this.idx = idx;
    }

    public void run(){
      while(true){
        DecodePackage pkg = null;
        try{
          pkg = (DecodePackage)(q[idx].take());
        }catch(InterruptedException e){
          continue;
        }

        inBuf.put(pkg.buf);
        inBuf.rewind();

        int inBufOffset = bufSize * stripeSize;
        byte flag = inBuf.get(4+inBufOffset);
        LOG.info("in seq:"+inBuf.getInt(inBufOffset));

        boolean doReconstruct = false;
        if(!Arrays.equals(this.locations, pkg.erasedLocations)){
          doReconstruct = true;
          this.locations = pkg.erasedLocations;
        }

        for(int i = 0; i<locations.length; i++)
          LOG.info("location "+i+": "+locations[i]);
        LOG.info("target:"+pkg.target);
        LOG.info("doReconstruct: "+doReconstruct);
        LOG.info("stripeSize:"+stripeSize);
        LOG.info("paritySize:"+paritySize);
        jrsDr(inBuf, outBuf, stripeSize, paritySize
          , locations.length, locations, pkg.target, bufSize
          , temp, doReconstruct);

        int outBufOffset = bufSize;

        int seq = outBuf.getInt(outBufOffset);

        LOG.info("out seq:"+seq);
        outBuf.rewind();

        int len = Math.min((int)(pkg.limit-seq*bufSize), bufSize);
        outBuf.get(pkg.outBuf, seq*bufSize, len);

        while(flag == (byte)1){
          try{
            p[idx].put(1);
            flag = (byte)0;
          }catch(InterruptedException e){
            LOG.warn(e);
          }
        }
      }
    }
  }

  class JRSRecoveryDecoder implements Runnable{
    private RandomAccessFile[] outs = null;
    private int[] locations = null;
    private final ByteBuffer inBuf;
    private final ByteBuffer outBuf;
    private final int idx;
    private final ByteBuffer temp;
    private int[] initLocations = null;

    native void jrsDecode(ByteBuffer in, ByteBuffer out, int k, int m, int n,
        int[] location, int bufSize, ByteBuffer temp, boolean doReconstruct);

    JRSRecoveryDecoder(int idx){
      inBuf = ByteBuffer.allocateDirect(bufSize*stripeSize+64);
      outBuf = ByteBuffer.allocateDirect(bufSize*paritySize+64);
      temp = ByteBuffer.allocateDirect(1024);
      this.idx = idx;
    }

    public void run(){
      while(true){
        DecodePackage pkg = null;
        try{
          pkg = (DecodePackage)(q[idx].take());
        }catch(InterruptedException e){
          continue;
        }

        inBuf.put(pkg.buf);
        inBuf.rewind();

        int length = pkg.erasedLocations.length;

        int inBufOffset = bufSize * stripeSize;
        byte flag = inBuf.get(4+inBufOffset);
        int s = inBuf.getInt(inBufOffset);
        LOG.info("in seq: "+inBuf.getInt(inBufOffset)+" with offset: "+inBufOffset+" buf Size: "+bufSize);

        boolean doReconstruct = false;
        if(!Arrays.equals(this.locations, pkg.erasedLocations)){
          doReconstruct = true;
          this.locations = pkg.erasedLocations;
        }

        LOG.info("anchor Decode_stripe "+s+" Data_decoding "+System.nanoTime());
        jrsDecode(inBuf, outBuf, stripeSize, paritySize
            , locations.length, locations, bufSize
            , temp, doReconstruct);
        LOG.info("anchor Decode_stripe "+s+" Data_decoded "+System.nanoTime());

        File[] fs = pkg.lfs;

        if(outs == null){
          try{
            outs = new RandomAccessFile[fs.length];
            initLocations = pkg.erasedLocations;
            for(int i = 0; i<fs.length; i++)
              outs[i] = new RandomAccessFile(fs[i], "rw");
          }catch(IOException e){
            //need to handle this
            LOG.error("IOException in JRSRecoveryDecoder");
            return;
          }
        }

        int outBufOffset = bufSize*pkg.erasedLocations.length;

        int seq = outBuf.getInt(outBufOffset);
        LOG.info("out seq: "+seq);

        outBuf.rewind();

        byte[] bufarr = new byte[outBuf.remaining()];
        outBuf.get(bufarr, 0, bufarr.length);
        try{
          for(int i = 0, j = 0; i < outs.length; i++){
            while(locations[j] != initLocations[i])
              j++;
            if(seq*bufSize < pkg.limits[i]){
              outs[i].seek(seq*bufSize);
              int len = Math.min((int)(pkg.limits[i]-seq*bufSize), bufSize);
              outs[i].write(bufarr, j*bufSize, len);
            }
            j++;
          }
        }catch(IOException e){
          LOG.error("Unexpected IOException in line 622 "+e);
        }

        while(flag == (byte)1){
          try{
            for(int i = 0; i<outs.length; i++){
              outs[i].close();
              outs[i] = null;
            }
            outs = null;
          }catch(IOException e){
            LOG.error("Unexpected IOException in line 614 "+e);
            return;
          }
          try{
            p[idx].put(1);
            flag = (byte)0;
          }catch(InterruptedException e){
            LOG.warn(e);
          }
        }
      }
    }
  }
}
