"""Unit tests for src/xrandr.py (parsing + transform math).

Run from the repo root with:
    python3 -m unittest discover -s tests -v
or:
    python3 tests/test_xrandr.py

No display needed - this only tests the pure-Python logic.
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))

import xrandr as xr

# Real xrandr output from a Dell Latitude 7400 + 32" Westinghouse TV
# over HDMI (1920x1080 @ 60 Hz, 708mm x 398mm).
SAMPLE_XRANDR = """\
Screen 0: minimum 320 x 200, current 1920 x 1080, maximum 16384 x 16384
DP-1 disconnected (normal left inverted right x axis y axis)
HDMI-1 connected primary 1920x1080+0+0 (normal left inverted right x axis y axis) 708mm x 398mm
   4096x2160      30.00    25.00    24.00    29.97    23.98
   3840x2160      30.00    25.00    24.00    29.97    23.98
   2288x1430      61.01
   1920x1080      60.00*   59.94    30.00    24.00    29.97    23.98
   1920x1080i     60.00    59.94
   1280x720       60.00    59.94
   1440x480i      59.94
   720x480        60.00    59.94
   720x480i       60.00    59.94
   640x480        60.00    59.94
DP-2 disconnected (normal left inverted right x axis y axis)
HDMI-2 disconnected (normal left inverted right x axis y axis)
"""


class TestParseXrandr(unittest.TestCase):
    def setUp(self):
        self.displays = xr.parse_xrandr_output(SAMPLE_XRANDR)
        self.by_name = {d.name: d for d in self.displays}

    def test_all_outputs_found(self):
        self.assertEqual(
            [d.name for d in self.displays],
            ["DP-1", "HDMI-1", "DP-2", "HDMI-2"])

    def test_hdmi1_connected_primary(self):
        hdmi = self.by_name["HDMI-1"]
        self.assertTrue(hdmi.connected)
        self.assertTrue(hdmi.primary)
        self.assertEqual(hdmi.current, (1920, 1080))
        self.assertEqual(hdmi.refresh, 60.00)
        self.assertEqual(hdmi.physical_mm, (708, 398))

    def test_hdmi1_modes(self):
        hdmi = self.by_name["HDMI-1"]
        names = [m.name for m in hdmi.modes]
        for expected in ("4096x2160", "3840x2160", "2288x1430",
                         "1920x1080", "1920x1080i", "1280x720",
                         "640x480"):
            self.assertIn(expected, names)
        current_modes = [m for m in hdmi.modes if m.current]
        self.assertEqual(len(current_modes), 1)
        self.assertEqual(current_modes[0].name, "1920x1080")
        self.assertEqual(current_modes[0].refresh, 60.00)
        self.assertEqual(hdmi.current_mode().name, "1920x1080")

    def test_disconnected_have_no_modes(self):
        for name in ("DP-1", "DP-2", "HDMI-2"):
            display = self.by_name[name]
            self.assertFalse(display.connected)
            self.assertEqual(display.modes, [])
            self.assertIsNone(display.current)


class TestTransformMath(unittest.TestCase):
    def test_five_percent_on_1080p(self):
        # The headline case: 1920x1080 at 0.95 -> offsets 48 and 27.
        self.assertEqual(
            xr.transform_string(1920, 1080, 0.95),
            "0.95,0,48,0,0.95,27,0,0,1")
        values = xr.compute_transform(1920, 1080, 0.95)
        self.assertEqual(values[2], 48.0)
        self.assertEqual(values[5], 27.0)

    def test_no_shrink_is_identity(self):
        self.assertEqual(
            xr.transform_string(1920, 1080, 1.0),
            "1,0,0,0,1,0,0,0,1")

    def test_ten_percent_on_1080p(self):
        self.assertEqual(
            xr.transform_string(1920, 1080, 0.90),
            "0.9,0,96,0,0.9,54,0,0,1")

    def test_fractional_offsets(self):
        # 1366x768 at 0.95: 1366*0.05/2 = 34.15, 768*0.05/2 = 19.2
        self.assertEqual(
            xr.transform_string(1366, 768, 0.95),
            "0.95,0,34.15,0,0.95,19.2,0,0,1")

    def test_transform_command(self):
        self.assertEqual(
            xr.build_transform_command("HDMI-1", 1920, 1080, 0.95),
            ["xrandr", "--output", "HDMI-1", "--transform",
             "0.95,0,48,0,0.95,27,0,0,1"])

    def test_separate_horizontal_vertical(self):
        # Intel-style per-axis shrink: H 5% (0.95) and V 10% (0.90)
        # on 1920x1080 -> offsets 48 (H) and 54 (V).
        self.assertEqual(
            xr.transform_string(1920, 1080, 0.95, 0.90),
            "0.95,0,48,0,0.9,54,0,0,1")
        values = xr.compute_transform(1920, 1080, 0.95, 0.90)
        self.assertEqual(values[0], 0.95)
        self.assertEqual(values[2], 48.0)
        self.assertEqual(values[4], 0.90)
        self.assertEqual(values[5], 54.0)

    def test_separate_axis_command(self):
        self.assertEqual(
            xr.build_transform_command("HDMI-1", 1920, 1080, 0.95, 0.90),
            ["xrandr", "--output", "HDMI-1", "--transform",
             "0.95,0,48,0,0.9,54,0,0,1"])

    def test_negative_shrink_expands(self):
        # -10% (expand / zoom in) is scale 1.10: 1920x1080 gives
        # offsets -96 (horizontal) and -54 (vertical).
        self.assertEqual(
            xr.transform_string(1920, 1080, 1.10),
            "1.1,0,-96,0,1.1,-54,0,0,1")
        values = xr.compute_transform(1920, 1080, 1.10)
        self.assertEqual(values[2], -96.0)
        self.assertEqual(values[5], -54.0)

    def test_uniform_falls_back_to_single_scale(self):
        # One scale argument still gives a uniform shrink.
        self.assertEqual(
            xr.transform_string(1920, 1080, 0.95),
            xr.transform_string(1920, 1080, 0.95, 0.95))


class TestPreviewMath(unittest.TestCase):
    def test_percent_to_scale(self):
        self.assertEqual(xr.percent_to_scale(0), 1.0)
        self.assertEqual(xr.percent_to_scale(5), 0.95)
        self.assertEqual(xr.percent_to_scale(10), 0.90)
        self.assertEqual(xr.percent_to_scale(-10), 1.10)
        self.assertEqual(xr.percent_to_scale(-5), 1.05)

    def test_preview_rect_no_change(self):
        self.assertEqual(xr.preview_rect(480, 270, 1.0, 1.0),
                         (0.0, 0.0, 480.0, 270.0))

    def test_preview_rect_shrunk(self):
        # 480x270 at 0.95/0.95: 456x256.5, centred -> (12, 6.75).
        self.assertEqual(xr.preview_rect(480, 270, 0.95, 0.95),
                         (12.0, 6.75, 456.0, 256.5))

    def test_preview_rect_expanded(self):
        # 480x270 at 1.10/1.10: 528x297, centred -> negative offsets
        # (edges clipped, like an expanded picture on the display).
        self.assertEqual(xr.preview_rect(480, 270, 1.10, 1.10),
                         (-24.0, -13.5, 528.0, 297.0))

    def test_preview_rect_mixed_axes(self):
        # H 0.95, V 0.90 on 480x270: 456x243 -> (12, 13.5).
        self.assertEqual(xr.preview_rect(480, 270, 0.95, 0.90),
                         (12.0, 13.5, 456.0, 243.0))

    def test_resolution_and_reset_commands(self):
        self.assertEqual(
            xr.build_resolution_command("HDMI-1", "1280x720"),
            ["xrandr", "--output", "HDMI-1", "--mode", "1280x720"])
        self.assertEqual(
            xr.build_reset_command("HDMI-1"),
            ["xrandr", "--output", "HDMI-1", "--transform",
             "1,0,0,0,1,0,0,0,1"])


class TestPreviewGeometry(unittest.TestCase):
    """fit_box / scaled_rect / panel_rect: the mini-desktop geometry
    used by the preview (v0.4.0)."""

    def test_fit_box_exact_match(self):
        # 480x270 widget, 1920x1080 screen: same 16:9 aspect, exact fit.
        self.assertEqual(xr.fit_box(480, 270, 1920, 1080),
                         (0.0, 0.0, 480.0, 270.0))

    def test_fit_box_taller_widget(self):
        # Widget taller than 16:9: width is the limit, centred vertically.
        self.assertEqual(xr.fit_box(480, 300, 1920, 1080),
                         (0.0, 15.0, 480.0, 270.0))

    def test_fit_box_wider_widget(self):
        # Widget wider than 16:9: height is the limit, centred horizontally.
        self.assertEqual(xr.fit_box(600, 270, 1920, 1080),
                         (60.0, 0.0, 480.0, 270.0))

    def test_fit_box_4x3_resolution(self):
        # 1024x768 (4:3) in a 480x360 widget: exact fit.
        self.assertEqual(xr.fit_box(480, 360, 1024, 768),
                         (0.0, 0.0, 480.0, 360.0))
        # Same 4:3 screen in a 16:9 widget: height is the limit.
        self.assertEqual(xr.fit_box(480, 270, 1280, 1024),
                         (71.25, 0.0, 337.5, 270.0))

    def test_scaled_rect_shrunk(self):
        # Same centring as preview_rect, but offset by the box origin.
        self.assertEqual(xr.scaled_rect(0, 0, 480, 270, 0.95, 0.95),
                         (12.0, 6.75, 456.0, 256.5))

    def test_scaled_rect_expanded_with_origin(self):
        # Box at (60, 0), expanded 1.10: desktop is 528x297, centred
        # on the box -> top-left (36, -13.5), edges clipped.
        self.assertEqual(xr.scaled_rect(60, 0, 480, 270, 1.10, 1.10),
                         (36.0, -13.5, 528.0, 297.0))

    def test_scaled_rect_mixed_axes_with_origin(self):
        self.assertEqual(xr.scaled_rect(0, 15, 480, 270, 0.95, 0.90),
                         (12.0, 28.5, 456.0, 243.0))

    def test_panel_rect_1080p(self):
        # Cinnamon panel ~40px tall at 1080p: 270 * 40/1080 = 10px in
        # a 480x270 mini-desktop, sitting at the bottom.
        self.assertEqual(xr.panel_rect(0, 0, 480, 270, 1080),
                         (0.0, 260.0, 480.0, 10.0))

    def test_panel_rect_shrunk_desktop(self):
        # Shrunk desktop (12, 6.75, 456, 256.5): panel is 9.5px tall.
        self.assertEqual(xr.panel_rect(12, 6.75, 456, 256.5, 1080),
                         (12.0, 253.75, 456.0, 9.5))

    def test_panel_rect_lower_resolution(self):
        # Same 40px panel is a bigger fraction at 720p: 15px tall.
        self.assertEqual(xr.panel_rect(0, 0, 480, 270, 720),
                         (0.0, 255.0, 480.0, 15.0))

    def test_panel_rect_custom_height(self):
        self.assertEqual(xr.panel_rect(0, 0, 480, 270, 1080, panel_px=48),
                         (0.0, 258.0, 480.0, 12.0))


class TestAutostart(unittest.TestCase):
    def test_autostart_content(self):
        content = xr.autostart_content("HDMI-1", 1920, 1080, 0.95)
        self.assertIn("[Desktop Entry]", content)
        self.assertIn("Type=Application", content)
        self.assertIn(
            "Exec=xrandr --output HDMI-1 --transform "
            "0.95,0,48,0,0.95,27,0,0,1",
            content)
        self.assertIn("X-GNOME-Autostart-enabled=true", content)

    def test_autostart_filename(self):
        self.assertEqual(xr.AUTOSTART_NAME, "screenfit-overscan.desktop")
        self.assertTrue(
            xr.autostart_path().endswith("screenfit-overscan.desktop"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
