#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include "ff.h"

static inline uint64_t read_tsc(void)
{
   uint64_t ts;

   __asm__ volatile(".byte 0x0f,0x31" : "=A" (ts));
   return ts;
}


void addv_local_test(uint8_t *z, uint8_t *x, int size, int cnt)
{
	uint64_t start;
	uint64_t end;
	int i;

	start = read_tsc();

	for (i = 0; i < cnt; i++) {
		ff_addv_local(z, x, size);
	}

	end = read_tsc();

	fprintf(stdout, "addv_local: count = %d, cycle = %llu\n", cnt, end-start);

}

void mulv_test(uint8_t *z, uint8_t *x, uint8_t y, int size, int cnt)
{
	uint64_t start;
	uint64_t end;
	int i;

	start = read_tsc();
	
	for (i = 0; i < cnt; i++) {
		ff_mulv(z, x, y, size);		
	}

	end = read_tsc();
	fprintf(stdout, "mulv: count = %d, cycle = %llu\n", cnt, end-start);
}

void mulv_local_test(uint8_t *x, uint8_t y, int size, int cnt)
{
	uint64_t start;
	uint64_t end;
	int i;

	start = read_tsc();

	for (i = 0; i < cnt; i++) {
		ff_mulv_local(x, y, size);
	}

	end = read_tsc();

	fprintf(stdout, "mulv_local: count = %d, cycle = %llu\n", cnt, end-start);
}

void add_mulv_local_test(uint8_t *z, uint8_t * x, uint8_t y, int size, int cnt)
{
	uint64_t start;
	uint64_t end;
	int i;

	start = read_tsc();
	
	for (i = 0; i < cnt; i++) {
		ff_add_mulv_local(z, x, y, size);
	}

	end = read_tsc();

	fprintf(stdout, "add_mulv_local: count = %d, cycle = %llu\n", cnt, end-start);
}




int main(int argc, char **argv)
{
	int cnt;
	int vector_len = 1024;
	int i;
	uint8_t *z;
	uint8_t *x;
	uint8_t *y;
	int seed = 9;

	srand(seed);

	z = (uint8_t *)malloc(vector_len);
	if (!z) {
		fprintf(stderr, "z malloc error\n");
		return -1;
	} else {
		for (i = 0; i < vector_len; i++) {
			z[i] = rand() & 0xff;
		}
	}

	x = (uint8_t *)malloc(vector_len);
	if (!x) {
		fprintf(stderr, "x malloc error\n");
		free(z);
		return -1;
	} else {
		for (i = 0 ; i < vector_len; i++) {
			x[i] = rand() & 0xff;
		}
	}

	y = (uint8_t *)malloc(vector_len);
	if (!y) {
		fprintf(stderr, "y malloc error\n");
		free(x);
		free(z);
		return -1;
	} else {
		for (i = 0; i < vector_len; i++) {
			y[i] = rand() & 0xff;
		}
	}


	if (argc != 3) {
		fprintf(stdout, "Usage:  perf_ff $operation $num\n");
		fprintf(stdout, "Operation: addv_local mulv mulv_local add_mulv_local\n");
		return -1;
	}

	cnt = atoi(argv[2]);

	ff_init();

	
	if (0 == strcmp(argv[1], "addv_local")) {

		addv_local_test(z, x, vector_len, cnt);

	} else if (0 == strcmp(argv[1], "mulv")) {

		mulv_test(z, x, 19, vector_len, cnt);

	} else if (0 == strcmp(argv[1], "mulv_local")) {

		mulv_local_test(x, 19, vector_len, cnt);

	} else if (0 == strcmp(argv[1], "add_mulv_local")) {

		add_mulv_local_test(z, x, 19, vector_len, cnt);

	} else if (0 == strcmp(argv[1], "all")) {

		addv_local_test(z, x, vector_len, cnt);
		mulv_test(z, x, 19, vector_len, cnt);
		mulv_local_test(x, 19, vector_len, cnt);
		add_mulv_local_test(z, x, 19, vector_len, cnt);
	      
	} else {

		fprintf(stderr, "invalud operaiton\n");
		return -1;
	}

	free(z);
	free(x);
	free(y);

}

