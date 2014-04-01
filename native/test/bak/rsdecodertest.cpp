/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include "ProductMatrix.hh"
#include "RS.hh"
#include <cstring>

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
	int k = 3;
	int n = 1;
	int m = 2;
	int* l = (int *)malloc(sizeof(int));
	l[0] = 0;
	int size = 1024*1024;
puts("good 1");
  RS rs=RS(k+m,k,8);
puts("good 1.3");
  rs.generate_encoding_matrix();
puts("good 1.6");
  rs.set_f(n,l);
puts("good 2");
	void * inData = malloc(sizeof(char)*((k+m-1)*1024*1024+5));
puts("good 3");
	void * outData = malloc(sizeof(char)*(1024*1024+4));
puts("good 4");
  rs.reconstruct_lost_data2((char*)inData+5,(char*)outData+4,size);
puts("good 5");
}
