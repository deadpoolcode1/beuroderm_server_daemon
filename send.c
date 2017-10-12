#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
 
int main(int argc , char *argv[])
{
    int socket_desc;
    struct sockaddr_in server;
    struct timeval tv;
	tv.tv_sec = 1;  /* 1 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors
	

    //example: ./send write_file:/sys/class/gpio/export=5
    //example: ./send read_file:/sys/class/gpio/gpio5/value
    if (argc<3) 
    {
	printf("error, usage: ./send [address][commnad]\n");
    	return 1;
    } 
    char *message , server_reply[2000];
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
         
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons( 5797 );
 	setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));
    //Connect to remote server
    if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("connect error");
        return 1;
    }
     
    puts("Connected now\n");
    //Send some data
    message=argv[2];
    if( send(socket_desc , message , strlen(message) , 0) < 0)
    {
        puts("Send failed");
        return 1;
    }
    puts("Data Send\n");

    //Receive a reply from the server

    if( recv(socket_desc, server_reply , 2000 , 0) < 0)
    {
        puts("recv failed");
        close(socket_desc);
        return 1;

    }
    puts("Reply received:\n");
    puts(server_reply);
    puts("Data Send1\n");
    close(socket_desc);
    puts("Data Send2\n");
    return 0;
}
