/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include "ProductMatrix.hh"
#include "RS.hh"
#include <cstring>

//JNI Header
#include "org_apache_hadoop_raid_PMDecoder.h"
#include "org_apache_hadoop_raid_PMDecoder_PMRDecoder.h"

/**
 * @param env
 * @param jobj
 * @param in in buffer
 * @param out out buffer
 * @param k coding scheme with k data block
 * @param m coding scheme with m code block
 * @param n number of failures
 * @param locations array to indicate failures
 * @param size size of one block
 * */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_PMDecoder_00024PMRDecoder_pmTest
(JNIEnv * env, jobject jobj, jint k, jint m, jint n, jintArray locations, jintArray locations2){


  //way to read fails nodes
	int* l = (int *)malloc(sizeof(int)*n);
	jint *elements = (env)->GetIntArrayElements(locations, 0);
	for(int i = 0; i < n; i++){
		l[i] = elements[i];
	}
	
  ProductMatrix pm=ProductMatrix(k+m,k,8);
  pm.generate_encoding_matrix();
  int ret_val=pm.test_validity(n,l);
  if (ret_val==0){
	  locations2[0]=n;
	  memcpy((char*)locations2+sizeof(int),(char*)l,n*sizeof(int));
  }else{
	  locations2[0]=pm.get_vnumber();
	  memcpy((char*)locations2+sizeof(int),(char*)pm.get_vlist(),locations2[0]*sizeof(int));
  }
  pm.cleanup();
  return;
  
}
