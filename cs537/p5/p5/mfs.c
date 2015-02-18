#include "mfs.h"
#include <stdio.h>
#include "udp.h"
#include <string.h>
#include "msg.h"



struct sockaddr_in saddr;
int sd;
//int server_connected = 0;

int MFS_Init(char* hostname , int port){
	 
//	if(server_connected != 0)
//		return -1;

//	server_connected = 1; 

	sd = UDP_Open(0);
	//assert(sd > -1);	
	if(sd <= -1){
		fprintf(stderr, "Invalid port\n");
		return -1;
	}
	int rc = UDP_FillSockAddr(&saddr, hostname, port);
	//assert(rc == 0);
	if(rc != 0){
		fprintf(stderr, "Invalid hostname\n");
		return -1;
	}


	return 0;
}

int send_message (msg_t *  msg){


		 int   rc = UDP_Write(sd, &saddr, (char*)msg, sizeof( msg_t));
//	         printf("CLIENT:: sent message (%d)\n", msg->func_type);
	 
	         struct timeval t;
	         t.tv_sec=10;
	         t.tv_usec=1;
	         fd_set r;
	         FD_ZERO(&r);
	         FD_SET(sd, &r);
	 
	         rc=select(sd+1,&r,NULL,NULL,&t);
	         if (rc <= 0){
	                 printf("timeout \n");
			 return -1; 
	 	 }
		 else if (rc > 0) {
	                 struct sockaddr_in raddr;
	                 UDP_Read(sd, &raddr, (char*)msg,sizeof(msg_t));
	                 //printf("CLIENT:: read %d bytes (message: '%d')\n", nbyte, msg->type);
		 }


	        return 0;

}

int inum_check (int inum){

	if( inum < 0 || inum > 4095){
	   fprintf(stderr, "Invalid pinum\n");
	   return -1;
	}
	return 0;

}

int block_check(int block){
	
		
	if(block < 0 || block > 13){
		fprintf(stderr, "Invalid block num\n");
		return -1;
	}
	return 0;


}





int MFS_Lookup(int pinum, char *name){


		if(inum_check(pinum) < 0)
			return -1;

		if( strlen(name) > 59)
			return -1;

		msg_t * msg = (msg_t * )malloc(sizeof(msg_t));
		msg->func_type = 1;
		msg->pinum = pinum;
		strncpy(msg->buf, name, 60);	
		
		int rc;
	       	do{	
			rc=send_message(msg);
		}while(rc == -1);

//		printf("rc: %d\n", rc);

		
//	        printf(" LOOKUP: inum found '%d'\n", msg->inum);
		 
		memset(msg->buf, '\0', sizeof(name));
		int inum_return = msg->inum;
		free(msg);

		return inum_return;

}
int MFS_Stat(int inum, MFS_Stat_t *m){
		
		if(inum_check(inum) < 0)
			return -1;

		msg_t * msg = (msg_t * )malloc(sizeof(msg_t));
		msg->func_type = 2;
		msg->inum = inum;
		msg->type = -1;
		msg->size = -1;
		
		int rc;
	       	do{	
			rc=send_message(msg);
		}while(rc == -1);

//		printf("rc: %d\n", rc);
	       printf("Stat Inode: %d\n", inum);
	       	printf("LIB:: Return from send packet: (msg size '%d')\n", msg->size);
	        printf("LIB:: Return from send packet: (msg type: '%d')\n", msg->type);
		
		if(msg->rc != -1){
			m->size = msg->size;
			m->type = msg->type;
		}

		memset(msg->buf, '\0', 4096);
		rc = msg->rc;
		free(msg);
		return rc;
		


		

	
	

}
int MFS_Write(int inum, char *buffer, int block){
	
		if(inum_check(inum) < 0)
			return -1;

		if(sizeof(buffer) > 4096){
			printf("size of buffer: %zu\n", sizeof(buffer));
			return -1;
		}

		if(block_check(block) < 0)
			return -1;
			
		msg_t * msg = (msg_t * )malloc(sizeof(msg_t));
		msg->func_type = 3;
		msg->inum = inum;
		memcpy(msg->buf, buffer, MFS_BLOCK_SIZE);
		msg->block = block;
		int rc;	
		printf("Write Block: %d\n", block);
	       	do{	
			rc=send_message(msg);
		}while(rc == -1);

		memset(msg->buf, '\0', 4096);
//	printf("rc: %d\n", rc);
		
		rc = msg->rc;
		free(msg);
		return rc;


}
int MFS_Read(int inum, char *buffer, int block)
{
		if(inum_check(inum) < 0)
			return -1;

		if(block_check(block) < 0)
			return -1;
		
		msg_t * msg = (msg_t * )malloc(sizeof(msg_t));
		msg->func_type = 4;
		msg->inum = inum;
		msg->block = block;
		printf("Read Block: %d\n", block);
		
		int rc;

	       	do{	
			rc=send_message(msg);
		}while(rc == -1);

//		printf("rc: %d\n", rc);
		
		memcpy(buffer, msg->buf, MFS_BLOCK_SIZE);

		rc = msg->rc;
		free(msg);
		return rc;
		

 }
int MFS_Creat(int pinum, int type, char *name)
{ 
		if(inum_check(pinum) < 0)
			return -1;

		if(strlen(name) > 59)
			return -1;

		msg_t * msg = (msg_t * )malloc(sizeof(msg_t));
		msg->func_type = 5;
		msg->pinum = pinum;
		msg->type = type;

//	printf("Create: %s  Type: %d\n", name, type);

		strncpy(msg->buf, name, 60);
		
		int rc;
	       	do{	
			rc=send_message(msg);
		}while(rc == -1);

		
		memset(msg->buf, '\0', sizeof(name));
	//	printf("rc: %d\n", rc);
	
		rc = msg->rc;
		free(msg);
		return rc;
	 }
int MFS_Unlink(int pinum, char *name)
{ 
		if(inum_check(pinum) < 0)
			return -1;

		if(strlen(name) > 59)
			return -1;

//		printf("Unlink: %s\n", name);
		
		msg_t * msg = (msg_t * )malloc(sizeof(msg_t));
		msg->func_type = 6;
		msg->pinum = pinum;
		strncpy(msg->buf, name, 60);
	
		int rc;
	       	do{	
			rc=send_message(msg);
		}while(rc == -1);

		
		memset(msg->buf, '\0', 4096);
	//	printf("rc: %d\n", rc);
		rc = msg->rc;
		free(msg);
		return rc;

}
int MFS_Shutdown()
{ 

//		if(server_connected != 1)
//			return -1;


		msg_t * msg = (msg_t * )malloc(sizeof(msg_t));
		msg->func_type = 7;
		
		int rc;
	       	do{	
			rc=send_message(msg);
		}while(rc == -1);

		
//		server_connected = 0;
	//	printf("rc: %d\n", rc);
		rc = msg->rc;
		free(msg);
		return rc;
}
