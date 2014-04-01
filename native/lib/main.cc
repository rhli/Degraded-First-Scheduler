# include <stdio.h>
# include <stdlib.h>
# include "ProductMatrix.hh"

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

int main(int argc,char** argv){
	if(argc!=5){
		printf("Usage: %s (filename) (n) (k) (f)\n",argv[0]);
		exit(0);
	}
	//printf("%d %d %d %d\n",sizeof(char),sizeof(short),sizeof(int),sizeof(long));
	
	//ProductMatrix pm=ProductMatrix(atoi(argv[2]),atoi(argv[3]),8);
	//pm.generate_encoding_matrix();
	//pm.show_encoding_matrix();
	//int number=atoi(argv[4]);
	//int* list=(int*)calloc(number,sizeof(int));
	//enumerate_init(atoi(argv[2]),number,list);
	//int counter=0;
	////for(int i=0;i<number;i++){
	////	printf("%4d",list[i]);
	////}
	////printf("\n");
	//pm.set_f(number,list);
	//if(pm.multi_node_repair(number,list)==NULL){
	//	//for(int i=0;i<number;i++){
	//	//	printf("%4d",list[i]);
	//	//}
	//	//printf("\n");
	//	counter++;
	//}
	//while(enumerate_next(atoi(argv[2]),number,list)!=1){
	//	//for(int i=0;i<number;i++){
	//	//	printf("%4d",list[i]);
	//	//}
	//	//printf("\n");
	//	pm.set_f(number,list);
	//	if(pm.multi_node_repair(number,list)==NULL){
	//		//for(int i=0;i<number;i++){
	//		//	printf("%4d",list[i]);
	//		//}
	//		//printf("\n");
	//		counter++;
	//	}
	//	//pm.data_regeneration(argv[1],536870912);
	//}
	//printf("%d\n",counter);

	struct timeval t1,t2;
	ProductMatrix pm=ProductMatrix(atoi(argv[2]),atoi(argv[3]),8);
	pm.set_thread_number(1);
	int kv=atoi(argv[3]);
	int nv=atoi(argv[2]);
	int wv=8;
	int block_size=8192;
	pm.generate_encoding_matrix();
	int rfd=open(argv[1],O_RDONLY);
	//char* ibuffer=(char*)calloc(kv*(kv-1)*wv*block_size*sizeof(long),sizeof(char));
	char* ibuffer=(char*)calloc(kv*(kv-1)*wv*block_size*sizeof(long),sizeof(char));
	char* sample_result=(char*)calloc(nv*(kv-1)*wv*block_size*sizeof(long),sizeof(char));
	for(int i=0;i<3;i++){
		//printf("Reading buffer: length:%d, offset: %d\n",(kv-1)*wv*block_size*sizeof(long),i*67108864/1048576);
		pread(rfd,ibuffer+i*1048576,(kv-1)*wv*block_size*sizeof(long),i*67108864);
	}
	for(int i=0;i<5;i++){
		//printf("Reading buffer: length:%d, offset: %d\n",(kv-1)*wv*block_size*sizeof(long),i*67108864/1048576);
		pread(rfd,sample_result+i*1048576,(kv-1)*wv*block_size*sizeof(long),i*67108864);
	}
	
	int wfd1=open("original",O_RDWR|O_CREAT,0644);
	int wfd2=open("encoded",O_RDWR|O_CREAT,0644);
	int wfd3=open("output",O_RDWR|O_CREAT,0644);
	pwrite(wfd1,ibuffer,kv*(kv-1)*wv*block_size*sizeof(long),0);
	//pwrite(wfd4,sample_result,nv*(kv-1)*wv*block_size*sizeof(long),0);
	free(sample_result);
	
	float io_time;
	gettimeofday(&t1,NULL);
	char* obuffer=pm.encode(ibuffer,kv*(kv-1)*wv*block_size*sizeof(long));
	gettimeofday(&t2,NULL);
	io_time=(t2.tv_sec-t1.tv_sec)+(float)(t2.tv_usec-t1.tv_usec)/1000000;
	printf(" encode time:%f\n",io_time);
	pwrite(wfd2,obuffer,nv*(kv-1)*wv*block_size*sizeof(long),0);

	int number=1;
	int list[]={0,1,5,6,7};
	char** received_data=(char**)calloc(12,sizeof(char*));
	char* output;

	pm.set_f(number,list);
	for(int i=0;i<2*kv-1-number;i++){
		int wfd4;
		gettimeofday(&t1,NULL);
		//pm.set_f(number,list);
		received_data[i]=pm.encode_offline_recovery(obuffer+(kv-1)*wv*block_size*sizeof(long)+i*(kv-1)*wv*block_size*sizeof(long)
				,(kv-1)*wv*block_size*sizeof(long));
		gettimeofday(&t2,NULL);
		io_time=(t2.tv_sec-t1.tv_sec)+(float)(t2.tv_usec-t1.tv_usec)/1000000;
		printf(" encode offline:%f\n",io_time);
		pwrite(wfd4,received_data[i],wv*block_size*sizeof(long),0);
		close(wfd4);
	}

	gettimeofday(&t1,NULL);
	//pm.set_f(number,list);
	output=pm.reconstruct_lost_data(received_data,(kv-1)*wv*block_size*sizeof(long));
	gettimeofday(&t2,NULL);
	io_time=(t2.tv_sec-t1.tv_sec)+(float)(t2.tv_usec-t1.tv_usec)/1000000;
	printf(" recovery:%f\n",io_time);
	pwrite(wfd3,output,number*(kv-1)*wv*block_size*sizeof(long),0);

	close(wfd1);
	close(wfd2);
	close(wfd3);
	//close(wfd4);
	
	//pm.divide_and_encode_file(argv[1]);
	//int number=2;
	//int list[]={0,1,5,6,7};
	//pm.set_f(number,list);
	//pm.data_regeneration(argv[1],536870912);
	
	/*int failed_num;
	printf("input number of failed nodes\n");
	scanf("%d",&failed_num);
	int* failed_list=(int*)calloc(failed_num,sizeof(int));
	printf("input %d failed nodes\n",failed_num);
	for(int i=0;i<failed_num;i++){
		scanf("%d",failed_list+i);
	}*/

	//RS rs=RS(atoi(argv[2]),atoi(argv[3]),8);
	//rs.generate_encoding_matrix();
	//rs.divide_and_encode_file(argv[1]);
	//int number=4;
	//int list[]={3,4,5,6,7};
	//rs.set_f(number,list);
	//rs.data_regeneration(argv[1],536870912);
	//rs.resemble_file(argv[1],536870912);
	return 0;
}
