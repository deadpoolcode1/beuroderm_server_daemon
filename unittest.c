#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h> //inet_addr
#include "jsmn.h"
// static const char *JSON_STRING =
// 	"{\"unittest\": [{\"message\":\"write_file:/sys/class/backlight/backlight/brightness=7\",\"delay\":\"2\"},{\"message\":\"write_file:/sys/class/backlight/backlight/brightness=2\",\"delay\":\"1\"}]}";




typedef int bool;
#define true 1
#define false 0

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

int main(int argc , char *argv[])
{

    int socket_desc;
    struct sockaddr_in server;
    struct timeval tv;
    tv.tv_sec = 1;  /* 1 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors
	int i;
	int r;
	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */

	char *tmp;

	FILE *fp;
	long lSize;
	char *JSON_STRING;

	// Check for address input
    if (argc<3) 
    {
	printf("Error, usage: ./unittest [address] [file]\n");
    	return 1;
	}

		//reading json file
	if(access(argv[2], R_OK) == -1)
	{
		printf("file %d not found or permission error\n");
		return 1;
	}


	printf("Loading tests from %s\n", argv[2] );
	fp = fopen ( argv[2] , "rb" );

	fseek( fp , 0L , SEEK_END);
	lSize = ftell( fp );
	rewind( fp );

	/* allocate memory for entire content */	
	JSON_STRING = calloc( 1, lSize+1 );
	if( !JSON_STRING ) 
	{
		fputs("memory alloc fails",stderr);
		fclose(fp);
		return 1;
	}
	
	if( fread(JSON_STRING , lSize, 1 , fp)!=1 )
	{
		fclose(fp);
		free(JSON_STRING);
		fputs("entire read fails",stderr);
		return 0;
	}

	// Variables

	char *message , server_reply[2000];
	char *response;
	long delay;
	
	
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
        puts("Connection error");
        return 1;
    }
     
    puts("Connected now\n");

    //Start testing, parse JSON and send commands
	jsmn_init(&p);

	r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t, sizeof(t)/sizeof(t[0]));

	if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
		return 1;
	}

    /* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		printf("Object expected\n");
		return 1;
	}

	int msg_on = 0;
	int delay_on = 0;
	int response_on = 0;
	bool ftitle = false;

	for (i = 1; i < r; i++) 
	{
		if (jsoneq(JSON_STRING, &t[i], "message") == 0) {

			if(msg_on == 1)
			{
				if (delay_on == 0)
				{
					delay = 1;
					printf("*Warning : No delay parameter, default value : %ds\n", delay);
				}

				if (response_on == 0)
				{
					response = "";
					printf("*Warning : No response parameter\n");
				}

				//Sending the message
				socket_desc.flush();
		    	if( send(socket_desc , message , strlen(message) , 0) < 0)
			    {
			        puts("Send failed");
			        return 1;
			    }
			    puts("Data Send\n");

				sleep(delay);

				printf("sending: %s %d %s\n", message, delay, response );

				//Resets the temp values
				msg_on = 0;
				response_on = 0;
				delay_on = 0;

				message = NULL;
				response = NULL;
				delay = 1;
			}
			if(msg_on == 0)
			{
				message = strndup(JSON_STRING + t[i+1].start, t[i+1].end-t[i+1].start);
				// printf("- message: %s\n", message);
				i++;
				msg_on++;
			}
		} 
		if (jsoneq(JSON_STRING, &t[i], "delay") == 0) {
			char *delay_string = strndup(JSON_STRING + t[i+1].start, t[i+1].end-t[i+1].start);
			delay = strtol(delay_string, &tmp, 10) ;
			// printf("- delay: %ld\n", delay);
			i++;
			delay_on++;
		} 
		if (jsoneq(JSON_STRING, &t[i], "response") == 0) {
			response = strndup(JSON_STRING + t[i+1].start, t[i+1].end-t[i+1].start);
			// printf("- response: %s\n", response);
			i++;
			response_on++;
		} 		
		else 
		{
			//printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
			//		JSON_STRING + t[i].start);
		}

		if (msg_on > 1 || response_on > 1 || delay_on > 1)
		{
			printf("Error: json not in the correct format\n");
			return 1;
		}

	}

	//sending last response
	if(msg_on == 1)
	{
		if (delay_on == 0)
		{
			delay = 1;
			printf("*Warning : No delay parameter, default value : %ds\n", delay);
		}

		if (response_on == 0)
		{
			printf("*Warning : No response parameter\n");
		}

		socket_desc.flush();
		//Last test send
		if( send(socket_desc , message , strlen(message) , 0) < 0)
	    {
	        puts("Send failed");
	        return 1;
	    }
	    puts("Data Send\n");
	    printf("sending: %s %d %s\n", message, delay, response );


		fclose(fp);
		free(JSON_STRING);
	}


    //Receive a reply from the server

    if( recv(socket_desc, server_reply , 2000 , 0) < 0)
    {
        puts("recv failed");
        close(socket_desc);
        return 1;

    }
    puts("Reply received:\n");
    puts(server_reply);

    close(socket_desc);
    return 0;
}
