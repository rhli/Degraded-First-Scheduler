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

//JNI Header

int main(){
  int bufsize = 6*64*1024;
  int m = 13;
  int k = 7;
  int n = 3;
	char * rawdata = (char*)malloc(sizeof(char)*6*64*1024);
	char * encdata = (char*)malloc(sizeof(char)*6*64*1024);
  int *f = (int *)malloc(sizeof(int)*n);
    f[0] = 0;
    f[1] = 1;
    f[2] = 2;

  void * r = malloc(sizeof(char)*1024);

  struct timeval t1,t2;
  unsigned long long start_utime, end_utime;

  for(int i = 0; i < 5; i++){
    puts("new round1");
    char flag = ((char*)r)[0];
    puts("before contruction");
    ProductMatrix pm=ProductMatrix(m,k,8);
    puts("after contruction");
    if(flag == 0){
      pm.generate_encoding_matrix();
      pm.set_f2(n, f,n,f);
      puts("before back");
      pm.backUpEverything(((char*)r + 1));
      puts("after back");
      ((char*)r)[0] = 1;
      puts("after back2");
    }else{
      //pm.set_f_nocal(n, f);

      puts("before restore");
      pm.restoreEverything((char*)r + 1);
      puts("after restore");
      puts("before set_f");
      gettimeofday(&t1,NULL);
      pm.set_f2_nocal(n, f,n, f);
      gettimeofday(&t2,NULL);
      puts("after set_f");

      start_utime = t1.tv_sec * 1000000 + t1.tv_usec;
      end_utime = t2.tv_sec * 1000000 + t2.tv_usec;
      printf(" runtime = %llu\n", end_utime - start_utime );
      //char* rec = 
        pm.encode_offline_recovery2(rawdata, encdata,bufsize);
    }

  }
  //if(flag == 2)
  //  pm.cleanup();
  free(f);
  //free(rec);
}
