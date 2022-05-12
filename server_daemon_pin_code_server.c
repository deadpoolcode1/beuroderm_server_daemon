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
#include  <setjmp.h>
#include <dirent.h>
#include "inih/ini.h"
#include "parson/parson.h"
#include "i2c.h"
#include "server.h"
#include "server_log.h"
#include "common.h"
#include "sha256/sha-256.h"



int main(void)
{
	char buffer_snp[MAX_SYSTEM_COMMAND_LEN] = {0};	
	char buffer_calculated_valid_pincode_result[MAX_SYSTEM_COMMAND_LEN] = {0};	

    	if (ND_openlog("server_daemon", ND_LOG_DEBUG) != 0) {
		server_daemon_kmsg_print("Error calling ND_openlog. Cannot log to file");
    	}
	
	read_file_data_no_space(SERIAL_NUMBER_FILE, buffer_snp, MAX_SYSTEM_COMMAND_LEN);
	calculate_pincode(buffer_calculated_valid_pincode_result, buffer_snp, strlen(buffer_snp));	
	ND_printlog(ND_LOG_INFO, "pincode to be sent to server:%s\n", buffer_snp);
	ND_printlog(ND_LOG_INFO, "calculated pincode expected from server:%s\n", buffer_calculated_valid_pincode_result);
	return 0;
}

