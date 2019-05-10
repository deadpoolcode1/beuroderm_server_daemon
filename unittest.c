#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h> //inet_addr
#include "jsmn.h"

typedef int bool;
#define true 1
#define false 0

#define default_delay 2

/*
function return index of occurance pattern in string
*/
int findSubstr(char *inpText, char *pattern)
{
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
				return inplen - strlen(inpText + 1);
			}
			if (remTxt == NULL) {
				return -1;
			}
		}
		remPat = pattern;

		inpText++;
	}
	return 0;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
	    strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

static bool inputCheck(int argc , const char *argv[])
{
	// Check for address input and file input
	// Example: ./unittest 10.0.0.36 unittest_def.json
	if (argc != 3) {
		printf("Error, usage: ./unittest [address] [file] \n");
		return false;
	}

	//Reading json file
	if (access(argv[2], R_OK) == -1) {
		printf("file %s not found or permission error\n", argv[2]);
		return false;
	}

	return true;
}

bool sendMessage(char ipAddress[255], char *message, long delay, char *response, int response_on)
{
	int socket_desc;
	struct sockaddr_in server;
	struct timeval tv;
	tv.tv_sec = 5;  /* 1 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors
	char server_reply[128];
	char *test_result;

	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1) {
		puts("Could not create socket");
	}

	server.sin_addr.s_addr = inet_addr(ipAddress);
	server.sin_family = AF_INET;
	server.sin_port = htons(5797);
	setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));

	//Connect to remote server
	if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0) {
		puts("Connection error");
		return false;
	}

	printf("sending: %s %ld %s\n", message, delay, response);

	//Sending the message
	if (send(socket_desc , message , strlen(message) , 0) < 0) {
		puts("Send failed");
		return 1;
	}

	// Receive a reply from the server
	if ((response && !response[0]) || response_on == 0) {
		puts("No Respone");
	} else {
		if (read(socket_desc, server_reply , 128) < 0) {
			puts("recv failed");
			close(socket_desc);
			return 1;

		}

		if (findSubstr(server_reply, response) > -1) {
			test_result = "succeed";
		} else {
			test_result = "failed";
		}
		printf("%ld\n", sizeof(server_reply));
		printf("Response received : %s , test result : %s \n", server_reply, test_result);
	}

	sleep(delay);
	close(socket_desc);

	return true;
}

void checkMessageValues(int delay_on, int response_on, long *delay, char **response)
{
	if (delay_on == 0) {
		*delay = default_delay;
		printf("*Warning : No delay parameter, default value : %lds\n", *delay);
	}

	if (response_on == 0) {
		*response = "";
		printf("*Warning : No response parameter\n");
	}

	if (delay_on == 1 && *delay < default_delay) {
		*delay = default_delay;
		printf("*Warning : Delay parameter most be at least 2s, value changed to : %lds\n", *delay);

	}
}

int main(int argc , const char *argv[])
{

	if (inputCheck(argc, argv) == false) {
		return 1;
	}


	//Json Parser variables
	int i;
	int r;
	jsmn_parser p;
	jsmntok_t t[128]; // We expect no more than 128 tokens

	//Variables for message value
	char *message;
	char *response;
	long delay;
	char *tmp;

	//File reading variables
	FILE *fp;
	long lSize;
	char *JSON_STRING;

	//Server varibles
	char inputAddress[255];
	strcpy(inputAddress, argv[1]);


	printf("Loading tests from %s\n", argv[2]);
	fp = fopen(argv[2] , "rb");

	fseek(fp , 0L , SEEK_END);
	lSize = ftell(fp);
	rewind(fp);

	//allocate memory for entire content
	JSON_STRING = calloc(1, lSize + 1);
	if (!JSON_STRING) {
		fputs("memory alloc fails", stderr);
		fclose(fp);
		return 1;
	}

	if (fread(JSON_STRING , lSize, 1 , fp) != 1) {
		fclose(fp);
		free(JSON_STRING);
		fputs("entire read fails", stderr);
		return 0;
	}


	//Start testing, parse JSON and send commands
	jsmn_init(&p);

	r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t, sizeof(t) / sizeof(t[0]));

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

	for (i = 1; i < r; i++) {
		if (jsoneq(JSON_STRING, &t[i], "message") == 0) {

			if (msg_on == 1) {
				checkMessageValues(delay_on, response_on, &delay, &response);

				if (sendMessage(inputAddress, message, delay, response, response_on) == false) {
					return 1;
				}

				//Resets the temp values
				msg_on = 0;
				response_on = 0;
				delay_on = 0;

				message = NULL;
				response = NULL;
				delay = 1;
			}
			if (msg_on == 0) {
				message = strndup(JSON_STRING + t[i + 1].start, t[i + 1].end - t[i + 1].start);
				// printf("- message: %s\n", message);
				i++;
				msg_on++;
			}
		}
		if (jsoneq(JSON_STRING, &t[i], "delay") == 0) {
			char *delay_string = strndup(JSON_STRING + t[i + 1].start, t[i + 1].end - t[i + 1].start);
			delay = strtol(delay_string, &tmp, 10) ;
			// printf("- delay: %ld\n", delay);
			i++;
			delay_on++;
		}
		if (jsoneq(JSON_STRING, &t[i], "response") == 0) {
			response = strndup(JSON_STRING + t[i + 1].start, t[i + 1].end - t[i + 1].start);
			// printf("- response: %s\n", response);
			i++;
			response_on++;
		} else {
			//printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
			//		JSON_STRING + t[i].start);
		}

		if (msg_on > 1 || response_on > 1 || delay_on > 1) {
			printf("Error: json not in the correct format\n");
			return 1;
		}

	}

	//sending last response
	if (msg_on == 1) {

		checkMessageValues(delay_on, response_on, &delay, &response);

		if (sendMessage(inputAddress, message, delay, response, response_on) == false) {
			return 1;
		}

	}


	fclose(fp);
	free(JSON_STRING);

	return 0;
}




