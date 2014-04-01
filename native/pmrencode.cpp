/* Examples/encoder.c
 * Catherine D. Schuman, James S. Plank

 Jerasure - A C/C++ Library for a Variety of Reed-Solomon and RAID-6 Erasure Coding Techniques
 Copright (C) 2007 James S. Plank

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 James S. Plank
 Department of Electrical Engineering and Computer Science
 University of Tennessee
 Knoxville, TN 37996
 plank@cs.utk.edu
 */

/*
 * $Revision: 1.2 $
 * $Date: 2008/08/19 17:53:34 $
 */

/* 

	 This program takes as input an inputfile, k, m, a coding 
	 technique, w, and packetsize.  It creates k+m files from 
	 the original file so that k of these files are parts of 
	 the original file and m of the files are encoded based on 
	 the given coding technique. The format of the created files 
	 is the file name with "_k#" or "_m#" and then the extension.  
	 (For example, inputfile test.txt would yield file "test_k1.txt".)

 */

/*

	 rewrite by LIN Jian
	 $Date: 2012/01/11 $

 */
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include "lib/ProductMatrix.hh"
#include "lib/RS.hh"

//JNI Header
#include "java_header/org_apache_hadoop_hdfs_server_datanode_PMBlockSender.h"

/**
 * pm datanode encoder, the encoding matrix has been initialized by pminit
 *
 * @params env, jni env variable
 * @params jobj, jni obj variable
 * @params data, input data buffer, can transform to character array by `(env)->GetByteArrayElements(data, NULL)`
 *   First `bufsize` bytes are real data
 * @params code, output data buffer, can update data by `env->SetByteArrayRegion(code, 0, bufsize*n/(k-1), (jbyte *)rec)`
 *   `encodedsize` bytes are real output data
 * @param mPlusk, m+k
 * @param k
 * @param n, number of fail strip
 * @param fails, array to indicate the fail locations
 * @param bufsize, packet size
 * @param reserve, memory segment for intermedia data(jrs object point),
 *   can transform to character array by `env->GetDirectBufferAddress(temp)`.
 *
 * @returns void
 *
 * @appendix, need to cleanup legacy pm object if the generate matrix need to reconstruct.
 *   But at the moment, I don't have time to write a good solution. So just leave it unclean.
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_hdfs_server_datanode_PMBlockSender_pmREncode
  (JNIEnv * env, jobject jobj, jbyteArray data, jbyteArray code, 
   jint bufsize, jint mPlusk, jint k, jint n, jintArray fails, jobject reserve){

  char * rawdata = (char *)((env)->GetByteArrayElements(data, NULL));
  int *f = (int *)malloc(sizeof(int)*n);
  jint *tmp = env->GetIntArrayElements(fails, 0);
  for(int i=0; i< n; i++)
    f[i] = tmp[i];

  void * r = env->GetDirectBufferAddress(reserve);
  char flag = ((char*)r)[0];
  ProductMatrix pm=ProductMatrix(mPlusk,k,8);
  //pm.generate_encoding_matrix();
  //if(flag == 0){
  //  pm.generate_encoding_matrix();
    //pm.set_f(n, f);
  //  pm.set_f2(n, f, n, f); 
  //  pm.backUpEverything((char*)r + 1);
  //  ((char*)r)[0] = 1;
  //}else{
    //  pm.set_f_nocal(n, f);
    pm.restoreEverything((char*)r + 1);
    pm.set_f2(n, f, n, f); 
    pm.backUpEverything((char*)r + 1);
  //}

  char* rec = (char*)calloc(bufsize*n/(mPlusk- k),sizeof(char));
  pm.encode_offline_recovery2(rawdata, rec, bufsize);

  env->SetByteArrayRegion(code, 0, bufsize*n/(mPlusk - k), (jbyte *)rec);

  if(flag == 2)
    pm.cleanup();
  free(f);
  free(rec);
  env->ReleaseByteArrayElements(data, (jbyte *)rawdata, 0);
}
