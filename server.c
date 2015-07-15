#include<stdio.h>
#include<string.h>    //strlen
#include<stdlib.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<signal.h>
#include<pthread.h> //for threading , link with lpthread
 
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
    server.sin_port = htons( 8888 );
     
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
        write(new_socket , message , strlen(message));
        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }\
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
    char *message , client_message[50];
     
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
	printf("%s\n",client_message);
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
