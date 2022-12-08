#include <stdio.h>
#include <assert.h>
#include "udp.h"
#include "mfs.h"


//Testing code for the mfs library
int main(int argc, char *argv[]) {
	MFS_Init("localhost", atoi(argv[1]));
	
	char a[28];
	strcpy(&a[0], ".");
	char b[28];
	strcpy(b, "my file");
	char c[28];
	strcpy(c, "my dir");

	//Note: Tests assume fresh test file image of with 64 data blocks/64 inodes
	assert(MFS_Lookup(0, a) == 0); //Test: get root directory
	assert(MFS_Lookup(1, a) == -1); //Test: get unused inode
	assert(MFS_Lookup(-1, a) == -1); //Test: get negative inode
	assert(MFS_Lookup(100000, a) == -1); //Test: inode out of bounds
	assert(MFS_Lookup(0, b) == -1); //Test: File does not exist

	MFS_Stat_t m;
	assert(MFS_Stat(-1, &m) == -1); //Test: invalid inode
	assert(MFS_Stat(0, &m) == 0); //Test: return 0 on valid inode
	assert(m.type == MFS_DIRECTORY); //Test: Correct file type returned
	assert(m.size == 2 * sizeof(MFS_DirEnt_t)); //Test: Directory is correct size
												
	assert(MFS_Creat(0, MFS_REGULAR_FILE, a) == -1); //Test: Create already existing file fails
	assert(MFS_Creat(1, MFS_REGULAR_FILE, b) == -1); //Test: Create file in non-existent inode
	assert(MFS_Creat(0, MFS_REGULAR_FILE, b) == 0);  //Test: Create new file succeeds
	assert(MFS_Lookup(0, b) != -1);					 //Test: New file can be found in system
	
	assert(MFS_Creat(0, MFS_DIRECTORY, c) == 0);	 //Test: Create new directory succeeds
	int pdir = MFS_Lookup(0, c);
	assert(pdir != -1);                               //Test: New directory can be found in system
	assert(MFS_Creat(pdir, MFS_REGULAR_FILE, b) == 0);//Test: Can create new file with same name in different directory
	assert(MFS_Lookup(pdir, b) != -1);				  //Test: Can find file in different directory
	MFS_Stat(0, &m);
	assert(m.size == 4 * sizeof(MFS_DirEnt_t));       //Test: Root directory size updated
	MFS_Stat(pdir, &m);
	assert(m.size == 3 * sizeof(MFS_DirEnt_t));		  //Test: new directory size has default directories as well as new one

	printf("ALL TESTS PASSED\n");
    return 0;
}
