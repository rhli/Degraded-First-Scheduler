#!/bin/bash

echo "check hadoop home"

${HADOOP_HOME:?"Need to set hadoop home"}

cp src/java/org/apache/hadoop/mapred/* ${HADOOP_HOME}/mapreduce/src/java/org/apache/hadoop/mapred/

echo "Going into mapreduce/ to compile"

cd ${HADOOP_HOME}/mapreduce
ant
cd -

echo "Successfully installed, you are good to go"

