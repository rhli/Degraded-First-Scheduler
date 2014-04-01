/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/IA.hh"
void iaoencode(char* env){
  //To get a byte array
  //Initialize a ProductMatrix Object
  int k=10;
  int n=20;
  int m=10;
  int blockSize=1048576;
  char * r = env;
  char flg = r[0];
  IA ia=IA(k+m,k,8);
  if(flg == 0){
    ia.generate_encoding_matrix();
    ia.backUpEverything(r + 1);
      r[0] = 1;
  }else{
    ia.restoreEverything(r + 1);
  }
  int whole_length=blockSize*k;
  void * inData = malloc(whole_length*sizeof(char));
  void * outData = malloc(m*blockSize*sizeof(char));
  memset(outData, 0, m*blockSize);
  ia.encode2((char*)inData,(char*)outData,whole_length);
}

int main(){
  char r[4096];
  r[0] = 0;
  iaoencode(r);
  iaoencode(r);
}
