/* Examples/encoder.c
 * Catherine D. Schuman, James S. Plank

 Jerasure - A C/C++ Library for a Variety of Reed-Solomon and RAID-6 Erasure Coding Techniques
 Copright (C) 2007 James S. Plank

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 James S. Plank
 Department of Electrical Engineering and Computer Science
 University of Tennessee
 Knoxville, TN 37996
 plank@cs.utk.edu
 */

/*
 * $Revision: 1.2 $
 * $Date: 2008/08/19 17:53:34 $
 */

/* 

	 This program takes as input an inputfile, k, m, a coding 
	 technique, w, and packetsize.  It creates k+m files from 
	 the original file so that k of these files are parts of 
	 the original file and m of the files are encoded based on 
	 the given coding technique. The format of the created files 
	 is the file name with "_k#" or "_m#" and then the extension.  
	 (For example, inputfile test.txt would yield file "test_k1.txt".)

 */

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
#include "ProductMatrix.hh"
#include "RS.hh"

int main(){
  int k = 7;
  int m = 13;
  int n = 1;
  int bufsize = 6*64*1024;
  char* r = (char*)malloc(sizeof(char)*1024);
  ProductMatrix pm=ProductMatrix(m,k,8);
  pm.generate_encoding_matrix();
  pm.backUpEverything(r + 1);

  int *f = (int *)malloc(sizeof(int)*n);
  for(int i=0; i< n; i++)
    f[i] = 4;

  char flag = ((char*)r)[0];
  pm=ProductMatrix(m,k,8);
  //if(flag == 0){
  // pm.generate_encoding_matrix();
  //  pm.set_f(n, f);
  //  pm.backUpEverything((char*)r + 1);
  //  ((char*)r)[0] = 1;
  //}else{
  //  pm.set_f_nocal(n, f);
  pm.restoreEverything((char*)r + 1);
  pm.set_f(n, f); 
  pm.backUpEverything((char*)r + 1);
  //}
  //
  char* rawdata= (char*)malloc(sizeof(char)*bufsize);

  char* rec = pm.encode_offline_recovery(rawdata, bufsize);


  //if(flag == 2)
  //  pm.cleanup();
  free(f);
  free(rec);
}
