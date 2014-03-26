Degraded-First scheduling is a MapReduce Scheduler improving 
performance of degraded tasks in erasure-coded storage.  
Default locality-first scheduler schedules degraded tasks to 
the end of map phase, which results in network congestion 
towards the end of map phase.  We distribute degraded tasks 
evenly in the map phase by making sure the ratio of launched 
degraded tasks equals to that of normal tasks. 

We also provide locality preservation and rack awareness 
heuristic to further improve the performances.

To use:

1. Download Hadoop (we recommend Hadoop-0.22.0), set $HADOOP_HOME
2. Run ./install.sh
3. Go to ${HADOOP_HOME}/conf/ and add the following in mapred-site.xml

<property> 
    <name>mapreduce.jobtracker.taskscheduler</name> 
    <value>org.apache.hadoop.mapred.DegradedFirstTaskScheduler</value> 
</property>

Now you are good to go, have fun!

