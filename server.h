#ifndef SERVER_LISTNER_H_
#define SERVER_LISTNER_H_

#define FW_CONFIG_FILE_PATH "/system/bin/config.file"
#define APK_CONFIG_FILE_PATH "/system/etc/cs_config.json"
#define BIT_I2C_PMIC "BIT_I2C_PMIC"
#define BIT_I2C_FUELGAUGE "BIT_I2C_FUELGAUGE"
#define BIT_I2C_MAX77818TOP "BIT_I2C_MAX77818TOP"
#define BIT_I2C_MAX77818CHARGER "BIT_I2C_MAX77816_U14"
#define BIT_I2C_DISPLAYTOUCHPANEL "BIT_I2C_DISPLAYTOUCHPANEL"
#define BIT_I2C_MAX77816DC3 "BIT_I2C_MAX77816DC3"
#define BIT_I2C_RTC "BIT_I2C_RTC"
#define BIT_I2C_RTCMEMBLOCK0 "BIT_I2C_RTCMEMBLOCK0"
#define BIT_I2C_RTCMEMBLOCK1 "BIT_I2C_RTCMEMBLOCK1"
#define BIT_I2C_CRADLETEMPSENSOR "BIT_I2C_CRADLETEMPSENSOR"
#define BIT_I2C_IOEXPENDER "BIT_I2C_IOEXPENDER"
#define BIT_BLE "BIT_BLE"
#define BIT_BATTERY "BIT_BATTERY"
#define BIT_RTCFUNCTIONAL "BIT_RTCFUNCTIONAL"
#define BIT_LANGUAGE_FILE "BIT_LANG_FILE"
#define BIT_LANGUAGE_VIDEO "BIT_LANG_VIDEO"
#define BIT_SERIAL_NUMBER "BIT_SERIAL_NUMBER"
#define BIT_U14_FUNCTIONALITY "BIT_U14_FUNCTIONALITY"
#define BIT_FWCRC "BIT_FWCRC"
#define BIT_APKCRC "BIT_APKCRC"
#define BIT_TIMESTAMP "BIT_TIMESTAMP"
#define SOCKET_MESSAGE_MAX_LENGTH 5000
#define REPLY_ACK "ACK\n"
#define REPLY_NACK "NACK\n"
#define API_VERSION "8"
#define MAX_FILE_SIZE 10000
#define CRC_STRING_JSON "\"crc\":\""
#define CRC_STRING_INI "crc="
#define HALL_DETECT "/sys/class/switch/hall_detect/state"
#define HALL_DETECTR "/data/gpiohallreflect"
//i2c bit test address, paths, registers definition

#define i2c0_path "/dev/i2c-0"
#define i2c2_path "/dev/i2c-2"
#define rtc_time_path "/sys/class/rtc/rtc0/since_epoch"
#define battery_exists_path "/sys/class/power_supply/battery/present"
#define battery_voltage_path "/sys/class/power_supply/battery/voltage_avg"
#define minimium_valid_battery_voltage 2400000
#define ble_result_path "/data/bt_result"
//i2c0_path
#define i2c_pmic_status_address 0x36
#define i2c_pmic_status_register 0x00
#define i2c_fuelgauge_status_address 0x66
#define i2c_fuelgauge_status_register 0x20
#define i2c_max77818top_status_address 0x69
#define i2c_max77818top_status_register 0xb0
#define i2c_max77818charger_status_address 0x18
#define i2c_max77818charger_status_register 0x00
#define i2c_max77818charger_ok 0x00
#define i2c_max77818charger_status_register_detailed 0x01

#define i2c_displaytouchpanel_status_address 0x26
#define i2c_displaytouchpanel_status_register 0x00
//i2c2_path
#define i2c_max77816dc3_status_address 0x18
#define i2c_max77816dc3_status_register 0x00
#define i2c_rtc_status_address 0x68
#define i2c_rtc_status_register 0x00
#define i2c_rtcmemblock0_status_address 0x69
#define i2c_rtcmemblock0_status_register 0x00
#define i2c_rtcmemblock1_status_address 0x6A
#define i2c_rtcmemblock1_status_register 0x00
#define i2c_cradletempsensor_status_address 0x48
#define i2c_cradletempsensor_status_register 0x00
#define i2c_ioexpender_status_address 0x20
#define i2c_ioexpender_status_register 0x00
//last reboot reason file path
#define LAST_REBOOT_FILE_PATH "/data/last_reset_reason"
//array sizes
#define STD_FILE_LENGTH 100
#define REPLY_STRING_LENGTH 1000
#define MIN_CONFIG_LINE_LEN 3
#define SHORT_BUFFER_LEN 10
#define RTC_DATA_LEN 20
#define FILE_BUFFER_LEN 150
//APK open commands
#define EXITATEMODE "0"
#define ENTERATEMODE "1"
#define VAR_COMMAND_EXITATEMODE "9"
#define VAR_COMMAND_ENTERATEMODE "10"
#define  VAR_OPEN_ND_MAIN_APK "14"
#define  VAR_OPEN_ND_CTA_APK "15"
#define  VAR_SEND_KEYCODE_SLEEP "16"
#define  VAR_SEND_KEYCODE_WAKEUP "17"
//APK NAMES
#define ND_HOME_APK "com.neuroderm.station"
#define ND_CTA_APK "com.neuroderm.ct_app"
//YEAR RELATED
#define YEAR_STRING_LEN 5
#define MAX_YEAR_ALLOWED 2038
//API write_command:device_charger support
#define BATTERY_CHARGER_OFF "0"
#define BATTERY_CHARGER_ON "1"

#define NUMBER_RETRY_MONITOR 3 
#define  VAR_COMMAND_REBOOT "0"
#define  BITTEST_ERROR "600"
#define  BATTERY_ERROR "542"
#define VAR_COMMAND_5v_OK "564"
#define VAR_COMMAND_5v_BB_OCP "565"
#define VAR_COMMAND_5v_BB_OVP "566"
#define VAR_COMMAND_5v_BB_POKn "567"
#define VAR_COMMAND_5v_TSHDN "568"
#define VAR_COMMAND_5v_no_communication "569"
#define VAR_COMMAND_5v_no_communication "569"
#define VAR_COMMAND_5v_no_communication "569"
#define VAR_COMMAND_error_in_languge_files "571"
#define VAR_COMMAND_error_in_video_files "572"
#define VAR_COMMAND_error_in_serial_file "573" 
#define USEC_I2C_RETRY 100000
#define I2C_MAX_RETRY 3
#define LATEST_BIT_STATUS "/data/latest_bit_status"

typedef struct {
	char* year;
} configuration;

configuration config;

enum BITRESULT            /* Defines results  */
{
	FAILED = 0,
	PASSED, 
	BIT_NOT_PERFORMED
} ;


enum BLOCKING_DEF          /* Defines blocking  */
{
        BLOCKING = 0,
        NON_BLOCKING
} ;



enum BITUPDATE            /* Defines results  */
{
	NOT_UPDATE_STATUS = 0,
	UPDATE_STATUS, 
} ;




enum FILETYPE {
	FILE_JSON,
	FILE_INI
};


typedef struct {
	uint8_t cpu_high_temperature_alarm;
	uint8_t cpu_critical_temperature_alarm;
	uint8_t wc_high_temperature_alarm;
	uint8_t battery_high_temperature_alarm;
	uint8_t battery_critical_temperature_alarm;
	uint8_t battery_not_detected;
	uint8_t wc_error_alarm;	
} alarms_struct;
#endif /* SERVER_LISTNER_H_ */

