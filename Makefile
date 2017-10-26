CROSS_COMPILE_ANDROID=	/opt/toolchains/arm-2009q3/bin/arm-none-linux-gnueabi-
CCARM=	$(CROSS_COMPILE_ANDROID)gcc
CC = gcc
CCMAC = gcc-4.8
CFLAGS  = -Wall  -ggdb
RM      = rm -f


server: server.c
	$(CCARM) $(CFLAGS)  -static -o server.o server.c -lpthread

send: send.c
	$(CC) $(CFLAGS)  -static -o send.o send.c

## for Linux:

unittest: unittest.c libjsmn.a 
	$(CC) $(CFLAGS)  -o unittest.o unittest.c libjsmn.a

## for MacOS:

unittest_mac: unittest.c libjsmn-mac.a
	$(CCMAC) $(CFLAGS)  -o unittest.o unittest.c libjsmn-mac.a

clean :
	$(RM) *.o


