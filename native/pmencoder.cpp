/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/ProductMatrix.hh"
#include "lib/RS.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_PMEncoder.h"
#include "java_header/org_apache_hadoop_raid_PMEncoder_PMMigrationEncoder.h"

/**
 * pm encoder
 *
 * @params env, jni env variable
 * @params jobj, jni obj variable
 * @params in, input data buffer, can transform to character array by `env->GetDirectBufferAddress(in)`
 *   First `encodedsize*(m+k-n)` bytes are real data, followed by 4 bytes to indicate the seq number
 * @params out, output data buffer, can transform to character array by `env->GetDirectBufferAddress(out)`
 *   Need to reset before importing data. First `n*size` bytes are real output data, followed by 4 bytes 
 *   to indicate the seq number
 * @param k
 * @param m
 * @param blockSize, packet size
 * @param reserve, memory segment for intermedia data(jrs object point),
 *   can transform to character array by `env->GetDirectBufferAddress(temp)`.
 *   If first byte is 0, then we need to reconstruct the encoding matrix.
 *
 * @returns void
 *
 * @appendix, need to cleanup legacy pm object if the generate matrix need to reconstruct.
 *   But at the moment, I don't have time to write a good solution. So just leave it unclean.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_PMEncoder_00024PMMigrationEncoder_pmEncode
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint blockSize, jobject reserve){

  //To get a byte array
  //Initialize a ProductMatrix Object
  char * r = (char *)env->GetDirectBufferAddress(reserve);
  char flg = r[0];
  ProductMatrix pm=ProductMatrix(k+m,k,8);
  if(flg == 0){ 
    pm.generate_encoding_matrix();
    pm.backUpEverything(r + 1); 
    r[0] = 1;
  }else{
    pm.restoreEverything(r + 1); 
  }
  int whole_length=blockSize*k;
  int index;

  jclass cls = env->GetObjectClass(jobj);

  jmethodID mid1 = env->GetMethodID(cls, "take", "()V");
  if (0 == mid1) return;

  char * inData = (char*)env->GetDirectBufferAddress(in);

  jmethodID mid2 = env->GetMethodID(cls, "push", "()V");
  if (0 == mid2) return;

  char * outData = (char*)env->GetDirectBufferAddress(out);

  while(1){
    env->CallVoidMethod(jobj, mid1);

    memset(outData, 0, 64+m*blockSize);
    pm.encode3(inData,outData,whole_length);
    memcpy(outData+m*blockSize, inData+whole_length,4);

    env->CallVoidMethod(jobj, mid2);

    char flag=inData[4+whole_length];
    if(flag==1){
      //pm.cleanup();
      return;
    }
  }
}
