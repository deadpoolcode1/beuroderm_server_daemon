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


timer_t timer_id_rtc_functional;
timer_t timer_id_suspend_to_ram;


struct bittest_info {
	uint8_t i2c_pmic_status;                /*!< i2c0 0x36 */
	uint8_t i2c_fuelgauge_status;           /*!< i2c0 0x66 */
	uint8_t i2c_max77818top_status;         /*!< i2c0 0x69 */
	uint8_t i2c_max77818charger_status;     /*!< i2c0 0x18 */
	uint8_t i2c_displaytouchpanel_status;   /*!< i2c0 0x26 */
	uint8_t i2c_max77816dc3_status;         /*!< i2c2 0x18 */
	uint8_t i2c_rtc_status;                 /*!< i2c2 0x68 */
	uint8_t i2c_rtcmemblock0_status;        /*!< i2c2 0x69 */
	uint8_t i2c_rtcmemblock1_status;        /*!< i2c2 0x6A */
	uint8_t i2c_cradletempsensor_status;    /*!< i2c2 0x48 */
	uint8_t i2c_ioexpender_status;          /*!< i2c2 0x20 */
	uint8_t ble_status;
	uint8_t battery_status;
	uint8_t rtc_functional_status;
	uint8_t fw_crc_status;
	uint8_t apk_crc_status;
	uint8_t language_file_status;
	uint8_t language_video_status;
	uint8_t serial_number_status;
	uint8_t u14_functionality;
	uint8_t audio_files_status;
	int store_rtc_time_data;
	char timestamp [STD_FILE_LENGTH];
	uint8_t bit_status;
	uint8_t bit_test_full_completion;
};

alarms_struct alarms;
struct bittest_info latest_bittest = {PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, PASSED, 0, {0}, BIT_NOT_PERFORMED, BIT_NOT_PERFORMED};
char * return_current_bit_status(char *update_string, bool print_result);

int exec_system_command(const char * parameter, const char* process_to_execute);
uint8_t test_serial_number_validity();


static void try_write_file(char *filename, char *value)
{
	int fd = 0;
	if ((fd = open_file(filename, O_RDWR, EMPTY_MODE)) < 0){
		PRINTE (error_message_fw_error);
	} else {
		if (write(fd, value, strlen(value)) == -1)
			ND_printlog(ND_LOG_ERROR, "error writing file: %s\n", filename);
	}
	safe_close(fd);
}

static void check_rtc_progress(char *filebufferl, size_t size_filebufferl)
{
       char rtc_raw_value[RTC_DATA_LEN] = {0};
       int time_now = 0;
       if (str2int(&latest_bittest.store_rtc_time_data, filebufferl, MAX_ALLOWED_TRAILING_SPACES, size_filebufferl) != STR2INT_SUCCESS)
               return;
       sleep(2);
       ND_printlog(ND_LOG_INFO, "testing if RTC time has progressed\n");
       if (read_file_data(rtc_time_path, rtc_raw_value, sizeof(rtc_raw_value) / sizeof(char)))
                        goto handler_timer_failedb;
       if (str2int(&time_now, rtc_raw_value, MAX_ALLOWED_TRAILING_SPACES, sizeof(rtc_raw_value) / sizeof(char)) != STR2INT_SUCCESS)
                        goto handler_timer_failedb;
       if (time_now  > latest_bittest.store_rtc_time_data)
               latest_bittest.rtc_functional_status = PASSED;
       else
               latest_bittest.rtc_functional_status = FAILED;

       return;
handler_timer_failedb:
        latest_bittest.rtc_functional_status = PASSED;

}

/** @brief function called when timer expires
 */
static void handler_timer(int sig, siginfo_t *si, void *uc)
{
	timer_t *tidp = NULL;
	int time_now  = 0, fd = 0;
	char return_reply[REPLY_STRING_LENGTH] = {0};
	char rtc_raw_value[RTC_DATA_LEN] = {0};
	ND_printlog(ND_LOG_DEBUG, "-- %s:%d -- \n", __func__, __LINE__);
	ND_printlog(ND_LOG_INFO, "Caught signal value %d\n", sig);
	tidp = si->si_value.sival_ptr;
	if (*tidp == timer_id_rtc_functional) {
		ND_printlog(ND_LOG_INFO, "testing if RTC time has progressed\n");
		if (read_file_data(rtc_time_path, rtc_raw_value, sizeof(rtc_raw_value) / sizeof(char)))
			goto handler_timer_failed;
		if (str2int(&time_now, rtc_raw_value, MAX_ALLOWED_TRAILING_SPACES, sizeof(rtc_raw_value) / sizeof(char)) != STR2INT_SUCCESS)
			goto handler_timer_failed;
		if (time_now  > latest_bittest.store_rtc_time_data)
			latest_bittest.rtc_functional_status = PASSED;
		else
			latest_bittest.rtc_functional_status = FAILED;
	} else if (*tidp == timer_id_suspend_to_ram) {
		ND_printlog(ND_LOG_INFO, "suspend to RAM\n");
		try_write_file("/sys/devices/soc0/filling_station-pm/disp_pus_en/value", "0");
		try_write_file("/sys/devices/soc0/filling_station-pm/disp_stby/value", "0");
		try_write_file("/sys/devices/soc0/filling_station-pm/lr_h_inv/value", "0");
		try_write_file("/sys/devices/soc0/filling_station-pm/rst_ctp/value", "0");
		try_write_file("/data/ledblue", "0");
		try_write_file("/sys/power/state", "mem");
		try_write_file("/data/ledblue", "1");
		try_write_file("/sys/devices/soc0/filling_station-pm/disp_pus_en/value", "1");
		try_write_file("/sys/devices/soc0/filling_station-pm/disp_stby/value", "1");
		try_write_file("/sys/devices/soc0/filling_station-pm/lr_h_inv/value", "1");
		try_write_file("/sys/devices/soc0/filling_station-pm/rst_ctp/value", "1");
		exec_system_command(VAR_SEND_KEYCODE_WAKEUP, "/system/bin/ate_commands.sh");
		return;
	}

	if ((return_current_bit_status(return_reply, true)) == NULL)
		ND_printlog(ND_LOG_ERROR, "error, bit test failed to perform\n");

	return;
handler_timer_failed:
	latest_bittest.rtc_functional_status = PASSED;
}

/** @brief creates timer, timer can be set as oneshot or periodic
 *  whenever timer expires the respective handler is being called
 */
int make_timer(char *name, timer_t *timerID, int expire_seconds, int interval_seconds)
{
	struct sigevent         te;
	struct itimerspec       its;
	struct sigaction        sa;
	int                     sigNo = SIGRTMIN;

	ND_printlog(ND_LOG_DEBUG, "-- %s:%d -- \n", __func__, __LINE__);
	/* Set up signal handler. */
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler_timer;
	if (sigemptyset(&sa.sa_mask))
		goto make_timer_error;
	if (sigaction(sigNo, &sa, NULL))
		goto make_timer_error;
	/* Set and enable alarm */
	te.sigev_notify = SIGEV_SIGNAL;
	te.sigev_signo = sigNo;
	te.sigev_value.sival_ptr = timerID;
	if (timer_create(CLOCK_REALTIME, &te, timerID))
		goto make_timer_error;
	its.it_interval.tv_sec = interval_seconds;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = expire_seconds;
	its.it_value.tv_nsec = 0;
	if (timer_settime(*timerID, 0, &its, NULL))
		goto make_timer_error;
	ND_printlog(ND_LOG_INFO, "setup signal handler, action will be performed in %d seconds", expire_seconds);
	return 0;

make_timer_error:
	ND_printlog(ND_LOG_ERROR, "%s: Failed to create timer for %s.\n", __func__, name);
	return -1;
}

/** @brief handles reading ini file for start year parameter
 */
static int handler(void* user, const char* section, const char* name,
		   const char* value)
{
	configuration* pconfig = (configuration*)user;

	if ((strcmp(section, "") == 0) && (strcmp(name, "start_year") != 0))
		return 1;
	pconfig->year = strdup(value);
	return 0;  /* unknown section/name, error */
}

/** @brief general delay function
 */
void delay(unsigned int mseconds)
{
	clock_t goal = mseconds + clock();
	while (goal > clock())
		;
}


void *connection_handler(void *);
void *socket_handler(void *);
void *wakeup_handler(void *);
void *language_handler(void *);
void *pincode_handler(void *);
void *greenled_handler(void *);
void *watchdog_handler(void *);

/** @brief read config data to buffer
 */
int read_config_file_data(char *filename, char *buffer, size_t buffer_size)
{
	char *tmp_buffer = NULL;
	char *line = NULL;
	FILE *fptr = NULL;
	size_t line_size = NULL;
	if ((fptr = fopen(filename, "r")) == NULL)
		goto read_config_file_data_error;
	while (getline(&tmp_buffer, &buffer_size, fptr) != -1) {
		if (strlen(tmp_buffer) > MIN_CONFIG_LINE_LEN) {
			if (check_snprintf(snprintf(buffer + strlen(buffer), buffer_size, "%s", tmp_buffer), buffer_size))
				goto read_config_file_data_error;
		}
	}
	if (fflush(stdout) == EOF)
		goto read_config_file_data_error;
	// make sure we close the filewhen we're
	// finished
	safe_close_stream(fptr);
	FREE (tmp_buffer);
	return 0;

read_config_file_data_error:
	ND_printlog(ND_LOG_ERROR, "Error, failed reading config file %s, %s", filename, strerror(errno));
	if (fptr != NULL)
		safe_close_stream(fptr);
	FREE(tmp_buffer);
	return -1;
}



/** @brief check if file exists
 */
int file_exists(char *filename)
{
	return access(filename, F_OK);
}


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


/** @brief checks if read data from I2C is vald, in Neuroderm implementation, 0XFF is invalid value
 */
uint8_t check_i2c_validity_basic(char *path, uint8_t m_address, uint8_t m_register)
{
	return read_i2c(path, m_address, m_register) == 0xff ? FAILED : PASSED;
}

/** @brief checks if read data from I2C is vald, in Neuroderm implementation, 0XFF is invalid value
 */
uint8_t check_i2c_validity(char *path, uint8_t m_address, uint8_t m_register)
{
	uint8_t res = FAILED;
	uint8_t i = 0;
	for (; i < I2C_MAX_RETRY; i++)
	{	
		res = check_i2c_validity_basic(path, m_address, m_register);
		if (res == PASSED)
			break;
		else
			ND_printlog(ND_LOG_ERROR, "error, i2c bit related read failed\n");	
		usleep(USEC_I2C_RETRY);
	}
	return res;
}

/** @brief return current BIT status, only read existing status not performing test
 */
char * return_current_bit_status(char *update_string, bool print_result)
{
	JSON_Value *root_value;
	JSON_Object *root_object;

	if ((root_value = json_value_init_object()) == NULL)
		ND_printlog(ND_LOG_INFO, "error, failed initilizing json object\n");

	if ((root_object = json_value_get_object(root_value)) == NULL)
		ND_printlog(ND_LOG_INFO, "error, failed getting json root object\n");

	if ((json_object_set_boolean(root_object, BIT_I2C_PMIC, latest_bittest.i2c_pmic_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_FUELGAUGE, latest_bittest.i2c_fuelgauge_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_MAX77818TOP, latest_bittest.i2c_max77818top_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_MAX77818CHARGER, latest_bittest.i2c_max77818charger_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_DISPLAYTOUCHPANEL, latest_bittest.i2c_displaytouchpanel_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_MAX77816DC3, latest_bittest.i2c_max77816dc3_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_RTC, latest_bittest.i2c_rtc_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_RTCMEMBLOCK0, latest_bittest.i2c_rtcmemblock0_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_RTCMEMBLOCK1, latest_bittest.i2c_rtcmemblock1_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_CRADLETEMPSENSOR, latest_bittest.i2c_cradletempsensor_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_I2C_IOEXPENDER, latest_bittest.i2c_ioexpender_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_BLE, latest_bittest.ble_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_BATTERY, latest_bittest.battery_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_RTCFUNCTIONAL, latest_bittest.rtc_functional_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_FWCRC, latest_bittest.fw_crc_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_APKCRC, latest_bittest.apk_crc_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_LANGUAGE_FILE, latest_bittest.language_file_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_LANGUAGE_VIDEO, latest_bittest.language_video_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_SERIAL_NUMBER, latest_bittest.serial_number_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_U14_FUNCTIONALITY, latest_bittest.u14_functionality)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_boolean(root_object, BIT_AUDIO_FILES, latest_bittest.audio_files_status)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((json_object_set_string(root_object, BIT_TIMESTAMP, latest_bittest.timestamp)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((update_string = json_serialize_to_string_pretty(root_value)) == NULL)
		ND_printlog(ND_LOG_ERROR, "error, JSON serilize to string failed\n");
	if (print_result)	
		ND_printlog(ND_LOG_INFO, "bit test: %s\n", update_string);
	json_value_free(root_value);
	if (update_string == NULL)
		PRINTE("error, failed reading bit status\n");
	return update_string;
}

/** @brief create timer, used for entering suspend to RAM mode
 */
void create_suspend_to_ram_timer()
{
	if (timer_id_suspend_to_ram != NULL) {
		if (timer_delete(timer_id_suspend_to_ram)) {
			ND_printlog(ND_LOG_ERROR, "error, failed deleting timer\n");
			return;
		}
	}

	if (make_timer("Suspend To Ram Timer", &timer_id_suspend_to_ram, 2, 0))
		ND_printlog(ND_LOG_ERROR, "error, failed creating timer\n");
}



/** @brief create timer, used for chacking RTC time is progressing
 */
void create_rtc_functional_timer(char *filebufferl, size_t size_filebufferl)
{
	if (str2int(&latest_bittest.store_rtc_time_data, filebufferl, MAX_ALLOWED_TRAILING_SPACES, size_filebufferl) != STR2INT_SUCCESS)
		return;
	
	if (timer_id_rtc_functional != NULL) {
		if (timer_delete(timer_id_rtc_functional)) {
			ND_printlog(ND_LOG_ERROR, "error, failed deleting timer\n");
			return;
		}
	}

	if (make_timer("RTC Functional Timer", &timer_id_rtc_functional, 2, 0))
		ND_printlog(ND_LOG_ERROR, "error, failed creating timer\n");
}

#define STATUS_REGISTER 0x01
uint8_t i2c_max77818charger_status_no_alarms()
{
	uint8_t res = read_i2c(i2c0_path, i2c_max77818charger_status_address, i2c_max77818charger_status_register_detailed);
	return (res == i2c_max77818charger_ok || res == i2c_max77818charger_ok + 1) ? PASSED : FAILED;
}
/** @brief testing I2C valid communication to devices
 */
void i2c_communications_tests()
{
	latest_bittest.i2c_pmic_status = check_i2c_validity(i2c0_path, i2c_pmic_status_address, i2c_pmic_status_register);
	latest_bittest.i2c_fuelgauge_status = check_i2c_validity(i2c0_path, i2c_fuelgauge_status_address, i2c_fuelgauge_status_register);
	latest_bittest.i2c_max77818top_status = check_i2c_validity(i2c0_path, i2c_max77818top_status_address, i2c_max77818top_status_register);
	latest_bittest.i2c_max77818charger_status = PASSED; // U14 component removed - skip test
	latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c0_path, i2c_displaytouchpanel_status_address, i2c_displaytouchpanel_status_register);
	latest_bittest.i2c_max77816dc3_status = check_i2c_validity(i2c2_path, i2c_max77816dc3_status_address, i2c_max77816dc3_status_register);
	latest_bittest.i2c_rtc_status = check_i2c_validity(i2c2_path, i2c_rtc_status_address, i2c_rtc_status_register);
	latest_bittest.i2c_rtcmemblock0_status = check_i2c_validity(i2c2_path, i2c_rtcmemblock0_status_address, i2c_rtcmemblock0_status_register);
	latest_bittest.i2c_rtcmemblock1_status = check_i2c_validity(i2c2_path, i2c_rtcmemblock1_status_address, i2c_rtcmemblock1_status_register);
	latest_bittest.i2c_cradletempsensor_status = check_i2c_validity(i2c2_path, i2c_cradletempsensor_status_address, i2c_cradletempsensor_status_register);
	latest_bittest.i2c_ioexpender_status = check_i2c_validity(i2c2_path, i2c_ioexpender_status_address, i2c_ioexpender_status_register);
}

/** @brief compare_md5
 */
uint8_t compare_md5(const char* md5_string_search, const char* saved_file)
{
	char *ptr;
	char file_data[MAX_MD5_DATA_LEN] = {0};
	read_config_file_data(saved_file, file_data, MAX_MD5_DATA_LEN);
	ptr = strstr(file_data, md5_string_search);
	ND_printlog(ND_LOG_INFO, "md5_string_search:%s\n",md5_string_search);
	ND_printlog(ND_LOG_INFO, "file_data:%s\n",file_data);
	return (ptr!= NULL)? PASSED : FAILED; 	
}

/** @brief perform languge test
 */
uint8_t test_languge(char *test_dir, char *dir_md5_file, const char* lang_to_test)
{
	DIR *d;
	struct dirent *dir;
	char lang_type[MAX_MD5_DATA_LEN] = {0};
	char md5_calculated[MAX_MD5_DATA_LEN] = {0};
	char command[MAX_SYSTEM_COMMAND_LEN] = {0};
	char md5_string_search[MAX_SYSTEM_COMMAND_LEN] = {0};
	uint8_t reply = FAILED;

	ND_printlog(ND_LOG_INFO, "test_dir: %s dir_md5_file:%s\n", test_dir, dir_md5_file, lang_to_test);
	if (strcmp(lang_to_test, NO_LANG) == 0) {
		read_file_data_no_space(LANGUGE_FILE, lang_type, MAX_SYSTEM_COMMAND_LEN);
		if (strlen (lang_type) < 2) {
			ND_printlog(ND_LOG_INFO, "languge not selected yet, pass test");
			return PASSED;
		}
	} else {
		sprintf(lang_type,"%s", lang_to_test);
	}
	d = opendir(SDCARD_DIR);
	while (!d)
	{
		d = opendir(SDCARD_DIR);
	}

	d = opendir(test_dir);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
        	{
	    		if (strlen(dir->d_name) < 3)
				continue;
			if (strcmp(dir->d_name, lang_type) == 0) {
				ND_printlog(ND_LOG_INFO, "check md5 for directory:%s\n",dir->d_name);
	    			sprintf(command,"%s%s",test_dir, dir->d_name);
	    			exec_system_command(command, MD5_SCRIPT);//write md5 to temp file
	    			usleep(DELAY_PER_MD5_CALC);
	    			//read temp file for md5
				while (file_exists(MD5_FILE) < 0) {
				}
				usleep(1000000);
	    			md5_calculated[0] = '\0';
	    			read_file_data(MD5_FILE, md5_calculated, MAX_SYSTEM_COMMAND_LEN);
				sprintf(md5_string_search,"%s=%s",dir->d_name,md5_calculated);
				reply = compare_md5(md5_string_search, dir_md5_file);
	    			closedir(d);
				return reply;
			}
        	}
        	closedir(d);
    	}
	return reply;
}

//uint8_t bittest_performed = 0;
/** @brief perform full bittest
 */
uint8_t bittest_init_full(uint8_t update_status, uint8_t blocking)
{
	char return_reply[REPLY_STRING_LENGTH] = {0};
	char *ptr;
	time_t t = time(NULL);
	struct tm * p = localtime(&t);
	char filebufferv[SHORT_BUFFER_LEN] = {0};
	char filebuffer[SHORT_BUFFER_LEN] = {0};
	char filebufferl[RTC_DATA_LEN] = {0};
	int battery_value = 0, bit_result = 0;
	long battery_voltage = 0;
	char bit_resultstr[REPLY_STRING_LENGTH] = {0};
	uint8_t value_to_pass;
	char tmp[2] = {0};
	char tmp1[2] = {0};
	
	ND_printlog(ND_LOG_INFO, "\n*** Bit testing procedure started! ***\n");
	i2c_communications_tests();
	value_to_pass = FAILED;
	if (!(read_file_data(ble_result_path, filebuffer, SHORT_BUFFER_LEN))) {
		ND_printlog(ND_LOG_INFO, "\nfilebuffer: %s \n", filebuffer);
		if (strncmp(filebuffer, "pass", 4) == 0)
			value_to_pass = PASSED;
	}
	latest_bittest.ble_status = value_to_pass;

	//first pass battery test, in order to exclude from result that effects system sleep time
	latest_bittest.battery_status = PASSED;
	//languge test
	value_to_pass = FAILED;
	if (test_languge(TEXT_DIR, TEXT_DIR_MD5, NO_LANG))
		value_to_pass = PASSED;

	latest_bittest.language_file_status = value_to_pass;
	//U14 finctionality - component removed, skip test and always pass
	latest_bittest.u14_functionality = PASSED;
       	if (blocking == NON_BLOCKING) {
               if (!read_file_data(rtc_time_path, filebufferl, sizeof(filebufferl) / sizeof(char)))
                       create_rtc_functional_timer(filebufferl,  sizeof(filebufferl) / sizeof(char));
       	} else {
               if (!read_file_data(rtc_time_path, filebufferl, sizeof(filebufferl) / sizeof(char)))
                       check_rtc_progress(filebufferl,  sizeof(filebufferl) / sizeof(char));
       	}

        value_to_pass = FAILED;
	if (crc_passed(FW_CONFIG_FILE_PATH, FILE_INI) == 0)
		value_to_pass = PASSED;
	latest_bittest.fw_crc_status = value_to_pass;

	value_to_pass = FAILED;
	if (crc_passed(APK_CONFIG_FILE_PATH, FILE_JSON) == 0)
		value_to_pass = PASSED;
	latest_bittest.apk_crc_status = value_to_pass;

        //audio files
        value_to_pass = FAILED;
        if (test_languge(AUDIO_DIR, AUDIO_DIR_MD5, "alarms"))
                value_to_pass = PASSED;
        latest_bittest.audio_files_status = value_to_pass;


	if (strftime(latest_bittest.timestamp, STD_FILE_LENGTH, "%c" , p) == 0)
		ND_printlog(ND_LOG_ERROR, "error, failed getting time");

	ND_printlog(ND_LOG_INFO, "\n*** Bit testing procedure endded! ***\n");

//check without battery existance bit, video files bit, and serial number test this effects sleep time 
	bit_resultstr[0] = '\0';	
//make sure to perform test with existance bit, video files bit, and serial number as pass, so it will not effect bit fail sleep behaviure 
	latest_bittest.battery_status = PASSED;
	latest_bittest.language_video_status = PASSED;
	latest_bittest.serial_number_status = PASSED;
	snprintf(bit_resultstr, sizeof(bit_resultstr) / sizeof(char), "%s", return_current_bit_status(bit_resultstr, false));
	ptr = strstr(bit_resultstr, "false");
	if (ptr!= NULL)
		bit_result = FAILED;
	else
		bit_result = PASSED;

	sprintf(tmp, "%d", bit_result);
	sprintf(tmp1, "%d", bit_result);
	
//perform battery test
	value_to_pass = FAILED;
	if (!(read_file_data(battery_exists_path, filebuffer, SHORT_BUFFER_LEN))) {
		//value_to_pass = PASSED;
		if ((str2int(&battery_value, filebuffer, MAX_ALLOWED_TRAILING_SPACES, sizeof(filebuffer) / sizeof(char)) == STR2INT_SUCCESS) && (battery_value > 0))
		{
			//seems like battary exists, varify existance by checking ,avarage measured voltage is above minimal voltage
			if (!(read_file_data(battery_voltage_path, filebufferv, SHORT_BUFFER_LEN))) {
				parse_long(filebufferv, &battery_voltage);
				ND_printlog(ND_LOG_INFO, "battery_voltage: %lu\n", battery_voltage);
				if (battery_voltage > minimium_valid_battery_voltage)
					value_to_pass = PASSED;
			}	
		}
	}
	latest_bittest.battery_status = value_to_pass;
	//serial number test
	value_to_pass = FAILED;
	value_to_pass = test_serial_number_validity();
	latest_bittest.serial_number_status = value_to_pass;

//check with all bit tests, including battery test
	snprintf(bit_resultstr, sizeof(bit_resultstr) / sizeof(char), "%s", return_current_bit_status(bit_resultstr, true));
	ND_printlog(ND_LOG_INFO, "bit_result:%d\n",bit_result);
	if (!update_status)
		return bit_result;

	latest_bittest.bit_status = bit_result;
	sprintf(tmp, "%d", bit_result);

	ND_printlog(ND_LOG_INFO, "LATEST_BIT_STATUS:%s : %s\n",LATEST_BIT_STATUS, tmp1);
	try_write_file(LATEST_BIT_STATUS, tmp1);
	latest_bittest.bit_test_full_completion = PASSED;
        //video test
        value_to_pass = FAILED;
        if (test_languge(VIDEO_DIR, VIDEO_DIR_MD5, NO_LANG))
                value_to_pass = PASSED;
	else
		exec_system_command(VAR_COMMAND_error_in_video_files, "/system/bin/ate_commands.sh");
        latest_bittest.language_video_status = value_to_pass;

	ND_printlog(ND_LOG_INFO, "after video testbit_result:%d\n",bit_result);
        snprintf(bit_resultstr, sizeof(bit_resultstr) / sizeof(char), "%s", return_current_bit_status(bit_resultstr, true));
        ND_printlog(ND_LOG_INFO, "bit_result:%d\n",bit_result);

	return latest_bittest.bit_status;
}

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



void sig_handler(int signo)
{
	if (signo == SIGABRT)
		ND_openlog("SIGABRT", ND_LOG_ERROR);
}

int main(void)
{
	//thread handling detection of suspend to ram \ wakeup by the FW system 
	server_daemon_kmsg_print("--- server daemon STARTED ---");
	if (ND_openlog("server_daemon", ND_LOG_DEBUG) != 0) {
		server_daemon_kmsg_print("Error calling ND_openlog. Cannot log to file");
	}
	signal(SIGABRT, sig_handler);

	pthread_t wakeup_thread;
	if (pthread_create(&wakeup_thread , NULL ,  wakeup_handler , (void*) NULL) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
	}

	pthread_t language_thread;
	if (pthread_create(&language_thread , NULL ,  language_handler , (void*) NULL) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
	}

	pthread_t pincode_thread;
	if (pthread_create(&pincode_thread , NULL ,  pincode_handler , (void*) NULL) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
	}

	pthread_t  greenled_thread;
	if (pthread_create(& greenled_thread , NULL ,  greenled_handler , (void*) NULL) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
	}
	pthread_t  watchdog_thread;
	if (pthread_create(& watchdog_thread , NULL ,  watchdog_handler , (void*) NULL) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
	}
	//thread handling reciving socket connections 
	pthread_t socket_thread;
	while (1) {
	if (pthread_create(&socket_thread , NULL ,  socket_handler , (void*) NULL) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
	}
	pthread_join(socket_thread , NULL);
	}
	return 0;
}

/** @brief handle socket connections.
 */
void *socket_handler(void *test)
{
	int socket_desc = 0, new_socket = 0, c = 0 , *new_sock = NULL;
	struct sockaddr_in server , client;
	
	//Create socket
	if ((socket_desc = socket(AF_INET , SOCK_STREAM , 0)) == -1)
		PRINTE("error, Could not create socket\n");
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(5797);

	//Bind
	if (bind(socket_desc, (struct sockaddr *)&server , sizeof(server)) < 0) {
		close(socket_desc);
		PRINTE("error, bind failed\n");
	}
	ND_printlog(ND_LOG_INFO, "bind done\n");
	//Listen
	if (listen(socket_desc , SOMAXCONN) == -1) {	
		close(socket_desc);
		PRINTE("error, can't listen to port\n");
	}
	//Accept and incoming connection
	pthread_t sniffer_thread;
	ND_printlog(ND_LOG_INFO, "Waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);
	while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
		if (new_socket == -1)
			ND_printlog(ND_LOG_INFO, "server Connection accepted\n");
		//Reply to the client
		//pthread_t sniffer_thread;
		if ((new_sock = malloc(sizeof(int))) == NULL) {
			FREE(new_sock);
			PRINTE ("error, Unable to allocate buffer");
			return (void *)0;
		}
		*new_sock = new_socket;
		if (new_socket < 0) {
			exit(-1);
			//ND_printlog(ND_LOG_ERROR, "error, server accept failed\n");
			return (void *)0;
		}
		if (pthread_create(&sniffer_thread , NULL ,  connection_handler , (void*) new_sock) !=0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return (void *)0;
		}
		pthread_detach(sniffer_thread);
		//Now join the thread , so that we dont terminate before the thread
		//pthread_join(sniffer_thread , NULL);
		//FREE(new_sock);
		ND_printlog(ND_LOG_INFO, "server Handler assigned\n");
	}

	if (new_socket < 0) {
		ND_printlog(ND_LOG_ERROR, "error, server accept failed\n");
		return (void *)0;
	}

	return (void *)0;
}

/** @brief handle socket connection opened.
 *  parse message recived, perform needed actions
 *  and reply accordinglly
 */
void *connection_handler(void *socket_desc)
{
	//Get the socket descriptor
	int sock = *(int*)socket_desc, read_size = 0, fd = 0, date_year = 0;
	char *ptr;
	char client_message[SOCKET_MESSAGE_MAX_LENGTH] = {0};
	char returnMsg[SOCKET_MESSAGE_MAX_LENGTH] = {0};
	char error_msg[STD_FILE_LENGTH];
	char read1[STD_FILE_LENGTH] = {0};
	char read2[STD_FILE_LENGTH] = {0};
	char read3[STD_FILE_LENGTH] = {0};
	char read4[STD_FILE_LENGTH] = {0};
	char buffer_read_file[FILE_BUFFER_LEN] = {0};
	char type[STD_FILE_LENGTH] = {0};
	size_t size_of_array_read_file = sizeof(buffer_read_file);
	struct file_action {
		char name [STD_FILE_LENGTH];
		char value [STD_FILE_LENGTH];
	};

	ND_printlog(ND_LOG_INFO, "connection_handler called:\n");
	struct file_action commandFile  = { {0}, {0}};

	//Receive a message from client
	while ((read_size = recv(sock , client_message , SOCKET_MESSAGE_MAX_LENGTH , 0)) > 0) {
		//Send the message back to client
		ND_printlog(ND_LOG_INFO, "message:%s\n", client_message);
		//now make action according to messaage
		if ((ptr = strstr(client_message, "write_file")) != NULL) {
			/*action is writing to a file
			example: write_file:/sys/class/gpio/export=5
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s", strstr(client_message, ":") + 1), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if ((ptr = strstr(commandFile.name, "=")) == NULL)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			else
				*ptr  =  '\0';
			//	commandFile.name[findSubstr(commandFile.name, "=") - 1] = '\0';
			ND_printlog(ND_LOG_INFO, "file to write:%s\n", commandFile.name);
			//filter wireless charger command in case wc alarm is open
			if (((strcmp(commandFile.name, "/sys/devices/soc0/filling_station-pm/wpc_stby/value") == 0) && alarms.wc_high_temperature_alarm == 1) ||
				((strcmp(commandFile.name, "/data/wc_enable_disable_api") == 0) && alarms.wc_high_temperature_alarm == 1)) {
				ND_printlog(ND_LOG_INFO, "disable WC status change, wc alarm status open and FW limits access in this case");
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				goto end_selection;
			}
			if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", strstr(client_message, "=") + 1), sizeof(commandFile.value) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "value:%s\n", commandFile.value);
			//is case ftu is done, APK sends command /data/IsFTU=0. and previuse value must be /data/IsFTU=2, in this case we just completed FTU and save FTU date
			if (strcmp(commandFile.name, "/data/IsFTU") == 0 && strncmp(commandFile.value, "0",1) == 0) {
				//check if prev FTU value is 2
				read_file_data_no_space("/data/IsFTU", read1, 1);
				if (strncmp(read1,"2", 1) == 0)
					exec_system_command("", "/system/bin/saveftudate_script.sh");
			}
			fd = open_file(commandFile.name, O_RDWR | O_TRUNC, EMPTY_MODE);
			if (fd < 0) {
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_NACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			} else {
				if (write(fd, commandFile.value, strlen(commandFile.value)) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				safe_close(fd);
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			}
		} else if ((ptr = strstr(client_message, "read_file")) != NULL) {
			/*action is reading a file
			example: read_file:/sys/class/gpio/gpio5/value
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s",  strstr(client_message, ":") + 1), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "file to read:%s\n", commandFile.name);
			if ((fd = open_file(commandFile.name, O_RDONLY, EMPTY_MODE)) < 0) {
				if (check_snprintf(snprintf(error_msg, sizeof(error_msg) / sizeof(char), "%s", "error, unable to read the value"), sizeof(error_msg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (write(sock , error_msg , strlen(error_msg)) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_NACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			} else {

				read(fd, commandFile.value, sizeof(commandFile.value));
				ND_printlog(ND_LOG_INFO, "value read:%s\n", commandFile.value);
				safe_close(fd);
				if (send(sock , commandFile.value , sizeof(commandFile.value), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				commandFile.value[0] = '\0';
			}
		} else if ((ptr = strstr(client_message, "write_bit:bittest")) != NULL) {
			/*action is bittest_init
			*/
			if (check_snprintf(snprintf(type, sizeof(type) / sizeof(char), "%s", strstr(client_message, ":") + 1), sizeof(type) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "bittest init type:%s\n", type);
			bittest_init_full(UPDATE_STATUS, NON_BLOCKING);
			if (write(sock , REPLY_ACK , strlen(REPLY_ACK)) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_nonvolotile")) != NULL) {
			/*action is reading nonvolotile value
			example: read_nonvolotile:main_sn
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s",  strstr(client_message, ":") + 1), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "nonvolotile value to read:%s\n", commandFile.name);
			nonvolotile_parse_parameter(commandFile.name, commandFile.value, sizeof(commandFile.value));
			ND_printlog(ND_LOG_INFO, "value read:%s\n", commandFile.value);
			if (send(sock , commandFile.value , sizeof(commandFile.value), 0) == -1)
			ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			commandFile.value[0] = '\0';
		}  else if ((ptr = strstr(client_message, "read_bit:bittest")) != NULL) {
			/*action is bittest_read
			*/
			client_message[0] = '\0';
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", return_current_bit_status(returnMsg, true)), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (write(sock , returnMsg , strlen(returnMsg)) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_time")) != NULL) {
			/*action is reading current time
			example: ./send.o 10.0.0.36 read_time
			*/
			time_t t = time(NULL);
			struct tm * p = localtime(&t);
			strftime(returnMsg, REPLY_STRING_LENGTH, "%c" , p);
			if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "write_time")) != NULL) {
			if (fflush(stdin) == EOF)
				ND_printlog(ND_LOG_ERROR, "Error,flushing stream %s", strerror(errno));
			/*action is writing time
			example: ./send.o 10.0.0.36 write_time:060911052016.00
			*/
			if (check_snprintf(snprintf(commandFile.value, YEAR_STRING_LEN, "%s", strstr(client_message, ".") - 4), sizeof(commandFile.value) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (str2int(&date_year, commandFile.value, YEAR_STRING_LEN, sizeof(commandFile.value) / sizeof(char)) != STR2INT_SUCCESS)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "year:%d\n", date_year);
			if (date_year > MAX_YEAR_ALLOWED) {  //maximal year allowed on this Android OS
				ND_printlog(ND_LOG_ERROR, "year set is greater then maximal allowed year on OS");
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				goto end_selection;
			}
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s", strstr(client_message, ":") + 1), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "time:%s\n", commandFile.name);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK) , sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			exec_system_command(commandFile.name, "/system/bin/date_script.sh");
		} else if ((ptr = strstr(client_message, "write_sleep_time")) != NULL) {
			if (fflush(stdin) == EOF)
				ND_printlog(ND_LOG_ERROR, "Error,flushing stream %s", strerror(errno));
			/*action is writing sleep time
			example: ./send.o 10.0.0.36 write_sleep_time:30000
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s", strstr(client_message, ":") + 1), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "sleep time:%s\n", commandFile.name);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK) , sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			exec_system_command(commandFile.name, "/system/bin/sleep_time_script.sh");
		}

		else if ((ptr = strstr(client_message, "read_command:info")) != NULL) {
			// Response: {"build_date": "millis","fw_version": "string_version","device_name": "string_name"}
			read_file_data_no_space("/data/ro_bootimage_build_date_utc", read1, size_of_array_read_file);
			read_file_data_no_space("/data/fs_bsp_version", read2, size_of_array_read_file);
			read_file_data_no_space("/data/ro_product_device", read3, size_of_array_read_file);
			read_file_data_no_space("/data/fs_bsp_version", buffer_read_file, size_of_array_read_file);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), " {\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"} \n", "build_date", read1, "fw_version", read2, "device_name", read3), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (write(sock , returnMsg , strlen(returnMsg)) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		}


		else if ((ptr = strstr(client_message, "read_command:temp_zones")) != NULL) {
			//Response: {"temp_cpu": "temp","temp_wc": "temp"}
			read_file_data_no_space("/sys/class/thermal/thermal_zone1/temp", read1, size_of_array_read_file);
			read_file_data_no_space("/sys/class/hwmon/hwmon1/temp1_input", read2, size_of_array_read_file);
			read_file_data_no_space("/sys/class/power_supply/battery/temp", read3, size_of_array_read_file);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), " {\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"} \n", "temp_cpu", read1, "temp_wc", read2, "temp_batt", read3), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (write(sock , returnMsg , strlen(returnMsg)) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_command:hw_info")) != NULL) {
			//Response: {"hall_status":"0\1","battery_status":"0/1","w_charger_state":"0\1"}
			read_file_data_no_space("/sys/class/switch/hall_detect/state", read1, size_of_array_read_file);
			read_file_data_no_space("/sys/class/power_supply/max77818-charger/online", read2, size_of_array_read_file);
			read_file_data_no_space("/sys/devices/soc0/filling_station-pm/wpc_stby/value", read3, size_of_array_read_file);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), " {\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"} \n", "hall_status", read1
						    , "battery_status_charging", read2,
						    "w_charger_state", read3),  sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (write(sock , returnMsg , strlen(returnMsg)) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_command:config")) != NULL) {
			if ((read_config_file_data("/data/config.file", client_message, SOCKET_MESSAGE_MAX_LENGTH))) {
				if (write(sock , error_message_read_file , strlen(error_message_read_file)) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			} else {
				if (write(sock , client_message , strlen(client_message)) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			}
		} else if ((ptr = strstr(client_message, "read_command:api_version")) != NULL) {
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", API_VERSION), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_command:device_year")) != NULL) {
			if (ini_parse("/data/config.file", handler, &config) < 0) {
				ND_printlog(ND_LOG_ERROR, "error, Can not load 'test.ini'\n");
				goto free_socket;
			}
			ND_printlog(ND_LOG_INFO, "Config loaded from '/data/config.file': device_year=%s\n", config.year);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", config.year), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "write_command:alarms")) != NULL) { //this is a temp fix for passing alarm status untill unify with interrupt script
			char alarm_status [8];
			if (check_snprintf(snprintf(alarm_status, sizeof(alarm_status) / sizeof(char), "%s", ptr + strlen("write_command:alarms")), sizeof(alarm_status) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			alarms.cpu_high_temperature_alarm = (uint8_t)(alarm_status[0] - '0');
			alarms.cpu_critical_temperature_alarm = (uint8_t)(alarm_status[1] - '0');
			alarms.wc_high_temperature_alarm = (uint8_t)(alarm_status[2] - '0');
			alarms.battery_high_temperature_alarm = (uint8_t)(alarm_status[3] - '0');
			alarms.battery_critical_temperature_alarm = (uint8_t)(alarm_status[4] - '0');
			alarms.battery_not_detected = (uint8_t)(alarm_status[5] - '0');
			alarms.wc_error_alarm = (uint8_t)(alarm_status[6] - '0');
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_command:alarms")) != NULL) {
			JSON_Value *root_value = json_value_init_object();
			JSON_Object *root_object = json_value_get_object(root_value);
			char *serialized_string = NULL;
			json_object_set_boolean(root_object, "cpu_high_temperature_alarm", alarms.cpu_high_temperature_alarm);
			json_object_set_boolean(root_object, "cpu_critical_temperature_alarm", alarms.cpu_critical_temperature_alarm);
			json_object_set_boolean(root_object, "wc_high_temperature_alarm", alarms.wc_high_temperature_alarm);
			json_object_set_boolean(root_object, "battery_high_temperature_alarm", alarms.battery_high_temperature_alarm);
			json_object_set_boolean(root_object, "battery_critical_temperature_alarm", alarms.battery_critical_temperature_alarm);
			json_object_set_boolean(root_object, "battery_not_detected", alarms.battery_not_detected);
			json_object_set_boolean(root_object, "wc_error_alarm", alarms.wc_error_alarm);
			if ((serialized_string = json_serialize_to_string_pretty(root_value)) == NULL)
				PRINTE("error, read command:alarms failed\n");
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", serialized_string), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			json_free_serialized_string(serialized_string);
			json_value_free(root_value);
			if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_command:last_reboot_reason")) != NULL) {
			fd = open_file(LAST_REBOOT_FILE_PATH, O_RDONLY, EMPTY_MODE);
			if (fd < 0) {
				if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", REPLY_NACK), sizeof(commandFile.value) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			} else {
				read(fd, commandFile.value, sizeof(commandFile.value));
				if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", commandFile.value), sizeof(commandFile.value) / sizeof(char)))
                                        ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				ND_printlog(ND_LOG_INFO, "value read:%s\n", commandFile.value);
				safe_close(fd);
			}
			if (send(sock , commandFile.value , strlen(commandFile.value), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		}  else if ((ptr = strstr(client_message, "read_command:is_wc_on")) != NULL) {
			read_file_data_no_space("/sys/devices/soc0/filling_station-pm/wpc_led_g/value", read1, size_of_array_read_file);
			read_file_data_no_space("/sys/devices/soc0/filling_station-pm/wpc_stby/value", read2, size_of_array_read_file);
			if (strcmp(read1,"1") == 0 && strcmp(read2,"1") == 0) {
				usleep(UDELAY_WC_IS_ON);
				read_file_data_no_space("/sys/devices/soc0/filling_station-pm/wpc_led_g/value", read3, size_of_array_read_file);
				read_file_data_no_space("/sys/devices/soc0/filling_station-pm/wpc_stby/value", read4, size_of_array_read_file);
			}
			if (strcmp(read1,"1") == 0 && strcmp(read2,"1") == 0 && strcmp(read1,read3) == 0 && strcmp(read2,read4) == 0)
				returnMsg[0] = '1';
			else
				returnMsg[0] = '0';
			returnMsg[1] = '\0';
			if (send(sock , returnMsg , 2, 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "write_command:last_reboot_reason_done")) != NULL) {
			fd = open_file(LAST_REBOOT_FILE_PATH, O_RDWR, EMPTY_MODE);
			if (fd < 0) {
				if (send(sock , REPLY_NACK , strlen(REPLY_NACK), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			} else {
				if (send(sock , REPLY_ACK , strlen(REPLY_ACK), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (write(fd, "POW", strlen("POW")) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				ND_printlog(ND_LOG_INFO, "set watchdog file to POW since state has been read");
				safe_close(fd);
			}
		} else if ((ptr = strstr(client_message, "switch_apk")) != NULL) {
			//example: switch_apk=com.neuroderm.ct_app
			if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", strstr(client_message, "=") + 1), sizeof(commandFile.value) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "value:%s\n", commandFile.value);
			if (send(sock , commandFile.value , sizeof(commandFile.value), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (strcmp(commandFile.value, ND_HOME_APK) == 0)
				exec_system_command(VAR_OPEN_ND_MAIN_APK, "/system/bin/ate_commands.sh");
			else if (strcmp(commandFile.value, ND_CTA_APK) == 0)
				exec_system_command(VAR_OPEN_ND_CTA_APK, "/system/bin/ate_commands.sh");
		} else if ((ptr = strstr(client_message, "ate_mode")) != NULL) {
			//example: ate_mode=1
			if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", strstr(client_message, "=") + 1), sizeof(commandFile.value) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "value:%s\n", commandFile.value);
			if (send(sock , commandFile.value , sizeof(commandFile.value), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (strcmp(commandFile.value, EXITATEMODE) == 0)
				exec_system_command(VAR_COMMAND_EXITATEMODE, "/system/bin/ate_commands.sh");
			else if (strcmp(commandFile.value, ENTERATEMODE) == 0)
				exec_system_command(VAR_COMMAND_ENTERATEMODE, "/system/bin/ate_commands.sh");
		} else if ((ptr = strstr(client_message, "sleep_now")) != NULL) {
			/*action is suspend to ram
			example: sleep_now
			*/
			create_suspend_to_ram_timer();
			if (send(sock , REPLY_ACK , strlen(REPLY_ACK), 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			exec_system_command(VAR_SEND_KEYCODE_SLEEP, "/system/bin/ate_commands.sh");
				exec_system_command(VAR_COMMAND_ENTERATEMODE, "/system/bin/ate_commands.sh");
		}  else if ((ptr = strstr(client_message, "read_android_ready")) != NULL) {
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", "0"), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			returnMsg[0] = '\0';
			//property_get("sys.boot_completed", returnMsg, "0");
			if (latest_bittest.bit_test_full_completion != PASSED)
				returnMsg[0] = '0';
			else
				returnMsg[0] = '1';
			returnMsg[1] = '\0';
			ND_printlog(ND_LOG_INFO, "sys.boot_completed:%s\n", returnMsg);
			if (send(sock , returnMsg , 2, 0) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "write_command:device_charger")) != NULL) {
			//example: write_command:device_charger=1
			if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", strstr(client_message, "=") + 1), sizeof(commandFile.value) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			if (strcmp(commandFile.value, "0") == 0)
				snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", BATTERY_CHARGER_OFF), sizeof(commandFile.value) / sizeof(char);
			else if (strcmp(commandFile.value, "1") == 0)
				snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", BATTERY_CHARGER_ON), sizeof(commandFile.value) / sizeof(char);
			else {
				ND_printlog(ND_LOG_INFO, "invalid value\n");
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				goto end_selection;
			}
			ND_printlog(ND_LOG_INFO, "value:%s\n", commandFile.value);
			fd = open_file("/sys/class/power_supply/max77818-charger/online", O_RDWR, EMPTY_MODE);
			if (fd < 0) {
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_NACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			} else {
				if (write(fd, commandFile.value, strlen(commandFile.value)) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				safe_close(fd);
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				if (send(sock , returnMsg , strlen(returnMsg), 0) == -1)
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			}

		}



		//sleep(1);
	}
end_selection:
	if (read_size == 0) {
		ND_printlog(ND_LOG_INFO, "server Client disconnected");
		if (fflush(stdout) == EOF)
			ND_printlog(ND_LOG_ERROR, "Error,flushing stream %s", strerror(errno));
	} else if (read_size == -1) {
		ND_printlog(ND_LOG_ERROR, "error, server recv failed");
	}
	//Free the socket pointer
free_socket:
	FREE(socket_desc);
	socket_desc = NULL;
	safe_close(sock);
	pthread_exit(NULL);	
	return 0;
}

uint8_t test_if_use_default_key()
{
	char config_file_data[SOCKET_MESSAGE_MAX_LENGTH] = {0};
	char *ptr;
	read_config_file_data(FW_CONFIG_FILE_PATH, config_file_data, SOCKET_MESSAGE_MAX_LENGTH);
	ptr = strstr(config_file_data, USE_DEFAULT_KEY);
	return (ptr!= NULL) ? true : false;
}

/** @brief perform serial nummber validity test
 */
uint8_t test_serial_number_validity()
{
	if (test_if_use_default_key()) {
		ND_printlog(ND_LOG_INFO, "config file use_default_key=1, therefore serial number validity test pass\n");
		return PASSED;
	}

	return (nonvolotile_calculate_crc() == nonvolotile_extract_crc()) ? PASSED : FAILED;
}



/** @brief perform pincode validity test
 */
void test_pincode_validity()
{
	char *pfound = NULL;
	char buffer_pincode[MAX_SYSTEM_COMMAND_LEN] = {0};
	char buffer_calculated_valid_pincode_result[MAX_SYSTEM_COMMAND_LEN] = {0};	
	read_file_data_no_space(RECIVED_PINCODE_NOTIFY_API, buffer_pincode, MAX_SYSTEM_COMMAND_LEN);
	ND_printlog(ND_LOG_INFO, "recived pincode:%s\n", buffer_pincode);
	if (test_if_use_default_key()) {
		sprintf(buffer_calculated_valid_pincode_result, "%s", DEFAULT_PINCODE);
		ND_printlog(ND_LOG_INFO, "default pincode:%s\n", buffer_calculated_valid_pincode_result);
	}
	else {
		calculate_pincode(buffer_calculated_valid_pincode_result, strlen(buffer_calculated_valid_pincode_result));	
		ND_printlog(ND_LOG_INFO, "calculated pincode:%s\n", buffer_calculated_valid_pincode_result);
	}
	if ((pfound = strstr(buffer_calculated_valid_pincode_result, buffer_pincode)) != NULL)
		try_write_file(PINCODE_RESULT_API, "1");
	else
		try_write_file(PINCODE_RESULT_API, "0");
	
}

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    return milliseconds;
}

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))
#define MIN_TIME_BETWEEN_NOTIFICATIONS 3000
#define MIN_TIME_BETWEEN_NOTIFICATIONS_SEC 3

void *wakeup_handler(void *socket_desc)
{
	int wd, fd, length, i=0, bit_result;
	long long bit_time = 0;
	char buffer[BUF_LEN];

	ND_printlog(ND_LOG_INFO, "wakeup handler called OK");
	bittest_init_full(UPDATE_STATUS, BLOCKING);
	fd = inotify_init();
	wd = inotify_add_watch(fd, "/data/sleep_track", IN_MODIFY | IN_CREATE | IN_DELETE);
	while (1) {
		length = read(fd, buffer, BUF_LEN);
		if  (i< length) {
        		struct inotify_event *event =(struct inotify_event *) &buffer[i];
			if (current_timestamp() - MIN_TIME_BETWEEN_NOTIFICATIONS < bit_time) 
				continue;

			bit_time = current_timestamp();

			if (latest_bittest.bit_status == FAILED) {
				ND_printlog(ND_LOG_INFO, "latest bit result failed, do not perform another test\n");
				continue;
			}
			bit_result =  bittest_init_full(UPDATE_STATUS, BLOCKING);
			length = 0;
			ND_printlog(ND_LOG_INFO, "bit result after wakeup:%d\n", bit_result);
			if (bit_result == FAILED)
				exec_system_command(BITTEST_ERROR, "/system/bin/ate_commands.sh");
			if (latest_bittest.battery_status == FAILED) {
				sleep(10);
				exec_system_command(BATTERY_ERROR, "/system/bin/ate_commands.sh");
			}
    		}
	}
	return 0;
}

void *language_handler(void *socket_desc)
{
	int wd, fd, length, i=0, bit_result;
	long long  bit_time = 0;
	char buffer[BUF_LEN];
	uint8_t test_langugetxt_res = 0;
	uint8_t test_langugevid_res = 0;
	
	ND_printlog(ND_LOG_INFO, "language handler called OK");
	fd = inotify_init();
	wd = inotify_add_watch(fd, LANGUGE_FILE, IN_MODIFY | IN_CREATE | IN_DELETE);
	while (1) {
		length = read(fd, buffer, BUF_LEN);
		if  (i< length) {
        		struct inotify_event *event =(struct inotify_event *) &buffer[i];
			if (current_timestamp() - bit_time < MIN_TIME_BETWEEN_NOTIFICATIONS)
				continue;

			bit_time = current_timestamp();
			ND_printlog(ND_LOG_INFO, "language selected, perform CRC test");
			usleep(1000000);
			test_langugetxt_res = test_languge(TEXT_DIR, TEXT_DIR_MD5, NO_LANG);
			usleep(1000000);
			test_langugevid_res = test_languge(VIDEO_DIR, VIDEO_DIR_MD5, NO_LANG);

			ND_printlog(ND_LOG_INFO, "test_langugetxt_res:%d, test_langugevid_res:%d",test_langugetxt_res, test_langugevid_res);
			if (test_langugetxt_res == FAILED) {
				latest_bittest.language_file_status = FAILED;
				exec_system_command(VAR_COMMAND_error_in_languge_files, "/system/bin/ate_commands.sh");
				try_write_file(LATEST_BIT_STATUS, "0");  //this fails the BIT status
			}

			if (test_langugevid_res == FAILED) {
				latest_bittest.language_video_status = FAILED;
				exec_system_command(VAR_COMMAND_error_in_video_files, "/system/bin/ate_commands.sh");
			}
			else {
				latest_bittest.language_video_status = PASSED;
			}
			
		}
	}
	return 0;
}


void *pincode_handler(void *socket_desc)
{
	int wd, fd, length, i=0, bit_result;
	long long  bit_time = 0;
	char buffer[BUF_LEN];

	ND_printlog(ND_LOG_INFO, "pincode handler called OK");
	while (1) {
	fd = inotify_init();
	wd = inotify_add_watch(fd, RECIVED_PINCODE_NOTIFY_API, IN_MODIFY | IN_CREATE | IN_DELETE);
	
		length = read(fd, buffer, BUF_LEN);
		if  (i< length) {
        		struct inotify_event *event =(struct inotify_event *) &buffer[i];

			ND_printlog(ND_LOG_INFO, "pincode entered, check if pincode is valid");
			test_pincode_validity();
			}
    	(void) inotify_rm_watch(fd, wd);
    	(void) close(fd);
	}
	return 0;
}


#define MIN_TIME_BETWEEN_WD 500
void *greenled_handler(void *socket_desc)
{
	char read1[SHORT_FILE_LEN] = {0};
	char read2[SHORT_FILE_LEN] = {0};
	char read3[SHORT_FILE_LEN] = {0};
	FILE *fptr;

    	//create result file if does not exists
    	if ((fptr = fopen(WATCHDOGTEST_FILE, "rb+")) == NULL) {
		ND_printlog(ND_LOG_INFO, "file %s not exist\n", WATCHDOGTEST_FILE);
		if ((fptr = fopen(WATCHDOGTEST_FILE, "wb")) == NULL) {
	    		PRINTE("error open fw_result_file");
		}  
		fprintf(fptr,"%d",0);
    		fclose(fptr);
    	}
    	else {
		ND_printlog(ND_LOG_INFO, "file %s exist\n", WATCHDOGTEST_FILE);
		fclose(fptr);
    	}

	ND_printlog(ND_LOG_INFO, "greenled handler called OK");
	while (1) {
			if (!check_if_file_exists(GREEN_LED_FILE)) {
				usleep(UDELAY_GREEN_LED_READ);
				continue;
			}
			read_file_data_no_space(GREEN_LED_FILE, read1, SHORT_FILE_LEN);
			if (strcmp(read1, read2) != 0) {
				sprintf(read2,"%s",read1);
				ND_printlog(ND_LOG_INFO, "greenled changed: %s", read1);
				read_file_data_no_space("/sys/devices/soc0/filling_station-pm/wpc_stby/value", read3, SHORT_FILE_LEN);
				if (strcmp(read3,"1") == 0 && strcmp(read1,"0") == 0) {	
					ND_printlog(ND_LOG_INFO, "green led zero while wc enabled");		
					exec_system_command(VAR_COMMAND_greenled_zero_while_wc_on, "/system/bin/ate_commands.sh");		
				}			
			}
			usleep(UDELAY_GREEN_LED_READ);
	}
	return 0;
}

void *watchdog_handler(void *socket_desc)
{
	ND_printlog(ND_LOG_INFO, "greenled handler called OK");
	while (1) {
			exec_system_command(VAR_COMMAND_wd_keepalive, "/system/bin/ate_commands.sh");
			usleep(UDELAY_KEEPALIVE);
	}
	return 0;
}
