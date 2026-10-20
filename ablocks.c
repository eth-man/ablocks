/* See LICENSE file for copyright and license details.
 *
 * ablocks - i3bar status command.
 *
 * Each block is a shell command run on an interval. Its stdout is read
 * i3blocks-style: line 1 is full_text, line 2 short_text, line 3 color;
 * lines starting with '@' are reserved for animation directives. Exit code
 * 33 marks the block urgent. Commands run asynchronously, so a slow block
 * never holds up the others.
 *
 * SIGRTMIN+n re-runs every block with signal n, SIGUSR1 re-runs them all.
 * A click from i3bar re-runs the block with BLOCK_BUTTON set. A signal or
 * click that lands while the block is still running is queued, so the bar
 * always ends up showing the state from after the last event.
 *
 * An animation is a list of pango frames wrapped around a block's output,
 * "%s" standing for the output itself. At startup the boot animation plays
 * across the bar as a wave, left to right; SIGUSR2 replays it.
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))
#define OUTLEN    1024
#define INLEN     4096
#define URGENT    33
#define MAXFRAMES 16

enum { Once = 0 }; /* interval: run at startup only */

typedef struct {
	const char *name;
	const char *command;
	int interval; /* seconds between runs, or Once */
	int signal;   /* RTMIN+signal re-runs the block; 0 for none */
	int separator;
} Block;

typedef struct {
	const char *name;
	unsigned int ms;                 /* how long each frame is shown */
	const char *frames[MAXFRAMES];   /* NULL-terminated; "%s" is the output */
} Anim;

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
	int pending;      /* re-run as soon as the current run finishes */
	int pbutton;      /* BLOCK_BUTTON for that re-run */
	const Anim *anim; /* animation playing, or NULL */
	long long astart; /* when it starts; frame 0 is held until then */
	int frame;        /* frame currently drawn */
} State;

static State st[LENGTH(blocks)];
static int dirty;
static int drawn;
static int sigfd[2];          /* self-pipe: signal handler -> main loop */
static int infd = STDIN_FILENO; /* i3bar's click events; -1 once closed */
static char inbuf[INLEN];
static size_t inlen;
static long long bootat = -1;   /* when to play the boot animation */

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
run(size_t i, int button)
{
	State *s = &st[i];
	char b[16] = "";
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
		if (button)
			snprintf(b, sizeof(b), "%d", button);
		setenv("BLOCK_BUTTON", b, 1);
		execl(shell, shell, "-c", blocks[i].command, (char *)NULL);
		_exit(127);
	}
	close(p[1]);
	fcntl(p[0], F_SETFD, FD_CLOEXEC);
	fcntl(p[0], F_SETFL, O_NONBLOCK);
	s->fd = p[0];
	s->len = 0;
}

/* Run now for a signal or click, or queue it behind the run in flight. */
static void
trigger(size_t i, int button)
{
	if (st[i].fd >= 0) {
		st[i].pending = 1;
		st[i].pbutton = button;
	} else {
		run(i, button);
	}
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
	if (s->pending) {
		s->pending = 0;
		run(i, s->pbutton);
	}
}

static void
onsignal(int sig)
{
	int e = errno;
	unsigned char c = sig;

	write(sigfd[1], &c, 1);
	errno = e;
}

static void
readsignals(void)
{
	unsigned char c[64];
	ssize_t n, k;
	size_t i;

	while ((n = read(sigfd[0], c, sizeof(c))) > 0) {
		for (k = 0; k < n; k++) {
			if (c[k] == SIGUSR2) {
				bootat = now();
				continue;
			}
			for (i = 0; i < LENGTH(blocks); i++)
				if (c[k] == SIGUSR1 || (blocks[i].signal &&
				    c[k] == SIGRTMIN + blocks[i].signal))
					trigger(i, 0);
		}
	}
}

static void
setupsignals(void)
{
	struct sigaction sa;
	size_t i;
	int j;

	if (pipe(sigfd) < 0)
		die("pipe");
	for (j = 0; j < 2; j++) {
		fcntl(sigfd[j], F_SETFD, FD_CLOEXEC);
		fcntl(sigfd[j], F_SETFL, O_NONBLOCK);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = onsignal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	for (i = 0; i < LENGTH(blocks); i++) {
		if (!blocks[i].signal)
			continue;
		if (SIGRTMIN + blocks[i].signal > SIGRTMAX)
			fprintf(stderr, "ablocks: %s: signal %d is past "
			        "SIGRTMAX (max %d), ignored\n", blocks[i].name,
			        blocks[i].signal, SIGRTMAX - SIGRTMIN);
		else
			sigaction(SIGRTMIN + blocks[i].signal, &sa, NULL);
	}
}

/* Copy the value of "key" from one flat i3bar click object; "" if absent. */
static void
jsonget(const char *obj, const char *key, char *dst, size_t size)
{
	char pat[32];
	const char *p;
	size_t n = 0;

	*dst = '\0';
	snprintf(pat, sizeof(pat), "\"%s\":", key);
	if (!(p = strstr(obj, pat)))
		return;
	for (p += strlen(pat); *p == ' '; p++)
		;
	if (*p == '"') {
		for (p++; *p && *p != '"' && n + 1 < size; p++) {
			if (*p == '\\' && p[1])
				p++;
			dst[n++] = *p;
		}
	} else {
		for (; *p && *p != ',' && *p != '}' && n + 1 < size; p++)
			dst[n++] = *p;
	}
	dst[n] = '\0';
}

static void
click(const char *obj)
{
	char name[64], button[16];
	size_t i;

	jsonget(obj, "name", name, sizeof(name));
	jsonget(obj, "button", button, sizeof(button));
	for (i = 0; i < LENGTH(blocks); i++)
		if (!strcmp(blocks[i].name, name))
			trigger(i, atoi(button));
}

/* i3bar sends "[" and then one click object per line, each after the
 * first prefixed with ','. Only whole lines are acted on. */
static void
readclicks(void)
{
	char *p, *nl;
	ssize_t n;

	n = read(infd, inbuf + inlen, sizeof(inbuf) - 1 - inlen);
	if (n < 0 && (errno == EAGAIN || errno == EINTR))
		return;
	if (n <= 0) {
		infd = -1; /* no i3bar on the other end: carry on without clicks */
		return;
	}
	inlen += n;
	inbuf[inlen] = '\0';
	for (p = inbuf; (nl = strchr(p, '\n')); p = nl + 1) {
		*nl = '\0';
		click(p);
	}
	inlen -= p - inbuf;
	memmove(inbuf, p, inlen);
	if (inlen == sizeof(inbuf) - 1)
		inlen = 0; /* a line that long is not a click: drop it */
}

static const Anim *
findanim(const char *name)
{
	size_t i;

	for (i = 0; name && i < LENGTH(anims); i++)
		if (!strcmp(anims[i].name, name))
			return &anims[i];
	return NULL;
}

static int
nframes(const Anim *a)
{
	int n = 0;

	while (n < MAXFRAMES && a->frames[n])
		n++;
	return n;
}

/* Start the boot animation on every visible block, each a little after its
 * left neighbour so it runs across the bar as a wave. */
static void
boot(long long t)
{
	const Anim *a = findanim(bootanim);
	size_t i, k = 0;

	if (!a || !nframes(a))
		return;
	for (i = 0; i < LENGTH(blocks); i++) {
		if (!st[i].full[0])
			continue;
		st[i].anim = a;
		st[i].astart = t + (long long)k++ * bootstagger;
		st[i].frame = 0;
	}
	dirty = 1;
}

/* Bring every animation up to time t; return ms until the next frame
 * change anywhere, or -1 when nothing is animating. */
static long long
animate(long long t)
{
	State *s;
	long long w = -1, at;
	size_t i;
	int f;

	for (i = 0; i < LENGTH(blocks); i++) {
		s = &st[i];
		if (!s->anim)
			continue;
		f = t < s->astart ? 0 : (t - s->astart) / s->anim->ms;
		if (f >= nframes(s->anim)) {
			s->anim = NULL;
			dirty = 1;
			continue;
		}
		if (f != s->frame) {
			s->frame = f;
			dirty = 1;
		}
		at = s->astart + (long long)(f + 1) * s->anim->ms;
		if (w < 0 || at - t < w)
			w = at - t;
	}
	return w;
}

/* Copy fmt into dst with every "%s" replaced by s. */
static void
subst(char *dst, size_t size, const char *fmt, const char *s)
{
	size_t n = 0, l;

	while (*fmt && n + 1 < size) {
		if (fmt[0] == '%' && fmt[1] == 's') {
			if ((l = strlen(s)) > size - 1 - n)
				l = size - 1 - n;
			memcpy(dst + n, s, l);
			n += l;
			fmt += 2;
		} else {
			dst[n++] = *fmt++;
		}
	}
	dst[n] = '\0';
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
	char text[2 * OUTLEN];
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
		if (st[i].anim) {
			subst(text, sizeof(text), st[i].anim->frames[st[i].frame], st[i].full);
			jsonstr(text);
		} else {
			jsonstr(st[i].full);
		}
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
	struct pollfd pfd[LENGTH(blocks) + 2];
	size_t map[LENGTH(blocks)];
	size_t i, n, nblk;
	long long t, wait, w;

	for (i = 0; i < LENGTH(blocks); i++) {
		st[i].fd = -1;
		st[i].next = 0;
	}
	setupsignals();
	if (bootanim)
		bootat = now() + bootdelay;
	fputs("{\"version\":1,\"click_events\":true}\n[\n", stdout);

	for (;;) {
		t = now();
		wait = -1;
		for (i = 0; i < LENGTH(blocks); i++) {
			if (st[i].next >= 0 && st[i].next <= t) {
				run(i, 0);
				st[i].next = blocks[i].interval > 0
				           ? t + blocks[i].interval * 1000LL : -1;
			}
			if (st[i].next >= 0 && (wait < 0 || st[i].next - t < wait))
				wait = st[i].next - t;
		}
		if (bootat >= 0 && bootat <= t) {
			boot(t);
			bootat = -1;
		}
		if (bootat >= 0 && (wait < 0 || bootat - t < wait))
			wait = bootat - t;
		if ((w = animate(t)) >= 0 && (wait < 0 || w < wait))
			wait = w;
		/* before poll, or a frame would only appear after the sleep */
		if (dirty)
			draw();

		for (i = n = 0; i < LENGTH(blocks); i++) {
			if (st[i].fd < 0)
				continue;
			pfd[n].fd = st[i].fd;
			pfd[n].events = POLLIN;
			map[n++] = i;
		}
		nblk = n;
		pfd[n].fd = sigfd[0];
		pfd[n++].events = POLLIN;
		pfd[n].fd = infd; /* poll ignores a negative fd */
		pfd[n++].events = POLLIN;
		if (poll(pfd, n, wait > 0x7fffffff ? 0x7fffffff : (int)wait) < 0 &&
		    errno != EINTR)
			die("poll");
		for (i = 0; i < nblk; i++)
			if (pfd[i].revents)
				collect(map[i]);
		if (pfd[nblk].revents)
			readsignals();
		if (pfd[nblk + 1].revents)
			readclicks();
	}
}
