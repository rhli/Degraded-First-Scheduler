/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/ProductMatrix.hh"
#include "../lib/RS.hh"
#include "../lib/IA.hh"

void pmencode(char* r){
  int k = 9;
  int m = 8;
  int blockSize = m*64*1024;

  char flg = r[0];
  ProductMatrix pm=ProductMatrix(k+m,k,8);
  if(flg == 0){ 
    pm.generate_encoding_matrix();
    pm.backUpEverything(r + 1); 
    r[0] = 1;
  }else{
    pm.restoreEverything(r + 1); 
  }
  int whole_length=blockSize*k;
  int index;

  char * inData = (char*)malloc((whole_length+64)*sizeof(char));
  char * outData = (char*)malloc((64+m*blockSize)*sizeof(char));
  inData[4+whole_length] = 1;
  while(1){
    memset(outData, 0, 64+m*blockSize);
    pm.encode3(inData,outData,whole_length);
    memcpy(outData+m*blockSize, inData+whole_length,4);
    char flag=inData[4+whole_length];
    if(flag==1){
      //pm.cleanup();
      return;
    }
  }
  //pm.cleanup();
}

int main(){
  char r[1024];
  r[0] = 0;
  pmencode(r);
  pmencode(r);
  return 0;
}
