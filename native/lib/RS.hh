# ifndef _RS_HH_
# define _RS_HH_

extern "C"{
}

# include "Coding.hh"
class RS:public Coding{
	int* _XORed_ori_encoding_matrix;
	int** _encoding_schedule;
	int* _XORed_recovery_equations;
	int** _recovery_schedule;
	int _XORed_systemetical_chunk_num_;
	int _XORed_encoded_chunk_num_;
	int _XORed_total_chunk_num_;
	int _XORed_chunk_number_per_node_;
	int _virtual_failed_num;
	int* _virtual_failed_list;
public:
	RS(int n,int k,int w);
	int cleanup();
	int generate_encoding_matrix();
	int* failed_node_repair();
	int encode2(char* in_buffer,char* out_buffer,int length);
	int reconstruct_lost_data2(char* in_buffer,char* out_buffer,int length);
	int set_f(int number,int* list);
	int set_f2(int vnumber,int *vlist,int number,int* list);
	int backUpEverything(char* buffer);
	int restoreEverything(char* buffer);
	int set_f_nocal(int number,int* list);
	int set_f2_nocal(int vnumber,int* vlist,int number,int* list);
};

# endif
