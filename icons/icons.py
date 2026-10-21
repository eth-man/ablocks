#!/usr/bin/env python3
# See LICENSE file for copyright and license details.
#
# suckless icons: every status bar icon as a few lines of drawing code.
#
# One 24-unit grid, y pointing down, one stroke width, round caps and
# joins; outlines by default, fills only where the fill carries meaning
# (battery charge, transport controls). Shapes are strokes along centre
# lines, arcs are exact cubic Beziers; fontforge expands the strokes into
# outlines when the font is built.
#
#   python3 icons.py build OUT.otf    font, glyphs from U+100000 in order
#   python3 icons.py list             codepoint and name of every glyph
#   python3 icons.py sh               the same as shell variables, I_NAME

import math
import sys

STROKE = 2.0        # grid units: about the stem weight of the text beside it
UNIT = 48           # font units per grid unit
CENTRE = 365        # font y of grid y 12: middle of the digits' cap height
BASE = 0x100000     # Supplementary Private Use Area-B: used by no other font
PAD = 1.2           # grid units of side bearing


def arcops(cx, cy, r, a0, a1):
	"""An arc as cubic Beziers, one per quarter turn at most. Degrees,
	0 = right, 90 = down; a1 < a0 runs the other way."""
	n = max(1, math.ceil(abs(a1 - a0) / 90))
	pt = lambda a: (cx + r * math.cos(a), cy + r * math.sin(a))
	ops = [("M", pt(math.radians(a0)))]
	for i in range(n):
		t0 = math.radians(a0 + (a1 - a0) * i / n)
		t1 = math.radians(a0 + (a1 - a0) * (i + 1) / n)
		k = 4 / 3 * math.tan((t1 - t0) / 4) * r
		p0, p1 = pt(t0), pt(t1)
		ops.append(("C", (p0[0] - k * math.sin(t0), p0[1] + k * math.cos(t0)),
		                 (p1[0] + k * math.sin(t1), p1[1] - k * math.cos(t1)), p1))
	return ops


def quadop(p0, p1, p2):
	"""A quadratic curve from p0 as the exact cubic op."""
	return ("C", (p0[0] + 2 / 3 * (p1[0] - p0[0]), p0[1] + 2 / 3 * (p1[1] - p0[1])),
	             (p2[0] + 2 / 3 * (p1[0] - p2[0]), p2[1] + 2 / 3 * (p1[1] - p2[1])), p2)


def lineops(pts):
	return [("M", pts[0])] + [("L", p) for p in pts[1:]]


def rrectops(x0, y0, x1, y1, r):
	r = min(r, (x1 - x0) / 2, (y1 - y0) / 2)
	ops = [("M", (x0 + r, y0)), ("L", (x1 - r, y0))]
	ops += arcops(x1 - r, y0 + r, r, -90, 0)[1:] + [("L", (x1, y1 - r))]
	ops += arcops(x1 - r, y1 - r, r, 0, 90)[1:] + [("L", (x0 + r, y1))]
	ops += arcops(x0 + r, y1 - r, r, 90, 180)[1:] + [("L", (x0, y0 + r))]
	ops += arcops(x0 + r, y0 + r, r, 180, 270)[1:]
	return ops


class Icon:
	def __init__(self):
		self.strokes = []  # (ops, closed)
		self.fills = []    # ops, always closed
		self.cuts = []     # an Icon whose shape is cut out of this one

	def ops(self, ops, closed=False):
		self.strokes.append((ops, closed)); return self

	def line(self, *p):
		return self.ops(lineops(p))

	def path(self, pts, closed=False):
		return self.ops(lineops(pts), closed)

	def arc(self, cx, cy, r, a0, a1):
		return self.ops(arcops(cx, cy, r, a0, a1))

	def circle(self, cx, cy, r):
		return self.ops(arcops(cx, cy, r, 0, 360), True)

	def rrect(self, x0, y0, x1, y1, r):
		return self.ops(rrectops(x0, y0, x1, y1, r), True)

	def dot(self, cx, cy, r):
		self.fills.append(arcops(cx, cy, r, 0, 360)); return self

	def frrect(self, x0, y0, x1, y1, r):
		self.fills.append(rrectops(x0, y0, x1, y1, r)); return self

	def cut(self, icon):
		"""Knock icon's shape out of this one, for a mark on a solid body."""
		self.cuts.append(icon); return self

	def solid(self, pts):
		"""Filled, with the stroke around it rounding the corners. The
		points are put in one winding first: the other way round, the
		fill comes out as a hole once merged with its stroke."""
		if sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1])) < 0:
			pts = pts[::-1]
		self.fills.append(lineops(pts))
		return self.path(pts, True)


# ---------------------------------------------------------------- shapes ---

def battery(level=None, bolt=False, alert=False):
	i = Icon().rrect(2, 7, 19, 17, 2.6).frrect(20.3, 10, 22, 14, 0.8)
	if level:
		i.frrect(4.4, 9.4, 4.4 + 12.2 * level / 10, 14.6, 0.9)
	if bolt:
		i.solid([(11.6, 8.8), (8.4, 12.6), (10.6, 12.6), (9.6, 15.2), (12.8, 11.4), (10.6, 11.4)])
	if alert:
		# As tall as the body allows: at bar size anything shorter is a blob
		i.line((10.5, 8.9), (10.5, 11.9)).dot(10.5, 14.6, 1.2)
	return i


def wifi(arcs, off=False):
	i = Icon().dot(12, 19, 1.5)
	for r in (5, 9.5, 14)[:arcs]:
		i.arc(12, 19.5, r, -138, -42)
	if off:
		i.line((4, 4), (20, 20))
	return i


def volume(waves, mute=False):
	i = Icon().solid([(3, 9.6), (6.6, 9.6), (11, 5.6), (11, 18.4), (6.6, 14.4), (3, 14.4)])
	for r in (3.6, 7, 10.4)[:waves]:
		i.arc(11, 12, r, -48, 48)
	if mute:
		i.line((15.5, 9.5), (20.5, 14.5)).line((20.5, 9.5), (15.5, 14.5))
	return i


def sun(ray):
	i = Icon().circle(12, 12, 4)
	for k in range(8):
		a = math.radians(k * 45)
		i.line((12 + 6.6 * math.cos(a), 12 + 6.6 * math.sin(a)),
		       (12 + (6.6 + ray) * math.cos(a), 12 + (6.6 + ray) * math.sin(a)))
	return i


def rune():
	return Icon().path([(7, 7.5), (16.5, 16.5), (12, 21), (12, 3), (16.5, 7.5), (7, 16.5)])


def shield():
	p0, c, p1 = (19.5, 11), (19.5, 18), (12, 21.2)
	return Icon().ops([("M", (12, 2.8)), ("L", (19.5, 5.8)), ("L", p0), quadop(p0, c, p1),
	                   quadop(p1, (4.5, 18), (4.5, 11)), ("L", (4.5, 5.8))], True)


def arrowhead(i, tip, angle, size=3.6):
	"""Two barbs back from tip; angle is the direction of travel."""
	for s in (-38, 38):
		a = math.radians(angle + 180 + s)
		i.line(tip, (tip[0] + size * math.cos(a), tip[1] + size * math.sin(a)))
	return i


def snowflake():
	i = Icon()
	for k in range(6):
		a = math.radians(90 + k * 60)
		i.line((12, 12), (12 + 9 * math.cos(a), 12 + 9 * math.sin(a)))
		mid = (12 + 5.8 * math.cos(a), 12 + 5.8 * math.sin(a))
		for s in (-42, 42):
			b = a + math.radians(s)
			i.line(mid, (mid[0] + 2.8 * math.cos(b), mid[1] + 2.8 * math.sin(b)))
	return i


def keyboard():
	i = Icon().rrect(2, 6, 22, 18, 2.6).line((8, 15), (16, 15))
	for x in (5.8, 9.4, 13, 16.6, 20.2 - 2):
		i.dot(x, 9.4, 0.95)
	for x in (7.6, 11.2, 14.8):
		i.dot(x, 12, 0.95)
	return i


ICONS = {}

for n in range(11):
	ICONS["battery-%d" % n] = battery(n)
ICONS["battery-charging"] = battery(bolt=True)
ICONS["battery-alert"] = battery(alert=True)
for n in range(4):
	ICONS["wifi-%d" % n] = wifi(n)
ICONS["wifi-off"] = wifi(3, off=True)
for n in range(4):
	ICONS["volume-%d" % n] = volume(n)
ICONS["volume-mute"] = volume(0, mute=True)
for n, ray in ((1, 1.2), (2, 2.3), (3, 3.4)):
	ICONS["brightness-%d" % n] = sun(ray)
ICONS["sun"] = sun(3.4)

ICONS["alert"] = (Icon().path([(12, 3.6), (21.4, 19.6), (2.6, 19.6)], True)
                  .line((12, 9.2), (12, 13.4)).dot(12, 16.4, 1.1))
ICONS["check"] = Icon().path([(5, 12.6), (9.8, 17.4), (19, 7)])
ICONS["lan"] = (Icon().rrect(9, 3, 15, 8.4, 1.2).line((12, 8.4), (12, 12))
                .path([(6, 15), (6, 12), (18, 12), (18, 15)])
                .rrect(3, 15, 9, 20.6, 1.2).rrect(15, 15, 21, 20.6, 1.2))
ICONS["shield"] = shield().path([(8.6, 12), (11, 14.4), (15.6, 9.6)])
ICONS["shield-off"] = shield().line((4, 3.5), (20, 20.5))
ICONS["gauge"] = Icon().arc(12, 14, 9, 150, 390).line((12, 14), (16, 9.6)).dot(12, 14, 1.4)
ICONS["drop"] = Icon().ops(arcops(12, 14.8, 6.2, -38, 218) + [("L", (12, 3))], True)
ICONS["snowflake"] = snowflake()
ICONS["clock"] = Icon().circle(12, 12, 9).path([(12, 7), (12, 12), (15.5, 14)])
ICONS["cpu"] = Icon().rrect(6, 6, 18, 18, 2.4).rrect(9.4, 9.4, 14.6, 14.6, 1)
for p in (9, 12, 15):
	ICONS["cpu"].line((p, 6), (p, 3.4)).line((p, 18), (p, 20.6)).line((6, p), (3.4, p)).line((18, p), (20.6, p))
ICONS["download"] = arrowhead(Icon().line((12, 3.6), (12, 15.4)), (12, 15.4), 90, 4.6).line((5, 20.4), (19, 20.4))
ICONS["upload"] = arrowhead(Icon().line((12, 15.4), (12, 3.6)), (12, 3.6), -90, 4.6).line((5, 20.4), (19, 20.4))
ICONS["seed"] = arrowhead(Icon().circle(12, 12, 9).line((12, 16.4), (12, 7.6)), (12, 7.6), -90, 3.4)
ICONS["git"] = (Icon().circle(7, 5.6, 2.2).circle(7, 18.4, 2.2).circle(17, 7.6, 2.2)
                .line((7, 7.8), (7, 16.2)).ops([("M", (17, 9.8)), quadop((17, 9.8), (17, 15), (8.6, 16.6))]))
ICONS["package"] = (Icon().path([(12, 2.8), (20.2, 7.4), (20.2, 16.6), (12, 21.2), (3.8, 16.6), (3.8, 7.4)], True)
                    .path([(3.8, 7.4), (12, 12), (20.2, 7.4)]).line((12, 12), (12, 21.2)))
ICONS["rss"] = Icon().dot(5.8, 18.2, 1.9).arc(5, 19, 7.4, -90, 0).arc(5, 19, 13.4, -90, 0)
ICONS["stopwatch"] = (Icon().circle(12, 13.5, 8).line((12, 13.5), (12, 9.5))
                      .line((10, 3.2), (14, 3.2)).line((12, 3.2), (12, 5.5))
                      .line((18, 7.5), (19.6, 5.9)))
ICONS["hourglass"] = (Icon().line((6, 3.4), (18, 3.4)).line((6, 20.6), (18, 20.6))
                      .path([(7.4, 3.4), (7.4, 6.4), (12, 12), (16.6, 6.4), (16.6, 3.4)])
                      .path([(7.4, 20.6), (7.4, 17.6), (12, 12), (16.6, 17.6), (16.6, 20.6)]))
ICONS["mail"] = Icon().rrect(3, 5.4, 21, 18.6, 2.6).path([(4, 7.2), (12, 13), (20, 7.2)])
_end = (12 + 8 * math.cos(math.radians(300)), 12 + 8 * math.sin(math.radians(300)))
ICONS["refresh"] = arrowhead(Icon().arc(12, 12, 8, 30, 300), _end, 30, 3.4)
ICONS["note"] = (Icon().line((14.4, 16.6), (14.4, 3.6)).path([(14.4, 3.6), (19, 6), (19, 9.2)])
                 .dot(11.4, 16.8, 3.0))
ICONS["play"] = Icon().solid([(7.4, 4.6), (18.8, 12), (7.4, 19.4)])
ICONS["pause"] = Icon().frrect(5.8, 4.6, 10, 19.4, 1.4).frrect(14, 4.6, 18.2, 19.4, 1.4)
ICONS["stop"] = Icon().frrect(5.6, 5.6, 18.4, 18.4, 2.4)
ICONS["keyboard"] = keyboard()
ICONS["mouse"] = Icon().rrect(7, 3, 17, 21, 5).line((12, 6.6), (12, 9.6))
ICONS["headphones"] = (Icon().arc(12, 13, 8.4, 180, 360)
                       .frrect(2.6, 13, 7, 20.4, 1.8).frrect(17, 13, 21.4, 20.4, 1.8))
ICONS["phone"] = Icon().rrect(7, 2.6, 17, 21.4, 2.6).line((10.6, 18.4), (13.4, 18.4))
ICONS["bluetooth"] = rune()
ICONS["bluetooth-off"] = rune().line((4, 4), (20, 20))
ICONS["bluetooth-connected"] = rune().dot(3.6, 12, 1.2).dot(20.4, 12, 1.2)
ICONS["bluetooth-scan"] = rune().arc(12, 12, 9.4, -36, 36)

# Network traffic: circled, so a rate never reads as a file transfer or
# as git's arrows. Appended, as every glyph is, to keep codepoints stable.
ICONS["rx"] = arrowhead(Icon().circle(12, 12, 9.2).line((12, 7.2), (12, 16.8)), (12, 16.8), 90, 3.8)
ICONS["tx"] = arrowhead(Icon().circle(12, 12, 9.2).line((12, 16.8), (12, 7.2)), (12, 7.2), -90, 3.8)

# The same body solid, the mark cut out of it: reads at bar size, where
# an outlined "!" inside a 15px battery is two pixels.
ICONS["battery-critical"] = (Icon().rrect(2, 7, 19, 17, 2.6).frrect(2, 7, 19, 17, 2.6)
                             .frrect(20.3, 10, 22, 14, 0.8)
                             .cut(Icon().line((10.5, 9.3), (10.5, 11.9)).dot(10.5, 14.5, 1.2)))

# The day of the week as its place in the week: today a tall bar, the
# other days short ticks. One glyph per place, week-1 = the first day.
def week(day):
	i = Icon()
	for k in range(1, 8):
		x = 2 + (k - 1) * 5
		i.line((x, 4.5), (x, 19.5)) if k == day else i.line((x, 10.5), (x, 13.5))
	return i


for n in range(1, 8):
	ICONS["week-%d" % n] = week(n)

# Families share one advance width, so animation frames never change width.
FAMILIES = ("battery-", "wifi-", "volume-", "brightness-", "bluetooth", "week-")


def family(name):
	for f in FAMILIES:
		if name.startswith(f):
			return f
	return name


def points(ops):
	for op in ops:
		yield from op[1:]


def bbox(icon):
	xs = [p[0] for o, _ in icon.strokes for p in points(o)]
	xs += [p[0] for o in icon.fills for p in points(o)]
	return min(xs) - STROKE / 2, max(xs) + STROKE / 2


def draw(pen, ops, closed, tf):
	for op in ops:
		if op[0] == "M":
			pen.moveTo(tf(op[1]))
		elif op[0] == "L":
			pen.lineTo(tf(op[1]))
		else:
			pen.curveTo(tf(op[1]), tf(op[2]), tf(op[3]))
	pen.closePath() if closed else pen.endPath()


def render(g, icon, tf):
	pen = g.glyphPen()
	for ops, closed in icon.strokes:
		draw(pen, ops, closed, tf)
	pen = None
	if icon.strokes:
		g.stroke("circular", STROKE * UNIT, "round", "round")
	pen = g.glyphPen(replace=False)
	for ops in icon.fills:
		draw(pen, ops, True, tf)
	pen = None
	g.removeOverlap()


def checkcodes():
	"""Refuse to build if an icon already in codepoints.txt would move:
	scripts carry the glyphs themselves, so a shifted codepoint silently
	turns into a different icon. New icons belong at the end."""
	import os
	path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "codepoints.txt")
	now = {name: BASE + k for k, name in enumerate(ICONS)}
	try:
		old = dict((l.split()[1], int(l.split()[0][2:], 16)) for l in open(path))
	except FileNotFoundError:
		old = {}
	moved = [n for n, c in old.items() if now.get(n) != c]
	if moved:
		sys.exit("icons.py: would move or drop %s; append new icons instead" % ", ".join(moved))
	with open(path, "w") as f:
		f.writelines("U+%06X %s\n" % (c, n) for n, c in now.items())


def build(out):
	checkcodes()
	import fontforge
	font = fontforge.font()
	font.encoding = "UnicodeFull"
	font.fontname = "suckless-icons"
	font.familyname = font.fullname = "suckless icons"
	font.copyright = "See LICENSE file for copyright and license details."
	font.em = 1000
	font.ascent, font.descent = 860, 140
	spans = {}
	for name, icon in ICONS.items():
		x0, x1 = bbox(icon)
		lo, hi = spans.get(family(name), (x0, x1))
		spans[family(name)] = (min(lo, x0), max(hi, x1))
	for k, (name, icon) in enumerate(ICONS.items()):
		lo, hi = spans[family(name)]
		tf = lambda p, dx=PAD - lo: ((p[0] + dx) * UNIT, CENTRE + (12 - p[1]) * UNIT)
		g = font.createChar(BASE + k, name)
		render(g, icon, tf)
		for c in icon.cuts:
			# Added running the other way, the cut's contours are holes once
			# overlaps are removed. (glyph.exclude() emptied the glyph.)
			tmp = font.createChar(-1, name + ".cut")
			render(tmp, c, tf)
			layer = g.foreground
			for contour in tmp.foreground:
				contour = contour.dup()
				if contour.isClockwise():
					contour.reverseDirection()
				layer += contour
			g.foreground = layer
			g.removeOverlap()
			font.removeGlyph(tmp)
		g.correctDirection()
		g.round()
		g.width = int(round((hi - lo + 2 * PAD) * UNIT))
	font.generate(out)


if __name__ == "__main__":
	cmd = sys.argv[1] if len(sys.argv) > 1 else "list"
	if cmd == "build":
		build(sys.argv[2])
	elif cmd == "list":
		for k, name in enumerate(ICONS):
			print("U+%06X %s" % (BASE + k, name))
	elif cmd == "sh":
		for k, name in enumerate(ICONS):
			print("I_%s='%s'" % (name.upper().replace("-", "_"), chr(BASE + k)))
