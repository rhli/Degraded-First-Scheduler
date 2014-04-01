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

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.io.File;
import java.io.FileInputStream;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.util.Progressable;
import java.nio.ByteBuffer;
import java.util.concurrent.*;

/**
 * encoder is only for migration
 */
public class JRSEncoder extends Encoder {

  private int threadNum = 2;

  static{
    System.loadLibrary("jrsencoder");
  }

  //data queue, from input thread to encode thread
  private ArrayBlockingQueue[] q;
  //trigger queue, input to encode
  private ArrayBlockingQueue[] fq;
  //trigger queue, encode to output
  private ArrayBlockingQueue[] p;

  public static final Log LOG = LogFactory.getLog(
      "org.apache.hadoop.raid.JRSEncoder");
  public JRSEncoder(
      Configuration conf, int stripeSize, int paritySize) {
    super(conf, stripeSize, paritySize);
    
    threadNum = conf.getInt("hdfs.raid.encoder.threadnum", 2);

    this.q = new ArrayBlockingQueue[threadNum];
    for(int i=0; i<threadNum; i++)
      q[i] = new ArrayBlockingQueue<ByteBuffer>(1024);

    this.p = new ArrayBlockingQueue[threadNum];
    for(int i=0; i<threadNum; i++)
      p[i] = new ArrayBlockingQueue<Integer>(100);

    this.fq = new ArrayBlockingQueue[threadNum];
    for(int i=0; i<threadNum; i++)
      fq[i] = new ArrayBlockingQueue< byte[][] >(2);
    
    //encode thread
    JRSMigrationEncoder[] encoder = new JRSMigrationEncoder[threadNum];
    Thread[] es = new Thread[threadNum];
    for(int i = 0; i < threadNum; i++){
      encoder[i] = new JRSMigrationEncoder(i);
      es[i] = new Thread(encoder[i]);
      es[i].start();
    }

    LOG.info("JRSEncoder 21/10/12");
  }
  
  protected void encodeFileToStream(
      FileSystem fs, 
      Path srcFile, 
      long srcSize,
      long blockSize, 
      OutputStream out, 
      Progressable reporter) throws IOException {
    // (disable) One parity block can be written directly to out, rest to local files.
    //tmpOuts[0] = out;


    //File[] tmpFiles = new File[paritySize];
    byte[][] bufs = new byte[paritySize][];

    /*
     * signal queue to trigger ouput
     * No need blocking queue (adjust in the future)
     */
    BlockingQueue<byte[]> closedBuf = new ArrayBlockingQueue<byte[]>(14);

    /*
     * Output thread
     */
    DataSender ds = new DataSender(closedBuf, out, blockSize, srcSize);
    Thread dst = new Thread(ds);
    dst.start();

    // Loop over stripes in the file.
    for (long stripeStart = 0; stripeStart < srcSize;
        stripeStart += blockSize * stripeSize) {
      reporter.progress();

      LOG.info("Starting encoding of stripe " + srcFile + ":" + stripeStart);

      /*
       * create temp file to write parity block (one file for each block)
       */
      for (int i = 0; i < paritySize; i++) {
        //tmpFiles[i] = File.createTempFile("parity", "_" + i); 
        //LOG.info("Created tmp file " + tmpFiles[i]);
        //tmpFiles[i].deleteOnExit();
        bufs[i] = new byte[(int)blockSize];
      }

      // Create input streams for blocks in the stripe.
      InputStream[] blocks = stripeInputs(fs, srcFile, stripeStart,
          srcSize, blockSize);

      /*
       * encode data
       */
      encodeStripe(blocks, stripeStart, blockSize, bufs, reporter);

      /*
       * triger output
       */
      for (int i = 0; i < paritySize; i++) {
        try{
          closedBuf.put(bufs[i]);
        }catch(InterruptedException e){
        }
        reporter.progress();
      }   
        }

    try{
      //waiting for the end of output
      dst.join();
    }catch(InterruptedException e){
      LOG.info("thread join interrupted");
    }
      }

  /*
   * Class that takes charged of sending coding data
   */
  class DataSender implements Runnable{
    //signal queue
    BlockingQueue<byte[]> q;

    //buf
    byte[] buf = new byte[bufSize];

    //block size
    long bs;

    //output stream
    OutputStream out;

    //number parity block
    long mc;

    DataSender(BlockingQueue<byte[]> q, OutputStream out, long bs, long ss){
      this.q = q;
      this.out = out;
      this.bs = bs;
      mc = (ss + bs - 1)/bs;
      mc = (mc + stripeSize - 1) / stripeSize * paritySize;
    }

    public void run(){
      long count = 0;

      //loop for sending parity block
      while(count < mc){

        if(count % paritySize == 0){
          try{
            // trigger
            // indicate the end of the encode thread
            for(int i=0; i<threadNum; i++)
              p[i].take();
          }catch(InterruptedException e){
          }
        }


        byte[] f = null;
        try{
          // trigger
          // indicate the end of the input thread
          // get the file information
          f = q.take();
        }catch(InterruptedException e){
        }

        LOG.info("anchor Output_parity "+count+" Start_output "+System.nanoTime());

        try{
          //InputStream in  = new FileInputStream(f);
          //RaidUtils.copyBytes(in, out, buf, bs);
          out.write(f, 0, (int)bs);
          //in.close();
          //f.delete();
          //LOG.info("Deleted tmp file " + f);
        }catch(IOException e){
          LOG.info("Unexpected IOException in JRSEncode line 140 "+e);
        }
        LOG.info("anchor Output_parity "+count+" End_output "+System.nanoTime());
        count++;
      }
    }
  }

  protected void encodeStripe(
      InputStream[] blocks,
      long stripeStartOffset,
      long blockSize,
      OutputStream[] outs,
      Progressable reporter) throws IOException {
    LOG.error("Unexpect call on encodeStripe");
      }

  protected void encodeStripe(
      InputStream[] blocks,
      long stripeStartOffset,
      long blockSize,
      byte[][] bufs,
      Progressable reporter) throws IOException {

    try{
      //trigger, pass file info
      for(int i=0; i<threadNum; i++)
        fq[i].put(bufs);
    }catch(InterruptedException e){
    }

    //seq number
    int s = 0;

    //number of data read
    int read = 0;

    //useless
    int cap = 1+11*threadNum;

    //ByteBuffer[] buf = new ByteBuffer[cap];
    //use buffer to pass data, can be replaced by Byte[]
    ByteBuffer buf;

    while(read < blockSize){
      //indecate the last threadNum# packet
      boolean important = false;

      //useless
      int idx = s%cap;
      //if(buf[idx] == null) buf[idx] = ByteBuffer.allocate(bufSize*stripeSize+5);
      
      //initial buffer
      buf = ByteBuffer.allocate(bufSize*stripeSize+64);
      //buf[idx].putInt(0, s);

      //seq number
      buf.putInt(stripeSize*bufSize, s);

      //check whether the last threadNum# packet
      if((blockSize - read + bufSize - 1)/bufSize <= threadNum){
        important = true;
        //buf[idx].put(4, (byte)1);
        buf.put(4+stripeSize*bufSize, (byte)1);
      }else{
        //buf[idx].put(4, (byte)0);
        buf.put(4+stripeSize*bufSize, (byte)0);
      }

      byte[] bufarr = buf.array();
      LOG.info("anchor Encode_stripe "+s+" Data_start_reading "+System.nanoTime());
      for(int i = 0; i < stripeSize; i++){
        try{
          //RaidUtils.readTillEnd(blocks[i], buf[idx].array(), true, 5+i*bufSize, bufSize);
          //read data
          RaidUtils.readTillEnd(blocks[i], bufarr, true, i*bufSize, bufSize);
        }catch(IOException e){
        }
      } 
      //LOG.info(s+" read: "+bufarr[5]+" "+bufarr[5+bufSize]+" "+bufarr[5+bufSize*2]);
      LOG.info("anchor Encode_stripe "+s+" Data_read "+System.nanoTime());
      //buf[idx].rewind();

      //update position
      buf.rewind();

      int remain = -1;
      int chosen = -1;
      //check the most free ring buffer
      for(int i =0; i<threadNum; i++){
        int rc = q[i].remainingCapacity();
        if(remain < rc){
          remain = rc;
          chosen = i;
        }
      }

      //decide to put the data to which ring buffer
      if(important){
        chosen = (((int)blockSize - read + bufSize - 1)/bufSize - 1)%threadNum;
      }

      //LOG.info("chosen number: "+chosen+" with seq: "+s+" and buf idx: "+idx);
      try{
        //out[chosen].put(buf[idx]);
        q[chosen].put(buf);
      }catch(InterruptedException e){
      }
      LOG.info("anchor Encode_stripe "+s+" Data_pushed "+System.nanoTime());

      //update status
      s++;
      read+=bufSize;
    }
      }

  @Override
    public Path getParityTempPath() {
      return new Path(RaidNode.jrsTempPrefix(conf));
    }

  /*
   * Class to take charged of encoding
   */
  class JRSMigrationEncoder implements Runnable{

    //private final RandomAccessFile[] outs = new RandomAccessFile[paritySize];
    
    private byte[][] bufs;

    private final ByteBuffer inBuf;
    private final ByteBuffer outBuf;

    //thread index
    private final int idx;

    native void jrsEncode(ByteBuffer in, ByteBuffer out, int k, int m, int bufSize);

    JRSMigrationEncoder(int i){
      inBuf = ByteBuffer.allocateDirect(bufSize*stripeSize+64);
      outBuf = ByteBuffer.allocateDirect(bufSize*paritySize+64);
      idx = i;
    }
    //private?
    public void take(){
      //LOG.info("JRSMigrationEncoder take called");
      inBuf.clear();

      try{
        inBuf.put((ByteBuffer)(q[idx].take()));
        inBuf.rewind();
      }catch(InterruptedException e){
      }
      LOG.info("anchor Encode_stripe "+inBuf.getInt(stripeSize*bufSize)+" Start_encoding "+System.nanoTime());
      //LOG.info(inBuf.getInt(0)+" take check point: "+inBuf.get(5)+" "+inBuf.get(5+1024*1024)+" "+inBuf.get(5+1024*1024*2));
    }

    public void push(){
      //LOG.info("JRSMigrationEncoder push called strange 3");
      int seq = outBuf.getInt(paritySize*bufSize);
      LOG.info("anchor Encode_stripe "+seq+" Encoded "+System.nanoTime());
      outBuf.rewind();

      //redundant, need to change
      //byte[] bufarr = new byte[outBuf.remaining()];

      //outBuf.get(bufarr, 0, bufarr.length);
      //LOG.info(outBuf.getInt(0)+" push check point: "+outBuf.get(4)+" "+outBuf.get(4+1024*1024));
      
      for (int i = 0; i < paritySize; i++) {
        //outs[i].seek(seq*bufSize);
        //outs[i].write(bufarr, 4+i*bufSize, bufSize);
        outBuf.get(bufs[i], seq*bufSize, bufSize);
      }
      
      //LOG.info("out buf seq number: "+seq);
      LOG.info("anchor Encode_stripe "+seq+" Parity_written "+System.nanoTime());
    }

    public void run(){
      //LOG.info("run jrsmigrationencoder");
      while(true){
        //File[] fs = null;
        try{
          //take signal
          bufs = (byte[][])(fq[idx].take());
        }catch(InterruptedException e){
        }

        //try{
        //  for(int i = 0; i < paritySize; i++)
        //    this.outs[i] = new RandomAccessFile(fs[i], "rw");
        //}catch(IOException e){
        //  LOG.error("Unexpected IOException in line 197 "+e);
        //}

        jrsEncode(inBuf, outBuf, stripeSize, paritySize, bufSize);

        //try{
        //  for(int i = 0; i < paritySize; i++){
        //    this.outs[i].close();
        //    this.outs[i] = null;
        //  }
        //}catch(IOException e){
        //  LOG.error("Unexpected IOException in line 244 "+e);
        //}

        //trigger output
        try{
          p[idx].put(1);
        }catch(InterruptedException e){
        }
      }
    }
  }

  /* 
   * No use
   */
  class SeqByteBuffer implements Comparable{
    private int seq;
    private ByteBuffer buf;
    public int getSeq(){return seq;}
    public SeqByteBuffer(ByteBuffer buf){
      seq = buf.getInt(0);
      this.buf = buf;
    }
    public ByteBuffer getBuf(){return buf;}
    public void update(ByteBuffer b){
      buf.put(b);
      buf.rewind();
      seq = b.getInt(0);
    }
    public int compareTo(Object o){
      int seq1 = this.seq;
      int seq2 = ((SeqByteBuffer)o).getSeq();
      if(seq1 == seq2)
        return 0;
      if(seq1 > seq2)
        return 1;
      return -1;
    }
  }

  /*
   * No use
   */
  class DataProducer implements Runnable{
    private final BlockingQueue<ByteBuffer>[] out;
    private final InputStream[] blocks;
    private final int blockSize;
    DataProducer(BlockingQueue<ByteBuffer>[] out, InputStream[] blocks, int blockSize){
      this.out = out;
      this.blocks = blocks;
      this.blockSize = blockSize;
    }
    public void run(){
      int s = 0;
      int read = 0;
      int cap = 1+11*threadNum;
      //ByteBuffer[] buf = new ByteBuffer[cap];
      ByteBuffer buf;
      while(read < blockSize){
        boolean important = false;
        int idx = s%cap;
        //if(buf[idx] == null) buf[idx] = ByteBuffer.allocate(bufSize*stripeSize+5);
        buf = ByteBuffer.allocate(bufSize*stripeSize+5);
        //buf[idx].putInt(0, s);
        buf.putInt(0, s);
        if((blockSize - read + bufSize - 1)/bufSize <= threadNum){
          important = true;
          //buf[idx].put(4, (byte)1);
          buf.put(4, (byte)1);
        }else{
          //buf[idx].put(4, (byte)0);
          buf.put(4, (byte)0);
        }
        byte[] bufarr = buf.array();
        LOG.info("anchor Encode_stripe "+s+" Data_start_reading "+System.nanoTime());
        for(int i = 0; i < stripeSize; i++){
          try{
            //RaidUtils.readTillEnd(blocks[i], buf[idx].array(), true, 5+i*bufSize, bufSize);
            RaidUtils.readTillEnd(blocks[i], bufarr, true, 5+i*bufSize, bufSize);
          }catch(IOException e){
          }
        }
        LOG.info("anchor Encode_stripe "+s+" Data_read "+System.nanoTime());
        //buf[idx].rewind();

        buf.rewind();
        int remain = -1;
        int chosen = -1;
        for(int i =0; i<threadNum; i++){
          int rc = out[i].remainingCapacity();
          if(remain < rc){
            remain = rc;
            chosen = i;
          }
        }
        if(important){
          chosen = ((blockSize - read + bufSize - 1)/bufSize - 1)%threadNum;
        }
        //LOG.info("chosen number: "+chosen+" with seq: "+s+" and buf idx: "+idx);
        try{
          //out[chosen].put(buf[idx]);
          out[chosen].put(buf);
        }catch(InterruptedException e){
        }
        LOG.info("anchor Encode_stripe "+s+" Data_pushed "+System.nanoTime());
        
        s++;
        read+=bufSize;
      }
    }
  }
}
