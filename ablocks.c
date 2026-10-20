/* See LICENSE file for copyright and license details.
 *
 * ablocks - i3bar status command.
 *
 * Each block is a shell command run on an interval. Its stdout is read
 * i3blocks-style: line 1 is full_text, line 2 short_text, line 3 color;
 * lines starting with '@' are reserved for animation directives. Exit code
 * 33 marks the block urgent. Commands run asynchronously, so a slow block
 * never holds up the others.
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))
#define OUTLEN    1024
#define URGENT    33

enum { Once = 0 }; /* interval: run at startup only */

typedef struct {
	const char *name;
	const char *command;
	int interval; /* seconds between runs, or Once */
	int signal;   /* RTMIN+signal re-runs the block; 0 for none */
	int separator;
} Block;

#include "config.h"

typedef struct {
	char full[OUTLEN];
	char shrt[OUTLEN];
	char color[32];
	int urgent;
	char buf[OUTLEN]; /* output of the command in flight */
	size_t len;
	pid_t pid;
	int fd;           /* read end of its stdout; -1 when idle */
	long long next;   /* monotonic ms of the next run; -1 for never */
} State;

static State st[LENGTH(blocks)];
static int dirty;
static int drawn;

static long long
now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static void
die(const char *s)
{
	fprintf(stderr, "ablocks: %s: %s\n", s, strerror(errno));
	exit(1);
}

static void
run(size_t i)
{
	State *s = &st[i];
	int p[2], null;

	if (s->fd >= 0)
		return; /* still running: never stack copies of a slow command */
	if (pipe(p) < 0) {
		perror("ablocks: pipe");
		return;
	}
	if ((s->pid = fork()) < 0) {
		perror("ablocks: fork");
		close(p[0]);
		close(p[1]);
		return;
	}
	if (s->pid == 0) {
		close(p[0]);
		dup2(p[1], STDOUT_FILENO);
		close(p[1]);
		/* stdin is i3bar's click stream; the command must not eat it */
		if ((null = open("/dev/null", O_RDONLY)) >= 0)
			dup2(null, STDIN_FILENO);
		setenv("BLOCK_NAME", blocks[i].name, 1);
		setenv("BLOCK_BUTTON", "", 1);
		execl(shell, shell, "-c", blocks[i].command, (char *)NULL);
		_exit(127);
	}
	close(p[1]);
	fcntl(p[0], F_SETFD, FD_CLOEXEC);
	fcntl(p[0], F_SETFL, O_NONBLOCK);
	s->fd = p[0];
	s->len = 0;
}

static void
setfield(char *dst, size_t size, const char *src, size_t len)
{
	if (len >= size)
		len = size - 1;
	memcpy(dst, src, len);
	dst[len] = '\0';
}

/* Split the finished command's output into fields; mark dirty on change. */
static void
parse(State *s, int status)
{
	char full[OUTLEN] = "", shrt[OUTLEN] = "", color[32] = "";
	char *p = s->buf, *end = s->buf + s->len, *nl;
	int n = 0, urgent;

	while (p < end) {
		if (!(nl = memchr(p, '\n', end - p)))
			nl = end;
		if (*p != '@') {
			if (n == 0)
				setfield(full, sizeof(full), p, nl - p);
			else if (n == 1)
				setfield(shrt, sizeof(shrt), p, nl - p);
			else if (n == 2)
				setfield(color, sizeof(color), p, nl - p);
			n++;
		}
		p = nl + 1;
	}
	urgent = WIFEXITED(status) && WEXITSTATUS(status) == URGENT;

	if (strcmp(full, s->full) || strcmp(shrt, s->shrt) ||
	    strcmp(color, s->color) || urgent != s->urgent) {
		strcpy(s->full, full);
		strcpy(s->shrt, shrt);
		strcpy(s->color, color);
		s->urgent = urgent;
		dirty = 1;
	}
}

static void
collect(size_t i)
{
	State *s = &st[i];
	char tmp[512];
	ssize_t n;
	size_t room;
	int status = 0;

	while ((n = read(s->fd, tmp, sizeof(tmp))) > 0) {
		/* keep draining past a full buffer so the writer never blocks */
		room = sizeof(s->buf) - s->len;
		if ((size_t)n > room)
			n = room;
		memcpy(s->buf + s->len, tmp, n);
		s->len += n;
	}
	if (n < 0 && (errno == EAGAIN || errno == EINTR))
		return;
	close(s->fd);
	s->fd = -1;
	waitpid(s->pid, &status, 0);
	parse(s, status);
}

static void
jsonstr(const char *s)
{
	unsigned char c;

	putchar('"');
	for (; (c = *s); s++) {
		if (c == '"' || c == '\\')
			printf("\\%c", c);
		else if (c < 0x20)
			printf("\\u%04x", c);
		else
			putchar(c);
	}
	putchar('"');
}

static void
draw(void)
{
	size_t i;
	int first = 1;

	fputs(drawn ? ",[" : "[", stdout);
	for (i = 0; i < LENGTH(blocks); i++) {
		if (!st[i].full[0])
			continue; /* an empty block would still draw a separator */
		if (!first)
			putchar(',');
		first = 0;
		fputs("{\"name\":", stdout);
		jsonstr(blocks[i].name);
		fputs(",\"full_text\":", stdout);
		jsonstr(st[i].full);
		if (st[i].shrt[0]) {
			fputs(",\"short_text\":", stdout);
			jsonstr(st[i].shrt);
		}
		if (st[i].color[0]) {
			fputs(",\"color\":", stdout);
			jsonstr(st[i].color);
		}
		if (st[i].urgent)
			fputs(",\"urgent\":true", stdout);
		printf(",\"markup\":\"%s\",\"separator\":%s,\"separator_block_width\":%d}",
		       markup, blocks[i].separator ? "true" : "false", sepwidth);
	}
	puts("]");
	if (fflush(stdout) == EOF)
		die("write"); /* i3bar is gone */
	drawn = 1;
	dirty = 0;
}

int
main(void)
{
	struct pollfd pfd[LENGTH(blocks)];
	size_t map[LENGTH(blocks)];
	size_t i, n;
	long long t, wait;

	for (i = 0; i < LENGTH(blocks); i++) {
		st[i].fd = -1;
		st[i].next = 0;
	}
	fputs("{\"version\":1}\n[\n", stdout);

	for (;;) {
		t = now();
		wait = -1;
		for (i = 0; i < LENGTH(blocks); i++) {
			if (st[i].next >= 0 && st[i].next <= t) {
				run(i);
				st[i].next = blocks[i].interval > 0
				           ? t + blocks[i].interval * 1000LL : -1;
			}
			if (st[i].next >= 0 && (wait < 0 || st[i].next - t < wait))
				wait = st[i].next - t;
		}

		for (i = n = 0; i < LENGTH(blocks); i++) {
			if (st[i].fd < 0)
				continue;
			pfd[n].fd = st[i].fd;
			pfd[n].events = POLLIN;
			map[n++] = i;
		}
		if (poll(pfd, n, wait > 0x7fffffff ? 0x7fffffff : (int)wait) < 0 &&
		    errno != EINTR)
			die("poll");
		for (i = 0; i < n; i++)
			if (pfd[i].revents)
				collect(map[i]);

		if (dirty)
			draw();
	}
}
