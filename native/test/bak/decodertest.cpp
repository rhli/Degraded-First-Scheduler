/* Examples/decoder.c
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
This program takes as input an inputfile, k, m, a coding
technique, w, and packetsize.  It is the companion program
of encoder.c, which creates k+m files.  This program assumes 
that up to m erasures have occurred in the k+m files.  It
reads in the k+m files or marks the file as erased. It then
recreates the original file and creates a new file with the
suffix "decoded" with the decoded contents of the file.

This program does not error check command line arguments because 
it is assumed that encoder.c has been called previously with the
same arguments, and encoder.c does error check.
*/

/*
	rewrite by Lin Jian
	$Date 16/01/2012 $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include "ProductMatrix.hh"

#define N 10

//JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_PMDecoder_pmDecode
//(JNIEnv * env, jobject jobj, jobjectArray jmessage, jobjectArray jparity, jintArray jlocations, jint k, jint m, jint bufferSize, jint n){
int main(){
  int n = 1;
  int m = 2;
  int k = 3;
	int* locations = (int *)malloc(sizeof(int)*n);
	//jint *elements = (env)->GetIntArrayElements(jlocations, 0);
	for(int i = 0; i < n; i++){
		locations[i] = 1;//elements[i];
	}

	void ** message;
	void ** parity;
	message = (void **)malloc(sizeof(void*)*k);
	parity = (void **)malloc(sizeof(void*)*m);
  char ** received_data = (char **)malloc(sizeof(char*)*(k+m-n));
	for(int i = 0; i < k; i++){
		//jobject tmp = (env)->GetObjectArrayElement(jmessage, i);
		//message[i] = (env)->GetDirectBufferAddress(tmp);
    message[i] = malloc(sizeof(char)*1024*1024);
	}
	for(int i = 0; i< m; i++){
		//jobject tmp = (env)->GetObjectArrayElement(jparity, i);
		//parity[i] = (env)->GetDirectBufferAddress(tmp);
    parity[i] = malloc(sizeof(char)*1024*1024);
	}

  int j = 0;
  for(int i = 0; i<m+k; i++){
    bool flag = false;
    for(int l = 0; l < n; l++)
      if(locations[l] == i) flag = true;
    if(!flag)
      if(i < k){
        printf("message %d %d\n", i, j);
        received_data[j++] = (char *)message[i];
      }else{
        printf("parity %d %d\n", i-k, j);
        received_data[j++] = (char *)parity[i-k];
      }
  }
  //locations[0]=0;
  //received_data[0]=(char*)message[0];
  //received_data[1]=(char*)message[1];
  //received_data[2]=(char*)message[2];
  //received_data[3]=(char*)parity[0];

  ProductMatrix pm=ProductMatrix(k+m, k, 8);

  pm.set_thread_number(1);
  pm.generate_encoding_matrix();

  pm.set_f(n, locations);

  pm.reconstruct_lost_data(received_data, 1024*1024);
}
