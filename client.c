#include <stdio.h>
#include "udp.h"
#include "mfs.h"

//Testing code for the mfs library
int main(int argc, char *argv[]) {
	MFS_Init("localhost", atoi(argv[1]));
	
	char a[28];

	MFS_Lookup(5, a);
	MFS_Stat(5, 0);
	MFS_Write(5, a, 0, 28);
	MFS_Read(5, a, 0, 28);
	MFS_Creat(5, 0, a);
	MFS_Shutdown();

    /*printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)\n", rc, message);*/
    return 0;
}
