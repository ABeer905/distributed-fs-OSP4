#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "mfs.h"
#include "udp.h"
#include "ufs.h"

#define BUFFER_SIZE (5008)

int res;

super_t *metadata; //File image metadata
int *inode_bitmap; //Bitmap for allocated inodes
int *data_bitmap;  //Bitmap for allocated data blocks
inode_t *inodes;   //Inodes
char *data;		   //Data blocks

/**
 * Loads a file image and initializes file system metadata, bitmaps, inodes, and data to memory.
 * fileimg[in] - the path of the file image
 * file[out] - the resulting file
 */
void load_image(char* fileimg, FILE *file) {
	file = fopen(fileimg, "r+");
	
	if(!file){
		fprintf(stderr, "An error has occured\n");
		exit(1);
	}
	
	int fd = fileno(file);

	//Read in data structures
	int bytes;
	metadata = (super_t*)malloc(sizeof(super_t));
	pread(fd, metadata, sizeof(super_t), 0);
	
	bytes = UFS_BLOCK_SIZE * metadata->inode_bitmap_len;
	inode_bitmap = (int*)malloc(bytes);
	pread(fd, inode_bitmap, bytes, UFS_BLOCK_SIZE * metadata->inode_bitmap_addr);

	bytes = UFS_BLOCK_SIZE * metadata->data_bitmap_len;
	data_bitmap = (int*)malloc(bytes);
	pread(fd, data_bitmap, bytes, UFS_BLOCK_SIZE * metadata->data_bitmap_addr);

	bytes = UFS_BLOCK_SIZE * metadata->inode_region_len;
	inodes = (inode_t*)malloc(bytes);
	pread(fd, inodes, bytes, UFS_BLOCK_SIZE * metadata->inode_region_addr);

	bytes = UFS_BLOCK_SIZE * metadata->data_region_len;
	data = (char*)malloc(bytes);
	pread(fd, data, bytes, UFS_BLOCK_SIZE * metadata->data_region_addr);
}

int inode_inuse(int inum){
	return inode_bitmap[inum / 32] >> (31 - inum % 32) & 0x01;
}

/**
 * Sets the buffer to be returned by the server to have the desired
 * code when an operation cannot be executd.
 * code[in] - The code to set into the buffer
 * msg[out] - The buffer to place the return code in
 */
void set_ret(char* msg, int code){
	int res = code;
	memcpy(msg, &res, sizeof(int));
	return;
}

/**
 * Takes a directory inode and finds the child inode containing name.
 * msg[in] - The look up message containing opcode, parent inode, name.
 * msg[out] - The inode of the child with name or -1 if failure
 */
void lookup(char *msg){
	int pinum = (int) msg[4];
	char name[28];
	strcpy(&name[0], &msg[8]);

	//Verify valid inode
	if(pinum < 0 || pinum > UFS_BLOCK_SIZE * metadata->inode_region_len / sizeof(inode_t)){
		return set_ret(msg, RES_FAIL);	
	}

	//Verify inode is in use
	if(!inode_inuse(pinum)){
		return set_ret(msg, RES_FAIL);
	}

	//Inode passed in must be a directory
	if(inodes[pinum].type != UFS_DIRECTORY){
		return set_ret(msg, RES_FAIL);
	}

	//Scan blocks for the file containing desired name
	for(int i=0; i < DIRECT_PTRS; i++){
		unsigned int data_block = inodes[pinum].direct[i];

		if(data_block != 0){
			int block = metadata->data_region_addr - data_block;

			for(int j = 0; j < UFS_BLOCK_SIZE / sizeof(dir_ent_t); j++){
				dir_ent_t *entry = (dir_ent_t*) &data[(block * UFS_BLOCK_SIZE) + (j * sizeof(dir_ent_t))];
				if(strcmp(name, entry->name) == 0){
					return set_ret(msg, entry->inum);
				}
			}
		}
	}
}

void stats(char *msg){
	int inum = (int) msg[4];
	//Verify valid inode
	if(inum < 0 || inum > UFS_BLOCK_SIZE * metadata->inode_region_len / sizeof(inode_t)){
		return set_ret(msg, RES_FAIL);	
	}

	//Verify inode is in use
	if(!inode_inuse(inum)){
		return set_ret(msg, RES_FAIL);
	}
	
	set_ret(msg, 0);
	memcpy(&msg[4], &inodes[inum].type, sizeof(int));
	memcpy(&msg[8], &inodes[inum].size, sizeof(int));
}

void img_write(char *msg){
	printf("WRITE NOT IMPLEMENTED\n");
}

void img_read(char *msg){
	printf("READ NOT IMPLEMENTED\n");
}

void img_creat(char *msg){
	printf("CREAT NOT IMPLEMENTED\n");
}

void img_unlink(char *msg){
	printf("UNLINK NOT IMPLEMENTED\n");
}

void terminate(){
	printf("TERMINATE NOT IMPLEMENTED\n");
}

// server code
int main(int argc, char *argv[]) {
	if(argc != 3){
		fprintf(stderr, "An error has occured\n");
		exit(1);
	}

	if(access(argv[2], F_OK | R_OK | W_OK) == -1){
		fprintf(stderr, "image does not exist\n");
		exit(1);
	}

	int port = atoi(argv[1]);
	FILE fimg;
	load_image(argv[2], &fimg);

    int sd = UDP_Open(port);
    assert(sd > -1);

    while (1) {
		struct sockaddr_in addr;
		char msg[BUFFER_SIZE];
		printf("server:: waiting...\n");
		UDP_Read(sd, &addr, msg, BUFFER_SIZE);
		
		int op;
		memcpy(&op, &msg[0], 4);
		switch((const int)op){
			case OP_LOOKUP:
				lookup(msg);
				break;
			case OP_STAT:
				stats(msg);
				break;
			case OP_WRITE:
				img_write(msg);
				break;
			case OP_READ:
				img_read(msg);
				break;
			case OP_CREAT:
				img_creat(msg);
				break;
			case OP_UNLINK:
				img_unlink(msg);
				break;
			case OP_TERM:
				terminate();
				break;
			default:
				fprintf(stderr, "Unsupported Opcode recieved\n");
				exit(1);
		}

        UDP_Write(sd, &addr, msg, BUFFER_SIZE);
		printf("server:: reply\n");
    }
    return 0; 
}
