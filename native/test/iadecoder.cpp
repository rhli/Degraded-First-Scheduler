/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/IA.hh"

void iadecode(bool reconstruct, char* r){
    //struct time_val t1,t2;
  int k = 10;
  int m = 10;
  int n = 3;
  int lSize = 3;
  int vls[3] = {0, 1, 2};
  int ls[3] = {0, 1, 2};
  int size = m*64*1024;

  IA* pm = NULL;
  if(reconstruct){
    memcpy((char*)&pm, r, sizeof(char*));
    pm=new IA(k+m,k,8);
    pm->generate_encoding_matrix();
    memcpy(r, (char*)&pm, sizeof(char*));
  }else{
    memcpy((char*)&pm, r, sizeof(char*));
  }


  char * inData = (char*)malloc((size*(k+m-n)*n/(k-1)+64)*sizeof(char));
  char * outData = (char*)malloc((n*size+64)*sizeof(char));
  memset(outData, 0, 64+n*size);

  //gettimeofday(&t1,NULL);
  pm->set_f2(n, vls, lSize, ls,1);

  int receivedSize=n*size/m;
  int realBlockSize=size/8;
  int realReceivedSize=receivedSize/8;
  char* tempin=(char*)calloc(realReceivedSize*(k+m-n),sizeof(char));
  char* tempout=(char*)calloc(n*realBlockSize,sizeof(char));
  //memset(outData, 0, 64+n*size);
  for(int i=0;i<8;i++){ 
      /*
       * gather received data
       */
      for(int j=0;j<m+k-n;j++){
          memcpy(tempin,
                  inData+j*receivedSize+i*realReceivedSize,
                  realReceivedSize);
      }
      memset(tempout,0,n*realBlockSize*sizeof(char));
      pm->reconstruct_lost_data2(tempin,tempout,realBlockSize);
      /*
       * place regenerated data
       */
      for(int j=0;j<n;j++){
          memcpy(outData+j*size+i*realBlockSize,
                  tempout+j*realBlockSize,
                  realReceivedSize);
      }
  }
  memcpy(outData+lSize*size,inData+size*(k+m-n)*n/m,4);
  //gettimeofday(&t2,NULL);
  //printf("%f\n",t2-t1+(t2.tv_sec-t1.tv_sec)+(float)(t2.tv_usec-t1.tv_usec)/1000000);
  //pm->cleanup();
}

int main(){
  char r[1024]={0};
  iadecode(true, r);
  iadecode(false, r);
  return 0;
}
