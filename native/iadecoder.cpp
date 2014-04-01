/*

   rewrite by LIN Jian
   $Date: 2012/07/10 $

*/
#include <stdlib.h>
#include <cstring>

#include "lib/IA.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_IADecoder_IARecoveryDecoder.h"
#include "java_header/org_apache_hadoop_raid_IADecoder_IADegradedReadDecoder.h"

/**
 * ia decoder for degraded read
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
 * @appendix, need to cleanup legacy ia object if the generate matrix need to reconstruct.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_IADecoder_00024IARecoveryDecoder_iaDecode
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
  IA* ia = NULL;
  if(reconstruct){
    //memcpy((char*)&ia, r, sizeof(char*));
    //if(ia) ia->cleanup();
    ia=new IA(k+m,k,8);
    ia->generate_encoding_matrix();
    ia->set_f2(n, vls, lSize, ls,1);
    memcpy(r, (char*)&ia, sizeof(char*));
  }else{
    memcpy((char*)&ia, r, sizeof(char*));
  }

  //ia->set_f2(n, vls, lSize, ls,1);

  /*
   * Commented by Runhui Jan 22nd to hard code for experiments
   * To return to the generalized version, uncomment the following part
   * BEGIN
   */
  /*
     char * inData = (char*)env->GetDirectBufferAddress(in);
     char * outData = (char*)env->GetDirectBufferAddress(out);
     memset(outData, 0, 64+n*size);
     ia->reconstruct_lost_data2(inData,outData,size);
     memcpy(outData+lSize*size,inData+size*(k+m-n)*n/m,4);
     */
  /*
   * Commented by Runhui Jan 22nd to hard code for experiments
   * To return to the generalized version, uncomment the following part
   * END
   */
  /*
   * Added by Runhui Jan 22nd to hard code for experiments, to return to
   * generalized version, comment the following content
   * BEGIN
   */
  int realNum=8;
  char * inData = (char*)env->GetDirectBufferAddress(in);
  char * outData = (char*)env->GetDirectBufferAddress(out);
  int receivedSize=n*size/m;
  int realBlockSize=size/realNum;
  int realReceivedSize=receivedSize/realNum;
  char* tempin=(char*)calloc(realReceivedSize*(k+m-n),sizeof(char));
  char* tempout=(char*)calloc(lSize*realBlockSize,sizeof(char));
  //memset(outData, 0, 64+n*size);
  for(int i=0;i<realNum;i++){ 
    /*
     * gather received data
     */
    for(int j=0;j<m+k-n;j++){
      memcpy(tempin+j*realReceivedSize,
          inData+j*receivedSize+i*realReceivedSize,
          realReceivedSize);
    }
    memset(tempout,0,lSize*realBlockSize*sizeof(char));
    ia->reconstruct_lost_data2(tempin,tempout,realBlockSize);
    /*
     * place regenerated data
     */
    for(int j=0;j<lSize;j++){
      memcpy(outData+j*size+i*realBlockSize,
          tempout+j*realBlockSize,
          realBlockSize);
    }
  }
  memcpy(outData+lSize*size,inData+size*(k+m-n)*n/m,4);
  free(tempin);
  free(tempout);
  //free(vls);
  //free(ls);
  /*
   * Added by Runhui Jan 22nd to hard code for experiments, to return to
   * generalized version, comment the following content
   * END
   */
}

/**
 * ia decoder for recovery
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
 * @appendix, need to cleanup legacy ia object if the generate matrix need to reconstruct.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_IADecoder_00024IADegradedReadDecoder_iaDecode
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint n, 
 jintArray vlocations, jintArray locations, jint bufSize, jobject temp, jboolean reconstruct){
  Java_org_apache_hadoop_raid_IADecoder_00024IARecoveryDecoder_iaDecode(env, 
      jobj, in, out, k, m, n, vlocations, locations, bufSize, temp, reconstruct);
}
