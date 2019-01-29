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
#include "i2c.h"
#define ServerDebug 1
#ifdef ServerDebug
#undef ServerDebug
#endif
#define CONFIG_FILE_PATH "/system/bin/config.file"
#define BIT_I2C "BIT_I2C"
#define BIT_BLE "BIT_BLE"
#define BIT_BATTERY "BIT_BATTERY"
#define BIT_RTC "BIT_RTC"
#define BIT_DISPLAY "BIT_DISPLAY"
#define BIT_GPIOEXPENDER "BIT_GPIOEXPENDER"
#define BIT_FUELGAUGE "BIT_FUELGAUGE"
#define BIT_CRC "BIT_CRC"
#define BIT_TIMESTAMP "BIT_TIMESTAMP"
#define SOCKET_MESSAGE_MAX_LENGTH 2000
#define REPLY_ACK "ACK"
#define REPLY_NACK "NACK"


enum BITRESULT            /* Defines results  */  
{  
    PASSED = 0, 
    FAILED      
} ;

struct bittest_info
{
    uint8_t i2c_status;
    uint8_t ble_status;
    uint8_t battery_status;
    uint8_t rtc_status;
    uint8_t display_status;
    uint8_t gpioexpender_status;
    uint8_t fuelgauge_status;
    uint8_t crc_status;
    char timestamp [100];
};



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

struct bittest_info currect_bittest;

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
/*
handles read i2c server command
exaqmple: /dev/i2c-1;0x1b;0x5d
*/
uint8_t read_i2c(char *string_command)
{
     char *path;
     uint8_t addr=0, reg=0;
     int data;
     int file, rc;

     const char s[2] = ";";

     char *tmp[4];

     tmp[0] = strtok(string_command, s);

     for (int i = 1; i < 4; ++i)
     {
         tmp[i] = strtok(NULL, s);
     }

     if(NULL != tmp[3])
     {
         printf("Error in i2c input");
         return (0);
     }
     else
     {
         path = tmp[0];
         addr = (uint8_t)strtol(tmp[1], NULL, 16);
         reg = (uint8_t)strtol(tmp[2], NULL, 16);
    #ifdef ServerDebug
         printf("Path: %s\nAddr: %s(hex), %d(int)\nReg: %s(hex), %d(int)\n", path, tmp[1], addr, tmp[2], reg);
    #endif
     }

    file = open(path, O_RDWR);
    if (file < 0)
        err(errno, "Tried to open '%s'", path);

    rc = ioctl(file, I2C_SLAVE_FORCE, addr);
    if (rc < 0)
        err(errno, "Tried to set device address '0x%02x'", addr);

    // i2c_smbus_read_byte_data - ?

    data = i2c_smbus_read_byte_data(file, reg);
    //printf("%s: device 0x%02x at address 0x%02x: 0x%02x\n",path, addr, reg, data);
    printf("%d\n", data );
    return data;
} 


int crc_passed(char * filename)
{
	size_t buffer_size = 64;
	char *buffer;
	char full_buffer[1024];
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
    while(-1 != getline(&buffer, &buffer_size, file))
    {
        char *pch = strstr(buffer, "crc");
        if(!pch) // crc line does not get included in crc calculation
        	strncat(full_buffer,buffer,strlen(buffer)-1);
        else
        {
        	char *p = buffer;
			while (*p) 
			{ // While there are more characters to process...
    			if (isdigit(*p)) 
    			{ // Upon finding a digit, ...
        			val = strtol(p, &p, 10); // Read a number, ...
        			printf("\r\nCRC:%ld\n", val); // and print it.
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
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x <<5)) ^ ((unsigned short)x);
    }
    printf ("CRC result: %d\r\n",crc);
    if (crc==val)
    	return 0;
    else return -1;
}

//fucnction that runs bittest full test
void bittest_init_full()
{
    // /sys/halleffect/
    // /dev/hci_tty

    time_t t = time(NULL);
    struct tm * p = localtime(&t);
    currect_bittest.i2c_status=FAILED;
    currect_bittest.ble_status=FAILED;
    currect_bittest.battery_status=FAILED;
    currect_bittest.rtc_status=FAILED;
    currect_bittest.display_status=FAILED;
    currect_bittest.gpioexpender_status=FAILED;
    currect_bittest.fuelgauge_status=FAILED;
    currect_bittest.crc_status=FAILED;

    printf("\n\n*** Bit testing procedure started! ***\n\n");
    //BLE test 
    if ( file_exists("/dev/hci_tty") >-1 ) currect_bittest.ble_status=PASSED;
    printf("BLE test: %d\n", currect_bittest.ble_status);
    //battery test
    char *batterybuffer = malloc(10 * sizeof(char));
    if ( atoi(read_file_data("/sys/class/power_supply/battery/present",batterybuffer,10)) >0 ) currect_bittest.battery_status=PASSED;
    free(batterybuffer);
    printf("battery test: %d\n", currect_bittest.battery_status );
    //rtc test
    if ( file_exists("/data/rtctest") >-1 )currect_bittest.rtc_status=PASSED;
    printf("rtc test: %d\n", currect_bittest.rtc_status );
    //display test
    currect_bittest.display_status=PASSED;
    printf("display test: %d\n", currect_bittest.display_status );
    //gpio i2c expender test
    if ( file_exists("/data/ledred") >-1 ) currect_bittest.gpioexpender_status=PASSED;
    printf("gpioexpender_status test: %d\n", currect_bittest.gpioexpender_status );
    //fuelgauge test
    char *fuelgaugebuffer = malloc(10 * sizeof(char));
    if ( atoi(read_file_data("/sys/class/power_supply/battery/voltage_now",fuelgaugebuffer,10)) >0 ) currect_bittest.fuelgauge_status=PASSED;
    free(fuelgaugebuffer);
    printf("fuelgauge test: %d\n", currect_bittest.fuelgauge_status );
    //CRC test
    if ( crc_passed(CONFIG_FILE_PATH) == 0) currect_bittest.crc_status=PASSED;
    printf("crc_status test: %d\n", currect_bittest.crc_status );
    //I2C test
    if (currect_bittest.gpioexpender_status==PASSED||currect_bittest.fuelgauge_status==PASSED||currect_bittest.battery_status==PASSED) currect_bittest.i2c_status=PASSED;
    printf("I2C test: %d\n", currect_bittest.i2c_status );


    if (currect_bittest.ble_status==PASSED  &&
    currect_bittest.rtc_status==PASSED  
    && currect_bittest.gpioexpender_status==PASSED && currect_bittest.crc_status==PASSED)
    {
        strftime(currect_bittest.timestamp, 1000, "%c" , p);
        printf("\n\n*** Bit test Passed! ***\n\n");
        printf("\n\n*** Timestamp : %s ***\n\n", currect_bittest.timestamp );
    }
    else
    {
    	strftime(currect_bittest.timestamp, 1000, "%c" , p);
        printf("\n\n*** Bit test Failed! ***\n\n");
    }
    


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
    int result;
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
    else if(findSubstr(client_message, "i2c_read")>-1)
    {
    	/*action is reading a I2C register
    	example: i2c_read:/dev/i2c-1;0x1b;0x5d
    	*/
    	struct file_action commandFile;
		strcpy( commandFile.name, client_message+findSubstr(client_message, ":") );
		result=read_i2c(commandFile.name);
    	#ifdef ServerDebug
    	printf("value read:%04x\n",result);
    	#endif
		sprintf(commandFile.value, "%04x\n", result);
    	write(sock , commandFile.value , strlen(commandFile.value));
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
            sprintf(returnMsg, " {\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%s\"} \n", BIT_BLE,currect_bittest.ble_status , BIT_RTC,currect_bittest.rtc_status, BIT_GPIOEXPENDER,currect_bittest.gpioexpender_status,BIT_CRC,currect_bittest.crc_status ,BIT_TIMESTAMP,currect_bittest.timestamp);
            printf("%s\n", returnMsg);
            write(sock , returnMsg , strlen(returnMsg));
        }
    }
    else if(findSubstr(client_message, "read_bit:bittest")>-1)
    {
        /*action is bittest_read
        */
        returnMsg[0] = '\0';

        sprintf(returnMsg, " {\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%s\"} \n", BIT_BLE,currect_bittest.ble_status , BIT_RTC,currect_bittest.rtc_status, BIT_GPIOEXPENDER,currect_bittest.gpioexpender_status,BIT_CRC,currect_bittest.crc_status ,BIT_TIMESTAMP,currect_bittest.timestamp);
        send(sock , returnMsg , strlen(returnMsg),0);
        printf("Bit test status sent\n");
        
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
        read_file_data_no_space("/sys/class/thermal/thermal_zone0/temp",read1,size_of_array_read_file);
        read_file_data_no_space("/sys/class/thermal/thermal_zone0/temp",read2,size_of_array_read_file);
		sprintf(returnMsg, " {\"%s\":\"%s\",\"%s\":\"%s\"} \n", "temp_cpu",read1
        ,"temp_wc",read2);
        write(sock , returnMsg , strlen(returnMsg));
    }
    else if(findSubstr(client_message, "read_command:hw_info")>-1)
    {
        //Response: {"hall_status":"0\1","battery_status":"0/1","w_charger_state":"0\1"}
        returnMsg[0] = '\0';
        read_file_data_no_space("/data/hall_detect",read1,size_of_array_read_file);
        read_file_data_no_space("/sys/class/power_supply/battery/charge_now",read2,size_of_array_read_file);
        read_file_data_no_space("/data/hall_detect",read3,size_of_array_read_file);
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
    free(socket_desc);
    close(sock);

    return 0;
}
