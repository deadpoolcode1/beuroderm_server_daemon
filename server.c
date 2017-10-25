#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<signal.h>
#include<pthread.h> //for threading , link with lpthread
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include "i2c.h"
#define ServerDebug 1
#ifdef ServerDebug
//#undef ServerDebug
#endif
/*
function return index of occurance pattern in string
*/
int findSubstr(char *inpText, char *pattern) {
    int inplen = strlen(inpText);
    while (inpText != NULL) {

        char *remTxt = inpText;
        char *remPat = pattern;

        if (strlen(remTxt) < strlen(remPat)) {
            /* printf ("length issue remTxt %s \nremPath %s \n", remTxt, remPat); */
            return -1;
        }
        while (*remTxt++ == *remPat++) {
            if (*remPat == '\0') {
                return inplen - strlen(inpText+1);
            }
            if (remTxt == NULL) {
                return -1;
            }
        }
        remPat = pattern;

        inpText++;
    }
    return -1;
}
//handles a new connection
void *connection_handler(void *);

int socket_desc_main;


/*
function to handle ctrl+c response 
ensure closing the socket before exiting software 
*/
void sig_handler(int signo)
{
  if (signo == SIGINT)
    printf("received SIGINT\n");
    close(socket_desc_main);    
    exit(1);
}
/*
function to handle opening a file 
*/
int open_file(char *filename)
{
	int fd;
	fd = open(filename, O_RDWR);
	if (fd < 0) 
	{
         	printf("can not open file\n");
        }
	return fd;
}
/*
handles read i2c server command
exaqmple: /dev/i2c-1;0x1b;0x5d
*/
uint8_t read_i2c(char *string_command)
{
	char *path;
	int addr=0, reg=0, data;
	int file, rc;
	path[0]='\0';
	//(scanf("%80[^;],%d[^;],%d",path ,addr , reg, &string_command) == 4)
	file = open(path, O_RDWR);
	if (file < 0)
		err(errno, "Tried to open '%s'", path); 

	rc = ioctl(file, I2C_SLAVE_FORCE, addr);
	if (rc < 0)
		err(errno, "Tried to set device address '0x%02x'", addr);

	data = i2c_smbus_read_byte_data(file, reg);
	#ifdef ServerDebug
	printf("%s: device 0x%02x at address 0x%02x: 0x%02x\n",
			path, addr, reg, data);
	#endif
	return data;
} 
int main(int argc , char *argv[])
{
    int socket_desc , new_socket , c , *new_sock;
    struct sockaddr_in server , client;
     
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 5797 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("bind failed");
        return 1;
    }
    socket_desc_main=socket_desc;
    puts("bind done");
    if (signal(SIGINT, sig_handler) == SIG_ERR); 
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("Connection accepted");
        //Reply to the client
        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }
        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL);
        puts("Handler assigned");
    }
     
    if (new_socket<0)
    {
        perror("accept failed");
        return 1;
    }
     
    return 0;
}
 
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    char client_message[2000];
    int fd;
    int result;

    struct file_action
    {
         char name [100];
         char value [100];
    };
    client_message[0]='\0';
    //Receive a message from client
    while( (read_size = recv(sock , client_message , 2000 , 0)) > 0 )
    {
        //Send the message back to client
        //write(sock , client_message , strlen(client_message));
	#ifdef ServerDebug
	printf("message:%s\n",client_message);
	#endif
	//now make action according to messaage
	if(findSubstr(client_message, "write_file")>-1)
	{
		/*action is writing to a file
		example: write_file:/sys/class/gpio/export=5
		*/
		struct file_action commandFile;
		strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
		commandFile.name[findSubstr(commandFile.name, "=")-1]='\0';
		#ifdef ServerDebug
		printf("file to write:%s\n",commandFile.name);
        	#endif
		strcpy( commandFile.value, client_message+findSubstr(client_message, "=") );
		#ifdef ServerDebug
		printf("value:%s\n",commandFile.value);
		#endif
		fd = open_file(commandFile.name);
		if (fd<0) {}
		else
		{
			write( fd, commandFile.value, strlen(commandFile.value) );
			close(fd);	
		}
	}
	else if(findSubstr(client_message, "read_file")>-1)
	{
		/*action is reading a file
		example: read_file:/sys/class/gpio/gpio5/value
		*/
		struct file_action commandFile;
		strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
		#ifdef ServerDebug
		printf("file to read:%s\n",commandFile.name);
        #endif
        fd = open_file(commandFile.name);
		if (fd<0) {}
		else
		{
			
			read(fd, commandFile.value, sizeof(commandFile.value));
			#ifdef ServerDebug
			printf("value read:%s\n",commandFile.value);
        		#endif
			close(fd);
		    	write(sock , commandFile.value , strlen(commandFile.value));
		}
		
	}
        else if(findSubstr(client_message, "i2c_read")>-1)
        {
        	/*action is reading a I2C register
        	example: i2c_read:/dev/i2c-1;0x1b;0x5d
        	*/
        	struct file_action commandFile;
			strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
			result=read_i2c(commandFile.name);
        	#ifdef ServerDebug
        	printf("value read:%04x\n",result);
        	#endif
			sprintf(commandFile.value, "%04x", result);
        	write(sock , commandFile.value , strlen(commandFile.value));
        }

	client_message[0]='\0';
	sleep(1);
    }
     
    if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(sock);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }
      
    //Free the socket pointer
    free(socket_desc);
     
    return 0;
}
