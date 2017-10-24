#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include "jsmn.h"
static const char *JSON_STRING =
	"{\"unittest\": [{\"message\":\"write_file:/sys/class/backlight/backlight/brightness=4\",\"timeout\":\"2\"}]}";



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

    if (argc<2) 
    {
	printf("error, usage: ./unittest [address]\n");
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
    //start testing, parse JSON and send commands
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
	for (i = 1; i < r; i++) 
	{
		if (jsoneq(JSON_STRING, &t[i], "unittest") == 0) 
		{
			int j;
			for (j = 0; j < t[i+1].size; j++) {
				jsmntok_t *g = &t[i+j+2];
				//printf("  * %.*s\n", g->end - g->start, JSON_STRING + g->start);
			}
			i += t[i+1].size + 1;
		} 
		if (jsoneq(JSON_STRING, &t[i], "message") == 0) {
			message = strndup(JSON_STRING + t[i+1].start, t[i+1].end-t[i+1].start);
			printf("- message: %s\n", message);

	    	if( send(socket_desc , message , strlen(message) , 0) < 0)
		    {
		        puts("Send failed");
		        return 1;
		    }
		    puts("Data Send\n");

			i++;
		} 
		if (jsoneq(JSON_STRING, &t[i], "timeout") == 0) {
			char *timeout_string = strndup(JSON_STRING + t[i+1].start, t[i+1].end-t[i+1].start);
			tv.tv_sec = strtol(timeout_string, &tmp, 10) ;
			printf("- timeout: %ld\n", tv.tv_sec);
			i++;
		} 
		if (jsoneq(JSON_STRING, &t[i], "response") == 0) {
			/* We may use strndup() to fetch string value */
			printf("- response: %.*s\n", t[i+1].end-t[i+1].start,
					JSON_STRING + t[i+1].start);
			i++;
		} 		
		else 
		{
			//printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
			//		JSON_STRING + t[i].start);
		}
	}
    message=argv[1];


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