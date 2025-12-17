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
int exec_system_command(const char* parameter, const char* process_to_execute) {
    ND_printlog(ND_LOG_DEBUG, "-- %s:%d -- \n", __func__, __LINE__);

    if (strlen(parameter) > MAX_SYSTEM_COMMAND_LEN)
        return -1;

    if (strncmp(parameter, "sudo", strlen("sudo")) == 0)
        return -1;

    pid_t pid;
    if ((pid = fork()) < 0) {
        ND_printlog(ND_LOG_ERROR, "fork failed");
        return -1;
    }

    if (pid == 0) { // First child
        if ((pid = fork()) < 0) {
            ND_printlog(ND_LOG_ERROR, "second fork failed");
            _exit(1);  // Exit immediately if second fork fails
        }

        if (pid > 0)
            _exit(0); // Exit first child

        // Grandchild process
        execl("/system/bin/sh", "/system/bin/sh", "-C", process_to_execute, parameter, (char *)NULL);
        _exit(1);  // Exit immediately if execl fails
    }

    // Parent process waits for the first child to exit
    waitpid(pid, NULL, 0);

    return 0;
}



#define NVM_FORCE_FAIL_COUNT 3  // Force all 3 retries to fail

int main( int argc, char *argv[] )
{
	char buf[MAX_CONF_SIZE] = {0};
	if( argc < 2 ) {
		printf("Error, please provide an argument, <print> [parameter]/ <delete> / <update> <parameter> <value> [force fail] / <test_nvm_ftu_fail>\r\n");
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
	else if (strcmp(argv[1],"test_nvm_ftu_fail") == 0) {
		// Test command: simulate NVM write failure for FTU date
		// Forces all 3 retries to fail, triggering nvm_write_status error (error code 600)
		printf("Starting NVM FTU date write failure simulation test...\r\n");
		printf("Forcing %d consecutive write failures to trigger NVM error\r\n", NVM_FORCE_FAIL_COUNT);
		nonvolotile_update("ftu_date", "1970-01-01:00:00:00", NVM_FORCE_FAIL_COUNT);
		printf("Test completed - NVM failure should have been raised\r\n");
	}
	else
		printf("Error, please provide a valid argument, <print> / <delete> / <update> <parameter> <value> / <test_nvm_ftu_fail>\r\n");

	return 0;
}
