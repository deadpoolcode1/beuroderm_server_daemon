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



int main( int argc, char *argv[] )
{
	char buf[MAX_CONF_SIZE] = {0};
	int result = 0;
	if( argc < 2 ) {
		printf("Error, please provide an argument:\r\n");
		printf("  <print> [parameter]           - Print NVM data or specific parameter\r\n");
		printf("  <delete>                      - Delete NVM data (reset to defaults)\r\n");
		printf("  <update> <parameter> <value>  - Update NVM parameter\r\n");
		printf("  <update_ftu_fail> <value>     - Update FTU date with wrong CRC (test failure)\r\n");
		printf("  <verify_crc>                  - Verify NVM CRC integrity\r\n");
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
		if( argc != 4 ) {
			printf("Error, please use as following: <update> <parameter> <value>\r\n");
			return 0;
		}
		nonvolotile_update(argv[2],argv[3]);
	}
	else if (strcmp(argv[1],"update_ftu_fail") == 0) {
		if( argc != 3 ) {
			printf("Error, please use as following: <update_ftu_fail> <ftu_date_value>\r\n");
			printf("Example: update_ftu_fail 2024-01-15:10:30:00\r\n");
			return 0;
		}
		printf("Attempting to update FTU date with intentional CRC failure...\r\n");
		result = nonvolotile_update_ftu_with_crc_fail(argv[2]);
		if (result == 0) {
			printf("FTU update completed (CRC verification unexpectedly passed)\r\n");
		} else {
			printf("FTU update FAILED after 3 retries - NVM CRC integrity check failed\r\n");
			printf("Failure status written to %s\r\n", NVM_WRITE_STATUS_FILE);
		}
		return result;
	}
	else if (strcmp(argv[1],"verify_crc") == 0) {
		printf("Verifying NVM CRC integrity...\r\n");
		result = nonvolotile_verify_crc();
		if (result == 0) {
			printf("NVM CRC verification PASSED\r\n");
		} else {
			printf("NVM CRC verification FAILED - data integrity compromised\r\n");
		}
		return result;
	}
	else
		printf("Error, please provide a valid argument, <print> / <delete> / <update> / <update_ftu_fail> / <verify_crc>\r\n");

	return 0;
}
