#include <stdio.h>
#include <assert.h>
#include "udp.h"
#include "mfs.h"


//Testing code for the mfs library
int main(int argc, char *argv[]) {
	MFS_Init("localhost", atoi(argv[1]));
	
	char a[28];
	strcpy(&a[0], ".");

	assert(MFS_Lookup(0, a) == 0);
	assert(MFS_Lookup(1, a) == -1);

	printf("ALL TESTS PASSED\n");

	//MFS_Stat(5, 0);
	//MFS_Write(5, a, 0, 28);
	//MFS_Read(5, a, 0, 28);
	//MFS_Creat(5, 0, a);
	//MFS_Shutdown();

    /*printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)\n", rc, message);*/
    return 0;
}
