/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/RS.hh"
#include "lib/ProductMatrix.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_JRSEncoder_JRSMigrationEncoder.h"

/**
 * jrs encoder for pipeline architecture
 *
 * @params env, jni env variable
 * @params jobj, jni obj variable
 * @params in, input data buffer, can transform to character array by `env->GetDirectBufferAddress(in)`
 *   First `size*k` bytes are real data, followed by 4 bytes to indicate the seq number and 1 byte to
 *   inidicate whether this is the end for a whole chunk
 * @params out, output data buffer, can transform to character array by `env->GetDirectBufferAddress(out)`
 *   Need to reset before importing data. First `n*size` bytes are real output data, followed by 4 bytes 
 *   to indicate the seq number
 * @param k
 * @param m
 * @param blockSize, packet size(normally 1MB)
 *
 * @returns void
 *
 * @appendix, need to add cleanup code.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_JRSEncoder_00024JRSMigrationEncoder_jrsEncode
(JNIEnv * env, jobject obj, jobject in, jobject out, jint k, jint m, jint blockSize){

	//To get a byte array
	//Initialize a ProductMatrix Object
	RS rs=RS(k+m,k,8);
	rs.generate_encoding_matrix();
	int whole_length=blockSize*k;
  jclass cls = env->GetObjectClass(obj);
  jmethodID mid1 = env->GetMethodID(cls, "take", "()V");
  if (NULL == mid1)
  {
    return;
  }

  char * inData = (char*)(env)->GetDirectBufferAddress(in);

  //To push a byte array
  jmethodID mid2 = env->GetMethodID(cls, "push", "()V");
  if (0 == mid2)
  {
    return;
  }


  char * outData = (char*)(env)->GetDirectBufferAddress(out);
  
  char flag;
  while(1){
    env->CallVoidMethod(obj, mid1);
    memset(outData, 0, 64+m*blockSize);
    rs.encode2((char*)inData,(char*)outData,whole_length);
    memcpy(outData+m*blockSize,inData+whole_length,4);
    env->CallVoidMethod(obj, mid2);
    flag=inData[4+whole_length];
    if(flag==1){
      return;
    }
  }
}
