#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <pthread.h>
#include "i2c.h"
#include "server.h"
#include "common.h"
#include "server_log.h"

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))
#define STATUS_REGISTER 0x01
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

void *wakeup_handler(void *);
int counter_flag = 0;
int alarm_status = 0;
/** @brief function called when timer expires
 */


/** @brief perform I2C read
 */
uint8_t read_i2c(char *path, uint8_t m_address, uint8_t m_register)
{
	int rc = 0, file = 0;
	int16_t read_data = 0;

	file = open(path, O_RDWR);
	if (file < 0)
		goto failed_reading;
	rc = ioctl(file, I2C_SLAVE_FORCE, m_address);
	if (rc < 0)
		goto failed_reading;
	if ((read_data = i2c_smbus_read_byte_data(file, m_register)) == -1)
		goto failed_reading;
	safe_close(file);
	return read_data;
failed_reading:
	ND_printlog(ND_LOG_ERROR, "error, failed reading i2c errno:%s\n", strerror(errno));
	safe_close(file);
	return read_data;
}

int main() {
    int length, i = 0;
    int fd;
    int wd;
    char buffer[BUF_LEN];
    FILE *fptr;
    server_daemon_kmsg_print("--- server daemon STARTED ---");
    if (ND_openlog("server_daemon", ND_LOG_DEBUG) != 0) {
	server_daemon_kmsg_print("Error calling ND_openlog. Cannot log to file");
    }
    ND_printlog(ND_LOG_INFO, "server_Daemon_5v started.\n");
    //create result file if does not exists
    if ((fptr = fopen(HALL_DETECTR, "rb+")) == NULL) //if file does not exist, create it
    {
	ND_printlog(ND_LOG_INFO, "file %s not exist\n", HALL_DETECTR);
	if ((fptr = fopen(HALL_DETECTR, "wb")) == NULL) {
	    PRINTE("error open fw_result_file");
	}  
	fprintf(fptr,"%d",0);
    	fclose(fptr);
    }
    else 
    {
	ND_printlog(ND_LOG_INFO, "file %s exist\n", HALL_DETECTR);
	fclose(fptr);
    }
     	pthread_t wakeup_thread;
	if (pthread_create(&wakeup_thread , NULL ,  wakeup_handler , (void*) NULL) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
	}
  

    while (1) {
    i = 0;
    fd = inotify_init();

    if (fd < 0) {
        PRINTE("inotify_init");
    }
    wd = inotify_add_watch(fd, HALL_DETECTR,  
        IN_MODIFY | IN_CREATE | IN_DELETE);
    length = read(fd, buffer, BUF_LEN);

    if (length < 0) {
        PRINTE("read");
    }

    while (i < length) {
        struct inotify_event *event =
            (struct inotify_event *) &buffer[i];
	ND_printlog(ND_LOG_INFO, "The file %d was modified.\n", event->wd);
        i += EVENT_SIZE + event->len;
	counter_flag = 0;
	if ((fptr = fopen(HALL_DETECTR, "w")) == NULL) {
		PRINTE("error open HALL_DETECTR");
	}

	fprintf(fptr,"%d",1);
	fclose(fptr);
    }

    (void) inotify_rm_watch(fd, wd);
    (void) close(fd);
    }
    return 0;
}


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

void send_alarms(int m_alarm_status)
{
	if (m_alarm_status == 0xff) {
		exec_system_command(VAR_COMMAND_5v_no_communication, "/system/bin/ate_commands.sh");
		return;
	}
	if (m_alarm_status == 0) {
		exec_system_command(VAR_COMMAND_5v_OK, "/system/bin/ate_commands.sh");
		return;
	}

	if (CHECK_BIT(m_alarm_status,0))
		exec_system_command(VAR_COMMAND_5v_BB_OCP, "/system/bin/ate_commands.sh");
	if (CHECK_BIT(m_alarm_status,1))
		exec_system_command(VAR_COMMAND_5v_BB_OVP, "/system/bin/ate_commands.sh");
	if (CHECK_BIT(m_alarm_status,2))
		exec_system_command(VAR_COMMAND_5v_BB_POKn, "/system/bin/ate_commands.sh");
	if (CHECK_BIT(m_alarm_status,3))
		exec_system_command(VAR_COMMAND_5v_TSHDN, "/system/bin/ate_commands.sh");
}



void manage_alarms_5v(int m_alarm_status)
{
	ND_printlog(ND_LOG_INFO, "m_alarm_status:%d\n", m_alarm_status);	
	if (alarm_status != m_alarm_status) {
		alarm_status = m_alarm_status;
		send_alarms(alarm_status);
	}
}

void read_5v_status()
{
	 alarm_status = read_i2c(i2c0_path, i2c_max77818charger_status_address, STATUS_REGISTER);
}

#define READ_TIMING 20
#define COUNTER_TIMES 2 
#define SLEEP_TIME READ_TIMING/COUNTER_TIMES
void *wakeup_handler(void *socket_desc)
{
	
	while (1) {
		sleep(SLEEP_TIME);
		if (counter_flag == 1) {
			ND_printlog(ND_LOG_INFO, "read 5V charger status\n");
			read_5v_status();
		}
		if (counter_flag == COUNTER_TIMES) {
			ND_printlog(ND_LOG_INFO, "use result from 5V charger read as valid reading\n");
			manage_alarms_5v(alarm_status);
			counter_flag = 0;
		}
		else
			counter_flag++;
	}
}

