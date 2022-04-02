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
#include <stdbool.h>
#ifndef C_COMPILE
#include <ND_LogLibrary.h>
#endif

#define EMPTY_MODE 0
#define MAX_ALLOWED_TRAILING_SPACES 5
#define error_message_read_file "error reading file"
#define error_message_fw_error "error, fw unexpected behaviour\n"
#define PRINTE(...) ND_printlog(ND_LOG_ERROR, "__VA_ARGS__", strerror(errno));
#define MD5_FILE "/data/md5"
#define LANGUGE_FILE "/data/lang"
#define MAX_SYSTEM_COMMAND_LEN 150
#define MAX_MD5_DATA_LEN 5000
#define SDCARD_DIR "/data/"
#define LANGUGE_DIR SDCARD_DIR"NEURODERM/LANGUAGES/"
#define TEXT_DIR LANGUGE_DIR"TEXT/"
#define TEXT_DIR_MD5 TEXT_DIR"md5"
#define VIDEO_DIR LANGUGE_DIR"VIDEO/"
#define VIDEO_DIR_MD5 VIDEO_DIR"md5"
#define MD5_SCRIPT "/system/bin/md5_calc.sh"
#define DELAY_PER_MD5_CALC 300000
#define REGIMEN_UPDATE_NOTIFICATION_FILE "/data/regedit"
#define SERIAL_NUMBER_FILE "/data/main_snp"
#define RECIVED_PINCODE_NOTIFY_API "/data/recivedPinCode"
#define PINCODE_RESULT_API "/data/pinCode"

enum {
	STR2INT_SUCCESS,
	STR2INT_OVERFLOW,
	STR2INT_UNDERFLOW,
	STR2INT_INCONVERTIBLE,
	STR2INT_FAILED
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
int safe_write_file_stream(char *filename, char *data);
int crc_passed(char *filename, uint8_t type);
bool parse_long(const char *str, long *val);
int read_file_data(char *filename, char *buffer, size_t buffer_size);

#define FREE(p) \
do \
{ \
  free(p); \
  p = NULL; \
} \
while(0)
#endif /* COMMON_H_ */
