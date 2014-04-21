Introduction
=====

This is the code package of degraded-first task scheduler. 
This package is ONLY tested with Ubuntu 12.04. Have fun.

Install
=====

This package relies on hadoop-0.22.0 and HDFS-RAID. To install, we embed 
source code into hadoop-0.22.0 and recompile. 

1.  Install g++, ant, java, set up $HADOOP_HOME(where you will place
    your hadoop), $JAVA_HOME(where is the sdk).

2.  Download
    [hadoop-0.22.0](http://archive.apache.org/dist/hadoop/core/hadoop-0.22.0/hadoop-0.22.0.tar.gz).
    Untar hadoop-0.22.0.tar.gz, mv to $HADOOP_HOME.

3.  Run install.sh. This script integrates degraded first scheduler into hadoop-0.22.0 and helps 
you finish most configurations.
>    bash install.sh

4.  In $HADOOP_HOME/conf/masters, enter the hostname or host IP of your namenode
    (don't forget to remove the default one if you don't need it).

5.  In $HADOOP_HOME/conf/slaves, enter the hostname or host IP of your datanode
    (don't forget to remove the default one if you don't need it).

6.  In $HADOOP_HOME/conf/core-site.xml, config the hadoop tmp directory
    (where you want to put the hadoop file in your machine, use absolute path) 
    and fs.default.name.
>   <property>  
>   <name>hadoop.tmp.dir</name>  
>   <value>*put your path here(absolute path)*</value>  
>   </property>  
>   <property>  
>   <name>fs.default.name</name>  
>   <value>hdfs://*your namenode hostname or IP*:54310</value>  
>   </property>  
    
7.  In $HADOOP_HOME/conf/hdfs-site.xml, configure the *block size* and *packet
    size(strip size)*, *block size* should be a multiple of *packet size*.
>    <property>  
>    <name>dfs.block.size</name>  
>    <value>*your block size*</value>  
>    </property>  
>    <property>  
>    <name>hdfs.raid.packet.size</name>  
>    <value>*your packet size*</value>  
>    </property>  

8.  In $HADOOP_HOME/conf/hdfs-site.xml, configure the path of raid.xml.
>   <property>  
>   <name>raid.config.file</name>  
>   <value>*your raid.xml path(Absolute Path)*</value>  
>   <description>This is needed by the RaidNode </description>  
>   </property>

9.  In $HADOOP_HOME/conf/mapred-site.xml, configure task scheduler.  
Comment this if you want to disable degraded-first task scheduler.
>   <property>   
>   <name>mapreduce.jobtracker.taskscheduler</name>   
>   <value>org.apache.hadoop.mapred.DegradedFirstTaskScheduler</value>   
>   </property>  

10.  In $HADOOP_HOME/conf/raid.xml, configure the source file path.
>   <srcPath prefix="hdfs://*namenode hostname or IP*:*port*/*file path*">

11.  In $HADOOP_HOME/conf/hadoop_env.sh, set the JAVA_HOME
>   export JAVA_HOME=*your java home(Absolute Path)*  
>   export HADOOP_OPTS=-Djava.net.preferIPv4Stack=true

After finishing the above installation and configuration, copy $HADOOP_HOME 
directory to *namenode* and *EVERY* *datanode*. Make sure each node is 
accessible by each other. Also make sure that c++, java, ant and $HADOOP_HOME 
are well set on *ALL* nodes. If you have several different types of nodes (for
example, different operating systems), you need to repeat the previous steps 
in every type of nodes.

Add $HADOOP_HOME/bin to PATH

>   export PATH=$PATH:$HADOOP_HOME/bin

Test Program
=====
This part walks you through an example test program.  The example works 
in a cluster of local mode, you can easily extend the example to a fully distributed
clustered.  First go to testProgram/ in this package.

1. Untar tmp.tar.gz, you got a file with some randomly generated
numbers, we use this as a wordcount input.  This file is a 5MB file,
since we set block size to 1MB, it will be stored in 5 blocks.  
Add the following lines to ${HADOOP_HOME}/conf/log4j.properties
>   log4j.logger.org.apache.hadoop.mapred.JobTracker=DEBUG  
>   log4j.logger.org.apache.hadoop.mapred.JobInProgress=DEBUG


2. Start your hadoop from all over again and copy the tmp file into HDFS.
>   hadoop namenode -format  
>   start-dfs.sh  
>   hadoop dfs -copyFromLocal tmp tmp  

3. Go to ${Your hadoop dir}/dfs/data/current/finalized.  Here you
will see five block/metadata file pairs.  Remember one of them, we will
delete the pair after they are encoded into erasure coded format.

4. Start raidnode
>   start-raidnode.sh  

5. Wait until there is a log message "Map task executor complete" in the
raidnode log. Now stop raidnode, we should not run it again, because
raidnode will recover lost blocks and interrupt our test program.  
Go to ${Your hadoop dir}/dfs/data/current/finalized again and delete
the block/metadata file pair you remembered. 

6. Let us help hadoop realize the file is corrupted by calling
>   hadoop dfs -copyToLocal tmp tmpOut  

7. Now start mapreduce and let us run a wordcount program. 
>   hadoop jar ${HADOOP_HOME}/hadoop-mapred-examples-0.22.0.jar wordcount tmp tmpOut.  

8. Use grep to find the lines in JobTracker log which contains word "Choosing".
The log message tells you the orders blocks are choosen to process, recall that
you have deleted a block, meaning that there is a degraded task, and according to
the lines in the log message, the degraded task is the first one to be launched. 

9. If you are still curious, just repeat this from step 1, but disable degraded
first scheduler, the degraded task will be scheduled as the last task.


Configuration
=====

Configurable options in hadoop-0.22.0 (include hdfs-raid in 0.22.0) are
inherited. Please refer to
[hdfs](http://hadoop.apache.org/docs/stable/cluster_setup.html) and
[hdfs-raid](http://wiki.apache.org/hadoop/HDFS-RAID).

Developer's Guide
=====

The source code of degraded-first task scheduler is in 
java/org/apache/hadoop/mapred/DegradedFirstTaskScheduler.java
java/org/apache/hadoop/mapred/JobInProgress.java

The other parts of the package contains bug fix in compilation and 
performance optimization of HDFS-RAID. 

Change Log
=====












