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

#include "../lib/IA.hh"

void iainit(char* r){
  int mPlusk=20;
  int k=10;
  IA ia=IA(mPlusk,k,8);
  ia.generate_encoding_matrix();
  ia.backUpEverything(r + 1);
}

int main(){
    char r[1024];
    iainit(r);
}
