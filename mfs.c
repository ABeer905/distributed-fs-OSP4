#include <stdio.h>
#include <string.h>
#include "mfs.h"
#include "udp.h"

//First 12 bytes -> Metadata
//Remaining 4096 Bytes -> Disk block for read/write ops
#define BUFFER_SIZE (5012)

int sd, op;
struct sockaddr_in addrSnd, addrRcv;

/*
 * Connects the client to the server.
 * Returns 0 on success, -1 otherwise
 * hostname[in] - The address of the server
 * port[in] - The port the server is listening on
 */
int MFS_Init(char *hostname, int port){
	sd = UDP_Open(20000);
	if(sd < 0){
		return sd;
	}

    return UDP_FillSockAddr(&addrSnd, hostname, port);
}

/*
 * Looks for a file inside a directory.
 * Returns the file's inode number, -1 otherwise
 * pinum[in] - The parent directory inode number
 * name[in] - The file name
 */
int MFS_Lookup(int pinum, char *name){
	op = OP_LOOKUP;
	char msg[BUFFER_SIZE];

	memcpy(&msg[0], &op, sizeof(int));
	memcpy(&msg[4], &pinum, sizeof(int));
	memcpy(&msg[8], name, 28);
	
	int rc = UDP_Write(sd, &addrSnd, msg, BUFFER_SIZE);
    if (rc < 0) {
		return -1;
    }

	rc = UDP_Read(sd, &addrRcv, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	return (int) msg[0];
}

/*
 * Gets stats for the a file
 * Returns 0 on success, -1 otherwise
 * inum[in] - The inode number of the file to stat
 * m[out] - A MFS_Stat_t struct
 */
int MFS_Stat(int inum, MFS_Stat_t *m){
	op = OP_STAT;
	char msg[BUFFER_SIZE];
	memcpy(&msg[0], &op, sizeof(int));
	memcpy(&msg[4], &inum, sizeof(int));

	int rc = UDP_Write(sd, &addrSnd, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	rc = UDP_Read(sd, &addrRcv, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}
	
	m->type = (int) msg[4];
	m->size = *(int*) &msg[8];
	return (int) msg[0];
}

/*
 * Writes a buffer to disk at the inode specified
 * Returns 0 on success, -1 otherwise
 * inum[in] - The inode to write to
 * buffer[in] - The buffer to be written
 * offset[in] - Start at offset byte of the buffer
 * nbytes[in] - Number of bytes to write starting at offset
 */
int MFS_Write(int inum, char *buffer, int offset, int nbytes){
	op = OP_WRITE;
	char msg[BUFFER_SIZE];

	if(nbytes > 4096){
		return -1;
	}

	memcpy(&msg[0], &op, sizeof(int));
	memcpy(&msg[4], &inum, sizeof(int));
	memcpy(&msg[8], &nbytes, sizeof(int));
	memcpy(&msg[12], &offset, sizeof(int));
	memcpy(&msg[16], buffer, nbytes); 
	
	int rc = UDP_Write(sd, &addrSnd, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	rc = UDP_Read(sd, &addrRcv, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	return (int) msg[0];
}

/*
 * Reads a file to a buffer
 * Returns 0 on success, -1 otherwise
 * inum[in] - The inode to write to
 * buffer[out] - The buffer where data will be written to
 * offset[in] - The offset byte to start reading the file at
 * nbytes[in] - The number of bytes to read
 */
int MFS_Read(int inum, char *buffer, int offset, int nbytes){
	op = OP_READ;
	char msg[BUFFER_SIZE];

	memcpy(&msg[0], &op, sizeof(int));
	memcpy(&msg[4], &inum, sizeof(int));
	memcpy(&msg[8], &nbytes, sizeof(int));
	memcpy(&msg[12], &offset, sizeof(int)); 

	int rc = UDP_Write(sd, &addrSnd, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	rc = UDP_Read(sd, &addrRcv, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	memcpy(buffer, &msg[4], nbytes);
	return (int) msg[0];
}

/*
 * Creates a file
 * Returns 0 on success, -1 otherwise
 * pinum[in] - The parent directory inode
 * type[in] - Either MFS_DIRECTORY or MFS_REGULAR_FILE
 * name[in] - The name of the file
 */
int MFS_Creat(int pinum, int type, char *name){
	op = OP_CREAT;
	char msg[BUFFER_SIZE];

	memcpy(&msg[0], &op, sizeof(int));
	memcpy(&msg[4], &pinum, sizeof(int));
	memcpy(&msg[8], &type, sizeof(int));
	memcpy(&msg[12], name, 28); 

	int rc = UDP_Write(sd, &addrSnd, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	rc = UDP_Read(sd, &addrRcv, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	return (int) msg[0];
}

/*
 * Removes a file
 * Returns 0 on success, -1 otherwise
 * pinum[in] - The parent directory inode
 * name[in] - The name of the file to remove
 */
int MFS_Unlink(int pinum, char *name){
	op = OP_UNLINK;
	char msg[BUFFER_SIZE];

	memcpy(&msg[0], &op, sizeof(int));
	memcpy(&msg[4], &pinum, sizeof(int));
	memcpy(&msg[12], name, 28); 

	int rc = UDP_Write(sd, &addrSnd, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	return 0;
}

/*
 * Forces all server data to disk and terminates the server.
 * Useful for testing purposes.
 */
int MFS_Shutdown(){
	op = OP_TERM;
	char msg[BUFFER_SIZE];

	memcpy(&msg[0], &op, sizeof(int));

	int rc = UDP_Write(sd, &addrSnd, msg, BUFFER_SIZE);
	if(rc < 0){
		return -1;
	}

	return 0;
}

