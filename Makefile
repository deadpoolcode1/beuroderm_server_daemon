CROSS_COMPILE_ANDROID=	/opt/toolchains/arm-2009q3/bin/arm-none-linux-gnueabi-
CCARM=	$(CROSS_COMPILE_ANDROID)gcc
CC = gcc
CCMAC = gcc-4.8
CFLAGS  = -Wall -std=gnu99 -ggdb
RM      = rm -f
#I2C functions
LDLIBS+=i2c.c
#parsing ini files 
LDLIBS+=inih/ini.c
#parse\serialize JSON
LDLIBS+=parson/parson.c


##Server complier is cross compile android

server: server.c
	$(CCARM) $(CFLAGS)  -static -o server.o server.c  $(LDLIBS)  -lpthread

send: send.c
	$(CCARM) $(CFLAGS)  -static -o send.o send.c

## for Linux:

unittest: unittest.c libjsmn.a 
	$(CC) $(CFLAGS)  -o unittest.o unittest.c libjsmn.a

## for MacOS:

unittest_mac: unittest.c libjsmn-mac.a
	$(CCMAC) $(CFLAGS)  -o unittest.o unittest.c libjsmn-mac.a

src=$(wildcard *.cpp *.hpp *.c *.h */*.c */*.cpp */*.h */*.hpp)

astyle:
	astyle --style=linux --lineend=linux --indent=force-tab=8 --pad-header --pad-oper --keep-one-line-blocks \
                --unpad-paren ${src}
clean :
	$(RM) *.o


