CROSS_COMPILE_ANDROID=	/opt/toolchains/arm-2009q3/bin/arm-none-linux-gnueabi-
CC=	$(CROSS_COMPILE_ANDROID)gcc
#CC = gcc
CFLAGS  = -g
RM      = rm -f




default: all

all: register_manipulate

server: server.c
	$(CC) $(CFLAGS)  -static -o server.o server.c -lpthread

send: send.c
	$(CC) $(CFLAGS)  -static -o send.o send.c

clean :
	$(RM) *.o


