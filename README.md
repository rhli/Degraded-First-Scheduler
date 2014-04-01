welcome
=====

This is the code package for degraded-first task scheduler. 
This readme file will lead you through an example of how degraded-first
works on a single-node cluster.
This package is ONLY tested with Ubuntu 12.04. Have fun.

Install
=====

This package relies on hadoop-0.22.0 and HDFS-RAID, in order to install, we embed 
source code into hadoop-0.22.0 and recompile it. 

1.  install g++, ant, java, set up $HADOOP_HOME(where you will place
    your hadoop), $JAVA_HOME(where is the sdk).

2.  download
    [hadoop-0.22.0](http://archive.apache.org/dist/hadoop/core/hadoop-0.22.0/hadoop-0.22.0.tar.gz).
    Untar hadoop-0.22.0.tar.gz, mv to $HADOOP_HOME

3.  Run install.sh. This script integrates degraded first scheduler into hadoop-0.22.0 and help you
finish most configurations.

    bash install.sh

4.  In $HADOOP_HOME/conf/masters, enter the hostname or host ip of your namenode.
    (don't forget to remove the default one if you don't need it)

5.  In $HADOOP_HOME/conf/slaves, enter the hostname or host ip of your datanode.
    (don't forget to remove the default one if you don't need it)

6.  In $HADOOP_HOME/conf/core-site.xml, config the hadoop tmp directory
    (where you want to put the hadoop file in your machine, better to
    use absolute path) and
    fs.default.name.
  
    <property>
      <name>hadoop.tmp.dir</name>
      <value>*put your path here(absolute path)*</value>
    </property>

    <property>
      <name>fs.default.name</name>
      <value>hdfs://*your namenode hostname or ip*:54310</value>
    </property>  


7.  In $HADOOP_HOME/conf/hdfs-site.xml, config the stripeLength(k) and
    parityLength(m). (Where the property name of parity length dependent
    on your chosen code)
    
    <property>
      <name>hdfs.raid.stripeLength</name>
      <value>*your k*</value>
    </property>

    <property>
      <name>hdfs.raidjrs.paritylength</name>
      <value>*your m for jrs code*</value>
    </property>

    <property>
      <name>hdfs.raidia.paritylength</name>
      <value>*your m for ia code*</value>
    </property>
    
8.  In $HADOOP_HOME/conf/hdfs-site.xml, config the *block size* and *packet
    size*(strip size). *packet size* should be a multiple of *parity length*(m) and *block
    size* should be a multiple of *packet size*.
  
    <property>
      <name>dfs.block.size</name>
      <value>*your block size*</value>
    </property>
  
    <property>
      <name>hdfs.raid.packet.size</name>
      <value>*your packet size*</value>
    </property>

9.  In $HADOOP_HOME/conf/hdfs-site.xml, config the path of
    raid.xml (Normally it should be in $HADOOP_HOME/conf/raid.xml,
    better to use absolute path).

    <property>
      <name>raid.config.file</name>
      <value>*your raid.xml path(Absolute Path)*</value>
      <description>This is needed by the RaidNode </description>
    </property>

10.  In $HADOOP_HOME/conf/mapred-site.xml, configure the task
scheduler.  Comment this to disable degraded-first.

    <property> 
        <name>mapreduce.jobtracker.taskscheduler</name> 
        <value>org.apache.hadoop.mapred.DegradedFirstTaskScheduler</value> 
    </property>

11.  In $HADOOP_HOME/conf/raid.xml, config the source file path.

    <srcPath prefix="hdfs://*namenode hostname or ip*:*port*/*file path*">

12.  In $HADOOP_HOME/conf/hadoop_env.sh, set the JAVA_HOME

    export JAVA_HOME=*your java home(Absolute Path)*

    export HADOOP_OPTS=-Djava.net.preferIPv4Stack=true

After finish the previous node installation and configuration, copy the
$HADOOP_HOME folder and spread the folder into the *namenode* and *ALL*
*datanodes*. Make sure each nodes can be public key accessible by each
other. Also make sure that c++, java, ant and $HADOOP_HOME are well set
for ALL nodes. If you have serveral different type of nodes (for
example, different OS), you may need to repeat the previous steps in
different types of nodes.

Add $HADOOP_HOME/bin to PATH

    export PATH=$PATH:$HADOOP_HOME/bin

Test Program
=====
This part I will walk your through a test program.  We work on a 
single node cluster.  Go to testProgram/ in this package.

1. Untar tmp.tar.gz, you got a file with some randomly generated
numbers, we use this as a wordcount input.  This file is a 5MB file,
since our block size is 1MB, it will be stored in 5 block.  

Add the following lines to ${HADOOP_HOME}/log4j.properties

log4j.logger.org.apache.hadoop.mapred.JobTracker=DEBUG
log4j.logger.org.apache.hadoop.mapred.JobInProgress=DEBUG


2. Start your hadoop from all over again.

hadoop namenode -format

start-dfs.sh

hadoop dfs -copyFromLocal tmp tmp

3. Go to ${Your hadoop dir}/dfs/data/current/finalized.  Here you
will see five block/metadata file pairs.  Remember one of them, we will
delete the pair after they are encoded into erasure coded format.

4. Start raidnode

start-raidnode.sh

Wait until there is a log message "Map task executor complete" in the
raidnode log. Now stop raidnode, we should not run it again, because
raidnode will recover lost blocks and interrupt our test program.

5. Go to ${Your hadoop dir}/dfs/data/current/finalized again and delete
the block/metadata file pair you remembered. 

6. Let us help hadoop realize the file is corrupted by calling

hadoop dfs -copyToLocal tmp tmpOut

Hadoop will realize by itself, but this step can save us time.

7. Now start mapreduce and let us run a wordcount program. 

hadoop jar ${HADOOP_HOME}/hadoop-mapred-examples-0.22.0.jar wordcount tmp tmpOut.

Use grep to find the lines in JobTracker log which contains word "Choosing".

The log message tells you the orders blocks are choosen to process, recall that
you have deleted a file, meaning that there is a degraded task, and according to
the lines in the log message, this one is the first task to be launched. 

8. If you are still curious, just repeat this from step 1, but disable degraded
first scheduler, the degraded task will be scheduled as the last task.


Configuration
=====

Configurable options in hadoop-0.22.0 (include hdfs-raid in 0.22.0) are
inherited. Please refer to
[hdfs](http://hadoop.apache.org/docs/stable/cluster_setup.html) and
[hdfs-raid](http://wiki.apache.org/hadoop/HDFS-RAID).

For raid.xml

Choose code type(options: jrs, ia)

    <erasureCode>jrs</erasureCode>

Change Log
=====












