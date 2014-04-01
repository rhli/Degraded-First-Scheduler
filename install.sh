#!/bin/bash

echo "** Check Environments"
    echo ' Check $HADOOP_HOME'
    count=`$HADOOP_HOME/bin/hadoop version | grep "Hadoop 0.22.0" | wc -l`
    if [ $count -eq "0" ]; then
      echo 'ERROR: $HADOOP_HOME is invalid or hadoop version mismatch.'
      exit
    fi
    echo ' Check $JAVA_HOME'
    count=`$JAVA_HOME/bin/java -version 2>&1 | grep "java version" | wc -l`
    if [ $count -eq "0" ]; then
      echo 'ERROR: $JAVA_HOME is invalid.'
      exit
    fi

echo "** Update source code"
    BASEDIR=$(dirname $0)
    echo " Update hadoop-common"
    echo " Coping conf"
    cp $BASEDIR/java/org/apache/hadoop/conf/*.java \
      $HADOOP_HOME/common/src/java/org/apache/hadoop/conf/
    echo " Coping fs"
    cp $BASEDIR/java/org/apache/hadoop/fs/*.java \
      $HADOOP_HOME/common/src/java/org/apache/hadoop/fs/
    echo " Coping net"
    cp $BASEDIR/java/org/apache/hadoop/net/*.java \
      $HADOOP_HOME/common/src/java/org/apache/hadoop/net/
    echo " Update hadoop-hdfs"
    echo " Coping hdfs"
    cp $BASEDIR/java/org/apache/hadoop/hdfs/protocol/*.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/protocol/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/datanode/BlockSender.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/server/datanode/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/datanode/DataNode.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/server/datanode/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/datanode/DataXceiver.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/server/datanode/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/datanode/DataXceiverServer.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/server/datanode/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/datanode/IABlockSender.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/server/datanode/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/datanode/PMBlockSender.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/server/datanode/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/BlockReader.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/DFSClient.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/DFSInputStream.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/DistributedFileSystem.java \
      $HADOOP_HOME/hdfs/src/java/org/apache/hadoop/hdfs/
    echo " Update hadoop-mapreduce"
    echo " Coping mapreduce"
    cp $BASEDIR/java/org/apache/hadoop/mapred/*.java \
      $HADOOP_HOME/mapreduce/src/java/org/apache/hadoop/mapred/
    echo " Update hdfs-raid"
    echo " Coping hdfs"
    RAIDDIR=$HADOOP_HOME/mapreduce/src/contrib/raid/src/java/org/apache/hadoop
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/datanode/RaidBlockSender.java \
      $RAIDDIR/hdfs/server/datanode/
    mkdir $RAIDDIR/hdfs/server/namenode
    cp $BASEDIR/java/org/apache/hadoop/hdfs/server/namenode/* \
      $RAIDDIR/hdfs/server/namenode/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/DistributedRaidFileSystem.java \
      $RAIDDIR/hdfs/
    cp $BASEDIR/java/org/apache/hadoop/hdfs/RaidDFSUtil.java \
      $RAIDDIR/hdfs/
    echo " Copying raid"
    cp -r $BASEDIR/java/org/apache/hadoop/raid/* \
      $RAIDDIR/raid
    echo " Deleting redundancy in raid"
    rm $RAIDDIR/raid/XOREncoder.java
    rm $RAIDDIR/raid/XORDecoder.java
    rm $RAIDDIR/raid/ReedSolomonEncoder.java
    rm $RAIDDIR/raid/ReedSolomonDecoder.java
    echo " Update ivy"
    echo " Coping ivy"
    cp $BASEDIR/ivy/hdfs/* \
      $HADOOP_HOME/hdfs/ivy/
    cp $BASEDIR/ivy/mapreduce/* \
      $HADOOP_HOME/mapreduce/ivy/
    echo " Update build properties"
    echo " Coping build properties"
    cp $BASEDIR/build.properties \
      $HADOOP_HOME/common
    cp $BASEDIR/build.properties \
      $HADOOP_HOME/hdfs
    cp $BASEDIR/build.properties \
      $HADOOP_HOME/mapreduce

echo "** Compile hadoop-common"
    (cd $HADOOP_HOME/common && exec ant mvn-install)

echo "** Compile hadoop-hdfs"
    cp $HADOOP_HOME/common/build/*.jar $HADOOP_HOME/hdfs/lib/
    (cd $HADOOP_HOME/hdfs && exec ant mvn-install)

echo "** Compile hadoop-mapreduce"
    cp $HADOOP_HOME/common/build/*.jar $HADOOP_HOME/mapreduce/lib/
    cp $HADOOP_HOME/hdfs/build/*.jar $HADOOP_HOME/mapreduce/lib/
    (cd $HADOOP_HOME/mapreduce && exec ant mvn-install)

echo "** Compile hdfs-raid (CORE)"
    cp $HADOOP_HOME/mapreduce/build/*.jar $HADOOP_HOME/mapreduce/lib/
    (cd $HADOOP_HOME/mapreduce/src/contrib/raid && exec ant)

echo "** Compile native code"
    (cd $BASEDIR/native/ && make clean && make)
   
echo "** Import object code"
    echo " Coping jar files"
    cp $HADOOP_HOME/common/build/*.jar $HADOOP_HOME
    cp $HADOOP_HOME/hdfs/build/*.jar $HADOOP_HOME
    cp $HADOOP_HOME/mapreduce/build/*.jar $HADOOP_HOME
    cp $HADOOP_HOME/mapreduce/build/contrib/raid/*.jar $HADOOP_HOME/lib
    echo " Coping native object files"
    (cd $BASEDIR/native/ && bash install.sh)

echo "** Conf Hadoop"
    echo " Coping shell script"
    cp $BASEDIR/bin/*.sh $HADOOP_HOME/bin
    echo " Coping Config files"
    cp $BASEDIR/conf/*.xml $HADOOP_HOME/conf
    cp $BASEDIR/conf/*.sh $HADOOP_HOME/conf

echo "==========="
echo "Congratuations, Install SUCCESSFUL."

