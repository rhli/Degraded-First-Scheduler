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

//iadecoder is not supposed to be access by multiple threads.
public class IADecoder extends Decoder {

	static{
    //native library for ia recovery
    System.loadLibrary("iadecoder");

    System.loadLibrary("iavalidate");
	}

  native void iaValidate(int k, int m, int n, int[] locations, int[] validLocations);

  //number of threads
  private int threadNum = 1;

  //data queue, input to decode
  private BlockingQueue[] q;

  //signal queue, decode to output
  private BlockingQueue[] p;

	private static final Log LOG = LogFactory.getLog(
			"org.apache.hadoop.raid.IADecoder");

  private FSDataInputStream[] inputs = null;

  //bufsize of request data
  private int encodedBufSize;

  private long startTime;
  private long endTime;

  /**
   * @param forRecovery determine the type of this decoder, for recovery or for degraded read 
   *   (someday, i may combine them into the same function)
   */
	public IADecoder(
			Configuration conf, int stripeSize, int paritySize, boolean forRecovery) {
    super(conf, stripeSize, paritySize);
    LOG.info("initial decoder: k="+stripeSize+" m="+paritySize+" bufSize:"+bufSize);
    
    inputs = new FSDataInputStream[stripeSize + paritySize];

    threadNum = conf.getInt("hdfs.raid.decoder.threadnum", 1); 

    //data queue, input to decode
    this.q = new BlockingQueue[threadNum];
    for(int i=0; i<threadNum; i++)
      q[i] = new ArrayBlockingQueue<DecodePackage>(1024/paritySize);

    //signal queue, decode to output
    this.p = new BlockingQueue[threadNum];
    for(int i=0; i<threadNum; i++)
      p[i] = new ArrayBlockingQueue<Integer>(100);

    Thread[] ds = new Thread[threadNum];
    for(int i = 0; i < threadNum; i++){
      if(forRecovery){
        IARecoveryDecoder decoder = new IARecoveryDecoder(i);
        ds[i] = new Thread(decoder);
      }else{
        IADegradedReadDecoder decoder = new IADegradedReadDecoder(i);
        ds[i] = new Thread(decoder);
      }
      ds[i].start();
    }

    LOG.info("IADecoder 27/1");

      }

  /*
   * fix erased block in a stripe. for recovery.
   */
  protected void fixErasedBlock(
      FileSystem fs, Path srcFile,
      FileSystem parityFs, Path parityFile,
      long blockSize, Map<Integer, LocatedBlock> corruptStripe,
      File[] lbfs, int stripeIdx) throws IOException {
    Set<Integer> tmploc = corruptStripe.keySet();
    int[] erasedLocations = new int[tmploc.size()];
    int idx = 0;
    for(Integer loc: tmploc){
      erasedLocations[idx++] = loc;
    }
    Arrays.sort(erasedLocations);

    int[] temp = new int[stripeSize+paritySize+1];

    iaValidate(stripeSize, paritySize, erasedLocations.length, erasedLocations, temp);
    LOG.info("iaValidate pass 1");
    int[] validErasedLocations = new int[temp[0]];
    System.arraycopy(temp, 1, validErasedLocations, 0, temp[0]);
    Arrays.sort(validErasedLocations);
    if(erasedLocations.length != validErasedLocations.length)
      LOG.info("fail pattern is invalidate");

    encodedBufSize = bufSize*validErasedLocations.length/paritySize;

    IAStreamFactory sf = new IAStreamFactory(fs, srcFile, parityFs, parityFile, stripeIdx);
    sf.buildStream(inputs, validErasedLocations);
    
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
    writeFixedBlock(inputs, erasedLocations, validErasedLocations, corruptStripe, lbfs, limits, sf);
  }

  /**
   * IAStreamFactory is for generating FSDataInputStreams
   * The streams need to be regenerated if the validErasedLocations is changed
   */
  class IAStreamFactory{
    private FileSystem fs = null;
    private Path srcFile = null;
    private FileSystem parityFs = null;
    private Path parityFile = null;
    private long stripeIdx = -1;

    public IAStreamFactory(
        FileSystem fs, Path srcFile, FileSystem parityFs, Path parityFile,
        long stripeIdx){
      this.fs = fs;
      this.parityFs = parityFs;
      this.srcFile = srcFile;
      this.parityFile = parityFile;
      this.stripeIdx = stripeIdx;
        }

    public void buildStream(FSDataInputStream[] inputs, int[] validErasedLocations)
      throws IOException{
      buildStream(inputs, validErasedLocations, 0);
    }

    public void buildStream(FSDataInputStream[] inputs, int[] validErasedLocations, int skipInBlock)
      throws IOException{
      FileStatus srcStat = fs.getFileStatus(srcFile);
      FileStatus parityStat = parityFs.getFileStatus(parityFile);
      
      long blockSize = srcStat.getBlockSize();
      
      for(int i = 0, j = 0; i < inputs.length; i++){
        long offset = 0;
        if(i < stripeSize)
          offset = blockSize * (stripeIdx * stripeSize + i);
        else
          offset = blockSize * (stripeIdx * paritySize + i - stripeSize);
        if(j >= validErasedLocations.length || i != validErasedLocations[j]){
          if(i < stripeSize){
            if (offset > srcStat.getLen()){
              inputs[i] = new FSDataInputStream(new RaidUtils.ZeroInputStream(
                    offset + blockSize));
            }else{
              //if(inputs[i] == null)
                inputs[i] = fs.open(
                  srcFile, conf.getInt("io.file.buffer.size", 64 * 1024), stripeSize,
                  paritySize, validErasedLocations.length, validErasedLocations, "ia");
              LOG.info("Opening stream at " + srcFile);
              inputs[i].seek(offset);
              LOG.info("Seeking stream at " + srcFile + ":" + offset);
            }
          }else{
            //if(inputs[i] == null)
              inputs[i] = parityFs.open(
                parityFile, conf.getInt("io.file.buffer.size", 64 * 1024), stripeSize,
                paritySize, validErasedLocations.length, validErasedLocations, "ia");
            LOG.info("Opening stream at " + srcFile);
            inputs[i].seek(offset);
            LOG.info("Seeking stream at " + srcFile + ":" + offset);
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

  /**
   * fix Erased block, for degraded read
   */
  public void fixErasedBlock(
      FileSystem fs, Path srcFile, FileSystem parityFs, Path parityFile,
      long blockSize, long errorOffset, long limit,
      OutputStream out) throws IOException{
    int stripeIdx = (int)(errorOffset / blockSize / stripeSize);

    IAStreamFactory sf = new IAStreamFactory(fs, srcFile, parityFs, parityFile, stripeIdx);

    int[] erasedLocations = new int[1];
    erasedLocations[0] = ((int)(errorOffset/blockSize)) % stripeSize;
    int[] temp = new int[stripeSize+paritySize+1];
    iaValidate(stripeSize, paritySize, erasedLocations.length, erasedLocations, temp);
    LOG.info("iaValidate pass 2");
    int[] validErasedLocations = new int[temp[0]];
    System.arraycopy(temp, 1, validErasedLocations, 0, temp[0]);

    FSDataInputStream[] inputs = new FSDataInputStream[stripeSize + paritySize];

    encodedBufSize = bufSize*validErasedLocations.length/paritySize;

    sf.buildStream(inputs, validErasedLocations);

    byte[]  buf = new byte[(int)limit];

    startTime = System.nanoTime();
    writeFixedBlock(inputs, erasedLocations, validErasedLocations, limit, buf, sf);

    endTime = System.nanoTime();
    LOG.info("anchor Degraded_time "+(-startTime + endTime));
    out.write(buf, 0, (int)limit);

  }

  /**
   * builder
   * keep the information that need by IADegradedReadDecoder/IARecoveryDecoder to recovery a stripe
   */
  class DecodePackage{
    public long[] limits;
    public int[] erasedLocations;
    public byte[] outBuf;
    public long limit;
    public File[] lfs;
    public int[] validErasedLocations;
    ByteBuffer buf;
    DecodePackage(int[] erasedLocations, int[] validErasedLocations, ByteBuffer buf){
      this.erasedLocations = erasedLocations;
      this.validErasedLocations = validErasedLocations;
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
  }

  class ReadPackage{
    public final int[] validErasedLocations;
    public final ByteBuffer buf;
    ReadPackage(int[] l, ByteBuffer b){
      this.validErasedLocations = l;
      this.buf = b;
    }
  }

  void writeFixedBlock(
      FSDataInputStream[] inputs,
      int[] erasedLocations,
      int[] validErasedLocations,
      long limit,
      byte[] outBuf,
      IAStreamFactory sf
      ) throws IOException {

    int seq = 0;

		for (long read = 0; read < limit; ) {

      boolean important = false;
      
      LOG.info("anchor Decode_stripe "+seq+" Data_reading "+System.nanoTime());
      //read packets
			
      ReadPackage rp = readFromInputs(inputs, validErasedLocations, sf, seq);
      ByteBuffer buf = rp.buf;
      validErasedLocations = rp.validErasedLocations;
      int bufOffset = encodedBufSize*(stripeSize+paritySize-validErasedLocations.length);
      LOG.info("anchor Decode_stripe "+seq+" Data_read "+System.nanoTime());
      buf.rewind();

      //last threadNum# packet checked
      if((limit-read + bufSize - 1)/bufSize <= threadNum){
        important = true;
        buf.put(bufOffset+4, (byte)1);
      }else{
        buf.put(bufOffset+4, (byte)0);
      }
			
      int toRead = (int)Math.min((long)bufSize, limit - read);

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
        chosen =(int)(((limit - read + bufSize - 1)/bufSize - 1)%threadNum);
      }

      DecodePackage dp = (new DecodePackage(erasedLocations, validErasedLocations, buf))
        .limit(limit).outputBuffer(outBuf);
      //dispatch
      boolean flag = true;
      while(flag){
        flag = false;
        try{
          q[chosen].put(dp);
        }catch(InterruptedException e){
          Thread.currentThread().interrupt();
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
          Thread.currentThread().interrupt();
          flag = true;
        }
      }
    }
  }



  void writeFixedBlock(
      FSDataInputStream[] inputs,
      int[] erasedLocations,
      int[] validErasedLocations,
      Map<Integer, LocatedBlock> corruptStripe,
      File[] lbfs,
      long[] limits,
      IAStreamFactory sf
      ) throws IOException {

    long limit = 0;

    for(int i = 0; i<limits.length; i++)
      if(limit < limits[i])
        limit = limits[i];

    int seq = 0;

		for (long read = 0; read < limit; ) {

      boolean important = false;
      
      LOG.info("anchor Decode_stripe "+seq+" Data_reading "+System.nanoTime());
      //read packets
      ReadPackage rp = readFromInputs(inputs, validErasedLocations, sf, seq);
      ByteBuffer buf = rp.buf;
      validErasedLocations = rp.validErasedLocations;
      int bufOffset = encodedBufSize*(stripeSize+paritySize-validErasedLocations.length);
      LOG.info("anchor Decode_stripe "+seq+" Data_read "+System.nanoTime());

      //last threadNum# packet checked
      if((limit-read + bufSize - 1)/bufSize <= threadNum){
        important = true;
        buf.put(bufOffset+4, (byte)1);
      }else{
        buf.put(bufOffset+4, (byte)0);
      }

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
        chosen = (int)((((limit - read) + bufSize - 1)/bufSize - 1)%threadNum);
      }

      DecodePackage dp = (new DecodePackage(erasedLocations, validErasedLocations, buf))
        .limits(limits).localFiles(lbfs);
      //dispatch
      boolean flag = true;
      while(flag){
        flag = false;
        try{
          q[chosen].put(dp);
        }catch(InterruptedException e){
          Thread.currentThread().interrupt();
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
          Thread.currentThread().interrupt();
          flag = true;
        }
      }

    }
  }

  ReadPackage readFromInputs(
      FSDataInputStream[] inputs,
      int[] validErasedLocations,
      IAStreamFactory sf,
      int seq) throws IOException {
    boolean flag = true;
    while(flag){
      flag = false;
      // For every input, read some data = bufSize
      for (int i = 0, j = 0; i < inputs.length; i++) {
        if(j >= validErasedLocations.length || i != validErasedLocations[j]){
          try {
            LOG.info("read input:"+i+" encoded bs:"+encodedBufSize+" "+System.nanoTime());
            RaidUtils.readTillEnd(inputs[i], readBufs[i], encodedBufSize, true);
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

        // too many fails
        if (validErasedLocations.length == paritySize) {
          String msg = "Too many read errors";
          LOG.error(msg);
          throw new IOException(msg);
        }

        // read fail, need to rebuild the stream.
        int[] newErasedLocations = new int[validErasedLocations.length + 1];
        for (int k = 0; k < validErasedLocations.length; k++) {
          newErasedLocations[k] = validErasedLocations[k];
        }
        newErasedLocations[newErasedLocations.length - 1] = i;
        int[] temp = new int[stripeSize+paritySize+1];
        iaValidate(stripeSize, paritySize, newErasedLocations.length, newErasedLocations, temp);
        LOG.info("iaValidate pass 3");
        validErasedLocations = new int[temp[0]];
        encodedBufSize = bufSize*validErasedLocations.length/paritySize;
        System.arraycopy(temp, 1, validErasedLocations, 0, temp[0]);
        Arrays.sort(validErasedLocations);

        sf.closeStreams(inputs);
        sf.buildStream(inputs, validErasedLocations, seq*bufSize);
        //reset
        startTime = System.nanoTime();
        flag = true;
        break;
      }
    }


    int failNum = validErasedLocations.length;
    int bufOffset = encodedBufSize*(stripeSize+paritySize-failNum);
    ByteBuffer buf = ByteBuffer.allocate(bufOffset+64);
    buf.putInt(bufOffset, seq);
    buf.rewind();

    LOG.info("end read encoded bs:"+encodedBufSize+" "+System.nanoTime());
    for (int i = 0, j = 0; i < inputs.length; i++)
      if(j >= validErasedLocations.length || i != validErasedLocations[j])
        buf.put(readBufs[i], 0, encodedBufSize);
      else
        j++;
    return new ReadPackage(validErasedLocations, buf);
      }

  class IADegradedReadDecoder implements Runnable{
    private int[] locations = null;
    private final ByteBuffer inBuf;
    private final ByteBuffer outBuf;
    private final int idx;
    private final ByteBuffer temp;

    native void iaDecode(ByteBuffer in, ByteBuffer out, int k, int m, int n,
        int[] validLocations, int[] locations, int bufSize, ByteBuffer temp, boolean doReconstruct);

    IADegradedReadDecoder(int idx){
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
          Thread.currentThread().interrupt();
          continue;
        }

        inBuf.put(pkg.buf);
        inBuf.rewind();

        int length = pkg.validErasedLocations.length;
        int inBufOffset = bufSize * (paritySize + stripeSize - length)*length/paritySize;
        byte flag = inBuf.get(4+inBufOffset);
        int s = inBuf.getInt(inBufOffset);
        LOG.info("stripeSize: "+stripeSize+" paritySize: "+paritySize+" length: "+length);
        LOG.info("in seq: "+inBuf.getInt(inBufOffset)+" with offset: "+inBufOffset+" buf Size: "+bufSize);

        boolean doReconstruct = false;
        if(!Arrays.equals(this.locations, pkg.validErasedLocations)){
          LOG.info("Do Reconstruct");
          doReconstruct = true;
          this.locations = pkg.validErasedLocations;
        }
        for(int i = 0; i<locations.length; i++)
          LOG.info("location: "+i+" "+locations[i]);
        LOG.info("actual location: "+pkg.erasedLocations.length);
        for(int i = 0; i<pkg.erasedLocations.length; i++)
          LOG.info("a location: "+i+" "+pkg.erasedLocations.length);

        LOG.info("anchor Decode_stripe "+s+" Data_decoding "+System.nanoTime());
        iaDecode(inBuf, outBuf, stripeSize, paritySize
          , locations.length, locations, pkg.erasedLocations, bufSize
          , temp, doReconstruct);
        LOG.info("anchor Decode_stripe "+s+" Data_decoded "+System.nanoTime());

        int outBufOffset = bufSize;

        int seq = outBuf.getInt(outBufOffset);

        outBuf.rewind();

        int len = (int)Math.min((pkg.limit-seq*bufSize), (long)bufSize);
        outBuf.get(pkg.outBuf, seq*bufSize, len);

        while(flag == (byte)1){
          try{
            p[idx].put(1);
            flag = (byte)0;
          }catch(InterruptedException e){
            Thread.currentThread().interrupt();
            LOG.warn(e);
          }
        }
      }
    }
  }

  class IARecoveryDecoder implements Runnable{
    private RandomAccessFile[] outs = null;
    private int[] locations = null;
    private final ByteBuffer inBuf;
    private final ByteBuffer outBuf;
    private final int idx;
    private final ByteBuffer temp;
    private int[] initLocations = null;

    native void iaDecode(ByteBuffer in, ByteBuffer out, int k, int m, int n,
        int[] validLocations, int[] locations, int bufSize, ByteBuffer temp, 
        boolean doReconstruct);

    IARecoveryDecoder(int idx){
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
          Thread.currentThread().interrupt();
          continue;
        }

        inBuf.put(pkg.buf);
        inBuf.rewind();

        int length = pkg.validErasedLocations.length;

        int inBufOffset = bufSize * (paritySize + stripeSize -
            length)*length/paritySize;
        byte flag = inBuf.get(4+inBufOffset);
        int s = inBuf.getInt(inBufOffset);
        LOG.info("in seq: "+inBuf.getInt(inBufOffset)+" with offset: "+inBufOffset+" buf Size: "+bufSize);
        boolean doReconstruct = false;
        if(!Arrays.equals(this.locations, pkg.validErasedLocations)){
          doReconstruct = true;
          this.locations = pkg.validErasedLocations;
        }

        LOG.info("anchor Decode_stripe "+s+" Data_decoding "+System.nanoTime());
        iaDecode(inBuf, outBuf, stripeSize, paritySize
            , locations.length, locations, pkg.erasedLocations, bufSize
            , temp, doReconstruct);
        LOG.info("anchor Decode_stripe "+s+" Data_decoded "+System.nanoTime());

        File[] fs = pkg.lfs;

        if(outs == null){
          try{
            outs = new RandomAccessFile[fs.length];
            for(int i = 0; i<fs.length; i++)
              outs[i] = new RandomAccessFile(fs[i], "rw");
          }catch(IOException e){
            //need to handle this
            LOG.error("IOException in IARecoveryDecoder");
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
          for(int i = 0; i < outs.length; i++){
            if(seq*bufSize < pkg.limits[i]){
              outs[i].seek(seq*bufSize);
              int len = (int)Math.min(pkg.limits[i]-seq*bufSize, (long)bufSize);
              outs[i].write(bufarr, i*bufSize, len);
            }
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
            Thread.currentThread().interrupt();
            LOG.warn(e);
          }
        }
      }
    }
  }
}
