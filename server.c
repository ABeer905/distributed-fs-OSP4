#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "mfs.h"
#include "udp.h"
#include "ufs.h"

#define BUFFER_SIZE (5008)

/**
 * Loads a file image and initializes file system metadata.
 * fileimg[in] - the path of the file image
 * file[out] - the resulting file
 *
 * Returns a pointer to metadata containing struct
 */
super_t *load_image(char* fileimg, FILE *file) {
	file = fopen(fileimg, "r+");
	
	if(!file){
		fprintf(stderr, "An error has occured\n");
		exit(1);
	}
	
	super_t *super_block = (super_t*)malloc(sizeof(super_t));
	fread(super_block, sizeof(super_t), 1, file);
	return super_block;
}

void lookup(char *msg, super_t *metadata){
	printf("LOOKUP NOT IMPLEMENTED\n");
}

void stats(char *msg, super_t *metadata){
	printf("STATS NOT IMPLEMENTED\n");
}

void img_write(char *msg, super_t *metadata){
	printf("WRITE NOT IMPLEMENTED\n");
}

void img_read(char *msg, super_t *metadata){
	printf("READ NOT IMPLEMENTED\n");
}

void img_creat(char *msg, super_t *metadata){
	printf("CREAT NOT IMPLEMENTED\n");
}

void img_unlink(char *msg, super_t *metadata){
	printf("UNLINK NOT IMPLEMENTED\n");
}

void terminate(super_t *metadata){
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
	super_t *metadata = load_image(argv[2], &fimg);
	printf("%d\n", metadata->data_region_len);

    int sd = UDP_Open(port);
    assert(sd > -1);

    while (1) {
		struct sockaddr_in addr;
		char msg[BUFFER_SIZE];
		printf("server:: waiting...\n");
		int rc = UDP_Read(sd, &addr, msg, BUFFER_SIZE);
		
		int op;
		memcpy(&op, &msg[0], 4);
		switch((const int)op){
			case OP_LOOKUP:
				lookup(msg, metadata);
				break;
			case OP_STAT:
				stats(msg, metadata);
				break;
			case OP_WRITE:
				img_write(msg, metadata);
				break;
			case OP_READ:
				img_read(msg, metadata);
				break;
			case OP_CREAT:
				img_creat(msg, metadata);
				break;
			case OP_UNLINK:
				img_unlink(msg, metadata);
				break;
			case OP_TERM:
				terminate(metadata);
				break;
			default:
				fprintf(stderr, "Unsupported Opcode recieved\n");
				exit(1);
		}

		if (rc > 0) {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
			printf("server:: reply\n");
		} 
    }
    return 0; 
}
