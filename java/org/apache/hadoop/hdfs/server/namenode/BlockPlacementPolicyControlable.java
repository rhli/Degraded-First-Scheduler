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
package org.apache.hadoop.hdfs.server.namenode;

import java.io.IOException;
import org.apache.commons.logging.*;
import org.apache.hadoop.classification.InterfaceAudience;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.hdfs.protocol.Block;
import org.apache.hadoop.hdfs.protocol.DatanodeInfo;
import org.apache.hadoop.hdfs.protocol.FSConstants;
import org.apache.hadoop.hdfs.protocol.LocatedBlock;
import org.apache.hadoop.net.NetworkTopology;
import org.apache.hadoop.net.Node;
import org.apache.hadoop.net.NodeBase;
import org.apache.hadoop.hdfs.DFSConfigKeys;
import java.util.*;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.raid.RaidNode;

/** The class is responsible for choosing the desired number of targets
 * for placing block replicas.
 * The replica placement strategy is that if the writer is on a datanode,
 * the 1st replica is placed on the local machine, 
 * otherwise a random datanode. The 2nd replica is placed on a datanode
 * that is on a different rack. The 3rd replica is placed on a datanode
 * which is on a different node of the rack as the second replica.
 */
@InterfaceAudience.Private
public class BlockPlacementPolicyControlable extends BlockPlacementPolicy {
  private boolean considerLoad; 
  private NetworkTopology clusterMap;
  private FSClusterStats stats;

  private int stripeLength;
  private int pmParityLength;
  private int iaParityLength;
  private int jrsParityLength;
  private String pmPrefix = null;
  private String iaPrefix = null;
  private String jrsPrefix = null;
  private String raidpmTempPrefix = null;
  private String raidiaTempPrefix = null;
  private String raidjrsTempPrefix = null;
  private FSNamesystem namesystem = null;

  BlockPlacementPolicyControlable(Configuration conf,  FSClusterStats stats,
                           NetworkTopology clusterMap) {
    initialize(conf, stats, clusterMap);
  }

  BlockPlacementPolicyControlable() {
  }

  Random rd = new Random(11234);
  int index;
  int head;
  int count = 0;
  boolean flag = false;
  ArrayList<Integer> nodeInfos;
  

  /** {@inheritDoc} */
  public void initialize(Configuration conf,  FSClusterStats stats,
                         NetworkTopology clusterMap) {
    this.considerLoad = conf.getBoolean(DFSConfigKeys.DFS_NAMENODE_REPLICATION_CONSIDERLOAD_KEY, true);
    this.stats = stats;
    this.clusterMap = clusterMap;

    this.stripeLength = RaidNode.getStripeLength(conf);
    this.jrsParityLength = RaidNode.jrsParityLength(conf);
    this.pmParityLength = RaidNode.pmParityLength(conf);
    this.iaParityLength = RaidNode.iaParityLength(conf);

    FSNamesystem.LOG.info("stripeLength "+stripeLength+" pmParityLength "+pmParityLength);
    if (this.pmPrefix == null) {
      this.pmPrefix = RaidNode.DEFAULT_RAIDPM_LOCATION;
    }
    if (this.iaPrefix == null) {
      this.iaPrefix = RaidNode.DEFAULT_RAIDIA_LOCATION;
    }
    if (this.jrsPrefix == null) {
      this.jrsPrefix = RaidNode.DEFAULT_RAIDJRS_LOCATION;
    }
    // Throws ClassCastException if we cannot cast here.
    this.namesystem = (FSNamesystem) stats;
    this.raidpmTempPrefix = RaidNode.pmTempPrefix(conf);
    this.raidiaTempPrefix = RaidNode.iaTempPrefix(conf);
    this.raidjrsTempPrefix = RaidNode.jrsTempPrefix(conf);

    index = 0;

    nodeInfos = new ArrayList<Integer>();
  }

  /** {@inheritDoc} */
  public DatanodeDescriptor[] chooseTarget(String srcPath,
                                    int numOfReplicas,
                                    DatanodeDescriptor writer,
                                    List<DatanodeDescriptor> chosenNodes,
                                    long blocksize) {
    return chooseTarget(srcPath, numOfReplicas, writer, chosenNodes, null, blocksize);
  }

  /** {@inheritDoc} */
  public DatanodeDescriptor[] chooseTarget(String srcPath,
                                    int numOfReplicas,
                                    DatanodeDescriptor writer,
                                    List<DatanodeDescriptor> chosenNodes,
                                    HashMap<Node, Node> excludedNodes,
                                    long blocksize) {
    try{
      FileType type = getFileType(srcPath);

      if(type == FileType.NOT_RAID)
        return chooseTarget(numOfReplicas, writer, chosenNodes, excludedNodes, blocksize, stripeLength, true);
      else{
        if(!flag){
          flag = true;
          index = 0;
        }
        return chooseTarget(numOfReplicas, writer, chosenNodes, excludedNodes, blocksize, iaParityLength, false);
      }

    }catch(IOException e){
      return chooseTarget(numOfReplicas, writer, chosenNodes, excludedNodes, blocksize, stripeLength, true);
    }
  }


  DatanodeDescriptor[] chooseTarget(
      int numOfReplicas,
      DatanodeDescriptor writer,
      List<DatanodeDescriptor> chosenNodes,
      HashMap<Node, Node> excludedNodes,
      long blocksize,
      int l,
      boolean isSource) {
    FSNamesystem.LOG.info("chooseTarget, round "+(count++)+" l: "+l+" index "+index);
    if(index % l == 0 && isSource){
      head = rd.nextInt(100);
      nodeInfos.add(head);
    }
    if(!isSource)
      head = nodeInfos.get(index/l)+stripeLength;
    DatanodeDescriptor[] ret = new DatanodeDescriptor[numOfReplicas];
    ret[0] = (DatanodeDescriptor)clusterMap.chooseByIndex(index%l + head);
    for(int i=1; i<numOfReplicas; i++){
      boolean f = true;
      while(f){
       ret[i] = (DatanodeDescriptor)clusterMap.chooseRandom(NodeBase.ROOT);
       f = false;
       for(int j = 0; j < i; j++)
         if(ret[i].getName() == ret[j].getName())
           f = true;
      }
    }
    index++;
    return ret;
      }


  /* Return a pipeline of nodes.
   * The pipeline is formed finding a shortest path that 
   * starts from the writer and traverses all <i>nodes</i>
   * This is basically a traveling salesman problem.
   */
  private DatanodeDescriptor[] getPipeline(
      DatanodeDescriptor writer,
      DatanodeDescriptor[] nodes) {
    if (nodes.length==0) return nodes;

    synchronized(clusterMap) {
      int index=0;
      if (writer == null || !clusterMap.contains(writer)) {
        writer = nodes[0];
      }
      for(;index<nodes.length; index++) {
        DatanodeDescriptor shortestNode = nodes[index];
        int shortestDistance = clusterMap.getDistance(writer, shortestNode);
        int shortestIndex = index;
        for(int i=index+1; i<nodes.length; i++) {
          DatanodeDescriptor currentNode = nodes[i];
          int currentDistance = clusterMap.getDistance(writer, currentNode);
          if (shortestDistance>currentDistance) {
            shortestDistance = currentDistance;
            shortestNode = currentNode;
            shortestIndex = i;
          }
        }
        //switch position index & shortestIndex
        if (index != shortestIndex) {
          nodes[shortestIndex] = nodes[index];
          nodes[index] = shortestNode;
        }
        writer = shortestNode;
      }
    }
    return nodes;
      }

  /** {@inheritDoc} */
  public int verifyBlockPlacement(String srcPath,
      LocatedBlock lBlk,
      int minRacks) {
    DatanodeInfo[] locs = lBlk.getLocations();
    if (locs == null)
      locs = new DatanodeInfo[0];
    int numRacks = clusterMap.getNumOfRacks();
    if(numRacks <= 1) // only one rack
      return 0;
    minRacks = Math.min(minRacks, numRacks);
    // 1. Check that all locations are different.
    // 2. Count locations on different racks.
    Set<String> racks = new TreeSet<String>();
    for (DatanodeInfo dn : locs)
      racks.add(dn.getNetworkLocation());
    return minRacks - racks.size();
  }

  /** {@inheritDoc} */
  public DatanodeDescriptor chooseReplicaToDelete(FSInodeInfo inode,
      Block block,
      short replicationFactor,
      Collection<DatanodeDescriptor> first, 
                                                 Collection<DatanodeDescriptor> second) {
    long minSpace = Long.MAX_VALUE;
    DatanodeDescriptor cur = null;

    // pick replica from the first Set. If first is empty, then pick replicas
    // from second set.
    Iterator<DatanodeDescriptor> iter =
          first.isEmpty() ? second.iterator() : first.iterator();

    // pick node with least free space
    while (iter.hasNext() ) {
      DatanodeDescriptor node = iter.next();
      long free = node.getRemaining();
      if (minSpace > free) {
        minSpace = free;
        cur = node;
      }
    }
    FSNamesystem.LOG.info("in chooseReplicaToDelete");
    FSNamesystem.LOG.info("Block "+block.getBlockName()+" in "+cur.getName()+" will be deleted");
    return cur;
  }

  enum FileType {
    NOT_RAID,
    PM_PARITY,
    JRS_PARITY, 
    IA_PARITY
  }

  FileType getFileType(String path) throws IOException {
    if (path.startsWith(raidpmTempPrefix + Path.SEPARATOR)) {
      return FileType.PM_PARITY;
    }
    if (path.startsWith(raidiaTempPrefix + Path.SEPARATOR)) {
      return FileType.IA_PARITY;
    }
    if (path.startsWith(raidjrsTempPrefix + Path.SEPARATOR)) {
      return FileType.JRS_PARITY;
    }
    if (path.startsWith(pmPrefix + Path.SEPARATOR)) {
      return FileType.PM_PARITY;
    }
    if (path.startsWith(iaPrefix + Path.SEPARATOR)) {
      return FileType.IA_PARITY;
    }
    if (path.startsWith(jrsPrefix + Path.SEPARATOR)) {
      return FileType.JRS_PARITY;
    }
    return FileType.NOT_RAID;
  }

}

