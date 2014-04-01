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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.util.Progressable;
import java.nio.ByteBuffer;

public class IAOEncoder extends Encoder {

	native void iaoEncode(ByteBuffer message, ByteBuffer parity, int k, int m, int bufferSize, ByteBuffer r);

  int seq;

	static{
		System.loadLibrary("iaoencoder");
	}

  public static final Log LOG = LogFactory.getLog(
                                  "org.apache.hadoop.raid.IAOEncoder");

  private final ByteBuffer reserve;
  private ByteBuffer dataBufs;
  private ByteBuffer parityBufs;
  public IAOEncoder(
    Configuration conf, int stripeSize, int paritySize) {
    super(conf, stripeSize, paritySize);
    LOG.info("bufsize: "+bufSize);
    dataBufs = ByteBuffer.allocateDirect(bufSize*stripeSize);
    parityBufs = ByteBuffer.allocateDirect(bufSize*paritySize);
    reserve = ByteBuffer.allocateDirect(1024);
  }

  protected void encodeStripe(
    InputStream[] blocks,
    long stripeStartOffset,
    long blockSize,
    OutputStream[] outs,
    Progressable reporter) throws IOException {

    int[] data = new int[stripeSize];
    int[] code = new int[paritySize];

    seq = 0;

    for (long encoded = 0; encoded < blockSize; encoded += bufSize) {
      // Read some data from each block = bufSize.
      LOG.info("anchor Encode_stripe "+seq+" Data_start_reading "+System.nanoTime());
      
      for (int i = 0; i < blocks.length; i++) {
        RaidUtils.readTillEnd(blocks[i], readBufs[i], true);
      }

      LOG.info("anchor Encode_stripe "+seq+" Data_read "+System.nanoTime());

      // Encode the data read.
      //for (int j = 0; j < bufSize; j++) {
      performEncode(readBufs, writeBufs);
      //}

      // Now that we have some data to write, send it to the temp files.
      for (int i = 0; i < paritySize; i++) {
        if(i == 0){
          LOG.info("anchor Output_parity "+i+seq+" Start_output "+System.nanoTime());
        }
        outs[i].write(writeBufs[i], 0, bufSize);
        if(i == 0)
          LOG.info("anchor Output_parity "+i+seq+" End_output "+System.nanoTime());
      }

      LOG.info("anchor Encode_stripe "+seq+" Parity_written "+System.nanoTime());

      if (reporter != null) {
        reporter.progress();
      }

      seq++;
    }
  }

  void performEncode(byte[][] readBufs, byte[][] writeBufs) {
    /*
    for (int i = 0; i < 2; i++) {
      code[i] = 0;
    }
    for (int i = 0; i < stripeSize; i++) {
      data[i] = readBufs[i][idx] & 0x000000FF;
    }
    */
    //iaCode.encode(data, code);
    /*
    for (int i = 0; i < 2; i++) {
      writeBufs[i][idx] = (byte)code[i];
    }
    */
    dataBufs.rewind();
    parityBufs.rewind();
		for(int i = 0; i < stripeSize; i++){
      dataBufs.put(readBufs[i], 0, bufSize);
		}

    LOG.info("before doing iao encode with bufSize" + bufSize + " and array size " + readBufs[0].length);
    LOG.info("anchor Encode_stripe "+seq+" Start_encoding "+System.nanoTime());
    iaoEncode(dataBufs, parityBufs, stripeSize, paritySize, bufSize, reserve);
    LOG.info("anchor Encode_stripe "+seq+" Encoded "+System.nanoTime());
    LOG.info("after doing iao encode");
    for(int i = 0; i< paritySize; i++){
      parityBufs.get(writeBufs[i], 0, bufSize);
    }
  }

  @Override
  public Path getParityTempPath() {
    return new Path(RaidNode.iaTempPrefix(conf));
  }
}
