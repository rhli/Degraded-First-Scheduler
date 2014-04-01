#!/bin/bash

if [ -n "`uname -m | grep '64'`" ]
then
  linux_dir=Linux-amd64-64
  #echo $linux_dir
else
  linux_dir=Linux-i386-32
  #echo $linux_dir
fi

mkdir $HADOOP_HOME/lib/native/$linux_dir
cp bin/*.so $HADOOP_HOME/lib/native/$linux_dir
