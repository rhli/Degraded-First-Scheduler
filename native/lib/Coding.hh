# ifndef _CODING_HH_
# define _CODING_HH_

# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <math.h>
# include <string.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>
# include <pthread.h>

extern "C"{
# include "Jerasure/galois.h"
# include "Jerasure/jerasure.h"
# include "Jerasure/reed_sol.h"
# include "Jerasure/cauchy.h"
# include "ff/ff.h"
}

class Coding{
	public:
		Coding();
		int _conf_n_;
		int _conf_k_;
		int _conf_w_;
		int _conf_f_;
		int _chunk_number_per_node_;
		int _systemetical_chunk_num_;
		int _encoded_chunk_num_;
		int _total_chunk_num_;
		int _thread_num;

		//Containing the failed nodes
		int* _failed_node_list;
	
		//Encoding Matrix
		int* _ori_encoding_matrix;
		//Stores 1/x, For Galois Field Calculations
		int* _inverse_table;
		int* _downloading_chunk_list;
		int _downloading_chunk_num;

		int* _recovery_equations;

		int _chunk_size_;

		int _XOR_flag;
		int _schedule_flag;
		int _SSE_flag;

		int galois_multiply(int x,int y){return galois_single_multiply(x,y,_conf_w_);};
		int XOR_buffer(char* des, char* src, int length);

		//Initialize the failed lists.
		int set_f(int f,int *list){_conf_f_=f;_failed_node_list=list;return 0;};
		int is_failed(int index);

		int round_file_size(int filesize);
		char* itoa(int value,char* des,int radix);

		int inverse_matrix(int *original_matrix,int *des_matrix,int size);
		int square_cauchy_matrix(int *des,int size);
		int generate_inverse_table();
		int show_matrix(int* mat, int row_num, int column_num);
        int show_shedule(int** schedule);
		int* matrix_multiply(int* mat1,int* mat2,int size);
		int* matrix_multiply2(int* mat1,int* mat2,int row,int column,int mcolumn);
		int show_failed_nodes();

		int set_thread_number(int thread_num){_thread_num=thread_num;return 0;};

        /*
         * Three equations for data encoding/decoding
         * buffer_cal1() for normal encode/decode
         * buffer_cal2() for XORized operation
         * buffer_cal3() for XOR scheduled operation
         */
        int buffer_cal1(char* des,char* src,int* equations,int desNum,int srcNum,int symbolSize);
        int buffer_cal2(char* des,char* src,int* equations,int desNum,int srcNum,int symbolSize);
        int buffer_cal3(char* des,char* src,int** schedule,int desNum,int srcNum,int symbolSize);

		//generate encoding matrix
		virtual int generate_encoding_matrix()=0;
		virtual int* failed_node_repair()=0;

		virtual int cleanup()=0;
		virtual int set_f2(int,int*,int,int*){return 0;};
		virtual int encode2(char*,char*,int){return 0;};
		virtual int encode_offline_recovery2(char*,char*,int){return 0;};
		virtual int reconstruct_lost_data2(char*,char*,int){return 0;};
		virtual int test_validity(int,int*){return 0;};
		virtual int* get_vlist(){return NULL;};
		
		//the functions below are for file operations
		int divide_and_encode_file(char* filename);
		int resemble_file(char* filename, int file_length);
		int data_regeneration(char* filename,int filelength);
		char* reconstruct_lost_data(char** downloaded_content,int length);

		//Library interfaces
		//virtual int encode2(char* ibuffer,char* obuffer,int length);

		//flag control
		int disable_XOR(){_XOR_flag=0;return 0;};
		int disable_schedule(){_schedule_flag=0;return 0;};
		int disable_SSE(){_SSE_flag=0;return 0;};

};

# endif
