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
		PRINTE("error, failed closing file : %s\n");
	return ret;
}

/** @brief close file stream
 */
void safe_close_stream(FILE *fd)
{
	int err = 0;
	if ((err = fclose(fd))) {
		ND_printlog(ND_LOG_ERROR, "Error number: %d closing file", err);//after failed fclose behaviour is unexpected
		exit(err);
	}
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
