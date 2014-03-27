Degraded-First scheduling is a MapReduce Scheduler improves 
performance of degraded tasks in erasure-coded storage.  
Default locality-first scheduler schedules degraded tasks to 
the end of map phase, which results in network congestion 
towards the end of map phase.  We distribute degraded tasks 
evenly in the map phase by making sure the ratio of launched 
degraded tasks equals to that of normal tasks. 

We also provide locality preservation and rack awareness 
heuristic to further improve the performances.

To use:

0. Preparation: install java, c++ and ant. Set environment 
variable $JAVA_HOME.

1. Download Hadoop-0.22.0 from http://archive.apache.org/dist/hadoop/core/hadoop-0.22.0/hadoop-0.22.0.tar.gz. 
Setup your hadoop cluster. Set environment variable $HADOOP_HOME.

2. Run ./install.sh

3. Configure HDFS_RAID according to 
http://wiki.apache.org/hadoop/HDFS-RAID. 

Go to ${HADOOP_HOME}/conf/ and add the following lines in 
mapred-site.xml

```
<property> 
    <name>mapreduce.jobtracker.taskscheduler</name> 
    <value>org.apache.hadoop.mapred.DegradedFirstTaskScheduler</value> 
</property>
```

You are ready to go, have fun!

