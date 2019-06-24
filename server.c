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
#include <cutils/klog.h>
#include <ND_LogLibrary.h>
#include "inih/ini.h"
#include "parson/parson.h"
#include "i2c.h"
#include "server.h"
#include "server_log.h"
#include "common.h"



struct bittest_info {
	uint8_t i2c_pmic_status;                        /*!< i2c0 0x36 */
	uint8_t i2c_fuelgauge_status;           /*!< i2c0 0x66 */
	uint8_t i2c_max77818top_status;         /*!< i2c0 0x69 */
	uint8_t i2c_max77818charger_status;             /*!< i2c0 0x18 */
	uint8_t i2c_displaytouchpanel_status;   /*!< i2c0 0x26 */
	uint8_t i2c_max77816dc3_status;         /*!< i2c2 0x18 */
	uint8_t i2c_rtc_status;                 /*!< i2c2 0x68 */
	uint8_t i2c_rtcmemblock0_status;                /*!< i2c2 0x69 */
	uint8_t i2c_rtcmemblock1_status;                /*!< i2c2 0x6A */
	uint8_t i2c_cradletempsensor_status;    /*!< i2c2 0x48 */
	uint8_t i2c_ioexpender_status;          /*!< i2c2 0x20 */
	uint8_t ble_status;
	uint8_t battery_status;
	uint8_t rtc_functional_status;
	uint8_t fw_crc_status;
	uint8_t apk_crc_status;
	char timestamp [STD_FILE_LENGTH];
};

alarms_struct alarms;


static int handler(void* user, const char* section, const char* name,
		   const char* value)
{
	configuration* pconfig = (configuration*)user;

	if ((strcmp(section, "") == 0) && (strcmp(name, "start_year") != 0))
		return 1;
	pconfig->year = strdup(value);
	return 0;  /* unknown section/name, error */
}

/*
function return index of occurance pattern in string
*/
int findSubstr(char *inpText, char *pattern)
{
	int inplen = strlen(inpText);
	char *remTxt = NULL, *remPat = NULL;
	while (inpText != NULL) {

		remTxt = inpText;
		char *remPat = pattern;

		if (strlen(remTxt) < strlen(remPat)) {
			return -1;
		}
		while (*remTxt++ == *remPat++) {
			if (*remPat == '\0') {
				return inplen - strlen(inpText + 1);
			}
			if (remTxt == NULL) {
				return -1;
			}
		}
		remPat = pattern;
		inpText++;
	}
	return 0;
}

void delay(unsigned int mseconds)
{
	clock_t goal = mseconds + clock();
	while (goal > clock())
		;
}

struct bittest_info latest_bittest;

//handles a new connection
void *connection_handler(void *);

int socket_desc_main = 0;

/*
function to handle ctrl+c response
ensure closing the socket before exiting software
*/
void sig_handler(int signo)
{
	if (signo == SIGINT)
		ND_printlog(ND_LOG_INFO, "received SIGINT\n");
	close(socket_desc_main);
	exit(1);
}

char * trim(char * s)
{
	int l = strlen(s);

	while (isspace(s[l - 1])) --l;
	while (* s && isspace(* s)) ++s, --l;

	return strndup(s, l);
}



int read_file_data(char *filename, char *buffer, size_t buffer_size)
{
	FILE *fptr = NULL;

	if ((fptr = fopen(filename, "r")) == NULL) {
		ND_printlog(ND_LOG_ERROR, "Error failed opening file %s", filename);
		return -1;
	}        // read each line and print it to the screen
	while (-1 != getline(&buffer, &buffer_size, fptr)) {
	}
	fflush(stdout);
	// make sure we close the filewhen we're
	// finished
	fclose(fptr);

	return 0;
}



int read_config_file_data(char *filename, char *buffer, size_t buffer_size)
{
	char *line = NULL;
	FILE *fptr = NULL;
	size_t line_size = NULL;

	if ((fptr = fopen(filename, "r")) == NULL) {
		ND_printlog(ND_LOG_ERROR, "Error failed opening file %s", filename);
		return -1;
	}
	while (getline(&line, &line_size, fptr) != -1) {
		if (strlen(line) > MIN_CONFIG_LINE_LEN) {
			ND_printlog(ND_LOG_ERROR, "strlen(buffer) %d\n", strlen(buffer));
			ND_printlog(ND_LOG_ERROR, "buffer_size %d\n", buffer_size);
			ND_printlog(ND_LOG_ERROR, "line %s\n", line);
			if (check_snprintf(snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%s", line), buffer_size))
				return -1;
		}
	}
	fflush(stdout);

	// make sure we close the filewhen we're
	// finished
	safe_close_stream(fptr);

	return 0;
	// safe_close_stream(fptr);
}


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

int file_exists(char *filename)
{
	int fd = 0;

	fd = access(filename, F_OK);
	if (fd == -1)
		ND_printlog(ND_LOG_ERROR, "error, file %s not exits\n", filename);

	return fd;
}



uint8_t read_i2c(char *path, uint8_t m_address, uint8_t m_register)
{
	int rc = 0, file = 0;
	uint8_t read_data = 0;

	file = open(path, O_RDWR);
	if (file < 0)
		goto failed_reading;
	rc = ioctl(file, I2C_SLAVE_FORCE, m_address);
	if (rc < 0)
		goto failed_reading;
	read_data = i2c_smbus_read_byte_data(file, m_register);
	return read_data;
failed_reading:
	err(errno, "failed reading");
	return read_data;
}

//function tests vale in specified address is not 0xff, thus varifying i2c communication
//return 0 on succesful communication (value not 0xff)
uint8_t check_i2c_validity(char *path, uint8_t m_address, uint8_t m_register)
{

	return read_i2c(path, m_address, m_register) == 0xff ? FAILED : PASSED;
}

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
		pfound = strstr(output, CRC_STRING_JSON); //pointer to the first character found  in the string
		if (pfound == NULL)
			goto exit_filter_crc_string;
		output[strlen(output) - strlen(pfound) + strlen(CRC_STRING_JSON)] = '\0';
		if (check_snprintf(snprintf(tmp_input + strlen(tmp_input), MAX_FILE_SIZE - strlen(tmp_input), "%s", output), MAX_FILE_SIZE))
			return input;
		pfound = strstr(pfound + strlen(CRC_STRING_JSON) + 1, "\"");
		if (pfound == NULL)
			return ""; //wrong file format, fail the test
		if (check_snprintf(snprintf(tmp_input + strlen(tmp_input), MAX_FILE_SIZE - strlen(tmp_input), "%s", pfound), MAX_FILE_SIZE))
			return input;
		output = tmp_input;
	} else {
		pfound = strstr(output, CRC_STRING_INI); //pointer to the first character found  in the string
		if (pfound == NULL)
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


int place_crc_if_not_exist(char *filename, unsigned short crc)
{
	int fd = 0, ret = -1;
	char write_data[SHORT_BUFFER_LEN] = {0};

	if (access(filename, F_OK) != -1)
		return 0;
	ND_printlog(ND_LOG_INFO, "file not exists, creating file");
	if ((fd = open_file(filename, O_RDWR | O_CREAT, 0666)) < 0)
		return -1;
	if (check_snprintf(snprintf(write_data, sizeof(write_data) / sizeof(char), "%d", crc),  sizeof(write_data) / sizeof(char)))
		goto place_crc_if_not_exist_close;
	write(fd, write_data, strlen(write_data));
	ret = 0;
place_crc_if_not_exist_close:
	close(fd);
	return ret;
}
long extract_crc_from_file(char *string_containing_crc, uint8_t file_type)
{
	char *pfound = NULL, *eptr = NULL;
	int crc_extracted = 0;
	unsigned int i = 0;

	if (file_type == FILE_JSON)
		pfound = strstr(string_containing_crc, CRC_STRING_JSON);
	else
		pfound = strstr(string_containing_crc, CRC_STRING_INI);
	if (pfound == NULL)
		return 0;
	for (; i < strlen(pfound); i++) {
		if (isdigit(pfound[i])) {
			if (str2int(&crc_extracted, pfound + i, MAX_ALLOWED_TRAILING_SPACES, STD_FILE_LENGTH) != STR2INT_SUCCESS)
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "\nexpected CRC is %lu\n", crc_extracted);
			break;
		}
	}
	return crc_extracted;
}

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
	if ((buffer = (char *)malloc(buffer_size * sizeof(char))) == NULL) {
		ND_printlog(ND_LOG_ERROR, "error, Unable to allocate buffer, exiting");
		exit(1);
	}
	//assign memory for filtered buffer
	buffer_filtered = (char *)malloc(buffer_size * sizeof(char));
	if (buffer_filtered == NULL) {
		ND_printlog(ND_LOG_ERROR, "error, Unable to allocate buffer_filtered, exiting");
		exit(1);
	}
	buffer_filtered[0] = '\0';
	while (-1 != getline(&buffer, &buffer_size, file)) {
		if (check_snprintf(snprintf(&full_buffer[strlen(full_buffer)], sizeof(full_buffer) / sizeof(char), "%s", buffer), sizeof(full_buffer) / sizeof(char)))
			ND_printlog(ND_LOG_ERROR, error_message_fw_error);
	}
	fclose(file);
	crc_expected = extract_crc_from_file(full_buffer, type);
	if (check_snprintf(snprintf(full_buffer, sizeof(full_buffer) / sizeof(char), "%s", filter_crc_string(full_buffer, type, sizeof(full_buffer) / sizeof(char))), sizeof(full_buffer) / sizeof(char)))
		ND_printlog(ND_LOG_ERROR, error_message_fw_error);
	fflush(stdout);
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
}


char * return_current_bit_status(char *update_string)
{
	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root_object = json_value_get_object(root_value);

	json_object_set_boolean(root_object, BIT_I2C_PMIC, latest_bittest.i2c_pmic_status);
	json_object_set_boolean(root_object, BIT_I2C_FUELGAUGE, latest_bittest.i2c_fuelgauge_status);
	json_object_set_boolean(root_object, BIT_I2C_MAX77818TOP, latest_bittest.i2c_max77818top_status);
	json_object_set_boolean(root_object, BIT_I2C_MAX77818CHARGER, latest_bittest.i2c_max77818charger_status);
	json_object_set_boolean(root_object, BIT_I2C_DISPLAYTOUCHPANEL, latest_bittest.i2c_displaytouchpanel_status);
	json_object_set_boolean(root_object, BIT_I2C_MAX77816DC3, latest_bittest.i2c_max77816dc3_status);
	json_object_set_boolean(root_object, BIT_I2C_RTC, latest_bittest.i2c_rtc_status);
	json_object_set_boolean(root_object, BIT_I2C_RTCMEMBLOCK0, latest_bittest.i2c_rtcmemblock0_status);
	json_object_set_boolean(root_object, BIT_I2C_RTCMEMBLOCK1, latest_bittest.i2c_rtcmemblock1_status);
	json_object_set_boolean(root_object, BIT_I2C_CRADLETEMPSENSOR, latest_bittest.i2c_cradletempsensor_status);
	json_object_set_boolean(root_object, BIT_I2C_IOEXPENDER, latest_bittest.i2c_ioexpender_status);
	json_object_set_boolean(root_object, BIT_BLE, latest_bittest.ble_status);
	json_object_set_boolean(root_object, BIT_BATTERY, latest_bittest.battery_status);
	json_object_set_boolean(root_object, BIT_RTCFUNCTIONAL, latest_bittest.rtc_functional_status);
	json_object_set_boolean(root_object, BIT_FWCRC, latest_bittest.fw_crc_status);
	json_object_set_boolean(root_object, BIT_APKCRC, latest_bittest.apk_crc_status);
	json_object_set_string(root_object, BIT_TIMESTAMP, latest_bittest.timestamp);
	update_string = json_serialize_to_string_pretty(root_value);
	ND_printlog(ND_LOG_INFO, "bit test: %s\n", update_string);
	json_value_free(root_value);
	return update_string;
}


//fucnction that runs bittest full test
int bittest_init_full()
{
	char return_reply[REPLY_STRING_LENGTH] = {0};
	int32_t rtc_time_value = 0;
	time_t t = time(NULL);
	struct tm * p = localtime(&t);
	int num_val = 0;

	ND_printlog(ND_LOG_INFO, "\n*** Bit testing procedure started! ***\n");
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
	if (file_exists("/dev/hci_tty") > -1)
		latest_bittest.ble_status = PASSED;
	else
		latest_bittest.ble_status = FAILED;
	char *filebuffer = malloc(SHORT_BUFFER_LEN * sizeof(char));
	if (read_file_data(battery_exists_path, filebuffer, SHORT_BUFFER_LEN))
		return -1;
	if (str2int(&num_val, filebuffer, MAX_ALLOWED_TRAILING_SPACES, sizeof(filebuffer) / sizeof(char)) != STR2INT_SUCCESS)
		return -1;
	if (num_val > 0)
		latest_bittest.battery_status = PASSED;
	else
		latest_bittest.battery_status = FAILED;
	free(filebuffer);
	char *filebufferl = malloc(20 * sizeof(char));
	if (read_file_data(rtc_time_read, filebufferl, 20))
		return -1;
	if ((num_val = str2int(&rtc_time_value, filebufferl, MAX_ALLOWED_TRAILING_SPACES, RTC_DATA_LEN)) != STR2INT_SUCCESS)
		return -1;
	//sleep(2);
	//if (atol(read_file_data(rtc_time_read, filebufferl, 20)) - rtc_time_value > 1)
	latest_bittest.rtc_functional_status = PASSED;
	//else
	//	latest_bittest.rtc_functional_status = FAILED;
	free(filebufferl);
	if (crc_passed(FW_CONFIG_FILE_PATH, FILE_INI) == 0)
		latest_bittest.fw_crc_status = PASSED;
	else
		latest_bittest.fw_crc_status = FAILED;
	if (crc_passed(APK_CONFIG_FILE_PATH, FILE_JSON) == 0)
		latest_bittest.apk_crc_status = PASSED;
	else
		latest_bittest.apk_crc_status = FAILED;
	strftime(latest_bittest.timestamp, REPLY_STRING_LENGTH, "%c" , p);
	return_current_bit_status(return_reply);
	ND_printlog(ND_LOG_INFO, "\n*** Bit testing procedure endded! ***\n");
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
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1) {
		ND_printlog(ND_LOG_ERROR, "error, Could not create socket\n");
	}
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(5797);

	//Bind
	if (bind(socket_desc, (struct sockaddr *)&server , sizeof(server)) < 0) {
		ND_printlog(ND_LOG_ERROR, "error, bind failed\n");
		return 1;
	}
	socket_desc_main = socket_desc;
	puts("bind done");
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		ND_printlog(ND_LOG_ERROR, "error, can't catch SIGINT\n");
	//Listen
	listen(socket_desc , 3);

	//Accept and incoming connection
	ND_printlog(ND_LOG_INFO, "Waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);
	while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
		ND_printlog(ND_LOG_INFO, "server Connection accepted\n");
		//Reply to the client
		pthread_t sniffer_thread;
		new_sock = malloc(sizeof(int));
		*new_sock = new_socket;
		if (pthread_create(&sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0) {
			ND_printlog(ND_LOG_ERROR, "error, server could not create thread\n");
			return 1;
		}
		//Now join the thread , so that we dont terminate before the thread
		pthread_join(sniffer_thread , NULL);
		ND_printlog(ND_LOG_INFO, "server Handler assigned\n");
	}

	if (new_socket < 0) {
		ND_printlog(ND_LOG_ERROR, "error, server accept failed\n");
		return 1;
	}

	return 0;
}
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
	//Get the socket descriptor
	int sock = *(int*)socket_desc, read_size = 0, fd = 0;
	char client_message[SOCKET_MESSAGE_MAX_LENGTH] = {0};
	char returnMsg[SOCKET_MESSAGE_MAX_LENGTH] = {0};
	char error_msg[STD_FILE_LENGTH];
	char read1[STD_FILE_LENGTH] = {0};
	char read2[STD_FILE_LENGTH] = {0};
	char read3[STD_FILE_LENGTH] = {0};
	char buffer_read_file[150] = {0};
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
		//write(sock , client_message , strlen(client_message));
		ND_printlog(ND_LOG_INFO, "message:%s\n", client_message);
		//now make action according to messaage
		if (findSubstr(client_message, "write_file") > -1) {
			fflush(stdin);
			/*action is writing to a file
			example: write_file:/sys/class/gpio/export=5
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s", client_message + findSubstr(client_message, ":")), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			commandFile.name[findSubstr(commandFile.name, "=") - 1] = '\0';
			ND_printlog(ND_LOG_INFO, "file to write:%s\n", commandFile.name);
			if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", client_message + findSubstr(client_message, "=")), sizeof(commandFile.value) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "value:%s\n", commandFile.value);
			fd = open_file(commandFile.name, O_RDWR, EMPTY_MODE);
			if (fd < 0) {
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_NACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				send(sock , returnMsg , strlen(returnMsg), 0);
			} else {
				write(fd, commandFile.value, strlen(commandFile.value));
				safe_close(fd);
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				send(sock , returnMsg , strlen(returnMsg), 0);
			}
		} else if (findSubstr(client_message, "read_file") > -1) {
			/*action is reading a file
			example: read_file:/sys/class/gpio/gpio5/value
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s", client_message + findSubstr(client_message, ":")), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "file to read:%s\n", commandFile.name);
			if ((fd = open_file(commandFile.name, O_RDONLY, EMPTY_MODE)) < 0) {
				if (check_snprintf(snprintf(error_msg, sizeof(error_msg) / sizeof(char), "%s", "error, unable to read the value"), sizeof(error_msg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				write(sock , error_msg , strlen(error_msg));
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_NACK), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				send(sock , returnMsg , strlen(returnMsg), 0);
			} else {

				read(fd, commandFile.value, sizeof(commandFile.value));
				ND_printlog(ND_LOG_INFO, "value read:%s\n", commandFile.value);
				safe_close(fd);
				send(sock , commandFile.value , sizeof(commandFile.value), 0);
				commandFile.value[0] = '\0';
			}
		} else if (findSubstr(client_message, "write_bit:bittest") > -1) {
			/*action is bittest_init
			*/
			if (check_snprintf(snprintf(type, sizeof(type) / sizeof(char), "%s", client_message + findSubstr(client_message, ":")), sizeof(type) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "bittest init type:%s\n", type);
			//if(findSubstr(client_message, "full")>-1)
			{
				bittest_init_full();
				if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", return_current_bit_status(returnMsg)), sizeof(returnMsg) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
				write(sock , returnMsg , strlen(returnMsg));
			}
		} else if (findSubstr(client_message, "read_bit:bittest") > -1) {
			/*action is bittest_read
			*/
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", return_current_bit_status(returnMsg)), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			write(sock , returnMsg , strlen(returnMsg));
		} else if (findSubstr(client_message, "read_time") > -1) {
			/*action is reading current time
			example: ./send.o 10.0.0.36 read_time
			*/
			time_t t = time(NULL);
			struct tm * p = localtime(&t);
			strftime(returnMsg, REPLY_STRING_LENGTH, "%c" , p);
			send(sock , returnMsg , strlen(returnMsg), 0);
		} else if (findSubstr(client_message, "write_time") > -1) {
			pid_t my_pid, parent_pid, child_pid;
			fflush(stdin);
			/*action is writing time
			example: ./send.o 10.0.0.36 write_time:060911052016.00
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s", client_message + findSubstr(client_message, ":")), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "time:%s\n", commandFile.name);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK) , sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			send(sock , returnMsg , strlen(returnMsg), 0);
			my_pid = getpid();
			parent_pid = getppid();
			/* print error message if fork() fails */
			if ((child_pid = fork()) < 0) {
				ND_printlog(ND_LOG_ERROR, "error, fork failure");
			}

			if (child_pid == 0) {
				my_pid = getpid();
				parent_pid = getppid();
				execl("/system/bin/sh", "/system/bin/sh", "-C", "/system/bin/date_script.sh", commandFile.name, (char *)NULL);
				ND_printlog(ND_LOG_ERROR, "error, execl() failure!\n");
			};

		} else if (findSubstr(client_message, "write_sleep_time") > -1) {
			pid_t my_pid, parent_pid, child_pid;
			fflush(stdin);
			/*action is writing sleep time
			example: ./send.o 10.0.0.36 write_sleep_time:30000
			*/
			if (check_snprintf(snprintf(commandFile.name, sizeof(commandFile.name) / sizeof(char), "%s", client_message + findSubstr(client_message, ":")), sizeof(commandFile.name) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			ND_printlog(ND_LOG_INFO, "sleep time:%s\n", commandFile.name);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK) , sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			send(sock , returnMsg , strlen(returnMsg), 0);
			my_pid = getpid();
			parent_pid = getppid();
			/* print error message if fork() fails */
			if ((child_pid = fork()) < 0) {
				ND_printlog(ND_LOG_ERROR, "error, fork failure\n");
			}

			if (child_pid == 0) {
				my_pid = getpid();
				parent_pid = getppid();
				execl("/system/bin/sh", "/system/bin/sh", "-C", "/system/bin/sleep_time_script.sh", commandFile.name, (char *)NULL);
				ND_printlog(ND_LOG_ERROR, "error, execl() failure!\n");
			};

		}

		else if (findSubstr(client_message, "read_command:info") > -1) {
			// Response: {"build_date": "millis","fw_version": "string_version","device_name": "string_name"}
			read_file_data_no_space("/data/ro_bootimage_build_date_utc", read1, size_of_array_read_file);
			read_file_data_no_space("/data/fs_bsp_version", read2, size_of_array_read_file);
			read_file_data_no_space("/data/ro_product_device", read3, size_of_array_read_file);
			read_file_data_no_space("/data/fs_bsp_version", buffer_read_file, size_of_array_read_file);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), " {\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"} \n", "build_date", read1, "fw_version", read2, "device_name", read3), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			write(sock , returnMsg , strlen(returnMsg));
		}


		else if (findSubstr(client_message, "read_command:temp_zones") > -1) {
			//Response: {"temp_cpu": "temp","temp_wc": "temp"}
			read_file_data_no_space("/sys/class/thermal/thermal_zone1/temp", read1, size_of_array_read_file);
			read_file_data_no_space("/sys/class/hwmon/hwmon1/temp1_input", read2, size_of_array_read_file);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), " {\"%s\":\"%s\",\"%s\":\"%s\"} \n", "temp_cpu", read1, "temp_wc", read2), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			write(sock , returnMsg , strlen(returnMsg));
		} else if (findSubstr(client_message, "read_command:hw_info") > -1) {
			//Response: {"hall_status":"0\1","battery_status":"0/1","w_charger_state":"0\1"}
			read_file_data_no_space("/sys/class/switch/hall_detect/state", read1, size_of_array_read_file);
			read_file_data_no_space("/sys/class/power_supply/max77818-charger/online", read2, size_of_array_read_file);
			read_file_data_no_space("/sys/class/switch/hall_detect/state", read3, size_of_array_read_file);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), " {\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"} \n", "hall_status", read1
						    , "battery_status_charging", read2,
						    "w_charger_state", read3),  sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			write(sock , returnMsg , strlen(returnMsg));
		} else if (findSubstr(client_message, "read_command:config") > -1) {
			if ((read_config_file_data("/data/config.file", client_message, SOCKET_MESSAGE_MAX_LENGTH)))
				write(sock , error_message_read_file , strlen(error_message_read_file));
			else
				write(sock , client_message , strlen(client_message));
		} else if (findSubstr(client_message, "read_command:api_version") > -1) {
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", API_VERSION), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			send(sock , returnMsg , strlen(returnMsg), 0);
		} else if (findSubstr(client_message, "read_command:device_year") > -1) {
			if (ini_parse("/data/config.file", handler, &config) < 0) {
				ND_printlog(ND_LOG_ERROR, "error, Can not load 'test.ini'\n");
				goto free_socket;
			}
			ND_printlog(ND_LOG_INFO, "Config loaded from '/data/config.file': device_year=%s\n", config.year);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", config.year), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			send(sock , returnMsg , strlen(returnMsg), 0);
		} else if (findSubstr(client_message, "write_command:alarms") > -1) { //this is a temp fix for passing alarm status untill unify with interrupt script
			char alarm_status [7];
			if (check_snprintf(snprintf(alarm_status, sizeof(alarm_status) / sizeof(char), "%s", client_message + findSubstr(client_message, "write_command:alarms") + strlen("write_command:alarms") - 1), sizeof(alarm_status) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			alarms.cpu_high_temperature_alarm = (uint8_t)(alarm_status[0] - '0');
			alarms.cpu_critical_temperature_alarm = (uint8_t)(alarm_status[1] - '0');
			alarms.wc_high_temperature_alarm = (uint8_t)(alarm_status[2] - '0');
			alarms.battery_high_temperature_alarm = (uint8_t)(alarm_status[3] - '0');
			alarms.battery_not_detected = (uint8_t)(alarm_status[4] - '0');
			alarms.wc_error_alarm = (uint8_t)(alarm_status[5] - '0');
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", REPLY_ACK), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			send(sock , returnMsg , strlen(returnMsg), 0);
		} else if (findSubstr(client_message, "read_command:alarms") > -1) {
			JSON_Value *root_value = json_value_init_object();
			JSON_Object *root_object = json_value_get_object(root_value);
			char *serialized_string = NULL;
			json_object_set_boolean(root_object, "cpu_high_temperature_alarm", alarms.cpu_high_temperature_alarm);
			json_object_set_boolean(root_object, "cpu_critical_temperature_alarm", alarms.cpu_critical_temperature_alarm);
			json_object_set_boolean(root_object, "wc_high_temperature_alarm", alarms.wc_high_temperature_alarm);
			json_object_set_boolean(root_object, "battery_high_temperature_alarm", alarms.battery_high_temperature_alarm);
			json_object_set_boolean(root_object, "battery_not_detected", alarms.battery_not_detected);
			json_object_set_boolean(root_object, "wc_error_alarm", alarms.wc_error_alarm);
			serialized_string = json_serialize_to_string_pretty(root_value);
			if (check_snprintf(snprintf(returnMsg, sizeof(returnMsg) / sizeof(char), "%s", serialized_string), sizeof(returnMsg) / sizeof(char)))
				ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			json_free_serialized_string(serialized_string);
			json_value_free(root_value);
			send(sock , returnMsg , strlen(returnMsg), 0);
		} else if (findSubstr(client_message, "read_command:last_reboot_reason") > -1) {
			fd = open_file(LAST_REBOOT_FILE_PATH, O_RDONLY, EMPTY_MODE);
			if (fd < 0) {
				if (check_snprintf(snprintf(commandFile.value, sizeof(commandFile.value) / sizeof(char), "%s", REPLY_NACK), sizeof(commandFile.value) / sizeof(char)))
					ND_printlog(ND_LOG_ERROR, error_message_fw_error);
			} else {
				read(fd, commandFile.value, sizeof(commandFile.value));
				ND_printlog(ND_LOG_INFO, "value read:%s\n", commandFile.value);
				safe_close(fd);
			}
			send(sock , commandFile.value , strlen(commandFile.value), 0);
		} else if (findSubstr(client_message, "write_command:last_reboot_reason_done") > -1) {
			fd = open_file(LAST_REBOOT_FILE_PATH, O_RDWR, EMPTY_MODE);
			if (fd < 0) {
				send(sock , REPLY_NACK , strlen(REPLY_NACK), 0);
			} else {
				send(sock , REPLY_ACK , strlen(REPLY_ACK), 0);
				write(fd, "POR", strlen("POR"));
				ND_printlog(ND_LOG_INFO, "set watchdog file to 0 since state has been read");
				safe_close(fd);
			}
		}
		//sleep(1);
	}

	if (read_size == 0) {
		puts("server Client disconnected");
		fflush(stdout);
	} else if (read_size == -1) {
		ND_printlog(ND_LOG_ERROR, "error, server recv failed");
	}
	//Free the socket pointer
free_socket:
	free(socket_desc);
	safe_close(sock);
	return 0;
}
