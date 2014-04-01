CC=gcc
JCC=g++
CLIB= -O3 #-m64
BIN=./bin
LIB=./lib

JERASUREDIR = $(LIB)/Jerasure

CFLAGS=-shared -fPIC -I$(HOME)/include  -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux 

ALL= jrsencoder jrsoencoder pmencoder pmoencoder iaencoder iaoencoder jrsdecoder pmdecoder iadecoder pmrencoder pminit iarencoder iainit
all: $(ALL)

JERASURE = $(BIN)/galois.o $(BIN)/cauchy.o $(BIN)/reed_sol.o $(BIN)/jerasure.o
CODING = $(BIN)/ProductMatrix.o $(BIN)/IA.o $(BIN)/RS.o

clean:
	rm -f core $(BIN)/*

#.SUFFIXES: .c .o
#.c.o:
#	$(CC) $(CFLAGS) -c $*.c

jrsencoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} jrsencoder.cpp $(BIN)/*.o -o $(BIN)/libjrsencoder.so $(CLIB)
jrsoencoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} jrsoencoder.cpp $(BIN)/*.o -o $(BIN)/libjrsoencoder.so $(CLIB)
pmencoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} pmencoder.cpp $(BIN)/*.o -o $(BIN)/libpmencoder.so $(CLIB)
iaencoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} iaencoder.cpp $(BIN)/*.o -o $(BIN)/libiaencoder.so $(CLIB)
pmoencoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} pmoencoder.cpp $(BIN)/*.o -o $(BIN)/libpmoencoder.so $(CLIB)
iaoencoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} iaoencoder.cpp $(BIN)/*.o -o $(BIN)/libiaoencoder.so $(CLIB)

jrsdecoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} jrsdecoder.cpp $(BIN)/*.o -o $(BIN)/libjrsdecoder.so $(CLIB)
pmdecoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} pmdecoder.cpp $(BIN)/*.o -o $(BIN)/libpmdecoder.so $(CLIB)
	$(JCC) ${CFLAGS} pmvalidate.cpp $(BIN)/*.o -o $(BIN)/libpmvalidate.so $(CLIB)
iadecoder: $(JERASURE) $(CODING)
	$(JCC) ${CFLAGS} iadecoder.cpp $(BIN)/*.o -o $(BIN)/libiadecoder.so $(CLIB)
	$(JCC) ${CFLAGS} iavalidate.cpp $(BIN)/*.o -o $(BIN)/libiavalidate.so $(CLIB)

pmrencoder: $(JERASURE) $(CODING)
	$(JCC) $(CFLAGS) pmrencode.cpp $(BIN)/*.o -o $(BIN)/libpmrencode.so $(CLIB)
iarencoder: $(JERASURE) $(CODING)
	$(JCC) $(CFLAGS) iarencode.cpp $(BIN)/*.o -o $(BIN)/libiarencode.so $(CLIB)
pminit: $(JERASURE) $(CODING)
	$(JCC) $(CFLAGS) pminit.cpp $(BIN)/*.o -o $(BIN)/libpminit.so $(CLIB)
iainit: $(JERASURE) $(CODING)
	$(JCC) $(CFLAGS) iainit.cpp $(BIN)/*.o -o $(BIN)/libiainit.so $(CLIB)

#pm:
#	${JCC} ${CFLAGS} -o $(BIN)/ProductMatrix.o -c $(LIB)/ProductMatrix.cc $(CLIB)
#ia:
#	${JCC} ${CFLAGS} -o $(BIN)/IA.o -c $(LIB)/IA.cc $(CLIB)
#rs:
#	${JCC} ${CFLAGS} -c $(LIB)/RS.cc $(BIN)/*.o -o $(BIN)/RS.o $(CLIB)
$(BIN)/IA.o: $(LIB)/IA.cc $(LIB)/IA.hh $(BIN)/Coding.o
	$(JCC) ${CFLAGS} -o $@ -c $< $(CLIB) $(BIN)/Coding.o
$(BIN)/ProductMatrix.o: $(LIB)/ProductMatrix.cc $(LIB)/ProductMatrix.hh $(BIN)/Coding.o
	$(JCC) ${CFLAGS} -o $@ -c $< $(CLIB) $(BIN)/Coding.o
$(BIN)/RS.o: $(LIB)/RS.cc $(LIB)/RS.hh $(BIN)/Coding.o
	$(JCC) ${CFLAGS} -o $@ -c $< $(CLIB) $(BIN)/Coding.o
$(BIN)/Coding.o: $(LIB)/Coding.cc $(LIB)/Coding.hh $(JERASURE)
	$(JCC) ${CFLAGS} -o $@ -c $< $(CLIB)

# Jerasure library

$(BIN)/galois.o: $(JERASUREDIR)/galois.c $(JERASUREDIR)/galois.h
	${CC} ${CFLAGS} -c $< -o $@ $(CLIB)
$(BIN)/cauchy.o: $(JERASUREDIR)/cauchy.c $(JERASUREDIR)/cauchy.h
	$(CC) $(CFLAGS) -o $@ -c $< $(CLIB)
$(BIN)/reed_sol.o: $(JERASUREDIR)/reed_sol.c $(JERASUREDIR)/reed_sol.h
	$(CC) ${CFLAGS} -c $< -o $@ $(CLIB)
$(BIN)/jerasure.o: $(JERASUREDIR)/jerasure.c $(JERASUREDIR)/jerasure.h
	${CC} ${CFLAGS} -c $< -o $@ $(CLIB)
$(BIN)/ff.o : $(LIB)/ff/ff.c
	${CC} ${CFLAGS} -msse2 -O3 -c $< -o $@
