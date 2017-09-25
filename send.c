#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
 
int main(int argc , char *argv[])
{
    int socket_desc;
    struct sockaddr_in server;
    char *message;
    //example: ./send write_file:/sys/class/gpio/export=5
    //example: ./send read_file:/sys/class/gpio/gpio5/value
    if (argc<2) 
    {
	printf("error, usage: ./send [commnad]\n");
    	return 1;
    } 
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
         
    server.sin_addr.s_addr = inet_addr("10.0.0.36");
    server.sin_family = AF_INET;
    server.sin_port = htons( 5797 );
 
    //Connect to remote server
    if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("connect error");
        return 1;
    }
     
    puts("Connected now\n");
    //Send some data
    message=argv[1];
    //message = "write_file:/sys/class/gpio/unexport=5\n";
    //message = "write_file:test.txt=5\n";
    if( send(socket_desc , message , strlen(message) , 0) < 0)
    {
        puts("Send failed");
        return 1;
    }
    puts("Data Send\n");

    
    close(socket_desc);
    return 0;
}
