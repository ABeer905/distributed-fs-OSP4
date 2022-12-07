#include <stdio.h>
#include <assert.h>
#include "udp.h"
#include "mfs.h"


//Testing code for the mfs library
int main(int argc, char *argv[]) {
	MFS_Init("localhost", atoi(argv[1]));
	
	char a[28];
	strcpy(&a[0], ".");

	//Note: Tests assume fresh test file image of with 64 data blocks/64 inodes
	assert(MFS_Lookup(0, a) == 0); //Test: get root directory
	assert(MFS_Lookup(1, a) == -1); //Test: get unused inode
	assert(MFS_Lookup(-1, a) == -1); //Test: get negative inode
	assert(MFS_Lookup(100000, a) == -1); //Test: inode out of bounds
	
	MFS_Stat_t m;
	assert(MFS_Stat(-1, &m) == -1); //Test: invalid inode
	assert(MFS_Stat(0, &m) == 0); //Test: return 0 on valid inode
	assert(m.type == MFS_DIRECTORY); //Test: Correct file type returned
	assert(m.size == 2 * sizeof(MFS_DirEnt_t));

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
