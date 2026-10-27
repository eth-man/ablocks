ablocks - a suckless status bar that sucks a little
====================================================
![Each block playing its animation in turn](showcase.gif)

I love suckless software. Small, simple, does one thing, gets out of the
way. So naturally I went and built a status bar with animations in it.

When I plug in the charger, the battery fills up. When the wifi drops,
the bars drain away. The speaker shakes when I turn the volume all the
way up, and the clock digits roll down like an old flip clock every
minute. None of this is necessary. All of it makes me smile.

By suckless standards, an animated bar sucks. I know. So I tried to make
it suck as little as possible: it is one small C file, you configure it
by editing config.h and recompiling, and it needs nothing but libc. Take
the animations out and you are left with a plain, boring replacement for
i3blocks that runs your existing block scripts as they are.


What you need
-------------
A C compiler, make and i3. Python 3 with fontforge if you want the icon
font that comes with it (you do, the icons are half the fun).


Installing
----------
    make clean install

or, just for yourself, without root:

    make PREFIX=$HOME/.local install

The first build creates config.h from config.def.h. That file is your
whole configuration: the blocks, how often they run, and the animations.
Change it, rebuild, restart i3. Yes, you recompile to change your bar.
It takes a second.

Then point i3bar at it in your i3 config:

    bar {
        status_command ablocks
    }


Blocks
------
A block is just a script that prints a line of text, exactly like with
i3blocks. Clicks and signals work the same way too, so if you already
have i3blocks scripts, they will run without changes.


Making a block move
-------------------
A script asks for an animation by printing one extra line, for example
when the battery runs low:

    if [ -n "$ABLOCKS" ] && [ "$capacity" -le 20 ]; then
        echo "@lowbat 0 $capacity%"
    fi

The animation itself lives in config.h as a short list of frames, each
one a bit of pango markup: a colour here, an icon there, something
nudged up or down. ablocks flips through them. Some animations play
once when something happens (the charger goes in), others keep going
while something is true (the battery is low).

Frames name their colours instead of hard-coding them: `%{alert}`, or
`%{cyan 45}` for a cyan faded 45% of the way up from the bar. The
actual values come from `~/.config/ablocks/colors`, a few lines of
`name #rrggbb` that ablocks reads again whenever it gets SIGUSR1. So if
your theme switcher writes that file and pokes the bar, the animations
change colour along with everything else.

The ABLOCKS check is there so the same script still works under plain
i3blocks, which would otherwise print the request on your bar.

Hold Shift and scroll on an animating block to speed it up or slow it
down, and it tells you the number to put in config.h.

Every time i3 restarts, a little wave runs across the bar from left to
right, catches its breath, and runs back. It serves no purpose
whatsoever. You can turn it off in config.h,
but I don't know why you would.

![The boot wave running across the bar and back](boot.gif)


The icons
---------
The bar comes with its own icon font, suckless icons. Every icon is a few
lines of Python drawing on the same small grid with the same stroke, so
they all look like they belong together. They live in a corner of
Unicode nobody else uses, so they never clash with your other fonts.

    python3 icons/icons.py list

shows them all.


A few things I learned the hard way
-----------------------------------
Moving things around in a status bar without the whole bar jittering is
harder than it looks. Most of the work went into making sure that when
one icon dances, its neighbours stay exactly where they are. If you
write your own animations, keep every frame the same width as the block
itself, and if your bar is see-through, fade things by changing their
colour, not their transparency. Trust me on that one.


Should you use it?
------------------
If you want a status bar that sucks less, use i3blocks. If you want your
battery to do a little dance when you plug it in, welcome.
