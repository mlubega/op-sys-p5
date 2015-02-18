#include "mfs.h"

#define Init     0
#define Lookup   1
#define Stat     2
#define Write    3
#define Read     4
#define Creat    5
#define Unlink   6
#define Shutdown 7




typedef struct __msg_t{

	int func_type;
	int type;
	int size;
	int rc; //success or not
	int inum;
	int pinum;
	int block;
	char buf[4096];

}msg_t;
