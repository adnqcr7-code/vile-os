#!/usr/bin/env python3
"""Vile Setup — the first-run wizard ("before-installation questions").

Adnan's direction (Nov 10, 2026): Vile should feel like a real premium
OS from the first boot — so before the bare desktop appears, the user
gets asked the setup questions: who they are, their picture, their
wallpaper, which apps to install, and their timezone.

How it is triggered (compositor, src/main.c):
  * On boot, if ~/.config/vile/setup-done does NOT exist, the
    compositor spawns this wizard INSTEAD of the startup command, so
    the wizard is the first (and only) window. VILE_NO_SETUP=1 skips
    it (headless tests). Once the marker exists, boot goes straight
    to the desktop. "Skip setup" and Finish both write the marker.
  * Re-run any time from Settings ("Run Setup again").

What it saves (all real files, no fake state):
  * Name      -> ~/.config/vile/display-name (the lock screen shows
               this instead of the system username when it exists)
  * Picture   -> ~/.config/vile/profile.png AND ~/.face, via the SAME
               set_profile_picture() Settings uses (imported, not copied)
  * Wallpaper -> ~/.config/vile/wallpaper (the compositor picks it up
               live within a second, or at next start)
  * Timezone  -> ~/.config/vile/timezone, plus a best-effort
               `timedatectl set-timezone` (needs sudo; logged honestly)
  * Apps      -> installed for real on Arch: official packages with
               `sudo pacman -S`, AUR packages with `paru -S` (paru is
               bootstrapped first if it is missing). One sudo password,
               typed once on the installing screen, fed to `sudo -S`,
               never stored. A failed install says so and lets the
               user continue — nothing is faked.

Honest limit: this VM has no pacman, so the real Arch installs
CANNOT be proven here. --dry-run (or VILE_SETUP_DRY_RUN=1) prints
the exact commands that would run instead of executing them, and
that is what the headless test proves. The real install is
unproven-until-Dell — see BUILD_LOG.md.

Stack: Python + GTK3 (same dark style as Settings — the CSS is
imported from vile-settings.py, so the two apps cannot drift apart).
GTK3 runs natively on Wayland (xdg-shell).

CLI:
  vile-setup.py [--dry-run] [--step N]
    --dry-run   show the install commands instead of running them
    --step N    open on step N (0 Welcome, 1 Name, 2 Wallpaper,
                3 Apps, 4 Timezone, 5 Done, 6 Installing) — for tests
"""

import argparse
import getpass
import importlib.util
import os
import shutil
import subprocess
import sys
import threading

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("GdkPixbuf", "2.0")
from gi.repository import Gtk, Gdk, GLib, GdkPixbuf

VERSION = "0.5.6"

# ---- Shared code from Settings (imported, never copied) ------------
# set_profile_picture / set_wallpaper / wallpaper thumbnails / CSS are
# the SAME functions Settings uses, so the wizard and Settings cannot
# drift apart. vile-settings.py is a plain module (its main() is
# guarded), so importing it is safe.
_SETTINGS_PATHS = (
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 "..", "settings", "vile-settings.py"),
    os.path.expanduser("~/vile-os/settings/vile-settings.py"),
    os.path.expanduser("~/workspace/vile-os/settings/vile-settings.py"),
)
_settings = None
for _p in _SETTINGS_PATHS:
    _p = os.path.normpath(_p)
    if os.path.isfile(_p):
        _spec = importlib.util.spec_from_file_location(
            "vile_settings_shared", _p)
        _settings = importlib.util.module_from_spec(_spec)
        _spec.loader.exec_module(_settings)
        break
if _settings is None:
    print("Vile Setup: cannot find vile-settings.py (shared code)",
          file=sys.stderr)
    sys.exit(1)

CSS = _settings.CSS
WALLPAPERS = _settings.WALLPAPERS
set_profile_picture = _settings.set_profile_picture
set_wallpaper = _settings.set_wallpaper
wallpapers_dir = _settings.wallpapers_dir
wallpaper_thumb_pixbuf = _settings.wallpaper_thumb_pixbuf
current_wallpaper_path = _settings.current_wallpaper_path
avatar_preview_pixbuf = _settings.avatar_preview_pixbuf
# Theme (0.5.4): same shared file as Settings and the compositor.
read_theme = _settings.read_theme
write_theme = _settings.write_theme
theme_css = _settings.theme_css
theme_preview_pixbuf = _settings.theme_preview_pixbuf

# ---- Vile logo (real ribbon V, Adnan's own asset, Nov 10, 2026) ---
# Welcome step hero mark. Same path pattern as the Settings import
# above: setup dir on this VM / ISO layout, then the two home dirs.
def _vile_logo_pixbuf(size=96):
    for _p in (
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "..", "assets", "vile-logo.png"),
        os.path.expanduser("~/vile-os/assets/vile-logo.png"),
        os.path.expanduser("~/workspace/vile-os/assets/vile-logo.png"),
    ):
        _p = os.path.normpath(_p)
        if os.path.isfile(_p):
            try:
                return GdkPixbuf.Pixbuf.new_from_file_at_size(
                    _p, size, size)
            except Exception as e:  # corrupt file: fall through
                print("Vile Setup: cannot load logo:", e,
                      file=sys.stderr)
    return None

# ---- What the wizard can install (real Arch packages only) ---------
# source: "official" = Arch extra repo via pacman, "aur" = AUR via
# paru, "included" = already part of Vile (shown, not installable).
# Verified Nov 10, 2026 against archlinux.org: thunar, mousepad,
# mpv, amberol, eog, btop and libreoffice-fresh are all in Extra;
# helium-browser-bin is an AUR package. libreoffice-fresh is
# 148 MB to download / 423 MB installed — off by default.
APPS = [
    ("Helium", "helium-browser-bin", "aur",
     "Private, fast browser (Chromium-based)", True),
    ("Thunar", "thunar", "official",
     "File manager — browse your files", True),
    ("Mousepad", "mousepad", "official",
     "Simple text editor", True),
    ("mpv", "mpv", "official",
     "Video player — plays almost anything", True),
    ("Amberol", "amberol", "official",
     "Music player for your songs", True),
    ("Eye of GNOME", "eog", "official",
     "Photo viewer", True),
    ("LibreOffice", "libreoffice-fresh", "official",
     "Documents, spreadsheets, slides — large download (~420 MB)",
     False),
    ("btop", "btop", "official",
     "System monitor, in your terminal", True),
    ("ScreenFit", None, "included",
     "Fixes TV overscan — already part of Vile", True),
]

TIMEZONES = [
    "America/Toronto", "America/Vancouver", "America/Winnipeg",
    "America/Montreal", "America/New_York", "America/Chicago",
    "America/Denver", "America/Los_Angeles", "America/Halifax",
    "Europe/London", "Europe/Paris", "Europe/Berlin",
    "Asia/Dubai", "Asia/Kolkata", "Asia/Singapore", "Asia/Tokyo",
    "Australia/Sydney", "Pacific/Auckland", "UTC",
]
DEFAULT_TIMEZONE = "America/Toronto"


def vile_dir():
    return os.path.expanduser("~/.config/vile")


def setup_done_path():
    return os.path.join(vile_dir(), "setup-done")


def display_name_path():
    return os.path.join(vile_dir(), "display-name")


def timezone_path():
    return os.path.join(vile_dir(), "timezone")


def write_setup_done(name):
    """The marker the compositor checks. Written at Finish and on
    Skip, so a crash during installs does not re-trigger the wizard."""
    import datetime
    os.makedirs(vile_dir(), exist_ok=True)
    try:
        with open(setup_done_path(), "w") as f:
            f.write("vile-setup 0.5.6\n")
            f.write("finished: %s\n" % datetime.datetime.now().isoformat())
            f.write("name: %s\n" % name)
        return True
    except OSError as e:
        print(f"Vile Setup: cannot write marker: {e}", file=sys.stderr)
        return False


def save_display_name(name):
    name = (name or "").strip()
    os.makedirs(vile_dir(), exist_ok=True)
    try:
        if name:
            with open(display_name_path(), "w") as f:
                f.write(name + "\n")
        else:
            try:
                os.unlink(display_name_path())
            except FileNotFoundError:
                pass
        return True
    except OSError as e:
        print(f"Vile Setup: cannot save name: {e}", file=sys.stderr)
        return False


def save_timezone(tz):
    os.makedirs(vile_dir(), exist_ok=True)
    try:
        with open(timezone_path(), "w") as f:
            f.write(tz + "\n")
        return True
    except OSError as e:
        print(f"Vile Setup: cannot save timezone: {e}", file=sys.stderr)
        return False


class SetupWindow(Gtk.Window):
    STEPS = ["Welcome", "You", "Wallpaper", "Theme", "Apps", "Timezone",
             "Done"]

    def __init__(self, start_step=0, dry_run=False):
        super().__init__(title="Vile Setup")
        self.set_default_size(760, 600)
        self.set_position(Gtk.WindowPosition.CENTER)
        self.dry_run = dry_run
        self.step = 0
        self._sudo_password = ""
        self._installing = False
        self._on_installing = False
        self._install_done = False
        self.data = {
            "name": getpass.getuser(),
            "picture_set": False,
            "wallpaper": current_wallpaper_path(),
            # Theme (0.5.4): default selection = the current theme
            # (dark on a first install — the file does not exist yet).
            "theme": "light" if read_theme() else "dark",
            "apps": {pkg: default for (_n, pkg, _s, _d, default) in APPS},
            "timezone": DEFAULT_TIMEZONE,
        }
        self._app_checks = {}  # pkg -> Gtk.CheckButton (None = included)

        # Theme-aware CSS (0.5.4): the provider is kept so picking
        # a theme on the Theme step restyles the wizard itself live.
        self._css = Gtk.CssProvider()
        self._css.load_from_data(theme_css(read_theme()).encode())
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), self._css,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        root = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.add(root)

        # Centred card: everything lives in here, one step at a time.
        card = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
        card.set_margin_top(28)
        card.set_margin_bottom(20)
        card.set_margin_start(48)
        card.set_margin_end(48)
        card.set_halign(Gtk.Align.CENTER)
        card.set_valign(Gtk.Align.CENTER)
        root.pack_start(card, True, True, 0)
        self.card = card

        # Progress dots (6 steps).
        dots = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        dots.set_halign(Gtk.Align.CENTER)
        card.pack_start(dots, False, False, 0)
        self.dots = []
        for _ in self.STEPS:
            dot = Gtk.Label(label="●")
            dot.get_style_context().add_class("muted")
            dots.pack_start(dot, False, False, 0)
            self.dots.append(dot)

        self.stack = Gtk.Stack()
        self.stack.set_transition_type(
            Gtk.StackTransitionType.SLIDE_LEFT_RIGHT)
        self.stack.set_transition_duration(200)
        card.pack_start(self.stack, True, True, 0)

        self._build_welcome()
        self._build_name()
        self._build_wallpaper()
        self._build_theme()
        self._build_apps()
        self._build_timezone()
        self._build_done()
        self._build_installing()

        # Nav row: Back | Skip setup ......... Continue/Finish.
        nav = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        card.pack_start(nav, False, False, 0)
        self.back_btn = Gtk.Button(label="Back")
        self.back_btn.get_style_context().add_class("secondary")
        self.back_btn.connect("clicked", self.on_back)
        nav.pack_start(self.back_btn, False, False, 0)
        self.skip_btn = Gtk.Button(label="Skip setup")
        self.skip_btn.get_style_context().add_class("secondary")
        self.skip_btn.connect("clicked", self.on_skip)
        nav.pack_start(self.skip_btn, False, False, 0)
        nav.pack_start(Gtk.Label(label=""), True, True, 0)
        self.continue_btn = Gtk.Button(label="Continue")
        self.continue_btn.connect("clicked", self.on_continue)
        nav.pack_start(self.continue_btn, False, False, 0)

        self.show_step(max(0, min(start_step, 6)))

    # ---- Step builders ----------------------------------------------

    def _step_title(self, parent, title, sub):
        t = Gtk.Label(label=title)
        t.get_style_context().add_class("title")
        t.set_halign(Gtk.Align.CENTER)
        parent.pack_start(t, False, False, 0)
        s = Gtk.Label(label=sub)
        s.get_style_context().add_class("muted")
        s.set_halign(Gtk.Align.CENTER)
        s.set_line_wrap(True)
        parent.pack_start(s, False, False, 0)

    def _build_welcome(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        pix = _vile_logo_pixbuf(96)
        if pix is not None:
            logo = Gtk.Image.new_from_pixbuf(pix)
        else:  # asset missing: the old text V, never a blank step
            logo = Gtk.Label(label="V")
            logo.get_style_context().add_class("big-logo")
        logo.set_halign(Gtk.Align.CENTER)
        box.pack_start(logo, False, False, 0)
        self._step_title(box, "Welcome to Vile",
                         "Your computer, your way.\n"
                         "A few quick questions, then you are in.")
        self.stack.add_named(box, "welcome")

    def _build_name(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self._step_title(box, "What should Vile call you?",
                         "Shown on the lock screen. Change it later "
                         "by running Setup again from Settings.")
        row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=20)
        row.set_halign(Gtk.Align.CENTER)
        box.pack_start(row, False, False, 0)
        self.avatar = Gtk.Image.new_from_pixbuf(
            avatar_preview_pixbuf(120))
        row.pack_start(self.avatar, False, False, 0)
        right = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        row.pack_start(right, False, False, 0)
        self.name_entry = Gtk.Entry()
        self.name_entry.set_text(self.data["name"])
        self.name_entry.set_placeholder_text("Your name")
        self.name_entry.set_width_chars(22)
        right.pack_start(self.name_entry, False, False, 0)
        pic_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                          spacing=8)
        right.pack_start(pic_row, False, False, 0)
        change = Gtk.Button(label="Choose picture…")
        change.connect("clicked", self.on_choose_picture)
        pic_row.pack_start(change, False, False, 0)
        remove = Gtk.Button(label="Remove")
        remove.get_style_context().add_class("secondary")
        remove.connect("clicked", self.on_remove_picture)
        pic_row.pack_start(remove, False, False, 0)
        self.pic_status = Gtk.Label(label="")
        self.pic_status.get_style_context().add_class("muted")
        self.pic_status.set_halign(Gtk.Align.START)
        right.pack_start(self.pic_status, False, False, 0)
        self.stack.add_named(box, "name")

    def _build_wallpaper(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self._step_title(box, "Pick a wallpaper",
                         "You can change it any time in Settings.")
        frame = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        frame.get_style_context().add_class("frame")
        box.pack_start(frame, False, False, 0)
        self.wp_rows = {}
        wdir = wallpapers_dir()
        if wdir is None:
            missing = Gtk.Label(
                label="Wallpapers not found (looked in ../wallpapers, "
                      "~/vile-os/wallpapers and "
                      "~/workspace/vile-os/wallpapers).")
            missing.get_style_context().add_class("muted")
            missing.set_line_wrap(True)
            frame.pack_start(missing, False, False, 0)
        for name, fname in WALLPAPERS:
            if wdir is None:
                break
            path = os.path.join(wdir, fname)
            row = Gtk.Button()
            row.get_style_context().add_class("wallpaper-row")
            row.connect("clicked", self.on_wallpaper_clicked, path)
            hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                           spacing=16)
            row.add(hbox)
            thumb = Gtk.Image.new_from_pixbuf(
                wallpaper_thumb_pixbuf(path))
            hbox.pack_start(thumb, False, False, 0)
            name_lbl = Gtk.Label(label=name)
            name_lbl.get_style_context().add_class("wp-name")
            name_lbl.set_halign(Gtk.Align.START)
            hbox.pack_start(name_lbl, True, True, 0)
            cur_lbl = Gtk.Label(label="✓ Current")
            cur_lbl.get_style_context().add_class("current-tag")
            cur_lbl.set_no_show_all(True)
            hbox.pack_start(cur_lbl, False, False, 0)
            frame.pack_start(row, False, False, 0)
            self.wp_rows[path] = (row, cur_lbl)
        self._refresh_wallpaper_selection()
        self.stack.add_named(box, "wallpaper")

    def _build_theme(self):
        # Theme step (0.5.4): two big choice cards, Dark / Light,
        # each with a small preview — same row pattern as the
        # Wallpaper step. Clicking one writes the theme file
        # immediately AND restyles this wizard live, so the choice
        # is visible before Continue. Default = current theme.
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self._step_title(box, "Dark or light?",
                         "The whole system follows — desktop, lock "
                         "screen and apps. Change it any time in "
                         "Settings.")
        frame = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        frame.get_style_context().add_class("frame")
        box.pack_start(frame, False, False, 0)
        self.theme_rows = {}
        for light, name, desc in (
                (False, "Dark",
                 "Dark surfaces, light text — the Vile look"),
                (True, "Light",
                 "Lavender-white surfaces, dark text")):
            row = Gtk.Button()
            row.get_style_context().add_class("wallpaper-row")
            row.connect("clicked", self.on_theme_clicked, light)
            hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                           spacing=16)
            row.add(hbox)
            thumb = Gtk.Image.new_from_pixbuf(
                theme_preview_pixbuf(light))
            hbox.pack_start(thumb, False, False, 0)
            text = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
            hbox.pack_start(text, True, True, 0)
            name_lbl = Gtk.Label(label=name)
            name_lbl.get_style_context().add_class("wp-name")
            name_lbl.set_halign(Gtk.Align.START)
            text.pack_start(name_lbl, False, False, 0)
            dims_lbl = Gtk.Label(label=desc)
            dims_lbl.get_style_context().add_class("wp-dims")
            dims_lbl.set_halign(Gtk.Align.START)
            text.pack_start(dims_lbl, False, False, 0)
            cur_lbl = Gtk.Label(label="✓ Current")
            cur_lbl.get_style_context().add_class("current-tag")
            cur_lbl.set_no_show_all(True)
            hbox.pack_start(cur_lbl, False, False, 0)
            frame.pack_start(row, False, False, 0)
            self.theme_rows[light] = (row, cur_lbl)
        self._refresh_theme_selection()
        self.stack.add_named(box, "theme")

    def _refresh_theme_selection(self):
        cur_light = (self.data["theme"] == "light")
        for light, (row, tag) in self.theme_rows.items():
            selected = (light == cur_light)
            tag.set_visible(selected)
            ctx = row.get_style_context()
            if selected:
                ctx.add_class("selected")
            else:
                ctx.remove_class("selected")

    def on_theme_clicked(self, _button, light):
        self.data["theme"] = "light" if light else "dark"
        write_theme(light)
        # Restyle the wizard itself live — the choice is visible
        # on this very step.
        self._css.load_from_data(theme_css(light).encode())
        self._refresh_theme_selection()

    def _build_apps(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self._step_title(box, "Which apps do you want?",
                         "Installed from Arch's official repos with "
                         "pacman, or the AUR with paru. Untick anything "
                         "you do not need — you can add it later.")
        frame = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        frame.get_style_context().add_class("frame")
        box.pack_start(frame, False, False, 0)
        for name, pkg, source, desc, default in APPS:
            row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                          spacing=12)
            check = Gtk.CheckButton(label=name)
            check.set_active(default)
            if source == "included":
                check.set_sensitive(False)
            else:
                check.connect("toggled", self.on_app_toggled, pkg)
            row.pack_start(check, False, False, 0)
            self._app_checks[pkg] = check if source != "included" else None
            d = Gtk.Label(label=desc)
            d.get_style_context().add_class("muted")
            d.set_halign(Gtk.Align.START)
            d.set_hexpand(True)
            row.pack_start(d, True, True, 0)
            tag = Gtk.Label(label={"official": "Official",
                                   "aur": "AUR",
                                   "included": "Included"}[source])
            tag.get_style_context().add_class("current-tag")
            row.pack_start(tag, False, False, 0)
            frame.pack_start(row, False, False, 0)
        note = Gtk.Label(
            label="Helium comes from the AUR, so Vile sets up paru first "
                  "if it is not there yet.")
        note.get_style_context().add_class("muted")
        note.set_halign(Gtk.Align.CENTER)
        note.set_line_wrap(True)
        box.pack_start(note, False, False, 0)
        self.stack.add_named(box, "apps")

    def _build_timezone(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self._step_title(box, "What time is it where you are?",
                         "Vile sets the system clock to your timezone.")
        combo = Gtk.ComboBoxText.new_with_entry()
        for tz in TIMEZONES:
            combo.append_text(tz)
        combo.set_active(TIMEZONES.index(DEFAULT_TIMEZONE))
        combo.set_halign(Gtk.Align.CENTER)
        box.pack_start(combo, False, False, 0)
        self.tz_combo = combo
        self.tz_status = Gtk.Label(label="")
        self.tz_status.get_style_context().add_class("muted")
        self.tz_status.set_halign(Gtk.Align.CENTER)
        box.pack_start(self.tz_status, False, False, 0)
        self.stack.add_named(box, "timezone")

    def _build_done(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self._step_title(box, "You are all set",
                         "Here is what Vile will do. Press Finish and "
                         "it installs your apps, then shows your desktop.")
        self.summary = Gtk.Label(label="")
        self.summary.set_halign(Gtk.Align.CENTER)
        self.summary.set_line_wrap(True)
        self.summary.set_justify(Gtk.Justification.CENTER)
        box.pack_start(self.summary, False, False, 0)
        self.stack.add_named(box, "done")

    def _build_installing(self):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self._step_title(box, "Installing your apps",
                         "This takes a while on first run. The log below "
                         "shows everything that happens — nothing is "
                         "hidden.")
        self.pw_label = Gtk.Label(
            label="Your password, once, for installing apps.\n"
                  "It is only used to run the installer — never stored.")
        self.pw_label.get_style_context().add_class("muted")
        self.pw_label.set_halign(Gtk.Align.CENTER)
        self.pw_label.set_line_wrap(True)
        box.pack_start(self.pw_label, False, False, 0)
        self.pw_entry = Gtk.Entry()
        self.pw_entry.set_visibility(False)
        self.pw_entry.set_placeholder_text("Password")
        self.pw_entry.set_halign(Gtk.Align.CENTER)
        self.pw_entry.set_width_chars(28)
        box.pack_start(self.pw_entry, False, False, 0)
        self.progress = Gtk.ProgressBar()
        self.progress.set_show_text(True)
        box.pack_start(self.progress, False, False, 0)
        self.install_status = Gtk.Label(label="")
        self.install_status.get_style_context().add_class("muted")
        self.install_status.set_halign(Gtk.Align.CENTER)
        box.pack_start(self.install_status, False, False, 0)
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.AUTOMATIC,
                            Gtk.PolicyType.AUTOMATIC)
        scrolled.set_min_content_height(160)
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_monospace(True)
        self.log_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        scrolled.add(self.log_view)
        box.pack_start(scrolled, True, True, 0)
        self.stack.add_named(box, "installing")

    # ---- Navigation ---------------------------------------------------

    def show_step(self, n):
        self.step = n
        names = ["welcome", "name", "wallpaper", "theme", "apps",
                 "timezone", "done"]
        self.stack.set_visible_child_name(names[n])
        for i, dot in enumerate(self.dots):
            dot.set_text("●" if i <= n else "○")
        self.back_btn.set_sensitive(n > 0)
        self.skip_btn.set_visible(n < 6)
        if n == 6:
            self._refresh_summary()
            self.continue_btn.set_label("Finish")
        else:
            self.continue_btn.set_label("Continue")
        self.continue_btn.set_visible(True)

    def on_back(self, _button):
        if self.step > 0:
            self.show_step(self.step - 1)

    def on_continue(self, _button):
        # One handler for the Continue button, dispatching on state —
        # never connect a second handler to it.
        if self._install_done:
            self.destroy()
            return
        if self._on_installing:
            self.on_install_clicked(_button)
            return
        if self.step < 6:
            self._save_current_step()
            self.show_step(self.step + 1)
        elif self.step == 6:
            self._save_current_step()
            self._finish()

    def on_skip(self, _button):
        # Defaults: system username, default wallpaper, no apps,
        # America/Toronto. The marker is written so the wizard does
        # not come back next boot.
        write_setup_done(getpass.getuser())
        save_timezone(DEFAULT_TIMEZONE)
        self.destroy()

    def _save_current_step(self):
        if self.step == 1:
            self.data["name"] = self.name_entry.get_text().strip() \
                or getpass.getuser()
            save_display_name(self.data["name"])
        elif self.step == 2:
            if self.data["wallpaper"]:
                set_wallpaper(self.data["wallpaper"])
        elif self.step == 3:
            # Theme: already written on click (and the wizard
            # restyled live) — re-write here so Continue alone
            # also saves it if the click handler ever changes.
            write_theme(self.data["theme"] == "light")
        elif self.step == 5:
            tz = self.tz_combo.get_active_text() or ""
            tz = tz.strip()
            if not tz:
                tz = DEFAULT_TIMEZONE
            self.data["timezone"] = tz
            save_timezone(tz)

    def _refresh_summary(self):
        apps = [name for (name, pkg, source, _d, _df) in APPS
                if pkg is None or self.data["apps"].get(pkg)]
        wp_name = "Purple Arch"
        for name, fname in WALLPAPERS:
            if self.data["wallpaper"] and \
                    self.data["wallpaper"].endswith(fname):
                wp_name = name
        lines = [
            f"Name: {self.data['name']}",
            "Picture: %s" % ("set ✓" if self.data["picture_set"]
                             else "not set (purple V)"),
            f"Wallpaper: {wp_name}",
            f"Theme: {self.data['theme'].capitalize()}",
            "Apps: %s" % (", ".join(apps) if apps else "none"),
            f"Timezone: {self.data['timezone']}",
        ]
        self.summary.set_text("\n".join(lines))

    def _finish(self):
        write_setup_done(self.data["name"])
        self._on_installing = True
        self.stack.set_visible_child_name("installing")
        self.back_btn.set_sensitive(False)
        self.skip_btn.set_visible(False)
        self.continue_btn.set_label("Install")
        self.continue_btn.set_visible(True)
        if self.dry_run:
            self.pw_label.set_visible(False)
            self.pw_entry.set_visible(False)
        # Installing starts when the user presses Install (the
        # Continue button, relabelled) — except in dry-run tests the
        # harness clicks it the same way.

    # ---- Step callbacks ------------------------------------------------

    def on_choose_picture(self, _button):
        dialog = Gtk.FileChooserDialog(
            title="Choose profile picture", parent=self,
            action=Gtk.FileChooserAction.OPEN)
        dialog.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                           Gtk.STOCK_OPEN, Gtk.ResponseType.OK)
        pictures = os.path.expanduser("~/Pictures")
        dialog.set_current_folder(
            pictures if os.path.isdir(pictures)
            else os.path.expanduser("~"))
        img_filter = Gtk.FileFilter()
        img_filter.set_name("Images (JPG, PNG)")
        img_filter.add_mime_type("image/jpeg")
        img_filter.add_mime_type("image/png")
        img_filter.add_pattern("*.jpg")
        img_filter.add_pattern("*.jpeg")
        img_filter.add_pattern("*.png")
        dialog.add_filter(img_filter)
        response = dialog.run()
        filename = dialog.get_filename() if response == Gtk.ResponseType.OK \
            else None
        dialog.destroy()
        if not filename:
            return
        if set_profile_picture(filename):
            self.data["picture_set"] = True
            self.avatar.set_from_pixbuf(avatar_preview_pixbuf(120))
            self.pic_status.set_text("Picture updated.")
        else:
            self.pic_status.set_text("Could not use that picture — "
                                     "try a JPG or PNG file.")

    def on_remove_picture(self, _button):
        _settings.remove_profile_picture()
        self.data["picture_set"] = False
        self.avatar.set_from_pixbuf(avatar_preview_pixbuf(120))
        self.pic_status.set_text("Picture removed.")

    def on_wallpaper_clicked(self, _button, path):
        self.data["wallpaper"] = path
        set_wallpaper(path)
        self._refresh_wallpaper_selection()

    def _refresh_wallpaper_selection(self):
        cur = self.data["wallpaper"] or current_wallpaper_path()
        for path, (row, tag) in self.wp_rows.items():
            selected = (path == cur)
            tag.set_visible(selected)
            ctx = row.get_style_context()
            if selected:
                ctx.add_class("selected")
            else:
                ctx.remove_class("selected")

    def on_app_toggled(self, check, pkg):
        self.data["apps"][pkg] = check.get_active()

    # ---- Install -------------------------------------------------------
    # Commands are built once, as data, so the dry-run log shows the
    # EXACT commands a real run would execute. Each step is
    # (label, argv, use_sudo). paru steps run as the user (paru
    # refuses root); pacman steps run under sudo with the password
    # typed once on this screen.

    def build_commands(self):
        """(steps, warnings) — steps is a list of (label, argv,
        use_sudo). No side effects: safe to call for the dry-run log
        and for the real run alike."""
        steps = []
        warnings = []
        official = [pkg for (_n, pkg, s, _d, _df) in APPS
                    if s == "official" and self.data["apps"].get(pkg)]
        aur = [pkg for (_n, pkg, s, _d, _df) in APPS
               if s == "aur" and self.data["apps"].get(pkg)]
        if official:
            steps.append(("Refresh package lists",
                          ["sudo", "-S", "pacman", "-Sy", "--noconfirm"],
                          True))
            steps.append(("Install from official repos: %s"
                          % ", ".join(official),
                          ["sudo", "-S", "pacman", "-S", "--noconfirm",
                           "--needed"] + official, True))
        if aur:
            if os.geteuid() == 0:
                warnings.append(
                    "AUR (%s) needs a normal user account — paru "
                    "refuses to run as root, so install it after "
                    "creating your user." % ", ".join(aur))
            else:
                if shutil.which("paru") is None:
                    steps.append(("Set up paru (AUR helper)",
                                  ["git", "clone",
                                   "https://aur.archlinux.org/paru.git",
                                   "/tmp/vile-paru"], False))
                    steps.append(("Build + install paru",
                                  ["bash", "-c",
                                   "cd /tmp/vile-paru && "
                                   "makepkg -si --noconfirm"], False))
                steps.append(("Install from the AUR: %s" % ", ".join(aur),
                              ["paru", "-S", "--noconfirm",
                               "--needed"] + aur, False))
        tz = self.data["timezone"]
        steps.append(("Set timezone to %s" % tz,
                      ["sudo", "-S", "timedatectl", "set-timezone", tz],
                      True))
        return steps, warnings

    def on_install_clicked(self, _button):
        if self._installing or self._install_done:
            return
        self._installing = True
        self.continue_btn.set_sensitive(False)
        self._sudo_password = self.pw_entry.get_text()
        self.pw_entry.set_text("")
        thread = threading.Thread(target=self._run_install,
                                  daemon=True)
        thread.start()

    def _log(self, text):
        buf = self.log_view.get_buffer()
        buf.insert(buf.get_end_iter(), text + "\n")
        mark = buf.create_mark(None, buf.get_end_iter(), False)
        self.log_view.scroll_to_mark(mark, 0.0, True, 0.0, 1.0)

    def _set_progress(self, frac, text):
        self.progress.set_fraction(frac)
        self.progress.set_text(text)
        self.install_status.set_text(text)

    def _run_install(self):
        steps, warnings = self.build_commands()
        for w in warnings:
            GLib.idle_add(self._log, "⚠ " + w)
        total = max(len(steps), 1)
        ok_count = 0
        fail_count = 0
        for i, (label, argv, use_sudo) in enumerate(steps):
            GLib.idle_add(self._log, "")
            GLib.idle_add(self._log, "▸ %s" % label)
            shown = ["sudo …" if a == "-S" else a for a in argv]
            if self.dry_run:
                GLib.idle_add(self._log,
                              "[dry run] would run: %s"
                              % " ".join(argv))
                ok_count += 1
                GLib.idle_add(self._set_progress,
                              (i + 1) / total,
                              "%d/%d — %s" % (i + 1, total, label))
                continue
            GLib.idle_add(self._log, "$ " + " ".join(shown))
            rc = self._run_one(argv, use_sudo)
            if rc == 0:
                ok_count += 1
                GLib.idle_add(self._log, "✓ done")
            else:
                fail_count += 1
                GLib.idle_add(self._log,
                              "✗ FAILED (exit %d) — continuing anyway; "
                              "you can install this later from a terminal."
                              % rc)
            GLib.idle_add(self._set_progress,
                          (i + 1) / total,
                          "%d/%d — %s" % (i + 1, total, label))
        if self.dry_run:
            GLib.idle_add(self._log, "")
            GLib.idle_add(self._log,
                          "[dry run] no packages were installed — this "
                          "VM has no pacman. The commands above are the "
                          "exact ones a real Arch run would execute.")
        self._sudo_password = ""
        GLib.idle_add(self._install_finished, ok_count, fail_count)

    def _run_one(self, argv, use_sudo):
        """Run one command, streaming its output to the log. Returns
        the exit code (127 if the program is missing)."""
        stdin_data = None
        if use_sudo:
            stdin_data = (self._sudo_password + "\n").encode()
        try:
            proc = subprocess.Popen(
                argv, stdin=subprocess.PIPE if use_sudo else None,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1)
        except FileNotFoundError as e:
            GLib.idle_add(self._log, "✗ not found: %s" % e.filename)
            return 127
        out, _ = proc.communicate(input=stdin_data)
        for line in (out or "").splitlines():
            if line.strip():
                GLib.idle_add(self._log, "  " + line)
        return proc.returncode

    def _install_finished(self, ok_count, fail_count):
        self._installing = False
        self._install_done = True
        self._on_installing = False
        self._set_progress(1.0, "Done")
        if fail_count:
            self.install_status.set_text(
                "%d step(s) failed — Vile still works, install the "
                "rest later from a terminal." % fail_count)
        else:
            self.install_status.set_text(
                "All set — welcome to Vile.")
        self.continue_btn.set_label("Open desktop")
        self.continue_btn.set_sensitive(True)


def main():
    parser = argparse.ArgumentParser(description="Vile Setup")
    parser.add_argument("--dry-run", action="store_true",
                        help="show install commands instead of running "
                             "them (no pacman on this VM)")
    parser.add_argument("--step", type=int, default=0,
                        help="open on step N (0 Welcome .. 6 Done, "
                             "7 Installing)")
    args = parser.parse_args()
    dry_run = args.dry_run or bool(os.environ.get("VILE_SETUP_DRY_RUN"))

    Gtk.Settings.get_default().set_property(
        "gtk-application-prefer-dark-theme", not read_theme())

    win = SetupWindow(start_step=args.step, dry_run=dry_run)
    if args.step == 7:
        # Test path: jump straight to the installing screen with the
        # default selections (Finish already implied — marker written).
        write_setup_done(getpass.getuser())
        win._on_installing = True
        win.stack.set_visible_child_name("installing")
        win.back_btn.set_sensitive(False)
        win.skip_btn.set_visible(False)
        win.continue_btn.set_label("Install")
        if dry_run:
            win.pw_label.set_visible(False)
            win.pw_entry.set_visible(False)
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    if args.step:
        # Re-assert the initial step AFTER the window is shown.
        # set_visible_child_name before show does not stick: the
        # stack shows its first child even though show_step ran
        # (proved: --step 3 showed Theme dots over the Welcome
        # page, twice — disabling the slide transition did not
        # fix it). Once realized, the same call works.
        win.show_step(max(0, min(args.step, 6)))
    # The Continue button starts as the window default, so Enter
    # advances through the wizard (the harness drives it that way
    # only where no text entry has focus; elsewhere it clicks).
    win.continue_btn.set_can_default(True)
    win.continue_btn.grab_default()
    # Enter-race fix (Nov 11): GTK gives the window's initial
    # focus to the first SENSITIVE focusable widget — at step 0
    # Back is insensitive, so focus landed on "Skip setup" and
    # one Enter fired on_skip (setup-done + destroy), skipping
    # the whole wizard. Focus Continue explicitly so Enter hits
    # Continue (one step) instead.
    win.continue_btn.grab_focus()
    Gtk.main()


if __name__ == "__main__":
    main()
