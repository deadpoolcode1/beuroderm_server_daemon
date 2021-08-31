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
