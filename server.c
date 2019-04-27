#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <signal.h>
#include <pthread.h> //for threading , link with lpthread
#include <ctype.h>
#include "inih/ini.h"
#include "parson/parson.h"


#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include "i2c.h"
#define ServerDebug 1
#ifdef ServerDebug
#undef ServerDebug
#endif
#define FW_CONFIG_FILE_PATH "/system/bin/config.file"
#define APK_CONFIG_FILE_PATH "/sdcard/NEURODERM/cs_config.json"
#define BIT_I2C_PMIC "BIT_I2C_PMIC"
#define BIT_I2C_FUELGAUGE "BIT_I2C_FUELGAUGE"
#define BIT_I2C_MAX77818TOP "BIT_I2C_MAX77818TOP"
#define BIT_I2C_MAX77818CHARGER "BIT_I2C_MAX77818CHARGER"
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
#define BIT_FWCRC "BIT_FWCRC"
#define BIT_APKCRC "BIT_APKCRC"
#define BIT_TIMESTAMP "BIT_TIMESTAMP"
#define SOCKET_MESSAGE_MAX_LENGTH 2000
#define REPLY_ACK "ACK"
#define REPLY_NACK "NACK"
#define API_VERSION "3"

//i2c bit test address, paths, registers definition

#define i2c0_path "/dev/i2c-0"
#define i2c2_path "/dev/i2c-2"
#define rtc_time_read "/sys/class/i2c-dev/i2c-2/device/2-0068/rtc/rtc0/since_epoch"
#define battery_exists_path "/sys/class/power_supply/battery/present"
//i2c0_path
#define i2c_pmic_status_address 0x36
#define i2c_pmic_status_register 0x00
#define i2c_fuelgauge_status_address 0x66
#define i2c_fuelgauge_status_register 0x20
#define i2c_max77818top_status_address 0x69
#define i2c_max77818top_status_register 0xb0
#define i2c_max77818charger_status_address 0x18
#define i2c_max77818charger_status_register 0x00
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
typedef struct
{
    char* year;
} configuration;

configuration config;

enum BITRESULT            /* Defines results  */  
{  
    PASSED = 0, 
    FAILED      
} ;



struct bittest_info
{
    uint8_t i2c_pmic_status;  			/*!< i2c0 0x36 */
    uint8_t i2c_fuelgauge_status;		/*!< i2c0 0x66 */
    uint8_t i2c_max77818top_status;		/*!< i2c0 0x69 */
    uint8_t i2c_max77818charger_status;		/*!< i2c0 0x18 */
    uint8_t i2c_displaytouchpanel_status;	/*!< i2c0 0x26 */
    uint8_t i2c_max77816dc3_status;		/*!< i2c2 0x18 */
    uint8_t i2c_rtc_status;			/*!< i2c2 0x68 */
    uint8_t i2c_rtcmemblock0_status;		/*!< i2c2 0x69 */
    uint8_t i2c_rtcmemblock1_status;		/*!< i2c2 0x6A */
    uint8_t i2c_cradletempsensor_status;	/*!< i2c2 0x48 */
    uint8_t i2c_ioexpender_status;		/*!< i2c2 0x20 */
    uint8_t ble_status;
    uint8_t battery_status;
    uint8_t rtc_functional_status;
    uint8_t fw_crc_status;
    uint8_t apk_crc_status;
    char timestamp [100];
};

typedef struct
{
    uint8_t cpu_high_temperature_alarm;
    uint8_t cpu_critical_temperature_alarm;
    uint8_t wc_high_temperature_alarm;
    uint8_t battery_high_temperature_alarm;
    uint8_t battery_not_detected;
    uint8_t wc_error_alarm;
} alarms_struct;

alarms_struct alarms;



static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
    configuration* pconfig = (configuration*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("", "start_year")) 
        pconfig->year = strdup(value);
	else 
        return 0;  /* unknown section/name, error */
    return 1;
}

/*
function return index of occurance pattern in string
*/
int findSubstr(char *inpText, char *pattern) {
    int inplen = strlen(inpText);
    while (inpText != NULL) {

        char *remTxt = inpText;
        char *remPat = pattern;

        if (strlen(remTxt) < strlen(remPat)) {
            /* printf ("length issue remTxt %s \nremPath %s \n", remTxt, remPat); */
            return -1;
        }
        while (*remTxt++ == *remPat++) {
            if (*remPat == '\0') {
                return inplen - strlen(inpText+1);
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

int socket_desc_main;

/*
function to handle ctrl+c response 
ensure closing the socket before exiting software 
*/
void sig_handler(int signo)
{
  if (signo == SIGINT)
    printf("received SIGINT\n");
    close(socket_desc_main);    
    exit(1);
}
/*
function to handle opening a file 
*/
int open_file(char *filename)
{
	int fd;
	fd = open(filename, O_RDWR);
	if (fd < 0) 
	{
         	printf(" - Can not open file : %s\n", filename);
    }
	return fd;
}


char * trim(char * s) {
    int l = strlen(s);

    while(isspace(s[l - 1])) --l;
    while(* s && isspace(* s)) ++s, --l;

    return strndup(s, l);
}



char* read_file_data(char *filename,char *buffer, size_t buffer_size)
{
    // open the file for reading
    FILE *file = fopen(filename, "r");
    // make sure the file opened properly
    if(NULL == file)
    {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return "";
    }

    // read each line and print it to the screen
    while(-1 != getline(&buffer, &buffer_size, file))
    {
    }
    fflush(stdout);

    // make sure we close the filewhen we're
    // finished
    fclose(file);

    return buffer;
}


char* read_config_file_data(char *filename,char *buffer, size_t buffer_size)
{
	char *line = NULL;
	size_t tmp_buf_size=100;
    // open the file for reading
    FILE *file = fopen(filename, "r");
    // make sure the file opened properly
    if(NULL == file)
    {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return "";
    }

    // read each line and print it to the screen
    while(-1 != getline(&line, &tmp_buf_size, file))
    {
    	if (strlen(line)>3)
    		strcat(buffer,line);
    }
    fflush(stdout);

    // make sure we close the filewhen we're
    // finished
    fclose(file);

    return buffer;
}

char* read_file_data_no_space(char *filename,char *buffer, size_t buffer_size)
{
	char *pos;
	read_file_data(filename,buffer, buffer_size);
	trim(buffer);
	if ((pos=strchr(buffer, '\n')) != NULL)
    	*pos = '\0';
	return buffer;
}


int file_exists(char *filename)
{
    int fd;
    fd = access( filename, F_OK );
    if( fd != -1 ) 
    {
        #ifdef ServerDebug
            printf(" - file %s exits\n", filename);
        #endif
    }
    else
    {
        #ifdef ServerDebug
            printf(" - file %s not exits\n", filename);
        #endif
    }

    return fd;
}



uint8_t read_i2c(char *path, uint8_t m_address, uint8_t m_register)
{
    int rc, file;
    uint8_t read_data;
    file = open(path, O_RDWR);
    if (file < 0)
    	goto failed_reading;
    rc = ioctl(file, I2C_SLAVE_FORCE, m_address);
    if (rc < 0)
    	goto failed_reading;
    read_data = i2c_smbus_read_byte_data(file, m_register);
    return read_data;
failed_reading:   err(errno, "failed reading");
    return read_data;
} 

//function tests vale in specified address is not 0xff, thus varifying i2c communication
//return 0 on succesful communication (value not 0xff)
uint8_t check_i2c_validity(char *path, uint8_t m_address, uint8_t m_register)
{

	return read_i2c(path, m_address, m_register) == 0xff ? FAILED : PASSED;
}


char* filter_crc_string(char* input)                                         
{
    int i,j;
    char *output=input;
    for (i = 0, j = 0; i<strlen(input); i++,j++)          
    {
        if (input[i]!=' ' && input[i]!='\r' && input[i]!='\n')                           
            output[j]=input[i];                     
        else
            j--;                                     
    }
    output[j]=0;
    return output;
}

int crc_passed(char * filename)
{
    size_t buffer_size = 150;
    char *buffer;
    char *buffer_filtered;
    char full_buffer[5000];
    unsigned char x;
    unsigned short crc = 0xFFFF;
    unsigned short length;
    char *data_p = full_buffer;
    long val=0;
    //length=strlen(data_p)	
    // open the file for reading
    FILE *file = fopen(filename, "r");
    // make sure the file opened properly
    if(NULL == file)
    {
        fprintf(stderr, "Cannot open file: %s\n", filename);
        return -1;
    }
    //assign memory for buffer
    buffer = (char *)malloc(buffer_size * sizeof(char));
    if( buffer == NULL)
    {
        perror("Unable to allocate buffer");
        exit(1);
    }
    full_buffer[0]='\0';
    //assign memory for filtered buffer
    buffer_filtered = (char *)malloc(buffer_size * sizeof(char));
    if( buffer_filtered == NULL)
    {
        perror("Unable to allocate buffer_filtered");
        exit(1);
    }
    buffer_filtered[0]='\0';
    while(-1 != getline(&buffer, &buffer_size, file))
    {
        char *pch = strstr(buffer, "crc");
        if(!pch) // crc line does not get included in crc calculation
        {
		sprintf(buffer_filtered,"%s",filter_crc_string(buffer));
 		sprintf(&full_buffer[strlen(full_buffer)], "%s", buffer_filtered);
        }
     else
         {

        	char *p = buffer;
			while (*p) 
			{ // While there are more characters to process...
    			if (isdigit(*p)) 
    			{ // Upon finding a digit, ...
        			val = strtol(p, &p, 10); // Read a number, ...
        			printf("\r\n%s CRC:%ld\n", filename, val); // and print it.
   				 } 
   				 else 
        			p++;
    	}
      }
    }
    fflush(stdout);

    // make sure we close the file when we're
    // finished
    fclose(file);
    //calculate CRC
    length=strlen(full_buffer);
    while (length--){
	if (*data_p +1 == '\r'  || *data_p +1 == '\n' || *data_p +1 == ' ')
	{
		data_p++;
		continue;
	}
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x <<5)) ^ ((unsigned short)x);
    }
    printf ("%s CRC result: %d\r\n",filename, crc);
    if (crc==val)
    	return 0;
    else return -1;
}


char * return_current_bit_status(char *update_string)
{
        update_string[0]='\0';
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
    	printf("bit test: %s\r\n", update_string);
    	json_value_free(root_value);
    	return update_string;
}


//fucnction that runs bittest full test
void bittest_init_full()
{
    char return_reply[600];
    uint32_t rtc_time_value;
    time_t t = time(NULL);
    struct tm * p = localtime(&t);

    printf("\n\n*** Bit testing procedure started! ***\n\n");
    latest_bittest.i2c_pmic_status = check_i2c_validity(i2c0_path,i2c_pmic_status_address,i2c_pmic_status_register);
    latest_bittest.i2c_fuelgauge_status = check_i2c_validity(i2c0_path,i2c_fuelgauge_status_address,i2c_fuelgauge_status_register);
    latest_bittest.i2c_max77818top_status = check_i2c_validity(i2c0_path,i2c_max77818top_status_address,i2c_max77818top_status_register);
    latest_bittest.i2c_max77818charger_status = check_i2c_validity(i2c0_path,i2c_max77818charger_status_address,i2c_max77818charger_status_register);
    latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c0_path,i2c_displaytouchpanel_status_address,i2c_displaytouchpanel_status_register);
    latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c2_path,i2c_max77816dc3_status_address,i2c_max77816dc3_status_register);
    latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c2_path,i2c_rtc_status_address,i2c_rtc_status_register);
    latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c2_path,i2c_rtcmemblock0_status_address,i2c_rtcmemblock0_status_register);
    latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c2_path,i2c_rtcmemblock1_status_address,i2c_rtcmemblock1_status_register);
    latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c2_path,i2c_cradletempsensor_status_address,i2c_cradletempsensor_status_register);
    latest_bittest.i2c_displaytouchpanel_status = check_i2c_validity(i2c2_path,i2c_ioexpender_status_address,i2c_ioexpender_status_register);
    if ( file_exists("/dev/hci_tty") >-1 )
    	latest_bittest.ble_status=PASSED;
    else
    	latest_bittest.ble_status=FAILED;
    char *filebuffer = malloc(10 * sizeof(char));
    if ( atoi(read_file_data(battery_exists_path,filebuffer,10)) >0 )
    	latest_bittest.battery_status=PASSED;
    else
    	latest_bittest.battery_status=FAILED;
    free(filebuffer);
    char *filebufferl = malloc(20 * sizeof(char));
    rtc_time_value = atol(read_file_data(rtc_time_read,filebufferl,20));
    sleep(2);
    if ( atol(read_file_data(rtc_time_read,filebufferl,20)) - rtc_time_value > 1 )
    	latest_bittest.rtc_functional_status=PASSED;
    else
    	latest_bittest.rtc_functional_status=FAILED;
    free(filebufferl);
    if ( crc_passed(FW_CONFIG_FILE_PATH) == 0)
    	latest_bittest.fw_crc_status=PASSED;
    else
    	latest_bittest.fw_crc_status=FAILED;
    if ( crc_passed(APK_CONFIG_FILE_PATH) == 0)
    	latest_bittest.apk_crc_status=PASSED;
    else
    	latest_bittest.apk_crc_status=FAILED;
    strftime(latest_bittest.timestamp, 1000, "%c" , p);
    return_current_bit_status(return_reply);
    printf("\n\n*** Bit testing procedure endded! ***\n\n");
}



int main(int argc , char *argv[])
{

    int socket_desc , new_socket , c , *new_sock;
    struct sockaddr_in server , client;
    bittest_init_full();
     
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 5797 );
     
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        puts("bind failed");
        return 1;
    }
    socket_desc_main=socket_desc;
    puts("bind done");
    if (signal(SIGINT, sig_handler) == SIG_ERR); 
    //Listen
    listen(socket_desc , 3);
     
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("server Connection accepted");
        //Reply to the client
        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = new_socket;
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
        {
            perror("server could not create thread");
            return 1;
        }
        //Now join the thread , so that we dont terminate before the thread
        pthread_join( sniffer_thread , NULL);
        puts("server Handler assigned");
    }
     
    if (new_socket<0)
    {
        perror("server accept failed");
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
    int sock = *(int*)socket_desc;
    int read_size;
    char client_message[SOCKET_MESSAGE_MAX_LENGTH];
    char returnMsg[100];
    int fd;
    char error_msg[100];
    char read1[100];
    char read2[100];
    char read3[100];

    struct file_action
    {
         char name [100];
         char value [100];
    };

    char buffer_read_file[150];
    size_t size_of_array_read_file = sizeof(buffer_read_file);

    client_message[0]='\0';
    //Receive a message from client
    while( (read_size = recv(sock , client_message , SOCKET_MESSAGE_MAX_LENGTH , 0)) > 0 )
    {
        //Send the message back to client
        //write(sock , client_message , strlen(client_message));
    client_message[read_size] = '\0';
	#ifdef ServerDebug
	printf("message:%s\n",client_message);
	#endif
	//now make action according to messaage
	if(findSubstr(client_message, "write_file")>-1)
	{
        fflush(stdin);
		/*action is writing to a file
		example: write_file:/sys/class/gpio/export=5
		*/
		struct file_action commandFile;
		strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
		commandFile.name[findSubstr(commandFile.name, "=")-1]='\0';
		#ifdef ServerDebug
		printf("file to write:%s\n",commandFile.name);
        	#endif
		strcpy( commandFile.value, client_message+findSubstr(client_message, "=") );
		#ifdef ServerDebug
		printf("value:%s\n",commandFile.value);
		#endif
		fd = open_file(commandFile.name);
		if (fd<0) 
		{
			strcpy(returnMsg,REPLY_NACK);
			send(sock , returnMsg , strlen(returnMsg),0);
		}
		else
		{
			write( fd, commandFile.value, strlen(commandFile.value) );
			close(fd);
			strcpy(returnMsg,REPLY_ACK);
			send(sock , returnMsg , strlen(returnMsg),0);	
		}
	}
	else if(findSubstr(client_message, "read_file")>-1)
	{
		/*action is reading a file
		example: read_file:/sys/class/gpio/gpio5/value
		*/
		struct file_action commandFile;
		strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
		#ifdef ServerDebug
		printf("file to read:%s\n",commandFile.name);
        #endif
        fd = open_file(commandFile.name);
		if (fd<0) 
        {
            strcpy(error_msg, "Error: Unable to read the value");
            write(sock , error_msg , strlen(error_msg));
            strcpy(returnMsg,REPLY_NACK);
			send(sock , returnMsg , strlen(returnMsg),0);
        }
		else
		{
			
			read(fd, commandFile.value, sizeof(commandFile.value));
			#ifdef ServerDebug
			printf("value read:%s\n",commandFile.value);
        	#endif
			close(fd);
		    send(sock , commandFile.value , sizeof(commandFile.value),0);
            commandFile.value[0] = '\0';
		}
    }
    else if(findSubstr(client_message, "write_bit:bittest")>-1)
    {
        /*action is bittest_init
        */
        char type[128];

        returnMsg[0] = '\0';


        strcpy( type, client_message+findSubstr(client_message, ":") );
        #ifdef ServerDebug
        printf("bittest init type:%s\n",type);
        #endif
        printf("bittest init type:%s\n",type);
        //if(findSubstr(client_message, "full")>-1)
        {
            bittest_init_full();
            sprintf(returnMsg,"%s",return_current_bit_status(returnMsg));
            printf("%s\n", returnMsg);
            write(sock , returnMsg , strlen(returnMsg));
        }
    }
    else if(findSubstr(client_message, "read_bit:bittest")>-1)
    {
        /*action is bittest_read
        */
        returnMsg[0] = '\0';
        sprintf(returnMsg,"%s",return_current_bit_status(returnMsg));
        write(sock , returnMsg , strlen(returnMsg));
    }
    else if(findSubstr(client_message, "read_time")>-1)
	{
		/*action is reading current time
		example: ./send.o 10.0.0.36 read_time
		*/	
		time_t t = time(NULL);
		struct tm * p = localtime(&t);
		returnMsg[0] = '\0';
		strftime(returnMsg, 1000, "%c" , p);	
        send(sock , returnMsg , strlen(returnMsg),0);
        printf("returnMsg\n");
    }
    else if(findSubstr(client_message, "write_time")>-1)
	{
		pid_t my_pid, parent_pid, child_pid;
        fflush(stdin);
		/*action is writing time
		example: ./send.o 10.0.0.36 write_time:060911052016.00
		*/
		struct file_action commandFile;
		strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
		//#ifdef ServerDebug
		printf("time:%s\n",commandFile.name);
        //#endif
        strcpy(returnMsg,REPLY_ACK);
		send(sock , returnMsg , strlen(returnMsg),0);       
   		my_pid = getpid();    
   		parent_pid = getppid();
   		#ifdef ServerDebug
   		printf("\n Parent: my pid is %d\n\n", my_pid);
   		printf("Parent: my parent's pid is %d\n\n", parent_pid);
   		#endif
		/* print error message if fork() fails */
   		if((child_pid = fork()) < 0 )
   		{
      		perror("fork failure");
   		}

   		if(child_pid == 0)
   		{  
   			#ifdef ServerDebug
   			printf("\nChild: I am a new-born process!\n\n");
   			#endif
      		my_pid = getpid();    
      		parent_pid = getppid();
      		#ifdef ServerDebug
      		printf("Child: my pid is: %d\n\n", my_pid);
      		printf("Child: my parent's pid is: %d\n\n", parent_pid);
      		printf("Child: I will execute - date - command \n\n");
      		printf("Child: Now, I woke up and am executing date command \n\n");
      		#endif
      		execl("/system/bin/sh", "/system/bin/sh", "-C", "/system/bin/date_script.sh",commandFile.name, (char *)NULL);
      		perror("execl() failure!\n\n");
   		};
		
	}
	else if(findSubstr(client_message, "write_sleep_time")>-1)
	{
		pid_t my_pid, parent_pid, child_pid;
        fflush(stdin);
		/*action is writing sleep time
		example: ./send.o 10.0.0.36 write_sleep_time:30000
		*/
		struct file_action commandFile;
		strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
		//#ifdef ServerDebug
		printf("sleep time:%s\n",commandFile.name);
        //#endif
        strcpy(returnMsg,REPLY_ACK);
		send(sock , returnMsg , strlen(returnMsg),0);
   		my_pid = getpid();    
   		parent_pid = getppid();
   		#ifdef ServerDebug
   		printf("\n Parent: my pid is %d\n\n", my_pid);
   		printf("Parent: my parent's pid is %d\n\n", parent_pid);
   		#endif
		/* print error message if fork() fails */
   		if((child_pid = fork()) < 0 )
   		{
      		perror("fork failure");
   		}

   		if(child_pid == 0)
   		{  
   			#ifdef ServerDebug
   			printf("\nChild: I am a new-born process!\n\n");
   			#endif
      		my_pid = getpid();    
      		parent_pid = getppid();
      		#ifdef ServerDebug
      		printf("Child: my pid is: %d\n\n", my_pid);
      		printf("Child: my parent's pid is: %d\n\n", parent_pid);
      		printf("Child: I will execute - sleep time - command \n\n");
      		printf("Child: Now, I woke up and am executing sleep time command \n\n");
      		#endif
      		execl("/system/bin/sh", "/system/bin/sh", "-C", "/system/bin/sleep_time_script.sh",commandFile.name, (char *)NULL);
      		perror("execl() failure!\n\n");
   		};
		
	}

    else if(findSubstr(client_message, "read_command:info")>-1)
    {
   // Response: {"build_date": "millis","fw_version": "string_version","device_name": "string_name"}
        returnMsg[0] = '\0';
        read_file_data_no_space("/data/ro_bootimage_build_date_utc",read1,size_of_array_read_file);
        read_file_data_no_space("/data/fs_bsp_version",read2,size_of_array_read_file);
        read_file_data_no_space("/data/ro_product_device",read3,size_of_array_read_file);
        read_file_data_no_space("/data/fs_bsp_version",buffer_read_file,size_of_array_read_file);
        sprintf(returnMsg, " {\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"} \n", "build_date",read1
        ,"fw_version",read2,
        "device_name",read3);
        write(sock , returnMsg , strlen(returnMsg));
    }


    else if(findSubstr(client_message, "read_command:temp_zones")>-1)
    {
        //Response: {"temp_cpu": "temp","temp_wc": "temp"}
        returnMsg[0] = '\0';
        read_file_data_no_space("/sys/class/thermal/thermal_zone1/temp",read1,size_of_array_read_file);
        read_file_data_no_space("/sys/class/thermal/thermal_zone1/temp",read2,size_of_array_read_file);
		sprintf(returnMsg, " {\"%s\":\"%s\",\"%s\":\"%s\"} \n", "temp_cpu",read1
        ,"temp_wc",read2);
        write(sock , returnMsg , strlen(returnMsg));
    }
    else if(findSubstr(client_message, "read_command:hw_info")>-1)
    {
        //Response: {"hall_status":"0\1","battery_status":"0/1","w_charger_state":"0\1"}
        returnMsg[0] = '\0';
        read_file_data_no_space("/sys/class/switch/hall_detect/state",read1,size_of_array_read_file);
        read_file_data_no_space("/sys/class/power_supply/max77818-charger/online",read2,size_of_array_read_file);
        read_file_data_no_space("/sys/class/switch/hall_detect/state",read3,size_of_array_read_file);
        sprintf(returnMsg, " {\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"} \n", "hall_status",read1
        ,"battery_status_charging",read2,
        "w_charger_state",read3);
        write(sock , returnMsg , strlen(returnMsg));
    }
    else if(findSubstr(client_message, "read_command:config")>-1)
    {
    	client_message[0]='\0';
        read_config_file_data("/data/config.file",client_message,SOCKET_MESSAGE_MAX_LENGTH);
        write(sock , client_message , strlen(client_message));
    }
    else if(findSubstr(client_message, "read_command:api_version")>-1)
    {
        client_message[0]='\0';
        strcpy(returnMsg,API_VERSION);
        send(sock , returnMsg , strlen(returnMsg),0);
    }
    else if(findSubstr(client_message, "read_command:device_year")>-1)
    {
		if (ini_parse("/data/config.file", handler, &config) < 0) {
        	printf("Can't load 'test.ini'\n");
        	goto free_socket;
    	}
    	printf("Config loaded from '/data/config.file': device_year=%s\n",
        config.year);
        client_message[0]='\0';
        strcpy(returnMsg,config.year);
        send(sock , returnMsg , strlen(returnMsg),0);
    }
    else if(findSubstr(client_message, "write_command:alarms")>-1)//this is a temp fix for passing alarm status untill unify with interrupt script
    {
    	char alarm_status [6];
    	strcpy( alarm_status, client_message+findSubstr(client_message, "write_command:alarms") + strlen("write_command:alarm") );
        alarms.cpu_high_temperature_alarm =  (uint8_t)(alarm_status[0] - '0');
        alarms.cpu_critical_temperature_alarm =  (uint8_t)(alarm_status[1] - '0');
        alarms.wc_high_temperature_alarm =  (uint8_t)(alarm_status[2] - '0');
        alarms.battery_high_temperature_alarm =  (uint8_t)(alarm_status[3] - '0');
        alarms.battery_not_detected =  (uint8_t)(alarm_status[4] - '0');
        alarms.wc_error_alarm =  (uint8_t)(alarm_status[5] - '0');
 		strcpy(returnMsg,REPLY_ACK);
		send(sock , returnMsg , strlen(returnMsg),0);
    }
    else if(findSubstr(client_message, "read_command:alarms")>-1)
    {
        client_message[0]='\0';
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
    	strcpy(returnMsg,serialized_string);
    	json_free_serialized_string(serialized_string);
    	json_value_free(root_value);       
        send(sock , returnMsg , strlen(returnMsg),0);
    }
    client_message[0]='\0';
	//sleep(1);
    }
     
    if(read_size == 0)
    {
        puts("server Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("server recv failed");
    }    
    //Free the socket pointer
free_socket:    free(socket_desc);
    			close(sock);
    			return 0;
}
