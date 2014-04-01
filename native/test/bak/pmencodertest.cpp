/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */

#include <stdlib.h>
#include "ProductMatrix.hh"
#include "RS.hh"
#include <cstring>
#include <sys/time.h>
#include <cstdio>

using namespace std;


//JNI Header

/**
 * @param env
 * @param jobj
 * @param in in buffer
 * @param out out buffer
 * @param k coding scheme with k data block
 * @param m coding scheme with m code block
 * @param blockSize size of one block
 * */
int main(){
  //array structure
  //first four byte: int sequence number
  //following one byte: byte 1 for terminating the task
  //following k*blocksize/m*blocksize byte: the data

  //To get a byte array
  //Initialize a ProductMatrix Object
  int k = 9;
  int m = 8;
  int blockSize = 512*1024;
  struct timeval t1,t2;
  unsigned long long start_utime, end_utime;


  ProductMatrix pm=ProductMatrix(k+m,k,8);
  pm.generate_encoding_matrix();

  int whole_length=blockSize*k;

  void * inData = malloc(sizeof(char)*k*blockSize+64);
  void * outData = malloc(sizeof(char)*m*blockSize+64);

  memset(inData, 0, k*blockSize+64);
  memset(outData, 0, m*blockSize+64);
  gettimeofday(&t1,NULL);
  pm.encode2(((char*)inData)+64,((char*)outData)+64,whole_length);
  gettimeofday(&t2,NULL);
  start_utime = t1.tv_sec * 1000000 + t1.tv_usec;
  end_utime = t2.tv_sec * 1000000 + t2.tv_usec;
  printf(" runtime = %llu\n", end_utime - start_utime );

}
