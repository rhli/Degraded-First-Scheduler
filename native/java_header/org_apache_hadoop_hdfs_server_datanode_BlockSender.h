/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_apache_hadoop_hdfs_server_datanode_BlockSender */

#ifndef _Included_org_apache_hadoop_hdfs_server_datanode_BlockSender
#define _Included_org_apache_hadoop_hdfs_server_datanode_BlockSender
#ifdef __cplusplus
extern "C" {
#endif
#undef org_apache_hadoop_hdfs_server_datanode_BlockSender_MIN_BUFFER_WITH_TRANSFERTO
#define org_apache_hadoop_hdfs_server_datanode_BlockSender_MIN_BUFFER_WITH_TRANSFERTO 65536L
/*
 * Class:     org_apache_hadoop_hdfs_server_datanode_BlockSender
 * Method:    pmREncode
 * Signature: (Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;III[I)V
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_hdfs_server_datanode_BlockSender_pmREncode
  (JNIEnv *, jobject, jobject, jobject, jint, jint, jint, jint, jintArray);

#ifdef __cplusplus
}
#endif
#endif