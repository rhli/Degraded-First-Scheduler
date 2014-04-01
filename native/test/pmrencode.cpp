/*

	 rewrite by LIN Jian
	 $Date: 2012/01/11 $

 */
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include "../lib/ProductMatrix.hh"
#include "../lib/RS.hh"

void pmrencode(int k, int m, char* r, int f[], int n){
  int bufsize = m*64*8*1024;

  char * rawdata = (char *)malloc(bufsize*sizeof(char));

  ProductMatrix pm=ProductMatrix(m+k,k,8);
  pm.generate_encoding_matrix();
  pm.set_f2(n, f, n, f); 
  pm.backUpEverything((char*)r + 1);

  char* rec = (char*)calloc(bufsize*n/m,sizeof(char));
  pm.encode_offline_recovery2(rawdata, rec, bufsize);
  pm.cleanup();

  free(rec);
}

int main(){
  printf("start pm test");
  char r[1024];
  struct timeval tv, tv2;
  unsigned long long start_utime, end_utime;
  gettimeofday(&tv,NULL);
  start_utime = tv.tv_sec * 1000000 + tv.tv_usec;
  for(int i = 0; i<16; i++){
    int f[1];
    f[0] = i;
    pmrencode(8, 8, r, f, 1);
    gettimeofday(&tv2,NULL);
    end_utime= tv2.tv_sec * 1000000 + tv2.tv_usec;
    printf("Run 8, 8, f=%d time=%lld\n", f[0], end_utime-start_utime);
  }
  for(int i = 0; i<20; i++){
    int f[1];
    f[0] = i;
    pmrencode(10, 10, r, f, 1);
    gettimeofday(&tv2,NULL);
    end_utime= tv2.tv_sec * 1000000 + tv2.tv_usec;
    printf("Run 10, 10, f=%d time=%lld\n", f[0], end_utime-start_utime);
  }
  return 0;
}
