/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include <cstring>

#include "../lib/ProductMatrix.hh"

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
void pmvalidate(int * l){
  int k = 9;
  int m = 8;
  int n = 3;

  ProductMatrix pm=ProductMatrix(k+m,k,8);
  pm.generate_encoding_matrix();
  int ret_val=pm.test_validity(n,l);

  if (ret_val==0){
    int vn = n;
	  int vls[vn+1];
    vls[0] = vn;
	  memcpy((char*)vls+sizeof(int),(char*)l,vn*sizeof(int));
  }else{
    int vn = pm.get_vnumber();
    int vls[vn+1];
	  vls[0]=vn;
  }
  pm.cleanup();

}

int main(){
  int l1[3] = {1, 2, 3};
  pmvalidate(l1);
  int l2[3] = {1, 10, 12};
  pmvalidate(l2);
}
