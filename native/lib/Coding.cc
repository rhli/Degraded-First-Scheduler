# include "Coding.hh"
# define EXPERIMENT 1

int Coding::XOR_buffer(char* des, char* src, int length){
	//call ff functions
	long* ides=(long*)des;
	long* isrc=(long*)src;
	int ilength=length/sizeof(long);
	for(int i=0;i<ilength;i++){
		ides[i]^=isrc[i];
	}
	return 0;
}

char* Coding::itoa(int value,char* des,int radix){
	if(value<10){
		des[0]='0'+value;
	}else if(value<100){
		des[0]='0'+value/10;
		des[1]='0'+value%10;
	}else if(value<1000){
		des[0]='0'+value/100;
		des[1]='0'+(value%100)/10;
		des[2]='0'+value%10;
	}else{
		printf("itoa error: %d\n",value);
		exit(0);
	}
	return des;
}

int Coding::generate_inverse_table(){
	_inverse_table=(int*)calloc(pow(2,_conf_w_),sizeof(int));
	for(int i=1;i<pow(2,_conf_w_);i++){
		for(int j=1;j<pow(2,_conf_w_);j++){
			if(galois_single_multiply(i,j,_conf_w_)==1){
				_inverse_table[i]=j;
				break;
			}
		}
	}
	return 0;
}

int Coding::square_cauchy_matrix(int * des,int size){
    int* Xset=(int*)calloc(size,sizeof(int));
    int* Yset=(int*)calloc(size,sizeof(int));
    for(int i=0;i<size;i++){
        Xset[i]=i+1;
        Yset[i]=i+1+size;
    }
    for(int i=0;i<size;i++){
        for(int j=0;j<size;j++){
            des[i*size+j]=_inverse_table[Xset[i]^Yset[j]];
        }
    }
    free(Xset);
    free(Yset);
    return 0;
}

int Coding::inverse_matrix(int * original,int *des, int size){
	int *workspace=(int*)calloc(size*size*2,sizeof(int));
	for(int i=0;i<size;i++){
		for(int j=0;j<size;j++){
			workspace[i*size*2+j]=original[i*size+j];
		}
		workspace[i*size*2+i+size]=1;
	}
	for(int i=0;i<size;i++){
		//Make the left part an identical matrix
		int base=workspace[i*size*2+i];
		if(base==0){
			for(int j=i;j<size;j++){
				if(workspace[j*2*size+i]!=0){
					for(int k=0;k<2*size;k++){
						workspace[i*2*size+k]^=workspace[j*2*size+k];
					}
					base=workspace[i*size*2+i];
					break;
				}
			}
			if(base==0){
				printf("Not an invertable matrix\n");
				return 1;
			}
		}
		for(int j=0;j<size;j++){
			if((j!=i)&&(workspace[j*size*2+i]!=0)){
				int factor=galois_multiply(workspace[j*size*2+i],_inverse_table[base]);
				for(int k=i;k<2*size;k++){
					workspace[j*size*2+k]^=galois_multiply(factor,workspace[i*size*2+k]);
				}
			}
		}
		int factor=_inverse_table[base];
		for(int j=i;j<size*2;j++){
			workspace[i*size*2+j]=galois_multiply(factor,workspace[i*size*2+j]);
		}
	}
	for(int i=0;i<size;i++){
		for(int j=0;j<size;j++){
			des[i*size+j]=workspace[i*size*2+j+size];
		}
	}
	free(workspace);
	int *res=matrix_multiply(original,des,size);
	//show_matrix(res,size,size);
	return 0;
}

int* Coding::matrix_multiply(int* mat1,int* mat2,int size){
	int* des=(int*)calloc(size*size,sizeof(int));
	for(int i=0;i<size;i++){
		for(int j=0;j<size;j++){
			for(int k=0;k<size;k++){
				des[i*size+j]^=galois_multiply(mat1[i*size+k],mat2[k*size+j]);
			}
		}
	}
	return des;
}

int* Coding::matrix_multiply2(int* mat1,int* mat2,int row,int column,int mcolumn){
	int* des=(int*)calloc(row*column,sizeof(int));
	for(int i=0;i<row;i++){
		for(int j=0;j<column;j++){
			for(int k=0;k<mcolumn;k++){
				des[i*column+j]^=galois_multiply(mat1[i*mcolumn+k],mat2[k*column+j]);
			}
		}
	}
	return des;
}

int Coding::show_matrix(int *mat, int row_num, int column_num){
	for(int i=0;i<row_num;i++){
		for(int j=0;j<column_num;j++){
			printf("%4d",mat[i*column_num+j]);
		}
		printf("\n");
	}
	printf("\n");
	return 0;
}

int Coding::round_file_size(int filelength){
	if(filelength%(_systemetical_chunk_num_*sizeof(long))==0){
		_chunk_size_=filelength/_systemetical_chunk_num_;
		return filelength;
	}else{
		_chunk_size_=(filelength/(_systemetical_chunk_num_*sizeof(long))+1)*sizeof(long);
		return _chunk_size_*_systemetical_chunk_num_;
	}
}

int Coding::buffer_cal1(char* des,char* src,int* equations,
        int desNum,int srcNum,
        int symbolSize){
    /*
     * This is simply based on the calculation of galois field operations.
     */
    for(int i=0;i<srcNum;i++){
        char* srcBuffer=src+i*symbolSize;
        for(int j=0;j<desNum;j++){
            int coefficient=equations[j*srcNum+i];
            if(coefficient!=0){
                galois_w08_region_multiply(srcBuffer,
                        coefficient,
                        symbolSize,
                        des+j*symbolSize,
                        1);
            }
        }
    }
    return 0;
}

int Coding::buffer_cal2(char* des,char* src,int* equations,
        int desNum,int srcNum,
        int symbolSize){
    /*
     * This is XOR operations.
     */
    for(int i=0;i<srcNum;i++){
        char* srcBuffer=src+i*symbolSize;
        for(int j=0;j<desNum;j++){
            equations[j*srcNum+i];
            if(equations[j*srcNum+i]!=0){
                XOR_buffer(des+j*symbolSize,srcBuffer,symbolSize);
            }
        }
    }
    return 0;
}

int Coding::buffer_cal3(char* des,char* src,int** schedule,
        int desNum,int srcNum,
        int symbolSize){
    /*
     * This is based on XOR scheduling.
     * The srcNum and desNum is equal to the values in the buffers_cal1()
     */
	int index=0;
	while(1){
		if(schedule[index][0]!=-1){
			if(schedule[index][0]<srcNum){
                //if(schedule[index][4]==0){
                //    /*
                //     * Just memcpy()
                //     */
                //    puts("HERE");
                //    memcpy(des+((schedule[index][2]-srcNum)*_conf_w_+
				//			    schedule[index][3])*symbolSize,
				//		    src+(schedule[index][0]*_conf_w_+
				//			    schedule[index][1])*symbolSize,
				//		    symbolSize);
                //}
				XOR_buffer(des+((schedule[index][2]-srcNum)*_conf_w_+
							schedule[index][3])*symbolSize,
						src+(schedule[index][0]*_conf_w_+
							schedule[index][1])*symbolSize,
						symbolSize);
			}else{
                //if(schedule[index][4]==0){
                //    /*
                //     * Just memcpy()
                //     */
                //    puts("HERE");
                //    memcpy(des+((schedule[index][2]-srcNum)*_conf_w_+
				//			    schedule[index][3])*symbolSize,
				//		    des+((schedule[index][0]-srcNum)*_conf_w_+
				//			    schedule[index][1])*symbolSize,
				//		    symbolSize);
                //}
				XOR_buffer(des+((schedule[index][2]-srcNum)*_conf_w_+
							schedule[index][3])*symbolSize,
						des+((schedule[index][0]-srcNum)*_conf_w_+
							schedule[index][1])*symbolSize,
						symbolSize);
			}
			index++;
		}else{
			break;
		}
	}
    return 0;
}

int Coding::show_shedule(int** schedule){
    int index=0;
    while(schedule[index][0]!=-1){
        printf("%4d:%4d%4d%4d%4d%4d\n",index,
                schedule[index][0],
                schedule[index][1],
                schedule[index][2],
                schedule[index][3],
                schedule[index][4]
                );
        index++;
    }
    return 0;
}

int Coding::is_failed(int index){
	for(int i=0;i<_conf_f_;i++){
		if(index==_failed_node_list[i]){
			return 1;
		}
	}
	return 0;
}

int Coding::show_failed_nodes(){
	for(int i=0;i<_conf_f_;i++){
		printf("%4d",_failed_node_list[i]);
	}
	printf("\n");
	return 0;
}

Coding::Coding(){
    _XOR_flag=1;
    _schedule_flag=1;
	_thread_num=8;
	_failed_node_list=NULL;
	_downloading_chunk_list=NULL;
}

