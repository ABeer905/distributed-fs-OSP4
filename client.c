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
	char msg[12] = "Hello World\0";

	//Persistence tests will run if a second argument is present
	//These tests should be ran on a seperate run after the tests below
	MFS_Stat_t m;
	if(argc == 3){
		MFS_Stat(0, &m);
		assert(m.size == 4 * sizeof(MFS_DirEnt_t));      //Test: Directory size retains 3rd entry
		int myfile = MFS_Lookup(0, b); 
		assert(myfile != -1);                            //Test: File with name "my file" still exists
		char msgn[12];
		assert(MFS_Read(myfile, msgn, 0, 12) == 0);      //Test: File is readable
		assert(strcmp(msg, msgn) == 0);                  //Test: File data is correct

		MFS_Shutdown();
		printf("TEST PASSED\n");
		return 0;
	}

	//Note: Tests assume fresh test file image of with 64 data blocks/64 inodes
	assert(MFS_Lookup(0, a) == 0); //Test: get root directory
	assert(MFS_Lookup(1, a) == -1); //Test: get unused inode
	assert(MFS_Lookup(-1, a) == -1); //Test: get negative inode
	assert(MFS_Lookup(100000, a) == -1); //Test: inode out of bounds
	assert(MFS_Lookup(0, b) == -1); //Test: File does not exist

	assert(MFS_Stat(-1, &m) == -1); //Test: invalid inode
	assert(MFS_Stat(0, &m) == 0); //Test: return 0 on valid inode
	assert(m.type == MFS_DIRECTORY); //Test: Correct file type returned
	assert(m.size == 2 * sizeof(MFS_DirEnt_t)); //Test: Directory is correct size
												
	assert(MFS_Creat(0, MFS_REGULAR_FILE, a) == 0); //Test: Create already existing file fails
	assert(MFS_Creat(1, MFS_REGULAR_FILE, b) == -1); //Test: Create file in non-existent inode
	assert(MFS_Creat(0, MFS_REGULAR_FILE, b) == 0);  //Test: Create new file succeeds
	int fd = MFS_Lookup(0, b);
	assert(fd != -1);            					 //Test: New file can be found in system
	assert(MFS_Creat(fd, MFS_DIRECTORY, c) == -1);   //Test: Create file fails when parent is not a directory
	
	assert(MFS_Creat(0, MFS_DIRECTORY, c) == 0);	 //Test: Create new directory succeeds
	int pdir = MFS_Lookup(0, c);	
	assert(pdir != -1);                               //Test: New directory can be found in system
	assert(MFS_Creat(pdir, MFS_REGULAR_FILE, b) == 0);//Test: Can create new file with same name in different directory
	assert(MFS_Lookup(pdir, b) != -1);				  //Test: Can find file in different directory
	MFS_Stat(0, &m);
	assert(m.size == 4 * sizeof(MFS_DirEnt_t));       //Test: Root directory size updated
	MFS_Stat(pdir, &m);
	assert(m.size == 3 * sizeof(MFS_DirEnt_t));		  //Test: new directory size has default directories as well as new one			

	MFS_Stat(fd, &m);
	assert(m.size == 0);                             //Test: Asserts new file has size 0
	assert(MFS_Write(fd, msg, 0, 5000) == -1);        //Test: Write greater than 4096 bytes (illegal)
	assert(MFS_Write(pdir, msg, 0, 12) == -1);        //Test: Write to directory (illegal)
	assert(MFS_Write(fd, msg, -1, 12) == -1);         //Test: Write to a negative offset (illegal)
	assert(MFS_Write(fd, msg, 0, 12) == 0);           //Test: Write buffer to file
	MFS_Stat(fd, &m);
	assert(m.size == 12);                             //Test: Size update reflected by write
	assert(MFS_Write(fd, b, 0, 8) == 0);			  //Test: Overwrite part of file
	MFS_Stat(fd, &m);
	assert(m.size == 12);                             //Test: Overwriting file does not cause size to increase when smaller than original
	assert(MFS_Write(fd, b, 14, 8) == -1);            //Test: offset cant be greater than file size
											  

	MFS_Write(fd, msg, 0, 12);
	char msg_read[12];
	assert(MFS_Read(fd, msg_read, 0, 12) == 0);       //Test: Read succeeds
	printf("%s\n", msg_read);
	assert(strcmp(msg, msg_read) == 0);				  //Test: Read data is correct "Hello World\0"
	char msg_short[6];
	assert(MFS_Read(fd, msg_short, 6, 6) == 0);       //Test: Read succeeds with offset
	assert(strcmp(msg, msg_read) == 0);               //Test: Read data is world\0"
	
	//Test: Write more than one block of data and then read it back
	char msg1[10] = "Message 1\0";
	char msg2[10] = "Message 2\0";
	char msg_tmp[10];
	int file = MFS_Lookup(pdir, b);
	assert(file != 0);

	for(int i = 0; i < 500; i++){
		if(i % 2 == 0){
			assert(MFS_Write(file, msg1, i * 10, 10) == 0);
		}else{
			assert(MFS_Write(file, msg2, i * 10, 10) == 0);
		}
	}

	assert(MFS_Stat(file, &m) == 0);
	assert(m.size == 10 * 500);

	for(int i = 0; i < 500; i++){
		if(i % 2 == 0){
			assert(MFS_Read(file, msg_tmp, i * 10, 10) == 0);
			assert(strcmp(msg1, msg_tmp) == 0);
		}else{
			assert(MFS_Read(file, msg_tmp, i * 10, 10) == 0);
			assert(strcmp(msg2, msg_tmp) == 0);
		}
	}

	assert(MFS_Unlink(pdir, c) == 0);				//Test: Remove succeeds when file doesn't exist
	assert(MFS_Unlink(0, c) == -1);                 //Test: Remove fails when directory isn't empty
	assert(MFS_Unlink(pdir, b) == 0);               //Test: File remove succeeds
	assert(MFS_Lookup(pdir, b) == -1);				//Test: Lookup fails on file
	assert(MFS_Unlink(0, c) == 0);					//Test: Remove empty directory
	assert(MFS_Lookup(0, c) == -1);                 //Test: Lookup fails on file
	MFS_Stat(0, &m);
	assert(m.size == 3 * sizeof(MFS_DirEnt_t));      //Test: Directory size updated correctly
	MFS_Creat(0, MFS_DIRECTORY, c);
	int new_dir = MFS_Lookup(0, c);
	assert(new_dir == pdir);                        //Test: Ensures new file gets previously freed inode
													
	MFS_Shutdown();
	printf("ALL TESTS PASSED\n");
    return 0;
}
