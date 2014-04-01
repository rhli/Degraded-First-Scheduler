# include "RS.hh"

//extern int XOR_buffer(char*,char*,int);

RS::RS(int n,int k,int w){
	_conf_n_=n;
	_conf_k_=k;
	_conf_w_=w;
	_inverse_table=NULL;
	_encoding_schedule=NULL;
	_ori_encoding_matrix=NULL;
	_XORed_ori_encoding_matrix=NULL;
	_recovery_equations=NULL;
	_XORed_recovery_equations=NULL;
	_recovery_schedule=NULL;
	//_degraded_read_schedule=NULL;
	_conf_n_=n;
	_conf_k_=k;
	_conf_w_=w;
	_chunk_number_per_node_=_conf_k_-1;
	_systemetical_chunk_num_=_conf_k_*(_conf_k_-1);
	_encoded_chunk_num_=(_conf_n_-_conf_k_)*(_conf_k_-1);
	_total_chunk_num_=_systemetical_chunk_num_+_encoded_chunk_num_;
	_XOR_flag=1;
	_schedule_flag=1;
	_SSE_flag=1;
	_systemetical_chunk_num_=_conf_k_;
	_encoded_chunk_num_=_conf_n_-_conf_k_;
	_total_chunk_num_=_conf_n_;
	_chunk_number_per_node_=1;
	generate_inverse_table();
}

int RS::restoreEverything(char* buffer){
	memcpy((char*)&_inverse_table,			buffer+0*sizeof(char*),sizeof(char*));
	memcpy((char*)&_ori_encoding_matrix,		buffer+1*sizeof(char*),sizeof(char*));
	memcpy((char*)&_XORed_ori_encoding_matrix,	buffer+2*sizeof(char*),sizeof(char*));
	memcpy((char*)&_recovery_equations,		buffer+3*sizeof(char*),sizeof(char*));
	memcpy((char*)&_XORed_recovery_equations,	buffer+4*sizeof(char*),sizeof(char*));
	memcpy((char*)&_encoding_schedule,		buffer+5*sizeof(char*),sizeof(char*));
	memcpy((char*)&_recovery_schedule,		buffer+6*sizeof(char*),sizeof(char*));
	return 0;
}

int RS::backUpEverything(char* buffer){
	memcpy(buffer+0*sizeof(char*),(char*)&_inverse_table,sizeof(char*));
	memcpy(buffer+1*sizeof(char*),(char*)&_ori_encoding_matrix,sizeof(char*));
	memcpy(buffer+2*sizeof(char*),(char*)&_XORed_ori_encoding_matrix,sizeof(char*));
	memcpy(buffer+3*sizeof(char*),(char*)&_recovery_equations,sizeof(char*));
	memcpy(buffer+4*sizeof(char*),(char*)&_XORed_recovery_equations,sizeof(char*));
	memcpy(buffer+5*sizeof(char*),(char*)&_encoding_schedule,sizeof(char*));
	memcpy(buffer+6*sizeof(char*),(char*)&_recovery_schedule,sizeof(char*));
	return 0;
}

int RS::cleanup(){
	if(_inverse_table!=NULL){
		free(_inverse_table);
		_inverse_table=NULL;
	}
	if(_ori_encoding_matrix!=NULL){
		free(_ori_encoding_matrix);
		_ori_encoding_matrix=NULL;
	}
	if(_XORed_ori_encoding_matrix!=NULL){
		free(_XORed_ori_encoding_matrix);
		_XORed_ori_encoding_matrix=NULL;
	}
	if(_recovery_equations!=NULL){
		free(_recovery_equations);
		_recovery_equations=NULL;
	}
	if(_XORed_recovery_equations!=NULL){
		free(_XORed_recovery_equations);
		_XORed_recovery_equations=NULL;
	}
	if(_encoding_schedule!=NULL){
		jerasure_free_schedule(_encoding_schedule);
		_encoding_schedule=NULL;
	}
	if(_recovery_schedule!=NULL){
		jerasure_free_schedule(_recovery_schedule);
		_recovery_schedule=NULL;
	}
}

int RS::generate_encoding_matrix(){
	int* encoded=reed_sol_vandermonde_coding_matrix(_conf_k_,_conf_n_-_conf_k_,_conf_w_);
	_ori_encoding_matrix=(int*)calloc(_conf_k_*_conf_n_,sizeof(int));
	for(int i=0;i<_conf_k_;i++){
		_ori_encoding_matrix[i*_conf_k_+i]=1;
	}
	memcpy((char*)_ori_encoding_matrix+_conf_k_*_conf_k_*sizeof(int),
			(char*)encoded,
			_conf_k_*(_conf_n_-_conf_k_)*sizeof(int));
	if(_XOR_flag==1){
		//Generate XORed encoding matrix
		_XORed_systemetical_chunk_num_=_conf_w_*_systemetical_chunk_num_;
		_XORed_encoded_chunk_num_=_conf_w_*_encoded_chunk_num_;
		_XORed_total_chunk_num_=_conf_w_*_total_chunk_num_;
		_XORed_chunk_number_per_node_=_conf_w_*_chunk_number_per_node_;
		_XORed_ori_encoding_matrix=
			jerasure_matrix_to_bitmatrix(_conf_k_,_conf_n_,_conf_w_,
					_ori_encoding_matrix);
		if(_schedule_flag==1){
			//generate encoding schedule
			_encoding_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_k_,_conf_n_-_conf_k_,
					_conf_w_,_XORed_ori_encoding_matrix+
					_XORed_systemetical_chunk_num_*_XORed_systemetical_chunk_num_);
		}
	}
	free(encoded);
	return 0;
}

int* RS::failed_node_repair(){
	int* survivers=(int *)calloc(_conf_k_*_conf_k_,sizeof(int));
	//pick the first unfailed nodes
	int picked=0;
	for(int i=0;i<_conf_n_;i++){
		if(is_failed(i)==0){
			memcpy((char*)survivers+picked*_conf_k_*sizeof(int),
					(char*)_ori_encoding_matrix+i*_conf_k_*sizeof(int),
					_conf_k_*sizeof(int));
			picked++;
		}
		if(picked==_conf_k_){
			break;
		}
	}
	int* des_mat=(int*)calloc(_conf_k_*_conf_k_,sizeof(int));
	inverse_matrix(survivers,des_mat,_conf_k_);
	int* failed_nodes=(int*)calloc(_conf_f_*_conf_k_,sizeof(int));
	for(int i=0;i<_conf_f_;i++){
		memcpy((char*)failed_nodes+i*_conf_k_*sizeof(int),
				(char*)_ori_encoding_matrix+_failed_node_list[i]*_conf_k_*sizeof(int),
				_conf_k_*sizeof(int));
	}
	_recovery_equations=matrix_multiply2(failed_nodes,des_mat,_conf_f_,_conf_k_,_conf_k_);
	return _recovery_equations;
}

int RS::encode2(char* in_buffer,char* out_buffer,int length){
	if(_XOR_flag!=1){
		int base=_systemetical_chunk_num_*_systemetical_chunk_num_;
		int chunk_size=length/_systemetical_chunk_num_;
		for(int i=0;i<_encoded_chunk_num_;i++){
			char* target=out_buffer+i*chunk_size;
			for(int j=0;j<_systemetical_chunk_num_;j++){
			    if(_SSE_flag==0){
				galois_w08_region_multiply(in_buffer+j*chunk_size,
						_ori_encoding_matrix[base+i*_systemetical_chunk_num_+j],
						chunk_size,
						target,
						1);
			    }else{
				ff_add_mulv_local((uint8_t*)target,
					(uint8_t*)in_buffer+j*chunk_size,
					_ori_encoding_matrix[base+i*_systemetical_chunk_num_+j],
					chunk_size);
			    }
			}
		}
		return 0;
	}else if(_schedule_flag!=1){
		//JUST XOR
		int base=_XORed_systemetical_chunk_num_*_XORed_systemetical_chunk_num_;
		int chunk_size=length/_XORed_systemetical_chunk_num_;
		for(int i=0;i<_XORed_encoded_chunk_num_;i++){
			char* target=out_buffer+i*chunk_size;
			for(int j=0;j<_XORed_systemetical_chunk_num_;j++){
				if(_XORed_ori_encoding_matrix[base+i*_XORed_systemetical_chunk_num_+j]==1){
					XOR_buffer(target,in_buffer+j*chunk_size,chunk_size);
				}
			}
		}
		return 0;
	}else{
		//XOR Scheduling
		int index=0;
		int chunk_size=length/_XORed_systemetical_chunk_num_;
		while(_encoding_schedule[index][0]!=-1){
			if(_encoding_schedule[index][0]<_conf_k_){
				XOR_buffer(out_buffer+
						((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_
						+_encoding_schedule[index][3])*chunk_size,
					in_buffer+(_encoding_schedule[index][0]*_XORed_chunk_number_per_node_
						+_encoding_schedule[index][1])*chunk_size,
					chunk_size);
			}else{
				XOR_buffer(out_buffer+
						((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_
						+_encoding_schedule[index][3])*chunk_size,
					out_buffer+((_encoding_schedule[index][0]-_conf_k_)*_XORed_chunk_number_per_node_
						+_encoding_schedule[index][1])*chunk_size,
					chunk_size);
			}
			index++;
		}
		return 0;
	}
	return 0;
}

int RS::set_f(int number,int* list){
	_conf_f_=number;
	_failed_node_list=list;
	this->failed_node_repair();
	if(_XOR_flag==1){
		_XORed_recovery_equations=jerasure_matrix_to_bitmatrix(_conf_k_,_conf_f_,_conf_w_,
				_recovery_equations);
		if(_schedule_flag==1){
			_recovery_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_k_,_conf_f_,_conf_w_,
					_XORed_recovery_equations);
		}
	}
	return 0;
}

int RS::set_f2_nocal(int vnumber,int* vlist,int number,int* list){
	_virtual_failed_num=vnumber;
	_virtual_failed_list=vlist;
	_conf_f_=number;
	_failed_node_list=list;
	return 0;
}

int RS::set_f2(int vnumber,int* vlist,int number,int* list){
	_virtual_failed_num=vnumber;
	_virtual_failed_list=vlist;
	_conf_f_=vnumber;
	_failed_node_list=vlist;
	this->failed_node_repair();
	_conf_f_=number;
	_failed_node_list=list;
	int index=0;
	int* temp=(int*)calloc(_conf_k_*_conf_f_,sizeof(int));
	for(int i=0;i<vnumber;i++){
		int failed_flag=0;
		for(int j=0;j<number;j++){
			if(vlist[i]==list[j]){
				memcpy((char*)temp+index*_conf_k_*sizeof(int),
						(char*)_recovery_equations+i*_conf_k_*sizeof(int),
						_conf_k_*sizeof(int));
				index++;
				break;
			}
		}
	}
	free(_recovery_equations);
	_recovery_equations=temp;
	if(_XOR_flag==1){
		_XORed_recovery_equations=jerasure_matrix_to_bitmatrix(_conf_k_,_conf_f_,_conf_w_,
				_recovery_equations);
		if(_schedule_flag==1){
			_recovery_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_k_,_conf_f_,_conf_w_,
					_XORed_recovery_equations);
		}
	}
	return 0;
}

int RS::reconstruct_lost_data2(char* in_buffer,char* out_buffer,int length){
	if(_XOR_flag!=1){
		int chunk_size=length/_chunk_number_per_node_;
		for(int i=0;i<_conf_f_*_chunk_number_per_node_;i++){
			char* target=out_buffer+i*chunk_size;
			for(int j=0;j<_conf_k_*_chunk_number_per_node_;j++){
			    if(_SSE_flag==0){
				galois_w08_region_multiply(in_buffer+j*chunk_size,
						_recovery_equations[i*_conf_k_*_chunk_number_per_node_+j],
						chunk_size,
						target,
						1);
			    }else{
				ff_add_mulv_local((uint8_t*)target,
					(uint8_t*)in_buffer+j*chunk_size,
					_recovery_equations[i*_conf_k_*_chunk_number_per_node_+j],
					chunk_size);
			    }
			}
		}
		return 0;
	}else if(_schedule_flag!=1){
		int chunk_size=length/_XORed_chunk_number_per_node_;
		for(int i=0;i<_conf_f_*_XORed_chunk_number_per_node_;i++){
			char* target=out_buffer+i*chunk_size;
			for(int j=0;j<_XORed_systemetical_chunk_num_;j++){
				if(_XORed_recovery_equations[i*_XORed_systemetical_chunk_num_+j]==1){
					XOR_buffer(target,in_buffer+j*chunk_size,chunk_size);
				}
			}
		}
		return 0;
	}else{
		int index=0;
		int chunk_size=length/_XORed_chunk_number_per_node_;
		while(_recovery_schedule[index][0]!=-1){
			if(_recovery_schedule[index][0]<_conf_k_){
				XOR_buffer(out_buffer+
						((_recovery_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_
						+_recovery_schedule[index][3])*chunk_size,
					in_buffer+(_recovery_schedule[index][0]*_XORed_chunk_number_per_node_
						+_recovery_schedule[index][1])*chunk_size,
					chunk_size);
			}else{
				XOR_buffer(out_buffer+
						((_recovery_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_
						+_recovery_schedule[index][3])*chunk_size,
					out_buffer+((_recovery_schedule[index][0]-_conf_k_)*_XORed_chunk_number_per_node_
						+_recovery_schedule[index][1])*chunk_size,
					chunk_size);
			}
			index++;
		}
		return 0;
	}
	return 0;
}
