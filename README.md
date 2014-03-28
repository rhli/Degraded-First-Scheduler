Introduction
====
Degraded-First scheduling is a MapReduce Scheduler improves 
performance of degraded tasks in erasure-coded storage.  
Default locality-first scheduler schedules degraded tasks to 
the end of map phase, which results in network congestion 
towards the end of map phase.  We distribute degraded tasks 
evenly in the map phase by making sure the ratio of launched 
degraded tasks equals to that of normal tasks. 

We also provide locality preservation and rack awareness 
heuristic to further improve the performances.

Installation and Configuration:
====

0. Preparation: install java (openjdk-6-jdk) and ant. Set environment 
variable $JAVA_HOME.

1. Download [ Hadoop-0.22.0 ](http://archive.apache.org/dist/hadoop/core/hadoop-0.22.0/hadoop-0.22.0.tar.gz). 
Setup your hadoop cluster. Set environment variable $HADOOP_HOME.

Note: Find the file $HADOOP_HOME/mapreduce/ivy/libraries.properties, and do the following modification.

"
hadoop-common.version=0.22.0-SNAPSHOT
hadoop-hdfs.version=0.22.0-SNAPSHOT
"

=>

"
hadoop-common.version=0.22.0
hadoop-hdfs.version=0.22.0
"

2. Run ./install.sh.

3. Configure [ Hadoop ](http://hadoop.apache.org/docs/stable/cluster_setup.html) 
and [ HDFS_RAID ](http://wiki.apache.org/hadoop/HDFS-RAID). 

4. Choose Degraded-first task scheduler by going to ${HADOOP_HOME}/conf/ 
and add the following lines in mapred-site.xml.
```
<property> 
    <name>mapreduce.jobtracker.taskscheduler</name> 
    <value>org.apache.hadoop.mapred.DegradedFirstTaskScheduler</value> 
</property>
```

You are ready to go, have fun!

