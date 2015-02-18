#include <stdio.h>
#include <stdlib.h>
#include "udp.h"
#include "msg.h"
#include "mfs.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>


#define BUFFER_SIZE (4096)



struct sockaddr_in s;
int fd;
MFS_Chk_t * chk_region;
int next_inum = 1;
int sd;


int init_chkpt_region(){


	//allocate and fill checkpoint region with initial values//
	chk_region = (MFS_Chk_t *)malloc(sizeof(MFS_Chk_t));

	int i ;
	int j;

	//initialize array of seg offsets to -1
	for(i = 0; i < 256; i++){
		chk_region->imap_seg_addr[i] = (off_t)-1;
	}

	//initialize ptrs to array of inode addresses
	for(i = 0; i < 256; i++){
		chk_region->imap[i] = (MFS_Imap_Seg_t *)malloc(sizeof(MFS_Imap_Seg_t));
		for(j = 0; j < 16; j ++){
			(chk_region->imap[i])->imap_seg[j] = (off_t) -1;
		}	
	}

	chk_region->log_end = (sizeof(off_t)*256)+ sizeof(off_t); //size of imap array + log_end variable
	return 0;

}

//always writes at beginning of file
off_t write_chk_region_to_file(){

	//seek to beginning of file
	lseek(fd, (off_t)0, SEEK_SET);	
	//	printf("begin of file: %zu\n", pos);

	//write log end to file//
	int bw = write(fd, &(chk_region->log_end), sizeof(off_t));
	//	printf("log end bytes written : %d\n", bw);

	//write imap_seg_addr array
	lseek(fd, sizeof(off_t), SEEK_SET);
	bw =write(fd, (chk_region->imap_seg_addr), (sizeof(off_t) * 256));

	//	printf(" imap array bytes written : %d\n, ", bw);
	//	printf("size written : %zu\n ", sizeof(off_t)+(sizeof(off_t)*256) );

	if(bw <= 0){
		return -1;
	}else{
		fsync(fd);
	}

	return 0;

}

int  create_dir_block( char* name, int pinum, int inum, off_t pos, int first_block, int new_block ){

	//create a 4KB block of directory entries
	MFS_DirEnt_t direcs[MFS_DIR_ENTRIES];
	//	printf("direcs block size: %zu\n", sizeof(direcs));

	//init directory entries
	int i;	
	for (i = 0; i < 64; i++){
		direcs[i].inum = -1;
	}

	//ensure only the first block has
	// "." and ".." dir entries
	if(first_block){

		//root
		strcpy(direcs[0].name, ".\0");
		direcs[0].inum = inum;


		//parent
		strcpy(direcs[1].name, "..\0");
		direcs[1].inum = pinum;

	}

	if(new_block){

		strcpy(direcs[0].name, name);
		direcs[0].inum = inum;
	 	
	}


	//seek to log_end & write directory block
	lseek(fd, pos, SEEK_SET);
	int bw = write(fd, direcs, MFS_BLOCK_SIZE); 

	if(bw <= 0){
		return -1;
	}else{
		//		printf("size written : %zu\n ",sizeof(direcs));
//		fsync(fd);
		chk_region->log_end += (MFS_BLOCK_SIZE);
	}

	return 0;
}

int create_inode(off_t first_block_addr, int type, off_t last_byte, off_t pos){

	//create inode
	MFS_Inode_t inode; 

	inode.type = type;

	//assign size based on type
	if(type == MFS_DIRECTORY){
		inode.size = MFS_BLOCK_SIZE;
	}else{
		inode.size = last_byte; 
	}

	//assign first ptr
	inode.directptrs[0] = first_block_addr;

	//init unsed ptrs to -1
	int i;
	for(i = 1; i < 14; i ++){
		inode.directptrs[i] = (off_t)-1;
	}	

	//write inode to file
	lseek(fd, pos, SEEK_SET);
	//	printf("inode offest (in creat_inode func): %ld\n", pos);
	int bw = write(fd, &inode, sizeof(MFS_Inode_t));

	if(bw <= 0){
		return -1;
	}else{
//		fsync(fd);
		chk_region->log_end +=(sizeof(MFS_Inode_t));

	}
	return 0;

}

int update_chk_region(int inum, off_t inode_addr, off_t pos){

	int imap_index = inum/16;
	int seg_index = inum%16;

	//update imap segment (with new address of inode) in memory
	(chk_region->imap[imap_index])->imap_seg[seg_index] = inode_addr;


	//write updated imap segment to file
	off_t seg_addr = lseek(fd, pos, SEEK_SET);
	write(fd, (chk_region->imap[imap_index])->imap_seg, sizeof(MFS_Imap_Seg_t));
//	fsync(fd);

	//update imap segment addr in chk_region memory
	chk_region->imap_seg_addr[imap_index] = seg_addr;

	//update log end
	chk_region->log_end +=sizeof(MFS_Imap_Seg_t);

	return 0;
}

off_t find_inode_offset(int inum){

	int imap_index = inum/16;
	int seg_index = inum%16;

	if(chk_region->imap_seg_addr[imap_index] == -1)
		return -1;

	if(chk_region->imap[imap_index] == NULL){
		fprintf(stderr, "Null Pointer Err- Should not occur\n");
		return -1;
	}

	off_t inode_offset = (chk_region->imap[imap_index])->imap_seg[seg_index];

	if(inode_offset ==-1)
		printf("inode does not exist\n");


	return inode_offset;
}


int server_lookup(int * inum, int pinum, char* name){

	//	printf("entered server lookup\n");
	//	printf("parent inum: %d\n", pinum);

	//preset inum to -1, in the case we return
	//without finding the inode number
	*inum = -1; 


	//find offset of inode of parent directory

	off_t inode_offset = find_inode_offset(pinum);

	if(inode_offset == (off_t) -1){
		return -1;
	}

	// read in inode

	MFS_Inode_t  temp_inode; 
	lseek(fd, inode_offset, SEEK_SET);
	read(fd, &temp_inode, sizeof(MFS_Inode_t));

	//verify parent is directory 
	if(temp_inode.type != MFS_DIRECTORY)
		return -1;

	//read directory entries for file/direc "name"
	MFS_DirEnt_t direcs[MFS_BLOCK_SIZE];

	int i;
	int found = 0;

	//loops through direct pointers
	for(i = 0; i < 14; i++){

		if(found == 1 )
			break;

		off_t dir_bloc = temp_inode.directptrs[i];
		
		if(dir_bloc == (off_t) -1)
			continue;
		
		lseek(fd, dir_bloc, SEEK_SET);
		read(fd, &direcs,MFS_BLOCK_SIZE);


		int j;

		//loops thru each entry,
		for(j =0; j < 64; j++){

			//continue if  unused directory ent is found;
			if(direcs[j].inum == -1)
				continue;



			//if name found,  return inum
			if( strcmp(direcs[j].name, name ) == 0){

				found = 1;
				*inum = direcs[j].inum;
				break;
			}

		}
	}	

//	fprintf(stderr, "LOOKUP NAME: %s; INUM: %d\n", name, *inum);

	return 0;

}

int server_stat(int inum, int * type, int * size){

	//find offset if inode
	off_t inode_offset = find_inode_offset(inum);

	if(inode_offset == (off_t) -1)
		return -1;



	//direc exists, read in inode to mem
	MFS_Inode_t temp_inode; 
	lseek(fd, inode_offset, SEEK_SET);
	read(fd, &temp_inode, sizeof(MFS_Inode_t));

	//return type
	*type = temp_inode.type;
	*size = temp_inode.size;

	return 0;

}

int server_write(int inum, char* buffer, int block){

//	printf("TO WRITE: %s\n", buffer);

	//find and read file inode
	off_t inode_offset = find_inode_offset(inum);

	if(inode_offset == (off_t) -1)
		return -1;

	MFS_Inode_t temp_inode;

	lseek(fd, inode_offset, SEEK_SET);
	read(fd, &temp_inode, sizeof(MFS_Inode_t));

	//cannot write to a directory
	if(temp_inode.type == MFS_DIRECTORY){
		fprintf(stderr, "Cannot write to directory\n");
		return -1;
	}

	//write block to log end
	lseek(fd, chk_region->log_end, SEEK_SET);
	 write(fd, buffer , MFS_BLOCK_SIZE);
//	fsync(fd);

	//update size
	if((block +1)*MFS_BLOCK_SIZE > temp_inode.size){
		temp_inode.size = ( block +1 )*MFS_BLOCK_SIZE;
	}


	//update inode
	temp_inode.directptrs[block] = chk_region->log_end;  
	chk_region->log_end+= MFS_BLOCK_SIZE; 



	//write updated inode 
	inode_offset = 	lseek(fd, chk_region->log_end, SEEK_SET);
	write(fd, &temp_inode , sizeof(MFS_Inode_t));
//	fsync(fd);

	chk_region->log_end+= sizeof(MFS_Inode_t);

	update_chk_region(inum, inode_offset, chk_region->log_end); 

	//update checkp point region
	write_chk_region_to_file();
	
	return 0;

}

int server_read(int inum, char* buffer, int block){

	//find inode and read inode

	off_t inode_offset = find_inode_offset(inum);

	if(inode_offset == (off_t) -1)
		return -1;

	MFS_Inode_t temp_inode;

	lseek(fd, inode_offset, SEEK_SET);
	read(fd, &temp_inode, sizeof(MFS_Inode_t));

	//verify block exists & read into buffer
	off_t block_offset;
	if((block_offset = temp_inode.directptrs[block]) == (off_t) -1){
		fprintf(stderr, "Cannot read from this location\n");
		return -1;
	}

	lseek(fd, block_offset, SEEK_SET);
	read(fd, buffer, MFS_BLOCK_SIZE);
//	printf("READ: %s\n", buffer);


	return 0;

}

int new_inum(){

	int i;
	int j;
	int inum = 0;
	for(i = 0; i < 256; i++){
		for(j = 0; j < 16; j++){
			
			if((chk_region->imap[i])->imap_seg[j] == -1){
				return inum = (16 * i) + j;
			}
		}
	}
	return -1;

}

int server_creat(int pinum, int type, char* name){


//	printf("Creating %s in directory %d\n", name, pinum);
	int rc;
	MFS_Inode_t temp_inode;

	//nd inode and read parent inode
	off_t inode_offset = find_inode_offset(pinum);

	if(inode_offset == -1)
		return -1;

	lseek(fd, inode_offset, SEEK_SET);
	read(fd, &temp_inode, sizeof(MFS_Inode_t));

	//verify parent is a directory	
	if(temp_inode.type == MFS_REGULAR_FILE){
		fprintf(stderr, "Parent not a directory");
		return -1;
	}

	//call lookup to determine if file already exists
	int inum_if_exists; 
	rc = server_lookup(&inum_if_exists, pinum, name);

	//return zero (success) if file already exists
	if(inum_if_exists != -1)
		return 0;


	//get new inode number
	int new_inode = new_inum();
/*	if(new_inode == 0){
		
		int l;
		for( l = 0; l < 256; l++){
			printf( " index[%d] %ld :  ",l, (long signed int)chk_region->imap_seg_addr[l] );

				int k;
				printf("imap seg %d [" , l);
				for(k=0; k < 16; k++){
					printf ("%ld, ", (chk_region->imap[l])->imap_seg[k]);	
				}
				printf(" ]\n");
			}
		
	
		exit(0);
	}

*/
//		fprintf(stderr, "FILE: %s INUM ASSIGNED : %d\n",name, new_inode);
	if(new_inode == -1){
		fprintf(stderr, "Max Inodes reached\n");
		return -1;
	}

	off_t dir_block_addr;
	off_t c_inode_addr;
	off_t p_inode_addr;


	//Create Directory
	if(type == MFS_DIRECTORY){

		//create directory block
		dir_block_addr = chk_region->log_end;
		rc = create_dir_block(0, pinum, new_inode, chk_region->log_end, 1, 0);
		assert(rc != -1);

		//create inode
		c_inode_addr = chk_region->log_end;
		rc = create_inode(dir_block_addr, MFS_DIRECTORY, MFS_BLOCK_SIZE, chk_region->log_end);	
		assert(rc != -1);


	}
	//Create Empty Regular File
	else{


		//create inode
		c_inode_addr = chk_region->log_end;
		rc = create_inode((off_t)-1,MFS_REGULAR_FILE, 0, chk_region->log_end);	
		assert(rc != -1);


	}       
	//force to file
//	fsync(fd);

	//check parent to verify there is space in parent direc 

	int i;
	int j;
	int found = 0;
	MFS_DirEnt_t direcs[MFS_DIR_ENTRIES];
	off_t dir_bloc;
	int update_this_ptr;
	off_t direc_block;
	int new_bloc_alloced = 0;

	//loops through direct pointers to find free directory entry
	for(i = 0; i < 14; i++){

		if(found == 1)
			break;
		//create new dir block and add file to it
		if( (i != 0 ) && (dir_bloc = temp_inode.directptrs[i]) == -1){
			direc_block = chk_region->log_end;
			update_this_ptr = i;
			create_dir_block(name, pinum, new_inode, direc_block,0,1);
			found = 1;
			new_bloc_alloced = 1;
			break;
			
		}else if((i == 0) && (dir_bloc = temp_inode.directptrs[i]) == -1){

			fprintf(stderr, "BAD MOJO: error constructing a direc\n");
			return -1;
			
		}

		//go to direc block location for reading
		lseek(fd, dir_bloc, SEEK_SET);
		read(fd, direcs, sizeof(direcs));

		//loops thru direc block and reads each entry,
		for(j =0; j < 64; j++){

			//check for free entry
			if ( direcs[j].inum == -1){
				found = 1;
				update_this_ptr = i;
				direcs[j].inum = new_inode;
				strncpy(direcs[j].name, name, 60);
				break;
			}
		}

	}

	//no free space in parent
	if (found == 0){
		fprintf(stderr, "BLOCK: %d, ENTRY: %d\n", i, j);
		//loops through direct pointers to find free directory entry
/*		printf("ENTIRE DIRECTORY\n");
	for(i = 0; i < 14; i++){



		//create new dir block and add file to it
		if(( dir_bloc = temp_inode.directptrs[i]) == -1){


			printf("Unused Directory\n");	
			continue;
		}			

		//go to direc block location for reading
		lseek(fd, dir_bloc, SEEK_SET);
		read(fd, direcs, sizeof(direcs));

		//loops thru direc block and reads each entry,
		for(j =0; j < 64; j++){

			printf("ENTRY %d: [ %s, %d]", (64*i)+j, direcs[j].name, direcs[j].inum);
		
		}

	}


*/

		write_chk_region_to_file(); //really only to update log end 
		fprintf(stderr,"No space in parent directory\n");
		return -1;		

	}

	if( !new_bloc_alloced){	


	//write updated directory block for parent
	direc_block = chk_region->log_end;
	lseek(fd, chk_region->log_end, SEEK_SET);
	write(fd, direcs, sizeof(direcs));
	chk_region->log_end+= sizeof(direcs);
	
	}else{
	
	temp_inode.size+= MFS_BLOCK_SIZE;
	}

	//update parent inode ptr & write updated inode	
	p_inode_addr = chk_region->log_end;
	temp_inode.directptrs[update_this_ptr] = direc_block;
	
	lseek(fd, chk_region->log_end, SEEK_SET);
	write(fd, &temp_inode, sizeof(MFS_Inode_t));
//	fsync(fd);

	chk_region->log_end+= sizeof(MFS_Inode_t);

	//update parent info
	update_chk_region(pinum, p_inode_addr, chk_region->log_end);

	//update child info
	update_chk_region(new_inode, c_inode_addr, chk_region->log_end);


	//update checkp point region
	write_chk_region_to_file();


	return 0 ;

}

int not_root_or_parent(char * name){



	int is_root =  strcmp(name, ".\0");
	int is_parent = strcmp(name, "..\0");
	
	if( is_root == 0 || is_parent == 0)
		return 0;

	return 1;

}

int server_unlink( int pinum, char* name){

//	fprintf(stderr, "NAME: %s\n", name);
	int inum;
	int rc;
	int i;
	int j;
	int empty = 1;
	off_t dir_bloc;
	int found = 0;
	int update_this_ptr;

	//temp structs entry for reading
	MFS_DirEnt_t  direcs[MFS_DIR_ENTRIES];
	MFS_Inode_t temp_inode;
	
	//verify parent exists 
	off_t p_inode_offset = find_inode_offset(pinum);

	if( p_inode_offset == -1)
		return -1;

	//verify if file exists in that directory 
	rc = server_lookup(&inum, pinum, name);
//	fprintf(stderr, "inum of name: %d\n", inum);

	if(inum == -1)
		return 0; // name does not exist in directory


	//find and read child inode -- just in case its 
	//a directory we need to check if it empty or not
	off_t c_inode_offset = find_inode_offset(inum);

	if(c_inode_offset == -1){
		fprintf(stderr, "Strange error since file should exist\n");
		return -1;
	}

	lseek(fd, c_inode_offset, SEEK_SET);
	read(fd, &temp_inode, sizeof(MFS_Inode_t));

//	fprintf(stderr, "TYPE: %d\n", temp_inode.type);



	//verify that the directory is non empty
	if(temp_inode.type == MFS_DIRECTORY ){

		//loops through direct pointers
		for(i = 0; i < 14; i++){

			if(empty == 0)
				break;

			if( (dir_bloc = temp_inode.directptrs[i]) == (off_t) -1)
				continue;

			//go to direc block location for reading
			lseek(fd, dir_bloc, SEEK_SET);
			read(fd, direcs, MFS_BLOCK_SIZE );

			//loops thru direc block and reads each entry,
			for(j =0; j < 64; j++){

				if( i = 0 && j < 2)
					continue;

				// terminate if one non-root/parent entry exists 
				if ( ( not_root_or_parent(direcs[j].name) && direcs[j].inum != -1)){
					empty = 0;
					break;
				}
			}

		}

	}
	if(!empty){
		fprintf(stderr, "Cannot delete non-empty directory heh\n");
		return -1;
	}


	//essentially lookup again but with parent
	//in order to delete reference 
	
	lseek(fd, p_inode_offset, SEEK_SET);
	read(fd, &temp_inode, sizeof(MFS_Inode_t));



	//loops through direct pointers to find correct block 
	for(i = 0; i < 14; i++){

		if(found == 1)
			break;

		if( (dir_bloc = temp_inode.directptrs[i]) == -1)
			continue;

		//go to direc block location for reading
		lseek(fd, dir_bloc, SEEK_SET);
		read(fd, direcs, MFS_BLOCK_SIZE);

		//loops thru direc block and reads each entry,
		int j;
		for(j =0; j < 64; j++){

			//"unlink" directory by setting inum = -1;
			if ( direcs[j].inum  == inum){
				found = 1;
				update_this_ptr = i;
				direcs[j].inum = -1;
				break;
			}
		}

	}

	//check that direc block has been emptied
	int is_empty = 1; //
	if(update_this_ptr != 0){

	for(j =0; j < 64; j++){
		 
			if ( direcs[j].inum  != -1 ){
				is_empty = 0;
				break;
			}
	}
	
	}else{
		is_empty == 0;
	}


	//if removing the file made an empty block,
	//decrease directory size
	if(is_empty){
		temp_inode.size-=MFS_BLOCK_SIZE;
		temp_inode.directptrs[update_this_ptr] = -1;
	}else{
		off_t direc_block = chk_region->log_end;
		
		//update parent direct ptr to new bblock
		temp_inode.directptrs[update_this_ptr] = direc_block;
	
	
		//write new directory block for parent
		lseek(fd, chk_region->log_end, SEEK_SET);
		write(fd, direcs, MFS_BLOCK_SIZE);
	
		chk_region->log_end+= MFS_BLOCK_SIZE;

	}

	//write updated inode	
	p_inode_offset  = chk_region->log_end;
	write(fd, &temp_inode, sizeof(MFS_Inode_t));

	chk_region->log_end+= sizeof(MFS_Inode_t);

	//update parent info

	//update_chk_region(inum, -1, chk_region->log_end);
	
	int imap_index = inum/16;
	int seg_index = inum%16;

	//update imap segment (with new address of inode) in memory
	(chk_region->imap[imap_index])->imap_seg[seg_index] = -1;

	is_empty = 1;
	for(i = 0; i < 16; i++){
	
	if ((chk_region->imap[imap_index])->imap_seg[i]  != -1){
		is_empty = 0;
	}

	}

	if(is_empty){
	
		chk_region->imap_seg_addr[imap_index] == -1;
	}


	update_chk_region(pinum, p_inode_offset, chk_region->log_end);
	write_chk_region_to_file();


	return 0;
}

int server_shutdown(){


	int l;
	for( l = 0; l < 256; l++){
		free(chk_region->imap[l]); 
	}

	free(chk_region);

//	fsync(fd);
	msg_t msg_reply;
	msg_reply.rc = 0;	

	UDP_Write(sd, &s, (char*)&msg_reply, sizeof(msg_t));
	UDP_Close(fd);
	exit(0);
}

	int
main(int argc, char *argv[])
{
	int port;
	char* file_sys_img; 

	//verify args
	if( argc != 3){
		fprintf(stderr, "Usage: server [portnum] [file-system-image]\n");
		exit(0);	
	}

	//	printf("sizeof int: %zu\n", sizeof(int));
	//	printf("sizeof off_t : %zu\n", sizeof(off_t));
	//	printf("sizeof dir ent: %zu\n", sizeof(MFS_DirEnt_t));	
	//	printf("size of inode %zu\n", sizeof(MFS_Inode_t));

	//get port num and file system image
	port = atoi(argv[1]);
	file_sys_img = argv[2];
	printf("file-system-img: %s\n", file_sys_img);

	sd = UDP_Open(port);
	assert(sd > -1);


	fd = open(file_sys_img, O_RDWR);
	printf("file descriptor: %d\n", fd);

	//init chk region
	//also sets a position after the end of chk_region
	//written in file
	int rc = init_chkpt_region();



	//if file image doesn't exist, create it 

	if(fd == -1){

		off_t dir_block_addr;
		off_t inode_addr;

		fd = open(file_sys_img, O_CREAT | O_RDWR | O_TRUNC, 00777 );
		printf("file descriptor: %d\n", fd);

		//update log end
		rc = write_chk_region_to_file();
		assert(rc != -1);
		//	printf("dir block offset: %zu\n", chk_region->log_end);

		////create directory block & update log end
		dir_block_addr = chk_region->log_end;
		rc= create_dir_block(0, 0 , 0 , chk_region->log_end, 1, 0);
		assert(rc != -1);
		//	printf("inode offset: %zu\n", chk_region->log_end);


		//create inode & update log end
		inode_addr = chk_region->log_end;
		rc =  create_inode(dir_block_addr, MFS_DIRECTORY,MFS_BLOCK_SIZE , chk_region->log_end);
		assert(rc != -1);
		//		printf("imap seg offset: %zu\n", chk_region->log_end);

		rc = update_chk_region(0, inode_addr, chk_region->log_end);

	write_chk_region_to_file();
/*
		//print statement
		int l;
		for( l = 0; l < 3; l++){
			printf( " index[%d] : %ld\n , ",l, (long signed int)chk_region->imap_seg_addr[l] );

			if(chk_region->imap_seg_addr[l] ==  (off_t)-1){
				printf("imap seg %d is NULL\n", l);
			}else{
				int j;
				printf("imap seg %d [" , l);
				for(j=0; j < 16; j++){
					printf ("%ld, ", (chk_region->imap[l])->imap_seg[j]);	
				}
				printf(" ]\n");
			}
		}


*/


	}
	///EXISTING FILE SYSTEM //////
	else{


		lseek(fd, 0, SEEK_SET);
		//read in check point region
		read(fd, &(chk_region->log_end), sizeof(off_t));
		read(fd, chk_region->imap_seg_addr, sizeof(off_t)* 256);


		//read in inode addresses to each segment 
		int l;
		for(l = 0; l< 256; l++){

			off_t saddr;
			if( (saddr = chk_region->imap_seg_addr[l] ) != (off_t) -1){

				lseek(fd, saddr, SEEK_SET);
				read(fd, (chk_region->imap[l])->imap_seg, sizeof(MFS_Imap_Seg_t));
			}

		}

		//print statement
		for( l = 0; l < 3; l++){
			printf( " index[%d] : %ld\n , ",l, (long signed int)chk_region->imap_seg_addr[l] );

			int j;
			printf("imap seg %d [" , l);
			for(j=0; j < 16; j++){
				printf ("%ld, ", (chk_region->imap[l])->imap_seg[j]);	
			}
			printf(" ]\n");
		}
	}





	printf("SERVER:: waiting in loop\n");

	msg_t msg;
	while (1) {
		//	char buffer[BUFFER_SIZE];
		int rc = UDP_Read(sd, &s, (char*)&msg, sizeof(msg_t));

		if (rc > 0) {


		//	printf("SERVER:: read %d bytes (message: '%d')\n", rc, msg.func_type);


			switch(msg.func_type){


				case Lookup:

					//				printf("Entered lookup\n");

					rc  = server_lookup(&msg.inum, msg.pinum, msg.buf);


				//	printf("lookup rc : %d ", rc);
					msg.rc = rc;
					break;
				case Stat: 
					rc = server_stat( msg.inum, &msg.type, &msg.size);
				//	printf("stat rc : %d ", rc);
					msg.rc = rc;
					break;
				case Write:
					rc = server_write(msg.inum, msg.buf, msg.block);
				//	printf("write rc : %d ", rc);
					msg.rc = rc;
					break;
				case Read:
					rc = server_read(msg.inum, msg.buf, msg.block);
				//	printf("read rc : %d ", rc);
					msg.rc = rc;
					break;
				case Creat:
					rc = server_creat(msg.pinum, msg.type, msg.buf);
				//	printf("creat rc : %d ", rc);
					msg.rc = rc;
					break;
				case Unlink:
					rc = server_unlink(msg.pinum, msg.buf);
				//	printf("unlink rc : %d ", rc);
					msg.rc = rc;
					break;
				case Shutdown:
					rc = server_shutdown();
				//	printf("shutdown rc : %d ", rc);
					msg.rc = rc;
					break;
				default:
					fprintf(stderr,"Invalid function type\n" );
					exit(-1);




			}	


			rc = UDP_Write(sd, &s, (char*)&msg, sizeof(msg_t));
		}
	}

	return 0;
}


