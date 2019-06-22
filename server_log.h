/*
 * server_log.h
 *
 *  Created on: June 22, 2019
 *      Author: Ilan ganor
 */

#ifndef SERVER_LOG_H_
#define SERVER_LOG_H_

#define C_COMPILE 1
#undef C_COMPILE    //remark this line to compile as stand alone standard C,
#ifndef C_COMPILE
#include <cutils/klog.h>
#endif
#ifndef C_COMPILE
#include <ND_LogLibrary.h>
#endif
#ifndef C_COMPILE
#include <ND_LogLibrary.h>
#endif

#ifndef C_COMPILE
#define server_daemon_kmsg_print(x...) KLOG_ERROR("server_daemon", x)
#else
#define server_daemon_kmsg_print(x...) printf(x...)
#define ND_printlog(x...)  printf("test")
#endif
#endif /* SERVER_LOG_H_ */

