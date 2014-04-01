/*

	 rewrite by LIN Jian
	 $Date: 2012/07/10 $

 */
#include <stdlib.h>
#include "RS.hh"
#include "ProductMatrix.hh"

//JNI Header

/**
 * @param env
 * @param jobj
 * @param in in buffer
 * @param out out buffer
 * @param k coding scheme with k data block
 * @param m coding scheme with m code block
 * @param blockSize size of one block
 * */
/*
JNIEXPORT void JNICALL Java_org_apache_hadoop_raid_PMEncoder_00024PMMigrationEncoder_pmEncode
(JNIEnv * env, jobject obj, jobject in, jobject out, jint k, jint m, jint blockSize){
*/
int main(){
	int k = 3;
	int m = 2;
	//array structure
	//first four byte: int sequence number
	//following one byte: byte 1 for terminating the task
	//following k*blocksize/m*blocksize byte: the data

	//To get a byte array
	//Initialize a ProductMatrix Object
	RS rs=RS(k+m,k,8);
	rs.generate_encoding_matrix();
	/*
	int whole_length=blockSize*k;
	int index;
	char flag;
	while(1){
		jclass cls = env->GetObjectClass(obj);
		jmethodID mid = env->GetMethodID(cls, "take", "()V");
		if (NULL == mid)
		{
			return;
		}
		env->CallVoidMethod(obj, mid);
		void * inData = (env)->GetDirectBufferAddress(in);

		//To push a byte array
		cls = env->GetObjectClass(obj);
		mid = env->GetMethodID(cls, "push", "()V");
		if (0 == mid)
		{
			return;
		}
		flag=((char*)inData)[4];
		void * outData = (env)->GetDirectBufferAddress(out);
		//rs.encode2((char*)inData+5,(char*)outData+4,whole_length);
		memcpy((char*)outData,(char*)inData,4);
		env->CallVoidMethod(obj, mid);
		if(flag==1){
			return;
		}
	}
	*/
	return 0;
}
