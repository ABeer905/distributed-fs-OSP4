#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "udp.h"
#include "ufs.h"

#define BUFFER_SIZE (1000)

/**
 * Loads a file image and initializes file system metadata.
 * fileimg[in] - the path of the file image
 * file[out] - the resulting file
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
	char message[BUFFER_SIZE];
	printf("server:: waiting...\n");
	int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
	printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
	if (rc > 0) {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
	    printf("server:: reply\n");
	} 
    }
    return 0; 
}
