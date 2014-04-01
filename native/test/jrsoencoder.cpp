/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/ProductMatrix.hh"
#include "../lib/RS.hh"

void jrsoencode(char* r){
  int k = 9;
  int m = 8;
  int blockSize = 1024*1024;
  
  char flg = r[0];
  RS * jrs = NULL;
  if(flg == 0){
    jrs=new RS(k+m,k,8);
    jrs->generate_encoding_matrix();
    memcpy(r+1, (char*)&jrs, sizeof(char*));
    r[0] = 1;
  }else{
    memcpy((char*)&jrs, r+1, sizeof(char*));
  }
  int whole_length=blockSize*k;
  char * inData = (char*)malloc(whole_length);
  char * outData = (char*)malloc(m*blockSize);
  memset(outData, 0, m*blockSize);
  jrs->encode2(inData, outData, whole_length);
}

int main(){
  char r[1024];
  r[0] = 0;
  jrsoencode(r);
  jrsoencode(r);
  return 0;
}
