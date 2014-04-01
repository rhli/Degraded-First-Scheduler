/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/IA.hh"

void iaencode(char* env){
  int n=20;
  int k=10;
  int m=10;
  char * r = env;
  char flg = r[0];
  int blockSize=1048576;
  IA ia=IA(k+m,k,8);
  if(flg == 0){ 
    ia.generate_encoding_matrix();
    ia.backUpEverything(r + 1); 
    r[0] = 1;
  }else{
    ia.restoreEverything(r + 1); 
  }
  int whole_length=blockSize*k;
  int parityLen=blockSize*m;
  int index;

  char * inData = (char*)calloc(whole_length+64,sizeof(char));
  char * outData = (char*)calloc(parityLen+64,sizeof(char));

  inData[4+whole_length]=1;

  while(1){

    int realBlockSize=blockSize/8;
    memset(outData, 0, 64+m*blockSize);
    char* tempin = (char*)calloc(realBlockSize*k,sizeof(char));
    char* tempout = (char*)calloc(realBlockSize*m,sizeof(char));
    for(int i=0;i<8;i++){
        /*
         * gether the input data
         */
        for(int j=0;j<k;j++){
            memcpy(tempin,
                    inData+j*blockSize+i*realBlockSize,
                    realBlockSize);
        }
        memset(tempout,0,blockSize/8*m);
        ia.encode2(tempin,tempout,realBlockSize*k);
        /*
         * place the output data
         */
        for(int j=0;j<m;j++){
            memcpy(outData+j*blockSize+i*realBlockSize,
                    tempout+j*realBlockSize,
                    realBlockSize);
        }
    }
    memcpy(outData+m*blockSize, inData+whole_length,4);

    char flag=inData[4+whole_length];
    if(flag==1){
      return;
    }
  }
}

int main(){
  char r[1024];
  r[0] = 0;
  iaencode(r);
  iaencode(r);
  return 0;
}
