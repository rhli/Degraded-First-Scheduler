/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/ProductMatrix.hh"

//JNI Header
#include "java_header/org_apache_hadoop_raid_PMDecoder.h"

/**
 * find a validate fail pattern for any fail case.
 *
 * @param env
 * @param jobj
 * @param k
 * @param m
 * @param n, number of failures
 * @param locations, array to indicate failures
 * @param validLocations, integer array to indicate validate fail pattern.
 *   first integer indicates how many failures.
 *
 * @returns void
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_PMDecoder_pmValidate
(JNIEnv * env, jobject jobj, jint k, jint m, jint n, jintArray locations, jintArray validLocations){

  //way to read fails nodes
	int* l = (int *)malloc(sizeof(int)*n);
	jint *elements = env->GetIntArrayElements(locations, 0);
	for(int i = 0; i < n; i++){
		l[i] = elements[i];
	}
	
  ProductMatrix pm=ProductMatrix(k+m,k,8);
  pm.generate_encoding_matrix();
  int ret_val=pm.test_validity(n,l);

  if (ret_val==0){
    int vn = n;
	  int vls[vn+1];
    vls[0] = vn;
	  memcpy((char*)vls+sizeof(int),(char*)l,vn*sizeof(int));
    env->SetIntArrayRegion(validLocations, 0, vn+1, vls);
  }else{
    int vn = pm.get_vnumber();
    int vls[vn+1];
	  vls[0]=vn;
	  memcpy((char*)vls+sizeof(int),(char*)pm.get_vlist(),vn*sizeof(int));
    env->SetIntArrayRegion(validLocations, 0, vn+1, vls);
  }
  pm.cleanup();

}
