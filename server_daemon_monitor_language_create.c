#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>  
#include <cutils/klog.h>
#include <ND_LogLibrary.h>
#include <cutils/properties.h>  
#include "server_log.h"
#include "common.h"

void create_md5_file(char *dir_name, char *md5_dir_name);


/** @brief general function used for executing shell commands from ate_daemon
 */
int exec_system_command2(const char * command, char* reply)
{
   char buffer[REPLY_STRING_LENGTH];

   // Open pipe to file
   FILE* pipe = popen(command, "r");
   if (!pipe) {
      return 1;
   }

   // read till end of process:
   while (!feof(pipe)) {

      // use buffer to read and add to result
      if (fgets(buffer, 128, pipe) != NULL)
         strcat(reply, buffer);
      ND_printlog(ND_LOG_INFO, "buf:%s\n", buffer);
   }

   pclose(pipe);
   return 0;
}

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


/** @brief check if file exists
 */
int file_exists(char *filename)
{
        return access(filename, F_OK);
}

int main(void)
{
    //create video dir MD5 
    create_md5_file(VIDEO_DIR, VIDEO_DIR_MD5);
    usleep(DELAY_PER_MD5_CALC);
    //create text dir MD5 
    create_md5_file(TEXT_DIR, TEXT_DIR_MD5);
    return(0);
}

void create_md5_file(char *dir_name, char *md5_dir_name)
{
    DIR *d;
    struct dirent *dir;
    char files_md5_data[MAX_MD5_DATA_LEN] = {0};
    char command[MAX_SYSTEM_COMMAND_LEN] = {0};
    char md5_calculated[MAX_SYSTEM_COMMAND_LEN] = {0};
    char reply[MAX_SYSTEM_COMMAND_LEN] = {0};

    server_daemon_kmsg_print("--- server_daemon_monitor_language STARTED ---");
    if (ND_openlog("server_daemon_monitor_language", ND_LOG_DEBUG) != 0) {
	server_daemon_kmsg_print("Error calling ND_openlog. Cannot log to file");
    }
    ND_printlog(ND_LOG_INFO, "server_Daemon_monitor_language started.\n");

    //create text dir MD5 

    d = opendir(dir_name);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
	    if (strlen(dir->d_name) < 3)
		continue;
        
	    sprintf(command,"%s%s%s%s","find ",dir_name,dir->d_name," -type f | xargs md5sum | md5sum | cut -d' ' -f1");
	    ND_printlog(ND_LOG_INFO, "command: %s\n", command);
	    reply[0] = '\0';
	    exec_system_command2(command, reply);
            sprintf(files_md5_data+strlen(files_md5_data),"%s=%s",dir->d_name,reply);
	    ND_printlog(ND_LOG_INFO, "directory calculated:%s\n",dir->d_name);
	    ND_printlog(ND_LOG_INFO, "md5::%s\n",reply);
        }
        closedir(d);
	ND_printlog(ND_LOG_INFO, "files_md5_data:%s\n",files_md5_data);
	ND_printlog(ND_LOG_INFO, "finished calculating MD5 values for all directories\n");
	safe_write_file_stream(md5_dir_name, files_md5_data);
    }
}
