/*

   rewrite by LIN Jian
   $Date: 2012/07/10 $

*/
#include <stdlib.h>
#include <cstring>

#include "lib/ProductMatrix.hh"
#include "lib/RS.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_JRSDecoder_JRSRecoveryDecoder.h"
#include "java_header/org_apache_hadoop_raid_JRSDecoder_JRSDegradedReadDecoder.h"

/**
 * jrs decoder for degraded read
 *
 * @params env, jni env variable
 * @params jobj, jni obj variable
 * @params in, input data buffer, can transform to character array by `env->GetDirectBufferAddress(in)`
 *   First `size*k` bytes are real data, followed by 4 bytes to indicate the seq number
 * @params out, output data buffer, can transform to character array by `env->GetDirectBufferAddress(out)`
 *   Need to reset before importing data. First `n*size` bytes are real output data, followed by 4 bytes 
 *   to indicate the seq number
 * @param k
 * @param m
 * @param n, number of fail strip
 * @param locations, array to indicate fail locations
 * @param size, packet size
 * @param temp, memory segment for intermedia data(jrs object pointer),
 *   can transform to character array by `env->GetDirectBufferAddress(temp)`
 * @param reconstruct, indicate whether need to reconstruct the matrix or not(restore from temp).
 *
 * @returns void
 *
 * @appendix, need to cleanup legacy jrs object if the generate matrix need to reconstruct.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_JRSDecoder_00024JRSDegradedReadDecoder_jrsDr
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint n, 
 jintArray locations, jint target, jint size, jobject temp, jboolean reconstruct){
  int* l = (int *)malloc(sizeof(int)*n);
  jint *elements = (env)->GetIntArrayElements(locations, 0);
  for(int i = 0; i < n; i++){
    l[i] = elements[i];
  }
  char * inData = (char *)env->GetDirectBufferAddress(in);
  char * outData = (char *)env->GetDirectBufferAddress(out);
  char * r = (char *)env->GetDirectBufferAddress(temp);
  RS * jrs = NULL;
  if(reconstruct){
    //memcpy((char*)&jrs, r, sizeof(char*));
    //if(!jrs) jrs->cleanup();
    jrs=new RS(k+m,k,8);
    jrs->generate_encoding_matrix();
    memcpy(r, (char*)&jrs, sizeof(char*));
  }else{
    memcpy((char*)&jrs, r, sizeof(char*));
  }

  int dr[1];
  dr[0] = target;
  jrs->set_f2(n, l, 1, dr);
  memset(outData, 0, 64+size);
  jrs->reconstruct_lost_data2(inData,outData,size);
  memcpy(outData+size,inData+size*k,4);
}

/**
 * jrs decoder for recovery
 *
 * @params env, jni env variable
 * @params jobj, jni obj variable
 * @params in, input data buffer, can transform to character array by `env->GetDirectBufferAddress(in)`
 *   First `size*k` bytes are real data, followed by 4 bytes to indicate the seq number
 * @params out, output data buffer, can transform to character array by `env->GetDirectBufferAddress(out)`
 *   Need to reset before importing data. First `n*size` bytes are real output data, followed by 4 bytes 
 *   to indicate the seq number
 * @param k
 * @param m
 * @param n, number of fail strip
 * @param locations, array to indicate fail locations
 * @param size, packet size
 * @param temp, memory segment for intermedia data(jrs object point),
 *   can transform to character array by `env->GetDirectBufferAddress(temp)`
 * @param reconstruct, indicate whether need to reconstruct the matrix or not(restore from temp).
 *
 * @returns void
 *
 * @appendix, need to cleanup legacy jrs object if the generate matrix need to reconstruct.
 *   It seems that I need to change some java code to make clean work.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_JRSDecoder_00024JRSRecoveryDecoder_jrsDecode
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint n, 
 jintArray locations, jint size, jobject temp, jboolean reconstruct){
  int* l = (int *)malloc(sizeof(int)*n);
  jint *elements = (env)->GetIntArrayElements(locations, 0);
  for(int i = 0; i < n; i++){
    l[i] = elements[i];
  }
  char * inData = (char *)env->GetDirectBufferAddress(in);
  char * outData = (char *)env->GetDirectBufferAddress(out);
  char * r = (char *)env->GetDirectBufferAddress(temp);
  RS * jrs = NULL;
  if(reconstruct){
    //memcpy((char*)&jrs, r, sizeof(char*));
    //if(!jrs) jrs->cleanup();
    jrs=new RS(k+m,k,8);
    jrs->generate_encoding_matrix();
    memcpy(r, (char*)&jrs, sizeof(char*));
  }else{
    memcpy((char*)&jrs, r, sizeof(char*));
  }

  jrs->set_f2(n, l, n, l);
  memset(outData, 0, 64+n*size);
  jrs->reconstruct_lost_data2(inData,outData,size);
  memcpy(outData+n*size,inData+size*k,4);
}
