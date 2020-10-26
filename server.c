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
#include <cutils/klog.h>
#include <ND_LogLibrary.h>
#include <cutils/properties.h>
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
	int store_rtc_time_data;
	char timestamp [STD_FILE_LENGTH];
	uint8_t timer_flag;
};

alarms_struct alarms;

struct bittest_info latest_bittest = {FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, FAILED, 0, {0}, 0};
char * return_current_bit_status(char *update_string);
int read_file_data(char *filename, char *buffer, size_t buffer_size);
int exec_system_command(const char * parameter, const char* process_to_execute);

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

	if ((return_current_bit_status(return_reply)) == NULL)
		ND_printlog(ND_LOG_ERROR, "error, bit test failed to perform\n");
	return;
handler_timer_failed:
	latest_bittest.rtc_functional_status = FAILED;
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

int socket_desc_main = 0;

/** @brief remove space charecters from string
 */
char * trim(char * s)
{
	int l = strlen(s);

	while (isspace(s[l - 1])) --l;
	while (* s && isspace(* s)) ++s, --l;

	return strndup(s, l);
}


/** @brief read file line by line, return only last line
 */
int read_file_data(char *filename, char *buffer, size_t buffer_size)
{
	FILE *fptr = NULL;

	if ((fptr = fopen(filename, "r")) == NULL)
		goto read_file_data_error;
	while (-1 != getline(&buffer, &buffer_size, fptr)) {
	}
	if (fflush(stdout) == EOF)
		goto read_file_data_error;
	// make sure we close the filewhen we're
	// finished
	if (fclose(fptr) == EOF)
		goto read_file_data_error;
	return 0;

read_file_data_error:
	ND_printlog(ND_LOG_ERROR, "Error, failed reading file %s, %s", filename, strerror(errno));
	if (fptr != NULL)
		safe_close_stream(fptr);
	return -1;
}


/** @brief read config data to buffer
 */
int read_config_file_data(char *filename, char *buffer, size_t buffer_size)
{
	char *line = NULL;
	FILE *fptr = NULL;
	size_t line_size = NULL;

	if ((fptr = fopen(filename, "r")) == NULL)
		goto read_config_file_data_error;
	while (getline(&line, &line_size, fptr) != -1) {
		if (strlen(line) > MIN_CONFIG_LINE_LEN) {
			ND_printlog(ND_LOG_ERROR, "line %s\n", line);
			if (check_snprintf(snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%s", line), buffer_size))
				goto read_config_file_data_error;
		}
	}
	if (fflush(stdout) == EOF)
		goto read_config_file_data_error;
	// make sure we close the filewhen we're
	// finished
	safe_close_stream(fptr);

	return 0;

read_config_file_data_error:
	ND_printlog(ND_LOG_ERROR, "Error, failed reading config file %s, %s", filename, strerror(errno));
	if (fptr != NULL)
		safe_close_stream(fptr);
	return -1;
}

/** @brief read file data, remove spaces
 */
void read_file_data_no_space(char *filename, char *buffer, size_t buffer_size)
{
	char *pos = NULL;

	if (read_file_data(filename, buffer, buffer_size)) {
		if (check_snprintf(snprintf(buffer, buffer_size, "%s", error_message_read_file), buffer_size))
			ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		return;
	}
	trim(buffer);
	if ((pos = strchr(buffer, '\n')) != NULL)
		* pos = '\0';
}

/** @brief check if file exists
 */
int file_exists(char *filename)
{
	int fd = 0;

	if ((fd = access(filename, F_OK)) == -1)
		ND_printlog(ND_LOG_ERROR, "error, file %s not exits\n", filename);

	return fd;
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
uint8_t check_i2c_validity(char *path, uint8_t m_address, uint8_t m_register)
{

	return read_i2c(path, m_address, m_register) == 0xff ? FAILED : PASSED;
}

/** @brief extract CRC string from full message
 */
char* filter_crc_string(char* input, uint8_t file_type, size_t input_length)
{
	unsigned int i = 0, j = 0;
	char tmp_input[MAX_FILE_SIZE] = {0};
	char *output = input, *pfound = NULL;

	for (i = 0, j = 0; i < strlen(input); i++, j++) {
		if (input[i] != ' ' && input[i] != '\r' && input[i] != '\n')
			output[j] = input[i];
		else
			j--;
	}
	output[j] = 0;
//filter out CRC number in calculation
	if (file_type == FILE_JSON) {
		if ((pfound = strstr(output, CRC_STRING_JSON)) == NULL)
			goto exit_filter_crc_string;
		output[strlen(output) - strlen(pfound) + strlen(CRC_STRING_JSON)] = '\0';
		if (check_snprintf(snprintf(tmp_input + strlen(tmp_input), MAX_FILE_SIZE - strlen(tmp_input), "%s", output), MAX_FILE_SIZE))
			return input;
		if ((pfound = strstr(pfound + strlen(CRC_STRING_JSON) + 1, "\"")) == NULL)
			return ""; //wrong file format, fail the test
		if (check_snprintf(snprintf(tmp_input + strlen(tmp_input), MAX_FILE_SIZE - strlen(tmp_input), "%s", pfound), MAX_FILE_SIZE))
			return input;
		output = tmp_input;
	} else {
		if ((pfound = strstr(output, CRC_STRING_INI)) == NULL) //pointer to the first character found  in the string
			goto exit_filter_crc_string;
		if (check_snprintf(snprintf(tmp_input + strlen(tmp_input), strlen(output) - strlen(pfound) + strlen(CRC_STRING_INI) + 1, "%s", output), MAX_FILE_SIZE))
			ND_printlog(ND_LOG_ERROR, "tmp_input%s\n", tmp_input);

		//	return input;
		output = tmp_input;
	}
exit_filter_crc_string:
	if (check_snprintf(snprintf(input, input_length, "%s", output), input_length))
		ND_printlog(ND_LOG_ERROR, error_message_fw_error);
	return (input);
}

/** @brief extract CRC string from file
 */
long extract_crc_from_file(char *string_containing_crc, uint8_t file_type)
{
	char *pfound = NULL, *eptr = NULL;
	int crc_extracted = 0;
	unsigned int i = 0;

	if (file_type == FILE_JSON) {
		if ((pfound = strstr(string_containing_crc, CRC_STRING_JSON)) == NULL)
			goto extract_crc_from_file_error;
	} else {
		if ((pfound = strstr(string_containing_crc, CRC_STRING_INI)) == NULL)
			goto extract_crc_from_file_error;
	}
	for (; i < strlen(pfound); i++) {
		if (isdigit(pfound[i])) {
			if (str2int(&crc_extracted, pfound + i, MAX_ALLOWED_TRAILING_SPACES, STD_FILE_LENGTH) != STR2INT_SUCCESS)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "\nexpected CRC is %lu\n", crc_extracted);
			break;
		}
	}
	return crc_extracted;
extract_crc_from_file_error:
	ND_printlog(ND_LOG_ERROR, "CRC not found\n");
	return -1;
}

/** @brief check if CRC passed, read CRC from file and compare to the calculated value
 */
int crc_passed(char *filename, uint8_t type)
{
	size_t buffer_size = STD_FILE_LENGTH;
	char *buffer = NULL, *buffer_filtered = NULL;
	char full_buffer[MAX_FILE_SIZE] = {0};
	unsigned char x = 0;
	unsigned short crc_calculated = 0xFFFF, length = 0;
	char *data_p = full_buffer;
	long crc_expected = 0;

	// open the file for reading
	FILE *file = fopen(filename, "r");
	// make sure the file opened properly
	if (NULL == file) {
		ND_printlog(ND_LOG_ERROR, "Cannot open file: %s\n", filename);
		return -1;
	}
	//assign memory for buffer
	if ((buffer = (char *)malloc(buffer_size * sizeof(char))) == NULL) 	
		goto exit_crc_passed;
	//assign memory for filtered buffer
	if ((buffer_filtered = (char *)malloc(buffer_size * sizeof(char))) == NULL) 
		goto exit_crc_passed;
	buffer_filtered[0] = '\0';
	while (-1 != getline(&buffer, &buffer_size, file)) {
		if (check_snprintf(snprintf(&full_buffer[strlen(full_buffer)], sizeof(full_buffer) / sizeof(char), "%s", buffer), sizeof(full_buffer) / sizeof(char)))
			ND_printlog(ND_LOG_ERROR, error_message_fw_error);
	}
	if (fclose(file) == EOF)
		ND_printlog(ND_LOG_ERROR, "Error,closing file %s, %s", filename, strerror(errno));
	crc_expected = extract_crc_from_file(full_buffer, type);
	if (check_snprintf(snprintf(full_buffer, sizeof(full_buffer) / sizeof(char), "%s", filter_crc_string(full_buffer, type, sizeof(full_buffer) / sizeof(char))), sizeof(full_buffer) / sizeof(char)))
		ND_printlog(ND_LOG_ERROR, error_message_fw_error);
	if (fflush(stdout) == EOF)
		ND_printlog(ND_LOG_ERROR, "Error,flushing stream %s", strerror(errno));
	//calculate CRC
	length = strlen(full_buffer);
	while (length--) {
		x = crc_calculated >> 8 ^ *data_p++;
		x ^= x >> 4;
		crc_calculated = (crc_calculated << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
	}
	ND_printlog(ND_LOG_INFO, "\n%s crc_calculated result: %d\n", filename, crc_calculated);
//now read expected CRC
	free(buffer);
	free(buffer_filtered);
	return (crc_calculated == crc_expected) ? 0 : 1;

exit_crc_passed:
	fclose(file);
	PRINTE("error, Unable to allocate buffer, exiting");
	return -1;
}

/** @brief return current BIT status, only read existing status not performing test
 */
char * return_current_bit_status(char *update_string)
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
	if ((json_object_set_string(root_object, BIT_TIMESTAMP, latest_bittest.timestamp)) == JSONFailure)
		ND_printlog(ND_LOG_ERROR, "error, setting JSON object\n");
	if ((update_string = json_serialize_to_string_pretty(root_value)) == NULL)
		ND_printlog(ND_LOG_ERROR, "error, JSON serilize to string failed\n");
	ND_printlog(ND_LOG_INFO, "bit test: %s\n", update_string);
	json_value_free(root_value);
	if (update_string == NULL)
		PRINTE("error, failed reading bit status\n");
	return update_string;
}

/** @brief initilize bittest results
 */
void init_latest_bit_results()
{
	latest_bittest.apk_crc_status = FAILED;
	latest_bittest.battery_status = FAILED;
	latest_bittest.ble_status = FAILED;
	latest_bittest.fw_crc_status = FAILED;
	latest_bittest.i2c_cradletempsensor_status = FAILED;
	latest_bittest.i2c_displaytouchpanel_status = FAILED;
	latest_bittest.i2c_fuelgauge_status = FAILED;
	latest_bittest.i2c_ioexpender_status = FAILED;
	latest_bittest.i2c_max77816dc3_status = FAILED;
	latest_bittest.i2c_max77818charger_status = FAILED;
	latest_bittest.i2c_max77818top_status = FAILED;
	latest_bittest.i2c_pmic_status = FAILED;
	latest_bittest.i2c_rtc_status = FAILED;
	latest_bittest.i2c_rtcmemblock0_status = FAILED;
	latest_bittest.i2c_rtcmemblock1_status = FAILED;
	latest_bittest.rtc_functional_status = FAILED;
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

/** @brief testing I2C valid communication to devices
 */
void i2c_communications_tests()
{
	latest_bittest.i2c_pmic_status = check_i2c_validity(i2c0_path, i2c_pmic_status_address, i2c_pmic_status_register);
	latest_bittest.i2c_fuelgauge_status = check_i2c_validity(i2c0_path, i2c_fuelgauge_status_address, i2c_fuelgauge_status_register);
	latest_bittest.i2c_max77818top_status = check_i2c_validity(i2c0_path, i2c_max77818top_status_address, i2c_max77818top_status_register);
	latest_bittest.i2c_max77818charger_status = check_i2c_validity(i2c0_path, i2c_max77818charger_status_address, i2c_max77818charger_status_register);
	latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c0_path, i2c_displaytouchpanel_status_address, i2c_displaytouchpanel_status_register);
	latest_bittest.i2c_max77816dc3_status = check_i2c_validity(i2c2_path, i2c_max77816dc3_status_address, i2c_max77816dc3_status_register);
	latest_bittest.i2c_rtc_status = check_i2c_validity(i2c2_path, i2c_rtc_status_address, i2c_rtc_status_register);
	latest_bittest.i2c_rtcmemblock0_status = check_i2c_validity(i2c2_path, i2c_rtcmemblock0_status_address, i2c_rtcmemblock0_status_register);
	latest_bittest.i2c_rtcmemblock1_status = check_i2c_validity(i2c2_path, i2c_rtcmemblock1_status_address, i2c_rtcmemblock1_status_register);
	latest_bittest.i2c_cradletempsensor_status = check_i2c_validity(i2c2_path, i2c_cradletempsensor_status_address, i2c_cradletempsensor_status_register);
	latest_bittest.i2c_ioexpender_status = check_i2c_validity(i2c2_path, i2c_ioexpender_status_address, i2c_ioexpender_status_register);
}

/** @brief perform full bittest
 */
void bittest_init_full()
{
	char return_reply[REPLY_STRING_LENGTH] = {0};
	time_t t = time(NULL);
	struct tm * p = localtime(&t);
	char filebuffer[SHORT_BUFFER_LEN] = {0};
	char filebufferl[RTC_DATA_LEN] = {0};
	int battery_value = 0;
	//initially assume all tests failed
	init_latest_bit_results();
	ND_printlog(ND_LOG_INFO, "\n*** Bit testing procedure started! ***\n");
	i2c_communications_tests();
	if (!(read_file_data(ble_result_path, filebuffer, SHORT_BUFFER_LEN))) {
		ND_printlog(ND_LOG_INFO, "\nfilebuffer: %s \n", filebuffer);
		if (strncmp(filebuffer, "pass", 4) == 0)
			latest_bittest.ble_status = PASSED;
	}
	if (!(read_file_data(battery_exists_path, filebuffer, SHORT_BUFFER_LEN))) {
		if ((str2int(&battery_value, filebuffer, MAX_ALLOWED_TRAILING_SPACES, sizeof(filebuffer) / sizeof(char)) == STR2INT_SUCCESS) && (battery_value > 0))
			latest_bittest.battery_status = PASSED;
	}

	if (!read_file_data(rtc_time_path, filebufferl, sizeof(filebufferl) / sizeof(char)))
		create_rtc_functional_timer(filebufferl,  sizeof(filebufferl) / sizeof(char));

	if (crc_passed(FW_CONFIG_FILE_PATH, FILE_INI) == 0)
		latest_bittest.fw_crc_status = PASSED;

	if (crc_passed(APK_CONFIG_FILE_PATH, FILE_JSON) == 0)
		latest_bittest.apk_crc_status = PASSED;

	if (strftime(latest_bittest.timestamp, STD_FILE_LENGTH, "%c" , p) == 0)
		ND_printlog(ND_LOG_ERROR, "error, failed getting time");

	ND_printlog(ND_LOG_INFO, "\n*** Bit testing procedure endded! ***\n");
	return;
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

int main(void)
{
	int socket_desc = 0, new_socket = 0, c = 0 , *new_sock = NULL;
	struct sockaddr_in server , client;

	server_daemon_kmsg_print("--- server daemon STARTED ---");
	if (ND_openlog("server_daemon", ND_LOG_DEBUG) != 0) {
		server_daemon_kmsg_print("Error calling ND_openlog. Cannot log to file");
	}
	bittest_init_full();
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
	socket_desc_main = socket_desc;
	ND_printlog(ND_LOG_INFO, "bind done\n");
	//Listen
	if (listen(socket_desc , SOMAXCONN) == -1) {	
		close(socket_desc);
		PRINTE("error, can't listen to port\n");
	}
	//Accept and incoming connection
	ND_printlog(ND_LOG_INFO, "Waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);
	while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
		if (new_socket == -1)
			latest_bittest.timer_flag = 1;
		ND_printlog(ND_LOG_INFO, "server Connection accepted\n");
		//Reply to the client
		pthread_t sniffer_thread;
		if ((new_sock = malloc(sizeof(int))) == NULL) {
			free(new_sock);
			PRINTE ("error, Unable to allocate buffer");
		}
		*new_sock = new_socket;
		if (pthread_create(&sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
		}
		//Now join the thread , so that we dont terminate before the thread
		pthread_join(sniffer_thread , NULL);
		free(new_sock);
		ND_printlog(ND_LOG_INFO, "server Handler assigned\n");
	}

	if (new_socket < 0) {
		ND_printlog(ND_LOG_ERROR, "error, server accept failed\n");
		return 1;
	}

	return 0;
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
	char buffer_read_file[FILE_BUFFER_LEN] = {0};
	char type[STD_FILE_LENGTH] = {0};
	size_t size_of_array_read_file = sizeof(buffer_read_file);
	struct file_action {
		char name [STD_FILE_LENGTH];
		char value [STD_FILE_LENGTH];
	};
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
			fd = open_file(commandFile.name, O_RDWR, EMPTY_MODE);
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
			bittest_init_full();
			if (write(sock , REPLY_ACK , strlen(REPLY_ACK)) == -1)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
		} else if ((ptr = strstr(client_message, "read_bit:bittest")) != NULL) {
			/*action is bittest_read
			*/
			client_message[0] = '\0';
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", return_current_bit_status(returnMsg)), sizeof(returnMsg) / sizeof(char)))
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
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), " {\"%s\":\"%s\",\"%s\":\"%s\"} \n", "temp_cpu", read1, "temp_wc", read2), sizeof(returnMsg) / sizeof(char)))
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
			char alarm_status [7];
			if (check_snprintf(snprintf(alarm_status, sizeof(alarm_status) / sizeof(char), "%s", ptr + strlen("write_command:alarms")), sizeof(alarm_status) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			alarms.cpu_high_temperature_alarm = (uint8_t)(alarm_status[0] - '0');
			alarms.cpu_critical_temperature_alarm = (uint8_t)(alarm_status[1] - '0');
			alarms.wc_high_temperature_alarm = (uint8_t)(alarm_status[2] - '0');
			alarms.battery_high_temperature_alarm = (uint8_t)(alarm_status[3] - '0');
			alarms.battery_not_detected = (uint8_t)(alarm_status[4] - '0');
			alarms.wc_error_alarm = (uint8_t)(alarm_status[5] - '0');
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
		}  else if ((ptr = strstr(client_message, "read_android_ready")) != NULL) {
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", "0"), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			property_get("sys.boot_completed", returnMsg, "0");
			returnMsg[1] = '\0';
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
	} else if (read_size == -1 && latest_bittest.timer_flag != 1) {
		ND_printlog(ND_LOG_ERROR, "error, server recv failed");
	}
	//Free the socket pointer
free_socket:
	free(socket_desc);
	if (latest_bittest.timer_flag != 1)
		safe_close(sock);
	latest_bittest.timer_flag = 0;
	return 0;
}
