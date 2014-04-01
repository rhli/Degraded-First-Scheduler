# ifndef _IA_HH_
# define _IA_HH_

# include "Coding.hh"

extern int enumerate_init(int,int,int*);
extern int enumerate_next(int,int,int*);

class IA:public Coding{
        int* _dual_enc_matrix;
        int* _final_enc_matrix;
        int* _offline_enc_vec;
        int* _XOR_ori_encoding_matrix;
        int** _enc_XOR_Schedule;

        int* _XOR_recovery_equations;
        int** _rec_XOR_Schedule;
        int*** _rec_XOR_Schedules;

        int* _enc_offline_mat;
        int* _XOR_enc_offline_mat;
        int** _enc_ol_XOR_Schedule;

        /*
         * This is for situations where n!=2k, more precisely, n>2k
         * Some logical nodes will not exist in the real system, we call them
         * virtual nodes
         */
        int _survNum;
        int _real_n;
        int _real_k;
        int* _user_fail_list;
        int* _virtual_node;
        int* _rv_map;
        int* _inverse_rv_map;

        /*
         * The following are for the situation where t nodes are failed yet we
         * just reconstruct t' nodes, e.g. bad failure pattern and degraded read,
         * etc.
         */
        int _real_f;
        int* _real_list;

        /*
         * for bad failure patterns
         */
        int _vf;
        int* _vlist;
        int* _user_vlist;

        /*
         * This function reorganize the recovery equation, so that recovery process
         * can be more easily performed. It is called in the end of multi_node_repair()
         */
        int* reEqReorg(int* reEq);
        int encMatXORization();
        int recMatXORization();
        int recMatXORization2();/* for pipeline decoding*/
        int encOlGenerator();

        /*
         * functions handling situation where n>2k
         */
        int initVirtualNode();
        int isVirtual(int index);

        /*
         * partialize the recovery equations for degraded read and bad failure pattern
         */
        int partializeRE();

        /*
         * For LJ's implementation
         */
        int freeBuffer(char* pointer);
    public:
        IA(int n, int k, int w);
        int* single_node_repair(int index);
        int* multi_node_repair();
        int set_f(int num, int* list,int mode);
        int set_f_nocal(int num, int* list,int mode);

        /*
         * This is a twin function of initVirtualNode(), by calling this we can set virtual nodes
         * by ourselves instead of by default
         */
        int setVirtualNode(int*);

        /*
         * for bad failure patterns and degraded read
         */
		int set_f2(int,int*,int,int*,int);
		int set_f2_nocal(int,int*,int,int*,int);
		int set_f3(int,int*,int,int*,int);

        /*
         * For LJ's implementation
         */
        int backUpEverything(char* buffer);
        int restoreEverything(char* buffer);

        /*
         * virtual functions
         */
		int generate_encoding_matrix();
		int* failed_node_repair(){return 0;};

		int cleanup();
		int set_f2(int,int*,int,int*){return 0;};
		int encode2(char*,char*,int);
		int encode_offline_recovery2(char*,char*,int);
		int reconstruct_lost_data2(char*,char*,int);
		int reconstruct_lost_data3(char*,char*,int,int);
		int test_validity(int,int*);
		int* get_vlist(){return _vlist;};
};

# endif
