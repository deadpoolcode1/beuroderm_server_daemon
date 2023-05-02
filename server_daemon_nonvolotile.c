#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  
#include <cutils/klog.h>
#include <ND_LogLibrary.h>
#include <cutils/properties.h> 
#include "inih/ini.h" 
#include "server_log.h"
#include "common.h"
#include "server.h"

/** @brief general function used for executing shell commands from ate_daemon
 */
int exec_system_command(const char * parameter, const char* process_to_execute)
{
	ND_printlog(ND_LOG_DEBUG, "-- %s:%d -- \n", __func__, __LINE__);
	if (strlen(parameter) > MAX_SYSTEM_COMMAND_LEN)
		return -1;
        if (strncmp (parameter,"sudo",strlen("sudo") == 0))
		return -1;
	pid_t my_pid, parent_pid, child_pid;
	if ((child_pid = fork()) < 0) {
		ND_printlog(ND_LOG_ERROR, "fork failed");
		return -1;
	}

	if (child_pid == 0) {
		execl("/system/bin/sh", "/system/bin/sh", "-C", process_to_execute, parameter, (char *)NULL);
		return -1; //return fail if reached here
	};
	return 0;
}



int main( int argc, char *argv[] )
{
	char buf[MAX_CONF_SIZE] = {0};
	if( argc < 2 ) {
		printf("Error, please provide an argument, <print> [parameter]/ <delete> / <update> <parameter> <value> [force fail]\r\n");
		return 0;
	}
	if (strcmp(argv[1],"print") == 0) {
		nonvolotile_readall(buf, MAX_FILE_SIZE);
		if (argc == 2) {
			printf("nonvolotile data:\n%s\r\n", buf, MAX_FILE_SIZE);
			return 0;
		}
		nonvolotile_parse_parameter(argv[2], buf, MAX_CONF_SIZE );
		printf("nonvolotile parameter:\n%s\r\n", buf);
	}
	else if (strcmp(argv[1],"delete") == 0) {
		nonvolotile_delete();	
	}
	else if (strcmp(argv[1],"update") == 0) {
		if( argc != 4 && argc != 5 ) {
			printf("Error, please use as following: <update> <parameter> <value>\r\n");
			return 0;
		}
		if (argc == 5)	
			nonvolotile_update(argv[2],argv[3],atoi(argv[4]));
		else
			nonvolotile_update(argv[2],argv[3],0);	
	}
	else 
		printf("Error, please provide a valid argument, <print> / <delete> / <update> <parameter> <value>\r\n");

	return 0;	
}
