/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/ProductMatrix.hh"
#include "../lib/RS.hh"

void jrsdr(bool reconstruct, char* r){
  int k = 9;
  int m = 8;
  int n = 3;
  int l[3] = {1, 2, 3};
  int size = 1024*1024;

  char * inData = (char *)malloc(sizeof(char)*(64+k*size));
  char * outData = (char *)malloc(sizeof(char)*(64+n*size));
  
  RS * jrs = NULL;
  if(reconstruct){
    memcpy((char*)&jrs, r, sizeof(char*));
    if(!jrs) jrs->cleanup();
    jrs=new RS(k+m,k,8);
    jrs->generate_encoding_matrix();
    memcpy(r, (char*)&jrs, sizeof(char*));
  }else{
    memcpy((char*)&jrs, r, sizeof(char*));
  }

  jrs->set_f(n, l);
  memset(outData, 0, 64+n*size);
  jrs->reconstruct_lost_data2(inData,outData,size);
  memcpy(outData+n*size,inData+size*k,4);
}

void jrsdecode(bool reconstruct, char* r){
  int k = 9;
  int m = 8;
  int n = 3;
  int l[3] = {1, 2, 3};
  int size = 1024*1024;

  char * inData = (char *)malloc(sizeof(char)*(64+k*size));
  char * outData = (char *)malloc(sizeof(char)*(64+n*size));
    
  RS * jrs = NULL;
  if(reconstruct){
    memcpy((char*)&jrs, r, sizeof(char*));
    //if(!jrs) jrs->cleanup();
    jrs=new RS(k+m,k,8);
    jrs->generate_encoding_matrix();
    memcpy(r, (char*)&jrs, sizeof(char*));
  }else{
    memcpy((char*)&jrs, r, sizeof(char*));
  }

  //jrs->set_f2(n, l, n, l);
  jrs->set_f(n, l);
  memset(outData, 0, 64+n*size);
  jrs->reconstruct_lost_data2(inData,outData,size);
  memcpy(outData+n*size,inData+size*k,4);
}

int main(){
  //char r1[1024]={0};
  //jrsdr(true, r1);
  //jrsdr(false, r1);
  char r2[1024]={0};
  jrsdecode(true, r2);
  jrsdecode(false, r2);
}

