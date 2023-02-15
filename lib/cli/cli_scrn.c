/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) <2016-2019> Intel Corporation.
 */

/* Created by Keith Wiles @ intel.com */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>

#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>

#include <cli.h>
#include "cli_scrn.h"
#include "cli_input.h"

RTE_DEFINE_PER_LCORE(struct cli_scrn *, scrn);

void
__attribute__((format(printf, 3, 4)))
scrn_printf(int16_t r, int16_t c, const char *fmt, ...)
{
	va_list vaList;

	/* In some cases a segfault was reported when this_scrn
	 * would become null.
	 */
	if (this_scrn && this_scrn->fd_out) {
		if ( (r != 0) && (c != 0) )
			scrn_pos(r, c);
		va_start(vaList, fmt);
		vdprintf(this_scrn->fd_out, fmt, vaList);
		va_end(vaList);
		fsync(this_scrn->fd_out);
	}
}

void
__attribute__((format(printf, 3, 4)))
scrn_cprintf(int16_t r, int16_t ncols, const char *fmt, ...)
{
	va_list vaList;
	char str[512];

	if (ncols == -1)
		ncols = this_scrn->ncols;

	va_start(vaList, fmt);
	vsnprintf(str, sizeof(str), fmt, vaList);
	va_end(vaList);

	scrn_pos(r, scrn_center_col(ncols, str));
	scrn_puts("%s", str);
}
#if 0
void
__attribute__((format(printf, 4, 5)))
scrn_fprintf(int16_t r, int16_t c, FILE *f, const char *fmt, ...)
{
	va_list vaList;

	if ( (r != 0) && (c != 0) )
		scrn_pos(r, c);
	va_start(vaList, fmt);
	vfprintf(f, fmt, vaList);
	va_end(vaList);
	fflush(f);
}
#else
void
__attribute__((format(printf, 4, 5)))
scrn_fprintf(int16_t r, int16_t c, int fd, const char *fmt, ...)
{
	va_list vaList;

	if ( (r != 0) && (c != 0) )
		scrn_pos(r, c);
	va_start(vaList, fmt);
	vdprintf(fd, fmt, vaList);
	va_end(vaList);
	fsync(fd);
}

#endif

static void
scrn_set_io(int in, int out)
{
	struct cli_scrn *scrn = this_scrn;

	if (scrn) {
		scrn->fd_in = in;
		scrn->fd_out = out;
	}
}
#ifdef CONFIG_TELNET_CLI
#include "cli_telnet.h"
static void sendstr(int fd, const char *s)
{
	send(fd, s, strlen(s), 0);
}

static void sendcrlf(int fd)
{
	sendstr(fd, "\r\n> ");
}

static void cli_send_welcome_banner(int fd)
{
	char sendbuf[512];
	sprintf(sendbuf,
		"\r\n"
		"--==--==--==--==--==--==--==--==--==--==--\r\n"
		"------ WELCOME to DPDK-Pktgen CLI ------\r\n"
		"--==--==--==--==--==--==--==--==--==--==--\r\n"
		);
	sendstr(fd, sendbuf);
	sendcrlf(fd);
}

static char telnet_echo_off[] = {
	0xff, 0xfb, 0x01, /* IAC WILL ECHO */
	0xff, 0xfb, 0x03, /* IAC WILL SUPPRESS_GO_AHEAD */
	0xff, 0xfd, 0x03, /* IAC DO SUPPRESS_GO_AHEAD */
};

static void cli_sa_accept(int fd)
{
	send(fd, telnet_echo_off, sizeof(telnet_echo_off), 0);
	printf("[Note] new sock %d opened\r\n", fd);
}
#endif

static int
scrn_stdin_setup(void)
{
	struct cli_scrn *scrn = this_scrn;
	if (!scrn)
		return -1;
#ifdef CONFIG_TELNET_CLI
	int fd = get_cli_socket_fd();
	scrn_set_io(fd, fd);
	cli_sa_accept(fd);
	cli_send_welcome_banner(fd);
#else
	struct termios term;
	scrn_set_io(fileno(stdin), fileno(stdout));

	memset(&scrn->oldterm, 0, sizeof(term));
	if (tcgetattr(scrn->fd_in, &scrn->oldterm) ||
	    tcgetattr(scrn->fd_in, &term)) {
		fprintf(stderr, "%s: setup failed for tty\n", __func__);
		return -1;
	}
	term.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);

	if (tcsetattr((scrn->fd_in), TCSANOW, &term)) {
		fprintf(stderr, "%s: failed to set tty\n", __func__);
		return -1;
	}
#endif
	return 0;
}

static void
scrn_stdin_restore(void)
{
	struct cli_scrn *scrn = this_scrn;

	if (!scrn)
		return;
#ifndef CONFIG_TELNET_CLI
	if (tcsetattr((scrn->fd_in), TCSANOW, &scrn->oldterm))
		fprintf(stderr, "%s: failed to set tty\n", __func__);

	if (system("stty sane"))
		fprintf(stderr, "%s: system command failed\n", __func__);
#endif
}

static void
handle_winch(int sig)
{
	struct winsize w;

	if (sig != SIGWINCH)
		return;

	ioctl(0, TIOCGWINSZ, &w);

	this_scrn->nrows = w.ws_row;
	this_scrn->ncols = w.ws_col;

	/* Need to refreash the screen */
	//cli_clear_screen();
	cli_clear_line(-1);
	cli_redisplay_line();
}

int
scrn_create(int scrn_type, int theme)
{
	struct winsize w;
	struct cli_scrn *scrn = this_scrn;
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = handle_winch;
	sigaction(SIGWINCH, &sa, NULL);

	if (!scrn) {
		scrn = malloc(sizeof(struct cli_scrn));
		if (!scrn)
			return -1;
		memset(scrn, 0, sizeof(struct cli_scrn));

		this_scrn = scrn;
	}

	rte_atomic32_set(&scrn->pause, SCRN_SCRN_PAUSED);

	ioctl(0, TIOCGWINSZ, &w);

	scrn->nrows = w.ws_row;
	scrn->ncols = w.ws_col;
	scrn->theme = theme;
	scrn->type  = scrn_type;

	if (scrn_type == SCRN_STDIN_TYPE) {
		if (scrn_stdin_setup()) {
			free(scrn);
			return -1;
		}
	} else {
		free(scrn);
		return -1;
	}

	scrn_color(SCRN_DEFAULT_FG, SCRN_DEFAULT_BG, SCRN_OFF);

	return 0;
}

int
scrn_create_with_defaults(int theme)
{
	return scrn_create(SCRN_STDIN_TYPE,
	                   (theme)? SCRN_THEME_ON : SCRN_THEME_OFF);
}

void
scrn_destroy(void)
{
	if (this_scrn && (this_scrn->type == SCRN_STDIN_TYPE))
		scrn_stdin_restore();
}
