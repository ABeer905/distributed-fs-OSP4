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
void load_image(char* fileimg, FILE **file) {
	*file = fopen(fileimg, "r+");
	
	if(!file){
		fprintf(stderr, "An error has occured\n");
		exit(1);
	}
	
	int fd = fileno(*file);

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

/**
 * Writes all data to disk
 */
void flush_data(FILE *file){
	pwrite(fileno(file), inode_bitmap, metadata->inode_bitmap_len * UFS_BLOCK_SIZE, UFS_BLOCK_SIZE);
	pwrite(fileno(file), data_bitmap, metadata->data_bitmap_len * UFS_BLOCK_SIZE, metadata->data_bitmap_addr * UFS_BLOCK_SIZE);
	pwrite(fileno(file), inodes, metadata->inode_region_len * UFS_BLOCK_SIZE, metadata->inode_region_addr * UFS_BLOCK_SIZE);
	pwrite(fileno(file), data, metadata->data_region_len * UFS_BLOCK_SIZE, metadata->data_region_addr * UFS_BLOCK_SIZE);
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
	
		if(data_block >= metadata->data_region_addr && 
		   data_block < metadata->data_region_len + metadata->data_region_addr){
			int block = data_block - metadata->data_region_addr;

			for(int j = 0; j < UFS_BLOCK_SIZE / sizeof(dir_ent_t); j++){
				dir_ent_t *entry = (dir_ent_t*) &data[(block * UFS_BLOCK_SIZE) + (j * sizeof(dir_ent_t))];
				if(strcmp(name, entry->name) == 0 && entry->inum > -1){
					return set_ret(msg, entry->inum);
				}
			}
		}
	}

	msg[36] = 1; //Set flag indicating that scan was complete. Useful for other ops
	return set_ret(msg, RES_FAIL); //File with name was not found
}

/**
 * Returns the stats of a file
 * msg[in] - The stat message containing opcode and inode
 * msg[out] - A buffer containing return code, type, and size
 */
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

/**
 * Finds a new block within the file image.
 * Returns the block id or 0 if failure
 */
unsigned int allocblock(){
	int free = 0;
	for(int i = 0; i < UFS_BLOCK_SIZE * metadata->data_bitmap_len / 4; i++){
		for(int j = 31; j > 0; j--){
			if(!(data_bitmap[i] >> j & 0x01)){
				//Found free spot
				free = i * 32 + 31 - j;
				data_bitmap[i] |= 1UL << j;
				break;
			}
		}

		if(free){
			if(free > metadata->data_region_len){
				return 0;
			}
			break;
		}
	}
	
	return free;
}

/**
 * Helper method to append data to a file or directory
 * inode[in] - inode of file to write to
 * buffer[in] - The data to write
 * n[in] - The number of bytes to write
 * offset[in] - Number of bytes from start of file to begin writing
 */
int writef(FILE *file, int inode, void* buffer, int n, int offset){
	if(offset / UFS_BLOCK_SIZE >= DIRECT_PTRS){
		return -1;
	}
	
	unsigned int block = inodes[inode].direct[offset / UFS_BLOCK_SIZE];
	if(block == 0){
		block = metadata->data_region_addr + allocblock();
		
		if(!block){
			return -1;
		}

		inodes[inode].direct[offset / UFS_BLOCK_SIZE] = block;
	}
	block -= metadata->data_region_addr;

	//Case if not all data cant fit in current block
	if(offset % UFS_BLOCK_SIZE + n > UFS_BLOCK_SIZE){
		int split = UFS_BLOCK_SIZE - (offset % UFS_BLOCK_SIZE + n);
		
		//Allocate second block
		if(offset / UFS_BLOCK_SIZE + 1 >= DIRECT_PTRS){
			return -1;
		}
		unsigned int block2 = inodes[inode].direct[offset / UFS_BLOCK_SIZE + 1];
		if(block2 == 0){
			block2 = metadata->data_region_addr + allocblock();
		
			if(!block2){
				return -1;
			}

			inodes[inode].direct[offset / UFS_BLOCK_SIZE + 1] = block2;
		}
		block2 -= metadata->data_region_addr;

		
		//Write to first block
		memcpy(&data[block * UFS_BLOCK_SIZE + offset % UFS_BLOCK_SIZE], buffer, n + split);
		//Write to second block
		memcpy(&data[block2 * UFS_BLOCK_SIZE], &((char*)buffer)[n + split], -1 * split);

	//Case if all data can fit in current block
	}else{
		memcpy(&data[block * UFS_BLOCK_SIZE + offset % UFS_BLOCK_SIZE], buffer, n);
	}

	//Update metadata
	if(offset + n > inodes[inode].size){
		inodes[inode].size = offset + n;
	}

	flush_data(file);

	return 0;
}

/**
 * Writes a buffer to a file of UFS_REGULAR_FILE type
 * msg[in] - The payload
 * file[in] - The file to write to
 */
void img_write(char *msg, FILE *file){
	int inum = *(int*) &msg[4];
	int bytes = *(int*) &msg[8];
	int offset = *(int*) &msg[12];

	//Verify valid inode
	if(inum < 0 || inum > UFS_BLOCK_SIZE * metadata->inode_region_len / sizeof(inode_t)){
		return set_ret(msg, RES_FAIL);	
	}

	//Verify inode is in use
	if(!inode_inuse(inum)){
		return set_ret(msg, RES_FAIL);
	}

	//Inode passed in must be a regular file
	if(inodes[inum].type != UFS_REGULAR_FILE){
		return set_ret(msg, RES_FAIL);
	}

	//Max byte size == 4096
	if(bytes > 4096){
		return set_ret(msg, RES_FAIL);
	}

	//Offset cant be nagative or greater than file size
	if(offset < 0 || offset > inodes[inum].size){
		return set_ret(msg, RES_FAIL);
	}

	if(writef(file, inum, &msg[16], bytes, offset) == -1){
		return set_ret(msg, RES_FAIL);
	}else{
		return set_ret(msg, 0);
	}
}

/**
 * Reads n bytes from file at byte offset
 * msg[in] - The message payload
 */
void img_read(char *msg){
	int inum = *(int*) &msg[4];
	int bytes = *(int*) &msg[8];
	int offset = *(int*) &msg[12];

	//Verify valid inode
	if(inum < 0 || inum > UFS_BLOCK_SIZE * metadata->inode_region_len / sizeof(inode_t)){
		return set_ret(msg, RES_FAIL);	
	}

	//Verify inode is in use
	if(!inode_inuse(inum)){
		return set_ret(msg, RES_FAIL);
	}

	//Inode passed in must be a regular file
	if(inodes[inum].type != UFS_REGULAR_FILE){
		return set_ret(msg, RES_FAIL);
	}

	//Max byte size == 4096
	if(bytes > 4096){
		return set_ret(msg, RES_FAIL);
	}

	//Offset cant be nagative or greater than file size
	if(offset < 0 || offset > inodes[inum].size || offset + bytes > inodes[inum].size){
		return set_ret(msg, RES_FAIL);
	}

	unsigned int block = inodes[inum].direct[offset / UFS_BLOCK_SIZE] - metadata->data_region_addr;
	if(offset % UFS_BLOCK_SIZE + bytes > UFS_BLOCK_SIZE){ //Case if we need to do a split read
		int split = UFS_BLOCK_SIZE - (offset % UFS_BLOCK_SIZE + bytes);
		int addr = block * UFS_BLOCK_SIZE + offset % UFS_BLOCK_SIZE;
		memcpy(&msg[4], &data[addr], bytes + split);
		memcpy(&msg[4 + bytes + split], &data[addr+bytes+split], -1 * split);
	} else { //Case if we can read in one go
		memcpy(&msg[4], &data[block * UFS_BLOCK_SIZE + offset % UFS_BLOCK_SIZE], bytes);
	}

	return set_ret(msg, 0);
}

/**
 * Creates a new file or directory
 * msg[in] - The message payload
 * file[in] - The file to write to
 */
void img_creat(char *msg, FILE *file){
	int pinum = (int) msg[4];
	int type = (int) msg[8];

	char buffer[37];
	memcpy(&buffer[4], &pinum, 4);
	memcpy(&buffer[8], &msg[12], 28);
	buffer[36] = 0;
	lookup(buffer);
	
	//Validates parent inode. Also ensures that a file with name does not already exist
	if(buffer[0] == -1 && buffer[36] != 1){
		return set_ret(msg, RES_FAIL);
	}else if(buffer[0] > -1){
		return set_ret(msg, 0); //File already exists - This is ok
	}

	//Parent inode must be a directory
	if(inodes[pinum].type != UFS_DIRECTORY){
		return set_ret(msg, RES_FAIL);
	}

	//Scan inode bitmap looking for free inode
	int free = -1;
	for(int i = 0; i < UFS_BLOCK_SIZE * metadata->inode_bitmap_len / 4; i++){
		for(int j = 31; j > 0; j--){
			if(!(inode_bitmap[i] >> j & 0x01)){
				//Found free spot
				free = i * 32 + 31 - j;
				break;
			}
		}

		if(free > -1){
			break;
		}
	}
	
	if(free < 0){
		return set_ret(msg, RES_FAIL);
	}

	//If type directory we must populate with default entries "." and ".."
	if(type == UFS_DIRECTORY){
		dir_ent_t entry;
		entry.inum = free; // current directory
		strcpy(entry.name, ".");
		if(writef(file, free, &entry, sizeof(dir_ent_t), 0) == -1){ return set_ret(msg, RES_FAIL); }

		entry.inum = pinum; // parent directory
		strcpy(entry.name, "..");
		if(writef(file, free, &entry, sizeof(dir_ent_t), sizeof(dir_ent_t)) == -1){ return set_ret(msg, RES_FAIL); }

		//Initialize remaining directory entries to -1 (free)
		entry.inum = -1;
		for(int i = 2*sizeof(dir_ent_t); i < UFS_BLOCK_SIZE; i+=sizeof(dir_ent_t)){
			memcpy(&data[free * UFS_BLOCK_SIZE + i], &entry, sizeof(dir_ent_t));
		}
	}
	
	//Update parent directory
	dir_ent_t entry;
	entry.inum = free;
	strcpy(entry.name, &msg[12]);

	//Find free entry in parent directory
	int offset = 0;
	for(int i = 0; i < DIRECT_PTRS; i++){
		int data_block = inodes[pinum].direct[i];
		if(data_block >= metadata->data_region_addr && 
		   data_block < metadata->data_region_len + metadata->data_region_addr){
			data_block -= metadata->data_region_addr;
			for(int j = 0; j < UFS_BLOCK_SIZE / sizeof(dir_ent_t); j++){
				if(((dir_ent_t*)&data[data_block * UFS_BLOCK_SIZE + j * sizeof(dir_ent_t)])->inum == -1){
					offset = i * UFS_BLOCK_SIZE + j * sizeof(dir_ent_t);
					break;
				}
			}
		}
		if(offset){
			break;
		}
	}

	if(!offset){
		offset = inodes[pinum].size;
	}

	if(writef(file, pinum, &entry, sizeof(dir_ent_t), offset) == 0){
		//Initialize inode
		inodes[free].type = type;
		if(type == UFS_REGULAR_FILE){
			inodes[free].size = 0;
		}
		inode_bitmap[free / 32] |= 1UL << (31 - free % 32);
		flush_data(file);
		return set_ret(msg, 0);
	}
	
	//Write to disk failed
	return set_ret(msg, RES_FAIL);
}

/**
 * Unlinks a file from the filesystem
 * msg[in] - The message payload
 */
void img_unlink(char *msg, FILE *file){
	int pinum = (int) msg[4];

	char buffer[37];
	memcpy(&buffer[4], &pinum, 4);
	memcpy(&buffer[8], &msg[8], 28);
	lookup(buffer);

	//Validates parent inode. Also returns success if file doesn't exist
	if(buffer[0] == -1 && buffer[36] != 1){
		return set_ret(msg, RES_FAIL);
	}else if(buffer[0] == -1){
		return set_ret(msg, 0); //File doesn't exists - This is ok
	}

	//Can't delete non-empty directory
	int fd = *(int *) &buffer[0];
	if(inodes[fd].type == UFS_DIRECTORY && inodes[fd].size > 2 * sizeof(dir_ent_t)){
		return set_ret(msg, RES_FAIL);
	}

	//free inode
	inode_bitmap[fd / 32] &= ~(1UL << (31 - fd % 32));

	//Free all allocated memory blocks
	for(int i = 0; i < inodes[fd].size / 4096; i++){
		int block = inodes[fd].direct[i] - metadata->data_region_addr;
		data_bitmap[block / 32] &= ~(1UL << (31 - block % 32));
	}

	//set file size to 0
	inodes[fd].size = 0;

	//Clear entry in parent directory
	for(int i = 0; i < inodes[pinum].size / 4096 + 1; i++){
		int block = inodes[pinum].direct[i] - metadata->data_region_addr;
		for(int j = 0; j < UFS_BLOCK_SIZE; j += sizeof(dir_ent_t)){
			dir_ent_t* entry = (dir_ent_t*) &data[block * UFS_BLOCK_SIZE + j];
			if(entry->inum == fd){
				entry->inum = -1;
				
				//Update size if needed
				if(i * UFS_BLOCK_SIZE + j == inodes[pinum].size - sizeof(dir_ent_t)){
					inodes[pinum].size -= sizeof(dir_ent_t);
				}
			}
		}
	}

	flush_data(file);
	return set_ret(msg, 0);
}

/**
 * Updates all disk data and closes file. Server exits, does not return
 */
void terminate(FILE *file){
	flush_data(file);
	close(fileno(file));
	exit(0);
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
	FILE *fimg;
	load_image(argv[2], &fimg);

    int sd = UDP_Open(port);
    assert(sd > -1);

    while (1) {
		struct sockaddr_in addr;
		char msg[BUFFER_SIZE];
		//printf("server:: waiting...\n");
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
				img_write(msg, fimg);
				break;
			case OP_READ:
				img_read(msg);
				break;
			case OP_CREAT:
				img_creat(msg, fimg);
				break;
			case OP_UNLINK:
				img_unlink(msg, fimg);
				break;
			case OP_TERM:
				terminate(fimg);
				break;
			default:
				fprintf(stderr, "Unsupported Opcode recieved\n");
				exit(1);
		}

        UDP_Write(sd, &addr, msg, BUFFER_SIZE);
		//printf("server:: reply\n");
    }
    return 0; 
}
