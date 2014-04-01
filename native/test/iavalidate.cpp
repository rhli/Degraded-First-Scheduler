/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "lib/IA.hh"

void iavalidate(int * l){
  int k = 10;
  int m = 10;
  int n = 3;

  IA pm=IA(k+m,k,8);
  pm.generate_encoding_matrix();
  int ret_val=pm.test_validity(n,l);

  if (ret_val==0){
    int vn = n;
	  int vls[vn+1];
    vls[0] = vn;
	  memcpy((char*)vls+sizeof(int),(char*)l,vn*sizeof(int));
  }else{
    int vn = n+ret_val;
    int vls[vn+1];
	  vls[0]=vn;
	  memcpy((char*)vls+sizeof(int),(char*)ia.get_vlist(),vn*sizeof(int));
  }
  pm.cleanup();

}

int main(){
  int l1[3] = {1, 2, 3};
  iavalidate(l1);
  int l2[3] = {0, 1, 19};
  iavalidate(l2);
}
