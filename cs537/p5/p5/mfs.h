#ifndef __MFS_h__
#define __MFS_h__

#include <sys/types.h>
#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)
#define MFS_INODE_MAX (4096)
#define MFS_INODE_DIRECT_PTRS (14)
#define MFS_IMAP_SEG_SIZE (16)
#define MFS_DIR_ENTRIES (64)


typedef struct __MFS_Stat_t {
     int type;                            // MFS_DIRECTORY or MFS_REGULAR
     int size;                            // bytes
                                          // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
     char name[60];                        // up to 60 bytes of name in directory (including \0)
     int  inum;                            // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

typedef struct __MFS_Imap_Seg_t{
	off_t imap_seg[MFS_IMAP_SEG_SIZE];

}MFS_Imap_Seg_t;


typedef struct __MFS_Inode_t{
	int size;                          //num of last byte in file
	int type;                          //MFS_DIRECTORY or MFS_REGULAR
	
	off_t directptrs[14];              //
} MFS_Inode_t;

typedef struct __MFS_Chk_t{
	
	off_t log_end;

       	//Pointers to inode image segments
        //4096 max inodes / 16 entries per segment = 256 segments max
	//Index of imap array is segment number
	MFS_Imap_Seg_t * imap[256];
	off_t imap_seg_addr[256];
	
}MFS_Chk_t;


int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // __MFS_h__
