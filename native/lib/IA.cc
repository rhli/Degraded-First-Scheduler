/* 
 * This is an implementation of regenerating codes of interference alignment
 * construction.
 * Mar 4th, 2013:
 *  We are making this work more tightly with pipelined model, meaning that
 *  we receive one encoded block and then update the reconstructed_data and
 *  this will make the computation parallel with data transmission.
 *  TODO: we should have (_conf_n-_conf_f) schedules. modify this
 */

# include "IA.hh"
# define EXPERIMENT 1


IA::IA(int n,int k,int w){
    _failed_node_list=NULL;
    _inverse_table=NULL;
    _recovery_equations=NULL;
    _ori_encoding_matrix=NULL;
    _dual_enc_matrix=NULL;
    _final_enc_matrix=NULL;
    _offline_enc_vec=NULL;
    _XOR_ori_encoding_matrix=NULL;
    _enc_XOR_Schedule=NULL;
    _XOR_recovery_equations=NULL;
    _rec_XOR_Schedule=NULL;
    _rec_XOR_Schedules=NULL;
    _enc_offline_mat=NULL;
    _XOR_enc_offline_mat=NULL;
    _enc_ol_XOR_Schedule=NULL;
    _user_fail_list=NULL;
    _virtual_node=NULL;
    _rv_map=NULL;
    _inverse_rv_map=NULL;
    _real_list=NULL;
    _vlist=NULL;
    _user_vlist=NULL;
    _real_n=n;
    _real_k=k;
	_conf_k_=_real_n-_real_k;
	_conf_n_=_conf_k_*2;
	_conf_w_=w;
	_chunk_number_per_node_=_conf_n_-_conf_k_;
	_systemetical_chunk_num_=_conf_k_*(_chunk_number_per_node_);
	_encoded_chunk_num_=(_conf_n_-_conf_k_)*(_chunk_number_per_node_);
	_total_chunk_num_=_systemetical_chunk_num_+_encoded_chunk_num_;
    initVirtualNode();
}

int IA::isVirtual(int index){
    for(int i=0;i<_conf_n_-_real_n;i++){
        if(_virtual_node[i]==index){
            return 1;
        }
    }
    return 0;
}

int IA::initVirtualNode(){
    /*
     * Generate the _virtual_node list
     */
    if(_real_k!=_conf_k_){
        _virtual_node=(int*)calloc(_conf_k_-_real_k,sizeof(int));
        for(int i=0;i<_conf_k_-_real_k;i++){
            _virtual_node[i]=i;
        }
    }
    _inverse_rv_map=(int*)calloc(_conf_n_,sizeof(int));

    /*
     * Generate the _rv_map, which is a map from real node index to logical node index
     */
    _rv_map=(int*)calloc(_real_n,sizeof(int));
    for(int i=0;i<_real_n;i++){
        _rv_map[i]=i;
    }
    for(int i=0;i<_conf_n_-_real_n;i++){
        int marker=0;
        for(int j=0;j<_real_n;j++){
            if(_rv_map[j]==_virtual_node[i]){
                marker=j;
            }
        }
        for(int j=marker;j<_real_n;j++){
            _rv_map[j]++;
        }
    }
    for(int i=0;i<_conf_n_;i++){
        _inverse_rv_map[i]=-1;
    }
    for(int i=0;i<_real_n;i++){
        _inverse_rv_map[_rv_map[i]]=i;
    }
    //printf("rv Map:\n");
    //show_matrix(_rv_map,1,_real_n);
    return 0;
}

int IA::setVirtualNode(int* list){
    /*
     * set _virtual_node list
     * We assume the user know the rules of this coding scheme...
     */
    if(_virtual_node!=NULL){
        free(_virtual_node);
        _virtual_node=NULL;
    }
    if(_rv_map!=NULL){
        free(_rv_map);
        _rv_map=NULL;
    }
    _virtual_node=list;

    /*
     * Generate the _rv_map, which is a map from real node index to logical node index
     */
    _rv_map=(int*)calloc(_real_n,sizeof(int));
    _inverse_rv_map=(int*)calloc(_conf_n_,sizeof(int));
    for(int i=0;i<_real_n;i++){
        _rv_map[i]=i;
    }
    for(int i=0;i<_conf_n_-_real_n;i++){
        int marker=0;
        for(int j=0;j<_real_n;j++){
            if(_rv_map[j]==_virtual_node[i]){
                marker=j;
            }
        }
        for(int j=marker;j<_real_n;j++){
            _rv_map[j]++;
        }
    }
    for(int i=0;i<_conf_n_;i++){
        _inverse_rv_map[i]=-1;
    }
    for(int i=0;i<_real_n;i++){
        _inverse_rv_map[_rv_map[i]]=i;
    }
    //printf("rv Map:\n");
    //show_matrix(_rv_map,1,_real_n);
    return 0;
}

int IA::generate_encoding_matrix(){
	generate_inverse_table();
    _ori_encoding_matrix=(int*)calloc(_total_chunk_num_*_systemetical_chunk_num_,
            sizeof(int));
    _dual_enc_matrix=(int*)calloc(_total_chunk_num_*_systemetical_chunk_num_,
            sizeof(int));
    _offline_enc_vec=(int*)calloc(_conf_n_*_conf_k_,sizeof(int));
    int* UMat=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
    int* VMat=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
    int* PMat=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
    int* invUMat=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
    int* invPMat=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
    int greekK=2;
    //generate the matrix P first
    square_cauchy_matrix(PMat,_conf_k_);
    //generate the matrix V and U, let us make V to be I
    for(int i=0;i<_conf_k_;i++){
        VMat[i*_conf_k_+i]=1;
    }
    //The equation for calculating Matrix U should be U=1/k * P
    int KMinus=_inverse_table[greekK];
    for(int i=0;i<_conf_k_;i++){
        for(int j=0;j<_conf_k_;j++){
            UMat[i*_conf_k_+j]=galois_multiply(KMinus,PMat[i*_conf_k_+j]);
        }
    }
    inverse_matrix(UMat,invUMat,_conf_k_);
    inverse_matrix(PMat,invPMat,_conf_k_);
    //printf("U matrix:\n");
    //show_matrix(UMat,_conf_k_,_conf_k_);
    //printf("P matrix:\n");
    //show_matrix(PMat,_conf_k_,_conf_k_);

    /*
     * Now the lower part of encoding matrix. It was composed of _conf_k_*_conf_k_ 
     * square matrices, each of size _conf_k_*_conf_k_. We generate them one by one
     */
    for(int i=0;i<_systemetical_chunk_num_;i++){
        _ori_encoding_matrix[i*_systemetical_chunk_num_+i]=1;
    }
    for(int i=0;i<_conf_k_;i++){
        for(int j=0;j<_conf_k_;j++){
            /*
             * generate the square matrix at i-th row and j-th column 
             * The way to calculate should be u_i * v_j^t + p[j,i] * I
             */
            int rStart=(_conf_k_+j)*_conf_k_;
            int cStart=i*_conf_k_;
            int *VRow=(int*)calloc(_conf_k_,sizeof(int));
            int *URow=(int*)calloc(_conf_k_,sizeof(int));
            for(int k=0;k<_conf_k_;k++){
                VRow[k]=VMat[k*_conf_k_+i];
                URow[k]=UMat[k*_conf_k_+j];
            }
            int* tmpMat=matrix_multiply2(URow,VRow,_conf_k_,_conf_k_,1);
            for(int k=0;k<_conf_k_;k++){
                tmpMat[k*_conf_k_+k]^=PMat[i*_conf_k_+j];
            }
            for(int k=0;k<_conf_k_;k++){
                for(int l=0;l<_conf_k_;l++){
                    _ori_encoding_matrix[(rStart+k)*_systemetical_chunk_num_+l+cStart]
                        =tmpMat[l*_conf_k_+k];
                }
            }
            free(tmpMat);
            //show_matrix(tmpMat,_conf_k_,_conf_k_);
        }
    }
    //printf("Encoding Matrix:\n");
    //show_matrix(_ori_encoding_matrix,_total_chunk_num_,_systemetical_chunk_num_);

    /*
     * Generate dual encoding matrix which maps the parity to data and data to parity
     */
    //for(int i=0;i<_conf_k_;i++){
    //    for(int j=0;j<_conf_k_;j++){
    //        int rStart=i*_conf_k_;
    //        int cStart=j*_conf_k_;
    //        for(int l=0;l<_conf_k_;l++){
    //            for(int m=0;m<_conf_k_;m++){
    //                _dual_enc_matrix[(rStart+l)*_systemetical_chunk_num_+cStart+m]
    //                    =galois_multiply(VMat[m*_conf_k_+i],invUMat[j*_conf_k_+l]);
    //                if(l==m){
    //                    int tmpval=galois_multiply(galois_multiply(greekK,greekK),
    //                            invPMat[l*_conf_k_+m]);
    //                    _dual_enc_matrix[(rStart+l)*_systemetical_chunk_num_+cStart+m] 
    //                        ^=tmpval;
    //                }
    //                _dual_enc_matrix[(rStart+l)*_systemetical_chunk_num_+cStart+m]
    //                    =galois_multiply(_inverse_table[1^galois_multiply(greekK,greekK)], 
    //                            _dual_enc_matrix[(rStart+l)*_systemetical_chunk_num_
    //                            +cStart+m]);
    //            }
    //        }
    //    }
    //}
    inverse_matrix(_ori_encoding_matrix+_systemetical_chunk_num_*_systemetical_chunk_num_,
            _dual_enc_matrix,
            _systemetical_chunk_num_);
    for(int i=0;i<_systemetical_chunk_num_;i++){ 
        _dual_enc_matrix[_systemetical_chunk_num_*_systemetical_chunk_num_ 
            +i*_systemetical_chunk_num_+i]=1;
    }

    /*
     * Create metadata for data reconstruction
     */
    memcpy((char*)_offline_enc_vec,(char*)VMat,_conf_k_*_conf_k_*sizeof(int));
    for(int i=0;i<_conf_k_;i++){
        for(int j=0;j<_conf_k_;j++){
            _offline_enc_vec[_conf_k_*_conf_k_+i*_conf_k_+j]=UMat[j*_conf_k_+i];
        }
    }
    //printf("Dual Encoding Matrix:\n");
    //show_matrix(_dual_enc_matrix,_total_chunk_num_,_systemetical_chunk_num_);
    //show_matrix(matrix_multiply(_dual_enc_matrix,_ori_encoding_matrix+
    //            _systemetical_chunk_num_*_systemetical_chunk_num_,
    //            _systemetical_chunk_num_),
    //        _systemetical_chunk_num_,
    //        _systemetical_chunk_num_);
    //printf("Enc Vec:\n");
    //show_matrix(_offline_enc_vec,_conf_n_,_conf_k_);
    free(UMat);
    free(VMat);
    free(PMat);
    free(invUMat);
    free(invPMat);

    /*
     * Now, in case n>2k, remove the equations in _virtual_node
     */
    if(_real_n!=_conf_n_){
        /*
         * _ori_encoding_matrix is a matrix of _total_chunk_num_ rows
         * and _systemetical_chunk_num_ columns
         * We need to create a matrix of _systemetical_chunk_num_ rows
         * and _real_k*_conf_k_ colomns
         */
        _final_enc_matrix=(int*)calloc(_conf_k_*_conf_k_*_real_k*_conf_k_,
                sizeof(int));
        int index=0;
        for(int i=0;i<_conf_k_;i++){
            int vir=0;
            for(int j=0;j<_conf_k_-_real_k;j++){
                if(_virtual_node[j]==i){
                    vir=1;
                    break;
                }
            }
            if(vir==1){
                continue;
            }
            //printf("**%d %d\n",index,i);
            for(int j=0;j<_conf_k_;j++){
                /*
                 * Copy every column to _final
                 */
                for(int k=0;k<_conf_k_*_conf_k_;k++){
                    //printf("%d %d\n",k*_real_k*_conf_k_+(index*_conf_k_+j),
                    //        k*_conf_k_*_conf_k_+(i*_conf_k_+j));
                    _final_enc_matrix[k*_real_k*_conf_k_+(index*_conf_k_+j)]
                        =_ori_encoding_matrix[k*_conf_k_*_conf_k_+(i*_conf_k_+j)
                        +_systemetical_chunk_num_*_systemetical_chunk_num_];
                }
            }
            index++;
        }
        //show_matrix(_final_enc_matrix,_conf_k_*_conf_k_,_conf_k_*_real_k);
    }else{
        _final_enc_matrix=_ori_encoding_matrix+
            _systemetical_chunk_num_*_systemetical_chunk_num_;
    }
    encMatXORization();
    return 0;
}

int IA::encMatXORization(){
    if(_XOR_flag==1){
        /*
         * generate XORized encoding matrix
         */
        //printf("%d %d %d\n",_conf_w_,_real_k,_conf_k_);
        _XOR_ori_encoding_matrix=jerasure_matrix_to_bitmatrix(
                _real_k*_conf_k_,
                _conf_k_*_conf_k_,
                _conf_w_,
                _final_enc_matrix);
        if(_schedule_flag==1){
            /*
             * generate XOR schedule
             */
            _enc_XOR_Schedule=jerasure_smart_bitmatrix_to_schedule(
                    _real_k*_conf_k_,
                    _conf_k_*_conf_k_,
                    _conf_w_,
                    _XOR_ori_encoding_matrix);
        }
    }
    //show_matrix(_XOR_ori_encoding_matrix,
    //        _conf_k_*_conf_k_*_conf_w_,
    //        _conf_k_*_real_k*_conf_w_);
    //int index=0;
    //while(_enc_XOR_Schedule[index][0]!=-1){
    //    printf("%d %d %d %d %d\n",
    //            _enc_XOR_Schedule[index][0],
    //            _enc_XOR_Schedule[index][1],
    //            _enc_XOR_Schedule[index][2],
    //            _enc_XOR_Schedule[index][3],
    //            _enc_XOR_Schedule[index][4]);
    //    index++;
    //}
    return 0;
}

int* IA::single_node_repair(int index){
    int* recoveryEquations=(int*)calloc(_chunk_number_per_node_*(_conf_n_-1),sizeof(int));
    int* recvData=(int*)calloc((_conf_n_-1)*_systemetical_chunk_num_,sizeof(int));

    /*
     * This part consists of 2 situations:
     * Node index is a data node, and Node index is a parity node, we consider them
     * separately
     */
    if(index<_conf_k_){
        /*
         * Data Node repair
         */
        /*
         * First we collect the recv'd data
         */
        int survInd=0;
        for(int i=0;i<_conf_n_;i++){
            if(i==index){
                continue;
            }
            //printf("%d %d\n",i,survInd);
            for(int j=0;j<_conf_k_;j++){
                if(_offline_enc_vec[index*_conf_k_+j]!=0){
                    for(int k=0;k<_systemetical_chunk_num_;k++){
                        recvData[survInd*_systemetical_chunk_num_+k]
                            ^=galois_multiply(_offline_enc_vec[index*_conf_k_+j],
                                    _ori_encoding_matrix[(i*_conf_k_+j)*
                                    _systemetical_chunk_num_+k]);
                    }
                }
            }
            survInd++;
        }
        //show_matrix(recvData,_conf_n_-1,_systemetical_chunk_num_);

        /*
         * Now we eliminate the interference alignment
         * Here comes the funny stuff!!
         */
        survInd=0;
        for(int i=0;i<_conf_k_;i++){
            recoveryEquations[i*(_conf_n_-1)+i+_conf_k_-1]=1;
        }
        for(int i=0;i<_conf_k_;i++){
            if(i==index){
                continue;
            }
            int coefficient=0;
            for(int j=0;j<_conf_k_;j++){
                for(int k=i*_conf_k_;k<(i+1)*_conf_k_;k++){
                    if((recvData[survInd*_systemetical_chunk_num_+k]!=0) 
                            &&(recvData[(j+_conf_k_-1)*_systemetical_chunk_num_+k]!=0)){
                        coefficient=recvData[(j+_conf_k_-1)*_systemetical_chunk_num_+k]*
                            _inverse_table[recvData[survInd*_systemetical_chunk_num_+k]];
                        recoveryEquations[j*(_conf_n_-1)+survInd]=coefficient;
                        break;
                    }
                }
            }
            survInd++;
        }
        //printf("After IA Elimination\n");
        //show_matrix(recoveryEquations,_conf_k_,_conf_n_-1);

        /*
         * Now generate the final recovery equations
         */
        int* oriSqr=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
        int* invOriSqr=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
        int rStart=_conf_k_-1;
        int cStart=index*_conf_k_;
        for(int i=0;i<_conf_k_;i++){
            for(int j=0;j<_conf_k_;j++){
                oriSqr[i*_conf_k_+j]=
                    recvData[(rStart+i)*_systemetical_chunk_num_+j+cStart];
            }
        }
        inverse_matrix(oriSqr,invOriSqr,_conf_k_);
        int* temp=recoveryEquations;
        recoveryEquations=matrix_multiply2(invOriSqr,temp,_conf_k_,_conf_n_-1,_conf_k_);

        //printf("Final Recovery Equations\n");
        //show_matrix(recoveryEquations,_conf_k_,_conf_n_-1);
        //int* validate;
        //validate=matrix_multiply2(recoveryEquations,recvData,_conf_k_,
        //        _systemetical_chunk_num_,_conf_n_-1);
        //show_matrix(validate,_conf_k_,_systemetical_chunk_num_);
        free(temp);
        free(oriSqr);
        free(invOriSqr);
        free(recvData);

    }else{
        /*
         * Parity Node repair
         * The process is almost the same as reconstruction for data node, but
         * we use the dual encoding matrix instead
         */
        /*
         * First we collect the recv'd data
         */
        int survInd=0;
        for(int i=0;i<_conf_n_;i++){
            if(i==index){
                continue;
            }
            //printf("%d %d\n",i,survInd);
            for(int j=0;j<_conf_k_;j++){
                if(_offline_enc_vec[index*_conf_k_+j]!=0){
                    for(int k=0;k<_systemetical_chunk_num_;k++){
                        recvData[survInd*_systemetical_chunk_num_+k]
                            ^=galois_multiply(_offline_enc_vec[index*_conf_k_+j],
                                    _dual_enc_matrix[(i*_conf_k_+j)*
                                    _systemetical_chunk_num_+k]);
                    }
                }
            }
            survInd++;
        }
        //show_matrix(recvData,_conf_n_-1,_systemetical_chunk_num_);

        /*
         * Now we eliminate the interference alignment
         * Here comes the funny stuff!!
         */
        survInd=_conf_k_;
        for(int i=0;i<_conf_k_;i++){
            recoveryEquations[i*(_conf_n_-1)+i]=1;
        }
        for(int i=_conf_k_;i<_conf_n_;i++){
            if(i==index){
                continue;
            }
            int coefficient=0;
            for(int j=0;j<_conf_k_;j++){
                for(int k=(i-_conf_k_)*_conf_k_;k<(i+1-_conf_k_)*_conf_k_;k++){
                    if((recvData[survInd*_systemetical_chunk_num_+k]!=0) 
                            &&(recvData[j*_systemetical_chunk_num_+k]!=0)){
                        coefficient=galois_multiply(
                                recvData[j*_systemetical_chunk_num_+k], 
                                _inverse_table[recvData[survInd*_systemetical_chunk_num_+k]]);
                        recoveryEquations[j*(_conf_n_-1)+survInd]=coefficient;
                        break;
                    }
                }
            }
            survInd++;
        }
        //printf("After IA Elimination\n");
        //show_matrix(recoveryEquations,_conf_k_,_conf_n_-1);

        /*
         * Now generate the final recover equations
         */
        int* oriSqr=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
        int* invOriSqr=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
        int rStart=0;
        int cStart=(index-_conf_k_)*_conf_k_;
        for(int i=0;i<_conf_k_;i++){
            for(int j=0;j<_conf_k_;j++){
                oriSqr[i*_conf_k_+j]=
                    recvData[(rStart+i)*_systemetical_chunk_num_+j+cStart];
            }
        }
        inverse_matrix(oriSqr,invOriSqr,_conf_k_);
        int* temp=recoveryEquations;
        recoveryEquations=matrix_multiply2(invOriSqr,temp,_conf_k_,_conf_n_-1,_conf_k_);

        //printf("Final Recovery Equations\n");
        //show_matrix(recoveryEquations,_conf_k_,_conf_n_-1);
        //int* validate;
        //validate=matrix_multiply2(recoveryEquations,recvData,_conf_k_,
        //        _systemetical_chunk_num_,_conf_n_-1);
        //show_matrix(validate,_conf_k_,_systemetical_chunk_num_);
        free(temp);
        free(recvData);
        free(oriSqr);
        free(invOriSqr);

    }
    return recoveryEquations;
}

int* IA::multi_node_repair(){
    /*
     * Generate repair equations for the failed nodes set by set_f()
     * For bad failure pattern, return NULL
     */

    /*
     * Validate the result
     */
    int* recvData=(int*)calloc(_conf_f_*(_conf_n_-_conf_f_)*_systemetical_chunk_num_,sizeof(int));
    int survInd=0;
    for(int i=0;i<_conf_n_;i++){
        if(is_failed(i)==1){
            continue;
        }
        if(isVirtual(i)==1){
            continue;
        }
        for(int k=0;k<_conf_f_;k++){
            int index=_failed_node_list[k];
            if(i==index){
                continue;
            }
            //printf("%d %d\n",i,survInd);
            for(int j=0;j<_conf_k_;j++){
                if(_offline_enc_vec[index*_conf_k_+j]!=0){
                    for(int k=0;k<_systemetical_chunk_num_;k++){
                        //printf("%d %d %d\n",galois_multiply(_offline_enc_vec[index*_conf_k_+j],
                        //            _ori_encoding_matrix[(i*_conf_k_+j)*
                        //            _systemetical_chunk_num_+k]),
                        //        recvData[survInd*_systemetical_chunk_num_+k],
                        //        recvData[survInd*_systemetical_chunk_num_+k]
                        //            ^(galois_multiply(_offline_enc_vec[index*_conf_k_+j],
                        //                    _ori_encoding_matrix[(i*_conf_k_+j)*
                        //                    _systemetical_chunk_num_+k])));
                        recvData[survInd*_systemetical_chunk_num_+k]
                            ^=galois_multiply(_offline_enc_vec[index*_conf_k_+j],
                                    _ori_encoding_matrix[(i*_conf_k_+j)*
                                    _systemetical_chunk_num_+k]);
                    }
                }
            }
            survInd++;
        }
    }

    int* recoveryEquations=(int*)calloc(_conf_k_*_conf_f_*_conf_f_*(_conf_n_-1),
            sizeof(int));
    /*
     * First we initialize the recovery equations to a system of equations with
     * both real symbols and virtual symbols, this is simple and can be done
     * simply by calling single_node_repair().
     */
    for(int i=0;i<_conf_f_;i++){
        /*
         * Get the recovery equations for each lost node, one-by-one
         */
        int* recEqua=this->single_node_repair(_failed_node_list[i]);
        int rStart=i*_conf_k_;
        int cStart=i*(_conf_n_-1);
        for(int j=0;j<_conf_k_;j++){
            for(int k=0;k<_conf_n_-1;k++){
                recoveryEquations[(rStart+j)*_conf_f_*(_conf_n_-1)+cStart+k]
                    =recEqua[j*(_conf_n_-1)+k];
            }
        }
        free(recEqua);
    }
    //printf("Initial Equations\n");
    //show_matrix(recoveryEquations,_conf_f_*_conf_k_,_conf_f_*(_conf_n_-1));
    //show_matrix(matrix_multiply2(recoveryEquations,recvData,
    //            _conf_f_*_conf_k_,_systemetical_chunk_num_,_conf_f_*(_conf_n_-1)),
    //        _conf_f_*_conf_k_,_systemetical_chunk_num_);

    /*
     * By this recovery equations, we generate a system of equations describing
     * the virtual symbols, one equation for one virtual symbols.
     */
    int* virtEqua=(int*)calloc(_conf_f_*(_conf_f_-1)*_conf_f_*(_conf_n_-1),
            sizeof(int));
    for(int i=0;i<_conf_f_;i++){
        int fromNode=_failed_node_list[i];
        int rStart=i*(_conf_f_-1);
        int toInd=0;
        for(int j=0;j<_conf_f_;j++){
            if(j==i){
                continue;
            }
            int toNode=_failed_node_list[j];
            for(int k=0;k<_conf_k_;k++){
                if(_offline_enc_vec[toNode*_conf_k_+k]!=0){
                    int coefficient=_offline_enc_vec[toNode*_conf_k_+k];
                    for(int l=0;l<_conf_f_*(_conf_n_-1);l++){
                        virtEqua[(rStart+toInd)*_conf_f_*(_conf_n_-1)+l]
                            ^=galois_multiply(coefficient,
                                    recoveryEquations[(i*_conf_k_+k)*_conf_f_*(_conf_n_-1)
                                        +l]);
                    }
                }
            }
            toInd++;
        }
    }
    //printf("virtual Equations:\n");
    //show_matrix(virtEqua,_conf_f_*(_conf_f_-1),_conf_f_*(_conf_n_-1));

    /*
     * get out the coefficient of virtual symbols and calculate the inverse matrix
     * Further use the real symbols to represent the virtual symbols
     */
    int* coefficient=(int*)calloc(_conf_f_*(_conf_f_-1)*_conf_f_*(_conf_f_-1),
            sizeof(int));
    for(int i=0;i<_conf_f_;i++){
        for(int j=0;j<i;j++){
            int cePos=j*(_conf_f_-1)+i-1;
            int vePos=i*(_conf_n_-1)+_failed_node_list[j];
            for(int k=0;k<_conf_f_*(_conf_f_-1);k++){
                coefficient[k*_conf_f_*(_conf_f_-1)+cePos]
                    ^=virtEqua[k*_conf_f_*(_conf_n_-1)+vePos];
                virtEqua[k*_conf_f_*(_conf_n_-1)+vePos]=0;
            }
        }
        for(int j=i+1;j<_conf_f_;j++){
            int cePos=j*(_conf_f_-1)+i;
            int vePos=i*(_conf_n_-1)+_failed_node_list[j]-1;
            for(int k=0;k<_conf_f_*(_conf_f_-1);k++){
                coefficient[k*_conf_f_*(_conf_f_-1)+cePos]
                    ^=virtEqua[k*_conf_f_*(_conf_n_-1)+vePos];
                virtEqua[k*_conf_f_*(_conf_n_-1)+vePos]=0;
            }
        }
    }
    for(int i=0;i<_conf_f_*(_conf_f_-1);i++){
        //int sender=i;
        //for(int j=0;j<i;j++){
        //    int receiver=j;
        //    coefficient[(i*(_conf_f_-1)+receiver)*_conf_f_*(_conf_f_-1)
        //        +j*(_conf_f_-1)+sender-1]^=1;
        //}
        //for(int j=i+1;j<_conf_f_;j++){
        //    int receiver=j-1;
        //    coefficient[(i*(_conf_f_-1)+receiver)*_conf_f_*(_conf_f_-1)
        //        +j*(_conf_f_-1)+sender]^=1;
        //}
        coefficient[i*_conf_f_*(_conf_f_-1)+i]^=1;
    }
    //printf("Coefficient:\n");
    //show_matrix(coefficient,_conf_f_*(_conf_f_-1),_conf_f_*(_conf_f_-1));
    int* invCoefficient=(int*)calloc(_conf_f_*(_conf_f_-1)*_conf_f_*(_conf_f_-1),
            sizeof(int));
    int retVal=inverse_matrix(coefficient,invCoefficient,_conf_f_*(_conf_f_-1));
    if(retVal==1){
        /*
         * Uninvertible, means bad failure pattern
         */
        _recovery_equations=NULL;
        return NULL;
    }
    //printf("inverse Coefficient:\n");
    //show_matrix(invCoefficient,_conf_f_*(_conf_f_-1),_conf_f_*(_conf_f_-1));
    int* virSol=matrix_multiply2(invCoefficient,virtEqua,
            _conf_f_*(_conf_f_-1),_conf_f_*(_conf_n_-1),_conf_f_*(_conf_f_-1));
    //printf("Virtual symbol solution:\n");
    //show_matrix(virSol,_conf_f_*(_conf_f_-1),_conf_f_*(_conf_n_-1));

    /*
     * Replace the virtual symbols with the equations in the solutions.
     */
    for(int i=0;i<_conf_f_;i++){
        for(int j=0;j<i;j++){
            int vePos=i*(_conf_n_-1)+_failed_node_list[j];
            for(int k=0;k<_conf_k_*_conf_f_;k++){
                int coefficient=recoveryEquations[k*_conf_f_*(_conf_n_-1)+vePos];
                recoveryEquations[k*_conf_f_*(_conf_n_-1)+vePos]=0;
                if(coefficient!=0){
                    for(int l=0;l<_conf_f_*(_conf_n_-1);l++){
                        recoveryEquations[k*_conf_f_*(_conf_n_-1)+l]
                            ^=galois_multiply(coefficient,
                                    virSol[(j*(_conf_f_-1)+i-1)*_conf_f_*(_conf_n_-1)+l]);
                    }
                }
            }
        }
        for(int j=i+1;j<_conf_f_;j++){
            int vePos=i*(_conf_n_-1)+_failed_node_list[j]-1;
            for(int k=0;k<_conf_f_*_conf_k_;k++){
                int coefficient=recoveryEquations[k*_conf_f_*(_conf_n_-1)+vePos];
                recoveryEquations[k*_conf_f_*(_conf_n_-1)+vePos]=0;
                if(coefficient!=0){
                    for(int l=0;l<_conf_f_*(_conf_n_-1);l++){
                        recoveryEquations[k*_conf_f_*(_conf_n_-1)+l]
                            ^=galois_multiply(coefficient,
                                    virSol[(j*(_conf_f_-1)+i)*_conf_f_*(_conf_n_-1)+l]);
                    }
                }
            }
        }
    }
    //printf("Final recovery equations:\n");
    //show_matrix(recoveryEquations,_conf_k_*_conf_f_,_conf_f_*(_conf_n_-1));

    //show_matrix(matrix_multiply2(virSol,recvData,
    //            _conf_f_*(_conf_f_-1),_systemetical_chunk_num_,_conf_f_*(_conf_n_-1)),
    //        _conf_f_*(_conf_f_-1),_systemetical_chunk_num_);
    //show_matrix(matrix_multiply2(recoveryEquations,recvData,
    //            _conf_f_*_conf_k_,_systemetical_chunk_num_,_conf_f_*(_conf_n_-1)),
    //        _conf_f_*_conf_k_,_systemetical_chunk_num_);

    free(coefficient);
    free(invCoefficient);
    free(virSol);
    free(virtEqua);

    //printf("Final recovery equations:\n");
    //show_matrix(recoveryEquations,_conf_k_*_conf_f_,_conf_f_*(_conf_n_-1));
    int* nEq=this->reEqReorg(recoveryEquations);
    free(recoveryEquations);
    //printf("reorged recovery equations:\n");
    //show_matrix(nEq,_conf_k_*_conf_f_,_conf_f_*(_conf_n_-_conf_f_));
    _recovery_equations=nEq;
    //show_matrix(recvData,_conf_f_*(_conf_n_-_conf_f_),_systemetical_chunk_num_);
    //show_matrix(matrix_multiply2(_recovery_equations,recvData,
    //            _conf_f_*_conf_k_,_systemetical_chunk_num_,_conf_f_*_survNum),
    //        _conf_f_*_conf_k_,_systemetical_chunk_num_);
    free(recvData);

    return nEq;
}

int IA::recMatXORization2(){
    /*
     * Compare with recMatXORization(), we just assume we will use XOR scheduling
     */
    if(_rec_XOR_Schedules==NULL){
        _rec_XOR_Schedules=(int***)calloc(_survNum,sizeof(int**));
    }
    //show_matrix(_recovery_equations,_real_f*_conf_k_,_conf_f_*_survNum);
    for(int i=0;i<_survNum;i++){
        /*
         * tempMat contains the rec mat for just 1 encoded block
         */
        int* tempMat=(int*)calloc(_conf_f_*_real_f*_conf_k_,
                sizeof(int));
        for(int j=0;j<_real_f*_conf_k_;j++){
            memcpy((char*)tempMat+j*_conf_f_*sizeof(int),
                    (char*)_recovery_equations+(j*_survNum+i)*_conf_f_*sizeof(int),
                    _conf_f_*sizeof(int));
        }
        /*
         * generate schedule..
         */
        int* tempXORMat=jerasure_matrix_to_bitmatrix(_conf_f_,
                _real_f*_conf_k_,
                _conf_w_,
                tempMat);
        _rec_XOR_Schedules[i]=jerasure_smart_bitmatrix_to_schedule(_conf_f_,
                _real_f*_conf_k_,
                _conf_w_,
                tempXORMat);
        free(tempMat);
        free(tempXORMat);
    }
    return 0;
}

int IA::recMatXORization(){
    /*
     * generate XORized matrix and schedule for recovery equations
     */
    if(_XOR_flag==1){
        _XOR_recovery_equations=jerasure_matrix_to_bitmatrix(
                _conf_f_*_survNum,
                _real_f*_conf_k_,
                _conf_w_,
                _recovery_equations);
        if(_schedule_flag==1){
            _rec_XOR_Schedule=jerasure_smart_bitmatrix_to_schedule(
                    _conf_f_*_survNum,
                    _real_f*_conf_k_,
                    _conf_w_,
                    _XOR_recovery_equations);
        }
    }
    return 0;
}

int IA::test_validity(int num,int* list){
    this->set_f(num,list,3);
    if(_recovery_equations!=NULL){
        _vlist=(int*)calloc(num,sizeof(int));
        _vf=num;
        for(int i=0;i<_vf;i++){
            _vlist[i]=list[i];
        }
        return 0;
    }else{
        /*
         * Handle bad failure pattern
         * Generate a list containing unfailed nodes
         * TODO:Finish this function
         */
        int* unfailed=(int*)calloc(_conf_n_-_conf_f_,sizeof(int));
        int index=0;
        for(int i=0;i<_real_n;i++){
            if(is_failed(_rv_map[i])==0){
                unfailed[index]=_rv_map[i];
                index++;
            }
        }
        //for(int i=0;i<_conf_f_;i++){
        //    printf("%4d",_failed_node_list[i]);
        //}
        //printf("\n");
        //printf("unfailed:\n");
        //for(int i=0;i<index-1;i++){
        //    printf("%4d",unfailed[i]);
        //}
        //printf("\n");

        /*
         * Let us start from adding 1 extra node
         */
        for(int addNum=1;addNum<_real_n-num;addNum++){
            int fakeFailedNum=addNum+num;
            int* addIndex=(int*)calloc(addNum,sizeof(int));
            enumerate_init(_real_n-num,addNum,addIndex);
            do{
                /*
                 * addContent contains virtual failed nodes
                 */
                int* addContent=(int*)calloc(addNum,sizeof(int));
                for(int i=0;i<addNum;i++){
                    addContent[i]=unfailed[addIndex[i]];
                }
                //printf("addContent\n");
                //for(int i=0;i<addNum;i++){
                //    printf("%4d",addContent[i]);
                //}
                //printf("\n");

                /*
                 * Generate the faked failed list
                 * Using insert sort, the efficiency is bad.. but the
                 * fact is, the code here is executed with such a tiny
                 * probability. Plus the arrays contain so little elements.
                 */
                int* nList=(int*)calloc(fakeFailedNum,sizeof(int));
                for(int i=0;i<num;i++){
                    nList[i]=list[i];
                }
                for(int i=0;i<addNum;i++){
                    int marker=-1;
                    for(int j=0;j<num+i;j++){
                        if(nList[j]>addContent[i]){
                            marker=j;
                            break;
                        }
                    }
                    printf("content:%d\n",addContent[i]);
                    if(marker!=-1){
                        for(int j=num+i-1;j>marker-1;j--){
                            nList[j+1]=nList[j];
                        }
                        nList[marker]=addContent[i];
                    }else{
                        //for(int j=num+i-1;j>=0;j--){
                        //    nList[j+1]=nList[j];
                        //}
                        nList[num+i]=addContent[i];
                    }
                }
                printf("Virtual Failed Num %d AddNum:%d\n",fakeFailedNum,addNum);
                for(int i=0;i<fakeFailedNum;i++){
                    printf("%4d",nList[i]);
                }
                printf("\n");
                /*
                 * Test whether nList works fine
                 */
                this->set_f(fakeFailedNum,nList,3);
                if(_recovery_equations==NULL){
                    /*
                     * free stuffs and start over
                     */
                    free(addContent);
                    free(nList);
                }else{
                    /*
                     * free stuffs and return
                     */
                    free(addContent);
                    free(addIndex);
                    free(_recovery_equations);
                    _recovery_equations=NULL;
                    _vlist=nList;
                    _vf=fakeFailedNum;
                    for(int j=0;j<_vf;j++){
                        _vlist[j]=_inverse_rv_map[_vlist[j]];
                    }
                    return addNum;
                }
            }while(enumerate_next(_real_n-num,addNum,addIndex)==0);
        }
    }
    return -1;
}

int IA::encOlGenerator(){
    _enc_offline_mat=(int*)calloc(_conf_f_*_conf_k_,sizeof(int));
    for(int i=0;i<_conf_f_;i++){
        memcpy((char*)_enc_offline_mat+i*_conf_k_*sizeof(int),
                (char*)_offline_enc_vec+_failed_node_list[i]*_conf_k_*sizeof(int),
                _conf_k_*sizeof(int));
    }
    if(_XOR_flag==1){
        _XOR_enc_offline_mat=jerasure_matrix_to_bitmatrix(
                _conf_k_,
                _conf_f_,
                _conf_w_,
                _enc_offline_mat);
        if(_schedule_flag==1){
            _enc_ol_XOR_Schedule=jerasure_smart_bitmatrix_to_schedule(
                    _conf_k_,
                    _conf_f_,
                    _conf_w_,
                    _XOR_enc_offline_mat);
        }
    }
    return 0;
}

int* IA::reEqReorg(int* reEq){
    if(reEq==NULL){
        return NULL;
    }
    int* survList=(int*)calloc(_conf_n_-_conf_f_,sizeof(int));
    int survIndex=0;
    for(int i=0;i<_conf_f_;i++){
        if(is_failed(i)==0){
            survList[survIndex]=i;
            survIndex++;
        }
    }
    int* nEq=(int*)calloc(_conf_f_*_conf_k_*(_conf_n_-_conf_f_)*_conf_f_,
            sizeof(int));
    for(int i=0;i<_conf_f_;i++){
        int toNode=_failed_node_list[i];
        int fromIndex=0;
        for(int j=0;j<toNode;j++){
            if(is_failed(j)==1){
                continue;
            }
            /*
             * From the i*(_conf_n_-1)+j column in old equations to
             * fromIndex*_conf_f_+i column in new equations
             */
            for(int k=0;k<_conf_f_*_conf_k_;k++){
                nEq[k*(_conf_n_-_conf_f_)*_conf_f_+fromIndex*_conf_f_+i]
                    =reEq[k*(_conf_n_-1)*_conf_f_+i*(_conf_n_-1)+j];
            }
            fromIndex++;
        }
        for(int j=toNode+1;j<_conf_n_;j++){
            if(is_failed(j)==1){
                continue;
            }
            /*
             * From the i*(_conf_n_-1)+j-1 column in old equations to
             * fromIndex*_conf_f_+i column in new equations
             */
            for(int k=0;k<_conf_f_*_conf_k_;k++){
                nEq[k*(_conf_n_-_conf_f_)*_conf_f_+fromIndex*_conf_f_+i]
                    =reEq[k*(_conf_n_-1)*_conf_f_+i*(_conf_n_-1)+j-1];
            }
            fromIndex++;
        }
    }
    free(survList);
    _survNum=_conf_n_-_conf_f_;
    //show_matrix(nEq,_conf_f_*_conf_k_,_conf_f_*(_conf_n_-_conf_f_));

    /*
     * TODO: remove the columns corresponding to the virtual node
     */
    if(_real_n!=_conf_n_){
        _survNum=0;
        int* realList=(int*)calloc(_real_n,sizeof(int));
        int* rsMap=(int*)calloc(_real_n,sizeof(int));
        for(int i=0;i<_conf_n_;i++){
            if(is_failed(i)==1){
                continue;
            }else{
                int marker=0;
                for(int j=0;j<_conf_n_-_real_n;j++){
                    if(_virtual_node[j]==i){
                        marker=1;
                        break;
                    }
                }
                if(marker==0){
                    /*
                     * Meaning this is a real & surviving node
                     */
                    int counter=0;
                    for(int j=0;j<_conf_f_;j++){
                        if(_failed_node_list[j]<i){
                            counter++;
                        }
                    }
                    realList[_survNum]=i;
                    rsMap[_survNum]=i-counter;
                    _survNum++;
                }
            }
        }
        int* temp=(int*)calloc(_conf_f_*_conf_k_*_survNum*_conf_f_,sizeof(int));
        for(int i=0;i<_survNum;i++){
            //int marker=0;
            //for(int j=0;j<_survNum;j++){
            //    if(realList[j]==i){
            //        marker=1;
            //        break;
            //    }
            //}
            //if(marker==1){
            //    continue;
            //}

            /*
             * Copy corresponding columns from nEq to temp
             */
            for(int j=0;j<_conf_f_;j++){
                for(int k=0;k<_conf_f_*_conf_k_;k++){
                    temp[k*_survNum*_conf_f_+(i*_conf_f_+j)]
                        =nEq[k*(_conf_n_-_conf_f_)*_conf_f_
                            +(rsMap[i]*_conf_f_+j)];
                }
            }
        }
        //show_matrix(nEq,_conf_f_*_conf_k_,_conf_f_*(_conf_n_-_conf_f_));
        free(nEq);
        free(realList);
        free(rsMap);
        nEq=temp;
        //show_matrix(nEq,_conf_f_*_conf_k_,_conf_f_*_survNum);
    }
    return nEq;
}

int IA::encode2(char* inBuffer,char* outBuffer,int fullSize){
    int rawSize=fullSize/(_real_k*_conf_k_);
    if(_XOR_flag==1){
        if(_schedule_flag==1){
            //show_shedule(_enc_XOR_Schedule);
            //printf("%d %d %d %d\n",_conf_k_,_real_k,rawSize,_conf_w_);
            buffer_cal3(outBuffer,inBuffer,
                    _enc_XOR_Schedule,
                    _conf_k_*_conf_k_,
                    _real_k*_conf_k_,
                    rawSize/_conf_w_);
            //show_shedule(_enc_XOR_Schedule);
        }else{
            buffer_cal2(outBuffer,inBuffer,
                    _XOR_ori_encoding_matrix,
                    _conf_k_*_conf_k_*_conf_w_,
                    _real_k*_conf_k_*_conf_w_,
                    rawSize/_conf_w_);
        }
    }else{
        buffer_cal1(outBuffer,inBuffer,
                _final_enc_matrix,
                _conf_k_*_conf_k_,
                _real_k*_conf_k_,
                rawSize);
    }
    return 0;
}

int IA::encode_offline_recovery2(char* inBuffer,char* outBuffer,int stripSize){
    int rawSize=stripSize/_chunk_number_per_node_;
    if(_XOR_flag==1){
        if(_schedule_flag==1){
            buffer_cal3(outBuffer,inBuffer,
                    _enc_ol_XOR_Schedule,
                    _conf_f_,
                    _conf_k_,
                    rawSize/_conf_w_);
        }else{
            buffer_cal2(outBuffer,inBuffer,
                    _XOR_enc_offline_mat,
                    (_conf_f_)*_conf_w_,
                    _conf_k_*_conf_w_,
                    rawSize/_conf_w_);
        }
    }else{
        buffer_cal1(outBuffer,inBuffer,
                _enc_offline_mat,
                _conf_f_,
                _conf_k_,
                rawSize);
    }
    return 0;
}

int IA::reconstruct_lost_data3(char* inBuffer,char* outBuffer,int stripSize,int index){
    int rawSize=stripSize/_chunk_number_per_node_/_conf_w_;
    buffer_cal3(outBuffer,inBuffer,
            _rec_XOR_Schedules[index],
            _real_f*_conf_k_,
            _conf_f_,
            rawSize);
    return 0;
}

int IA::reconstruct_lost_data2(char* inBuffer,char* outBuffer,int stripSize){
    int rawSize=stripSize/_chunk_number_per_node_;
    //puts("BEFORE");
    //int index=0;
    //while(_rec_XOR_Schedule[index][0]!=-1){
    //    printf("%d %d %d %d %d\n",
    //            _rec_XOR_Schedule[index][0],
    //            _rec_XOR_Schedule[index][1],
    //            _rec_XOR_Schedule[index][2],
    //            _rec_XOR_Schedule[index][3],
    //            _rec_XOR_Schedule[index][4]);
    //    printf("%d: %x\n",index,_rec_XOR_Schedule[index]);
    //    index++;
    //}
    if(_XOR_flag==1){
        if(_schedule_flag==1){
            buffer_cal3(outBuffer,inBuffer,
                    _rec_XOR_Schedule,
                    _real_f*_conf_k_,
                    _conf_f_*_survNum,
                    rawSize/_conf_w_);
        }else{
            buffer_cal2(outBuffer,inBuffer,
                    _XOR_recovery_equations,
                    _real_f*_conf_k_*_conf_w_,
                    _conf_f_*_survNum*_conf_w_,
                    rawSize/_conf_w_);
        }
    }else{
        buffer_cal1(outBuffer,inBuffer,
                _recovery_equations,
                _real_f*_conf_k_,
                _conf_f_*_survNum,
                rawSize);
    }
    //puts("AFTER");
    //index=0;
    ////printf("%d %d\n",_conf_f_*_survNum,_conf_f_*_conf_k_);
    //while(_rec_XOR_Schedule[index][0]!=-1){
    //    printf("%d %d %d %d %d\n",
    //            _rec_XOR_Schedule[index][0],
    //            _rec_XOR_Schedule[index][1],
    //            _rec_XOR_Schedule[index][2],
    //            _rec_XOR_Schedule[index][3],
    //            _rec_XOR_Schedule[index][4]);
    //    printf("%d: %x\n",index,_rec_XOR_Schedule[index]);
    //    index++;
    //}
    return 0;
}

int IA::set_f_nocal(int num,int* list,int mode){
    _conf_f_=num;
    return 0;
}

int IA::set_f(int num,int* list,int mode){
    /*
     * For different modes:
     * 0. Generate everything
     * 1. Relayer side
     * 2. DataNode side
     * 3. Only Generate non-XORed _recMat (for test_validity())
     */
    _conf_f_=num;
    _user_fail_list=list;
    _failed_node_list=(int*)calloc(num,sizeof(int));
    for(int i=0;i<_conf_f_;i++){
        _failed_node_list[i]=_rv_map[_user_fail_list[i]];
    }

    /*
     * Create _enc_offline_mat and _enc_ol_XOR_Schedule
     */
    if((mode==0)||(mode==2)){
        encOlGenerator();
    }
    //printf("XORed encode offline matrix:\n");
    //show_matrix(_XOR_enc_offline_mat,_conf_f_*_conf_w_,_conf_k_*_conf_w_);
    
    /*
     * Generate _recovery_equations and _rec_XOR_Schedule
     */
    if((mode==0)||(mode==1)||(mode==3)){
        this->multi_node_repair();
    }
    if((mode==0)||(mode==1)){
        recMatXORization();
    }
    //printf("XORed rec equations:\n");
    //show_matrix(_XOR_recovery_equations,_conf_f_*_conf_k_*_conf_w_,
    //        _conf_f_*_survNum*_conf_w_);
    return 0;
}

/*
 * This is for bad failure patterns and degraded read operations.
 * We still generate recovery equations for failure pattern of
 * _failed_node_list, but we only regenerate the data for 
 * _real_list.
 */

int IA::partializeRE(){
    int* temp=(int*)calloc(_survNum*_conf_f_*_conf_f_*_conf_k_,
            sizeof(int));
    int marker=0;
    show_matrix(_recovery_equations,_conf_f_*_conf_k_,_survNum*_conf_f_);
    for(int i=0;i<_real_f;i++){
        for(int j=marker;j<_conf_f_;j++){
            if(_failed_node_list[j]==_real_list[i]){
                printf("%d %d\n",i,j);
                memcpy((char*)temp+
                            i*_survNum*_conf_f_*_conf_k_*sizeof(int),
                        (char*)_recovery_equations+
                            j*_survNum*_conf_f_*_conf_k_*sizeof(int),
                        _survNum*_conf_f_*_conf_k_*sizeof(int));
            }
            marker=j+1;
            break;
        }
    }
    free(_recovery_equations);
    _recovery_equations=temp;
    show_matrix(_recovery_equations,_conf_f_*_conf_k_,_survNum*_conf_f_);
    return 0;
}

int IA::set_f2_nocal(int num,int* list,int rnum,int*rlist,int mode){
    _conf_f_=num;
    _real_f=rnum;
    return 0;
}

int IA::set_f2(int num,int* list,int rnum,int*rlist,int mode){
    /*
     * For different modes:
     * 0. Generate everything
     * 1. Relayer side
     * 2. DataNode side
     * 3. Only Generate non-XORed _recMat (for test_validity())
     *
     * We still assume the user know the rules of IA code.
     */
    _conf_f_=num;
    _user_fail_list=list;
    _failed_node_list=(int*)calloc(num,sizeof(int));
    for(int i=0;i<_conf_f_;i++){
        _failed_node_list[i]=_rv_map[_user_fail_list[i]];
    }

    _real_f=rnum;
    _real_list=(int*)calloc(_real_f,sizeof(int));
    for(int i=0;i<_real_f;i++){
        _real_list[i]=_rv_map[_user_fail_list[i]];
    }

    /*
     * Create _enc_offline_mat and _enc_ol_XOR_Schedule
     */
    if((mode==0)||(mode==2)){
        encOlGenerator();
    }
    //printf("XORed encode offline matrix:\n");
    //show_matrix(_XOR_enc_offline_mat,_conf_f_*_conf_w_,_conf_k_*_conf_w_);
    
    /*
     * Generate _recovery_equations and _rec_XOR_Schedule
     */
    if((mode==0)||(mode==1)||(mode==3)){
        this->multi_node_repair();
    }

    if(_real_f!=_conf_f_){
        /*
         * partialize the recovery equations
         */
        partializeRE();
    }

    if((mode==0)||(mode==1)){
        recMatXORization();
    }
    //printf("XORed rec equations:\n");
    //show_matrix(_XOR_recovery_equations,_conf_f_*_conf_k_*_conf_w_,
    //        _conf_f_*_survNum*_conf_w_);
    return 0;
}

int IA::set_f3(int num,int* list,int rnum,int*rlist,int mode){
    /*
     * FOR parallel decoding
     * For different modes:
     * 0. Generate everything
     * 1. Relayer side
     * 2. DataNode side
     * 3. Only Generate non-XORed _recMat (for test_validity())
     *
     * We still assume the user know the rules of IA code.
     */
    _conf_f_=num;
    _user_fail_list=list;
    _failed_node_list=(int*)calloc(num,sizeof(int));
    for(int i=0;i<_conf_f_;i++){
        _failed_node_list[i]=_rv_map[_user_fail_list[i]];
    }

    _real_f=rnum;
    _real_list=(int*)calloc(_real_f,sizeof(int));
    for(int i=0;i<_real_f;i++){
        _real_list[i]=_rv_map[_user_fail_list[i]];
    }

    /*
     * Create _enc_offline_mat and _enc_ol_XOR_Schedule
     */
    if((mode==0)||(mode==2)){
        encOlGenerator();
    }
    //printf("XORed encode offline matrix:\n");
    //show_matrix(_XOR_enc_offline_mat,_conf_f_*_conf_w_,_conf_k_*_conf_w_);
    
    /*
     * Generate _recovery_equations and _rec_XOR_Schedule
     */
    if((mode==0)||(mode==1)||(mode==3)){
        this->multi_node_repair();
    }

    if(_real_f!=_conf_f_){
        /*
         * partialize the recovery equations
         */
        partializeRE();
    }

    if((mode==0)||(mode==1)){
        recMatXORization2();
    }
    //printf("XORed rec equations:\n");
    //show_matrix(_XOR_recovery_equations,_conf_f_*_conf_k_*_conf_w_,
    //        _conf_f_*_survNum*_conf_w_);
    return 0;
}

/*
 * backUpEverything() and restoreEverything() are two functions which avoids 
 * redundant calculation
 */
int IA::backUpEverything(char* buffer){
    /*
     * First the content in base class Coding, then the variables in IA
     */
    memcpy(buffer+ 0*sizeof(char*),(char*)&_failed_node_list,        sizeof(char*));
    memcpy(buffer+ 1*sizeof(char*),(char*)&_inverse_table,           sizeof(char*));
    memcpy(buffer+ 2*sizeof(char*),(char*)&_recovery_equations,      sizeof(char*));
    memcpy(buffer+ 3*sizeof(char*),(char*)&_ori_encoding_matrix,     sizeof(char*));
    memcpy(buffer+ 4*sizeof(char*),(char*)&_dual_enc_matrix,         sizeof(char*));
    memcpy(buffer+ 5*sizeof(char*),(char*)&_final_enc_matrix,        sizeof(char*));
    memcpy(buffer+ 6*sizeof(char*),(char*)&_offline_enc_vec,         sizeof(char*));
    memcpy(buffer+ 7*sizeof(char*),(char*)&_XOR_ori_encoding_matrix, sizeof(char*));
    memcpy(buffer+ 8*sizeof(char*),(char*)&_enc_XOR_Schedule,        sizeof(char*));
    memcpy(buffer+ 9*sizeof(char*),(char*)&_XOR_recovery_equations,  sizeof(char*));
    memcpy(buffer+10*sizeof(char*),(char*)&_rec_XOR_Schedule,        sizeof(char*));
    memcpy(buffer+11*sizeof(char*),(char*)&_enc_offline_mat,         sizeof(char*));
    memcpy(buffer+12*sizeof(char*),(char*)&_XOR_enc_offline_mat,     sizeof(char*));
    memcpy(buffer+13*sizeof(char*),(char*)&_enc_ol_XOR_Schedule,     sizeof(char*));
    memcpy(buffer+14*sizeof(char*),(char*)&_user_fail_list,          sizeof(char*));
    memcpy(buffer+15*sizeof(char*),(char*)&_virtual_node,            sizeof(char*));
    memcpy(buffer+16*sizeof(char*),(char*)&_rv_map,                  sizeof(char*));
    memcpy(buffer+17*sizeof(char*),(char*)&_inverse_rv_map,          sizeof(char*));
    memcpy(buffer+18*sizeof(char*),(char*)&_real_list,               sizeof(char*));
    memcpy(buffer+19*sizeof(char*),(char*)&_vlist,                   sizeof(char*));
    memcpy(buffer+20*sizeof(char*),(char*)&_user_vlist,              sizeof(char*));
    memcpy(buffer+21*sizeof(char*),(char*)&_survNum,                 sizeof(char*));
    memcpy(buffer+22*sizeof(char*),(char*)&_rec_XOR_Schedules,       sizeof(char*));
    return 0;
}

int IA::restoreEverything(char* buffer){
    /*
     * First the content in base class Coding, then the variables in IA
     */
    memcpy((char*)&_failed_node_list,        buffer+ 0*sizeof(char*),sizeof(char*));
    memcpy((char*)&_inverse_table,           buffer+ 1*sizeof(char*),sizeof(char*));
    memcpy((char*)&_recovery_equations,      buffer+ 2*sizeof(char*),sizeof(char*));
    memcpy((char*)&_ori_encoding_matrix,     buffer+ 3*sizeof(char*),sizeof(char*));
    memcpy((char*)&_dual_enc_matrix,         buffer+ 4*sizeof(char*),sizeof(char*));
    memcpy((char*)&_final_enc_matrix,        buffer+ 5*sizeof(char*),sizeof(char*));
    memcpy((char*)&_offline_enc_vec,         buffer+ 6*sizeof(char*),sizeof(char*));
    memcpy((char*)&_XOR_ori_encoding_matrix, buffer+ 7*sizeof(char*),sizeof(char*));
    memcpy((char*)&_enc_XOR_Schedule,        buffer+ 8*sizeof(char*),sizeof(char*));
    memcpy((char*)&_XOR_recovery_equations,  buffer+ 9*sizeof(char*),sizeof(char*));
    memcpy((char*)&_rec_XOR_Schedule,        buffer+10*sizeof(char*),sizeof(char*));
    memcpy((char*)&_enc_offline_mat,         buffer+11*sizeof(char*),sizeof(char*));
    memcpy((char*)&_XOR_enc_offline_mat,     buffer+12*sizeof(char*),sizeof(char*));
    memcpy((char*)&_enc_ol_XOR_Schedule,     buffer+13*sizeof(char*),sizeof(char*));
    memcpy((char*)&_user_fail_list,          buffer+14*sizeof(char*),sizeof(char*));
    memcpy((char*)&_virtual_node,            buffer+15*sizeof(char*),sizeof(char*));
    memcpy((char*)&_rv_map,                  buffer+16*sizeof(char*),sizeof(char*));
    memcpy((char*)&_inverse_rv_map,          buffer+17*sizeof(char*),sizeof(char*));
    memcpy((char*)&_real_list,               buffer+18*sizeof(char*),sizeof(char*));
    memcpy((char*)&_vlist,                   buffer+19*sizeof(char*),sizeof(char*));
    memcpy((char*)&_user_vlist,              buffer+20*sizeof(char*),sizeof(char*));
    memcpy((char*)&_survNum,                 buffer+21*sizeof(char*),sizeof(char*));
    memcpy((char*)&_rec_XOR_Schedules,       buffer+22*sizeof(char*),sizeof(char*));
    return 0;
}

int IA::cleanup(){
    if(_failed_node_list!=NULL){
        free(_failed_node_list);
        _failed_node_list=NULL;
    }
    if(_recovery_equations!=NULL){
        free(_recovery_equations);
        _recovery_equations=NULL;
    }
    if(_XOR_recovery_equations!=NULL){
        free(_XOR_recovery_equations);
        _XOR_recovery_equations==NULL;
    }
    if(_rec_XOR_Schedule!=NULL){
        //jerasure_free_schedule(_rec_XOR_Schedule);
        //int index=0;
        //while(_rec_XOR_Schedule[index][0]!=-1){
        //    printf("%4d%4d%4d%4d%4d\n",
        //            _rec_XOR_Schedule[index][0],
        //            _rec_XOR_Schedule[index][1],
        //            _rec_XOR_Schedule[index][2],
        //            _rec_XOR_Schedule[index][3],
        //            _rec_XOR_Schedule[index][4]
        //            );
        //    free(_rec_XOR_Schedule[index]);
        //    index++;
        //}
        //free(_rec_XOR_Schedule[index]);
        //free(_rec_XOR_Schedule);
        jerasure_free_schedule(_rec_XOR_Schedule);
        _rec_XOR_Schedule==NULL;
    }
    if(_rec_XOR_Schedules!=NULL){
        for(int i=0;i<_survNum;i++){
            jerasure_free_schedule(_rec_XOR_Schedules[i]);
        }
        free(_rec_XOR_Schedules);
    }
    if(_enc_offline_mat!=NULL){
        free(_enc_offline_mat);
        _enc_offline_mat=NULL;
    }
    if(_XOR_enc_offline_mat!=NULL){
        free(_XOR_enc_offline_mat);
        _XOR_enc_offline_mat=NULL;
    }
    if(_enc_ol_XOR_Schedule!=NULL){
        jerasure_free_schedule(_enc_ol_XOR_Schedule);
        _enc_ol_XOR_Schedule=NULL;
    }
    if(_user_fail_list!=NULL){
        free(_user_fail_list);
        _user_fail_list=NULL;
    }
    if(_real_list!=NULL){
        free(_real_list);
        _real_list=NULL;
    }
    if(_vlist!=NULL){
        free(_vlist);
        _vlist=NULL;
    }
    if(_user_vlist!=NULL){
        free(_user_vlist);
        _user_vlist=NULL;
    }

    /*
     * This is something I am not sure about, whether to free the encode metadata
     * or not...
     * Just put the commented code here for furture use.
     */
    //if(_ori_encoding_matrix!=NULL){
    //    free(_ori_encoding_matrix);
    //    _ori_encoding_matrix=NULL;
    //}
    //if(_dual_enc_matrix!=NULL){
    //    free(_dual_enc_matrix);
    //    _dual_enc_matrix=NULL;
    //}
    //if((_final_enc_matrix!=NULL)&&(_real_k!=_conf_k_)){
    //    free(_final_enc_matrix);
    //    _final_enc_matrix=NULL;
    //}
    //if(_offline_enc_vec!=NULL){
    //    free(_offline_enc_vec);
    //    _offline_enc_vec=NULL;
    //}
    //if(_XOR_ori_encoding_matrix!=NULL){
    //    free(_XOR_ori_encoding_matrix);
    //    _XOR_ori_encoding_matrix=NULL;
    //}
    //if(_enc_XOR_Schedule!=NULL){
    //    jerasure_free_schedule(_enc_XOR_Schedule);
    //    _enc_XOR_Schedule=NULL;
    //}
    //if(_inverse_table!=NULL){
    //    free(_inverse_table);
    //    _inverse_table=NULL;
    //}
    return 0;
}
















