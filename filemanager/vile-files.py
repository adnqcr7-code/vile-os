#!/usr/bin/env python3
"""Vile Files — the Vile file manager, v1.2.

Follows the Settings pattern (Python + GTK3, dark Vile look, native
Wayland client, no fake data): everything shown is a real directory
listing — real names, sizes and dates, read live from disk.

v1.2 scope (on top of v1.1):
  * Zip / unzip, right-click menu, Python zipfile/tarfile only:
    - "Compress to ZIP" for one or more selected files/folders
      (Deflate, folder structure + relative paths preserved,
      Windows-style " (2)" duplicates, background thread with a
      real progress dialog so the UI never freezes).
    - "Extract Here" (into a folder named after the archive) and
      "Extract to…" (folder chooser) for .zip, plus .tar.gz/.tgz.
      Zip-slip paths are blocked, corrupt or password-protected
      archives show a clear error and extract nothing.
    - .zip / .tar.gz files get a distinct archive glyph.
  * Test-only hook: VILE_FILES_SLOW_ZIP=1 makes the archive worker
    pause briefly per file so the progress dialog stays visible
    long enough to screenshot. Off by default; never set in the
    shipped launcher.

v1.1 scope (on top of v1):
  * Sidebar: standard folders are ALWAYS shown — Home, Desktop,
    Documents, Downloads, Pictures, Music, Videos. The missing ones
    are created on first run (standard xdg-user-dirs behaviour:
    creating empty standard folders is correct and expected).
  * "This PC" page: every mounted drive — name, mount point,
    filesystem, total / used / free, with a Windows-style usage bar.
    Driven by Gio.VolumeMonitor (volume/mount added/removed
    signals) with a /proc/self/mounts fallback + 1s poll, because
    the Gio unix monitor only sees block-device mounts and this
    test VM has none (overlay/tmpfs only). Unmounted volumes show
    greyed with a click-to-mount action.
  * Status bar also shows the current drive's free space
    ("X free of Y on /").

v1 scope:
  * Sidebar: Home, Documents, Downloads, Pictures, Music, Videos —
    each shown ONLY if the directory exists (no invented entries).
  * Icon grid of the real listing (folders first, then files,
    alphabetical); double-click a folder to enter it, double-click
    a file to open it with the system default (xdg-open).
  * Header: Back / Forward / Up, clickable path breadcrumbs,
    New Folder, search/filter box (filters the current listing by
    name as you type), grid/list view toggle, show-hidden toggle.
  * File ops: New Folder (asks for a name), Rename, Delete → moved
    to trash via Gio (NEVER rm, never permanent delete).
  * Status bar: item count + selected item size. Window title is the
    current folder name.

Stack: Python + GTK3 (python3-gi, gir1.2-gtk-3.0). On Arch:
  sudo pacman -S gtk3 python-gobject python-pillow
GTK3 runs natively on Wayland (xdg-shell) — no XWayland needed, so
the window opens directly in the Vile compositor.

CLI:
  vile-files.py [--path DIR]   open at DIR (default: home)
"""

import argparse
import datetime
import os
import shutil
import subprocess
import sys
import tarfile
import threading
import time
import zipfile

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, GdkPixbuf, GLib, Gio

from PIL import Image, ImageDraw

APP_NAME = "Vile Files"
VERSION = "1.2.0"

ACCENT = (109, 74, 255)  # #6D4AFF
BG = "#0F0B1A"


# ---- icons (drawn with PIL — decorative glyphs only; the listing
# itself is always real data) --------------------------------------
def _folder_pil(size, accent=True):
    """A folder glyph: back tab + body, purple when accent."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pad = size // 8
    tab_w = size // 3
    body_top = size // 4
    back = (74, 52, 200, 255) if accent else (90, 82, 120, 255)
    front = (109, 74, 255, 255) if accent else (120, 110, 150, 255)
    # tab + back body
    d.rounded_rectangle([pad, pad, pad + tab_w, body_top + pad // 2],
                        radius=size // 16, fill=back)
    d.rounded_rectangle([pad, body_top, size - pad, size - pad],
                        radius=size // 12, fill=back)
    # front face, slightly lower so the tab reads
    d.rounded_rectangle([pad, body_top + size // 12,
                         size - pad, size - pad],
                        radius=size // 12, fill=front)
    # top edge highlight
    d.line([(pad + size // 10, body_top + size // 12 + 2),
            (size - pad - size // 10, body_top + size // 12 + 2)],
           fill=(255, 255, 255, 60), width=max(1, size // 48))
    return im


def _file_pil(size):
    """A plain page glyph with a folded corner, light grey."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pad = size // 6
    fold = size // 4
    body = (226, 224, 238, 255)
    edge = (155, 147, 184, 255)
    pts = [(pad, pad), (size - pad - fold, pad),
           (size - pad, pad + fold), (size - pad, size - pad),
           (pad, size - pad)]
    d.polygon(pts, fill=body)
    d.line(pts + [pts[0]], fill=edge, width=max(1, size // 48), joint="curve")
    # folded corner
    d.polygon([(size - pad - fold, pad), (size - pad, pad + fold),
               (size - pad - fold, pad + fold)], fill=(190, 184, 210, 255))
    # text lines
    for i, y in enumerate((size // 2 - size // 16, size // 2 + size // 16,
                           size // 2 + size // 5)):
        d.line([(pad + size // 8, y), (size - pad - size // 8, y)],
               fill=(155, 147, 184, 255), width=max(1, size // 64))
    return im


def _drive_pil(size):
    """A hard-drive glyph: dark body, lighter slot, purple LED."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pad = size // 6
    top = size // 4
    bot = size - pad
    # body
    d.rounded_rectangle([pad, top, size - pad, bot],
                        radius=size // 12, fill=(42, 33, 64, 255))
    d.rounded_rectangle([pad, top, size - pad, bot],
                        radius=size // 12, outline=(120, 110, 150, 255),
                        width=max(1, size // 48))
    # platter slot
    d.rounded_rectangle([pad + size // 8, top + (bot - top) // 3,
                         size - pad - size // 8,
                         top + (bot - top) // 3 + size // 16],
                        radius=size // 32, fill=(23, 18, 40, 255))
    # LED
    d.ellipse([size - pad - size // 6, bot - size // 5,
               size - pad - size // 12, bot - size // 10],
              fill=(109, 74, 255, 255))
    return im


def _archive_pil(size):
    """An archive glyph: a zipped package — dark body, purple
    zipper line down the middle with a pull tab at the top."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pad = size // 6
    top = size // 5
    bot = size - pad
    # body
    d.rounded_rectangle([pad, top, size - pad, bot],
                        radius=size // 12, fill=(42, 33, 64, 255))
    d.rounded_rectangle([pad, top, size - pad, bot],
                        radius=size // 12, outline=(120, 110, 150, 255),
                        width=max(1, size // 48))
    # zipper: two rails + teeth down the centre
    mid = size // 2
    d.line([(mid, top), (mid, bot)], fill=(109, 74, 255, 255),
           width=max(1, size // 24))
    tooth = max(1, size // 32)
    y = top + size // 12
    while y < bot - size // 12:
        d.line([(mid - tooth * 2, y), (mid + tooth * 2, y)],
               fill=(155, 147, 184, 255), width=max(1, size // 64))
        y += size // 10
    # pull tab at the top of the zipper
    d.rounded_rectangle([mid - size // 12, top - size // 24,
                         mid + size // 12, top + size // 8],
                        radius=size // 32, fill=(109, 74, 255, 255))
    return im


def _pixbuf_from_pil(im):
    import io
    buf = io.BytesIO()
    im.save(buf, "PNG")
    buf.seek(0)
    loader = GdkPixbuf.PixbufLoader.new_with_type("png")
    loader.write(buf.read())
    loader.close()
    return loader.get_pixbuf()


_ICON_CACHE = {}


def icon_pixbuf(kind, size):
    """Cached pixbuf for 'folder', 'file', 'drive' or 'archive'."""
    key = (kind, size)
    if key not in _ICON_CACHE:
        if kind == "folder":
            im = _folder_pil(size)
        elif kind == "drive":
            im = _drive_pil(size)
        elif kind == "archive":
            im = _archive_pil(size)
        else:
            im = _file_pil(size)
        _ICON_CACHE[key] = _pixbuf_from_pil(im)
    return _ICON_CACHE[key]


def is_archive(path):
    """True for files the manager can extract: .zip, .tar.gz, .tgz."""
    name = os.path.basename(path).lower()
    return (name.endswith(".zip") or name.endswith(".tar.gz")
            or name.endswith(".tgz"))


def archive_stem(path):
    """'Pack.zip' -> 'Pack', 'data.tar.gz' -> 'data'."""
    name = os.path.basename(path)
    low = name.lower()
    for ext in (".tar.gz", ".tgz", ".zip"):
        if low.endswith(ext):
            return name[:-len(ext)]
    return os.path.splitext(name)[0]


def unique_path(directory, name):
    """`name` in `directory`, or 'name (2)' / 'name (3)' … if taken
    (Windows-style duplicate handling, for files and folders)."""
    candidate = os.path.join(directory, name)
    if not os.path.exists(candidate):
        return candidate
    stem, ext = os.path.splitext(name)
    n = 2
    while True:
        candidate = os.path.join(directory, f"{stem} ({n}){ext}")
        if not os.path.exists(candidate):
            return candidate
        n += 1


def human_size(nbytes):
    """1234 -> '1.2 KB'. Folders show 'Folder' instead (see listing)."""
    if nbytes < 1024:
        return f"{nbytes} B"
    for unit in ("KB", "MB", "GB", "TB"):
        nbytes /= 1024.0
        if nbytes < 1024:
            return f"{nbytes:.1f} {unit}"
    return f"{nbytes:.1f} PB"


def fmt_date(mtime):
    return datetime.datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M")


CSS = """
window { background-color: #0F0B1A; color: #EDE9FE; }
label { color: #EDE9FE; }
headerbar, .titlebar {
  background-color: #241640; background-image: none; color: #EDE9FE;
  border: none; box-shadow: none;
}
headerbar label { color: #EDE9FE; }
headerbar button {
  background-color: #2A2140; background-image: none; color: #EDE9FE;
  border-radius: 8px; min-width: 28px; min-height: 28px; padding: 4px 8px;
}
headerbar button:hover {
  background-color: #3A2E58; background-image: none;
}
headerbar button:disabled {
  background-color: #1E1836; background-image: none; color: #6B6486;
}
button {
  background-color: #6D4AFF; background-image: none; color: white;
  border-radius: 12px; padding: 8px 16px; font-size: 15px; border: none;
}
button:hover { background-color: #7D5CFF; background-image: none; }
button.secondary {
  background-color: #2A2140; background-image: none; color: #EDE9FE;
}
button.secondary:hover {
  background-color: #3A2E58; background-image: none;
}
button.toggle:checked, button:checked {
  background-color: #6D4AFF; background-image: none; color: white;
}
button.crumb {
  background-color: transparent; background-image: none; color: #EDE9FE;
  border-radius: 8px; padding: 4px 8px;
}
button.crumb:hover { background-color: #2A2140; background-image: none; }
entry, searchentry {
  background-color: #171228; background-image: none; color: #EDE9FE;
  border: 1px solid #2A2140; border-radius: 8px; padding: 6px 10px;
}
entry:focus { border-color: #6D4AFF; }
.muted { color: #9B93B8; font-size: 13px; }
.statusbar { color: #9B93B8; font-size: 13px; padding: 6px 10px; }
.sidebar { background-color: #14102A; }
.sidebar row { padding: 10px 14px; border-radius: 8px; }
.sidebar row:hover { background-color: #1E1836; }
.sidebar row:selected {
  background-color: #6D4AFF; background-image: none; color: white;
}
.sidebar row:selected label { color: white; }
.sidebar .place-name { font-size: 15px; }
flowboxchild { border-radius: 12px; padding: 4px; }
flowboxchild:selected {
  background-color: #2A2140; background-image: none;
}
flowboxchild:selected label { color: #FFFFFF; }
.item-name { font-size: 14px; }
.item-meta { color: #9B93B8; font-size: 12px; }
treeview {
  background-color: #0F0B1A; color: #EDE9FE;
}
treeview:selected, treeview selection {
  background-color: #6D4AFF; background-image: none; color: #FFFFFF;
}
treeview header button {
  background-color: #171228; background-image: none; color: #EDE9FE;
  border: none;
}
scrolledwindow { background-color: #0F0B1A; }
separator { background-color: #2A2140; }
dialog { background-color: #0F0B1A; color: #EDE9FE; }
menu { background-color: #171228; color: #EDE9FE; border: 1px solid #2A2140; }
menuitem { color: #EDE9FE; padding: 8px 14px; }
menuitem:hover { background-color: #6D4AFF; background-image: none; color: white; }
.drive-name { font-size: 16px; font-weight: bold; }
.drive-sub { color: #9B93B8; font-size: 13px; }
.drive-free { font-size: 14px; }
.drive-row { padding: 12px 16px; border-bottom: 1px solid #2A2140; }
.drive-row:hover { background-color: #1E1836; }
.drive-row.unmounted label { color: #6B6486; }
progressbar { min-height: 10px; border-radius: 5px; }
progressbar trough {
  background-color: #2A2140; background-image: none;
  border-radius: 5px; min-height: 10px;
}
progressbar progress {
  background-color: #6D4AFF; background-image: none;
  border-radius: 5px; min-height: 10px;
}
progressbar.warn progress { background-color: #E8A33D; background-image: none; }
progressbar.full progress { background-color: #E84A5F; background-image: none; }
"""

# Sidebar places: (label, subdir of home or None for home itself).
# v1.1: the standard folders are ALWAYS shown — missing ones are
# created on first run (see ensure_standard_dirs), which is the
# standard xdg-user-dirs behaviour, not invented data.
STANDARD_DIRS = ("Desktop", "Documents", "Downloads",
                 "Pictures", "Music", "Videos")
PLACES = [
    ("Home", None),
    ("Desktop", "Desktop"),
    ("Documents", "Documents"),
    ("Downloads", "Downloads"),
    ("Pictures", "Pictures"),
    ("Music", "Music"),
    ("Videos", "Videos"),
]


def ensure_standard_dirs():
    """Create the missing standard user folders (~/Desktop, ...).

    This is what xdg-user-dirs does on first login: the folders are
    part of the user's home layout, so creating the empty ones is
    correct — the sidebar never shows a place that does not exist.
    """
    home = os.path.expanduser("~")
    for sub in STANDARD_DIRS:
        try:
            os.makedirs(os.path.join(home, sub), exist_ok=True)
        except OSError:
            pass  # a missing place is skipped by the sidebar anyway


def place_path(label, subdir):
    home = os.path.expanduser("~")
    return home if subdir is None else os.path.join(home, subdir)


class FilesWindow(Gtk.Window):
    def __init__(self, start_path):
        super().__init__(title="Files")
        self.set_default_size(1000, 640)
        self.set_position(Gtk.WindowPosition.CENTER)

        self.current = os.path.abspath(start_path)
        self.back_stack = []
        self.fwd_stack = []
        self.filter_text = ""
        self.show_hidden = False
        self.view_grid = True
        self._entries = []  # current listing: dicts (name, path, is_dir, size, mtime)
        self._selected_path = None  # first selected item (Enter/Delete/F2)
        self._selected_paths = []  # all selected items (Compress uses all)
        self.devices_mode = False  # True while the This PC page is showing
        self._devices_sig = None  # signature of the last rendered drive list

        css = Gtk.CssProvider()
        css.load_from_data(CSS.encode())
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), css,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        # ---- header bar: nav + actions ------------------------------
        hb = Gtk.HeaderBar()
        hb.set_show_close_button(True)
        self.set_titlebar(hb)

        self.back_btn = Gtk.Button.new_from_icon_name(
            "go-previous-symbolic", Gtk.IconSize.BUTTON)
        self.back_btn.set_tooltip_text("Back")
        self.back_btn.connect("clicked", self.on_back)
        hb.pack_start(self.back_btn)

        self.fwd_btn = Gtk.Button.new_from_icon_name(
            "go-next-symbolic", Gtk.IconSize.BUTTON)
        self.fwd_btn.set_tooltip_text("Forward")
        self.fwd_btn.connect("clicked", self.on_forward)
        hb.pack_start(self.fwd_btn)

        self.up_btn = Gtk.Button.new_from_icon_name(
            "go-up-symbolic", Gtk.IconSize.BUTTON)
        self.up_btn.set_tooltip_text("Up to parent folder")
        self.up_btn.connect("clicked", self.on_up)
        hb.pack_start(self.up_btn)

        new_btn = Gtk.Button(label="＋ New Folder")
        new_btn.connect("clicked", self.on_new_folder)
        hb.pack_end(new_btn)
        self.new_btn = new_btn

        self.view_toggle = Gtk.ToggleButton(label="List")
        self.view_toggle.set_tooltip_text("Switch between grid and list view")
        self.view_toggle.connect("toggled", self.on_view_toggled)
        hb.pack_end(self.view_toggle)

        self.hidden_toggle = Gtk.ToggleButton(label="Hidden")
        self.hidden_toggle.set_tooltip_text("Show hidden files")
        self.hidden_toggle.connect("toggled", self.on_hidden_toggled)
        hb.pack_end(self.hidden_toggle)

        # ---- toolbar row: breadcrumbs + search ----------------------
        toolbar = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        toolbar.set_margin_top(8)
        toolbar.set_margin_bottom(8)
        toolbar.set_margin_start(12)
        toolbar.set_margin_end(12)

        crumb_scroll = Gtk.ScrolledWindow()
        crumb_scroll.set_policy(Gtk.PolicyType.AUTOMATIC,
                                Gtk.PolicyType.NEVER)
        crumb_scroll.set_shadow_type(Gtk.ShadowType.NONE)
        self.crumb_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                                 spacing=2)
        crumb_scroll.add(self.crumb_box)
        toolbar.pack_start(crumb_scroll, True, True, 0)

        self.search = Gtk.SearchEntry()
        self.search.set_placeholder_text("Search this folder…")
        self.search.set_width_chars(24)
        self.search.connect("changed", self.on_search_changed)
        toolbar.pack_end(self.search, False, False, 0)

        # ---- body: sidebar + views ----------------------------------
        body = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)

        # Sidebar (places that exist on disk only)
        self.sidebar = Gtk.ListBox()
        self.sidebar.get_style_context().add_class("sidebar")
        self.sidebar.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self.sidebar.connect("row-selected", self.on_place_selected)
        side_scroll = Gtk.ScrolledWindow()
        side_scroll.set_policy(Gtk.PolicyType.NEVER,
                               Gtk.PolicyType.AUTOMATIC)
        side_scroll.set_size_request(200, -1)
        side_scroll.add(self.sidebar)
        body.pack_start(side_scroll, False, False, 0)
        self._build_sidebar()

        sep = Gtk.Separator(orientation=Gtk.Orientation.VERTICAL)
        body.pack_start(sep, False, False, 0)

        # Stack: grid view / list view
        self.stack = Gtk.Stack()
        self.stack.set_transition_type(
            Gtk.StackTransitionType.CROSSFADE)
        body.pack_start(self.stack, True, True, 0)

        # Grid view
        grid_scroll = Gtk.ScrolledWindow()
        grid_scroll.set_policy(Gtk.PolicyType.AUTOMATIC,
                               Gtk.PolicyType.AUTOMATIC)
        self.flow = Gtk.FlowBox()
        self.flow.set_selection_mode(Gtk.SelectionMode.MULTIPLE)
        self.flow.set_homogeneous(True)
        # valign START: without this the FlowBox stretches to fill
        # the whole scrolled window and a selected child draws a
        # giant full-height highlight pillar (seen in vile-files-04
        # before this fix). Content height only, like a real grid.
        self.flow.set_valign(Gtk.Align.START)
        self.flow.set_max_children_per_line(8)
        self.flow.set_min_children_per_line(3)
        self.flow.set_column_spacing(8)
        self.flow.set_row_spacing(8)
        self.flow.set_margin_top(12)
        self.flow.set_margin_bottom(12)
        self.flow.set_margin_start(12)
        self.flow.set_margin_end(12)
        self.flow.connect("selected-children-changed",
                          self.on_grid_selection)
        # NOTE: do NOT connect "child-activated" — this GTK build
        # fires it on a SINGLE click, so a click would open instead
        # of selecting. Activation is handled explicitly in
        # on_grid_button_press (real double-click) and on_key_press
        # (Enter on the selected item).
        grid_scroll.add(self.flow)
        self.stack.add_named(grid_scroll, "grid")

        # List view
        list_scroll = Gtk.ScrolledWindow()
        list_scroll.set_policy(Gtk.PolicyType.AUTOMATIC,
                               Gtk.PolicyType.AUTOMATIC)
        self.store = Gtk.ListStore(GdkPixbuf.Pixbuf, str, str, str,
                                   str, bool)  # icon, name, size, date, path, is_dir
        self.tree = Gtk.TreeView(model=self.store)
        self.tree.set_headers_visible(True)
        self.tree.get_selection().set_mode(Gtk.SelectionMode.MULTIPLE)
        self.tree.get_selection().connect("changed",
                                          self.on_list_selection)
        self.tree.connect("row-activated", self.on_list_activated)
        self.tree.connect("button-press-event", self.on_list_button_press)
        for i, (title, width) in enumerate(
                (("Name", 320), ("Size", 110), ("Modified", 170))):
            col = Gtk.TreeViewColumn(title)
            col.set_min_width(width)
            if i == 0:
                cell_icon = Gtk.CellRendererPixbuf()
                col.pack_start(cell_icon, False)
                col.add_attribute(cell_icon, "pixbuf", 0)
                cell_text = Gtk.CellRendererText()
                col.pack_start(cell_text, True)
                col.add_attribute(cell_text, "text", 1)
            else:
                cell = Gtk.CellRendererText()
                col.pack_start(cell, True)
                col.add_attribute(cell, "text", i + 1)
            col.set_resizable(True)
            self.tree.append_column(col)
        list_scroll.add(self.tree)
        self.stack.add_named(list_scroll, "list")

        # Devices ("This PC") page — one row per mounted drive, built
        # live by _refresh_devices().
        devices_scroll = Gtk.ScrolledWindow()
        devices_scroll.set_policy(Gtk.PolicyType.AUTOMATIC,
                                  Gtk.PolicyType.AUTOMATIC)
        self.devices_list = Gtk.ListBox()
        self.devices_list.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self.devices_list.connect("row-activated",
                                  self.on_device_activated)
        devices_scroll.add(self.devices_list)
        self.stack.add_named(devices_scroll, "devices")

        # ---- volume monitoring: live drive add/remove ---------------
        # Primary source: Gio.VolumeMonitor signals. The Gio unix
        # monitor only sees block-device mounts, so on systems without
        # any (this test VM: overlay/tmpfs only) it reports nothing —
        # _refresh_devices() therefore ALSO reads /proc/self/mounts
        # and a 1s poll re-checks it. Both paths refresh the same page.
        self._monitor = Gio.VolumeMonitor.get()
        for sig in ("volume-added", "volume-removed", "volume-changed",
                    "mount-added", "mount-removed", "mount-changed"):
            self._monitor.connect(sig, self._on_volumes_changed)
        GLib.timeout_add_seconds(1, self._poll_devices)

        # ---- status bar ----------------------------------------------
        self.status = Gtk.Label(label="")
        self.status.get_style_context().add_class("statusbar")
        self.status.set_halign(Gtk.Align.START)

        root = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        root.pack_start(toolbar, False, False, 0)
        root.pack_start(body, True, True, 0)
        root.pack_start(Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL),
                        False, False, 0)
        root.pack_start(self.status, False, False, 0)
        self.add(root)

        self.connect("key-press-event", self.on_key_press)

        self.navigate(self.current, record=False)

    # ---- sidebar ----------------------------------------------------
    def _build_sidebar(self):
        self._place_rows = {}  # path -> ListBoxRow
        for label, subdir in PLACES:
            path = place_path(label, subdir)
            row = self._sidebar_row(label, path)
            self._place_rows[path] = row
        # Separator, then This PC (the devices page).
        sep_row = Gtk.ListBoxRow()
        sep_row.set_selectable(False)
        sep_row.set_activatable(False)
        sep = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        sep.set_margin_top(8)
        sep.set_margin_bottom(8)
        sep_row.add(sep)
        self.sidebar.add(sep_row)
        self._devices_row = self._sidebar_row("This PC", None)
        self._devices_row._vile_devices = True
        self.sidebar.show_all()

    def _sidebar_row(self, label, path):
        row = Gtk.ListBoxRow()
        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        lbl = Gtk.Label(label=label)
        lbl.get_style_context().add_class("place-name")
        lbl.set_halign(Gtk.Align.START)
        box.pack_start(lbl, True, True, 0)
        row.add(box)
        row._vile_path = path
        row._vile_devices = False
        self.sidebar.add(row)
        return row

    def on_place_selected(self, _listbox, row):
        if row is None:
            return
        if getattr(row, "_vile_devices", False):
            self.show_devices()
            return
        self.navigate(row._vile_path)

    def _sync_sidebar(self):
        # Highlight the sidebar row for the current folder (or its
        # parent place) — or the This PC row in devices mode —
        # without re-triggering navigation.
        self.sidebar.handler_block_by_func(self.on_place_selected)
        if self.devices_mode:
            self.sidebar.select_row(self._devices_row)
        else:
            best = None
            for path, row in self._place_rows.items():
                if self.current == path or \
                        self.current.startswith(path + os.sep):
                    if best is None or len(path) > len(best[0]):
                        best = (path, row)
            if best:
                self.sidebar.select_row(best[1])
            else:
                self.sidebar.unselect_all()
        self.sidebar.handler_unblock_by_func(self.on_place_selected)

    # ---- listing ------------------------------------------------------
    def _read_entries(self):
        """Real scandir of the current folder, folders first then
        files, each alphabetical (case-insensitive)."""
        entries = []
        try:
            with os.scandir(self.current) as it:
                for e in it:
                    if not self.show_hidden and e.name.startswith("."):
                        continue
                    try:
                        st = e.stat(follow_symlinks=False)
                        is_dir = e.is_dir(follow_symlinks=False)
                    except OSError:
                        continue
                    entries.append({
                        "name": e.name, "path": e.path, "is_dir": is_dir,
                        "size": st.st_size, "mtime": st.st_mtime,
                    })
        except OSError as exc:
            self.status.set_text(f"Cannot open this folder: {exc}")
            return []
        entries.sort(key=lambda en: (not en["is_dir"],
                                     en["name"].lower()))
        return entries

    def _filtered(self):
        if not self.filter_text:
            return self._entries
        needle = self.filter_text
        return [en for en in self._entries
                if needle in en["name"].lower()]

    def refresh(self, select_path=None):
        self._entries = self._read_entries()
        shown = self._filtered()

        # Grid
        for child in self.flow.get_children():
            self.flow.remove(child)
        for en in shown:
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
            box.set_size_request(140, 132)
            kind = ("folder" if en["is_dir"] else
                    "archive" if is_archive(en["path"]) else "file")
            img = Gtk.Image.new_from_pixbuf(icon_pixbuf(kind, 64))
            box.pack_start(img, False, False, 0)
            name = Gtk.Label(label=en["name"])
            name.get_style_context().add_class("item-name")
            name.set_line_wrap(True)
            name.set_max_width_chars(16)
            name.set_ellipsize(3)  # PANGO_ELLIPSIZE_MIDDLE
            name.set_halign(Gtk.Align.CENTER)
            box.pack_start(name, False, False, 0)
            meta = Gtk.Label(
                label="Folder" if en["is_dir"]
                else f"{human_size(en['size'])} · {fmt_date(en['mtime'])}")
            meta.get_style_context().add_class("item-meta")
            meta.set_halign(Gtk.Align.CENTER)
            box.pack_start(meta, False, False, 0)
            ev = Gtk.EventBox()
            ev.add(box)
            ev._vile_path = en["path"]
            ev._vile_is_dir = en["is_dir"]
            ev.connect("button-press-event", self.on_grid_button_press)
            child = Gtk.FlowBoxChild()
            child.add(ev)
            child._vile_path = en["path"]
            self.flow.add(child)
        self.flow.show_all()

        # List
        self.store.clear()
        for en in shown:
            kind = ("folder" if en["is_dir"] else
                    "archive" if is_archive(en["path"]) else "file")
            self.store.append([
                icon_pixbuf(kind, 24),
                en["name"],
                "Folder" if en["is_dir"] else human_size(en["size"]),
                fmt_date(en["mtime"]),
                en["path"], en["is_dir"],
            ])

        self._update_status()
        if select_path:
            self.select_path(select_path)

    def _update_status(self):
        shown = self._filtered()
        total = len(self._entries)
        if self.filter_text:
            text = f"{len(shown)} of {total} items"
        else:
            text = f"{total} item{'s' if total != 1 else ''}"
        if self._selected_path:
            for en in self._entries:
                if en["path"] == self._selected_path:
                    if en["is_dir"]:
                        text += f"  •  {en['name']} (folder)"
                    else:
                        text += (f"  •  {en['name']} — "
                                 f"{human_size(en['size'])}")
                    break
        # Current drive's free space, Windows-style.
        free, total_bytes, mountpoint = self._drive_space(self.current)
        if total_bytes:
            text += (f"  •  {human_size(free)} free of "
                     f"{human_size(total_bytes)} on {mountpoint}")
        self.status.set_text(text)

    def _drive_space(self, path):
        """(free, total, mountpoint) for the drive holding `path`."""
        try:
            st = os.statvfs(path)
            total = st.f_blocks * st.f_frsize
            free = st.f_bavail * st.f_frsize
        except OSError:
            return 0, 0, ""
        return free, total, self._enclosing_mount(path)

    @staticmethod
    def _enclosing_mount(path):
        """The mount point of the filesystem holding `path`."""
        path = os.path.abspath(path)
        while not os.path.ismount(path):
            parent = os.path.dirname(path)
            if parent == path:
                return path
            path = parent
        return path

    def select_path(self, path):
        """Select `path` in whichever view is showing."""
        self._selected_path = path
        self._selected_paths = [path]
        if self.view_grid:
            for child in self.flow.get_children():
                if getattr(child, "_vile_path", None) == path:
                    self.flow.unselect_all()
                    self.flow.select_child(child)
                    break
        else:
            for row in self.store:
                if row[4] == path:
                    self.tree.get_selection().unselect_all()
                    self.tree.get_selection().select_iter(row.iter)
                    break
        self._update_status()

    def _select_only(self, path):
        """Replace the current selection with just `path` (used when
        right-clicking an item that is not already selected)."""
        self._selected_path = path
        self._selected_paths = [path]
        if self.view_grid:
            self.flow.unselect_all()
            for child in self.flow.get_children():
                if getattr(child, "_vile_path", None) == path:
                    self.flow.select_child(child)
                    break
        else:
            sel = self.tree.get_selection()
            sel.unselect_all()
            for row in self.store:
                if row[4] == path:
                    sel.select_iter(row.iter)
                    break
        self._update_status()

    # ---- navigation ---------------------------------------------------
    def navigate(self, path, record=True):
        path = os.path.abspath(path)
        if not os.path.isdir(path):
            self.status.set_text(f"Not a folder: {path}")
            return
        if self.devices_mode:
            self.devices_mode = False
            self._set_devices_controls(False)
        if record and path != self.current:
            self.back_stack.append(self.current)
            self.fwd_stack.clear()
        self.current = path
        self._selected_path = None
        self._selected_paths = []
        self._rebuild_crumbs()
        self.set_title(os.path.basename(path) or path)
        self.back_btn.set_sensitive(bool(self.back_stack))
        self.fwd_btn.set_sensitive(bool(self.fwd_stack))
        self.up_btn.set_sensitive(os.path.dirname(path) != path)
        self.refresh()
        self._sync_sidebar()

    def _set_devices_controls(self, devices_mode):
        # The file actions only make sense in a folder view.
        self.stack.set_visible_child_name(
            "devices" if devices_mode else
            ("list" if not self.view_grid else "grid"))
        self.new_btn.set_sensitive(not devices_mode)
        self.search.set_sensitive(not devices_mode)
        self.view_toggle.set_sensitive(not devices_mode)
        self.hidden_toggle.set_sensitive(not devices_mode)
        self.back_btn.set_sensitive(not devices_mode and bool(self.back_stack))
        self.fwd_btn.set_sensitive(not devices_mode and bool(self.fwd_stack))
        self.up_btn.set_sensitive(
            not devices_mode and os.path.dirname(self.current) != self.current)

    def show_devices(self):
        """Open the This PC page (not a folder — a drive list)."""
        self.devices_mode = True
        self._set_devices_controls(True)
        self.set_title("This PC")
        self._rebuild_crumbs()
        self._refresh_devices()
        self._sync_sidebar()

    # ---- drives ("This PC") ------------------------------------------
    @staticmethod
    def _is_interesting_mount(point, fstype):
        """Is this /proc mount a drive a user would expect to see?

        Windows-style: the root volume, a separate home volume,
        hand-mounted locations (/mnt, /media, /run/media — where USB
        sticks land) and whole top-level volumes (/data, …). System
        mounts are hidden: /boot and /boot/efi, /proc, /sys, /dev,
        /tmp, /run, and anything nested deep under a system tree
        (/opt/hatch/bin, /var/cache/… — container bind mounts of
        the runtime image, not drives). The filesystem type does
        not decide — a tmpfs drive mounted at /mnt is still a drive.
        """
        if point == "/":
            return True
        if point in ("/boot", "/boot/efi"):
            return False
        if fstype in ("sysfs", "proc", "devpts", "cgroup", "cgroup2",
                      "mqueue", "debugfs", "securityfs", "pstore",
                      "configfs", "autofs"):
            return False
        parts = [p for p in point.split("/") if p]
        if len(parts) == 1:
            # A whole top-level volume — except the system trees.
            return parts[0] not in ("dev", "proc", "sys", "run", "tmp",
                                    "etc", "var", "usr", "lib", "lib64",
                                    "bin", "sbin", "root", "boot")
        if point == "/home" or (len(parts) == 2 and parts[0] == "home"):
            return True
        if point in ("/mnt", "/media", "/run/media"):
            return True
        if point.startswith(("/mnt/", "/media/", "/run/media/")):
            return True
        return False

    def _list_drives(self):
        """Every mounted drive, as dicts.

        Two sources, merged by mount point (the monitor wins when
        both see the same mount):
          A. Gio.VolumeMonitor — the live source on real hardware.
          B. /proc/self/mounts — the fallback that also covers
             filesystems the Gio unix monitor skips (overlay, tmpfs,
             anything without a block device — i.e. this whole test
             VM, where source A is empty).
        Plus unmounted volumes from the monitor (greyed rows).
        Sizes come from os.statvfs on the mount point — real numbers.
        """
        by_mount = {}

        try:
            for m in self._monitor.get_mounts():
                root = m.get_root()
                point = root.get_path()
                if not point:
                    continue
                vol = m.get_volume()
                drive = vol.get_drive() if vol else None
                free, total, _mp = self._drive_space(point)
                by_mount[point] = {
                    "name": m.get_name() or
                            os.path.basename(point) or point,
                    "mount": point, "fstype": "",
                    "total": total, "free": free,
                    "removable": bool(drive and drive.is_removable()),
                    "mounted": True, "volume": vol,
                }
        except Exception:
            pass

        try:
            with open("/proc/self/mounts", "r") as f:
                mount_lines = f.readlines()
        except OSError:
            mount_lines = []
        for line in mount_lines:
            parts = line.split()
            if len(parts) < 3:
                continue
            device = parts[0]
            point = parts[1].replace("\\040", " ").replace("\\011", "\t")
            fstype = parts[2]
            if point in by_mount:
                if not by_mount[point]["fstype"]:
                    by_mount[point]["fstype"] = fstype
                # Same mount point twice (bind mounts in containers):
                # prefer the entry backed by a real block device.
                if (not by_mount[point].get("device", "").startswith("/dev")
                        and device.startswith("/dev")):
                    by_mount[point]["fstype"] = fstype
                    by_mount[point]["device"] = device
                continue
            if not self._is_interesting_mount(point, fstype):
                continue
            free, total, _mp = self._drive_space(point)
            by_mount[point] = {
                "name": "Local Disk" if point == "/"
                        else os.path.basename(point),
                "mount": point, "fstype": fstype,
                "total": total, "free": free,
                "removable": False,
                "mounted": True, "volume": None,
                "device": device,
            }

        try:
            for v in self._monitor.get_volumes():
                if v.get_mount() is not None:
                    continue
                key = "vol:" + (v.get_identifier("uuid") or
                                v.get_name() or "")
                by_mount.setdefault(key, {
                    "name": v.get_name() or "Removable drive",
                    "mount": "", "fstype": "",
                    "total": 0, "free": 0, "removable": True,
                    "mounted": False, "volume": v,
                })
        except Exception:
            pass

        drives = sorted(by_mount.values(),
                        key=lambda d: (d["mount"] != "/", d["mount"]))

        # One row per VOLUME, not per mount point. Bind mounts of a
        # directory inside an already-listed volume share its st_dev
        # (this container's /opt/hatch/* and /var/* binds, /tmp's
        # duplicate entry, …) — they are the same drive, not new
        # ones. "/" goes first, then parents before children, so a
        # bind never displaces its volume. Two different tmpfs
        # mounts have different st_devs, so a newly mounted drive
        # still shows up.
        def _st_dev(point):
            try:
                return os.stat(point).st_dev
            except OSError:
                return None

        ordered = sorted(drives,
                         key=lambda d: (d["mount"] != "/",
                                        len(d["mount"])))
        seen_devs = set()
        unique = []
        for d in ordered:
            dev = _st_dev(d["mount"]) if d["mounted"] else None
            if dev is not None and dev in seen_devs:
                continue
            if dev is not None:
                seen_devs.add(dev)
            unique.append(d)
        return unique

    def _refresh_devices(self):
        drives = self._list_drives()
        self._devices_sig = tuple(
            (d["mount"], d["mounted"], d["total"], d["free"])
            for d in drives)
        for row in self.devices_list.get_children():
            self.devices_list.remove(row)
        if not drives:
            row = Gtk.ListBoxRow()
            row.set_selectable(False)
            lbl = Gtk.Label(label="No drives found.")
            lbl.get_style_context().add_class("muted")
            row.add(lbl)
            self.devices_list.add(row)
        for d in drives:
            row = Gtk.ListBoxRow()
            row.get_style_context().add_class("drive-row")
            row._vile_drive = d
            hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                           spacing=16)
            img = Gtk.Image.new_from_pixbuf(icon_pixbuf("drive", 48))
            hbox.pack_start(img, False, False, 0)
            vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL,
                           spacing=2)
            name = Gtk.Label(label=d["name"])
            name.get_style_context().add_class("drive-name")
            name.set_halign(Gtk.Align.START)
            vbox.pack_start(name, False, False, 0)
            if d["mounted"]:
                sub = f"{d['mount']}  ·  {d['fstype'] or 'filesystem'}"
                if d["removable"]:
                    sub += "  ·  Removable"
            else:
                sub = "Not mounted — click to mount"
            sub_lbl = Gtk.Label(label=sub)
            sub_lbl.get_style_context().add_class("drive-sub")
            sub_lbl.set_halign(Gtk.Align.START)
            vbox.pack_start(sub_lbl, False, False, 0)
            hbox.pack_start(vbox, True, True, 0)
            if d["mounted"] and d["total"]:
                used = d["total"] - d["free"]
                pct = used / d["total"]
                rbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL,
                               spacing=4)
                free_lbl = Gtk.Label(
                    label=f"{human_size(d['free'])} free of "
                          f"{human_size(d['total'])}")
                free_lbl.get_style_context().add_class("drive-free")
                free_lbl.set_halign(Gtk.Align.END)
                rbox.pack_start(free_lbl, False, False, 0)
                bar = Gtk.ProgressBar()
                bar.set_fraction(pct)
                bar.set_size_request(240, -1)
                bar.set_show_text(False)
                if pct >= 0.9:
                    bar.get_style_context().add_class("full")
                elif pct >= 0.75:
                    bar.get_style_context().add_class("warn")
                rbox.pack_start(bar, False, False, 0)
                pct_lbl = Gtk.Label(label=f"{pct * 100:.0f}% used")
                pct_lbl.get_style_context().add_class("drive-sub")
                pct_lbl.set_halign(Gtk.Align.END)
                rbox.pack_start(pct_lbl, False, False, 0)
                hbox.pack_start(rbox, False, False, 0)
            if not d["mounted"]:
                row.get_style_context().add_class("unmounted")
            row.add(hbox)
            self.devices_list.add(row)
        self.devices_list.show_all()
        self.status.set_text(
            f"{len(drives)} drive{'s' if len(drives) != 1 else ''}")

    def _on_volumes_changed(self, _monitor, *_args):
        # Gio signals arrive on the main loop already.
        if self.devices_mode:
            self._refresh_devices()

    def _poll_devices(self):
        # 1s backstop for mounts the VolumeMonitor does not see
        # (and for the fallback /proc/self/mounts source): re-render
        # the page only when the drive set actually changed.
        if self.devices_mode:
            drives = self._list_drives()
            sig = tuple((d["mount"], d["mounted"], d["total"], d["free"])
                        for d in drives)
            if sig != self._devices_sig:
                self._refresh_devices()
        return True

    def on_device_activated(self, _listbox, row):
        if row is None:
            return
        d = getattr(row, "_vile_drive", None)
        if not d:
            return
        if d["mounted"] and d["mount"]:
            self.navigate(d["mount"])
        elif d["volume"] is not None:
            self._mount_volume(d["volume"])

    def _mount_volume(self, volume):
        try:
            volume.mount(Gio.MountOperation.new(), None,
                         self._on_volume_mounted, None)
        except GLib.Error as exc:
            self.status.set_text(f"Could not mount: {exc.message}")

    def _on_volume_mounted(self, volume, result, _data):
        try:
            volume.mount_finish(result)
        except GLib.Error as exc:
            self.status.set_text(f"Could not mount: {exc.message}")
            return
        self._refresh_devices()

    def show_devices(self):
        """Open the This PC page (not a folder — a drive list)."""
        self.devices_mode = True
        self._set_devices_controls(True)
        self.set_title("This PC")
        self._rebuild_crumbs()
        self._refresh_devices()
        self._sync_sidebar()

    def _rebuild_crumbs(self):
        for child in self.crumb_box.get_children():
            self.crumb_box.remove(child)
        if self.devices_mode:
            lbl = Gtk.Label(label="This PC")
            lbl.get_style_context().add_class("place-name")
            self.crumb_box.pack_start(lbl, False, False, 0)
            self.crumb_box.show_all()
            return
        home = os.path.expanduser("~")
        if self.current == home:
            parts = [("Home", home)]
        elif self.current.startswith(home + os.sep):
            rel = os.path.relpath(self.current, home)
            parts = [("Home", home)]
            acc = home
            for p in rel.split(os.sep):
                acc = os.path.join(acc, p)
                parts.append((p, acc))
        else:
            parts = [("/", "/")]
            acc = "/"
            for p in self.current.split(os.sep):
                if not p:
                    continue
                acc = os.path.join(acc, p)
                parts.append((p, acc))
        for i, (label, path) in enumerate(parts):
            if i:
                sep = Gtk.Label(label="›")
                sep.get_style_context().add_class("muted")
                self.crumb_box.pack_start(sep, False, False, 0)
            btn = Gtk.Button(label=label)
            btn.get_style_context().add_class("crumb")
            btn.connect("clicked", lambda _b, p=path: self.navigate(p))
            self.crumb_box.pack_start(btn, False, False, 0)
        self.crumb_box.show_all()

    def on_back(self, _btn):
        if self.back_stack:
            self.fwd_stack.append(self.current)
            self.current = self.back_stack.pop()
            self._selected_path = None
            self._selected_paths = []
            self._rebuild_crumbs()
            self.set_title(os.path.basename(self.current) or self.current)
            self.back_btn.set_sensitive(bool(self.back_stack))
            self.fwd_btn.set_sensitive(bool(self.fwd_stack))
            self.up_btn.set_sensitive(
                os.path.dirname(self.current) != self.current)
            self.refresh()
            self._sync_sidebar()

    def on_forward(self, _btn):
        if self.fwd_stack:
            self.back_stack.append(self.current)
            self.current = self.fwd_stack.pop()
            self._selected_path = None
            self._selected_paths = []
            self._rebuild_crumbs()
            self.set_title(os.path.basename(self.current) or self.current)
            self.back_btn.set_sensitive(bool(self.back_stack))
            self.fwd_btn.set_sensitive(bool(self.fwd_stack))
            self.up_btn.set_sensitive(
                os.path.dirname(self.current) != self.current)
            self.refresh()
            self._sync_sidebar()

    def on_up(self, _btn):
        parent = os.path.dirname(self.current)
        if parent != self.current:
            self.navigate(parent)

    # ---- opening ------------------------------------------------------
    def open_entry(self, path, is_dir):
        if is_dir:
            self.navigate(path)
            return
        try:
            subprocess.Popen(["xdg-open", path],
                             stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
        except FileNotFoundError:
            self.status.set_text("No application available to open files "
                                 "(xdg-open not found).")

    def on_grid_activated(self, _flow, child):
        ev = child.get_child()
        self.open_entry(ev._vile_path, ev._vile_is_dir)

    def on_grid_button_press(self, ev, event):
        if event.button == 3:  # right-click → context menu
            # Standard behaviour: right-clicking an item outside the
            # current selection selects just it; inside keeps the
            # whole selection (so Compress can take several items).
            if ev._vile_path not in self._selected_paths:
                self._select_only(ev._vile_path)
            self._popup_menu(event, ev._vile_path)
            return True
        if event.button == 1 and \
                event.type == Gdk.EventType._2BUTTON_PRESS:
            # Real double-click: open. A single click only selects;
            # it must NOT open.
            self.open_entry(ev._vile_path, ev._vile_is_dir)
            return True
        if event.button == 1 and \
                event.type == Gdk.EventType.BUTTON_PRESS:
            # Single click: Explorer behaviour — plain click selects
            # ONLY this item. This GTK build's FlowBox in MULTIPLE
            # mode ADDS on a plain click instead, so a click on Many
            # after Pack.zip was selected left Pack.zip selected and
            # it got zipped too (caught by the v1.2 harness: Bundle
            # held 304 files, Pack.zip among them). Ctrl+click still
            # toggles (FlowBox handles that — return False).
            if event.state & Gdk.ModifierType.CONTROL_MASK:
                return False
            self._select_only(ev._vile_path)
            # FlowBox's own click handler also focuses the clicked
            # CHILD — arrow-key navigation is driven by the focused
            # child, not the FlowBox. We consumed the event, so do
            # it here: focusing the FlowBox itself does NOT restore
            # arrows (caught by the v1.2 harness — Ctrl+Right /
            # Ctrl+Space after a click went nowhere and the
            # multi-select collapsed to [Pack]).
            ev.get_parent().grab_focus()
            return True
        return False

    def on_list_activated(self, _tree, path, _col):
        row = self.store[path]
        self.open_entry(row[4], row[5])

    def on_list_button_press(self, _tree, event):
        if event.button != 3:
            return False
        info = self.tree.get_path_at_pos(int(event.x), int(event.y))
        if not info:
            return False
        path, _col, _x, _y = info
        row = self.store[path]
        if row[4] not in self._selected_paths:
            self._select_only(row[4])
        self._popup_menu(event, row[4])
        return True

    def on_grid_selection(self, _flow):
        children = self.flow.get_selected_children()
        self._selected_paths = [c.get_child()._vile_path
                                for c in children]
        self._selected_path = (self._selected_paths[0]
                               if self._selected_paths else None)
        self._update_status()

    def on_list_selection(self, selection):
        _model, rows = selection.get_selected_rows()
        self._selected_paths = [self.store[p][4] for p in rows]
        self._selected_path = (self._selected_paths[0]
                               if self._selected_paths else None)
        self._update_status()

    # ---- search / view / hidden ---------------------------------------
    def on_search_changed(self, entry):
        self.filter_text = entry.get_text().strip().lower()
        self.refresh()

    def on_view_toggled(self, btn):
        self.view_grid = not btn.get_active()
        self.view_toggle.set_label("Grid" if not self.view_grid else "List")
        self.stack.set_visible_child_name(
            "list" if not self.view_grid else "grid")
        self.refresh()

    def on_hidden_toggled(self, btn):
        self.show_hidden = btn.get_active()
        self.refresh()

    # ---- file ops ------------------------------------------------------
    def _ask_name_dialog(self, title, initial=""):
        dialog = Gtk.Dialog(title=title, parent=self, modal=True)
        dialog.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                           Gtk.STOCK_OK, Gtk.ResponseType.OK)
        dialog.set_default_response(Gtk.ResponseType.OK)
        entry = Gtk.Entry()
        entry.set_text(initial)
        entry.set_width_chars(32)
        # Enter in the entry confirms (activates-default) as well as
        # the explicit activate handler — a real user expects Enter
        # to work in a name dialog.
        entry.set_activates_default(True)
        entry.connect("activate",
                      lambda _e: dialog.response(Gtk.ResponseType.OK))
        box = dialog.get_content_area()
        box.set_margin_top(12)
        box.set_margin_bottom(12)
        box.set_margin_start(12)
        box.set_margin_end(12)
        box.add(entry)
        dialog.show_all()
        response = dialog.run()
        name = entry.get_text().strip()
        dialog.destroy()
        return name if response == Gtk.ResponseType.OK else None

    def on_new_folder(self, _btn):
        name = self._ask_name_dialog("New Folder", "Untitled Folder")
        if not name:
            return
        if "/" in name or name in (".", ".."):
            self.status.set_text("That is not a valid folder name.")
            return
        dest = os.path.join(self.current, name)
        if os.path.exists(dest):
            self.status.set_text(f"'{name}' already exists here.")
            return
        try:
            os.mkdir(dest)
        except OSError as exc:
            self.status.set_text(f"Could not create folder: {exc}")
            return
        self.refresh(select_path=dest)

    def _rename(self, path):
        name = self._ask_name_dialog("Rename",
                                     os.path.basename(path))
        if not name or name == os.path.basename(path):
            return
        if "/" in name or name in (".", ".."):
            self.status.set_text("That is not a valid name.")
            return
        dest = os.path.join(os.path.dirname(path), name)
        if os.path.exists(dest):
            self.status.set_text(f"'{name}' already exists here.")
            return
        try:
            os.rename(path, dest)
        except OSError as exc:
            self.status.set_text(f"Could not rename: {exc}")
            return
        self.refresh(select_path=dest)

    def _trash(self, path):
        name = os.path.basename(path)
        dialog = Gtk.MessageDialog(
            parent=self, modal=True,
            message_type=Gtk.MessageType.QUESTION,
            buttons=Gtk.ButtonsType.NONE,
            text=f"Move '{name}' to Trash?")
        dialog.format_secondary_text(
            "It can be restored from the trash — nothing is deleted "
            "permanently.")
        dialog.add_button("Cancel", Gtk.ResponseType.CANCEL)
        dialog.add_button("Move to Trash", Gtk.ResponseType.OK)
        dialog.set_default_response(Gtk.ResponseType.CANCEL)
        response = dialog.run()
        dialog.destroy()
        if response != Gtk.ResponseType.OK:
            return
        try:
            # Gio trash — moves to the freedesktop trash, never rm.
            Gio.File.new_for_path(path).trash(None)
        except GLib.Error as exc:
            self.status.set_text(f"Could not move to trash: {exc.message}")
            return
        self._selected_path = None
        self.refresh()
        self.status.set_text(f"'{name}' moved to Trash.")

    # ---- keyboard ------------------------------------------------------
    def on_key_press(self, _widget, event):
        key = Gdk.keyval_name(event.keyval)
        ctrl = event.state & Gdk.ModifierType.CONTROL_MASK
        shift = event.state & Gdk.ModifierType.SHIFT_MASK
        if ctrl and key in ("f", "F"):
            self.search.grab_focus()
            return True
        if ctrl and shift and key in ("n", "N"):
            self.on_new_folder(None)
            return True
        if key in ("Return", "KP_Enter") and not self.search.has_focus():
            # Enter opens the selected item (the grid has no
            # child-activated handler — see the note above).
            if self._selected_path:
                is_dir = os.path.isdir(self._selected_path)
                self.open_entry(self._selected_path, is_dir)
                return True
            return False
        if key == "Delete" and self._selected_path:
            self._trash(self._selected_path)
            return True
        if key == "F2" and self._selected_path:
            self._rename(self._selected_path)
            return True
        return False

    # ---- zip / unzip (v1.2) -----------------------------------------
    def _popup_menu(self, event, path):
        menu = Gtk.Menu()
        paths = list(self._selected_paths) or [path]
        compress_item = Gtk.MenuItem(label="Compress to ZIP")
        compress_item.connect("activate",
                              lambda _i: self._compress(paths))
        menu.append(compress_item)
        if len(paths) == 1 and not os.path.isdir(path) and \
                is_archive(path):
            here_item = Gtk.MenuItem(label="Extract Here")
            here_item.connect("activate",
                              lambda _i: self._extract_here(path))
            menu.append(here_item)
            to_item = Gtk.MenuItem(label="Extract to…")
            to_item.connect("activate",
                            lambda _i: self._extract_to(path))
            menu.append(to_item)
        menu.append(Gtk.SeparatorMenuItem())
        rename_item = Gtk.MenuItem(label="Rename…")
        rename_item.connect("activate", lambda _i: self._rename(path))
        menu.append(rename_item)
        trash_item = Gtk.MenuItem(label="Move to Trash")
        trash_item.connect("activate", lambda _i: self._trash(path))
        menu.append(trash_item)
        menu.show_all()
        # popup_at_pointer (not the deprecated menu.popup): on
        # Wayland a menu without a transient parent fails to
        # position — Gdk-CRITICAL "temporary window without
        # parent" — and sometimes never appears at all. The
        # triggering button event carries the right parent.
        menu.popup_at_pointer(event)

    def _error_dialog(self, title, message):
        dialog = Gtk.MessageDialog(
            parent=self, modal=True, message_type=Gtk.MessageType.ERROR,
            buttons=Gtk.ButtonsType.OK, text=title)
        dialog.format_secondary_text(message)
        dialog.run()
        dialog.destroy()

    def _start_job(self, title, worker, on_done):
        """Run `worker` in a background thread with a progress
        dialog; the UI never freezes. worker(is_cancelled, report)
        returns (ok, message); on_done(ok, message) runs on the main
        thread after the dialog closes. report(text, fraction) is
        safe to call from the worker thread."""
        dlg = _ProgressDialog(self, title)

        def run():
            try:
                ok, msg = worker(
                    lambda: dlg.cancelled,
                    lambda text, frac: GLib.idle_add(
                        dlg.update, text, frac))
            except _Cancelled:
                ok, msg = False, "cancelled"
            except Exception as exc:  # corrupt archive, I/O error, …
                ok, msg = False, str(exc)
            GLib.idle_add(_done, ok, msg)

        def _done(ok, msg):
            dlg.close()
            on_done(ok, msg)
            return False

        threading.Thread(target=run, daemon=True).start()

    def _compress(self, paths):
        paths = [p for p in paths if os.path.exists(p)]
        if not paths:
            self.status.set_text("Nothing to compress.")
            return
        if len(paths) == 1:
            src = paths[0]
            if os.path.isdir(src):
                stem = os.path.basename(src.rstrip(os.sep))
            else:
                stem = os.path.splitext(os.path.basename(src))[0]
            target = unique_path(self.current, stem + ".zip")
        else:
            # Several items: ask for a name (default "Archive").
            name = self._ask_name_dialog("Compress to ZIP", "Archive")
            if not name:
                return
            if not name.lower().endswith(".zip"):
                name += ".zip"
            target = unique_path(self.current, name)
        self._start_job(f"Compressing to {os.path.basename(target)}…",
                        lambda cancel, report: _zip_worker(
                            paths, target, cancel, report),
                        lambda ok, msg: self._on_compress_done(
                            ok, msg, target))

    def _on_compress_done(self, ok, msg, target):
        if ok:
            self.refresh(select_path=target)
            self.status.set_text(msg)
        elif msg == "cancelled":
            self.status.set_text("Compression cancelled — nothing "
                                 "was created.")
        else:
            self._error_dialog("Could not create the ZIP file.", msg)

    def _extract_here(self, archive):
        dest_parent = os.path.dirname(archive)
        target = unique_path(dest_parent, archive_stem(archive))
        self._extract(archive, target)

    def _extract_to(self, archive):
        dialog = Gtk.FileChooserDialog(
            title="Extract to…", parent=self,
            action=Gtk.FileChooserAction.SELECT_FOLDER)
        dialog.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                           "Extract", Gtk.ResponseType.OK)
        dialog.set_current_folder(self.current)
        response = dialog.run()
        folder = dialog.get_filename() if response == Gtk.ResponseType.OK \
            else None
        dialog.destroy()
        if not folder:
            return
        target = unique_path(folder, archive_stem(archive))
        self._extract(archive, target)

    def _extract(self, archive, target):
        name = os.path.basename(archive)
        self._start_job(f"Extracting {name}…",
                        lambda cancel, report: _extract_worker(
                            archive, target, cancel, report),
                        lambda ok, msg: self._on_extract_done(
                            ok, msg, target))

    def _on_extract_done(self, ok, msg, target):
        if ok:
            # Show the extracted folder: navigate into its parent
            # if we are elsewhere, then select it.
            parent = os.path.dirname(target)
            if os.path.abspath(parent) != os.path.abspath(self.current):
                self.navigate(parent, record=False)
            self.refresh(select_path=target)
            self.status.set_text(msg)
        elif msg == "cancelled":
            self.status.set_text("Extraction cancelled — nothing "
                                 "was extracted.")
        else:
            # A failed extract leaves nothing behind.
            shutil.rmtree(target, ignore_errors=True)
            self._error_dialog("Could not extract the archive.", msg)


class _Cancelled(Exception):
    """Raised inside an archive worker when the user hits Cancel."""


def _slow_zip_tick():
    """Test-only hook: VILE_FILES_SLOW_ZIP=1 pauses the worker
    briefly per file so the progress dialog stays visible long
    enough to screenshot. Never set in the shipped launcher."""
    if os.environ.get("VILE_FILES_SLOW_ZIP"):
        time.sleep(0.05)


def _zip_worker(sources, target, is_cancelled, report):
    """Write `sources` into `target` as a .zip (Deflate, relative
    paths, folder structure preserved). Runs in a worker thread.
    Returns (True, status message). Raises on error; the partial
    .zip is removed by the caller."""
    target_abs = os.path.abspath(target)
    entries = []  # (disk path, archive name, is_dir)
    for src in sources:
        base = os.path.basename(src.rstrip(os.sep))
        if os.path.isdir(src):
            for root, dirs, files in os.walk(src):
                dirs.sort()
                rel_root = os.path.relpath(root, os.path.dirname(src))
                entries.append((root, rel_root + "/", True))
                for fn in sorted(files):
                    fp = os.path.join(root, fn)
                    entries.append(
                        (fp, os.path.join(rel_root, fn), False))
        else:
            entries.append((src, base, False))
    # Never zip the archive into itself.
    entries = [e for e in entries
               if os.path.abspath(e[0]) != target_abs]
    files = [e for e in entries if not e[2]]
    total_bytes = sum(os.path.getsize(p) for p, _a, _d in files)
    done_bytes = 0
    done_files = 0
    try:
        with zipfile.ZipFile(target, "w",
                             zipfile.ZIP_DEFLATED) as zf:
            for path, arc, is_dir in entries:
                if is_cancelled():
                    raise _Cancelled()
                if is_dir:
                    zf.write(path, arc)
                else:
                    zf.write(path, arc)
                    done_bytes += os.path.getsize(path)
                    done_files += 1
                report(f"{done_files} of {len(files)} files",
                       done_bytes / total_bytes if total_bytes else 1)
                _slow_zip_tick()
    except Exception:
        try:
            os.remove(target)
        except OSError:
            pass
        raise
    return True, (f"Created {os.path.basename(target)} — "
                  f"{len(files)} files, {human_size(total_bytes)}")


def _safe_join(base, name):
    """The path `name` would land at inside `base` — or raise
    ValueError. Blocks zip-slip (absolute paths, '..' escaping the
    target folder) for zip and tar members alike."""
    if os.path.isabs(name):
        raise ValueError(f"Unsafe path in archive: {name}")
    base_real = os.path.realpath(base)
    target = os.path.realpath(os.path.join(base, name))
    if target != base_real and \
            not target.startswith(base_real + os.sep):
        raise ValueError(f"Unsafe path in archive: {name}")
    return target


def _extract_worker(archive, target, is_cancelled, report):
    """Extract `archive` into a fresh folder `target`. Validates
    every member name BEFORE extracting anything (zip-slip), and
    any failure — corrupt archive, password-protected member,
    I/O error — raises; the caller removes the partial folder, so
    a failed extract leaves nothing behind. Runs in a thread."""
    low = archive.lower()
    os.makedirs(target, exist_ok=True)
    try:
        if low.endswith(".zip"):
            with zipfile.ZipFile(archive) as zf:
                infos = zf.infolist()
                for info in infos:
                    _safe_join(target, info.filename)
                for i, info in enumerate(infos):
                    if is_cancelled():
                        raise _Cancelled()
                    report(f"Extracting {info.filename}",
                           i / len(infos) if infos else 1)
                    try:
                        zf.extract(info, target)
                    except RuntimeError as exc:
                        # Encrypted member without a password.
                        raise ValueError(
                            "This archive is password-protected; "
                            "Vile Files cannot open it.") from exc
                    _slow_zip_tick()
                count = len([i for i in infos if not i.is_dir()])
        elif low.endswith((".tar.gz", ".tgz")):
            with tarfile.open(archive, "r:*") as tf:
                members = tf.getmembers()
                for m in members:
                    _safe_join(target, m.name)
                for i, m in enumerate(members):
                    if is_cancelled():
                        raise _Cancelled()
                    report(f"Extracting {m.name}",
                           i / len(members) if members else 1)
                    # filter="data" (Python 3.12+) is the tar
                    # equivalent of the zip-slip check above.
                    tf.extract(m, target, filter="data")
                    _slow_zip_tick()
                count = len([m for m in members if m.isfile()])
        else:
            raise ValueError(f"Not a supported archive: "
                             f"{os.path.basename(archive)}")
    except Exception:
        shutil.rmtree(target, ignore_errors=True)
        raise
    return True, (f"Extracted {count} files to "
                  f"{os.path.basename(target)}")


class _ProgressDialog:
    """Modal progress dialog for archive jobs. Shown (not run())
    so the main loop keeps running while the worker thread works;
    Cancel only sets a flag the worker checks between files."""

    def __init__(self, parent, title):
        self.cancelled = False
        self.dialog = Gtk.Dialog(title=title, parent=parent, modal=True)
        self.dialog.set_default_size(380, 130)
        self.label = Gtk.Label(label="Starting…")
        self.label.set_halign(Gtk.Align.START)
        self.bar = Gtk.ProgressBar()
        self.bar.set_show_text(True)
        box = self.dialog.get_content_area()
        box.set_margin_top(16)
        box.set_margin_bottom(16)
        box.set_margin_start(16)
        box.set_margin_end(16)
        box.set_spacing(12)
        box.add(self.label)
        box.add(self.bar)
        self.dialog.add_button("Cancel", Gtk.ResponseType.CANCEL)
        self.dialog.connect("response", self._on_response)
        self.dialog.show_all()

    def _on_response(self, _dialog, _response):
        self.cancelled = True

    def update(self, text, fraction):
        self.label.set_text(text)
        self.bar.set_fraction(max(0.0, min(1.0, fraction)))
        self.bar.set_text(f"{int(fraction * 100)}%")
        return False

    def close(self):
        self.dialog.destroy()


def main():
    parser = argparse.ArgumentParser(description="Vile Files")
    parser.add_argument("--path", metavar="DIR", default=None,
                        help="open at DIR instead of the home folder")
    args = parser.parse_args()

    start = args.path or os.path.expanduser("~")
    if not os.path.isdir(start):
        print(f"Vile Files: not a folder: {start}", file=sys.stderr)
        sys.exit(1)

    Gtk.Settings.get_default().set_property(
        "gtk-application-prefer-dark-theme", True)

    ensure_standard_dirs()  # first run: create ~/Desktop, ~/Documents, …

    win = FilesWindow(start)
    win.connect("destroy", Gtk.main_quit)
    win.show_all()
    Gtk.main()


if __name__ == "__main__":
    main()
