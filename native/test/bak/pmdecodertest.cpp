/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include "ProductMatrix.hh"
#include "RS.hh"
#include <cstring>
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
 * @param n number of failures
 * @param locations array to indicate failures
 * @param size size of one block
 * */
int main(){
  int k = 9;
  int m = 8;
  int n = 3;
  int size = 1024*1024;

  struct timeval t1,t2;
  unsigned long long start_utime, end_utime;
  //way to read fails nodes
	int* l = (int *)malloc(sizeof(int)*n);
	for(int i = 0; i < n; i++){
		l[i] = i;
	}

  printf("sizeof(char)*size*(m+k-n)*n/(k-1) = %d\n", sizeof(char)*size*(m+k-n)*n/(k-1));
  char * inData = (char *)malloc(sizeof(char)*size*(m+k-n)*n/(k-1)+64);

  char * outData = (char *)malloc(sizeof(char)*n*size+64);


  gettimeofday(&t1,NULL);
  ProductMatrix pm=ProductMatrix(k+m,k,8);
  pm.generate_encoding_matrix();
  pm.set_f(n,l);
  char flag;


    memset(outData, 0, 64+n*size);
    pm.reconstruct_lost_data2(inData,outData,size);
    memcpy(outData+n*size,inData+size*(k+m-n)*n/(k-1),4);

  gettimeofday(&t2,NULL);
  start_utime = t1.tv_sec * 1000000 + t1.tv_usec;
  end_utime = t2.tv_sec * 1000000 + t2.tv_usec;
  printf(" runtime = %llu\n", end_utime - start_utime );
}
