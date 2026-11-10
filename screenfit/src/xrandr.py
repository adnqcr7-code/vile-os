"""xrandr helpers for ScreenFit.

Pure-Python parsing of `xrandr` output plus the transform math used to
compensate for TV overscan. This module has no GTK dependency, so it can
be unit-tested without a display.

Overscan background
-------------------
Many TVs crop a few percent of the picture on every edge (overscan), so
the bottom panel / taskbar disappears. The fix is to shrink the image
slightly and centre it, using xrandr's --transform. For a screen of
width W and height H with horizontal shrink factor sx and vertical
shrink factor sy (1.0 = no shrink, 0.95 = a 5% shrink), the transform is:

    xrandr --output <display> --transform sx,0,W*(1-sx)/2,0,sy,H*(1-sy)/2,0,0,1

For example, 1920x1080 at 0.95/0.95 gives offsets 48 (horizontal) and 27
(vertical):  --transform 0.95,0,48,0,0.95,27,0,0,1
Like Intel Graphics Command Center, horizontal and vertical shrink are
independent, so a TV that crops more on one axis can be fixed per-axis.
"""

import os
import re
import subprocess

XRANDR = "xrandr"
AUTOSTART_NAME = "screenfit-overscan.desktop"

# e.g. "HDMI-1 connected primary 1920x1080+0+0 (...)"
_HEADER_RE = re.compile(r"^(\S+)\s+(connected|disconnected)\b")
# e.g. "   1920x1080      60.00*   59.94    30.00"
_MODE_RE = re.compile(r"^\s+(\d+)x(\d+)(i?)\s+(.+)$")
_RATE_RE = re.compile(r"^([\d.]+)(\*?)(\+?)$")


class Mode:
    """One display mode (resolution + refresh rates) from xrandr."""

    def __init__(self, width, height, rates, current=False, preferred=False,
                 interlaced=False):
        self.width = width
        self.height = height
        self.rates = rates  # list[float], in the order xrandr printed them
        self.current = current
        self.preferred = preferred
        self.interlaced = interlaced

    @property
    def name(self):
        """Mode name as xrandr expects it, e.g. '1920x1080' or '1920x1080i'."""
        suffix = "i" if self.interlaced else ""
        return "{0}x{1}{2}".format(self.width, self.height, suffix)

    @property
    def refresh(self):
        """First listed refresh rate, or None."""
        return self.rates[0] if self.rates else None

    def __repr__(self):
        return "Mode({0}, rates={1}, current={2})".format(
            self.name, self.rates, self.current)


class Display:
    """One output from xrandr (connected or disconnected)."""

    def __init__(self, name, connected, primary=False):
        self.name = name
        self.connected = connected
        self.primary = primary
        self.current = None      # (width, height) tuple, or None
        self.refresh = None      # float, current refresh rate, or None
        self.physical_mm = None  # (width_mm, height_mm) tuple, or None
        self.modes = []          # list[Mode]

    def current_mode(self):
        """The Mode matching the current resolution, or None."""
        if self.current is None:
            return None
        for mode in self.modes:
            if (mode.width, mode.height) == self.current and not mode.interlaced:
                return mode
        for mode in self.modes:
            if (mode.width, mode.height) == self.current:
                return mode
        return None

    def __repr__(self):
        return "Display({0}, connected={1}, current={2})".format(
            self.name, self.connected, self.current)


def parse_xrandr_output(text):
    """Parse the output of `xrandr` (no args) into a list of Display."""
    displays = []
    current = None
    for line in text.splitlines():
        header = _HEADER_RE.match(line)
        if header:
            name, state = header.group(1), header.group(2)
            current = Display(name, state == "connected",
                              primary=(" primary " in line))
            if current.connected:
                res = re.search(r"(\d+)x(\d+)\+(\d+)\+(\d+)", line)
                if res:
                    current.current = (int(res.group(1)), int(res.group(2)))
                phys = re.search(r"(\d+)mm\s+x\s+(\d+)mm", line)
                if phys:
                    current.physical_mm = (int(phys.group(1)),
                                           int(phys.group(2)))
            displays.append(current)
            continue

        mode_match = _MODE_RE.match(line)
        if mode_match and current is not None and current.connected:
            width = int(mode_match.group(1))
            height = int(mode_match.group(2))
            interlaced = bool(mode_match.group(3))
            rates = []
            current_rate = None
            is_current = False
            is_preferred = False
            for token in mode_match.group(4).split():
                rate_match = _RATE_RE.match(token)
                if not rate_match:
                    continue
                rate = float(rate_match.group(1))
                rates.append(rate)
                if rate_match.group(2):  # '*' marks the current rate
                    is_current = True
                    current_rate = rate
                if rate_match.group(3):  # '+' marks the preferred mode
                    is_preferred = True
            mode = Mode(width, height, rates, current=is_current,
                        preferred=is_preferred, interlaced=interlaced)
            current.modes.append(mode)
            if is_current:
                current.refresh = current_rate

    # Fallback: if no mode carried a '*', use the current mode's first rate.
    for display in displays:
        if display.connected and display.refresh is None:
            mode = display.current_mode()
            if mode is not None:
                display.refresh = mode.refresh
    return displays


def percent_to_scale(percent):
    """Shrink percent -> scale factor: +5% -> 0.95, 0% -> 1.0,
    -10% (expand) -> 1.10."""
    return 1.0 - percent / 100.0


def preview_rect(box_width, box_height, scale_x, scale_y):
    """(x, y, width, height) of the preview picture inside a box of the
    given size, scaled by scale_x/scale_y and centred. Negative x/y
    mean the picture is larger than the box (zoomed in) and its edges
    are clipped, exactly like an expanded picture on the real display."""
    width = round(box_width * scale_x, 2)
    height = round(box_height * scale_y, 2)
    x = round((box_width - width) / 2.0, 2)
    y = round((box_height - height) / 2.0, 2)
    return (x, y, width, height)


def fit_box(box_width, box_height, res_width, res_height):
    """(x, y, width, height) of the largest box with the aspect ratio
    of a resolution (res_width x res_height) that fits inside an
    allocation of box_width x box_height, centred. This is the
    mini-screen in the preview: it always has the real screen's
    aspect ratio, whatever size the preview widget is."""
    res_aspect = res_width / float(res_height)
    box_aspect = box_width / float(box_height)
    if box_aspect > res_aspect:
        # Allocation is wider than the screen: height is the limit.
        height = round(float(box_height), 2)
        width = round(box_height * res_aspect, 2)
        x = round((box_width - width) / 2.0, 2)
        y = 0.0
    else:
        # Allocation is taller than (or exactly) the screen: width
        # is the limit.
        width = round(float(box_width), 2)
        height = round(box_width / res_aspect, 2)
        x = 0.0
        y = round((box_height - height) / 2.0, 2)
    return (x, y, width, height)


def scaled_rect(box_x, box_y, box_width, box_height, scale_x, scale_y):
    """(x, y, width, height) of the mini-desktop inside a box, scaled
    by scale_x/scale_y and centred on the box. Same centring as
    preview_rect, but relative to the box's top-left corner, so the
    result is in the preview widget's coordinates."""
    x, y, width, height = preview_rect(box_width, box_height,
                                       scale_x, scale_y)
    return (round(box_x + x, 2), round(box_y + y, 2), width, height)


def panel_rect(desktop_x, desktop_y, desktop_w, desktop_h, res_height,
               panel_px=40):
    """(x, y, width, height) of the taskbar/panel at the bottom of a
    mini-desktop. A real Cinnamon panel is about panel_px pixels tall
    on a screen res_height pixels high, so in the preview the panel
    takes the same fraction (panel_px / res_height) of the
    mini-desktop's height. The panel scales with the desktop, so when
    the desktop is shrunk the panel shrinks too, and when it is
    expanded the panel can be clipped off the bottom - which is
    exactly what the preview is for: seeing whether the taskbar
    will be visible."""
    height = round(desktop_h * panel_px / float(res_height), 2)
    return (round(desktop_x, 2),
            round(desktop_y + desktop_h - height, 2),
            round(desktop_w, 2), height)


def format_number(value):
    """Format a number for xrandr: whole numbers without a decimal point."""
    if abs(value - round(value)) < 1e-9:
        return str(int(round(value)))
    text = "{:.2f}".format(value).rstrip("0").rstrip(".")
    return text


def compute_transform(width, height, scale, scale_y=None):
    """Return the 9 xrandr --transform values for a centred shrink.

    scale is the horizontal shrink (1.0 for no shrink, 0.95 for a 5%
    shrink). scale_y is the vertical shrink; when omitted it equals
    scale, giving a uniform shrink. The image is scaled and centred,
    leaving an equal black border on every side that the TV's overscan
    crops instead of the desktop. Offsets are rounded to 2 decimals
    (0.95 is not exact in binary floating point, so 1920*(1-0.95)/2
    would otherwise come out as 48.00000000000004 instead of 48).
    """
    if scale_y is None:
        scale_y = scale
    offset_x = round(width * (1.0 - scale) / 2.0, 2)
    offset_y = round(height * (1.0 - scale_y) / 2.0, 2)
    return (scale, 0, offset_x, 0, scale_y, offset_y, 0, 0, 1)


def transform_string(width, height, scale, scale_y=None):
    """The --transform argument, e.g. '0.95,0,48,0,0.95,27,0,0,1'."""
    return ",".join(format_number(v)
                    for v in compute_transform(width, height, scale,
                                               scale_y))


def build_transform_command(display_name, width, height, scale,
                            scale_y=None):
    """argv for: xrandr --output <display> --transform <values>"""
    return [XRANDR, "--output", display_name, "--transform",
            transform_string(width, height, scale, scale_y)]


def build_resolution_command(display_name, mode_name):
    """argv for: xrandr --output <display> --mode <WxH>"""
    return [XRANDR, "--output", display_name, "--mode", mode_name]


def build_reset_command(display_name):
    """argv that clears any transform (identity matrix)."""
    return [XRANDR, "--output", display_name, "--transform",
            "1,0,0,0,1,0,0,0,1"]


def run_command(argv):
    """Run a command. Returns (returncode, stdout, stderr)."""
    proc = subprocess.run(argv, capture_output=True, text=True)
    return proc.returncode, proc.stdout, proc.stderr


def get_xrandr_output():
    """Run `xrandr` and return its stdout.

    Raises FileNotFoundError if xrandr is not installed, RuntimeError
    if xrandr itself fails.
    """
    rc, out, err = run_command([XRANDR])
    if rc != 0:
        raise RuntimeError("xrandr failed: {0}".format(err.strip()))
    return out


def get_displays():
    """All outputs (connected and disconnected) from a live xrandr run."""
    return parse_xrandr_output(get_xrandr_output())


# ---------------------------------------------------------------------------
# Autostart (Cinnamon / freedesktop .desktop file in ~/.config/autostart/)
# ---------------------------------------------------------------------------

def autostart_dir():
    return os.path.join(os.path.expanduser("~"), ".config", "autostart")


def autostart_path():
    return os.path.join(autostart_dir(), AUTOSTART_NAME)


def autostart_exists():
    return os.path.exists(autostart_path())


def autostart_content(display_name, width, height, scale, scale_y=None):
    """The text of the autostart .desktop file for these settings."""
    command = " ".join(build_transform_command(display_name, width, height,
                                               scale, scale_y))
    return (
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=ScreenFit Overscan Fix\n"
        "Comment=Applies the saved overscan (fit to screen) transform at login.\n"
        "Exec={0}\n"
        "Hidden=false\n"
        "NoDisplay=true\n"
        "X-GNOME-Autostart-enabled=true\n"
    ).format(command)


def write_autostart(display_name, width, height, scale, scale_y=None):
    """Write the autostart file. Returns the path written."""
    os.makedirs(autostart_dir(), exist_ok=True)
    path = autostart_path()
    with open(path, "w") as handle:
        handle.write(autostart_content(display_name, width, height, scale,
                                       scale_y))
    return path


def remove_autostart():
    """Delete the autostart file if it exists."""
    path = autostart_path()
    if os.path.exists(path):
        os.remove(path)
