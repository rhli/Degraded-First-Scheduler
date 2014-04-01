/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/IA.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_IAEncoder.h"
#include "java_header/org_apache_hadoop_raid_IAEncoder_IAMigrationEncoder.h"

/**
 * ia encoder
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
 * @appendix, need to cleanup legacy ia object if the generate matrix need to reconstruct.
 *   But at the moment, I don't have time to write a good solution. So just leave it unclean.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_IAEncoder_00024IAMigrationEncoder_iaEncode
(JNIEnv * env, jobject jobj, jobject in, jobject out, jint k, jint m, jint blockSize, jobject reserve){

  //To get a byte array
  //Initialize a IA Object
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
  int index;

  jclass cls = env->GetObjectClass(jobj);

  jmethodID mid1 = env->GetMethodID(cls, "take", "()V");
  if (0 == mid1) return;

  char * inData = (char*)env->GetDirectBufferAddress(in);

  jmethodID mid2 = env->GetMethodID(cls, "push", "()V");
  if (0 == mid2) return;

  char * outData = (char*)env->GetDirectBufferAddress(out);

  int realNum=8;
  int realBlockSize= blockSize/realNum;
  char * tempin=(char*)calloc(realBlockSize*k, sizeof(char));
  char * tempout=(char*)calloc(realBlockSize*m, sizeof(char));
  int realWholeLength = realBlockSize*k;

  while(1){
    env->CallVoidMethod(jobj, mid1);

    /*
     * Commented by Runhui Jan 22nd, to hard code some parameter for experiments
     * uncomment following lines to return to generalized version
     * BEGIN
     */
    for(int i=0; i < realNum; i++){
      for(int j = 0; j<k; j++)
        memcpy(tempin+j*realBlockSize,
            inData+j*blockSize+i*realBlockSize,
            realBlockSize);
      memset(tempout, 0, m*realBlockSize);
      ia.encode2(tempin,tempout,realWholeLength);
      for(int j = 0; j<m; j++)
        memcpy(outData+j*blockSize+i*realBlockSize,
            tempout+j*realBlockSize,
            realBlockSize);
    }

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
        
         //* gether the input data
         
        for(int j=0;j<k;j++){
            memcpy(tempin,
                    inData+j*blockSize+i*realBlockSize,
                    realBlockSize);
        }
        memset(tempout,0,blockSize/realNum*m);
        ia.encode2(tempin,tempout,realBlockSize*k);
        
         //* place the output data
        
        for(int j=0;j<m;j++){
            memcpy(outData+j*blockSize+i*realBlockSize,
                    tempout+j*realBlockSize,
                    realBlockSize);
        }
    }
    memcpy(outData+m*blockSize, inData+whole_length,4);
    free(tempin);
    free(tempout);
    */
    /*
     * Added by Runhui Jan 22nd, hard coded code for experiments, to return to
     * generalized version, comment the following lines
     * END
     */

    env->CallVoidMethod(jobj, mid2);

    char flag=inData[4+whole_length];
    if(flag==1){
      //ia.cleanup();
      return;
    }
  }
}
