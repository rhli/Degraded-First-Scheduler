# ifndef _PRODUCT_MATRIX_HH_
# define _PRODUCT_MATRIX_HH_

# include "Coding.hh"

class ProductMatrix:public Coding{
	int _real_k;
	int _real_n;
	int _fake_num;
	//This is the one multiplies the message matrix
	int* _encoding_matrix;

	//This is a mapping from naive data chunks to messages in message matrix
	int* _msg_mapping;
	int* _delta;
	int* _received;
	int* _downloading_list;

	int** _encoding_schedule;

	int* _sr_mapping_table;
	int* _inverse_sr_mapping_table;
	
	/*
	 * These three are for encode_offline_recovery() and is generated in
	 * set_f()
	 */
	int* _encode_offline_matrix;
	int* _binary_encode_offline_matrix;
	int** _encode_offline_schedule;

	/*
	 * These three are for reconstruct_lost_data() and is generated in set_f()
	 */
	int* _recovery_equations;
	int* _binary_recovery_equations;
	int** _recovery_schedule;
		
	/*
	 * For Bad failure pattern and degraded read
	 * repair part of the failed nodes
	 */
	int _virtual_f;
	int* _virtual_failed_list;
    //int* _user_vlist;
	int _repair_number;
	int* _repair_list;

	int* _XORed_ori_encoding_matrix;
	int _XORed_chunk_number_per_node_;
	int _XORed_systemetical_chunk_num_;
	int _XORed_encoded_chunk_num_;
	int _XORed_total_chunk_num_;

	/*
	 * for degraded read
	 */
	int*** _degraded_read_schedule;

	
	int round_file_size(int filelength);
	int pos_in_downloadinglist(int index);

	int systematical();
	int encoding_matrix_XORization();
	int update_downloading_list();

	public:
	ProductMatrix(int n,int k,int w);

	/*
	 * free memories
	 */
    int cleanup();

	int* get_encoding_matrix(){return _encoding_matrix;};
	int print_parameters();
	int generate_encoding_matrix();
	int show_encoding_matrix();
	int* single_node_repair(int failed,int* list);
	int* multi_node_repair(int num,int* list);
	int set_f(int number,int* list);
	int set_f2(int vnumber,int *vlist,int number,int* list);
	int set_f3(int vnumber,int *vlist,int number,int* list);

	/*
	 * There are cases when the repair scheme do not work, in this case, we need
	 * to loose the repair bandwidth
	 */
	int test_validity(int number,int* list);
	int test_validity2(int number,int* list);
	int get_vnumber(){return _virtual_f;};
	int* get_vlist(){return _virtual_failed_list;};
	
	int generate_received_blocks(char* filename,int node_index);

	/*
	 * different versions of encode functions
	 */
	char* encode(char* content, int length);
	int encode2(char* in_buffer, char* out_buffer, int length);
	int encode3(char* in_buffer, char* out_buffer, int length);

	/*
	 * different versions of decode functions
	 */
	char* reconstruct_lost_data(char** filename,int length);
	int reconstruct_lost_data2(char* in_buffer,char* out_buffer,int length);
	int reconstruct_lost_data3(char* in_buffer,char* out_buffer,int length);
	int reconstruct_lost_data4(char* in_buffer,char* out_buffer,int length);

	/*
	 * different versions of surviving nodes encode functions
	 */
	char* encode_offline_recovery(char* content,int length);
	int encode_offline_recovery2(char* in_buffer,char* out_buffer,int length);

	int data_regeneration(char* filename,int filelength);
	int* failed_node_repair(){return NULL;};

	/*
	 * degraded_read
	 */
	int degradedReadInitialization();
	int degraded_read(char* inbuffer, char* out_buffer, int length, int index);

	/*
	 * This is for LJ's implementation
	 */
	int backUpEverything(char* buffer);
	int restoreEverything(char* buffer);
	int set_f_nocal(int number,int* list);
	int set_f2_nocal(int vnumber,int *vlist,int number,int* list);
	int set_f3_nocal(int vnumber,int *vlist,int number,int* list);
};

# endif
