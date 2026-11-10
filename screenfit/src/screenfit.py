#!/usr/bin/env python3
"""ScreenFit - a simple display settings GUI for Linux.

A community clone of the Intel Graphics Command Center display page,
focused on resolution and overscan (fit-to-screen) for TVs and monitors.
Not affiliated with Intel.

Runs as a normal user - xrandr works without root.
"""

import os
import sys

# Make the sibling xrandr.py importable no matter where this file lives.
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import xrandr as xr

import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk

try:
    import cairo
    _HAVE_CAIRO = True
except ImportError:
    # pycairo is optional: the preview gradient degrades to a solid
    # fill and the font constants fall back to their numeric values.
    cairo = None
    _HAVE_CAIRO = False

APP_NAME = "ScreenFit"
DEFAULT_SHRINK_PERCENT = 5.0  # sensible starting point for TV overscan


class ScreenFitWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title=APP_NAME)
        self.set_default_size(540, 780)
        self.set_border_width(14)

        self.displays = []
        self.current_display = None

        # --- layout -------------------------------------------------------
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.add(vbox)

        title = Gtk.Label()
        title.set_markup("<b>{0}</b>".format(APP_NAME))
        title.set_halign(Gtk.Align.START)
        vbox.pack_start(title, False, False, 0)

        subtitle = Gtk.Label(
            "Resolution and fit-to-screen (overscan) settings. "
            "No root needed - this just runs xrandr as your user.")
        subtitle.set_line_wrap(True)
        subtitle.set_halign(Gtk.Align.START)
        vbox.pack_start(subtitle, False, False, 0)

        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        grid.set_row_spacing(8)
        vbox.pack_start(grid, False, False, 0)

        # Display selector
        grid.attach(Gtk.Label(label="Display", halign=Gtk.Align.START),
                    0, 0, 1, 1)
        self.display_combo = Gtk.ComboBoxText()
        self.display_combo.connect("changed", self.on_display_changed)
        grid.attach(self.display_combo, 1, 0, 1, 1)

        # Info (read-only)
        grid.attach(Gtk.Label(label="Current", halign=Gtk.Align.START),
                    0, 1, 1, 1)
        self.info_label = Gtk.Label(label="-")
        self.info_label.set_halign(Gtk.Align.START)
        self.info_label.set_selectable(True)
        grid.attach(self.info_label, 1, 1, 1, 1)

        # Resolution picker
        grid.attach(Gtk.Label(label="Resolution", halign=Gtk.Align.START),
                    0, 2, 1, 1)
        self.res_combo = Gtk.ComboBoxText()
        self.res_combo.connect("changed", self.on_resolution_changed)
        grid.attach(self.res_combo, 1, 2, 1, 1)

        # Refresh rate (read-only for v1)
        grid.attach(Gtk.Label(label="Refresh rate", halign=Gtk.Align.START),
                    0, 3, 1, 1)
        self.rate_label = Gtk.Label(label="-")
        self.rate_label.set_halign(Gtk.Align.START)
        grid.attach(self.rate_label, 1, 3, 1, 1)

        # Overscan / fit-to-screen sliders (like Intel Command Center:
        # separate horizontal and vertical, so a TV that crops more on
        # one axis can be fixed per-axis).
        slider_title = Gtk.Label()
        slider_title.set_markup("<b>Fit to screen (overscan fix)</b>")
        slider_title.set_halign(Gtk.Align.START)
        vbox.pack_start(slider_title, False, False, 0)

        slider_help = Gtk.Label(
            "TVs crop a few percent of the picture on every edge, which "
            "hides the taskbar. Positive % shrinks the picture so the "
            "whole desktop fits. Negative % expands / zooms in, to fill "
            "the screen if there is a black border around it. "
            "-10% is the most you can expand, +10% the most you can shrink. "
            "Horizontal and vertical are independent. The preview below "
            "updates live as you move the sliders; the real screen only "
            "changes when you click Apply.")
        slider_help.set_line_wrap(True)
        slider_help.set_halign(Gtk.Align.START)
        vbox.pack_start(slider_help, False, False, 0)

        self.shrink_label = Gtk.Label()
        self.shrink_label.set_halign(Gtk.Align.START)
        vbox.pack_start(self.shrink_label, False, False, 0)

        h_label = Gtk.Label(label="Horizontal")
        h_label.set_halign(Gtk.Align.START)
        vbox.pack_start(h_label, False, False, 0)
        self.h_slider = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, -10.0, 10.0, 0.5)
        self.h_slider.set_value(DEFAULT_SHRINK_PERCENT)
        self.h_slider.set_draw_value(True)
        self.h_slider.set_digits(1)
        self.h_slider.connect("value-changed", self.on_slider_changed)
        vbox.pack_start(self.h_slider, False, False, 0)

        v_label = Gtk.Label(label="Vertical")
        v_label.set_halign(Gtk.Align.START)
        vbox.pack_start(v_label, False, False, 0)
        self.v_slider = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, -10.0, 10.0, 0.5)
        self.v_slider.set_value(DEFAULT_SHRINK_PERCENT)
        self.v_slider.set_draw_value(True)
        self.v_slider.set_digits(1)
        self.v_slider.connect("value-changed", self.on_slider_changed)
        vbox.pack_start(self.v_slider, False, False, 0)

        # Preview (centre of the window, like Intel's preview): a
        # miniature of the real desktop, drawn in code - no image file
        # needed. It has the aspect ratio of the selected resolution,
        # with a taskbar along the bottom, and it scales with the
        # sliders so you can see whether the taskbar will fit before
        # you click Apply. The real display does NOT change while you
        # drag the sliders - only the preview does.
        preview_label = Gtk.Label()
        preview_label.set_markup("<b>Preview - how your desktop will look</b>")
        preview_label.set_halign(Gtk.Align.START)
        vbox.pack_start(preview_label, False, False, 0)

        self.preview = Gtk.DrawingArea()
        self.preview.set_size_request(480, 270)
        self.preview.connect("draw", self.on_preview_draw)
        vbox.pack_start(self.preview, False, False, 0)

        # Buttons
        button_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                             spacing=8)
        vbox.pack_start(button_box, False, False, 0)

        self.apply_button = Gtk.Button(label="Apply")
        self.apply_button.connect("clicked", self.on_apply)
        button_box.pack_start(self.apply_button, True, True, 0)

        self.reset_button = Gtk.Button(label="Reset (no shrink)")
        self.reset_button.connect("clicked", self.on_reset)
        button_box.pack_start(self.reset_button, True, True, 0)

        # Autostart
        self.autostart_check = Gtk.CheckButton(
            label="Apply this overscan fix at login")
        self.autostart_check.connect("toggled", self.on_autostart_toggled)
        vbox.pack_start(self.autostart_check, False, False, 0)

        # Status line
        self.status_label = Gtk.Label(label="")
        self.status_label.set_line_wrap(True)
        self.status_label.set_halign(Gtk.Align.START)
        vbox.pack_start(self.status_label, False, False, 0)

        self.on_slider_changed(self.h_slider)
        self.refresh_displays()

    # --- data -----------------------------------------------------------

    def refresh_displays(self):
        """Re-read xrandr and fill the display selector."""
        try:
            self.displays = xr.get_displays()
        except FileNotFoundError:
            self.set_status(
                "xrandr not found. Please install it: "
                "sudo pacman -S xorg-xrandr")
            self.set_controls_sensitive(False)
            return
        except RuntimeError as exc:
            self.set_status("Could not read displays: {0}".format(exc))
            self.set_controls_sensitive(False)
            return

        connected = [d for d in self.displays if d.connected]
        self.display_combo.remove_all()
        if not connected:
            self.set_status("No connected display found.")
            self.set_controls_sensitive(False)
            return

        for display in connected:
            label = display.name
            if display.primary:
                label += " (primary)"
            self.display_combo.append_text(label)
        self.display_combo.set_active(0)
        self.set_controls_sensitive(True)
        # on_display_changed fires from set_active() and fills the rest.

    def set_controls_sensitive(self, sensitive):
        for widget in (self.display_combo, self.res_combo, self.h_slider,
                       self.v_slider, self.apply_button, self.reset_button,
                       self.autostart_check):
            widget.set_sensitive(sensitive)

    def set_status(self, message):
        self.status_label.set_text(message)

    # --- UI callbacks -----------------------------------------------------

    def on_display_changed(self, _combo):
        index = self.display_combo.get_active()
        connected = [d for d in self.displays if d.connected]
        if index < 0 or index >= len(connected):
            return
        self.current_display = connected[index]
        display = self.current_display

        # Info line
        info = "-"
        if display.current is not None:
            info = "{0}x{1}".format(*display.current)
            if display.refresh is not None:
                info += " @ {0:.2f} Hz".format(display.refresh)
            if display.physical_mm is not None:
                width_mm, height_mm = display.physical_mm
                info += "  ({0}mm x {1}mm)".format(width_mm, height_mm)
        self.info_label.set_text(info)

        # Refresh rate (read-only)
        if display.refresh is not None:
            self.rate_label.set_text("{0:.2f} Hz".format(display.refresh))
        else:
            self.rate_label.set_text("-")

        # Resolution list (de-duplicated, current mode first)
        self.res_combo.remove_all()
        seen = set()
        modes = sorted(display.modes,
                       key=lambda m: (m.width * m.height, m.rates[0]
                                      if m.rates else 0),
                       reverse=True)
        current_name = None
        current_mode = display.current_mode()
        if current_mode is not None:
            current_name = current_mode.name
        for mode in modes:
            if mode.name in seen:
                continue
            seen.add(mode.name)
            self.res_combo.append_text(mode.name)
        if current_name is not None:
            # Combo order matches `modes` order, de-duplicated.
            unique_names = []
            for mode in modes:
                if mode.name not in unique_names:
                    unique_names.append(mode.name)
            if current_name in unique_names:
                self.res_combo.set_active(unique_names.index(current_name))

        # Autostart checkbox reflects the saved file, if any.
        self.autostart_check.set_active(xr.autostart_exists())

        # The preview matches the selected resolution's aspect ratio.
        self.update_preview_size()

    def preview_resolution(self):
        """(width, height) the preview should match: the resolution
        selected in the dropdown, else the display's current one,
        else a 1920x1080 fallback."""
        if self.current_display is not None:
            selected = self.selected_resolution()
            if selected is not None:
                return selected[0], selected[1]
            if self.current_display.current is not None:
                return self.current_display.current
        return (1920, 1080)

    def update_preview_size(self):
        """Size the preview widget to the selected resolution's aspect
        ratio and redraw it."""
        preview = getattr(self, "preview", None)
        if preview is None:
            return
        res_w, res_h = self.preview_resolution()
        height = max(120, int(round(480 * res_h / float(res_w))))
        preview.set_size_request(480, height)
        preview.queue_draw()

    def on_resolution_changed(self, _combo):
        """Resolution picker changed: re-size and redraw the preview
        so its aspect ratio matches the newly selected resolution."""
        self.update_preview_size()

    def on_slider_changed(self, _slider):
        h_percent = self.h_slider.get_value()
        v_percent = self.v_slider.get_value()
        h_scale, v_scale = self.shrink_scales()
        self.shrink_label.set_text(
            "Horizontal: {0:+.1f}% (scale {1:.2f})    "
            "Vertical: {2:+.1f}% (scale {3:.2f})".format(
                h_percent, h_scale, v_percent, v_scale))

        # Preview only: redraw the mini-desktop live as the sliders
        # move. The real display does NOT change here - that happens
        # on Apply (or Reset). The autostart file is likewise only
        # touched by Apply / the checkbox, never by a slider move.
        preview = getattr(self, "preview", None)
        if preview is not None:
            preview.queue_draw()

    def shrink_scales(self):
        """(horizontal scale, vertical scale) from the two sliders."""
        return (xr.percent_to_scale(self.h_slider.get_value()),
                xr.percent_to_scale(self.v_slider.get_value()))

    def on_preview_draw(self, _widget, cr):
        """Draw a miniature of the real desktop, scaled like the real
        display will be after Apply.

        The mini-screen has the aspect ratio of the selected
        resolution, a blue desktop background (Cinnamon's wallpaper
        tone), and a taskbar along the bottom with a menu button and
        a few app icons - at the same proportions as a real Cinnamon
        panel (about 40px tall at 1080p). The whole mini-desktop is
        scaled by the Horizontal/Vertical factors and centred. Shrink
        it and dark border shows around it; expand it and its edges
        (including the taskbar) are clipped by the mini-screen, just
        like the picture on the real display. That is the point: you
        can see whether the taskbar will be visible before you click
        Apply.
        """
        alloc = self.preview.get_allocation()
        box_w, box_h = alloc.width, alloc.height
        scale_x, scale_y = self.shrink_scales()
        res_w, res_h = self.preview_resolution()

        # Outside the mini-screen: near-black surround.
        cr.set_source_rgb(0.05, 0.05, 0.08)
        cr.rectangle(0, 0, box_w, box_h)
        cr.fill()

        # The mini-screen, aspect-matched to the selected resolution.
        bx, by, bw, bh = xr.fit_box(box_w, box_h, res_w, res_h)
        # The mini-desktop inside it, scaled by the sliders, centred.
        dx, dy, dw, dh = xr.scaled_rect(bx, by, bw, bh, scale_x, scale_y)

        # Clip everything below to the mini-screen, so an expanded
        # desktop gets its edges cropped exactly like the real one.
        cr.save()
        cr.rectangle(bx, by, bw, bh)
        cr.clip()

        # Desktop background: Cinnamon-blue gradient.
        if _HAVE_CAIRO:
            grad = cairo.LinearGradient(dx, dy, dx, dy + dh)
            grad.add_color_stop_rgb(0, 0.15, 0.45, 0.85)
            grad.add_color_stop_rgb(1, 0.05, 0.20, 0.55)
            cr.set_source(grad)
        else:
            cr.set_source_rgb(0.10, 0.35, 0.75)
        cr.rectangle(dx, dy, dw, dh)
        cr.fill()

        # Soft wallpaper blob (a lighter translucent circle, like the
        # abstract shapes on Cinnamon's default wallpaper).
        cr.set_source_rgba(1, 1, 1, 0.08)
        cr.arc(dx + dw * 0.72, dy + dh * 0.30, dh * 0.32,
               0, 2 * 3.14159)
        cr.fill()

        # Taskbar / panel along the bottom of the mini-desktop.
        px, py, pw, ph = xr.panel_rect(dx, dy, dw, dh, res_h)
        cr.set_source_rgba(0.12, 0.12, 0.14, 0.95)
        cr.rectangle(px, py, pw, ph)
        cr.fill()
        # Thin light edge on top of the panel.
        cr.set_source_rgba(1, 1, 1, 0.12)
        cr.rectangle(px, py, pw, max(1.0, ph * 0.08))
        cr.fill()

        # Menu button (Cinnamon mint green) at the left of the panel.
        icon = ph * 0.72
        gap = ph * 0.28
        cx = px + gap
        cy = py + (ph - icon) / 2.0
        cr.set_source_rgb(0.25, 0.75, 0.45)
        cr.arc(cx + icon / 2.0, cy + icon / 2.0, icon / 2.0,
               0, 2 * 3.14159)
        cr.fill()

        # A few app icons: small coloured squares.
        app_colors = [(1.00, 0.44, 0.22),   # Firefox orange
                      (0.29, 0.56, 0.85),   # Files blue
                      (0.20, 0.60, 0.35),   # Terminal / software green
                      (0.85, 0.65, 0.15)]   # amber
        ix = cx + icon + gap
        for r, g, b in app_colors:
            if ix + icon > px + pw - gap:
                break
            cr.set_source_rgb(r, g, b)
            cr.rectangle(ix, cy, icon, icon)
            cr.fill()
            ix += icon + gap

        # Clock / system area on the right of the panel.
        cr.set_source_rgba(1, 1, 1, 0.75)
        clock_w = ph * 1.6
        cr.rectangle(px + pw - gap - clock_w, cy + icon * 0.2,
                     clock_w, icon * 0.6)
        cr.fill()

        cr.restore()

        # Thin border around the mini-screen so it reads as a screen.
        cr.set_source_rgba(1, 1, 1, 0.35)
        cr.set_line_width(1)
        cr.rectangle(bx + 0.5, by + 0.5, bw - 1, bh - 1)
        cr.stroke()
        return False

    def selected_resolution(self):
        """(width, height, mode_name) from the resolution combo."""
        text = self.res_combo.get_active_text()
        if not text:
            return None
        for mode in self.current_display.modes:
            if mode.name == text:
                return mode.width, mode.height, mode.name
        return None

    def run_and_report(self, argv, success_message):
        """Run an xrandr command and report the result in the status line."""
        try:
            rc, _out, err = xr.run_command(argv)
        except FileNotFoundError:
            self.set_status("xrandr not found. Please install xorg-xrandr.")
            return False
        if rc != 0:
            self.set_status("Failed: {0}".format(err.strip() or
                                                 "xrandr exited with "
                                                 "code {0}".format(rc)))
            return False
        self.set_status(success_message)
        return True

    def on_apply(self, _button):
        if self.current_display is None:
            return
        display = self.current_display
        selected = self.selected_resolution()
        if selected is None:
            self.set_status("Pick a resolution first.")
            return
        width, height, mode_name = selected
        h_shrink = self.h_slider.get_value()
        v_shrink = self.v_slider.get_value()
        scale_x, scale_y = self.shrink_scales()

        # Change resolution first (only if it differs from current).
        if display.current != (width, height):
            if not self.run_and_report(
                    xr.build_resolution_command(display.name, mode_name),
                    ""):
                return
            display.current = (width, height)

        # Then apply the overscan transform for the (possibly new) size.
        ok = self.run_and_report(
            xr.build_transform_command(display.name, width, height,
                                       scale_x, scale_y),
            "Applied {0}x{1} with H {2:.1f}% / V {3:.1f}% shrink.".format(
                width, height, h_shrink, v_shrink))
        if not ok:
            return

        # Keep the autostart file in sync if it is enabled.
        if self.autostart_check.get_active():
            xr.write_autostart(display.name, width, height,
                               scale_x, scale_y)
            self.set_status(
                "Applied {0}x{1} with H {2:.1f}% / V {3:.1f}% shrink. "
                "Autostart updated.".format(width, height, h_shrink,
                                           v_shrink))

    def on_reset(self, _button):
        if self.current_display is None:
            return
        self.h_slider.set_value(0.0)
        self.v_slider.set_value(0.0)
        ok = self.run_and_report(
            xr.build_reset_command(self.current_display.name),
            "Overscan fix removed (no shrink).")
        if ok and self.autostart_check.get_active():
            xr.remove_autostart()
            self.autostart_check.set_active(False)

    def on_autostart_toggled(self, check):
        if self.current_display is None:
            return
        if check.get_active():
            selected = self.selected_resolution()
            if selected is None:
                check.set_active(False)
                self.set_status("Pick a resolution first.")
                return
            width, height, _mode_name = selected
            scale_x, scale_y = self.shrink_scales()
            path = xr.write_autostart(self.current_display.name, width,
                                      height, scale_x, scale_y)
            self.set_status("Will apply at login (saved to {0}).".format(
                path))
        else:
            xr.remove_autostart()
            self.set_status("Autostart removed.")


def main():
    window = ScreenFitWindow()
    window.connect("destroy", Gtk.main_quit)
    window.show_all()
    Gtk.main()


if __name__ == "__main__":
    main()
