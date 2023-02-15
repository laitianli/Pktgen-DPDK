#ifndef __CLI_TELNET_H_
#define __CLI_TELNET_H_

#include <semaphore.h>

void cli_create_thread(void);
int get_cli_socket_fd(void);
void cli_telnet_server_start(sem_t *sem);
void cli_quit_telnet_server(void);
int get_telnet_server_status(void);
#endif


