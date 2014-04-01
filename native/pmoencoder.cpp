/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/ProductMatrix.hh"
#include "lib/RS.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_CGEncoder.h"

/**
 * pm encoder, for original architecture
 *
 * @params env, jni env variable
 * @params jobj, jni obj variable
 * @params in, input data buffer, can transform to character array by `env->GetDirectBufferAddress(in)`
 *   First `size*k` bytes are real data
 * @params out, output data buffer, can transform to character array by `env->GetDirectBufferAddress(out)`
 *   Need to reset before importing data. First `m*size` bytes are real output data
 * @param k
 * @param m
 * @param blockSize, packet size
 * @param reserve, memory segment for intermedia data,
 *   can transform to character array by `env->GetDirectBufferAddress(reserve)`.
 *   If first byte is 0, then we need to reconstruct the encoding matrix.
 *
 * @returns void
 *
 * @appendix, need to cleanup legacy pm object if the generate matrix need to reconstruct.
 *   But at the moment, I don't have time to write a good solution. So just leave it unclean.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_CGEncoder_pmoEncode
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint blockSize, jobject reserve){
  
  //To get a byte array
  //Initialize a ProductMatrix Object
  char * r = (char *)env->GetDirectBufferAddress(reserve);
  char flg = r[0];
  ProductMatrix pm=ProductMatrix(k+m,k,8);
  if(flg == 0){
    //pm.generate_encoding_matrix();
    pm.backUpEverything(r + 1);
      r[0] = 1;
  }else{
    pm.restoreEverything(r + 1);
  }
  int whole_length=blockSize*k;
  void * inData = env->GetDirectBufferAddress(in);
  void * outData = env->GetDirectBufferAddress(out);
  memset(outData, 0, m*blockSize);
  pm.encode3((char*)inData,(char*)outData,whole_length);
  //pm.cleanup();
}
