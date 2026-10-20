/* See LICENSE file for copyright and license details. */

/* shell that runs each block's command */
static const char shell[] = "/bin/sh";

/* "pango" or "none"; applied to every block */
static const char markup[] = "pango";

/* pixels between blocks. Drawn as plain space, no separator lines: even
 * gaps read calmer than a rule between every pair. */
static const int sepwidth = 24;

/* Animations: pango frames wrapped around a block's output. In a frame, "%s"
 * is the block's own output, "%a" the argument from the script's
 * "@name [ms [arg]]" line, and "%n" that argument with its first number
 * counting up from 0 over the burst, padded so the width never changes. alpha fades without touching the script's
 * colours and rise moves text without changing its width, so neighbouring
 * blocks never shift. Needs markup = "pango".
 *
 * ms     frame time; a script's "@name ms" or Shift+scroll overrides it
 * loops  plays of the frame list per burst
 * every  seconds from one burst to the next while the script keeps asking;
 *        0 plays once each time the request appears, Always never pauses */
static const Anim anims[] = {
	/* name     ms   loops  every   frames */
	{ "boot",   40,  1,     0, {
		"<span alpha='1%' rise='-4000'>%s</span>",   /* held until its turn */
		"<span alpha='25%' rise='-2500'>%s</span>",
		"<span alpha='55%' rise='0'>%s</span>",
		"<span alpha='80%' rise='2500'>%s</span>",
		"<span rise='1000'>%s</span>",
		"<span rise='-800'>%s</span>",
		"<span rise='0'>%s</span>",
		NULL } },
	/* a battery filling up, red: a plea for the charger. suckless icons
	 * levels, as the battery block uses, in 10% steps. %a = "18%" */
	{ "lowbat", 70,  3,     30, {
		"<span color='#DC143C'>\U00100000 %a</span>",
		"<span color='#DC143C'>\U00100001 %a</span>",
		"<span color='#DC143C'>\U00100002 %a</span>",
		"<span color='#DC143C'>\U00100003 %a</span>",
		"<span color='#DC143C'>\U00100004 %a</span>",
		"<span color='#DC143C'>\U00100005 %a</span>",
		"<span color='#DC143C'>\U00100006 %a</span>",
		"<span color='#DC143C'>\U00100007 %a</span>",
		"<span color='#DC143C'>\U00100008 %a</span>",
		"<span color='#DC143C'>\U00100009 %a</span>",
		"<span color='#DC143C'>\U0010000A %a</span>",
		NULL } },
	/* critical battery: only the icon blinks, the percentage stays put,
	 * for as long as the script asks (Always). %a = "9%" */
	{ "batcrit", 500, 1,    Always, {
		"<span color='#DC143C'>\U0010003D %a</span>",
		"<span color='#DC143C'><span alpha='15%'>\U0010003D</span> %a</span>",
		NULL } },
	/* charger plugged in: the battery fills, then the bolt pulses, in
	 * purple, once. %a = "59%" */
	{ "plugin", 180, 1,     0, {
		"<span color='#bb86fc'>\U00100000 %a</span>",
		"<span color='#bb86fc'>\U00100002 %a</span>",
		"<span color='#bb86fc'>\U00100004 %a</span>",
		"<span color='#bb86fc'>\U00100006 %a</span>",
		"<span color='#bb86fc'>\U00100008 %a</span>",
		"<span color='#bb86fc'>\U0010000A %a</span>",
		"<span color='#bb86fc'>\U0010000B %a</span>",
		"<span color='#bb86fc'><span alpha='35%'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'>\U0010000B %a</span>",
		"<span color='#bb86fc'><span alpha='35%'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'>\U0010000B %a</span>",
		NULL } },
	/* charger pulled: the bolt fades away, then the block's own icon
	 * fades back in, once; the reverse of plugin. %a = "59%" */
	{ "unplug", 180, 1,     0, {
		"<span color='#bb86fc'>\U0010000B %a</span>",
		"<span color='#bb86fc'><span alpha='65%'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'><span alpha='35%'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'><span alpha='12%'>\U0010000B</span> %a</span>",
		"<span alpha='30%'>%s</span>",
		"<span alpha='60%'>%s</span>",
		"<span alpha='85%'>%s</span>",
		NULL } },
	{ "blink",  400, 3,     0, {
		"%s",
		"<span alpha='15%'>%s</span>",
		NULL } },
	{ "pulse",  90,  2,     60, {
		"<span alpha='70%'>%s</span>",
		"<span alpha='40%'>%s</span>",
		"<span alpha='20%'>%s</span>",
		"<span alpha='40%'>%s</span>",
		"<span alpha='70%'>%s</span>",
		"%s",
		NULL } },
	{ "bounce", 60,  2,     0, {
		"<span rise='0'>%s</span>",
		"<span rise='3000'>%s</span>",
		"<span rise='5000'>%s</span>",
		"<span rise='3000'>%s</span>",
		"<span rise='0'>%s</span>",
		"<span rise='-1200'>%s</span>",
		"<span rise='0'>%s</span>",
		NULL } },
	/* wifi bars draining to off, red; repeats while the script keeps
	 * asking, i.e. while offline. %a = the rest of the block (LAN icon) */
	{ "wifidown", 140, 1,   30, {
		"<span color='#DC143C'>\U00100010</span>%a",
		"<span color='#DC143C'>\U0010000F</span>%a",
		"<span color='#DC143C'>\U0010000E</span>%a",
		"<span color='#DC143C'>\U0010000D</span>%a",
		"<span color='#DC143C'>\U00100011</span>%a",
		"<span color='#DC143C'>\U00100011</span>%a",
		NULL } },
	/* wifi bars sweeping, as if scanning: played on a click. Only the icon
	 * changes; %a = the rest of the block exactly as the script prints it */
	{ "wifiscan", 70, 2,    0, {
		"<span color='#83a598'>\U0010000D</span>%a",
		"<span color='#83a598'>\U0010000E</span>%a",
		"<span color='#83a598'>\U0010000F</span>%a",
		"<span color='#83a598'>\U00100010</span>%a",
		NULL } },
	/* wifi bars filling up, green, once on reconnect, while the signal
	 * strength counts up to its value. %n = "92% " and the LAN icon */
	{ "wifiup", 100, 3,     0, {
		"<span color='#90EE90'>\U0010000D %n</span>",
		"<span color='#90EE90'>\U0010000E %n</span>",
		"<span color='#90EE90'>\U0010000F %n</span>",
		"<span color='#90EE90'>\U00100010 %n</span>",
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
	{ "failed",             SB "failed",                        60,       15,     0 },
	{ "record",             "cat /tmp/recordingicon",           Once,     9,      0 },
	{ "spotifyControls",    SB "spotifyControls",               10,       19,     0 },
	{ "spotifyPlay",        SB "spotifyPlay",                   1,        0,      0 },
	{ "screen-timeout",     SB "screen-timeout",                10,       0,      0 },
	{ "music",              SB "music",                         Once,     11,     0 },
	{ "news",               SB "news",                          Once,     6,      0 },
	{ "pacpackages",        SB "pacpackages",                   Once,     8,      0 },
	{ "tasks",              SB "tasks",                         10,       16,     0 },
	{ "torrent",            SB "torrent",                       20,       7,      0 },
	{ "weather",            SB "weather",                       18000,    5,      0 },
	{ "mailbox",            SB "mailbox",                       180,      12,     0 },
	{ "cpu",                SB "cpu",                           15,       0,      0 },
	{ "volume",             SB "volume",                        Once,     10,     0 },
	{ "brightness",         SB "brightness",                    5,        13,     0 },
	{ "battery",            SB "battery BAT0",                  5,        21,     0 },
	{ "clock",              SB "clock",                         30,       0,      0 },
	{ "gitcheck",           SB "gitcheck",                      300,      14,     0 },
	{ "sb-bluetooth",       SB "sb-bluetooth",                  5,        17,     0 },
	{ "nettraf",            SB "nettraf",                       2,        0,      0 },
	{ "ping-lat",           SB "ping-lat",                      60,       18,     0 },
	{ "vpn",                SB "vpn",                           60,       3,      0 },
	{ "internet",           SB "internet",                      10,       20,     0 },
	{ "i3-keyboardlayout",  SB "i3-keyboard-layout i3status",   10,       30,     0 },
};
