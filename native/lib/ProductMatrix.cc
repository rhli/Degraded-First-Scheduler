/* VERSION 0.1
 * Now this is a systematical version
 * hopefully, it will be a XORized version after several days...(Apr 17th, 2012)
 * Implemented by LI Runhui from CUHK
 * I just implement the most simple one of the Product Matrix paper. 
 * Just the MSR point, because we are more interested in the MSR side. 
 * d=2k-2
 * alpha=k-1 ...
 * I wanna make a library, which can produce the encoding matrix
 * decoding scheme and so on
 * Most importantly, I wanna support multi-node failure, exact repair, I mean. Hope
 * this is a meaningful work. 
 * Further versions will cover: 
 * Systematic code,
 * d is larger than 2k-2...
 */

# include "ProductMatrix.hh"
# define EXPERIMENT 1
# define DEGRADED_MODE 1

int enumerate_init(int range,int number,int* array){
	for(int i=0;i<number;i++){
		array[i]=i;
	}
	return 0;
}

int enumerate_next(int range,int number,int* array){
	int marker=-1;
	for(int i=number-1;i>=0;i--){
		if(array[i]==range+i-number){
			continue;
		}else{
			marker=i;
			break;
		}
	}
	if(marker==-1){
		return 1;
	}else{
		array[marker]++;
		for(int i=marker+1;i<number;i++){
			array[i]=array[marker]+i-marker;
		}
		return 0;
	}
}

ProductMatrix::ProductMatrix(int n,int k,int w){
	_encoding_matrix=NULL;
	_msg_mapping=NULL;
	_delta=NULL;
	_received=NULL;
	_downloading_list=NULL;
	_encoding_schedule=NULL;
	_sr_mapping_table=NULL;
	_inverse_sr_mapping_table=NULL;
	_ori_encoding_matrix=NULL;
	_XORed_ori_encoding_matrix=NULL;
	_encode_offline_matrix=NULL;
	_binary_encode_offline_matrix=NULL;
	_encode_offline_schedule=NULL;
	_recovery_equations=NULL;
	_binary_recovery_equations=NULL;
	_recovery_schedule=NULL;
	_virtual_failed_list=NULL;
	_repair_list=NULL;
	_degraded_read_schedule=NULL;
	_real_n=n;
	_real_k=k;
	_conf_k_=n-k+1;
	_conf_n_=2*_conf_k_-1;
	_fake_num=_conf_k_-_real_k;
	_conf_w_=w;
	_chunk_number_per_node_=_conf_k_-1;
	_systemetical_chunk_num_=_conf_k_*(_conf_k_-1);
	_encoded_chunk_num_=(_conf_n_-_conf_k_)*(_conf_k_-1);
	_total_chunk_num_=_systemetical_chunk_num_+_encoded_chunk_num_;
	_XORed_chunk_number_per_node_=_chunk_number_per_node_*_conf_w_;
	_XORed_systemetical_chunk_num_=_systemetical_chunk_num_*_conf_w_;
	_XORed_encoded_chunk_num_=_encoded_chunk_num_*_conf_w_;
	_XORed_total_chunk_num_=_total_chunk_num_*_conf_w_;
	_XOR_flag=1;
	_schedule_flag=1;
	_SSE_flag=1;
}

int ProductMatrix::restoreEverything(char* buffer){
	memcpy((char*)&_inverse_table,			        buffer,sizeof(char*));
	memcpy((char*)&_msg_mapping,			        buffer+1*sizeof(char*),sizeof(char*));
	memcpy((char*)&_delta,				            buffer+2*sizeof(char*),sizeof(char*));
	memcpy((char*)&_received,			            buffer+3*sizeof(char*),sizeof(char*));
	memcpy((char*)&_sr_mapping_table,		        buffer+4*sizeof(char*),sizeof(char*));
	memcpy((char*)&_inverse_sr_mapping_table,	    buffer+5*sizeof(char*),sizeof(char*));
	memcpy((char*)&_encoding_matrix,		        buffer+6*sizeof(char*),sizeof(char*));
	memcpy((char*)&_ori_encoding_matrix,		    buffer+7*sizeof(char*),sizeof(char*));
	memcpy((char*)&_XORed_ori_encoding_matrix,	    buffer+8*sizeof(char*),sizeof(char*));
	memcpy((char*)&_encoding_schedule,		        buffer+9*sizeof(char*),sizeof(char*));
	memcpy((char*)&_encode_offline_matrix,		    buffer+10*sizeof(char*),sizeof(char*));
	memcpy((char*)&_binary_encode_offline_matrix,	buffer+11*sizeof(char*),sizeof(char*));
	memcpy((char*)&_encode_offline_schedule,	    buffer+12*sizeof(char*),sizeof(char*));
	memcpy((char*)&_recovery_equations,		        buffer+13*sizeof(char*),sizeof(char*));
	memcpy((char*)&_binary_recovery_equations,	    buffer+14*sizeof(char*),sizeof(char*));
	memcpy((char*)&_downloading_list,		        buffer+15*sizeof(char*),sizeof(char*));
	memcpy((char*)&_recovery_schedule,		        buffer+16*sizeof(char*),sizeof(char*));
	return 0;
}

int ProductMatrix::backUpEverything(char* buffer){
	memcpy(buffer,			(char*)&_inverse_table,sizeof(char*));
	memcpy(buffer+1*sizeof(char*),	(char*)&_msg_mapping,sizeof(char*));
	memcpy(buffer+2*sizeof(char*),	(char*)&_delta,sizeof(char*));
	memcpy(buffer+3*sizeof(char*),	(char*)&_received,sizeof(char*));
	memcpy(buffer+4*sizeof(char*),	(char*)&_sr_mapping_table,sizeof(char*));
	memcpy(buffer+5*sizeof(char*),	(char*)&_inverse_sr_mapping_table,sizeof(char*));
	memcpy(buffer+6*sizeof(char*),	(char*)&_encoding_matrix,sizeof(char*));
	memcpy(buffer+7*sizeof(char*),	(char*)&_ori_encoding_matrix,sizeof(char*));
	memcpy(buffer+8*sizeof(char*),	(char*)&_XORed_ori_encoding_matrix,sizeof(char*));
	memcpy(buffer+9*sizeof(char*),	(char*)&_encoding_schedule,sizeof(char*));
	memcpy(buffer+10*sizeof(char*),	(char*)&_encode_offline_matrix,sizeof(char*));
	memcpy(buffer+11*sizeof(char*),	(char*)&_binary_encode_offline_matrix,sizeof(char*));
	memcpy(buffer+12*sizeof(char*),	(char*)&_encode_offline_schedule,sizeof(char*));
	memcpy(buffer+13*sizeof(char*),	(char*)&_recovery_equations,sizeof(char*));
	memcpy(buffer+14*sizeof(char*),	(char*)&_binary_recovery_equations,sizeof(char*));
	memcpy(buffer+15*sizeof(char*),	(char*)&_downloading_list,sizeof(char*));
	memcpy(buffer+16*sizeof(char*),	(char*)&_recovery_schedule,sizeof(char*));
	return 0;
}

int ProductMatrix::cleanup(){
    /*
     * mode==0 means do not free encoded related stuff
     * mode==1 means free everything
     */
	if(_received!=NULL){
		//printf("freeing _received\n");
		free(_received);
		_received=NULL;
	}
	if(_sr_mapping_table!=NULL){
		//printf("freeing _sr_mapping_table\n");
		free(_sr_mapping_table);
		_sr_mapping_table=NULL;
	}
	if(_inverse_sr_mapping_table!=NULL){
		//printf("freeing _inverse_sr_mapping_table\n");
		free(_inverse_sr_mapping_table);
		_inverse_sr_mapping_table=NULL;
	}
	if(_encode_offline_matrix!=NULL){
		//printf("freeing _encode_offline_matrix\n");
		free(_encode_offline_matrix);
		_encode_offline_matrix=NULL;
	}
	if(_binary_encode_offline_matrix!=NULL){
		//printf("freeing _binary_encode_offline_matrix\n");
		free(_binary_encode_offline_matrix);
		_binary_encode_offline_matrix=NULL;
	}
	if(_recovery_equations!=NULL){
		//printf("freeing _recovery_equations\n");
		free(_recovery_equations);
		_recovery_equations=NULL;
	}
	if(_binary_recovery_equations!=NULL){
		//printf("freeing _binary_recovery_equations\n");
		free(_binary_recovery_equations);
		_binary_recovery_equations=NULL;
	}
	if(_downloading_list!=NULL){
		//printf("freeing _downloading_list\n");
		free(_downloading_list);
		_downloading_list=NULL;
	}
	if(_encode_offline_schedule!=NULL){
		//printf("freeing _encoding_offline_schedule\n");
		jerasure_free_schedule(_encode_offline_schedule);
		_encode_offline_schedule=NULL;
	}
	if(_recovery_schedule!=NULL){
		//printf("freeing _recovery_schedule\n");
		jerasure_free_schedule(_recovery_schedule);
		_recovery_schedule=NULL;
	}
    //if(mode==1){
	//    if(_ori_encoding_matrix!=NULL){
	//    	//printf("freeing _ori_encoding_matrix\n");
	//    	free(_ori_encoding_matrix);
	//    	_ori_encoding_matrix=NULL;
	//    }
	//    if(_XORed_ori_encoding_matrix!=NULL){
	//    	//printf("freeing _XORed_ori_encoding_matrix\n");
	//    	free(_XORed_ori_encoding_matrix);
	//    	_XORed_ori_encoding_matrix=NULL;
	//    }
	//    if(_inverse_table!=NULL){
	//    	//printf("freeing encoding_matrix\n");
	//    	free(_inverse_table);
	//    	_inverse_table=NULL;
	//    }
	//    if(_encoding_matrix!=NULL){
	//    	//printf("freeing encoding_matrix\n");
	//    	free(_encoding_matrix);
	//    	_encoding_matrix=NULL;
	//    }
	//    if(_msg_mapping!=NULL){
	//    	//printf("freeing _msg_mapping\n");
	//    	free(_msg_mapping);
	//    	_msg_mapping=NULL;
	//    }
	//    if(_delta!=NULL){
	//    	//printf("freeing _delta\n");
	//    	free(_delta);
	//    	_delta=NULL;
	//    }
	//    if(_encoding_schedule!=NULL){
	//    	//printf("freeing _encoding_schedule\n");
	//    	jerasure_free_schedule(_encoding_schedule);
	//    	_encoding_schedule=NULL;
	//    }
    //}
	return 0;
}

int ProductMatrix::set_f_nocal(int number,int* list){
	_conf_f_=number;
	_failed_node_list=(int*)calloc(number,sizeof(int));
	for(int i=0;i<number;i++){
		_failed_node_list[i]=list[i]+_fake_num;
	}
}

int ProductMatrix::set_f(int number,int* list){
	_conf_f_=number;
	_failed_node_list=list;
	this->multi_node_repair(_conf_f_,_failed_node_list);
	if(_recovery_equations==NULL){
		return 1;
	}

	//generate encoding_offline_matrix
	/*_encode_offline_matrix=(int*)calloc(_conf_f_*(_conf_k_-1),sizeof(int));
	for(int i=0;i<_conf_f_;i++){
		memcpy((char*)_encode_offline_matrix+i*(_conf_k_-1)*sizeof(int),
				(char*)_encoding_matrix+_failed_node_list[i]*(2*_conf_k_-2)*sizeof(int),
                (_conf_k_-1)*sizeof(int));
	}*/
	/*if(_XOR_flag==1){
		_binary_encode_offline_matrix=
                jerasure_matrix_to_bitmatrix(_conf_k_-1,_conf_f_,_conf_w_,
                                _encode_offline_matrix);
		if(_schedule_flag==1){
			//generate a schedule
			_encode_offline_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_k_-1,_conf_f_,_conf_w_,
					_binary_encode_offline_matrix);
		}
	}

	//generate _binary_recovery_equations & corresponding schedule
	if(_XOR_flag==1){
		_binary_recovery_equations=
                jerasure_matrix_to_bitmatrix(_conf_f_*(2*_conf_k_-2),_conf_f_*(_conf_k_-1),_conf_w_,
                                _recovery_equations);
		if(_schedule_flag==1){
			//generate recovery schedule
			_recovery_schedule=
                    jerasure_smart_bitmatrix_to_schedule(_conf_f_*(2*_conf_k_-2),_conf_f_*(_conf_k_-1),_conf_w_,
                                    _binary_recovery_equations);
		}
	}*/
	return 0;
}

int ProductMatrix::set_f2_nocal(int vnumber,int* vlist,int number,int* list){
	_conf_f_=vnumber;
	_failed_node_list=vlist;
	_repair_number=number;
	_repair_list=list;
}

int ProductMatrix::set_f2(int vnumber,int* vlist,int number,int* list){
	_conf_f_=vnumber;
	_failed_node_list=(int*)calloc(vnumber,sizeof(int));
    for(int i=0;i<number;i++){
        _failed_node_list[i]=vlist[i]+_fake_num;
    }
	_repair_number=number;
	_repair_list=list;
	_repair_list=(int*)calloc(number,sizeof(int));
    for(int i=0;i<number;i++){
        _repair_list[i]=list[i]+_fake_num;
    }
	if(_recovery_equations!=NULL){
		free(_recovery_equations);
		_recovery_equations=NULL;
	}
	this->multi_node_repair(_conf_f_,_failed_node_list);
	if(_recovery_equations==NULL){
		return 1;
	}
	int* temp=(int*)calloc(_repair_number*(_conf_k_-1)*_conf_f_*(2*_conf_k_-2),sizeof(int));
	int index=0;
	//show_matrix(_recovery_equations,(_conf_k_-1)*vnumber,vnumber*(2*_conf_k_-2));
	for(int i=0;i<_conf_f_;i++){
		if(_failed_node_list[i]==_repair_list[index]){
			memcpy(temp+index*(_conf_k_-1)*_conf_f_*(2*_conf_k_-2),
					_recovery_equations+i*(_conf_k_-1)*_conf_f_*(2*_conf_k_-2),
					(_conf_k_-1)*_conf_f_*(2*_conf_k_-2)*sizeof(int));
			index++;
			if(index==number){
				break;
			}
		}
	}
	free(_recovery_equations);
	_recovery_equations=temp;

	//generate encoding_offline_matrix
	_encode_offline_matrix=(int*)calloc(_conf_f_*(_conf_k_-1),sizeof(int));
	for(int i=0;i<_conf_f_;i++){
		memcpy((char*)_encode_offline_matrix+i*(_conf_k_-1)*sizeof(int),
				(char*)_encoding_matrix+_failed_node_list[i]*(2*_conf_k_-2)*sizeof(int),(_conf_k_-1)*sizeof(int));
	}
	int* send_coefficient=(int *)calloc((_conf_n_-_conf_f_)*_conf_f_*_systemetical_chunk_num_,sizeof(int));

	if(_XOR_flag==1){
		_binary_encode_offline_matrix=jerasure_matrix_to_bitmatrix(_conf_k_-1,_conf_f_,_conf_w_,
                _encode_offline_matrix);
		if(_schedule_flag==1){
			//generate a schedule
			_encode_offline_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_k_-1,_conf_f_,_conf_w_,
					_binary_encode_offline_matrix);
		}
	}

	//generate _binary_recovery_equations & corresponding schedule
	if(_XOR_flag==1){
		_binary_recovery_equations=jerasure_matrix_to_bitmatrix(_conf_f_*(2*_conf_k_-2),
				_repair_number*(_conf_k_-1),
				_conf_w_,
				_recovery_equations);
        //show_matrix(_binary_recovery_equations,
        //        _repair_number*(_conf_k_-1)*_conf_w_,
        //        (2*_conf_f_-2)*_conf_k_*_conf_w_);
        //show_matrix(_recovery_equations,_repair_number*(_conf_k_-1),_conf_f_*(2*_conf_k_-2));
        //show_matrix(_binary_recovery_equations,
        //        _repair_number*(_conf_k_-1)*_conf_w_,
        //        _conf_f_*(2*_conf_k_-2)*_conf_w_);
		if(_schedule_flag==1){
			//generate recovery schedule
			_recovery_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_f_*(2*_conf_k_-2),
					_repair_number*(_conf_k_-1),
					_conf_w_,
					_binary_recovery_equations);
            //show_shedule(_recovery_schedule);
		}
	}
	return 0;
}

int ProductMatrix::set_f3_nocal(int vnumber,int* vlist,int number,int* list){
	_conf_f_=vnumber;
	_repair_number=number;
	_virtual_failed_list=(int*)calloc(vnumber,sizeof(int));
	_failed_node_list=(int*)calloc(number,sizeof(int));
	for(int i=0;i<number;i++){
		_failed_node_list[i]=list[i]+_fake_num;
	}
	for(int i=0;i<vnumber;i++){
		_virtual_failed_list[i]=vlist[i]+_fake_num;
	}
}

int ProductMatrix::set_f3(int vnumber,int* vlist,int number,int* list){
	_conf_f_=vnumber;
	_repair_number=number;
	_virtual_failed_list=(int*)calloc(vnumber,sizeof(int));
	_failed_node_list=(int*)calloc(number,sizeof(int));
	for(int i=0;i<number;i++){
		_failed_node_list[i]=list[i]+_fake_num;
	}
	for(int i=0;i<vnumber;i++){
		_virtual_failed_list[i]=vlist[i]+_fake_num;
	}
	if(_recovery_equations!=NULL){
		free(_recovery_equations);
		_recovery_equations=NULL;
	}
	this->multi_node_repair(_conf_f_,_failed_node_list);
	if(_recovery_equations==NULL){
		return 1;
	}
	int* temp=(int*)calloc(_repair_number*(_conf_k_-1)*_conf_f_*(2*_conf_k_-2),sizeof(int));
	int index=0;
	//show_matrix(_recovery_equations,(_conf_k_-1)*vnumber,vnumber*(2*_conf_k_-2));
	for(int i=0;i<_conf_f_;i++){
		if(_failed_node_list[i]==_repair_list[index]){
			memcpy(temp+index*(_conf_k_-1)*_conf_f_*(2*_conf_k_-2),
					_recovery_equations+i*(_conf_k_-1)*_conf_f_*(2*_conf_k_-2),
					(_conf_k_-1)*_conf_f_*(2*_conf_k_-2)*sizeof(int));
			index++;
			if(index==number){
				break;
			}
		}
	}
	free(_recovery_equations);
	_recovery_equations=temp;

	//generate encoding_offline_matrix
	_encode_offline_matrix=(int*)calloc(_conf_f_*(_conf_k_-1),sizeof(int));
	for(int i=0;i<_conf_f_;i++){
		memcpy((char*)_encode_offline_matrix+i*(_conf_k_-1)*sizeof(int),
				(char*)_encoding_matrix+_failed_node_list[i]*(2*_conf_k_-2)*sizeof(int),(_conf_k_-1)*sizeof(int));
	}
	int* send_coefficient=(int *)calloc((_conf_n_-_conf_f_)*_conf_f_*_systemetical_chunk_num_,sizeof(int));

	if(_XOR_flag==1){
		_binary_encode_offline_matrix=jerasure_matrix_to_bitmatrix(_conf_k_-1,_conf_f_,_conf_w_,
						_encode_offline_matrix);
		if(_schedule_flag==1){
			//generate a schedule
			_encode_offline_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_k_-1,_conf_f_,_conf_w_,
					_binary_encode_offline_matrix);
		}
	}

	//generate _binary_recovery_equations & corresponding schedule
	if(_XOR_flag==1){
		_binary_recovery_equations=jerasure_matrix_to_bitmatrix(_conf_f_*(2*_conf_k_-2),
				_repair_number*(_conf_k_-1),
				_conf_w_,
				_recovery_equations);
		if(_schedule_flag==1){
			//generate recovery schedule
			_recovery_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_f_*(2*_conf_k_-2),
					_repair_number*(_conf_k_-1),
					_conf_w_,
					_binary_recovery_equations);
		}
	}
	return 0;
}

int ProductMatrix::systematical(){
	//Let the first k node to be the systemetical node
	int row_num=_conf_k_*(_conf_k_-1);
	_msg_mapping=(int*)calloc(_systemetical_chunk_num_*_systemetical_chunk_num_,sizeof(int));
	inverse_matrix(_ori_encoding_matrix,_msg_mapping,_conf_k_*(_conf_k_-1));
	_ori_encoding_matrix=matrix_multiply2(_ori_encoding_matrix,
					_msg_mapping,
					_total_chunk_num_,
					_systemetical_chunk_num_,_systemetical_chunk_num_);
	return 0;
}

int ProductMatrix::print_parameters(){
	printf("Product Matrix n:%d\n",_conf_n_);
	printf("Product Matrix k:%d\n",_conf_k_);
	printf("Product Matrix w:%d\n",_conf_w_);
}

int ProductMatrix::generate_encoding_matrix(){
	//this is for generating encoding matrix
	//construcing encoding matrix
	//The coding matrix have n rows and (2k-2) columns
	int row_num=_conf_n_;
	int column_num=2*_conf_k_-2;
	generate_inverse_table();
	_encoding_matrix=(int*)calloc(row_num*column_num,sizeof(int));
	_delta=(int*)calloc(_conf_n_,sizeof(int));
	_received=(int*)calloc((2*_conf_k_-2)*_conf_k_*(_conf_k_-1),sizeof(int));
	
	for(int i=0;i<_conf_n_;i++){
		_encoding_matrix[i*column_num]=1;
	}

	int current=1;
	for(int i=0;i<row_num;i++){
		while(1){
			if(current>pow(2,_conf_w_)){
				_conf_w_++;
				printf("W increased to %d\n",_conf_w_);
				free(_encoding_matrix);
				free(_inverse_table);
				free(_delta);
				free(_received);
				_encoding_matrix=NULL;
				generate_inverse_table();
				this->generate_encoding_matrix();
				return 0;
			}
			_encoding_matrix[i*column_num+1]=current;
			for(int j=2;j<column_num;j++){
				_encoding_matrix[i*column_num+j]=galois_multiply(current,_encoding_matrix[i*column_num+j-1]);
			}
			//distinction check
			int distinct=1;
			for(int j=0;j<i;j++){
				if(_encoding_matrix[j*column_num+_conf_k_-1]==_encoding_matrix[i*column_num+_conf_k_-1]){
					distinct=0;
					break;
				}
			}
			if(distinct==0){
				current++;
				continue;
			}else{
				_delta[i]=_encoding_matrix[i*column_num+_conf_k_-1];
				current++;
				break;
			}
		}
	}

	int* message_matrix=(int*)calloc((2*_conf_k_-2)*(_conf_k_-1),sizeof(int));
	int index=0;
	for(int i=0;i<_conf_k_-1;i++){
		for(int j=i;j<_conf_k_-1;j++){
			message_matrix[i*(_conf_k_-1)+j]=index;
			message_matrix[j*(_conf_k_-1)+i]=index;
			index++;
		}
	}
	for(int i=0;i<_conf_k_-1;i++){
		for(int j=i;j<_conf_k_-1;j++){
			message_matrix[(i+_conf_k_-1)*(_conf_k_-1)+j]=index;
			message_matrix[(j+_conf_k_-1)*(_conf_k_-1)+i]=index;
			index++;
		}
	}

	_ori_encoding_matrix=(int*)calloc(_conf_n_*(_conf_k_-1)*_conf_k_*(_conf_k_-1),sizeof(int));
	for(int i=0;i<_conf_n_;i++){
		for(int j=0;j<_conf_k_-1;j++){
			for(int k=0;k<2*_conf_k_-2;k++){
				int pos=message_matrix[k*(_conf_k_-1)+j];
				_ori_encoding_matrix[(i*(_conf_k_-1)+j)*_conf_k_*(_conf_k_-1)+pos]=
					_encoding_matrix[i*(2*_conf_k_-2)+k];
			}
		}
	}
	
	systematical();

	if(_XOR_flag==1){
		encoding_matrix_XORization();
	}else{
		return 0;
	}

	
	return 0;
}

int ProductMatrix::encoding_matrix_XORization(){
	_XORed_ori_encoding_matrix=jerasure_matrix_to_bitmatrix(_systemetical_chunk_num_,_total_chunk_num_,_conf_w_,
			_ori_encoding_matrix);
	if(_schedule_flag==1){
		_encoding_schedule=jerasure_smart_bitmatrix_to_schedule(_conf_k_,_conf_n_-_conf_k_,
				_XORed_chunk_number_per_node_,
				_XORed_ori_encoding_matrix+_XORed_systemetical_chunk_num_*_XORed_systemetical_chunk_num_);
	}
	return 0;
}

int ProductMatrix::show_encoding_matrix(){
	for(int i=0;i<_conf_n_;i++){
		for(int j=0;j<2*_conf_k_-2;j++){
			printf("%4d",_encoding_matrix[i*(2*_conf_k_-2)+j]);
		}
		printf("\n");
	}
	return 0;
}

int* ProductMatrix::single_node_repair(int failed,int* _downloading_list){
	if(failed>=_conf_n_){
		printf("Product Matrix Error: Wrong failed node index\n");
		exit(0);
	}
	int* origin=(int*)calloc((2*_conf_k_-2)*(2*_conf_k_-2),sizeof(int));
	int index=0;
	for(int i=0;i<2*_conf_k_-1;i++){
		if(failed>_downloading_list[i]){
			memcpy((char*)origin+i*(2*_conf_k_-2)*sizeof(int),
					(char*)_encoding_matrix+_downloading_list[i]*(2*_conf_k_-2)*sizeof(int),
					(2*_conf_k_-2)*sizeof(int));
		}else if(failed<_downloading_list[i]){
			memcpy((char*)origin+(i-1)*(2*_conf_k_-2)*sizeof(int),
					(char*)_encoding_matrix+_downloading_list[i]*(2*_conf_k_-2)*sizeof(int),
					(2*_conf_k_-2)*sizeof(int));
		}
	}

	int* desmatrix=(int*)calloc((2*_conf_k_-2)*(2*_conf_k_-2),sizeof(int));
	inverse_matrix(origin,desmatrix,2*_conf_k_-2);
	free(origin);
	return desmatrix;
}

int ProductMatrix::update_downloading_list(){
	int* unlost_node=(int*)calloc(2*_conf_k_-1-_conf_f_,sizeof(int));
	int connected_nodes=2*_conf_k_-1-_conf_f_;
	int index=0;
	for(int i=0;i<2*_conf_k_-1;i++){
		if(is_failed(_downloading_list[i])==0){
			unlost_node[index]=_downloading_list[i];
			index++;
		}
	}
	int marker=-1;
	for(int i=connected_nodes-1;i>=0;i--){
		int larger=0;
		for(int j=unlost_node[i]+1;j<_conf_n_;j++){
			if(is_failed(j)==0){
				larger++;
			}
		}
		if(larger==connected_nodes-1-i){
			;
		}else{
			//find a larger index
			for(int j=unlost_node[i]+1;j<_conf_n_;j++){
				if(is_failed(j)==0){
					unlost_node[i]=j;
					break;
				}
			}
			for(int j=i+1;j<connected_nodes;j++){
				for(int k=unlost_node[j-1]+1;k<_conf_n_;k++){
					if(is_failed(k)==0){
						unlost_node[j]=k;
						break;
					}
				}
			}
			for(int j=0;j<_conf_f_;j++){
				_downloading_list[j]=_failed_node_list[j];
			}
			for(int j=0;j<connected_nodes;j++){
				_downloading_list[j+_conf_f_]=unlost_node[j];
			}
			for(int k=1;k<2*_conf_k_-1;k++){
				for(int j=k;j>0;j--){
					if(_downloading_list[j]<_downloading_list[j-1]){
						int temp=_downloading_list[j];
						_downloading_list[j]=_downloading_list[j-1];
						_downloading_list[j-1]=temp;
					}else{
						break;
					}
				}
			}
			return 0;
		}
	}
	return 1;
}

int ProductMatrix::test_validity(int number,int* list){
	this->set_f(number,list);
	if(_recovery_equations!=NULL){
		_virtual_failed_list=(int*)calloc(number,sizeof(int));
		for(int i=0;i<number;i++){
			_virtual_failed_list[i]=list[i];
		}
		return 0;
	}else{
		//Actually, it is quite rarely happened
		//So the efficience is not so important
		//first, let's make a list of unfailed nodes
		int unfailed[_conf_n_-number];
		int index=0;
		for(int i=0;i<_conf_n_;i++){
			int failed_flag=0;
			for(int j=0;j<number;j++){
				if(i==list[j]){
					failed_flag=1;
					break;
				}
			}
			if(failed_flag==0){
				unfailed[index]=i;
				index++;
			}
		}
		//create a new list containing the virtual failed nodes
		int* added_list=(int*)calloc(_conf_n_-number,sizeof(int));
		_virtual_failed_list=(int*)calloc(_conf_n_-_conf_k_,sizeof(int));
		int fulfill_flag=0;
		for(int i=0;i<_conf_n_-_conf_k_-number;i++){
			_virtual_f=number+i+1;
			//add 1+i failed nodes
			enumerate_init(_conf_n_-number,i+1,added_list);
			while(1){
				memcpy((char*)_virtual_failed_list,(char*)list,number*sizeof(int));
				//insert the virtual failed nodes
				for(int j=0;j<i+1;j++){
					int inserted=unfailed[added_list[j]];
					//printf("inserted: %d\n",inserted);
					int position=number+j;
					for(int k=0;k<number+j;k++){
						if(_virtual_failed_list[k]>inserted){
							position=k;
							break;
						}
					}
					for(int k=number+j;k>position;k--){
						_virtual_failed_list[k]=_virtual_failed_list[k-1];
					}
					_virtual_failed_list[position]=inserted;
				}
				//test whether the new list is OK
				this->set_f(_virtual_f,_virtual_failed_list);
				if(_recovery_equations!=NULL){
					fulfill_flag=1;
					break;
				}else{
					if(enumerate_next(_conf_n_-number,i+1,added_list)==1){
						break;
					}
				}
			}
			if(fulfill_flag==1){
				break;
			}
		}
		free(added_list);
	}
	return _virtual_f-number;
}

int ProductMatrix::test_validity2(int number,int* ilist){
    int* list=(int*)calloc(number,sizeof(int));
	for(int i=0;i<number;i++){
		list[i]=ilist[i]+_fake_num;
	}
	this->set_f(number,list);
	if(_recovery_equations!=NULL){
		_virtual_failed_list=(int*)calloc(number,sizeof(int));
		for(int i=0;i<number;i++){
			_virtual_failed_list[i]=ilist[i];
		}
		return 0;
	}else{
		//Actually, it is quite rarely happened
		//So the efficience is not so important
		//first, let's make a list of unfailed nodes
		int unfailed[_real_n-number];
		int index=0;
		for(int i=_fake_num;i<_conf_n_;i++){
			int failed_flag=0;
			for(int j=0;j<number;j++){
				if(i==list[j]){
					failed_flag=1;
					break;
				}
			}
			if(failed_flag==0){
				unfailed[index]=i;
				index++;
			}
		}
		//create a new list containing the virtual failed nodes
		int* added_list=(int*)calloc(_real_n-number,sizeof(int));
		_virtual_failed_list=(int*)calloc(_real_n-_conf_k_,sizeof(int));
		int fulfill_flag=0;
		for(int i=0;i<_real_n-_real_k-number;i++){
			_virtual_f=number+i+1;
			//add 1+i failed nodes
			enumerate_init(_real_n-number,i+1,added_list);
			while(1){
				memcpy((char*)_virtual_failed_list,(char*)list,number*sizeof(int));
				//insert the virtual failed nodes
				for(int j=0;j<i+1;j++){
					int inserted=unfailed[added_list[j]];
					//printf("inserted: %d\n",inserted);
					int position=number+j;
					for(int k=0;k<number+j;k++){
						if(_virtual_failed_list[k]>inserted){
							position=k;
							break;
						}
					}
					for(int k=number+j;k>position;k--){
						_virtual_failed_list[k]=_virtual_failed_list[k-1];
					}
					_virtual_failed_list[position]=inserted;
				}
				//test whether the new list is OK
				this->set_f(_virtual_f,_virtual_failed_list);
				if(_recovery_equations!=NULL){
					fulfill_flag=1;
					break;
				}else{
					if(enumerate_next(_conf_n_-number,i+1,added_list)==1){
						break;
					}
				}
			}
			if(fulfill_flag==1){
				break;
			}
		}
		free(added_list);
		for(int i=0;i<_virtual_f;i++){
			_virtual_failed_list[i]-=_fake_num;
		}
	}
	return _virtual_f-number;
}

int* ProductMatrix::multi_node_repair(int number,int* list){
	//The number of virtual chunks is (number-1)*number
	//Picking out the nodes to send data. 
	_downloading_list=(int*)calloc((2*_conf_k_-1),sizeof(int));
	int* equations=(int*)calloc(number*number*(number-1)*(2*_conf_k_-2),sizeof(int));
	_recovery_equations=(int*)calloc(number*(_conf_k_-1)*number*(2*_conf_k_-2),sizeof(int));
	//int* chunks__received=(int*)calloc(number*(2*_conf_k_-2)*_conf_k_*(_conf_k_-1),sizeof(int));
	int involve=0;
	for(int i=0;i<number;i++){
		_downloading_list[i]=list[i];
	}
	int index=number;
	int position=0;
	while(index!=2*_conf_k_-1){
		int i;
		for(i=0;i<index;i++){
			if(_downloading_list[i]==position){
				break;
			}
		}
		if(i==index){
			_downloading_list[index]=position;
			index++;
		}
		position++;
	}
	//sort the downloading list
	for(int i=1;i<2*_conf_k_-1;i++){
		for(int j=i;j>0;j--){
			if(_downloading_list[j]<_downloading_list[j-1]){
				int temp=_downloading_list[j];
				_downloading_list[j]=_downloading_list[j-1];
				_downloading_list[j-1]=temp;
			}else{
				break;
			}
		}
	}
	
	int* coeffience=(int*)calloc(number*number*(number-1)*(number-1),sizeof(int));
	int* inverse_coeffience=(int*)calloc(number*number*(number-1)*(number-1),sizeof(int));
	int fposition[number];
	int cposition[number*(number-1)];

	//find coefficients and ...
	for(int i=0;i<number;i++){
		int* result=this->single_node_repair(list[i],_downloading_list);
		int* equa=(int*)malloc((_conf_k_-1)*(2*_conf_k_-2)*sizeof(int));
		for(int j=0;j<_conf_k_-1;j++){
			for(int k=0;k<2*_conf_k_-2;k++){
				equa[j*(2*_conf_k_-2)+k]=result[j*(2*_conf_k_-2)+k];
			}
		}
		for(int j=0;j<_conf_k_-1;j++){
			for(int k=0;k<2*_conf_k_-2;k++){
				equa[j*(2*_conf_k_-2)+k]^=galois_multiply(_delta[list[i]],
						result[(j+_conf_k_-1)*(2*_conf_k_-2)+k]);
				memcpy((char*)_recovery_equations+((i*(_conf_k_-1)+j)*number+i)*(2*_conf_k_-2)*sizeof(int),
						(char*)equa+j*(2*_conf_k_-2)*sizeof(int),
						(2*_conf_k_-2)*sizeof(int));
			}
		}
		for(int m=0;m<i;m++){
			int* sending_coeffience=(int*)calloc((2*_conf_k_-2),sizeof(int));
			int sendto=m;
			for(int j=0;j<_conf_k_-1;j++){
				for(int k=0;k<2*_conf_k_-2;k++){
					sending_coeffience[k]^=
						galois_multiply(_encoding_matrix[list[sendto]*(2*_conf_k_-2)+j],
							equa[j*(2*_conf_k_-2)+k]);
				}
			}
			memcpy((char*)equations+((i*(number-1)+m)*number*(2*_conf_k_-2)+i*(2*_conf_k_-2))*sizeof(int),
					(char*)sending_coeffience,(2*_conf_k_-2)*sizeof(int));
			free(sending_coeffience);
		}
		for(int m=i+1;m<number;m++){
			int* sending_coeffience=(int*)calloc((2*_conf_k_-2),sizeof(int));
			int sendto=m;
			for(int j=0;j<_conf_k_-1;j++){
				for(int k=0;k<2*_conf_k_-2;k++){
					sending_coeffience[k]^=
						galois_multiply(_encoding_matrix[list[sendto]*(2*_conf_k_-2)+j],
							equa[j*(2*_conf_k_-2)+k]);
				}
			}
			memcpy((char*)equations+((i*(number-1)+m-1)*number*(2*_conf_k_-2)+i*(2*_conf_k_-2))*sizeof(int),
					(char*)sending_coeffience,(2*_conf_k_-2)*sizeof(int));
			free(sending_coeffience);
		}
		free(equa);
		free(result);
	}
	for(int i=0;i<number;i++){
		for(int j=0;j<2*_conf_k_-1;j++){
			if(list[i]==_downloading_list[j]){
				fposition[i]=j;
				break;
			}
		}
	}
	//cposition mean the positions of the virtual chunks
	for(int i=0;i<number;i++){
		for(int j=0;j<i;j++){
			cposition[i*(number-1)+j]=i*(2*_conf_k_-2)+fposition[j];
		}
		for(int j=i+1;j<number;j++){
			cposition[i*(number-1)+j-1]=i*(2*_conf_k_-2)+fposition[j]-1;
		}
	}
	//show_matrix(equations,number*(number-1),number*(2*_conf_k_-2));
	for(int i=0;i<number*(number-1);i++){
		for(int j=0;j<number*(number-1);j++){
			coeffience[j*number*(number-1)+i]=equations[j*number*(2*_conf_k_-2)+cposition[i]];
			equations[j*number*(2*_conf_k_-2)+cposition[i]]=0;
		}
	}
	for(int i=0;i<number*(number-1);i++){
		int sender=i/(number-1);
		int receiver=i%(number-1);
		if(receiver>=sender){
			receiver++;
		}
		if(receiver<sender){
			sender--;
		}
		coeffience[i*number*(number-1)+receiver*(number-1)+sender]^=1;
	}
	int ret_val=inverse_matrix(coeffience,inverse_coeffience,number*(number-1));
	if(ret_val==1){
		show_failed_nodes();
		if(update_downloading_list()==1){
			free(equations);
			free(coeffience);
			free(inverse_coeffience);
			free(_recovery_equations);
			_recovery_equations=NULL;
			return NULL;
		}
	}	
	int* final=matrix_multiply2(inverse_coeffience,equations,number*(number-1),
			number*(2*_conf_k_-2),number*(number-1));
	//show_matrix(final,number*(number-1),number*(2*_conf_k_-2));
	for(int i=0;i<number*(number-1);i++){
		int sender=i/(number-1);
		int receiver=i%(number-1);
		int column_pos;
		if(receiver>=sender){
			receiver++;
			column_pos=sender*(2*_conf_k_-2)+fposition[receiver]-1;
		}else{
			column_pos=sender*(2*_conf_k_-2)+fposition[receiver];
		}
		for(int j=0;j<number*(_conf_k_-1);j++){
			if(_recovery_equations[j*number*(2*_conf_k_-2)+column_pos]!=0){
				int factor=_recovery_equations[j*number*(2*_conf_k_-2)+column_pos];
				for(int k=0;k<number*(2*_conf_k_-2);k++){
					_recovery_equations[j*number*(2*_conf_k_-2)+k]^=
						galois_multiply(final[i*number*(2*_conf_k_-2)+k],factor);
				}
				_recovery_equations[j*number*(2*_conf_k_-2)+column_pos]=0;
			}
		}
	}
	//show_matrix(_recovery_equations,number*(_conf_k_-1),number*(2*_conf_k_-2));
	free(equations);
	free(final);
	free(coeffience);
	free(inverse_coeffience);

	/*
	 * Create _sr_mapping_table, which shows the chunk position in the recovery equations
	 * also _inverse_sr_mapping_table, which look up chunk position by index in
	 * recovery equations
	 */
	_sr_mapping_table=(int*)calloc(_conf_f_*(2*_conf_k_-1-number),sizeof(int));
	_inverse_sr_mapping_table=(int*)calloc(_conf_f_*(2*_conf_k_-2),sizeof(int));
	int sen_index=0;
	for(int i=0;i<2*_conf_k_-1;i++){
		//i is the index of sender
		if(is_failed(_downloading_list[i])==0){
			//this means it is a real provider
			;
		}else{
			continue;
		}
		for(int j=0;j<_conf_f_;j++){
			if(_downloading_list[i]>_failed_node_list[j]){
				_sr_mapping_table[sen_index*_conf_f_+j]=j*(2*_conf_k_-2)+i-1;
				_inverse_sr_mapping_table[j*(2*_conf_k_-2)+i-1]=sen_index*_conf_f_+j;
			}else{
				_sr_mapping_table[sen_index*_conf_f_+j]=j*(2*_conf_k_-2)+i;
				_inverse_sr_mapping_table[j*(2*_conf_k_-2)+i]=sen_index*_conf_f_+j;
			}
		}
		sen_index++;
	}

	return _recovery_equations;
}

int ProductMatrix::encode_offline_recovery2(char* content,char* buffer,int length){
	if(_XOR_flag!=1){
		//Without XOR optimization, Galois Field operations
		int chunksize=length/_chunk_number_per_node_;
		for(int i=0;i<_chunk_number_per_node_;i++){
			//Each chunk per round
			char* rbuffer=content+i*chunksize;
			for(int j=0;j<_conf_f_;j++){
				if(_SSE_flag==0){
					galois_w08_region_multiply(rbuffer,
							_encode_offline_matrix[
								j*_chunk_number_per_node_+i],
							chunksize,
							buffer+j*chunksize,1);
				}else{
					ff_add_mulv_local((uint8_t*)buffer+j*chunksize,
							(uint8_t*)rbuffer,
							_encode_offline_matrix[
								j*_chunk_number_per_node_+i],
							chunksize);
				}
			}
		}
		return 0;
	}

	int chunksize=length/_XORed_chunk_number_per_node_;
	if(_XOR_flag==1){
		if(_schedule_flag==1){
			int index=0;
			while(_encode_offline_schedule[index][0]!=-1){
				if(_encode_offline_schedule[index][0]<_conf_k_-1){
					XOR_buffer(buffer+((_encode_offline_schedule[index][2]-_conf_k_+1)*_conf_w_+
								_encode_offline_schedule[index][3])*chunksize,
							content+(_encode_offline_schedule[index][0]*_conf_w_+
								_encode_offline_schedule[index][1])*chunksize,
							chunksize);
				}else{
					XOR_buffer(buffer+((_encode_offline_schedule[index][2]-_conf_k_+1)*_conf_w_+
								_encode_offline_schedule[index][3])*chunksize,
							buffer+((_encode_offline_schedule[index][0]-_conf_k_+1)*_conf_w_+
								_encode_offline_schedule[index][1])*chunksize,
							chunksize);
				}
				index++;
			}
		}else{
			for(int i=0;i<_XORed_chunk_number_per_node_;i++){
				//Each chunk per round
				char* rbuffer=content+i*chunksize;
				for(int j=0;j<_conf_f_*_conf_w_;j++){
					if(_binary_encode_offline_matrix[j*_XORed_chunk_number_per_node_+i]==0){
						continue;
					}
					XOR_buffer(buffer+j*chunksize,rbuffer,chunksize);
				}
			}
		}
	}
	return 0;
}

char* ProductMatrix::encode_offline_recovery(char* content,int length){
	int chunk_num_per_node=_XORed_chunk_number_per_node_;
	int chunksize=length/_XORed_chunk_number_per_node_;
	char* send_chunk=(char*)calloc(_conf_f_*_conf_w_*chunksize,sizeof(char));

	if(_XOR_flag==1){
		if(_schedule_flag==1){
			int index=0;
			while(_encode_offline_schedule[index][0]!=-1){
				if(_encode_offline_schedule[index][0]<(_conf_k_-1)){
					XOR_buffer(send_chunk+((_encode_offline_schedule[index][2]-_conf_k_+1)*_conf_w_+
								_encode_offline_schedule[index][3])*chunksize,
							content+(_encode_offline_schedule[index][0]*_conf_w_+
								_encode_offline_schedule[index][1])*chunksize,
							chunksize);
				}else{
					XOR_buffer(send_chunk+((_encode_offline_schedule[index][2]-_conf_k_+1)*_conf_w_+
								_encode_offline_schedule[index][3])*chunksize,
							send_chunk+((_encode_offline_schedule[index][0]-_conf_k_+1)*_conf_w_+
								_encode_offline_schedule[index][1])*chunksize,
							chunksize);
				}
				index++;
			}
		}else{
			for(int i=0;i<chunk_num_per_node;i++){
				//Each chunk per round
				char* rbuffer=content+i*chunksize;
				for(int j=0;j<_conf_f_*_conf_w_;j++){
					if(_binary_encode_offline_matrix[j*chunk_num_per_node+i]==0){
						continue;
					}
					XOR_buffer(send_chunk+j*chunksize,rbuffer,chunksize);
				}
			}
		}
	}
	return send_chunk;
}

int ProductMatrix::pos_in_downloadinglist(int index){
	for(int i=0;i<2*_conf_k_-1;i++){
		if(index==_downloading_list[i]){
			return i;
		}
	}
	return -1;
}

int ProductMatrix::reconstruct_lost_data3(char* received_chunks,char* reconstructed_chunks,int length){
	//show_matrix(_recovery_equations,_repair_number*(_conf_k_-1),_conf_f_*(2*_conf_k_-2));
	if(_XOR_flag!=1){
		//Without XOR optimization
		int connected=2*_conf_k_-2;
		int connected_nodes=2*_conf_k_-1-_conf_f_;
		int chunk_size=length/_chunk_number_per_node_;
		//printf("%d\n",chunk_size);
		int rec_equations_column_num=_conf_f_*connected;
		for(int i=0;i<connected_nodes;i++){
			for(int j=0;j<_conf_f_;j++){
				int sr_pos=_sr_mapping_table[i*_conf_f_+j];
				char* rbuffer=received_chunks+(i*_conf_f_+j)*chunk_size;
				for(int l=0;l<_repair_number*_chunk_number_per_node_;l++){
				    if(_SSE_flag==0){
					galois_w08_region_multiply(rbuffer,
							_recovery_equations[l*rec_equations_column_num+
								_sr_mapping_table[i*_conf_f_+j]],
							chunk_size,
							reconstructed_chunks+l*chunk_size,1);
				    }else{
					ff_add_mulv_local((uint8_t *)reconstructed_chunks+l*chunk_size,
						(uint8_t*)rbuffer,
						_recovery_equations[l*rec_equations_column_num+
							_sr_mapping_table[i*_conf_f_+j]],
						chunk_size);
				    }
				}
			}
		}
		return 0;
	}

	int connected=2*_conf_k_-2;
	int connected_nodes=2*_conf_k_-1-_conf_f_;
	int chunk_size=length/_XORed_chunk_number_per_node_;
	int rec_equations_column_num=_conf_f_*connected*_conf_w_;
	
	//Do regeneration
	if((_XOR_flag==1)&&(_schedule_flag==0)){
		for(int i=0;i<connected_nodes;i++){
			for(int j=0;j<_conf_f_;j++){
				int sr_pos=_sr_mapping_table[i*_conf_f_+j]*_conf_w_;
				for(int k=0;k<_conf_w_;k++){
					//get a _received chunk
					char* rbuffer=received_chunks+((i*_conf_f_+j)*_conf_w_+k)*chunk_size;
					int pos_in_rec=sr_pos+k;
					for(int l=0;l<_repair_number*_XORed_chunk_number_per_node_;l++){
						if(_binary_recovery_equations[l*rec_equations_column_num+pos_in_rec]==1){
							XOR_buffer(reconstructed_chunks+l*chunk_size,
									rbuffer,
									chunk_size);
						}
					}
				}
			}
		}
	}else if((_XOR_flag==1)&&(_schedule_flag==1)){
		int index=0;
		while(_recovery_schedule[index][0]!=-1){
			if(_recovery_schedule[index][0]<_conf_f_*(2*_conf_k_-2)){
				XOR_buffer(reconstructed_chunks+((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][3])*chunk_size,
						received_chunks+(_inverse_sr_mapping_table[_recovery_schedule[index][0]]*_conf_w_+
							_recovery_schedule[index][1])*chunk_size,
						chunk_size);
			}else{
				XOR_buffer(reconstructed_chunks+((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][3])*chunk_size,
						reconstructed_chunks+((_recovery_schedule[index][0]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][1])*chunk_size,
						chunk_size);
			}
			index++;
		}
	}
	
	return 0;
}

int ProductMatrix::reconstruct_lost_data4(char* received_chunks,char* reconstructed_chunks,int length){
	//TODO:modify this!!!
	//show_matrix(_recovery_equations,_repair_number*(_conf_k_-1),_conf_f_*(2*_conf_k_-2));
	if(_XOR_flag!=1){
		//Without XOR optimization
		int connected=2*_conf_k_-2;
		int connected_nodes=2*_conf_k_-1-_conf_f_;
		int chunk_size=length/_chunk_number_per_node_;
		//printf("%d\n",chunk_size);
		int rec_equations_column_num=_conf_f_*connected;
		for(int i=_fake_num;i<connected_nodes;i++){
			for(int j=0;j<_conf_f_;j++){
				int sr_pos=_sr_mapping_table[i*_conf_f_+j];
				char* rbuffer=received_chunks+((i-_fake_num)*_conf_f_+j)*chunk_size;
				for(int l=0;l<_repair_number*_chunk_number_per_node_;l++){
				    if(_SSE_flag==0){
						galois_w08_region_multiply(rbuffer,
								_recovery_equations[l*rec_equations_column_num+
									_sr_mapping_table[i*_conf_f_+j]],
								chunk_size,
								reconstructed_chunks+l*chunk_size,1);
				    }else{
						ff_add_mulv_local((uint8_t *)reconstructed_chunks+l*chunk_size,
							(uint8_t*)rbuffer,
							_recovery_equations[l*rec_equations_column_num+
								_sr_mapping_table[i*_conf_f_+j]],
							chunk_size);
				    }
				}
			}
		}
		return 0;
	}

	int connected=2*_conf_k_-2;
	int connected_nodes=2*_conf_k_-1-_conf_f_;
	int chunk_size=length/_XORed_chunk_number_per_node_;
	int rec_equations_column_num=_conf_f_*connected*_conf_w_;
	
	//Do regeneration
	if((_XOR_flag==1)&&(_schedule_flag==0)){
		for(int i=0;i<connected_nodes;i++){
			for(int j=_fake_num;j<_conf_f_;j++){
				int sr_pos=_sr_mapping_table[i*_conf_f_+j]*_conf_w_;
				for(int k=0;k<_conf_w_;k++){
					//get a _received chunk
					char* rbuffer=received_chunks+(((i-_fake_num)*_conf_f_+j)*_conf_w_+k)*chunk_size;
					int pos_in_rec=sr_pos+k;
					for(int l=0;l<_repair_number*_XORed_chunk_number_per_node_;l++){
						if(_binary_recovery_equations[l*rec_equations_column_num+pos_in_rec]==1){
							XOR_buffer(reconstructed_chunks+l*chunk_size,
									rbuffer,
									chunk_size);
						}
					}
				}
			}
		}
	}else if((_XOR_flag==1)&&(_schedule_flag==1)){
		int index=0;
		int start=_fake_num*_conf_f_;
        //printf("chunk size:%d\n",chunk_size);
        //printf("eq addr:%x\n",_recovery_equations);
        //printf("eq addr:%x\n",_binary_recovery_equations);
        //for(int i=0;i<_repair_number*(_conf_k_-1)*_conf_w_;i++){
        //    for(int j=0;j<(2*_conf_k_-2)*_conf_f_*_conf_w_;j++){
        //        printf("%4d",_binary_recovery_equations[
        //                i*(2*_conf_k_-2)*_conf_f_*_conf_w_+j]);
        //    }
        //    printf("\n");
        //}
        //printf("\n");
        //show_shedule(_recovery_schedule);
		while(_recovery_schedule[index][0]!=-1){
            //printf("%4d:%4d%4d%4d%4d%4d\n",index,
            //        _encoding_schedule[index][0],
            //        _encoding_schedule[index][1],
            //        _encoding_schedule[index][2],
            //        _encoding_schedule[index][3],
            //        _encoding_schedule[index][4]
            //        );
            if((_inverse_sr_mapping_table[_recovery_schedule[index][0]]<start)
                    &&(_recovery_schedule[index][0]<_conf_f_*(2*_conf_k_-2))){
                index++;
                continue;
            }
			if(_recovery_schedule[index][0]<_conf_f_*(2*_conf_k_-2)){
                //printf("1:%d %d\n",((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
				//			_recovery_schedule[index][3])*chunk_size,
                //        (_inverse_sr_mapping_table[_recovery_schedule[index][0]]*_conf_w_+
				//			_recovery_schedule[index][1]-start)*chunk_size);
				XOR_buffer(reconstructed_chunks+((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][3])*chunk_size,
						received_chunks+((_inverse_sr_mapping_table[_recovery_schedule[index][0]]-start)*_conf_w_+
							_recovery_schedule[index][1])*chunk_size,
						chunk_size);
			}else{
                //printf("%4d:%4d%4d%4d%4d%4d\n",index,
                //        _recovery_schedule[index][0],
                //        _recovery_schedule[index][1],
                //        _recovery_schedule[index][2],
                //        _recovery_schedule[index][3],
                //        _recovery_schedule[index][4]
                //        );
                //printf("2:%d %d\n",((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
				//			_recovery_schedule[index][3])*chunk_size,
				//		((_recovery_schedule[index][0]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
				//			_recovery_schedule[index][1])*chunk_size);
				XOR_buffer(reconstructed_chunks+((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][3])*chunk_size,
						reconstructed_chunks+((_recovery_schedule[index][0]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][1])*chunk_size,
						chunk_size);
			}
			index++;
		}
	}
	
	return 0;
}

int ProductMatrix::reconstruct_lost_data2(char* _received_chunks,char* reconstructed_chunks,int length){
	if(_XOR_flag!=1){
		//Without XOR optimization
		int connected=2*_conf_k_-2;
		int connected_nodes=2*_conf_k_-1-_conf_f_;
		int chunk_size=length/_chunk_number_per_node_;
		int rec_equations_column_num=_conf_f_*connected;
		for(int i=0;i<connected_nodes;i++){
			for(int j=0;j<_conf_f_;j++){
				int sr_pos=_sr_mapping_table[i*_conf_f_+j];
				char* rbuffer=_received_chunks+(i*_conf_f_+j)*chunk_size;
				for(int l=0;l<_conf_f_*_chunk_number_per_node_;l++){
				    if(_SSE_flag==0){
					galois_w08_region_multiply(rbuffer,
						_recovery_equations[l*rec_equations_column_num+
							_sr_mapping_table[i*_conf_f_+j]],
						chunk_size,
						reconstructed_chunks+l*chunk_size,1);
				    }else{
					ff_add_mulv_local((uint8_t*)reconstructed_chunks+l*chunk_size,
						(uint8_t*)rbuffer,
						_recovery_equations[l*rec_equations_column_num+
							_sr_mapping_table[i*_conf_f_+j]],
						chunk_size);
				    }
				}
			}
		}
		return 0;
	}

	int connected=2*_conf_k_-2;
	int connected_nodes=2*_conf_k_-1-_conf_f_;
	int chunk_size=length/_XORed_chunk_number_per_node_;
	//char* reconstructed_chunks=(char*)calloc(_conf_f_*_XORed_chunk_number_per_node_*chunk_size,sizeof(char));
	int rec_equations_column_num=_conf_f_*connected*_conf_w_;
	
	//Do regeneration
	if((_XOR_flag==1)&&(_schedule_flag==0)){
		for(int i=0;i<connected_nodes;i++){
			for(int j=0;j<_conf_f_;j++){
				int sr_pos=_sr_mapping_table[i*_conf_f_+j]*_conf_w_;
				//printf("sr_pos:%d\n",sr_pos);
				for(int k=0;k<_conf_w_;k++){
					//get a _received chunk
					char* rbuffer=_received_chunks+((i*_conf_f_+j)*_conf_w_+k)*chunk_size;
					int pos_in_rec=sr_pos+k;
					//printf(" pos_in_rec:%d\n",pos_in_rec);
					for(int l=0;l<_conf_f_*_XORed_chunk_number_per_node_;l++){
						//printf("  index:%d\n",l*rec_equations_column_num+pos_in_rec);
						if(_binary_recovery_equations[l*rec_equations_column_num+pos_in_rec]==1){
							XOR_buffer(reconstructed_chunks+l*chunk_size,
									rbuffer,
									chunk_size);
						}
					}
				}
			}
		}
	}else if((_XOR_flag==1)&&(_schedule_flag==1)){
		int index=0;
		while(_recovery_schedule[index][0]!=-1){
			if(_recovery_schedule[index][0]<_conf_f_*(2*_conf_k_-2)){
				XOR_buffer(reconstructed_chunks+((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][3])*chunk_size,
						_received_chunks+(_inverse_sr_mapping_table[_recovery_schedule[index][0]]*_conf_w_+
							_recovery_schedule[index][1])*chunk_size,
						chunk_size);
			}else{
				XOR_buffer(reconstructed_chunks+((_recovery_schedule[index][2]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][3])*chunk_size,
						reconstructed_chunks+((_recovery_schedule[index][0]-_conf_f_*(2*_conf_k_-2))*_conf_w_+
							_recovery_schedule[index][1])*chunk_size,
						chunk_size);
			}
			index++;
		}
	}
	
	return 0;
}

char* ProductMatrix::reconstruct_lost_data(char** _received_chunks,int length){
	int chunk_num_per_node=_XORed_chunk_number_per_node_;
	int connected=2*_conf_k_-2;
	int connected_nodes=2*_conf_k_-1-_conf_f_;
	int chunk_size=length/_XORed_chunk_number_per_node_;
	char* reconstructed_chunks=(char*)calloc(_conf_f_*chunk_num_per_node*chunk_size,sizeof(char));
	int rec_equations_column_num=_conf_f_*connected*_conf_w_;
	
	int* binary_recovery_equations=jerasure_matrix_to_bitmatrix(_conf_f_*(2*_conf_k_-2),_conf_f_*(_conf_k_-1),_conf_w_,
			_recovery_equations);

	//Do regeneration
	for(int i=0;i<connected_nodes;i++){
		for(int j=0;j<_conf_f_;j++){
			int sr_pos=_sr_mapping_table[i*_conf_f_+j]*_conf_w_;
			for(int k=0;k<_conf_w_;k++){
				//get a _received chunk
				char* rbuffer=_received_chunks[i]+(j*_conf_w_+k)*chunk_size;
				int pos_in_rec=sr_pos+k;
				for(int l=0;l<_conf_f_*chunk_num_per_node;l++){
					if(binary_recovery_equations[l*rec_equations_column_num+pos_in_rec]==1){
						XOR_buffer(reconstructed_chunks+l*chunk_size,
								rbuffer,
								chunk_size);
					}
				}
			}
		}
	}
	return reconstructed_chunks;
}

char* ProductMatrix::encode(char* content, int length){
	int ori_chunk_num=_XORed_systemetical_chunk_num_;
	int chunk_size=length/_XORed_systemetical_chunk_num_;
	char* buffer=(char*)calloc(length*_conf_n_/_conf_k_,sizeof(char));
	int base=_XORed_systemetical_chunk_num_*_XORed_systemetical_chunk_num_;
	memcpy(buffer,content,length);

	if(_schedule_flag==0){
		for(int i=0;i<_XORed_encoded_chunk_num_;i++){
			char* target=buffer+(_XORed_systemetical_chunk_num_+i)*chunk_size;
			for(int j=0;j<_XORed_systemetical_chunk_num_;j++){
				if(_XORed_ori_encoding_matrix[base+i*_XORed_systemetical_chunk_num_+j]==1){
					XOR_buffer(target,content+j*chunk_size,chunk_size);
				}
			}
		}
	}else{
		int index=0;
		while(1){
			if(_encoding_schedule[index][0]!=-1){
				XOR_buffer(buffer+(_encoding_schedule[index][2]*_XORed_chunk_number_per_node_+
							_encoding_schedule[index][3])*chunk_size,
						buffer+(_encoding_schedule[index][0]*_XORed_chunk_number_per_node_+
							_encoding_schedule[index][1])*chunk_size,
						chunk_size);
				index++;
			}else{
				break;
			}
		}
	}

	return buffer;
}

int ProductMatrix::encode2(char* content,char* buffer, int length){
	if(_XOR_flag!=1){
		int chunk_size=length/_systemetical_chunk_num_;
		int base=_systemetical_chunk_num_*_systemetical_chunk_num_;
		for(int i=0;i<_encoded_chunk_num_;i++){
			char* target=buffer+i*chunk_size;
			for(int j=0;j<_systemetical_chunk_num_;j++){
				if(_SSE_flag==0){
					galois_w08_region_multiply(content+j*chunk_size,
							_ori_encoding_matrix[base+
								i*_systemetical_chunk_num_+j],
							chunk_size,target,1);
				}else{
					ff_add_mulv_local((uint8_t*)target,
							(uint8_t*)content+j*chunk_size,
							_ori_encoding_matrix[base+
								i*_systemetical_chunk_num_+j],
							chunk_size);
				}
			}
		}
		return 0;
	}

	int chunk_size=length/_XORed_systemetical_chunk_num_;
	int base=_XORed_systemetical_chunk_num_*_XORed_systemetical_chunk_num_;

	if(_schedule_flag==0){
		for(int i=0;i<_XORed_encoded_chunk_num_;i++){
			char* target=buffer+i*chunk_size;
			for(int j=0;j<_XORed_systemetical_chunk_num_;j++){
				if(_XORed_ori_encoding_matrix[base+i*_XORed_systemetical_chunk_num_+j]==1){
					XOR_buffer(target,content+j*chunk_size,chunk_size);
				}
			}
		}
	}else{
		int index=0;
		while(1){
			if(_encoding_schedule[index][0]!=-1){
				if(_encoding_schedule[index][0]<_conf_k_){
					XOR_buffer(buffer+((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][3])*chunk_size,
							content+(_encoding_schedule[index][0]*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][1])*chunk_size,
							chunk_size);
				}else{
					XOR_buffer(buffer+((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][3])*chunk_size,
							buffer+((_encoding_schedule[index][0]-_conf_k_)*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][1])*chunk_size,
							chunk_size);
				}
				index++;
			}else{
				break;
			}
		}
	}
	return 0;
}

int ProductMatrix::encode3(char* content,char* buffer, int length){
	if(_XOR_flag!=1){
		int chunk_size=length/_systemetical_chunk_num_;
		int base=_systemetical_chunk_num_*_systemetical_chunk_num_;
		int start=_fake_num*_chunk_number_per_node_;
		for(int i=0;i<_encoded_chunk_num_;i++){
			char* target=buffer+i*chunk_size;
			for(int j=start;j<_systemetical_chunk_num_;j++){
				if(_SSE_flag==0){
					galois_w08_region_multiply(content+(j-start)*chunk_size,
							_ori_encoding_matrix[base+
								i*_systemetical_chunk_num_+j],
							chunk_size,target,1);
				}else{
					ff_add_mulv_local((uint8_t*)target,
							(uint8_t*)content+(j-start)*chunk_size,
							_ori_encoding_matrix[base+
								i*_systemetical_chunk_num_+j],
							chunk_size);
				}
			}
		}
		return 0;
	}

	int chunk_size=length/(_XORed_systemetical_chunk_num_-
            _fake_num*_XORed_chunk_number_per_node_);
	int base=_XORed_systemetical_chunk_num_*_XORed_systemetical_chunk_num_;
	int start=_fake_num*_XORed_chunk_number_per_node_;

	if(_schedule_flag==0){
		for(int i=0;i<_XORed_encoded_chunk_num_;i++){
			char* target=buffer+i*chunk_size;
			for(int j=start;j<_XORed_systemetical_chunk_num_;j++){
				if(_XORed_ori_encoding_matrix[base+i*_XORed_systemetical_chunk_num_+j]==1){
					XOR_buffer(target,content+(j-start)*chunk_size,chunk_size);
				}
			}
		}
	}else{
		int index=0;
		while(1){
			if(_encoding_schedule[index][0]!=-1){
                //printf("%4d:%4d%4d%4d%4d%4d\n",index,
                //        _encoding_schedule[index][0],
                //        _encoding_schedule[index][1],
                //        _encoding_schedule[index][2],
                //        _encoding_schedule[index][3],
                //        _encoding_schedule[index][4]
                //        );
                if(_encoding_schedule[index][0]<(_conf_k_-_real_k)){
                    index++;
                    continue;
                }
				if((_encoding_schedule[index][0]<_conf_k_)){
                    //printf("1:%d %d\n",((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_+
					//			_encoding_schedule[index][3])*chunk_size,
                    //        (_encoding_schedule[index][0]*_XORed_chunk_number_per_node_+
					//			_encoding_schedule[index][1]-start)*chunk_size);
					XOR_buffer(buffer+((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][3])*chunk_size,
							content+(_encoding_schedule[index][0]*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][1]-start)*chunk_size,
							chunk_size);
				}else{
                    //printf("2:%d %d\n",((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_+
					//			_encoding_schedule[index][3])*chunk_size,
                    //        ((_encoding_schedule[index][0]-_conf_k_)*_XORed_chunk_number_per_node_+
					//			_encoding_schedule[index][1])*chunk_size);
					XOR_buffer(buffer+((_encoding_schedule[index][2]-_conf_k_)*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][3])*chunk_size,
							buffer+((_encoding_schedule[index][0]-_conf_k_)*_XORed_chunk_number_per_node_+
								_encoding_schedule[index][1])*chunk_size,
							chunk_size);
				}
				index++;
			}else{
				break;
			}
		}
	}
	return 0;
}
