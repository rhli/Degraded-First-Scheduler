/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/ProductMatrix.hh"
#include "lib/RS.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_PMDecoder_PMRecoveryDecoder.h"
#include "java_header/org_apache_hadoop_raid_PMDecoder_PMDegradedReadDecoder.h"

/**
 * pm decoder for degraded read
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
 * @param n, number of fail strip, virtual
 * @param vlocations, array to indicate the virtual locations (different from actual location when
 *   we encounter a fail pattern)
 * @param locations, array to indicate actual fail locations
 * @param size, packet size
 * @param temp, memory segment for intermedia data(jrs object point),
 *   can transform to character array by `env->GetDirectBufferAddress(temp)`
 * @param reconstruct, indicate whether need to reconstruct the matrix or not(restore from temp).
 *
 * @returns void
 *
 * @appendix, need to cleanup legacy pm object if the generate matrix need to reconstruct.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_PMDecoder_00024PMRecoveryDecoder_pmDecode
  (JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint n, 
   jintArray vlocations, jintArray locations, jint size, jobject temp, jboolean reconstruct){
	int* vls = (int *)malloc(sizeof(int)*n);
	jint *elements = (env)->GetIntArrayElements(vlocations, 0);
	for(int i = 0; i < n; i++){
		vls[i] = elements[i];
	}

  int lSize = env->GetArrayLength(locations);
	int* ls = (int *)malloc(sizeof(int)*lSize);
	elements = env->GetIntArrayElements(locations, 0);
	for(int i = 0; i < lSize; i++){
		ls[i] = elements[i];
	}

  char * r = (char *)env->GetDirectBufferAddress(temp);
  ProductMatrix * pm = NULL;
  if(reconstruct){
    memcpy((char*)&pm, r, sizeof(char*));
    if(pm) pm->cleanup();
    pm=new ProductMatrix(k+m,k,8);
    pm->generate_encoding_matrix();
    memcpy(r, (char*)&pm, sizeof(char*));
  }else{
    memcpy((char*)&pm, r, sizeof(char*));
  }

  pm->set_f2(n, vls, lSize, ls);

  char * inData = (char*)env->GetDirectBufferAddress(in);
  char * outData = (char*)env->GetDirectBufferAddress(out);
  memset(outData, 0, 64+n*size);
  pm->reconstruct_lost_data4(inData,outData,size);
  memcpy(outData+lSize*size,inData+size*(k+m-n)*n/m,4);
}

/**
 * pm decoder for recovery
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
 * @param n, number of fail strip, virtual
 * @param vlocations, array to indicate the virtual locations (different from actual location when
 *   we encounter a fail pattern)
 * @param locations, array to indicate actual fail locations
 * @param size, packet size
 * @param temp, memory segment for intermedia data(jrs object point),
 *   can transform to character array by `env->GetDirectBufferAddress(temp)`
 * @param reconstruct, indicate whether need to reconstruct the matrix or not(restore from temp).
 *
 * @returns void
 *
 * @appendix, need to cleanup legacy pm object if the generate matrix need to reconstruct.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_PMDecoder_00024PMDegradedReadDecoder_pmDecode
  (JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint n, 
   jintArray vlocations, jintArray locations, jint bufSize, jobject temp, jboolean reconstruct){
  Java_org_apache_hadoop_raid_PMDecoder_00024PMRecoveryDecoder_pmDecode(env, 
      jobj, in, out, k, m, n, vlocations, locations, bufSize, temp, reconstruct);
}
