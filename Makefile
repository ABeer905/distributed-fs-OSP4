CC     := gcc
CFLAGS := -Wall -Werror 

SRCS   := client.c \
	server.c 

OBJS   := ${SRCS:c=o}
PROGS  := ${SRCS:.c=}

.PHONY: all
all: ${PROGS}

${PROGS} : % : %.o Makefile
	${CC} $< -o $@ udp.c mfs.c
	${CC} ${CFLAGS} -shared -o libmfs.so -fPIC mfs.c udp.c
	ldconfig -n /home/abeers/distributed-fs-OSP4
	${CC} ${CFLAGS} client.c -o client -L/home/abeers/distributed-fs-OSP4 -lmfs

clean:
	rm -f ${PROGS} ${OBJS}

%.o: %.c Makefile
	${CC} ${CFLAGS} -c $<
