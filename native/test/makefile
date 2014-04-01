# Makefile
# James S. Plank
# 
# JERASURE - Library for Erasure Coding
# Copright (C) 2007 James S. Plank
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
# 
# James S. Plank
# Department of Electrical Engineering and Computer Science
# University of Tennessee
# Knoxville, TN 37996
# plank@cs.utk.edu

# $Revision: 1.0 $
# $Date: 2007/09/25 15:12:20 $

JAVA_HOME=/home/corec/jdk1.6.0_35

CC=gcc
JCC=g++
CLIB= -O3
BIN=bin
LIB=../lib

CFLAGS=-pg

ALL= jrsencoder jrsoencoder pmencoder pmoencoder jrsdecoder pmdecoder pmrencoder pminit
all: $(ALL)

clean:
	rm -f core $(BIN)/*

#.SUFFIXES: .c .o
#.c.o:
#	$(CC) $(CFLAGS) -c $*.c

jrsencoder: galois jerasure rs
	$(JCC) ${CFLAGS} jrsencoder.cpp $(BIN)/*.o -o $(BIN)/libjrsencoder.test $(CLIB)
jrsoencoder: galois jerasure rs
	$(JCC) ${CFLAGS} jrsoencoder.cpp $(BIN)/*.o -o $(BIN)/libjrsoencoder.test $(CLIB)

pmencoder: galois pm coding jerasure
	$(JCC) ${CFLAGS} pmencoder.cpp $(BIN)/*.o -o $(BIN)/libpmencoder.test $(CLIB)
	$(JCC) ${CFLAGS} iaencoder.cpp $(BIN)/*.o -o $(BIN)/libiaencoder.test $(CLIB)
pmoencoder: galois pm coding jerasure
	$(JCC) ${CFLAGS} pmoencoder.cpp $(BIN)/*.o -o $(BIN)/libpmoencoder.test $(CLIB)
	$(JCC) ${CFLAGS} iaoencoder.cpp $(BIN)/*.o -o $(BIN)/libiaoencoder.test $(CLIB)

jrsdecoder: galois jerasure rs
	$(JCC) ${CFLAGS} jrsdecoder.cpp $(BIN)/*.o -o $(BIN)/libjrsdecoder.test $(CLIB)
pmdecoder: galois pm coding jerasure
	$(JCC) ${CFLAGS} pmdecoder.cpp $(BIN)/*.o -o $(BIN)/libpmdecoder.test $(CLIB)
	$(JCC) ${CFLAGS} pmvalidate.cpp $(BIN)/*.o -o $(BIN)/libpmvalidate.test $(CLIB)
	$(JCC) ${CFLAGS} iadecoder.cpp $(BIN)/*.o -o $(BIN)/libiadecoder.test $(CLIB)
	$(JCC) ${CFLAGS} pmdecoder.cpp $(BIN)/*.o -o $(BIN)/libiavalidate.test $(CLIB)

pmrencoder: galois pm coding jerasure
	$(JCC) $(CFLAGS) pmrencode.cpp $(BIN)/*.o -o $(BIN)/libpmrencode.test $(CLIB)
	$(JCC) $(CFLAGS) iarencode.cpp $(BIN)/*.o -o $(BIN)/libiarencode.test $(CLIB)
pminit: galois pm coding jerasure
	$(JCC) $(CFLAGS) pminit.cpp $(BIN)/*.o -o $(BIN)/libpminit.test $(CLIB)
	$(JCC) $(CFLAGS) iainit.cpp $(BIN)/*.o -o $(BIN)/libiainit.test $(CLIB)

pm:
	${JCC} ${CFLAGS} -o $(BIN)/ProductMatrix.o -c $(LIB)/ProductMatrix.cc $(CLIB)
	${JCC} ${CFLAGS} -o $(BIN)/IA.o -c $(LIB)/IA.cc $(CLIB)
coding:
	$(JCC) ${CFLAGS} -o $(BIN)/Coding.o -c $(LIB)/Coding.cc $(CLIB)
galois:
	${CC} ${CFLAGS} -c $(LIB)/Jerasure/galois.c -o $(BIN)/galois.o $(CLIB)
	${CC} ${CFLAGS} -msse2 -O3 -c $(LIB)/ff/ff.c -o $(BIN)/ff.o
rs: reed_sol coding
	${JCC} ${CFLAGS} -c $(LIB)/RS.cc $(BIN)/*.o -o $(BIN)/RS.o $(CLIB)
reed_sol:
	$(CC) ${CFLAGS} -c $(LIB)/Jerasure/reed_sol.c -o $(BIN)/reed_sol.o $(CLIB)
jerasure:
	${CC} ${CFLAGS} -c $(LIB)/Jerasure/jerasure.c -o $(BIN)/jerasure.o $(CLIB)
	${CC} ${CFLAGS} -c $(LIB)/Jerasure/cauchy.c -o $(BIN)/cauchy.o $(CLIB)
