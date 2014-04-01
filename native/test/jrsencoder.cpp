/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/RS.hh"
#include "../lib/ProductMatrix.hh"

void jrsencode(){
  int k = 9;
  int m = 8;
  int blockSize = 1024*1024;
	RS rs=RS(k+m,k,8);
	rs.generate_encoding_matrix();
	int whole_length=blockSize*k;

  char * inData = (char*)malloc(sizeof(char)*(whole_length+64));
  char * outData = (char*)malloc(sizeof(char)*(64+m*blockSize));

  inData[whole_length+4] = 1;
  
  char flag;
  while(1){
    memset(outData, 0, 64+m*blockSize);
    rs.encode2(inData, outData, whole_length);
    memcpy(outData+m*blockSize,inData+whole_length,4);
    flag=inData[4+whole_length];
    if(flag==1){
      return;
    }
  }
}

int main(){
  jrsencode();
  return 0;
}
