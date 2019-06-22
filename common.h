/*
 * common.h
 *
 *  Created on: Mar 29, 2019
 *      Author: root
 */

#ifndef COMMON_H_
#define COMMON_H_
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define EMPTY_MODE 0
#define error_message_read_file "error reading file"

enum {
	STR2INT_SUCCESS,
	STR2INT_OVERFLOW,
	STR2INT_UNDERFLOW,
	STR2INT_INCONVERTIBLE
};

enum {
	OPEN_FILE_CREATE_IF_NOT_EXIST,
	OPEN_FILE_READ,
	OPEN_FILE_READ_WRITE
};

int check_snprintf(int ret_value, int max_length);
int str2int(int *out, char *s, int max_spaces, int size_of_string);
void chop_string(char *str, size_t n);
int open_file(char *filename, int flags, mode_t mode);
int safe_close(int fd);
void safe_close_stream(FILE *fd);
FILE *safe_open_stream(const char *filename, const char *mode);
int safe_write_file_stream(char *filename, char *data);

#endif /* COMMON_H_ */
