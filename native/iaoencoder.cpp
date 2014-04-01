/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/IA.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_IAOEncoder.h"

/**
 * ia encoder, for original architecture
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
 * @appendix, need to cleanup legacy ia object if the generate matrix need to reconstruct.
 *   But at the moment, I don't have time to write a good solution. So just leave it unclean.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_IAOEncoder_iaoEncode
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint blockSize, jobject reserve){
  
  //To get a byte array
  //Initialize a ProductMatrix Object
  char * r = (char *)env->GetDirectBufferAddress(reserve);
  char flg = r[0];
  IA ia=IA(k+m,k,8);
  if(flg == 0){
    ia.generate_encoding_matrix();
    ia.backUpEverything(r + 1);
      r[0] = 1;
  }else{
    ia.restoreEverything(r + 1);
  }
  int whole_length=blockSize*k;
  char * inData = (char*)env->GetDirectBufferAddress(in);
  char * outData = (char*)env->GetDirectBufferAddress(out);
  //memset(outData, 0, m*blockSize);
    /*
     * Commented by Runhui Jan 22nd, to hard code some parameter for experiments
     * uncomment following lines to return to generalized version
     * BEGIN
     */
    memset(outData, 0, 64+m*blockSize);
    ia.encode2(inData,outData,whole_length);
    memcpy(outData+m*blockSize, inData+whole_length,4);
    /*
     * Commented by Runhui Jan 22nd, to hard code some parameter for experiments
     * uncomment following lines to return to generalized version
     * END
     */
    /*
     * Added by Runhui Jan 22nd, hard coded code for experiments, to return to
     * generalized version, comment the following lines
     * BEGIN
     */
    /*
    int realNum=4;
    int realBlockSize=blockSize/realNum;
    memset(outData, 0, 64+m*blockSize);
    char* tempin = (char*)calloc(realBlockSize*k,sizeof(char));
    char* tempout = (char*)calloc(realBlockSize*m,sizeof(char));
    for(int i=0;i<realNum;i++){
        //gether the input data
        for(int j=0;j<k;j++){
            memcpy(tempin,
                    inData+j*blockSize+i*realBlockSize,
                    realBlockSize);
        }
        memset(tempout,0,blockSize/realNum*m);
        ia.encode2(tempin,tempout,realBlockSize*k);
        //place the output data
        for(int j=0;j<m;j++){
            memcpy(outData+j*blockSize+i*realBlockSize,
                    tempout+j*realBlockSize,
                    realBlockSize);
        }
    }
    free(tempin);
    free(tempout);
    memcpy(outData+m*blockSize, inData+whole_length,4);
    */
    /*
     * Added by Runhui Jan 22nd, hard coded code for experiments, to return to
     * generalized version, comment the following lines
     * END
     */
  //ia.cleanup();
}
