/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/ProductMatrix.hh"
#include "lib/RS.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_JRSOEncoder.h"

/**
 * jrs encoder for original architecture
 *
 * @params env, jni env variable
 * @params jobj, jni obj variable
 * @params in, input data buffer, can transform to character array by `env->GetDirectBufferAddress(in)`
 *   First `size*k` bytes are real data
 * @params out, output data buffer, can transform to character array by `env->GetDirectBufferAddress(out)`
 *   Need to reset before importing data. First `n*size` bytes are real output data
 * @param k
 * @param m
 * @param blockSize, packet size(normally 1MB)
 * @param reserve, memory segment for intermedia data(jrs object pointer),
 *   can transform to character array by `env->GetDirectBufferAddress(reserve)`
 *
 * @returns void
 *
 * @appendix, do NOT need to add cleanup code.
 */

JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_JRSOEncoder_jrsoEncode
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint blockSize, jobject reserve){

  //To get a byte array
  //Initialize a ProductMatrix Object
  char * r = (char *)env->GetDirectBufferAddress(reserve);
  char flg = r[0];
  RS * jrs = NULL;
  if(flg == 0){
    jrs=new RS(k+m,k,8);
    jrs->generate_encoding_matrix();
    //jrs.backUpEverything(r + 1);
    memcpy(r+1, (char*)&jrs, sizeof(char*));
    r[0] = 1;
  }else{
    //jrs.restoreEverything(r + 1);
    memcpy((char*)&jrs, r+1, sizeof(char*));
  }
  int whole_length=blockSize*k;
  void * inData = env->GetDirectBufferAddress(in);
  void * outData = env->GetDirectBufferAddress(out);
  memset(outData, 0, m*blockSize);
  jrs->encode2((char*)inData,(char*)outData,whole_length);
  //jrs.cleanup();
}
