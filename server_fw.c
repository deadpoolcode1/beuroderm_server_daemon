/** @file
 *
 * @brief server module.
 *
 * service handling requests from APK, used as a binder for APK
 * communication method is based on socket communication
 */
#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <signal.h>
#include <pthread.h> //for threading , link with lpthread
#include <ctype.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/inotify.h>
#include <cutils/klog.h>
#include <ND_LogLibrary.h>
#include <cutils/properties.h>
#include "inih/ini.h"
#include "parson/parson.h"
#include "i2c.h"
#include "server.h"
#include "server_log.h"
#include "common.h"


int exec_system_command(const char * parameter, const char* process_to_execute);






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

uint8_t latest_bittest = PASSED;

int main(void)
{
	int i = 0;
	server_daemon_kmsg_print("--- server daemon fw STARTED ---");
	if (ND_openlog("server_daemon", ND_LOG_DEBUG) != 0) {
		server_daemon_kmsg_print("Error calling ND_openlog. Cannot log to file");
	}
	ND_printlog(ND_LOG_INFO, "periodicmon_handler called\n");
	if (crc_passed(APK_CONFIG_FILE_PATH, FILE_JSON) != 0) {
		ND_printlog(ND_LOG_INFO, "FW file CRC is initially wrong, no monitoring is done\n");
		return 0;
	}
		
	while (1) {
		sleep (10);
		if (latest_bittest != PASSED)
			continue;
		ND_printlog(ND_LOG_INFO, "perfrom periodic crc tests\n");
		for (i = 0; i < NUMBER_RETRY_MONITOR; i++) {
			if (crc_passed(APK_CONFIG_FILE_PATH, FILE_JSON) == 0) {
				latest_bittest = PASSED;
				break;
			}
			else {
				latest_bittest = FAILED;
				ND_printlog(ND_LOG_ERROR, "apk flash periodic monitor failed\n");
			}
		}

		if (latest_bittest == FAILED) {
			ND_printlog(ND_LOG_ERROR, "periodic monitor failed, perform reboot\n");
			exec_system_command(VAR_COMMAND_REBOOT, "/system/bin/ate_commands.sh");		
		}
	}
}





