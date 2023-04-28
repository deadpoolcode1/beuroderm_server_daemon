/** @file
 *
 * @brief common module.
 *
 * provides the common functions layer, general purpuse utilities
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <time.h>
#include "sha256/sha-256.h"
#ifndef C_COMPILE
#include <ND_LogLibrary.h>
#endif
#include "common.h"
#include "server.h"

/** @brief checks sprintf return function, prints and logs accordinglly.
 */
int check_snprintf(int ret_value, int max_length)
{
	if (ret_value < 0)
		ND_printlog(ND_LOG_ERROR, "error, invalid sprintf action \n");
	else if (ret_value >= max_length)
		ND_printlog(ND_LOG_ERROR, "error, string truncated \n");
	else
		return 0;
	return -1;
}

/** @brief converts string to integer, performs tests and catchs
 *  errors in conversion
 */
int str2int(int *out, char *s, int max_spaces, int size_of_string)
{
	char *end;
	int i = 0, j = 0, ret = 0;
	char *convert_string;
	convert_string = (char *) malloc(size_of_string);
	if (convert_string == NULL) {
		ret = STR2INT_FAILED;
		goto exit_str2int;
	}
	if (check_snprintf(snprintf(convert_string, size_of_string, "%s", s), size_of_string) != 0) {
		ret = STR2INT_INCONVERTIBLE;
		goto exit_str2int;
	}
	//remove trailing spaces
	while (isspace(convert_string[i]) && (i < max_spaces) && (i < size_of_string - 1))
		i++;
	if (!(isdigit(convert_string[i]))) {
		ret = STR2INT_INCONVERTIBLE;
		goto exit_str2int;
	}
	errno = 0;
	//allow charecters which are not numeric after the number
	while ((i + j) < size_of_string) {
		if (!isdigit(convert_string[i + j])) {
			convert_string[i + j] = '\0';
			break;
		}
		j++;
	}
	long l = strtol(convert_string + i, &end, 10);
	/* Both checks are needed because INT_MAX == LONG_MAX is possible. */
	if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX)) {
		ret =  STR2INT_OVERFLOW;
		goto exit_str2int;
	}
	if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN)) {
		ret = STR2INT_UNDERFLOW;
		goto exit_str2int;
	}
	if (*end != '\0') {
		ret = STR2INT_INCONVERTIBLE;
		goto exit_str2int;
	}
	*out = l;
	ret = STR2INT_SUCCESS;
exit_str2int:
	free(convert_string);
	return ret;
}

/** @brief read file line by line, return only last line
 */
int read_file_data(char *filename, char *buffer, size_t buffer_size)
{
	FILE *fptr = NULL;
	char *tmp_buffer = NULL;
	if ((fptr = fopen(filename, "r")) == NULL)
		goto read_file_data_error;
	while (-1 != getline(&tmp_buffer, &buffer_size, fptr)) {
	}
	if (fflush(stdout) == EOF)
		goto read_file_data_error;
	// make sure we close the filewhen we're
	// finished
	if (fclose(fptr) == EOF)
		goto read_file_data_error;
	if (check_snprintf(snprintf(buffer, buffer_size, "%s", tmp_buffer), buffer_size))
		goto read_file_data_error;
	FREE (tmp_buffer);
	return 0;

read_file_data_error:
	ND_printlog(ND_LOG_ERROR, "Error, failed reading file %s, %s", filename, strerror(errno));
	if (fptr != NULL)
		safe_close_stream(fptr);
	FREE (tmp_buffer);
	return -1;
}

bool parse_long(const char *str, long *val)
{
    char *temp;
    bool rc = true;
    errno = 0;
    *val = strtol(str, &temp, 0);

    if (temp == str || *temp != '\0' ||
        ((*val == LONG_MIN || *val == LONG_MAX) && errno == ERANGE))
        rc = false;

    return rc;
}

/** @brief removes number of chars from beginning of string
 */
void chop_string(char *str, size_t n)
{
	size_t len = strlen(str);

	if (n > len)
		n = len;
	memmove(str, str + n, len - n + 1);
}

/** @brief open file, log errors
 */
int open_file(char *filename, int flags, mode_t mode)
{
	int fd;
	if (mode != EMPTY_MODE)
		fd = open(filename, flags, mode);
	else
		fd = open(filename, flags);
	if (fd < 0)
		ND_printlog(ND_LOG_ERROR, "error Can not open file : %s\n", filename);
	return fd;
}

/** @brief close file, document fail closing file
 */
int safe_close(int fd)
{
	int ret = 0;
	if ((ret = close(fd)))
		ND_printlog(ND_LOG_ERROR, "error Can not close file \n");
	return ret;
}

/** @brief close file stream
 */
void safe_close_stream(FILE *fd)
{
	int err = 0;
	if ((err = fclose(fd)))
		ND_printlog(ND_LOG_ERROR, "Error number: %d closing file", err);//after failed fclose behaviour is unexpected
}

/** @brief write file, lock file used to avoid race conditioning
 */
int safe_write_file_stream(char *filename, char *data)
{
	FILE *fptr = NULL;
	unsigned int ret = 0;

	ND_printlog(ND_LOG_DEBUG, "-- %s:%d -- \n", __func__, __LINE__);
	if ((fptr = fopen(filename, "w")) == NULL)
		return -1;
	if (flock(fileno(fptr), LOCK_EX | LOCK_NB)) {
		ND_printlog(ND_LOG_ERROR, "Error failed locking file");
		goto write_file_error_after_open;
	}
	if ((ret = fprintf(fptr, "%s", data)) != strlen(data))
		goto write_file_error_after_lock;
	safe_close_stream(fptr);
	return 0;

write_file_error_after_lock:
	if (flock(fileno(fptr), LOCK_UN))
		ND_printlog(ND_LOG_ERROR, "Error failed unlocking file");
write_file_error_after_open:
	safe_close_stream(fptr);
	return -1;
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

/** @brief CRC ITT16 calculation
 */

unsigned short calc_crc(char *full_buffer, unsigned short length)
{
	unsigned short crc_calculated = 0xFFFF;
	unsigned char x = 0;
	char *data_p = full_buffer;

	while (length--) {
		x = crc_calculated >> 8 ^ *data_p++;
		x ^= x >> 4;
		crc_calculated = (crc_calculated << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
	}
	return crc_calculated;
}

/** @brief check if CRC passed, read CRC from file and compare to the calculated value
 */
int crc_passed(char *filename, uint8_t type)
{
	size_t buffer_size = STD_FILE_LENGTH;
	char *buffer = NULL, *buffer_filtered = NULL;
	char full_buffer[MAX_FILE_SIZE] = {0};
	unsigned short crc_calculated = 0xFFFF, length = 0;
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
	crc_calculated = calc_crc(full_buffer, length);	
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

bool check_if_file_exists(const char* filename)
{
    struct stat buffer;
    int exist = stat(filename,&buffer);
    return (exist == 0)? true : false; 
}


void create_file_if_needed(const char* filename)
{
	FILE * fPtr;
	fPtr = fopen(filename, "w");
    	if(fPtr == NULL)
    	{
       		PRINTE("error, Unable to create file, exiting");
        	return;
    	}

	fclose(fPtr);
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


/** @brief calculate pincode
 */
void calculate_pincode(char* buffer_calculated_valid_pincode_result, int length)
{
	char tmp[MAX_SYSTEM_COMMAND_LEN] = {0};
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	nonvolotile_parse_parameter("main_sn", tmp, sizeof(tmp) / sizeof(char));
	tmp[strlen(tmp) - 1] = '\0';
	sprintf(buffer_calculated_valid_pincode_result, "%s%02d", tmp + strlen(tmp) - 6, tm.tm_mday);//get 6 last digits 
}


/** @brief remove space charecters from string
 */
char * trim(char * s)
{
	int l = strlen(s);

	while (isspace(s[l - 1])) --l;
	while (* s && isspace(* s)) ++s, --l;

	return strndup(s, l);
}

int m_exec_system_command(const char * parameter, const char* process_to_execute)
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


void read_file(const char *filename, char *reply, size_t buffer_size)
{
    	char *ptr;
    	char *fcontent = NULL;
    	int fsize = 0;
    	FILE *fp;

    	fp = fopen(filename, "r");
    	if(fp) {
        	fseek(fp, 0, SEEK_END);
        	fsize = ftell(fp);
		if (fsize > buffer_size) {
			fclose(fp);
			return;
		}
        	rewind(fp);
        	fread(reply, 1, fsize, fp);
        	fclose(fp);
    	}
}

/** @brief handles reading ini file for start year parameter
 */
static int handler_nonvolotileparse(void* user, const char* section, const char* name,
		   const char* value)
{

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("", "main_sn")) {
	sprintf(nonvolotile_config.main_sn, "%s", value);
    } else if (MATCH("", "main_pn")) {
 	sprintf(nonvolotile_config.main_pn, "%s", value);
    } else if (MATCH("", "som_sn")) {
	sprintf(nonvolotile_config.som_sn, "%s", value);
    } else if (MATCH("", "cradle_sn")) {
	sprintf(nonvolotile_config.cradle_sn, "%s", value);
    } else if (MATCH("", "ftu_date")) {
	sprintf(nonvolotile_config.ftu_date, "%s", value);
    } else if (MATCH("", "crc")) {
	sprintf(nonvolotile_config.crc, "%s", value);
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;

}

void init_nonvolotile_var()
{
	printf("initilizing data\r\n");
	sprintf(nonvolotile_config.main_sn, "%s", END_STRING);
	sprintf(nonvolotile_config.main_pn, "%s", END_STRING);
	sprintf(nonvolotile_config.som_sn, "%s", END_STRING);
	sprintf(nonvolotile_config.cradle_sn, "%s", END_STRING);
	sprintf(nonvolotile_config.ftu_date, "%s", END_STRING);
	sprintf(nonvolotile_config.crc, "%s", END_STRING);
}
void nonvolotile_parse_parameter(const char* parameter, char* reply, size_t buffer_size)
{
	int err = 0;

	err = ini_parse(NONVOLOTILE_USERDATA_FILE, handler_nonvolotileparse, &nonvolotile_config); 
	printf("error : %d\r\n", err);	
	if (err == 1) {
		printf("error parsing data:%d\r\n", err);
		return;
	}

	if (strcmp(parameter,"main_sn") == 0) {
		sprintf(reply, "%s", nonvolotile_config.main_sn);
	}
	else if (strcmp(parameter,"main_pn") == 0)
		sprintf(reply, "%s", nonvolotile_config.main_pn);
	else if (strcmp(parameter,"som_sn") == 0)
		sprintf(reply, "%s", nonvolotile_config.som_sn);
	else if (strcmp(parameter,"cradle_sn") == 0)
		sprintf(reply, "%s", nonvolotile_config.cradle_sn);
	else if (strcmp(parameter,"ftu_date") == 0)
		sprintf(reply, "%s", nonvolotile_config.ftu_date);
	else if (strcmp(parameter,"crc") == 0)
		sprintf(reply, "%s", nonvolotile_config.crc);
	else
		sprintf(reply, "%s", "");
	
}

void nonvolotile_parse_parameter_string(const char* full_String, const char* parameter, char* reply)
{
	int err = 0;

	err = ini_parse_string(full_String, handler_nonvolotileparse, &nonvolotile_config); 

	if (strcmp(parameter,"main_sn") == 0) {
		sprintf(reply, "%s", nonvolotile_config.main_sn);
	}
	else if (strcmp(parameter,"main_pn") == 0)
		sprintf(reply, "%s", nonvolotile_config.main_pn);
	else if (strcmp(parameter,"som_sn") == 0)
		sprintf(reply, "%s", nonvolotile_config.som_sn);
	else if (strcmp(parameter,"cradle_sn") == 0)
		sprintf(reply, "%s", nonvolotile_config.cradle_sn);
	else if (strcmp(parameter,"ftu_date") == 0)
		sprintf(reply, "%s", nonvolotile_config.ftu_date);
	else if (strcmp(parameter,"crc") == 0)
		sprintf(reply, "%s", nonvolotile_config.crc);
	else
		sprintf(reply, "%s", "");
	
}

void nonvolotile_readall(char *reply, size_t buffer_size)
{
	m_exec_system_command2("eval \"dd if=/dev/block/mmcblk0 of=/data/boot_args bs=512 skip=1536 count=1\" && cat /data/boot_args", reply);
	reply[EMPTY_NONVOLOTILE_MAXLEN] = '\0';
}

void nonvolotile_delete()
{
	char reply[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	char command[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	sprintf(command,"%s%s%s","echo \"",EMPTY_NONVOLOTILE ,"\" > /data/boot_args && dd if=/data/boot_args of=/dev/block/mmcblk0 bs=512 seek=1536 count=1");
	m_exec_system_command2(command,reply);
	usleep(USEC_NONVOLOTILE_ACTION);
}

unsigned short nonvolotile_extract_crc()
{
	unsigned short crc;
	int tmp;
	char buf[MAX_CONF_SIZE] = {0};
	nonvolotile_parse_parameter("crc", buf, MAX_CONF_SIZE );
	str2int(&tmp, buf, MAX_ALLOWED_TRAILING_SPACES, sizeof(buf) / sizeof(char));
	crc = (unsigned short)tmp;
	ND_printlog(ND_LOG_INFO, "nonvolotile crc value written in file: %hu\r\n", crc);
	return crc;
}


void read_current_nonvolotile_data()
{
	char reply[MAX_CONF_SIZE] = {0};

	nonvolotile_readall(reply, EMPTY_NONVOLOTILE_MAXLEN);
	nonvolotile_parse_parameter_string(reply, "main_sn", reply);
	nonvolotile_parse_parameter_string(reply, "main_pn", reply);
	nonvolotile_parse_parameter_string(reply, "som_sn", reply);
	nonvolotile_parse_parameter_string(reply, "cradle_sn", reply);
	nonvolotile_parse_parameter_string(reply, "ftu_date", reply);
	nonvolotile_parse_parameter_string(reply, "crc", reply);
}

void update_relevant_nonvolotile_data_field(const char* parameter, const char* value)
{
	if (strcmp(parameter,"main_sn") == 0)
		sprintf(nonvolotile_config.main_sn, "%s\n", value);
	else if (strcmp(parameter,"main_pn") == 0)
		sprintf(nonvolotile_config.main_pn, "%s\n", value);
	else if (strcmp(parameter,"som_sn") == 0)
		sprintf(nonvolotile_config.som_sn, "%s\n", value);
	else if (strcmp(parameter,"cradle_sn") == 0)
		sprintf(nonvolotile_config.cradle_sn, "%s\n", value);
	else if (strcmp(parameter,"ftu_date") == 0)
		sprintf(nonvolotile_config.ftu_date, "%s\n", value);
	else if (strcmp(parameter,"crc") == 0)
		sprintf(nonvolotile_config.crc, "%s\n", value);
	else { 
		printf("no such parameter\r\n");
		return;
	}
}

unsigned short nonvolotile_calculate_crc()
{
	int err = 0;
	unsigned short calculated_crc = 0;
	char buf[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	char buft[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	char tmp[VAR_MAXLEN] = {0};

	buf[0] = '\0';
	sprintf(tmp, "main_sn=%s", nonvolotile_config.main_sn);
	strcat(buf,tmp);
	sprintf(tmp, "main_pn=%s", nonvolotile_config.main_pn);
	strcat(buf,tmp);
	sprintf(tmp, "som_sn=%s", nonvolotile_config.som_sn);
	strcat(buf,tmp);
	sprintf(tmp, "cradle_sn=%s", nonvolotile_config.cradle_sn);
	strcat(buf,tmp);
	sprintf(tmp, "ftu_date=%s", nonvolotile_config.ftu_date);
	strcat(buf,tmp);
	sprintf(buft, "%s", filter_crc_string(buf, FILE_INI, strlen(buf)));
	calculated_crc = calc_crc(buft, strlen(buft));
	ND_printlog(ND_LOG_INFO, "nonvolotile calculated_crc: %hu\r\n", calculated_crc);
	
	return calculated_crc;
}

void nonvolotile_update_crc()
{
	char buf[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	char tmp[VAR_MAXLEN] = {0};
	buf[0] = '\0';
	sprintf(tmp, "main_sn=%s", nonvolotile_config.main_sn);
	strcat(buf,tmp);
	sprintf(tmp, "main_pn=%s", nonvolotile_config.main_pn);
	strcat(buf,tmp);
	sprintf(tmp, "som_sn=%s", nonvolotile_config.som_sn);
	strcat(buf,tmp);
	sprintf(tmp, "cradle_sn=%s", nonvolotile_config.cradle_sn);
	strcat(buf,tmp);
	sprintf(tmp, "ftu_date=%s", nonvolotile_config.ftu_date);
	strcat(buf,tmp);
	filter_crc_string(buf, FILE_INI, strlen(buf));
	sprintf(nonvolotile_config.crc,"%hu\n", calc_crc(buf, strlen(buf)));
}



bool nonvolotile_varify_parameter(const char* parameter, const char* expected)
{	
	int i = 0;
    const char* supported_params[] = {"main_sn", "main_pn", "som_sn", "cradle_sn", "ftu_date", "crc"};
    const int num_params = sizeof(supported_params) / sizeof(supported_params[0]);
	char buf[MAX_CONF_SIZE] = {0};
    for (i = 0; i < num_params; i++) {
        if (strcmp(parameter, supported_params[i]) == 0) {
            // Update the corresponding parameter
			nonvolotile_parse_parameter(parameter, buf, MAX_CONF_SIZE );
			if (strncmp (expected, buf, strlen(expected)) == 0)
            	return true;
			else
				return false;
        }
        if (i == num_params - 1) {
            // Parameter not found, handle error
			return false;
        }
    }	

	return false;
}


/** @brief general function used for executing shell commands from ate_daemon
 */
int exec_system_command3(const char * command, char* reply, bool wait_reply)
{
   char buffer[REPLY_STRING_LENGTH];

   // Open pipe to file
   FILE* pipe = popen(command, "r");
   if (!pipe) {
      return 1;
   }

   if (!wait_reply)
	return 0;

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
#define NVM_WRITE_RETRY_MAX 3
void nonvolotile_update(const char* parameter, const char* value)
{
	char buf[MAX_CONF_SIZE] = {0};
	char reply[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	char tmp[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	char command[EMPTY_NONVOLOTILE_MAXLEN] = {0};
	int i = 0;
	read_current_nonvolotile_data();
	update_relevant_nonvolotile_data_field(parameter, value);
	if (strcmp(parameter,"crc") != 0) 
		nonvolotile_update_crc();
	sprintf(tmp, "main_sn=%s", nonvolotile_config.main_sn);
	strcat(reply,tmp);
	sprintf(tmp, "main_pn=%s", nonvolotile_config.main_pn);
	strcat(reply,tmp);
	sprintf(tmp, "som_sn=%s", nonvolotile_config.som_sn);
	strcat(reply,tmp);
	sprintf(tmp, "cradle_sn=%s", nonvolotile_config.cradle_sn);
	strcat(reply,tmp);
	sprintf(tmp, "ftu_date=%s", nonvolotile_config.ftu_date);
	strcat(reply,tmp);
	sprintf(tmp, "crc=%s", nonvolotile_config.crc);
	strcat(reply,tmp);
	strcat(reply,"000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
	sprintf(command,"%s%s%s","echo \"",reply ,"\" > /data/boot_args && dd if=/data/boot_args of=/dev/block/mmcblk0 bs=512 seek=1536 count=1");
	m_exec_system_command2(command,reply);
	for (i = 0; i < NVM_WRITE_RETRY_MAX; i++)
	{
		if (nonvolotile_varify_parameter(parameter, value )) {
			ND_printlog(ND_LOG_INFO, "NVM write OK\n");
			break;
		}
		else {
			m_exec_system_command2(command,reply);
			if (i < (NVM_WRITE_RETRY_MAX -1) )
				ND_printlog(ND_LOG_INFO, "NVM write fail, retry\n");
			else
				{
					ND_printlog(ND_LOG_INFO, "NVM write failed\n");
					sprintf(command,"eval \"/system/bin/server_daemon_send 127.0.0.1 nvm_write_status\"");
					exec_system_command3(command, NULL, false);
					sprintf(command,"/system/bin/log -p v -t \"server_daemon\" \"keycode \"%d",600);
					exec_system_command3(command, NULL, false);
					sprintf(command,"eval \"input keyevent %d\"",600);
					exec_system_command3(command, NULL, false);
				}
		}
	}
}


int m_exec_system_command2(const char * command, char* reply)
{
   char buffer[MAX_CONF_SIZE];

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
