#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>
#include <cli.h>
#include "cli_telnet.h"
static int cli_serv_fd = -1, telnet_client_fd = -1;
static int telnet_is_running = 1;

struct sp_epoll_fds {
	int 		pfd;
	unsigned int numfds;
};

static struct sp_epoll_fds sp_fds;


static inline int init_epoll_pfd(int fd)
{
	static int pfd = 0;
	static int first = 1;
	if (first) {
		memset(&sp_fds, 0, sizeof(sp_fds));
		pfd = epoll_create(1);
		first = 0;
	}
	sp_fds.pfd = pfd;
	static struct epoll_event pipe_event = {
			.events = EPOLLIN | EPOLLPRI,
		};
	sp_fds.numfds += 1;
	pipe_event.data.fd = fd;
	if (epoll_ctl(pfd, EPOLL_CTL_ADD, fd, &pipe_event) < 0) {
			printf("Error adding fd to %d epoll_ctl, %s\n",
					fd, strerror(errno));
			return -1;
	}
	return 0;
}

static void* cli_server(void *arg)
{
    int alen;
    struct sockaddr_in my_addr, caller;
    int reuse = 1;
	int i = 0;
	int nfds = 0;
	sem_t *sem = (sem_t*)arg;
	pthread_setname_np(pthread_self(), "cli-telnet");
	struct epoll_event events[sp_fds.numfds];
	cli_serv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (cli_serv_fd < 0) {
		printf("cli serv socket\n");		
		return NULL;
	}
	if (setsockopt(cli_serv_fd, SOL_SOCKET,
		   SO_REUSEADDR, (void *)&reuse, sizeof(reuse)) < 0)
		printf("[Error] cli setsockopt (SO_REUSEADDR)\n");
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(9988);
	my_addr.sin_addr.s_addr = htonl(0);

	if (bind(cli_serv_fd, (struct sockaddr *)&my_addr,
		 sizeof(struct sockaddr)) < 0) {
		printf("[Error] serv bind failed\n");
		return NULL;
	}

	listen(cli_serv_fd, 1);
	init_epoll_pfd(cli_serv_fd);
	while (telnet_is_running) {
		nfds = epoll_wait(sp_fds.pfd, events, sp_fds.numfds, 2000);
		/* epoll_wait fail */
		if (nfds < 0) {
			if (errno == EINTR)
				continue;
			printf("[Error] epoll_wait returns with fail\n");
			return NULL;
		}
		/* epoll_wait timeout, will never happens here */
		else if (nfds == 0)
			continue;
		for (i = 0; i < nfds; i++) {
			if (cli_serv_fd == events[i].data.fd) {
				alen = sizeof(caller);
				if (telnet_client_fd != -1) {
					printf("[Note] new client connect, so close old connet!\n");
					close(telnet_client_fd);
					telnet_client_fd = -1;
				}
				telnet_client_fd = accept(cli_serv_fd,
						(struct sockaddr *)&caller,
						(socklen_t *)&alen);
				if (telnet_client_fd < 0) {
					printf("cli serv accept failed, errno: %d\n", errno);
					break;
				}				
				printf("CLI connection established\r\n");
				sem_post(sem);
			}
		}	
	} /* while () */
	if (telnet_client_fd > 0)
		close(telnet_client_fd);
	telnet_client_fd = -1;

	close(cli_serv_fd);
	cli_serv_fd = -1;
	return NULL;
}

void cli_telnet_server_start(sem_t *sem)
{
	pthread_t tid;
	pthread_create(&tid, NULL, cli_server, sem);
}

int get_cli_socket_fd(void)
{
	return telnet_client_fd;
}

void cli_quit_telnet_server(void)
{
	telnet_is_running = 0;
}

int get_telnet_server_status(void)
{
	return telnet_is_running;
}



