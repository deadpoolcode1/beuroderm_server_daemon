# TWIKO FW Failures Table

This document contains the complete list of firmware (FW) failures for the TWIKO system.

| Failure Name | Failure ID | Source | Failure Description | Root cause | Evidence logs | CS Log ID | Alarm Type | Actions/Available user actions | Level of support | Blocking user |
|--------------|------------|--------|---------------------|------------|---------------|-----------|------------|-------------------------------|------------------|---------------|
| BIT_I2C_PMIC | 1 | FW | Communicate with the power supply chip via I2C and checks valid communication | | ID: 1 BIT_I2C_PMIC | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_FUELGAUGE | 2 | FW | Communicate with the fuel gauge chip via I2C and checks valid communication | | ID: 2 BIT_I2C_FUELGAUGE | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_MAX77818TOP | 3 | FW | Communicate with the voltage stabilizer chip via I2C and checks valid communication | | ID: 3 BIT_I2C_MAX77818TOP | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_MAX77816_U14 | 4 | FW | Communicate with the battery charger chip via I2C and checks valid communication | | ID: 4 BIT_I2C_MAX77816_U14 | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_DISPLAYTOUCHPANEL | 5 | FW | Communicate with the touch screen chip via I2C and checks valid communication | | ID: 5 BIT_I2C_DISPLAYTOUCHPANEL | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_MAX77816DC3 | 6 | FW | Communicate with the 3.3 voltage stabilizer chip via I2C and checks valid communication | | ID: 6 BIT_I2C_MAX77816DC3 | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_RTC | 7 | FW | Communicate with the RTC chip via I2C and checks valid communication | | ID: 7 BIT_I2C_RTC | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_RTCMEMBLOCK0 | 8 | FW | Communicate with the RTC memory block0 chip via I2C and checks valid communication | | ID: 8 BIT_I2C_RTCMEMBLOCK0 | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_RTCMEMBLOCK1 | 9 | FW | Communicate with the RTC memory block1 chip via I2C and checks valid communication | | ID: 9 BIT_I2C_RTCMEMBLOCK1 | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_CRADLETEMPSENSOR | 10 | FW | Communicate with the WC temperature sensor chip via I2C and checks valid communication | | ID: 10 BIT_I2C_CRADLETEMPSENSOR | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_I2C_IOEXPENDER | 11 | FW | Communicate with the I/O expender chip via I2C and checks valid communication | | ID: 11 BIT_I2C_IOEXPENDER | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_BLE | 12 | FW | Checks BLE component is placed in the CS by checking the FW driver status, in case status is valid test concludes no major errors occurred, meaning communication between CS processor and BLE is OK, BLE component is answering on UART lines | | ID: 12 BIT_BLE | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_BATTERY | 13 | FW | Checks battery inserted to the CS using fuel gauge status exposed by fuel gauge driver under: /sys/class/power_supply/battery/present In case battery is indicated as "1" (present) verify average voltage read from battery is within valid limits (2.4v<X<7v) available under: /sys/class/power_supply/battery/avg_voltage. The voltage verification is required since battery present flag is updated by driver only on powerup, since BIT is performed every wakeup, the only way to detect battery removal is testing valid voltage reading | | ID: 13 BIT_BATTERY | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_RTCFUNCTIONAL | 14 | FW | Checks that the time in RTC component progress, by that verifying the RTC correct functionality. Done by reading RTC time, wait 2 seconds read time again, and verify time progressed (no min max limits, just time progress is checked) | | ID: 14 BIT_RTCFUNCTIONAL | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_FWCRC | 15 | FW | Calculates CRC for the FW configuration file, compares with the CRC written in the file and checks for validity | FW Configuration file CRC value missmatch | ID: 15 BIT_FWCRC | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_APKCRC | 16 | FW | Calculates CRC for the Twiko Control Station application configuration file, compares with the CRC written in the file and checks for validity | APK Configuration file CRC value missmatch | ID: 16 BIT_APKCRC | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_LANG_FILE | 17 | FW | Calculate MD5 for language folder text files according to the language selected and compares with expected language MD5 pre-set language calculation file | Selected language strings file MD5 value missmatch | ID: 17 BIT_LANG_FILE | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_LANG_VIDEO | 18 | FW | Calculate MD5 for language folder video files according to the language selected and compares with expected language MD5 pre-set video calculation file | Selected language video files MD5 value missmatch | ID: 18 BIT_LANG_VIDEO | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_SERIAL_NUMBER | 19 | FW | Checks serial number validity from the non-volatile memory | | ID: 19 BIT_SERIAL_NUMBER | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_U14_FUNCTIONALITY | 20 | FW | Verify U14 has no error BIT set | | ID: 20 BIT_U14_FUNCTIONALITY | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |
| BIT_AUDIO_FILES | 21 | FW | Calculate MD5 for alarm audio files folder and compares with expected MD5 pre-set alarm audio calculation file | Audio files MD5 value missmatch | ID: 21 BIT_AUDIO_FILES | 100 (filed_b); 522 (read_bittestt) | Maintenance | | Yes | |

## Notes

- All FW failures have **Alarm Type**: Maintenance
- All FW failures have **Level of support**: Yes
- **CS Log ID** format: 100 (filed_b); 522 (read_bittestt)
- **Source**: FW (Firmware)

## Failure Categories

### I2C Communication Failures (IDs 1-11)
These failures test communication with various hardware chips via the I2C bus:
- Power supply (PMIC)
- Fuel gauge
- Voltage stabilizers (MAX77818TOP, MAX77816DC3, MAX77816_U14)
- Touch screen display panel
- RTC and RTC memory blocks
- Temperature sensor
- I/O expander

### Component Functionality Failures (IDs 12-14)
- BLE component communication
- Battery presence and voltage validation
- RTC time progression verification

### Configuration/Integrity Failures (IDs 15-21)
- FW configuration file CRC validation
- APK configuration file CRC validation
- Language file MD5 validation
- Language video files MD5 validation
- Serial number validation
- U14 error bit verification
- Audio files MD5 validation
