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
 * "@name [ms [arg]]" line, "%n" that argument with its first number
 * counting up from 0 over the burst, padded so the width never changes, and
 * "%1".."%9" the argument's tab-separated fields. rise moves text without changing its
 * width, so neighbouring blocks never shift. Fade with darker colours, not
 * alpha: on a transparent bar the compositor shows alpha text far brighter
 * than meant. Needs markup = "pango".
 *
 * ms     frame time; a script's "@name ms" or Shift+scroll overrides it
 * loops  plays of the frame list per burst
 * every  seconds from one burst to the next while the script keeps asking;
 *        0 plays once each time the request appears, Always never pauses */
static const Anim anims[] = {
	/* the whole block moved up by r. The bar centres a block by its
	 * height, which rise stretches, so on its own the block would not move
	 * at all: an invisible zero-width space sinks by as much to hold it. */
#define HOP(r, nr) "<span rise='" r "'>%s</span><span rise='" nr "'>\u200B</span>"
	/* name     ms   loops  every   frames */
	/* the one fade left in alpha: each block fading in as the wave
	 * reaches it, which on a transparent bar shows as a quick flash */
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
		"<span color='#DC143C'><span color='#550817'>\U0010003D</span> %a</span>",
		NULL } },
	/* CPU at or over its alert temperature: only the icon blinks, the
	 * temperature stays put, for as long as the script asks (Always), i.e.
	 * until it cools down. %a = "85C" */
	{ "cpuhot", 500, 1,     Always, {
		"<span color='#DC143C'>\U00100024 %a</span>",
		"<span color='#DC143C'><span color='#550817'>\U00100024</span> %a</span>",
		NULL } },
	/* brightness changed: the percentage shows beside the icon for 1.5s,
	 * then fades out and leaves the bare icon. Each new level restarts it.
	 * %1 = the icon, %2 = "61%" */
	/* a level's text fading out: colours stepping toward the bar, not
	 * alpha, which a transparent bar shows far brighter than meant */
#define LVFADE \
	"%1 <span color='#9da2ac'>%2</span>", \
	"%1 <span color='#616570'>%2</span>", \
	"%1 <span color='#30343d'>%2</span>"
	{ "brightshow", 100, 1, 0, {
		"%1 %2", "%1 %2", "%1 %2", "%1 %2", "%1 %2",
		"%1 %2", "%1 %2", "%1 %2", "%1 %2", "%1 %2",
		"%1 %2", "%1 %2", "%1 %2", "%1 %2", "%1 %2",
		LVFADE,
		NULL } },
	/* the volume changed: its percentage beside the icon for 1.5s, then a
	 * fade, like brightshow; volup brings it up from below, voldown down
	 * from above, and volmax, at 100% and over, shakes the speaker and
	 * shows both in amber. A zero-width space rising the other way keeps
	 * the block's height, so the bar does not bob it. %1 = the speaker
	 * icon, %2 = "65%" */
#define LVRISE(r, nr) "%1 <span color='#d8dee9' rise='" r "'>%2</span><span rise='" nr "'>\u200B</span>"
#define LVHOLD "%1 <span color='#d8dee9'>%2</span>"
#define LVHOLD4 LVHOLD, LVHOLD, LVHOLD, LVHOLD
	{ "volup",  100, 1,     0, {
		LVRISE("-5000", "5000"), LVRISE("-2500", "2500"), LVRISE("-800", "800"),
		LVHOLD4, LVHOLD4, LVHOLD4, LVFADE,
		NULL } },
	{ "voldown", 100, 1,    0, {
		LVRISE("5000", "-5000"), LVRISE("2500", "-2500"), LVRISE("800", "-800"),
		LVHOLD4, LVHOLD4, LVHOLD4, LVFADE,
		NULL } },
#define LVSHAKE(r, nr) "<span color='#e8cb61' rise='" r "'>%1</span><span rise='" nr "'>\u200B</span> <span color='#e8cb61'>%2</span>"
#define LVLOUD "<span color='#e8cb61'>%1 %2</span>"
	{ "volmax", 70,  1,     0, {
		LVSHAKE("1500", "-1500"), LVSHAKE("-1500", "1500"), LVSHAKE("1500", "-1500"),
		LVSHAKE("-1500", "1500"), LVSHAKE("1000", "-1000"), LVSHAKE("-1000", "1000"),
		LVSHAKE("500", "-500"), LVSHAKE("-500", "500"),
		LVLOUD, LVLOUD, LVLOUD, LVLOUD, LVLOUD, LVLOUD, LVLOUD, LVLOUD,
		LVLOUD, LVLOUD, LVLOUD, LVLOUD, LVLOUD, LVLOUD,
		"%1 <span color='#e8cb61'>%2</span>",
		"%1 <span color='#9da2ac'>%2</span>",
		"%1 <span color='#616570'>%2</span>",
		"%1 <span color='#30343d'>%2</span>",
		NULL } },
	/* muted: the speaker's waves drain away; unmuted: they fill back up
	 * and the level shows for a moment. The muted icon is grey, like the
	 * block's. %1 = the speaker icon as it ends, %2 = "65%" */
	{ "volmute", 110, 1,    0, {
		"<span color='#d8dee9'>\U00100015</span>",
		"<span color='#d8dee9'>\U00100014</span>",
		"<span color='#d8dee9'>\U00100013</span>",
		"<span color='#d8dee9'>\U00100012</span>",
		"<span color='#969993'>\U00100016</span>",
		NULL } },
	{ "volunmute", 110, 1,  0, {
		"<span color='#969993'>\U00100016</span>",
		"<span color='#d8dee9'>\U00100012</span>",
		"<span color='#d8dee9'>\U00100013</span>",
		"<span color='#d8dee9'>\U00100014</span>",
		"<span color='#d8dee9'>\U00100015</span>",
		LVHOLD4, LVHOLD4, LVHOLD4, LVFADE,
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
		"<span color='#bb86fc'><span color='#6f4f95'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'>\U0010000B %a</span>",
		"<span color='#bb86fc'><span color='#6f4f95'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'>\U0010000B %a</span>",
		NULL } },
	/* charger pulled: the bolt fades away, then the block's own icon
	 * rises back into place, once; the reverse of plugin. The block's
	 * colour is the script's, so this one moves and doesn't fade.
	 * %a = "59%" */
	{ "unplug", 180, 1,     0, {
		"<span color='#bb86fc'>\U0010000B %a</span>",
		"<span color='#bb86fc'><span color='#976ccb'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'><span color='#6f4f95'>\U0010000B</span> %a</span>",
		"<span color='#bb86fc'><span color='#412e57'>\U0010000B</span> %a</span>",
		HOP("-4000", "4000"),
		HOP("-1500", "1500"),
		HOP("500", "-500"),
		"%s",
		NULL } },
	/* the block blinks, three times. A frame can't recolour the script's
	 * own colours, so it passes its text without them. %a = the text */
	{ "blink",  400, 3,     0, {
		"%s",
		"<span color='#30343d'>%a</span>",
		NULL } },
	{ "bounce", 60,  2,     0, {
		"%s",
		HOP("3000", "-3000"),
		HOP("5000", "-5000"),
		HOP("3000", "-3000"),
		"%s",
		HOP("-1200", "1200"),
		"%s",
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
	{ "wifiscan", 110, 2,   0, {
		"<span color='#83a598'>\U0010000D</span>%a",
		"<span color='#83a598'>\U0010000E</span>%a",
		"<span color='#83a598'>\U0010000F</span>%a",
		"<span color='#83a598'>\U00100010</span>%a",
		NULL } },
	/* wifi bars filling up, green, once on reconnect, while the signal
	 * strength counts up to its value. %n = "92% " and the LAN icon */
	{ "wifiup", 140, 3,     0, {
		"<span color='#90EE90'>\U0010000D %n</span>",
		"<span color='#90EE90'>\U0010000E %n</span>",
		"<span color='#90EE90'>\U0010000F %n</span>",
		"<span color='#90EE90'>\U00100010 %n</span>",
		NULL } },
	/* the same without the percentage, when the block hides it. Its own
	 * animation because %n would count the first number it finds, and with
	 * no percentage that is one in the LAN icon's colour. %a = the LAN icon */
	{ "wifiupicon", 140, 3, 0, {
		"<span color='#90EE90'>\U0010000D</span>%a",
		"<span color='#90EE90'>\U0010000E</span>%a",
		"<span color='#90EE90'>\U0010000F</span>%a",
		"<span color='#90EE90'>\U00100010</span>%a",
		NULL } },
	/* network traffic past its speed threshold: the fast arrows stream in
	 * their direction, received down and sent up, in the speed's colour,
	 * for 1.4s; then each shows its rate beside it in that colour, counting
	 * up, held for 1s, over and over while it stays fast; nettrafmax is the
	 * same for very fast. The script passes four tab-separated fields per
	 * arrow, received then sent: the arrow with its text if it keeps still,
	 * the arrow alone if it moves (else a zero-width space), the arrow with
	 * its rate for the count, and the text beside it while it moves. A
	 * zero-width space rising the other way keeps the block's height, so
	 * the bar does not bob it; it comes after the text, which as the
	 * arrow's neighbour keeps the space it has while the arrow is still. */
#define NTMOVE(c, r, nr) \
	"%1<span color='" c "' rise='" r "'>%2</span>%4<span rise='" nr "'>\u200B</span>" \
	"%5<span color='" c "' rise='" nr "'>%6</span>%8<span rise='" r "'>\u200B</span>"
#define NTCOUNT(c, p) \
	"%1<span color='" c "'>%{3 " p "}</span>%5<span color='" c "'>%{7 " p "}</span>"
#define NTFRAMES(c, c85, c55, c20) { \
	NTMOVE(c20, "6000", "-6000"), \
	NTMOVE(c55, "4000", "-4000"), \
	NTMOVE(c85, "2000", "-2000"), \
	NTMOVE(c, "0", "0"), \
	NTMOVE(c85, "-2000", "2000"), \
	NTMOVE(c55, "-4000", "4000"), \
	NTMOVE(c20, "-6000", "6000"), \
	NTMOVE(c20, "6000", "-6000"), \
	NTMOVE(c55, "4000", "-4000"), \
	NTMOVE(c85, "2000", "-2000"), \
	NTMOVE(c, "0", "0"), \
	NTMOVE(c85, "-2000", "2000"), \
	NTMOVE(c55, "-4000", "4000"), \
	NTMOVE(c20, "-6000", "6000"), \
	NTMOVE(c20, "6000", "-6000"), \
	NTMOVE(c55, "4000", "-4000"), \
	NTMOVE(c85, "2000", "-2000"), \
	NTMOVE(c, "0", "0"), \
	NTCOUNT(c, "0"), NTCOUNT(c, "40"), NTCOUNT(c, "70"), NTCOUNT(c, "88"), NTCOUNT(c, "97"), NTCOUNT(c, "100"), \
	NTCOUNT(c, "100"), NTCOUNT(c, "100"), NTCOUNT(c, "100"), NTCOUNT(c, "100"), \
	NTCOUNT(c, "100"), NTCOUNT(c, "100"), NTCOUNT(c, "100"), NTCOUNT(c, "100"), \
	NTCOUNT(c, "100"), NTCOUNT(c, "100"), NTCOUNT(c, "100"), NTCOUNT(c, "100"), \
	NULL }
	{ "nettraf",    80, 1,  Always, NTFRAMES("#83a598", "#79988c", "#617a71", "#3b4a44") },
	{ "nettrafmax", 80, 1,  Always, NTFRAMES("#e8cb61", "#d6bb59", "#ac9748", "#685b2b") },
	/* the same for a block hiding its rates, kept to its two arrows: after
	 * the motion the rate, field 9, covers both of them, counting up.
	 * letter_spacing stretches its 4 characters (44px at mono 14) to the
	 * two arrows and the space between them (45px), so nothing moves. */
#define NTMINI(c, c85, c55, c20) { \
	NTMOVE(c20, "6000", "-6000"), \
	NTMOVE(c55, "4000", "-4000"), \
	NTMOVE(c85, "2000", "-2000"), \
	NTMOVE(c, "0", "0"), \
	NTMOVE(c85, "-2000", "2000"), \
	NTMOVE(c55, "-4000", "4000"), \
	NTMOVE(c20, "-6000", "6000"), \
	NTMOVE(c20, "6000", "-6000"), \
	NTMOVE(c55, "4000", "-4000"), \
	NTMOVE(c85, "2000", "-2000"), \
	NTMOVE(c, "0", "0"), \
	NTMOVE(c85, "-2000", "2000"), \
	NTMOVE(c55, "-4000", "4000"), \
	NTMOVE(c20, "-6000", "6000"), \
	NTMOVE(c20, "6000", "-6000"), \
	NTMOVE(c55, "4000", "-4000"), \
	NTMOVE(c85, "2000", "-2000"), \
	NTMOVE(c, "0", "0"), \
	NTNUM(c, "0"), NTNUM(c, "40"), NTNUM(c, "70"), \
	NTNUM(c, "88"), NTNUM(c, "97"), NTNUM(c, "100"), \
	NTNUM(c, "100"), NTNUM(c, "100"), NTNUM(c, "100"), NTNUM(c, "100"), \
	NTNUM(c, "100"), NTNUM(c, "100"), NTNUM(c, "100"), NTNUM(c, "100"), \
	NTNUM(c, "100"), NTNUM(c, "100"), NTNUM(c, "100"), NTNUM(c, "100"), \
	NULL }
#define NTNUM(c, p) "<span color='" c "' letter_spacing='342'>%{9 " p "}</span>"
	{ "nettrafmini",    80, 1, Always, NTMINI("#83a598", "#79988c", "#617a71", "#3b4a44") },
	{ "nettrafminimax", 80, 1, Always, NTMINI("#e8cb61", "#d6bb59", "#ac9748", "#685b2b") },
	/* a weather threshold crossed: only the icons over it move, the drop
	 * falling twice, the snowflake drifting down and the sun pulsing, for
	 * 2.2s; then each is replaced by its number, counting up from 0, fast
	 * and slowing, for 0.7s, held for 1s, and the icons come back. Every five
	 * minutes until a middle click. The script passes three tab-separated
	 * fields per icon, rain, cold and heat in order: the icon if it keeps
	 * still, the icon if it moves, and its number, each with the space after
	 * it: a space between two spans takes the text font's wider space, not
	 * the icon font's, and would push the icons apart as the motion starts. */
#define WXMOVE(r, c, h) \
	"%1<span " r ">%2</span>" \
	"%4<span " c ">%5</span>" \
	"%7<span " h ">%8</span>"
#define WXCOUNT(p) \
	"%1<span color='#83a598'>%{3 " p "}</span>" \
	"%4<span color='#83a598'>%{6 " p "}</span>" \
	"%7<span color='#e8cb61'>%{9 " p "}</span>"
#define WXHOLD WXCOUNT("100")
	{ "weather", 90,  1,    300, {
		WXMOVE("color='#3b4a44' rise='6000'", "color='#33403b' rise='6000'", "color='#e8cb61'"),
		WXMOVE("color='#485a53' rise='5900'", "color='#3f4f49' rise='5600'", "color='#e2c65f'"),
		WXMOVE("color='#536860' rise='5600'", "color='#495c55' rise='5200'", "color='#d0b657'"),
		WXMOVE("color='#5d756b' rise='5200'", "color='#52675f' rise='4800'", "color='#b49d4b'"),
		WXMOVE("color='#658076' rise='4500'", "color='#5b7269' rise='4400'", "color='#93803d'"),
		WXMOVE("color='#6e8a7f' rise='3700'", "color='#627b72' rise='4100'", "color='#746630'"),
		WXMOVE("color='#759488' rise='2600'", "color='#69847a' rise='3700'", "color='#685b2b'"),
		WXMOVE("color='#7c9d90' rise='1400'", "color='#6f8c81' rise='3400'", "color='#746630'"),
		WXMOVE("color='#83a598' rise='0'", "color='#759488' rise='3000'", "color='#93803d'"),
		WXMOVE("color='#83a598' rise='-1500'", "color='#7b9b8f' rise='2700'", "color='#b49d4b'"),
		WXMOVE("color='#83a598' rise='-600'", "color='#80a295' rise='2400'", "color='#d0b657'"),
		WXMOVE("color='#83a598' rise='0'", "color='#83a598' rise='2100'", "color='#e2c65f'"),
		WXMOVE("color='#3b4a44' rise='6000'", "color='#83a598' rise='1800'", "color='#e8cb61'"),
		WXMOVE("color='#485a53' rise='5900'", "color='#83a598' rise='1600'", "color='#e2c65f'"),
		WXMOVE("color='#536860' rise='5600'", "color='#83a598' rise='1300'", "color='#d0b657'"),
		WXMOVE("color='#5d756b' rise='5200'", "color='#83a598' rise='1100'", "color='#b49d4b'"),
		WXMOVE("color='#658076' rise='4500'", "color='#83a598' rise='900'", "color='#93803d'"),
		WXMOVE("color='#6e8a7f' rise='3700'", "color='#83a598' rise='700'", "color='#746630'"),
		WXMOVE("color='#759488' rise='2600'", "color='#83a598' rise='500'", "color='#685b2b'"),
		WXMOVE("color='#7c9d90' rise='1400'", "color='#83a598' rise='400'", "color='#746630'"),
		WXMOVE("color='#83a598' rise='0'", "color='#83a598' rise='200'", "color='#93803d'"),
		WXMOVE("color='#83a598' rise='-1500'", "color='#83a598' rise='100'", "color='#b49d4b'"),
		WXMOVE("color='#83a598' rise='-600'", "color='#83a598' rise='0'", "color='#d0b657'"),
		WXMOVE("color='#83a598' rise='0'", "color='#83a598' rise='0'", "color='#e2c65f'"),
		WXCOUNT("0"), WXCOUNT("37"), WXCOUNT("64"), WXCOUNT("82"),
		WXCOUNT("92"), WXCOUNT("97"), WXCOUNT("99"), WXCOUNT("100"),
		WXHOLD, WXHOLD, WXHOLD, WXHOLD, WXHOLD, WXHOLD, WXHOLD,
		WXHOLD, WXHOLD, WXHOLD, WXHOLD, WXHOLD,
		NULL } },
	/* every number for 2.5s, once: a middle click with no alert showing.
	 * %a = the block with its numbers */
	{ "weathershow", 250, 1, 0, {
		"%a", "%a", "%a", "%a", "%a", "%a", "%a", "%a", "%a", "%a",
		NULL } },
	/* a flip clock: the digits that changed drop out, the new ones fall in
	 * from above and settle. The bar centres a block by its height, which
	 * rise stretches, so an invisible zero-width space rises the other way
	 * to keep the block still. %1 = colour, %2 = icon and date, %3 = the
	 * digits both times share, %4 = the old rest, %5 = the new rest. The
	 * moving digits fade through darker shades of the text colour. */
#define FLIP(d, r, counter, c) "<span color='%1'>%2<b>%3<span rise='" r "' color='" c "'>" d "</span></b></span><span rise='" counter "'>\u200B</span>"
	{ "clockflip", 45, 1,   0, {
		FLIP("%4", "-1200", "1200", "#c7cdd7"),
		FLIP("%4", "-3000", "3000", "#a7acb4"),
		FLIP("%4", "-5500", "5500", "#80838a"),
		FLIP("%4", "-8000", "8000", "#44464a"),
		FLIP("%5", "8000", "-8000", "#44464a"),
		FLIP("%5", "5500", "-5500", "#80838a"),
		FLIP("%5", "3000", "-3000", "#a7acb4"),
		FLIP("%5", "1200", "-1200", "#c7cdd7"),
		FLIP("%5", "0", "0", "%1"),
		FLIP("%5", "-900", "900", "%1"),
		FLIP("%5", "-300", "300", "%1"),
		FLIP("%5", "0", "0", "%1"),
		NULL } },
};

/* played across the bar each time ablocks starts, i.e. on every i3 restart;
 * `pkill -USR2 -x ablocks` replays it. NULL to disable. */
static const char *bootanim = "boot";
static const unsigned int bootdelay = 700;  /* ms for the blocks to print first */
static const unsigned int bootstagger = 80; /* ms between neighbouring blocks */
static const int bootreturn = 1;            /* and back again from the last */
static const unsigned int bootpause = 500;  /* ms at the last block before that */

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
	{ "clock",              SB "clock",                         1,        0,      0 },
	{ "gitcheck",           SB "gitcheck",                      300,      14,     0 },
	{ "sb-bluetooth",       SB "sb-bluetooth",                  5,        17,     0 },
	{ "nettraf",            SB "nettraf",                       2,        0,      0 },
	{ "ping-lat",           SB "ping-lat",                      60,       18,     0 },
	{ "vpn",                SB "vpn",                           60,       3,      0 },
	{ "internet",           SB "internet",                      10,       20,     0 },
	{ "i3-keyboardlayout",  SB "i3-keyboard-layout i3status",   10,       30,     0 },
};
