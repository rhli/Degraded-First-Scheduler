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

#include "../lib/IA.hh"

void iarencode(char* env){

  int m = 20;
  int k = 10;
  int n = 3;
  char * r=env;
  int mPlusk=20;
  int f[3] = {0, 1, 2};
  int bufsize = (m-k)*64*1024;

  char * rawdata = (char *)malloc(bufsize*sizeof(char));
  //IA ia=IA(mPlusk,k,8);
  //ia.generate_encoding_matrix();
    //ia.restoreEverything((char*)r + 1);
    //ia.set_f2(n, f, n, f, 2); 
    //ia.backUpEverything((char*)r + 1);

  char* rec = (char*)calloc(bufsize*n/(mPlusk-k),sizeof(char));
  int realBufSize=bufsize/8;
  int realReceivedSize=realBufSize*n/(mPlusk-k);
  for(int i=0;i<8;i++){
    memcpy(rec+i*realReceivedSize,
            rawdata+i*realBufSize,
            realReceivedSize);
  }
  //ia.cleanup();
}


int main(){
  char r[1024];
  iarencode(r);
  return 0;
}
