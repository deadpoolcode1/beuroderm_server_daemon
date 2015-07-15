CROSS_COMPILE =	/mnt/Work/arm-2009q3/bin/arm-none-linux-gnueabi-
#CC=	$(CROSS_COMPILE)gcc
CC=	gcc
CFLAGS  = -g
RM      = rm -f



send: send.c
	$(CC) $(CFLAGS)  -o send.o send.c

server: server.c
	$(CC) $(CFLAGS)  -o server.o server.c -lpthread

clean :
	$(RM) *.o
