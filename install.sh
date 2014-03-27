#!/bin/bash

${HADOOP_HOME:?"Need to set hadoop home"}

cp src/java/org/apache/hadoop/mapred/* ${HADOOP_HOME}/src/java/org/apache/hadoop/mapred/

echo "Going into mapreduce/ to compile"

cd mapreduce
ant
cd -

echo "Successfully installed, you are good to go"

