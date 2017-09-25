#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<signal.h>
#include<pthread.h> //for threading , link with lpthread
#include <fcntl.h>
#include "general.h"
#define ServerDebug 1
#ifdef ServerDebug
//#undef ServerDebug
#endif
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
}
void *connection_handler(void *);

int socket_desc_main;

void sig_handler(int signo)
{
  int j=0;
  if (signo == SIGINT)
    printf("received SIGINT\n");
    close(socket_desc_main);    
    exit(1);
}

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
 
int main(int argc , char *argv[])
{
    int socket_desc , new_socket , c , *new_sock;
    struct sockaddr_in server , client;
    char *message;
     
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
        message = "Hello Client , I have received your connection. And now I will assign a handler for you\n";
        //write(new_socket , message , strlen(message));
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
    char *message , client_message[2000];
    int fd;
    struct file_action
    {
         char name [100];
         char value [100];
    };
    char * my_copy;

    //Send some messages to the client
    message = "Greetings! I am your connection handler\n";
    //write(sock , message , strlen(message));
     
    message = "Now type something and i shall repeat what you type \n";
    // write(sock , message , strlen(message));
     
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
	client_message[0]='\0';
	sleep(1);
    }
     
    if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }
      
    //Free the socket pointer
    free(socket_desc);
     
    return 0;
}
