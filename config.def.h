/* See LICENSE file for copyright and license details. */

/* shell that runs each block's command */
static const char shell[] = "/bin/sh";

/* "pango" or "none"; applied to every block */
static const char markup[] = "pango";

/* pixels between blocks; i3bar's default clips the rightmost block */
static const int sepwidth = 8;

/* Animations: pango frames wrapped around a block's output, "%s" standing for
 * the output. alpha fades without touching the script's own colours, and
 * rise moves the text vertically without changing its width, so neighbouring
 * blocks never shift. Needs markup = "pango". */
static const Anim anims[] = {
	/* name     ms per frame, frames */
	{ "boot",   40, {
		"<span alpha='1%' rise='-4000'>%s</span>",   /* held until its turn */
		"<span alpha='25%' rise='-2500'>%s</span>",
		"<span alpha='55%' rise='0'>%s</span>",
		"<span alpha='80%' rise='2500'>%s</span>",
		"<span rise='1000'>%s</span>",
		"<span rise='-800'>%s</span>",
		"<span rise='0'>%s</span>",
		NULL } },
};

/* played across the bar each time ablocks starts, i.e. on every i3 restart;
 * `pkill -USR2 -x ablocks` replays it. NULL to disable. */
static const char *bootanim = "boot";
static const unsigned int bootdelay = 700;  /* ms for the blocks to print first */
static const unsigned int bootstagger = 80; /* ms between neighbouring blocks */

/* where the block scripts live, like i3blocks' command=~/.local/bin/statusbar/$BLOCK_NAME */
#define SB "$HOME/.local/bin/statusbar/"

static const Block blocks[] = {
	/* name                 command                             interval  signal  separator */
	{ "failed",             SB "failed",                        60,       15,     1 },
	{ "record",             "cat /tmp/recordingicon",           Once,     9,      1 },
	{ "spotifyControls",    SB "spotifyControls",               10,       19,     0 },
	{ "spotifyPlay",        SB "spotifyPlay",                   1,        0,      1 },
	{ "screen-timeout",     SB "screen-timeout",                10,       0,      1 },
	{ "music",              SB "music",                         Once,     11,     1 },
	{ "news",               SB "news",                          Once,     6,      1 },
	{ "pacpackages",        SB "pacpackages",                   Once,     8,      1 },
	{ "tasks",              SB "tasks",                         10,       16,     1 },
	{ "torrent",            SB "torrent",                       20,       7,      1 },
	{ "weather",            SB "weather",                       18000,    5,      1 },
	{ "mailbox",            SB "mailbox",                       180,      12,     1 },
	{ "cpu",                SB "cpu",                           15,       0,      1 },
	{ "volume",             SB "volume",                        Once,     10,     1 },
	{ "brightness",         SB "brightness",                    5,        13,     1 },
	{ "battery",            SB "battery BAT0",                  5,        0,      1 },
	{ "clock",              SB "clock",                         30,       0,      1 },
	{ "gitcheck",           SB "gitcheck",                      300,      14,     1 },
	{ "sb-bluetooth",       SB "sb-bluetooth",                  5,        17,     1 },
	{ "nettraf",            SB "nettraf",                       2,        0,      1 },
	{ "ping-lat",           SB "ping-lat",                      60,       18,     1 },
	{ "vpn",                SB "vpn",                           60,       3,      1 },
	{ "internet",           SB "internet",                      10,       0,      1 },
	{ "i3-keyboardlayout",  SB "i3-keyboard-layout i3status",   10,       30,     1 },
};
