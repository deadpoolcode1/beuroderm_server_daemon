#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  
#include <cutils/klog.h>
#include <ND_LogLibrary.h>
#include <cutils/properties.h>  
#include "server_log.h"
#include "common.h"

#define LANGUGE_DIR "/sdcard/NEURODERM/LANGUAGES/"
#define MAX_SYSTEM_COMMAND_LEN 100
#define MAX_MD5_DATA_LEN 5000


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

int main(void)
{
    DIR *d;
    struct dirent *dir;
    char files_md5_data[MAX_MD5_DATA_LEN] = {0};
    char command[MAX_SYSTEM_COMMAND_LEN] = {0};
    char md5_calculated[MAX_SYSTEM_COMMAND_LEN] = {0};

    server_daemon_kmsg_print("--- server_daemon_monitor_language STARTED ---");
    if (ND_openlog("server_daemon_monitor_language", ND_LOG_DEBUG) != 0) {
	server_daemon_kmsg_print("Error calling ND_openlog. Cannot log to file");
    }
    ND_printlog(ND_LOG_INFO, "server_Daemon_monitor_language started.\n");

    d = opendir(LANGUGE_DIR);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
	    ND_printlog(ND_LOG_INFO, "%s\n", dir->d_name);
	    sprintf(command,"%s%s",LANGUGE_DIR, dir->d_name);
	    exec_system_command(command, MD5_SCRIPT);//write md5 to temp file
	    usleep(DELAY_PER_MD5_CALC);
	    //read temp file for md5
	    md5_calculated[0] = '\0';
	    read_file_data(MD5_FILE, md5_calculated, MAX_SYSTEM_COMMAND_LEN);
            sprintf(files_md5_data,"%s=%s",dir->d_name,md5_calculated);
	    //ND_printlog(ND_LOG_INFO, "filename:%s md5:%s\n", command, files_md5_data);
        }
        closedir(d);
	ND_printlog(ND_LOG_INFO, "finished calculating MD5 values for all directories\n");
    }
    return(0);
}
