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

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Random;
import java.util.Map;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.hdfs.BlockMissingException;
import org.apache.hadoop.fs.ChecksumException;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.hdfs.protocol.LocatedBlock;

/**
 * Represents a generic decoder that can be used to read a file with
 * corrupt blocks by using the parity file.
 * This is an abstract class, concrete subclasses need to implement
 * fixErasedBlock.
 */
public abstract class Decoder {
  public static final Log LOG = LogFactory.getLog(
                                  "org.apache.hadoop.raid.Decoder");
  protected Configuration conf;
  protected int stripeSize;
  protected int paritySize;
  protected Random rand;
  protected int bufSize;
  protected byte[][] readBufs; // legacy
  protected byte[][] writeBufs; // legacy

  Decoder(Configuration conf, int stripeSize, int paritySize) {
    this.conf = conf;
    this.stripeSize = stripeSize;
    this.paritySize = paritySize;
    this.rand = new Random(12345);
    this.bufSize = conf.getInt("hdfs.raid.packet.size", 1024 * 1024);
    this.readBufs = new byte[stripeSize + paritySize][];
    this.writeBufs = new byte[paritySize][];
    allocateBuffers();
  }

  private void allocateBuffers() {
    for (int i = 0; i < stripeSize + paritySize; i++) {
      readBufs[i] = new byte[bufSize];
    }
    for (int i = 0; i < paritySize; i++) {
      writeBufs[i] = new byte[bufSize];
    }
  }

  private void configureBuffers(long blockSize) {
    if ((long)bufSize > blockSize) {
      bufSize = (int)blockSize;
      allocateBuffers();
    } else if (blockSize % bufSize != 0) {
      bufSize = (int)(blockSize / 256L); // heuristic.
      if (bufSize == 0) {
        bufSize = 1024;
      }
      bufSize = Math.min(bufSize, 1024 * 1024);
      allocateBuffers();
    }
  }

  /**
   * Recovers a corrupt block to local file.
   *
   * @param srcFs The filesystem containing the source file.
   * @param srcPath The damaged source file.
   * @param parityPath The filesystem containing the parity file. This could be
   *        different from fs in case the parity file is part of a HAR archive.
   * @param parityFile The parity file.
   * @param blockSize The block size of the file.
   * @param corruptStripe Map to store the block info in the stripe
   * @param localBlockFiles The files to write the blocks to.
   * @param stripeIdx the index of the stripe
   */
  public void recoverStripeToFile(
      FileSystem srcFs, Path srcPath, FileSystem parityFs, Path parityPath,
      long blockSize, Map<Integer, LocatedBlock> corruptStripe, 
      File[] localBlockFiles, int stripeIdx)
  throws IOException{
      fixErasedBlock(srcFs, srcPath, parityFs, parityPath,
        blockSize, corruptStripe, localBlockFiles, stripeIdx);
      }

  /**
   * Implementation-specific mechanism of writing a fixed block.
   * @param fs The filesystem containing the source file.
   * @param srcFile The damaged source file.
   * @param parityFs The filesystem containing the parity file. This could be
   *        different from fs in case the parity file is part of a HAR archive.
   * @param parityFile The parity file.
   * @param blockSize The maximum size of a block.
   * @param errorOffset Known location of error in the source file. There could
   *        be additional errors in the source file that are discovered during
   *        the decode process.
   * @param limit The maximum number of bytes to be written out, including
   *       bytesToSkip. This is to prevent writing beyond the end of the file.
   * @param out The output.
   */
  protected abstract void fixErasedBlock(
      FileSystem fs, Path srcFile, FileSystem parityFs, Path parityFile,
      long blockSize, long errorOffset, long limit,
      OutputStream out) throws IOException;

  /**
   * Implementation-specific mechanism of writing a fixed block.
   * @param fs The filesystem containing the source file.
   * @param srcFile The damaged source file.
   * @param parityFs The filesystem containing the parity file. This could be
   *        different from fs in case the parity file is part of a HAR archive.
   * @param parityFile The parity file.
   * @param blockSize The maximum size of a block.
   * @param corruptStripe Map to store the block info in the stripe
   * @param lbfs The files to write the blocks to.
   * @param stripeIdx the index of the stripe
   */
  protected abstract void fixErasedBlock(
      FileSystem fs, Path srcFile, FileSystem parityFs, Path parityFile,
      long blockSize, Map<Integer, LocatedBlock> corruptStripe,
      File[] lbfs, int stripeIdx) throws IOException;

}
