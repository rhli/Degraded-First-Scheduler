/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/ProductMatrix.hh"
#include "../lib/RS.hh"

void pmdecode(bool reconstruct, char* r){
  int k = 8;
  int m = 7;
  int n = 3;
  int lSize = 3;
  int vls[3] = {0, 1, 2};
  int ls[3] = {0, 1, 2};
  int size = m*64*1024;

  ProductMatrix * pm = NULL;
  if(reconstruct){
    memcpy((char*)&pm, r, sizeof(char*));
    //if(pm) pm->cleanup();
    pm=new ProductMatrix(k+m,k,8);
    pm->generate_encoding_matrix();
    memcpy(r, (char*)&pm, sizeof(char*));
  }else{
    memcpy((char*)&pm, r, sizeof(char*));
  }


  char * inData = (char*)malloc((size*(k+m-n)*n/(k-1)+64)*sizeof(char));
  char * outData = (char*)malloc((n*size+64)*sizeof(char));
  memset(outData, 0, 64+n*size);

  //gettimeofday(&t1,NULL);
  pm->set_f2(n, vls, lSize, ls);
  pm->reconstruct_lost_data4(inData,outData,size);
  memcpy(outData+lSize*size,inData+size*(k+m-n)*n/(k-1),4);
  //gettimeofday(&t2,NULL);
  //printf("%f\n",t2-t1+(t2.tv_sec-t1.tv_sec)+(float)(t2.tv_usec-t1.tv_usec)/1000000);
  //pm->cleanup();
}

int main(){
  char r[1024]={0};
  pmdecode(true, r);
  pmdecode(false, r);
  return 0;
}
