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

void pminit(char* r){
  int k = 9;
  int m = 8;
  ProductMatrix pm=ProductMatrix(k+m,k,8);
  pm.generate_encoding_matrix();
  pm.backUpEverything(r + 1);
}

int main(){
  char r[1024];
  pminit(r);
}
  
