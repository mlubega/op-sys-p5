#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include "msg.h"
#include <string.h>
#include <sys/select.h>




#define BUFFER_SIZE (4096)


msg_t msg;



int
main(int argc, char *argv[])
{
	int rc = MFS_Init("mumble-31.cs.wisc.edu", 520000);

	if(rc == -1){
		fprintf(stderr, "Unable to connect to server\n");
		exit(0);
	}


	char buffer[BUFFER_SIZE];

	printf("CLIENT:: about to send message (%d)\n", rc);

	
	rc = MFS_Lookup(0, "main\0");
	printf("Lookup returned: %d\n", rc);
	rc= MFS_Creat(0, MFS_DIRECTORY, "main\0");


	int main_dir = MFS_Lookup(0, "main");
	printf("Lookup returned: %d\n", main_dir);
	int i;
	for(i = 0; i < (64*14 - 1); i++){
		sprintf(buffer, "file %d", i);
		rc = MFS_Creat(main_dir, MFS_REGULAR_FILE, buffer);
		fprintf(stderr,"Creat returned: %d\n\n", rc);
	}
	printf("FINISHED CREATING; STARTING LOOKUP\n");
	for(i = 0; i < (64*14 - 2); i++){
		sprintf(buffer, "file %d", i);
		rc = MFS_Lookup(main_dir, buffer);
		fprintf(stderr, "Lookup returned: %d\n", rc);
	}	

	MFS_Shutdown();


	return 0;
}


