/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/ProductMatrix.hh"
#include "../lib/RS.hh"

void pmoencode(char* r){
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
  void * inData = malloc(whole_length*sizeof(char));
  void * outData = malloc(m*blockSize*sizeof(char));
  memset(outData, 0, m*blockSize);
  pm.encode2((char*)inData,(char*)outData,whole_length);
  //pm.cleanup();
}

int main(){
  char r[1024];
  r[0] = 0;
  pmoencode(r);
  pmoencode(r);
}
