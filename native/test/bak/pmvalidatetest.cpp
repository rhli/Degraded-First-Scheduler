/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include "ProductMatrix.hh"
#include <cstring>

/**
 * @param env
 * @param jobj
 * @param in in buffer
 * @param out out buffer
 * @param k coding scheme with k data block
 * @param m coding scheme with m code block
 * @param n number of failures
 * @param locations array to indicate failures
 * @param size size of one block
 * */
int main(){
  int k = 5, m = 4, n = 1;
  int l[1] = {1};

  ProductMatrix pm=ProductMatrix(k+m,k,8);
  pm.generate_encoding_matrix();
  int ret_val=pm.test_validity(n,l);
  if (ret_val==0){
	  int vls[n+1];
    vls[0] = n;
	  memcpy((char*)vls+sizeof(int),(char*)l,n*sizeof(int));
  }else{
    int vn = pm.get_vnumber();
    int vls[vn+1];
	  vls[0]=vn;
	  memcpy((char*)vls+sizeof(int),(char*)pm.get_vlist(),vn*sizeof(int));
  }
  pm.cleanup();
}
