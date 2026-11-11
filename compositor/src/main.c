/* =============================================================================
 * Vile compositor — Piece 1: backend + renderer + output + wallpaper
 *                  Piece 2: xdg-shell windows (first window: foot terminal)
 *                  Piece 3: full input — keyboard, pointer/cursor, focus,
 *                  keybinds (Super+Enter/Q/L/M)
 * =============================================================================
 * A NEW Wayland compositor built on wlroots 0.19, written from scratch for
 * Vile OS. No Hyprland code, no Hyprland package as the compositor.
 *
 * Piece 1 does exactly four things, and nothing else yet:
 *   1. Backend  — wlroots backend (DRM on real hardware, headless or X11
 *      when WLR_BACKENDS says so). Talks to the screen / input hardware.
 *   2. Renderer — wlroots renderer (GLES2 on hardware, pixman headless).
 *      Turns the scene graph into pixels.
 *   3. Output   — one output, 1920x1080. On a headless backend we ask for
 *      a custom 1920x1080 mode so the test screenshot matches the wallpaper
 *      1:1. On real hardware we use the monitor's preferred mode.
 *   4. Wallpaper — Adnan's default screen (purple Arch logo, dark bg) is
 *      decoded from JPEG, painted into a buffer, and put in the scene graph
 *      as the bottom-most node, full-screen. The compositor NEVER shows a
 *      black screen or a stock wallpaper on boot.
 *
 * Piece 2 adds:
 *   5. xdg-shell — the protocol clients use to create application windows.
 *      A new toplevel gets a scene tree, is configured to 960x600, centred
 *      on the output when mapped, raised above the wallpaper, and marked
 *      activated. Popups (menus) are hosted the same way.
 *   6. Startup command — `-s <cmd>` (or VILE_STARTUP) is forked after the
 *      backend starts, with WAYLAND_DISPLAY set. Piece 2 uses it to launch
 *      `foot`, the first client window.
 *
 * Piece 3 adds (structure follows tinywl, the wlroots minimal example;
 * the code itself is written fresh for Vile):
 *   7. Keyboard — libinput keyboards via new_input, US keymap, keys are
 *      delivered to the focused window through the seat; focus follows
 *      the most recently mapped / clicked window.
 *   8. Pointer — libinput pointers + virtual pointers, aggregated by
 *      wlr_cursor; visible cursor (xcursor "default" theme, rendered
 *      into the frame by the scene output as a software cursor, so it
 *      shows in grim screenshots too); click focuses + raises a window,
 *      click on empty space clears focus.
 *   9. Keybinds (Super = Windows key): Super+Enter opens foot,
 *      Super+Q closes the focused window, Super+L locks now (reuses
 *      the lock overlay), Super+M or Super+Shift+Q exits cleanly.
 *
 * Piece 4 adds:
 *  10. Tiling — master-stack layout: 1 window fills the area, 2 sit
 *      side by side, 3+ split the right half as a stack. Focused
 *      window gets a purple #6D4AFF border, others a dim border.
 *      Super+J/K or Super+Left/Right moves focus between tiles,
 *      Super+F toggles fullscreen on the focused window. Closing a
 *      window re-tiles the rest. Wallpaper stays full-screen behind
 *      everything; ~/.config/vile/wallpaper (plain text path) picks
 *      the wallpaper file, so Settings can switch it later.
 *      Follow-up: Super+Escape clears keyboard focus — a tiled
 *      desktop leaves almost no clickable wallpaper (only the
 *      12px outer frame and the 4px wallpaper strips in the
 *      gaps), so unfocus needs a keybind as well as a click.
 *      The click path still works on those gaps (outer frame). 
 * Piece 7 adds (Nov 10, 2026 — Adnan: make Vile FEEL better):
 *  11. Animations — window open (fade + slight scale-up) and close
 *      (fade-out of a snapshot of the last frame), focus-border
 *      cross-fade, dock hover grow + click pop, system menu rise,
 *      lock fade with a small clock drift, startup fade/slide/pop.
 *      Ease-out cubic, ~150-250ms, driven by the output frame loop
 *      at 60fps; wall-clock progress, so a slow frame just finishes
 *      an animation early (never blocks input). VILE_ANIM_OFF=1
 *      disables them (instant); VILE_ANIM_DURATION_MS=N overrides
 *      the duration (slow-motion, for screenshots).
 *  12. First-run Setup wizard — if ~/.config/vile/setup-done is
 *      missing, the wizard (setup/vile-setup, GTK3, same dark
 *      style as Settings) opens FIRST, before the bare desktop;
 *      the startup command is skipped so the wizard is the only
 *      window. VILE_NO_SETUP=1 skips the wizard (headless tests).
 *
 * Env vars (Piece 7):
 *   VILE_ANIM_OFF          1 = animations instant (off)
 *   VILE_ANIM_DURATION_MS  override animation length in ms
 *   VILE_NO_SETUP          1 = never auto-open the Setup wizard
 *
 *   - The screencopy manager IS enabled already, so `grim` can capture a
 *     real screenshot of what the compositor presents.
 *
 * How it was built (for Adnan — this is the "how did Hyprland start" answer
 * in code form): wlroots does the hard low-level work (DRM/KMS, libinput,
 * EGL, Wayland protocols). We write the compositor logic on top: create a
 * wl_display, autocreate a backend + renderer + allocator, build a scene
 * graph, add an output, add our wallpaper node to the scene, start the
 * backend, run the event loop. That is the whole job at this level.
 *
 * Lock screen (queued after Piece 2): `-l` / VILE_LOCK=1 starts locked.
 * Huge clock above the password field; the typed password is checked
 * with real PAM (service "vile", see pam.d/vile) — no hardcoded
 * password. VILE_LOCK_USER overrides which user is checked (test only).
 *
 * Env vars:
 *   VILE_WALLPAPER         path to the wallpaper JPEG (default: tries
 *                          ../wallpapers/vile-purple-arch.jpg, then
 *                          ~/.config/vile/wallpapers/vile-purple-arch.jpg)
 *   ~/.config/vile/wallpaper
 *                          plain-text file with the wallpaper path —
 *                          Settings writes this to switch wallpapers
 *                          (Piece 4 hook; VILE_WALLPAPER wins over it)
 *   VILE_QUIT_AFTER_SEC    exit after N seconds (0 = run forever)
 *   VILE_SOCKET_FILE       if set, write the Wayland socket name here
 *   VILE_STARTUP           command to run once the compositor is up
 *                          (same as -s; e.g. "foot")
 *   VILE_LOCK              1 = start with the lock screen on
 *   VILE_LOCK_USER         user the lock screen checks (default: you)
 *   WLR_BACKENDS           passed through to wlroots (headless/drm/x11)
 *   WLR_RENDERER           passed through (pixman/gles2/vulkan)
 *   WLR_HEADLESS_OUTPUTS   passed through (number of headless outputs)
 *
 * Build (Arch):  meson setup build && ninja -C build
 *   needs: wlroots, wayland-server, libdrm, pixman, pangocairo, cairo,
 *   xkbcommon, pam
 * Run (test):    ./run-headless.sh   (headless + grim screenshot)
 * Run (real):    ./build/vile -s foot   (DRM, from a TTY, as your user)
 * Run (locked):  ./build/vile -l -s foot
 * =============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define WLR_USE_UNSTABLE

#include <assert.h>
#include <cairo.h>
#include <dirent.h>
#include <drm_fourcc.h>
#include <getopt.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strncasecmp: dock find is case-insensitive */
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

/* stb_image: single-header JPEG/PNG decoder (public domain, Sean Barrett).
 * Only used to decode the wallpaper file into RGBA pixels. */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* wlr_renderer_get_render_formats is exported by libwlroots but only
 * declared in wlroots' private headers, so we declare it here. */
extern const struct wlr_drm_format_set *wlr_renderer_get_render_formats(
		struct wlr_renderer *renderer);

/* Forward declarations: the lock screen functions are defined in the
 * lock section, but the output/map handlers (above it) call them. */
struct vile_server;
struct vile_toplevel;
static void vile_lock_render(struct vile_server *server);
static void vile_lock_raise(struct vile_server *server);
static void vile_lock_fit(struct vile_server *server, int out_w, int out_h);
static void vile_lock_destroy(struct vile_server *server);
static void vile_lock_now(struct vile_server *server);
static void vile_lock_click(struct vile_server *server, double lx, double ly);
static void vile_focus_toplevel(struct vile_server *server,
	struct vile_toplevel *toplevel);
static void vile_spawn(const char *cmd);
static const char *vile_wallpaper_path(void);
static void vile_ensure_default_wallpaper(void);
static bool vile_theme_path(char *buf, size_t len);
static bool vile_read_theme(void);
static void vile_apply_theme(struct vile_server *server, bool light);
static void vile_lock_reload_bg(struct vile_server *server);
static void vile_tile(struct vile_server *server);
static void vile_update_borders(struct vile_server *server);
/* Interactive move/resize + minimize/maximize (0.5.4 post-theme
 * fixes): the helpers live next to the xdg request handlers, but
 * focus/cursor code above them calls them. */
static void vile_unminimize(struct vile_server *server,
	struct vile_toplevel *t);
static void vile_unmaximize(struct vile_server *server,
	struct vile_toplevel *t);
static void vile_grab_begin_move(struct vile_server *server,
	struct vile_toplevel *t);
static void vile_grab_begin_resize(struct vile_server *server,
	struct vile_toplevel *t, uint32_t edges);
static void vile_grab_end(struct vile_server *server);
static void vile_grab_update(struct vile_server *server);
/* Pending grab (click-vs-drag): armed on press in the cursor code
 * above the grab section, fired from the motion path. */
static void vile_grab_arm(struct vile_server *server,
	struct vile_toplevel *t, int mode, uint32_t edges);
static void vile_grab_disarm(struct vile_server *server);
static bool vile_grab_maybe_fire(struct vile_server *server);
/* Piece 5 — Vile look (bottom bar, dock). The huge desktop clock
 * was removed Nov 10, 2026 — the bar centre carries the small
 * clock now; the lock clock is separate and untouched. */
static void vile_shell_create(struct vile_server *server, int out_w, int out_h);
static void vile_shell_fit(struct vile_server *server, int out_w, int out_h);
static void vile_shell_raise(struct vile_server *server);
static void vile_shell_update(struct vile_server *server);
static bool vile_shell_click(struct vile_server *server, double lx,
		double ly, uint32_t button);
static void vile_dock_release(struct vile_server *server);
static int vile_dock_slot_at(struct vile_server *server, int x, int y);
static void vile_shell_close_menu(struct vile_server *server);
static void vile_shell_set_menu(struct vile_server *server, bool open);
static int vile_shell_timer(void *data);
static void vile_shell_render_dock(struct vile_server *server);
/* Launch jump (0.5.6): the tick (defined before the dock section)
 * needs these. */
static void vile_shell_dock_launch_start(struct vile_server *server,
		int app);
static bool vile_dock_launch_appeared(struct vile_server *server,
		int app);
/* Piece 7 — animations + first-run Setup. The animation tick runs
 * in the output frame handler, which is defined before the animation
 * section itself, so it is forward-declared here. */
static bool vile_anim_tick(struct vile_server *server);
static void vile_anim_startup(struct vile_server *server);
static int64_t vile_anim_now_ms(void);
static bool vile_anim_enabled(void);
static bool vile_box_contains(struct wlr_box *b, int x, int y);
static void vile_spawn_setup(void);
static bool vile_setup_needed(void);

/* Launch jump (0.5.6 — KDE Plasma style): one up-down bounce every
 * 500ms until the app's window maps, or the 5s timeout. Defined
 * here (before the animation tick) because the tick, the dock
 * render, and the launch helpers all use them. */
#define VILE_LAUNCH_TIMEOUT_MS 5000
#define VILE_LAUNCH_JUMP_MS 500
#define VILE_SPIN_MAX 53 /* circle-spin loader frames on disk */

/* Vile design tokens (same as the rest of Vile OS). */
#define VILE_BG_R 0.059f   /* #0F0B1A */
#define VILE_BG_G 0.043f
#define VILE_BG_B 0.102f

/* Theme (0.5.4): light/dark through ONE palette struct. Every
 * shell colour in the compositor comes from here — never a bare
 * literal in a render function (accent #6D4AFF and the error red
 * are the same in both themes, so they stay literals). DARK is the
 * exact 0.5.3 tokens, so dark renders pixel-identical to 0.5.3.
 * LIGHT is lavender-white with the same purple accent; dark tiles
 * (dock icons, ScreenFit tile) stay dark in both — app-icon look. */
struct vile_palette {
	float bg[3];        /* letterbox behind the wallpaper */
	float text[3];      /* primary text: 0.93,0.91,0.98 */
	float text_hi[3];   /* near-white: 0.96,0.95,1.0 (lock clock) */
	float text2[3];     /* secondary: 0.72,0.69,0.82 */
	float text3[3];     /* bar date: 0.68,0.65,0.78 */
	float muted[3];     /* section labels: 0.55,0.52,0.62 */
	float hint[3];      /* lock hint: 0.54,0.50,0.66 */
	float status[3];    /* lock bottom status: 0.60,0.58,0.70 */
	float panel_val[3]; /* menu/panel values: 0.92,0.91,0.95 */
	float glyph[3];     /* status glyphs: 0.9,0.9,0.95 */
	float date_hi[3];   /* lock date: 0.79,0.75,0.95 */
	float close_x[3];   /* panel close x: 0.85,0.84,0.9 */
	float white[3];     /* solid white (password dots/knob): white */
	float edge[3];      /* hairlines: white in dark, dark in light */
	float panel_rgb[3]; /* card/pill rgb: 0.05,0.035,0.09 */
	float bar_rgb[3];   /* bar tint rgb: 0.06,0.04,0.11 */
	float field_rgb[3]; /* input field rgb: 0.04,0.03,0.08 */
	float dim_rgb[3];   /* lock dim rgb: 0.04,0.03,0.07 */
	float tile_rgb[3];  /* dark icon tiles: 0.09,0.07,0.16 */
	float border_dim[3];/* unfocused border: 0.25,0.24,0.33 */
	float shadow[4];    /* drop shadows: black 0.35 */
};
static const struct vile_palette VILE_PALETTE_DARK = {
	.bg = { 0.059f, 0.043f, 0.102f },
	.text = { 0.93f, 0.91f, 0.98f },
	.text_hi = { 0.96f, 0.95f, 1.0f },
	.text2 = { 0.72f, 0.69f, 0.82f },
	.text3 = { 0.68f, 0.65f, 0.78f },
	.muted = { 0.55f, 0.52f, 0.62f },
	.hint = { 0.54f, 0.50f, 0.66f },
	.status = { 0.60f, 0.58f, 0.70f },
	.panel_val = { 0.92f, 0.91f, 0.95f },
	.glyph = { 0.9f, 0.9f, 0.95f },
	.date_hi = { 0.79f, 0.75f, 0.95f },
	.close_x = { 0.85f, 0.84f, 0.9f },
	.white = { 1.0f, 1.0f, 1.0f },
	.edge = { 1.0f, 1.0f, 1.0f },
	.panel_rgb = { 0.05f, 0.035f, 0.09f },
	.bar_rgb = { 0.06f, 0.04f, 0.11f },
	.field_rgb = { 0.04f, 0.03f, 0.08f },
	.dim_rgb = { 0.04f, 0.03f, 0.07f },
	.tile_rgb = { 0.09f, 0.07f, 0.16f },
	.border_dim = { 0.25f, 0.24f, 0.33f },
	.shadow = { 0.0f, 0.0f, 0.0f, 0.35f },
};
static const struct vile_palette VILE_PALETTE_LIGHT = {
	.bg = { 0.949f, 0.941f, 0.980f }, /* #F2F0FA (was missing: bg was black) */
	.text = { 0.102f, 0.086f, 0.149f },
	.text_hi = { 0.102f, 0.086f, 0.149f },
	.text2 = { 0.420f, 0.396f, 0.502f },
	.text3 = { 0.420f, 0.396f, 0.502f },
	.muted = { 0.45f, 0.42f, 0.52f },
	.hint = { 0.45f, 0.42f, 0.52f },
	.status = { 0.42f, 0.40f, 0.50f },
	.panel_val = { 0.102f, 0.086f, 0.149f },
	.glyph = { 0.102f, 0.086f, 0.149f },
	.date_hi = { 0.42f, 0.40f, 0.50f },
	.close_x = { 0.102f, 0.086f, 0.149f },
	.white = { 0.102f, 0.086f, 0.149f },
	.edge = { 0.102f, 0.086f, 0.149f },
	.panel_rgb = { 0.988f, 0.980f, 1.0f },
	.bar_rgb = { 0.965f, 0.957f, 0.992f },
	.field_rgb = { 1.0f, 1.0f, 1.0f },
	/* dim_rgb is PREMULTIPLIED by the 0.30 dim alpha: wlroots renders
	 * scene rects as premultiplied on both pixman and GLES2, so a
	 * (1,1,1,0.30) dim composites as opaque white. (0.30,0.30,0.30)
	 * is the true 30% white wash. Dark keeps its shipped values. */
	.dim_rgb = { 0.30f, 0.30f, 0.30f },
	.tile_rgb = { 0.09f, 0.07f, 0.16f },
	.border_dim = { 0.72f, 0.71f, 0.78f },
	.shadow = { 0.0f, 0.0f, 0.0f, 0.18f },
};
/* Takes the flag, not the server: the palette accessor is defined
 * before struct vile_server, and a forward declaration is not
 * enough to dereference a member. */
static const struct vile_palette *vile_pal(bool light) {
	return light ? &VILE_PALETTE_LIGHT : &VILE_PALETTE_DARK;
}
#define VILE_OUT_W 1920
#define VILE_OUT_H 1080
/* Dock entries (Nov 10, 2026 ~16:31 — frosted dock (v3, Nov 10 ~19:52) + reorder +
 * unpin + Setup icon). The dock is an ORDERED list of app ids, not
 * four fixed slots. Defined here because the server struct below
 * sizes its dock arrays from it. */
#define VILE_DOCK_MAX 8
enum vile_dock_app {
	DOCK_FOOT = 0,
	DOCK_SETTINGS,
	DOCK_SCREENFIT,
	DOCK_FILES,
	DOCK_SETUP,
	DOCK_APP_COUNT,
};
static const char *vile_dock_app_names[DOCK_APP_COUNT] = {
	"foot", "settings", "screenfit", "files", "setup",
};
/* Piece 2 fallback window size, used only before the first tile
 * computation (e.g. if a client commits before any output exists).
 * Tiling (Piece 4) owns sizes once the window maps. */
#define VILE_WIN_W 960
#define VILE_WIN_H 600

struct vile_server {
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	struct wlr_output_layout *output_layout;

	struct wl_list outputs; /* struct vile_output.link */
	struct wl_listener new_output;

	/* Piece 2: xdg-shell (application windows). */
	struct wlr_xdg_shell *xdg_shell;
	struct wl_list toplevels; /* struct vile_toplevel.link */
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;

	/* Piece 2 seat: foot refuses to open a window at all if the
	 * compositor advertises no wl_seat ("no seats available").
	 * Piece 3 wires it up for real: keyboard + pointer capabilities,
	 * keys forwarded to the focused window, pointer focus + clicks. */
	struct wlr_seat *seat;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	/* Piece 3: pointer / cursor. wlr_cursor aggregates every pointer
	 * device (libinput mice + virtual pointers) into one cursor
	 * position; the xcursor manager supplies the visible arrow image.
	 * The scene output draws it as a software cursor into the frame,
	 * so it shows in grim screenshots too (headless has no hardware
	 * cursor plane). */
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;
	struct wlr_virtual_pointer_manager_v1 *vptr_manager;
	struct wl_listener new_virtual_pointer;
	/* Keyboard focus: the toplevel keys go to (NULL = none). */
	struct vile_toplevel *focused_toplevel;

	/* Piece 4: tiling — creation sequence (stable tile order, so
	 * focusing never reshuffles the layout) and the fullscreen
	 * window, if any. */
	uint64_t next_seq;
	struct vile_toplevel *fullscreen_toplevel;

	/* Interactive move/resize grab (0.5.4 post-theme fixes): one
	 * active pointer grab at a time. Started by a client request
	 * (title-bar drag / edge drag via xdg-shell), by a
	 * compositor-side title-bar or edge press, or by Super+drag.
	 * grab_edges is a WLR_EDGE_* bitmask for resize. The grab
	 * intercepts pointer motion (the window follows live) and ends
	 * on button release. */
	int grab_mode; /* 0 = none, 1 = move, 2 = resize */
	struct vile_toplevel *grab_toplevel;
	double grab_cursor_x, grab_cursor_y;
	int grab_start_x, grab_start_y, grab_start_w, grab_start_h;
	uint32_t grab_edges;
	/* Pending grab (0.5.4 drag fix): a press on a title bar /
	 * edge / Super+drag arms a grab but does NOT start it — the
	 * grab only begins once the pointer has moved past
	 * VILE_DRAG_THRESHOLD. A plain click (press + release with
	 * no motion) therefore reaches the client untouched, so a
	 * CSD minimize/maximize/close button works even on a
	 * maximized window, where starting the grab on press used
	 * to unmaximize + re-anchor the window and steal the click.
	 * The press point is kept so the grab, once it fires,
	 * anchors on the press — the window follows the pointer
	 * exactly from where the drag started. */
	int pending_grab_mode; /* 0 = none, 1 = move, 2 = resize */
	struct vile_toplevel *pending_grab_toplevel;
	double pending_grab_x, pending_grab_y;
	uint32_t pending_grab_edges;

	/* Lock screen (queued after Piece 2). Minimal keyboard slice:
	 * just enough to type a password — full input (focus, keybinds,
	 * cursor) is Piece 3. */
	struct wl_list keyboards; /* struct vile_keyboard.link */
	struct wl_listener new_input;
	struct wlr_virtual_keyboard_manager_v1 *vkbd_manager;
	struct wl_listener new_virtual_keyboard;
	bool locked;
	struct wlr_scene_tree *lock_tree;
	struct wlr_scene_rect *lock_dim;
	struct wlr_scene_buffer *lock_ui_node;
	struct wlr_buffer *lock_ui_buffer;
	/* Lock v2 (video-style): the lock background is the wallpaper
	 * pre-blurred + darkened, drawn as its own full-screen node
	 * inside the lock tree — windows are hidden entirely, so nothing
	 * can bleed through. */
	struct wlr_scene_buffer *lock_bg_node;
	struct wlr_buffer *lock_bg_buffer;
	int lock_out_w, lock_out_h;
	/* Lock v2 state: show the typed password in clear text instead
	 * of dots (Hide-password toggle, default masked), and whether
	 * the right-edge settings panel is open. */
	bool lock_show_password;
	bool lock_settings_open;
	/* Click targets on the lock screen (output coords), filled in
	 * by vile_lock_render so the pointer handler can hit-test them. */
	struct wlr_box lock_gear_rect;
	struct wlr_box lock_toggle_rect;
	struct wlr_box lock_reboot_rect;
	struct wlr_box lock_poweroff_rect;
	struct wlr_box lock_close_rect;
	char lock_password[64];
	size_t lock_password_len;
	bool lock_failed;
	struct wl_event_source *clock_timer;
	const char *lock_user;

	/* Settings: user profile picture. Written by Vile Settings to
	 * ~/.config/vile/profile.png (and ~/.face); the lock avatar reads
	 * this SAME file, so a change in Settings shows on the lock
	 * screen. Cached with mtime — reloaded only when the file
	 * changes, so the once-a-second clock tick stays cheap. If the
	 * file is missing/unreadable, the avatar falls back to the
	 * purple V placeholder (profile_pixels == NULL). */
	unsigned char *profile_pixels;
	int profile_w, profile_h;
	time_t profile_mtime;

	/* Piece 5 — Vile look: bottom bar, dock.
	 * Scene order: wallpaper -> windows -> shell tree (bar + dock
	 * + menu) -> lock tree. The shell tree is re-raised after every
	 * focus/map/tile, because focusing raises a window above it.
	 * Fullscreen hides the shell tree (a fullscreen window covers
	 * the bar and dock, like every OS). */
	bool shell_created;
	int shell_w, shell_h;
	struct wlr_scene_tree *shell_tree;
	struct wlr_scene_buffer *shell_bar_node;
	struct wlr_scene_buffer *shell_dock_node;
	struct wlr_scene_buffer *shell_menu_node;
	struct wlr_buffer *shell_bar_buffer;
	struct wlr_buffer *shell_dock_buffer;
	struct wlr_buffer *shell_menu_buffer;
	struct wl_event_source *shell_timer;
	char shell_minute[16];
	/* Bar frost cache (Bar Redesign): blurred wallpaper strip
	 * behind the bar, rebuilt only when the wallpaper path, its
	 * mtime, or the output size changes. */
	cairo_surface_t *shell_bar_frost;
	char shell_bar_frost_path[1024];
	int shell_bar_frost_w, shell_bar_frost_h;
	time_t shell_bar_frost_mtime;
	/* Dock frost cache (Frosted dock, Nov 10, 2026 ~19:52 —
	 * macOS-style: blurred wallpaper behind the dock pill,
	 * rebuilt only when the wallpaper path, its mtime, or the
	 * dock rect (x/y/w/h) changes. The tint is applied at paint
	 * time from the theme, so a theme flip needs no rebuild. */
	cairo_surface_t *shell_dock_frost;
	char shell_dock_frost_path[1024];
	time_t shell_dock_frost_mtime;
	int shell_dock_frost_x, shell_dock_frost_y,
		shell_dock_frost_w, shell_dock_frost_h;
	cairo_surface_t *shell_foot_icon;
	cairo_surface_t *shell_files_icon;
	cairo_surface_t *shell_vile_icon;
	int shell_dock_x, shell_dock_y;
	int shell_menu_x, shell_menu_y;
	bool shell_menu_open;
	int shell_dock_foot_n, shell_dock_set_n, shell_dock_screenfit_n,
		shell_dock_files_n;
	struct vile_toplevel *shell_dock_focused;
	/* Dock (frosted, ordered, Nov 10, 2026 ~16:31): the dock
	 * is an ordered list of app ids (dock_order[0..dock_n-1]),
	 * centred, growing symmetrically from the middle as entries
	 * are added. dock_setup_shown tracks whether the Setup icon
	 * is currently appended (first-run setup incomplete, not
	 * unpinned). Per-slot click rects live in shell_dock_rects. */
	int dock_order[VILE_DOCK_MAX];
	int dock_n;
	bool dock_setup_shown;
	struct wlr_box shell_dock_rects[VILE_DOCK_MAX];
	/* Unpin context menu (right-click a dock icon): one row,
	 * "Unpin", in a small card above the dock. */
	struct wlr_scene_buffer *shell_unpin_node;
	struct wlr_buffer *shell_unpin_buffer;
	bool shell_unpin_open;
	int shell_unpin_slot;
	struct wlr_box shell_unpin_rect;
	struct wlr_box shell_unpin_row_rect;
	/* Dock press / reorder drag: a press arms the slot, a 400ms
	 * hold starts the reorder, a quick release activates the app.
	 * dock_press_down tracks the button so a release after the
	 * timer fired still drops the drag. */
	struct wl_event_source *dock_hold_timer;
	int dock_press_slot; /* -1 none */
	int64_t dock_press_ms;
	double dock_press_x, dock_press_y;
	bool dock_press_moved;
	bool dock_press_down;
	bool dock_reorder;
	int dock_drag_slot;
	double dock_drag_x;
	/* Click targets on the shell (output coords). */
	struct wlr_box shell_status_rect;
	struct wlr_box shell_dock_rect;
	struct wlr_box shell_menu_rect;
	struct wlr_box shell_menu_lock_rect;
	struct wlr_box shell_menu_settings_rect;
	struct wlr_box shell_menu_power_rect;
	struct wlr_box shell_menu_logout_rect;

	/* Piece 7 — animations. All of them are wall-clock driven
	 * (monotonic ms, ease-out cubic) and advanced by vile_anim_tick
	 * in the output frame handler; a slow frame just finishes an
	 * animation early, and VILE_ANIM_OFF=1 makes everything instant.
	 * Opacity / position live on the scene nodes, so no re-render of
	 * the cairo buffers is needed mid-animation — except the dock,
	 * whose icons are painted into one buffer (hover grow + click
	 * pop re-render that small buffer while active). */
	int64_t anim_startup_ms; /* 0 = startup anim not started yet */
	bool anim_startup_done;
	int dock_hover; /* -1 none, else the dock SLOT hovered (0..dock_n-1) */
	float dock_icon_scale[VILE_DOCK_MAX]; /* current per-slot scale (1.0 normal) */
	float dock_rendered_scale[VILE_DOCK_MAX]; /* scale last painted into the dock buffer */
	float dock_icon_from[VILE_DOCK_MAX];
	float dock_icon_to[VILE_DOCK_MAX];
	int64_t dock_icon_ms[VILE_DOCK_MAX]; /* when each slot's scale anim started */
	int dock_pop_icon; /* -1 none, else the dock SLOT popping */
	int64_t dock_pop_ms;
	/* Launch jump (0.5.6 — KDE Plasma style): per-APP launch start
	 * time, monotonic ms (0 = not launching). Set when a dock click
	 * spawns an app; cleared when its window maps or after the 5s
	 * timeout. While set, the tick re-renders the dock every frame
	 * and the icon bounces. */
	int64_t dock_launch_ms[DOCK_APP_COUNT];
	/* Circle-spin loader frames (assets/loaders/circle-spin/),
	 * drawn as a purple mask on a bouncing icon while its app
	 * loads. Loaded lazily on the first dock launch. */
	cairo_surface_t *shell_spin_frames[53];
	int shell_spin_n;
	int shell_dock_buf_w, shell_dock_buf_h; /* last dock buffer size */
	/* System menu rise/fade: current opacity + y offset, animated
	 * toward the targets; menu_closing defers disabling the node
	 * until the fade-out finishes. */
	bool menu_anim_active;
	int64_t menu_anim_ms;
	float menu_op, menu_dy;
	float menu_from_op, menu_to_op;
	float menu_from_dy, menu_to_dy;
	bool menu_closing;
	/* Lock fade: current opacity (bg + UI nodes) and UI y drift;
	 * lock_fading_out defers vile_lock_destroy until the fade ends. */
	bool lock_anim_active;
	int64_t lock_anim_ms;
	bool lock_anim_in;
	bool lock_fading_out;
	float lock_op;
	/* Close fade: a snapshot node holding the closing window's last
	 * buffer, fading out over the re-tiled desktop. */
	struct wlr_scene_buffer *close_fade_node;
	int64_t close_fade_ms;

	/* Piece 2: startup command, run once the backend is up. */
	const char *startup_cmd;

	/* Wallpaper: bottom-most scene node, full-screen. */
	struct wlr_scene_rect *bg_rect;
	struct wlr_scene_buffer *wallpaper_node;
	struct wlr_buffer *wallpaper_buffer;
	struct wlr_texture *wallpaper_texture;
	int wallpaper_w, wallpaper_h;
	/* No logo overlay (Nov 10, 2026 ~16:31 — Adnan, twice: the
	 * Arch logo must not be stamped on ANY wallpaper. The overlay
	 * draw was deleted entirely; the Arch wallpaper keeps its own
	 * baked-in logo, nebula/dunes show clean. The arch-logo.png /
	 * arch-logo-dark.png assets stay on disk, unused.) */
	/* Live reload (UI polish): the 1-second shell timer watches
	 * ~/.config/vile/wallpaper; when its mtime changes, the
	 * compositor swaps the wallpaper without a restart.
	 * wallpaper_current is the resolved path now on screen (the
	 * VILE_WALLPAPER env override, the hook file, or the default);
	 * wallpaper_hook_mtime is the hook file's mtime at last check
	 * (0 = no hook file). */
	time_t wallpaper_hook_mtime;
	char wallpaper_current[1024];

	/* Theme (0.5.4): ~/.config/vile/theme ("dark"/"light", default
	 * dark). Read at startup and watched by the 1s shell timer,
	 * same pattern as the wallpaper hook — switching applies live.
	 * VILE_THEME=light|dark (test override) wins over the file. */
	bool theme_light;
	time_t theme_mtime;

	/* Headless-test helpers. */
	struct wl_event_source *quit_timer;
	int quit_after_sec;
	const char *socket_file;
};

struct vile_output {
	struct wl_list link;
	struct vile_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
	int frames_committed;
};

/* ------------------------------------------------------------------ */
/* Wallpaper                                                           */
/* ------------------------------------------------------------------ */

/* Paint decoded RGBA pixels into a wlroots buffer using the renderer.
 * Generic helper: the wallpaper and the lock background both use it.
 * If tex_out is non-NULL the texture is returned to the caller (the
 * wallpaper keeps its texture for the scene buffer); otherwise it is
 * dropped after the pass (the lock background only needs the buffer). */
static struct wlr_buffer *vile_paint_pixels(struct vile_server *server,
		const unsigned char *pixels, int width, int height,
		struct wlr_texture **tex_out) {
	struct wlr_renderer *renderer = server->renderer;

	struct wlr_texture *tex = wlr_texture_from_pixels(renderer,
		DRM_FORMAT_ABGR8888, width * 4, width, height, pixels);
	if (!tex) {
		wlr_log(WLR_ERROR, "Vile: wlr_texture_from_pixels failed");
		return NULL;
	}
	if (tex_out) {
		*tex_out = tex;
	}

	/* Pick a buffer format the renderer can actually render into. */
	const struct wlr_drm_format_set *fmts =
		wlr_renderer_get_render_formats(renderer);
	const struct wlr_drm_format *fmt = NULL;
	if (fmts) {
		fmt = wlr_drm_format_set_get(fmts, DRM_FORMAT_ARGB8888);
		if (!fmt) {
			fmt = wlr_drm_format_set_get(fmts, DRM_FORMAT_XRGB8888);
		}
	}
	if (!fmt) {
		wlr_log(WLR_ERROR, "Vile: no ARGB8888/XRGB8888 render format");
		if (!tex_out) {
			wlr_texture_destroy(tex);
		}
		return NULL;
	}

	struct wlr_buffer *buf = wlr_allocator_create_buffer(server->allocator,
		width, height, fmt);
	if (!buf) {
		wlr_log(WLR_ERROR, "Vile: wlr_allocator_create_buffer failed");
		if (!tex_out) {
			wlr_texture_destroy(tex);
		}
		return NULL;
	}

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(renderer, buf, NULL);
	if (!pass) {
		wlr_log(WLR_ERROR, "Vile: wlr_renderer_begin_buffer_pass failed");
		wlr_buffer_drop(buf);
		if (!tex_out) {
			wlr_texture_destroy(tex);
		}
		return NULL;
	}
	struct wlr_render_texture_options opts = {
		.texture = tex,
		.dst_box = { .x = 0, .y = 0, .width = width, .height = height },
		.filter_mode = WLR_SCALE_FILTER_BILINEAR,
	};
	wlr_render_pass_add_texture(pass, &opts);
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "Vile: wallpaper render pass submit failed");
		wlr_buffer_drop(buf);
		if (!tex_out) {
			wlr_texture_destroy(tex);
		}
		return NULL;
	}
	if (!tex_out) {
		wlr_texture_destroy(tex);
	}
	return buf;
}

/* Paint the decoded RGBA wallpaper pixels into a wlroots buffer using
 * the renderer, so the scene graph can display it as a scene buffer. */
static struct wlr_buffer *vile_paint_wallpaper(struct vile_server *server,
		const unsigned char *pixels, int width, int height) {
	return vile_paint_pixels(server, pixels, width, height,
		&server->wallpaper_texture);
}

/* Decode an image file and paint it into a wlroots buffer. Returns
 * NULL on failure. Used for the lock background (pre-blurred
 * wallpaper); the desktop wallpaper has its own loader above. */
static struct wlr_buffer *vile_paint_file_to_buffer(struct vile_server *server,
		const char *path, int *out_w, int *out_h) {
	int w = 0, h = 0, channels = 0;
	unsigned char *pixels = stbi_load(path, &w, &h, &channels, 4);
	if (!pixels) {
		wlr_log(WLR_ERROR, "Vile: could not decode '%s': %s",
			path, stbi_failure_reason());
		return NULL;
	}
	struct wlr_buffer *buf = vile_paint_pixels(server, pixels, w, h, NULL);
	stbi_image_free(pixels);
	if (!buf) {
		return NULL;
	}
	if (out_w) {
		*out_w = w;
	}
	if (out_h) {
		*out_h = h;
	}
	return buf;
}

/* Wallpaper hook path: ~/.config/vile/wallpaper — the file Settings
 * writes when the user picks a wallpaper. Returns false when HOME
 * is unset. */
static bool vile_wallpaper_hook_path(char *buf, size_t len) {
	const char *home = getenv("HOME");
	if (!home) {
		return false;
	}
	snprintf(buf, len, "%s/.config/vile/wallpaper", home);
	return true;
}

static bool vile_load_wallpaper(struct vile_server *server, const char *path) {
	int w = 0, h = 0, channels = 0;
	unsigned char *pixels = stbi_load(path, &w, &h, &channels, 4);
	if (!pixels) {
		wlr_log(WLR_ERROR, "Vile: could not decode wallpaper '%s': %s",
			path, stbi_failure_reason());
		return false;
	}
	wlr_log(WLR_INFO, "Vile: wallpaper '%s' decoded: %dx%d", path, w, h);
	server->wallpaper_w = w;
	server->wallpaper_h = h;

	struct wlr_buffer *buf = vile_paint_wallpaper(server, pixels, w, h);
	stbi_image_free(pixels);
	if (!buf) {
		return false;
	}
	server->wallpaper_buffer = buf;

	/* Bottom-most node in the scene, under everything else. The dark
	 * purple rect goes underneath it so any letterbox area is Vile
	 * purple-dark, never black. */
	const struct vile_palette *P = vile_pal(server->theme_light);
	const float bg[4] = { P->bg[0], P->bg[1], P->bg[2], 1.0f };
	server->bg_rect = wlr_scene_rect_create(&server->scene->tree,
		VILE_OUT_W, VILE_OUT_H, bg);
	server->wallpaper_node = wlr_scene_buffer_create(&server->scene->tree, buf);
	if (!server->wallpaper_node) {
		wlr_log(WLR_ERROR, "Vile: wlr_scene_buffer_create failed");
		return false;
	}
	wlr_scene_buffer_set_dest_size(server->wallpaper_node, w, h);
	wlr_log(WLR_INFO, "Vile: wallpaper node in scene (%dx%d)", w, h);
	return true;
}

/* Fit wallpaper + background to the real output size when an output
 * appears (or changes mode). Cover-fit: scale to cover, centred. */
static void vile_fit_wallpaper(struct vile_server *server, int out_w, int out_h) {
	if (server->bg_rect) {
		wlr_scene_rect_set_size(server->bg_rect, out_w, out_h);
	}
	if (!server->wallpaper_node || server->wallpaper_w <= 0) {
		return;
	}
	/* Cover scale, centred. */
	double sx = (double)out_w / server->wallpaper_w;
	double sy = (double)out_h / server->wallpaper_h;
	double scale = sx > sy ? sx : sy;
	int dw = (int)(server->wallpaper_w * scale + 0.5);
	int dh = (int)(server->wallpaper_h * scale + 0.5);
	wlr_scene_buffer_set_dest_size(server->wallpaper_node, dw, dh);
	wlr_scene_node_set_position(&server->wallpaper_node->node,
		(out_w - dw) / 2, (out_h - dh) / 2);
	wlr_log(WLR_INFO, "Vile: wallpaper fitted to output %dx%d (drawn %dx%d)",
		out_w, out_h, dw, dh);
}

/* Live wallpaper reload (Settings wallpaper switching): decode the
 * new file, paint a fresh buffer, swap it onto the existing scene
 * node, and drop the old buffer + texture. The node stays in place
 * at the bottom of the scene, so the swap is a single frame — no
 * flicker, no restart. If the new file cannot be decoded, the old
 * wallpaper stays up. */
static void vile_reload_wallpaper(struct vile_server *server) {
	const char *path = vile_wallpaper_path();
	if (!path || strcmp(path, server->wallpaper_current) == 0) {
		return;
	}
	struct wlr_buffer *old = server->wallpaper_buffer;
	struct wlr_texture *old_tex = server->wallpaper_texture;
	int w = 0, h = 0, channels = 0;
	unsigned char *pixels = stbi_load(path, &w, &h, &channels, 4);
	if (!pixels) {
		wlr_log(WLR_ERROR, "Vile: reload: could not decode '%s': %s",
			path, stbi_failure_reason());
		return;
	}
	struct wlr_buffer *buf = vile_paint_wallpaper(server, pixels, w, h);
	stbi_image_free(pixels);
	if (!buf) {
		/* The paint failed after the texture was already swapped
		 * into server->wallpaper_texture — put the old texture
		 * back so the field still matches the live buffer. */
		if (server->wallpaper_texture != old_tex) {
			wlr_texture_destroy(server->wallpaper_texture);
			server->wallpaper_texture = old_tex;
		}
		return;
	}
	server->wallpaper_buffer = buf;
	server->wallpaper_w = w;
	server->wallpaper_h = h;
	wlr_scene_buffer_set_buffer(server->wallpaper_node, buf);
	if (old) {
		wlr_buffer_drop(old);
	}
	if (old_tex) {
		wlr_texture_destroy(old_tex);
	}
	snprintf(server->wallpaper_current, sizeof(server->wallpaper_current),
		"%s", path);
	vile_fit_wallpaper(server, server->shell_w, server->shell_h);
	wlr_log(WLR_INFO, "Vile: wallpaper reloaded live: '%s' (%dx%d)",
		path, w, h);
}

/* ------------------------------------------------------------------ */
/* Output                                                              */
/* ------------------------------------------------------------------ */

static void vile_output_frame(struct wl_listener *listener, void *data) {
	struct vile_output *output = wl_container_of(listener, output, frame);
	/* Piece 7: advance every running animation BEFORE the commit,
	 * so this frame already shows the new state — a window that
	 * mapped this event loop never flashes at full opacity first. */
	bool anims_active = vile_anim_tick(output->server);
	if (!wlr_scene_output_commit(output->scene_output, NULL)) {
		return;
	}
	output->frames_committed++;
	if (output->frames_committed == 1 || output->frames_committed % 120 == 0) {
		wlr_log(WLR_INFO, "Vile: frame %d committed on '%s'",
			output->frames_committed, output->wlr_output->name);
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(output->scene_output, &now);
	/* Piece 7: while any animation is running, keep the frame loop
	 * going at the output refresh rate — otherwise the headless
	 * backend would only present a frame on damage and the animation
	 * would stall between frames. */
	if (anims_active) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

static void vile_output_request_state(struct wl_listener *listener, void *data) {
	struct vile_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
	vile_fit_wallpaper(output->server,
		output->wlr_output->width, output->wlr_output->height);
	/* An output resize re-tiles the windows to the new size. */
	vile_tile(output->server);
	/* Piece 5: an output resize re-fits the bar / clock / dock. */
	vile_shell_fit(output->server,
		output->wlr_output->width, output->wlr_output->height);
}

static void vile_output_destroy(struct wl_listener *listener, void *data) {
	struct vile_output *output = wl_container_of(listener, output, destroy);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}

static void vile_server_new_output(struct wl_listener *listener, void *data) {
	struct vile_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	struct vile_output *output = calloc(1, sizeof(*output));
	if (!output) {
		return;
	}
	output->server = server;
	output->wlr_output = wlr_output;
	output->frame.notify = vile_output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	output->request_state.notify = vile_output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);
	output->destroy.notify = vile_output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	wl_list_insert(&server->outputs, &output->link);

	output->scene_output = wlr_scene_output_create(server->scene, wlr_output);
	if (!output->scene_output) {
		wlr_log(WLR_ERROR, "Vile: wlr_scene_output_create failed");
		return;
	}

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	if (wlr_output_is_headless(wlr_output)) {
		/* Headless test output: force 1920x1080 so the screenshot is
		 * exactly the wallpaper's size. Test hook: VILE_OUT_W /
		 * VILE_OUT_H override the size (e.g. 1280x800 for the QEMU
		 * resolution check) — real hardware always uses the
		 * preferred mode below. */
		int out_w = VILE_OUT_W, out_h = VILE_OUT_H;
		const char *ew = getenv("VILE_OUT_W");
		const char *eh = getenv("VILE_OUT_H");
		if (ew) {
			int v = atoi(ew);
			if (v >= 320 && v <= 7680) {
				out_w = v;
			}
		}
		if (eh) {
			int v = atoi(eh);
			if (v >= 240 && v <= 4320) {
				out_h = v;
			}
		}
		wlr_output_state_set_custom_mode(&state,
			out_w, out_h, 0);
		wlr_log(WLR_INFO, "Vile: headless output, forcing %dx%d",
			out_w, out_h);
	} else {
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		if (mode) {
			wlr_output_state_set_mode(&state, mode);
		}
	}
	if (!wlr_output_commit_state(wlr_output, &state)) {
		wlr_log(WLR_ERROR, "Vile: output commit_state failed");
	}
	wlr_output_state_finish(&state);

	wlr_output_create_global(wlr_output, server->display);
	/* The output must be in the output layout or the scene has nowhere
	 * to place it — and xdg-output (grim) asserts without this. */
	wlr_output_layout_add(server->output_layout, wlr_output, 0, 0);
	wlr_log(WLR_INFO, "Vile: output '%s' ready (%dx%d)",
		wlr_output->name, wlr_output->width, wlr_output->height);
	vile_fit_wallpaper(server, wlr_output->width, wlr_output->height);
	if (server->locked) {
		vile_lock_fit(server, wlr_output->width, wlr_output->height);
	}
	/* Piece 5: shell (bar + clock + dock) once the output size is
	 * known. If we started locked, the lock tree was created before
	 * the shell — raise the lock back above it. */
	vile_shell_create(server, wlr_output->width, wlr_output->height);
	if (server->locked) {
		vile_lock_raise(server);
	}
	/* Piece 7: startup animation — wallpaper fades in, bar slides
	 * up, dock pops in. Runs once, on the first output. */
	vile_anim_startup(server);
	/* Kick the first frame so the scene is presented even if nothing
	 * else ever damages it (headless backends need the nudge). */
	wlr_output_schedule_frame(wlr_output);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* xdg-shell windows (Piece 2)                                         */
/* ------------------------------------------------------------------ */

/* One application window. The scene tree is what actually gets drawn —
 * it is created from the xdg surface, so wlroots keeps it in sync with
 * the client's buffers for us. Structure follows tinywl (the wlroots
 * minimal example); the code itself is written fresh for Vile. */
struct vile_toplevel {
	struct wl_list link;
	struct vile_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	/* Piece 4: tiling — border rect behind the window (a scene-root
	 * rect, kept below the window), creation sequence (stable tile
	 * order), fullscreen state, and the last tile rect given. */
	struct wlr_scene_rect *border;
	uint64_t seq;
	bool fullscreen;
	int tile_x, tile_y, tile_w, tile_h;
	/* Interactive move/resize + minimize/maximize (0.5.4 post-theme
	 * fixes, Nov 10 2026 — the Dell run: windows were not draggable
	 * at all and the minimize/maximize buttons did nothing).
	 * floating = dragged out of the tiling layout (float_* is the
	 * rect it was dropped at); minimized = hidden by
	 * set_minimized, restorable from the dock or Super+J/K;
	 * maximized = fills the work area, max_prev_* is the rect to
	 * restore (and max_was_floating says whether that rect was a
	 * float rect or a tile). tile_* is kept in sync with the
	 * current visible rect, so the open animation and the close
	 * fade use the right geometry for floating windows too. */
	bool floating;
	bool minimized;
	bool maximized;
	bool max_was_floating;
	int float_x, float_y, float_w, float_h;
	int max_prev_x, max_prev_y, max_prev_w, max_prev_h;
	/* Piece 7 — window open animation + focus border cross-fade.
	 * surf_buf is the surface's scene buffer, found by walking the
	 * scene tree (the subsurface tree is the oldest child, so it is
	 * last in the children list — reverse iteration finds it) and
	 * cached; the open animation fades its opacity 0->1 while
	 * scaling its dest size 0.96->1.0 around the tile centre. The
	 * border cross-fade lerps border_focus 0 (dim) -> 1 (focused)
	 * over 150ms instead of snapping the colour. */
	struct wlr_scene_buffer *surf_buf;
	bool anim_active;
	int64_t anim_ms;
	int anim_dur;
	float anim_from_op, anim_to_op;
	float anim_from_scale, anim_to_scale;
	bool border_anim_active;
	int64_t border_anim_ms;
	float border_from, border_to;
	float border_focus;
	/* Dock: true when this window is a Vile Files window. The
	 * Files title changes with navigation (folder name), so title
	 * matching cannot identify it — app_files is set once at map
	 * time from the client process cmdline (contains
	 * "vile-files.py"), with a "Files" title fallback. */
	bool app_files;
};

struct vile_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

/* Ask every output to present a new frame soon. Call this whenever a
 * window maps, commits new content, or unmaps, so the change actually
 * reaches the screen before a screenshot is taken. */
static void vile_schedule_frames(struct vile_server *server) {
	struct vile_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

/* ------------------------------------------------------------------ */
/* Tiling (Piece 4)                                                    */
/* ------------------------------------------------------------------ */

/* Layout: master-stack. Gap 12px around the edges and between
 * windows; a 4px border around each window (purple when focused).
 *   1 window:  fills the whole area (1896x924 at 1920x1080).
 *   2 windows: master left half, stack right half (942 wide each).
 *   3+ windows: master left half full height; the rest share the
 *   right half, split equally top-to-bottom.
 * Tile order is creation order (oldest = master), NOT focus order —
 * focusing a window must never reshuffle the layout. */
#define VILE_TILE_GAP 12
#define VILE_BORDER 4
/* Bottom-bar layout (Nov 10, 2026 — Adnan moved the bar from the
 * top to the bottom; Bar Redesign the same day made it 40px
 * frosted): the bar (40px) sits at the bottom, the floating dock
 * pill (80px) sits 12px above it, and the tiling area runs from
 * the top (12px gap) down to 12px above the dock — windows never
 * cover the bar or the dock. */
#define VILE_BAR_H 40
#define VILE_DOCK_H 80
/* Interactive move/resize (0.5.4 post-theme fixes): the top strip
 * of a window that starts a move, and the edge/corner strip that
 * starts a resize. */
#define VILE_TITLE_H 36
#define VILE_EDGE_PX 8

static int vile_tile_count(struct vile_server *server) {
	int n = 0;
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		n++;
	}
	return n;
}

/* Windows that take part in the master-stack layout: visible,
 * tiled ones only. Floating (dragged), maximized and minimized
 * windows are positioned/hidden by their own code paths in
 * vile_tile, never by the layout — so dragging one window out
 * never reshuffles the ones left behind. */
static int vile_tileable_count(struct vile_server *server) {
	int n = 0;
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		if (!t->minimized && !t->floating && !t->maximized) {
			n++;
		}
	}
	return n;
}

/* Tile rect for the window at `index` (0 = master) out of `count`
 * windows on an output of out_w x out_h. */
static void vile_tile_rect(int index, int count, int out_w, int out_h,
		int *x, int *y, int *w, int *h) {
	int gap = VILE_TILE_GAP;
	int ax = gap, ay = gap;
	int aw = out_w - gap * 2;
	/* Bottom-bar layout: the tiling area starts at the top (12px
	 * gap) and ends 12px above the floating dock pill, which sits
	 * 12px above the 40px bottom bar — windows never cover the bar
	 * or the dock. At 1920x1080 that is 1896x924 at (12,12).
	 * Fullscreen still covers the whole output (and hides the shell
	 * tree — see vile_tile). */
	int ah = out_h - gap - gap - VILE_DOCK_H - gap - VILE_BAR_H;
	if (count <= 1) {
		*x = ax; *y = ay; *w = aw; *h = ah;
		return;
	}
	int master_w = (aw - gap) / 2;
	int stack_w = aw - master_w - gap;
	if (index == 0) {
		*x = ax; *y = ay; *w = master_w; *h = ah;
		return;
	}
	/* Stack: right half, split equally vertically. */
	int k = count - 1; /* stack windows */
	int stack_h = (ah - gap * (k - 1)) / k;
	int si = index - 1;
	*x = ax + master_w + gap;
	*y = ay + si * (stack_h + gap);
	*w = stack_w;
	*h = stack_h;
}

/* qsort comparator: creation order (oldest first). */
static int vile_tile_cmp(const void *a, const void *b) {
	const struct vile_toplevel * const *ta = a;
	const struct vile_toplevel * const *tb = b;
	if ((*ta)->seq < (*tb)->seq) return -1;
	if ((*ta)->seq > (*tb)->seq) return 1;
	return 0;
}

/* Focused window: purple #6D4AFF border; unfocused: dim grey.
 * Borders stay hidden while any window is fullscreen. */
/* Lerp the border colour for one window from its current
 * border_focus (0 = dim grey, 1 = focused purple). Called by the
 * animation tick every frame while a border is cross-fading, and
 * once at the end so the final colour is exact. */
static void vile_border_apply(struct vile_toplevel *t) {
	if (!t->border) {
		return;
	}
	const float focused[4] = { 0.43f, 0.29f, 1.0f, 1.0f };
	const struct vile_palette *pal = vile_pal(t->server->theme_light);
	const float dim[4] = { pal->border_dim[0], pal->border_dim[1],
		pal->border_dim[2], 1.0f };
	float f = t->border_focus;
	float c[4] = {
		dim[0] + (focused[0] - dim[0]) * f,
		dim[1] + (focused[1] - dim[1]) * f,
		dim[2] + (focused[2] - dim[2]) * f,
		1.0f,
	};
	wlr_scene_rect_set_color(t->border, c);
}

static void vile_update_borders(struct vile_server *server) {
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		if (!t->border) continue;
		/* Minimized and maximized windows have no border at all
		 * (vile_tile hides them) — never re-enable it here, or
		 * a minimized window would leave a purple rect floating
		 * on the desktop. */
		bool show = (server->fullscreen_toplevel == NULL) &&
			!t->minimized && !t->maximized;
		wlr_scene_node_set_enabled(&t->border->node, show);
		/* Piece 7: focus change cross-fades the border instead of
		 * snapping it. Re-base the fade from the CURRENT focus
		 * value, so a fast double-switch never jumps. With
		 * animations off, the colour is set immediately. */
		float target = (t == server->focused_toplevel) ? 1.0f : 0.0f;
		if (!vile_anim_enabled() ||
				fabsf(t->border_focus - target) < 0.001f) {
			t->border_focus = target;
			t->border_anim_active = false;
			vile_border_apply(t);
			continue;
		}
		t->border_from = t->border_focus;
		t->border_to = target;
		t->border_anim_ms = vile_anim_now_ms();
		t->border_anim_active = true;
	}
	vile_schedule_frames(server);
}

/* ------------------------------------------------------------------ */
/* Piece 7 — animations                                               */
/* ------------------------------------------------------------------ */
/* Every animation is wall-clock driven (monotonic ms, ease-out
 * cubic) and advanced by vile_anim_tick() from the output frame
 * handler, right before the scene commit — so a frame never shows a
 * stale state, and a slow frame just finishes an animation early
 * (animations never block input: input is event-driven, the tick
 * only ever moves nodes). VILE_ANIM_OFF=1 makes everything instant;
 * VILE_ANIM_DURATION_MS=N overrides every duration (slow-motion,
 * for mid-animation screenshots). While any animation is active the
 * tick asks for another frame, so the headless backend keeps
 * presenting at its refresh rate instead of stalling between frames. */

static int64_t vile_anim_now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static float vile_anim_ease_out_cubic(float p) {
	if (p <= 0.0f) {
		return 0.0f;
	}
	if (p >= 1.0f) {
		return 1.0f;
	}
	float u = 1.0f - p;
	return 1.0f - u * u * u;
}

/* Duration for one animation: the env override wins (test /
 * slow-motion), otherwise the per-animation default. */
static int vile_anim_dur_for(int def_ms) {
	const char *e = getenv("VILE_ANIM_DURATION_MS");
	if (e) {
		int v = atoi(e);
		if (v >= 0 && v <= 60000) {
			return v;
		}
	}
	return def_ms;
}

/* Launch-jump bounce period (0.5.6): one up-down bounce per
 * VILE_LAUNCH_JUMP_MS ms (default 500). Overridable for tests —
 * the harness slows it to 2000ms because grim takes ~0.75s per
 * shot, so a 500ms bounce is easy to miss between frames. */
static int vile_launch_jump_ms(void) {
	const char *e = getenv("VILE_LAUNCH_JUMP_MS");
	if (e) {
		int v = atoi(e);
		if (v >= 100 && v <= 10000) {
			return v;
		}
	}
	return VILE_LAUNCH_JUMP_MS;
}

static bool vile_anim_enabled(void) {
	const char *e = getenv("VILE_ANIM_OFF");
	return !(e && strcmp(e, "0") != 0);
}

/* Find a toplevel's surface buffer by walking its scene tree. The
 * subsurface tree is the OLDEST child of the toplevel tree (scene
 * children are inserted at the head, so the oldest is last) —
 * reverse iteration finds it first. Its oldest child is the main
 * surface buffer; popups and subsurfaces are newer and sit in front
 * of it in the list. Cached on the toplevel once found. */
static struct wlr_scene_buffer *vile_toplevel_surface_buffer(
		struct vile_toplevel *t) {
	if (t->surf_buf) {
		return t->surf_buf;
	}
	if (!t->scene_tree) {
		return NULL;
	}
	struct wlr_scene_node *n;
	wl_list_for_each_reverse(n, &t->scene_tree->children, link) {
		if (n->type != WLR_SCENE_NODE_TREE) {
			continue;
		}
		struct wlr_scene_tree *sub = (struct wlr_scene_tree *)n;
		struct wlr_scene_node *m;
		wl_list_for_each_reverse(m, &sub->children, link) {
			if (m->type == WLR_SCENE_NODE_BUFFER) {
				t->surf_buf = (struct wlr_scene_buffer *)m;
				return t->surf_buf;
			}
		}
	}
	return NULL;
}

/* Window open: fade in + slight scale-up, from the tile centre. */
static void vile_anim_start_window_open(struct vile_server *server,
		struct vile_toplevel *t) {
	if (!vile_anim_enabled()) {
		return;
	}
	int dur = vile_anim_dur_for(200);
	if (dur <= 0) {
		return;
	}
	t->anim_from_op = 0.0f;
	t->anim_to_op = 1.0f;
	t->anim_from_scale = 0.96f;
	t->anim_to_scale = 1.0f;
	t->anim_ms = vile_anim_now_ms();
	t->anim_dur = dur;
	t->anim_active = true;
	/* Apply the start state NOW so the first presented frame is
	 * already faded — the tick re-applies every frame after. */
	struct wlr_scene_buffer *sb = vile_toplevel_surface_buffer(t);
	if (sb) {
		wlr_scene_buffer_set_opacity(sb, 0.0f);
	}
	wlr_log(WLR_INFO, "Vile: anim: window open '%s' (%dms)",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?", dur);
	vile_schedule_frames(server);
}

/* Window close: snapshot the closing window's last frame into its
 * own scene node and fade THAT out over the re-tiled desktop. The
 * snapshot reproduces the exact last frame — the buffer node sits
 * at (-geometry.x, -geometry.y) inside the window tree, so the
 * snapshot node goes at tile + geometry with the surface's own
 * size. The scene buffer takes its own reference to the buffer, so
 * the frame survives the client disconnecting. Skipped when locked
 * (a closing window must never show over the lock) or when the
 * surface has no buffer left at unmap time. */
static void vile_anim_start_close_fade(struct vile_server *server,
		struct vile_toplevel *t) {
	if (!vile_anim_enabled() || server->locked) {
		return;
	}
	/* A previous close fade is still running: drop it instantly. */
	if (server->close_fade_node) {
		wlr_scene_node_destroy(&server->close_fade_node->node);
		server->close_fade_node = NULL;
		server->close_fade_ms = 0;
	}
	struct wlr_scene_buffer *sb = vile_toplevel_surface_buffer(t);
	if (!sb || !sb->buffer) {
		wlr_log(WLR_INFO,
			"Vile: anim: close fade skipped (no surface buffer)");
		return;
	}
	struct wlr_surface *surf = t->xdg_toplevel->base->surface;
	int bw = surf->current.width, bh = surf->current.height;
	if (bw <= 0 || bh <= 0) {
		return;
	}
	struct wlr_box geo = t->xdg_toplevel->base->geometry;
	struct wlr_scene_buffer *node =
		wlr_scene_buffer_create(&server->scene->tree, sb->buffer);
	if (!node) {
		return;
	}
	wlr_scene_buffer_set_dest_size(node, bw, bh);
	wlr_scene_node_set_position(&node->node,
		t->tile_x + geo.x, t->tile_y + geo.y);
	server->close_fade_node = node;
	server->close_fade_ms = vile_anim_now_ms();
	/* Shell stays crisp above the fading snapshot. */
	vile_shell_raise(server);
	wlr_log(WLR_INFO, "Vile: anim: close fade '%s' at %d,%d %dx%d",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?",
		t->tile_x + geo.x, t->tile_y + geo.y, bw, bh);
	vile_schedule_frames(server);
}

/* Dock hover: retarget each icon's scale (hovered = 1.14, rest =
 * 1.0), re-based from the CURRENT scale so a fast move never jumps.
 * Only retargets when the hovered icon actually changed. */
static void vile_anim_dock_hover(struct vile_server *server, int hover) {
	if (hover == server->dock_hover) {
		return;
	}
	server->dock_hover = hover;
	int64_t now = vile_anim_now_ms();
	for (int i = 0; i < server->dock_n; i++) {
		float target = (i == hover) ? 1.14f : 1.0f;
		if (fabsf(server->dock_icon_scale[i] - target) < 0.001f) {
			server->dock_icon_from[i] = target;
			server->dock_icon_to[i] = target;
			server->dock_icon_ms[i] = 0;
			continue;
		}
		server->dock_icon_from[i] = server->dock_icon_scale[i];
		server->dock_icon_to[i] = target;
		server->dock_icon_ms[i] = now;
	}
	vile_schedule_frames(server);
}

/* Dock click pop: the clicked icon swells briefly (the tick
 * re-renders the dock while the pop is alive). */
static void vile_anim_dock_pop(struct vile_server *server, int icon) {
	server->dock_pop_icon = icon;
	server->dock_pop_ms = vile_anim_now_ms();
	vile_schedule_frames(server);
}

/* Pop multiplier for one dock icon (1.0 when no pop is running):
 * 1.0 -> 1.2 -> 1.0 over 220ms, sine-shaped. */
static float vile_dock_pop_scale(struct vile_server *server, int i) {
	if (server->dock_pop_icon != i || server->dock_pop_ms == 0) {
		return 1.0f;
	}
	float p = (float)(vile_anim_now_ms() - server->dock_pop_ms) / 220.0f;
	if (p >= 1.0f) {
		return 1.0f;
	}
	return 1.0f + 0.20f * sinf((float)M_PI * p);
}

/* System menu: rise + fade in from the bottom bar; fade + sink on
 * close (the node is disabled only when the fade-out finishes). */
static void vile_anim_menu_open(struct vile_server *server) {
	if (!vile_anim_enabled()) {
		server->menu_op = 1.0f;
		server->menu_dy = 0.0f;
		return;
	}
	server->menu_from_op = server->menu_op;
	server->menu_from_dy = server->menu_dy;
	server->menu_to_op = 1.0f;
	server->menu_to_dy = 0.0f;
	server->menu_anim_ms = vile_anim_now_ms();
	server->menu_closing = false;
	server->menu_anim_active = true;
	if (server->shell_menu_node) {
		wlr_scene_buffer_set_opacity(server->shell_menu_node,
			server->menu_op);
		wlr_scene_node_set_position(&server->shell_menu_node->node,
			server->shell_menu_x,
			server->shell_menu_y + (int)server->menu_dy);
	}
	vile_schedule_frames(server);
}

static void vile_anim_menu_close(struct vile_server *server) {
	if (!vile_anim_enabled()) {
		server->menu_closing = false;
		return;
	}
	server->menu_from_op = server->menu_op;
	server->menu_from_dy = server->menu_dy;
	server->menu_to_op = 0.0f;
	server->menu_to_dy = 16.0f;
	server->menu_anim_ms = vile_anim_now_ms();
	server->menu_closing = true;
	server->menu_anim_active = true;
	vile_schedule_frames(server);
}

/* Lock fade-in: the blurred background + UI fade in while the whole
 * UI drifts up 14px (the clock is the dominant element, so it reads
 * as the clock drifting up). Start state is applied immediately. */
static void vile_anim_lock_in(struct vile_server *server) {
	if (!vile_anim_enabled()) {
		server->lock_op = 1.0f;
		return;
	}
	server->lock_anim_in = true;
	server->lock_fading_out = false;
	server->lock_anim_ms = vile_anim_now_ms();
	server->lock_anim_active = true;
	server->lock_op = 0.0f;
	if (server->lock_bg_node) {
		wlr_scene_buffer_set_opacity(server->lock_bg_node, 0.0f);
	}
	if (server->lock_ui_node) {
		wlr_scene_buffer_set_opacity(server->lock_ui_node, 0.0f);
		wlr_scene_node_set_position(&server->lock_ui_node->node, 0, 14);
	}
	if (server->lock_dim) {
		const struct vile_palette *pal = vile_pal(server->theme_light);
		const float dim0[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		wlr_scene_rect_set_color(server->lock_dim, dim0);
	}
	vile_schedule_frames(server);
}

/* Lock fade-out: started on a successful PAM unlock; the tick
 * destroys the lock overlay when the fade finishes. */
static void vile_anim_lock_out(struct vile_server *server) {
	if (!vile_anim_enabled()) {
		return;
	}
	server->lock_anim_in = false;
	server->lock_fading_out = true;
	server->lock_anim_ms = vile_anim_now_ms();
	server->lock_anim_active = true;
	vile_schedule_frames(server);
}

/* Startup: wallpaper fades in, the bar slides up from below the
 * screen, the dock pops in (scale 0.9 -> 1.0 + fade + slide). The
 * start state is applied immediately so the first presented frame
 * is already mid-fade, never a flash of the finished desktop. */
static void vile_anim_startup(struct vile_server *server) {
	if (server->anim_startup_ms != 0 || server->anim_startup_done) {
		return;
	}
	if (!vile_anim_enabled()) {
		server->anim_startup_done = true;
		return;
	}
	server->anim_startup_ms = vile_anim_now_ms();
	if (server->wallpaper_node) {
		wlr_scene_buffer_set_opacity(server->wallpaper_node, 0.0f);
	}
	if (server->shell_bar_node) {
		wlr_scene_buffer_set_opacity(server->shell_bar_node, 0.0f);
		wlr_scene_node_set_position(&server->shell_bar_node->node,
			0, server->shell_h);
	}
	if (server->shell_dock_node) {
		wlr_scene_buffer_set_opacity(server->shell_dock_node, 0.0f);
		wlr_scene_node_set_position(&server->shell_dock_node->node,
			server->shell_dock_x - 40,
			server->shell_dock_y - 20 + 24);
	}
	wlr_log(WLR_INFO, "Vile: anim: startup (%dms)",
		vile_anim_dur_for(250));
	vile_schedule_frames(server);
}

/* Advance every running animation to the current wall-clock time.
 * Returns true while anything is still animating (the frame handler
 * then schedules another frame). Each branch is independent: a
 * window can open while the menu rises while the dock pops. */
static bool vile_anim_tick(struct vile_server *server) {
	bool active = false;
	int64_t now = vile_anim_now_ms();

	/* --- Startup: wallpaper fade, bar slide-up, dock pop. --- */
	if (!server->anim_startup_done && server->anim_startup_ms != 0) {
		int sdur = vile_anim_dur_for(250);
		float p = sdur > 0 ?
			(float)(now - server->anim_startup_ms) / sdur : 1.0f;
		if (p >= 1.0f) {
			server->anim_startup_done = true;
			if (server->wallpaper_node) {
				wlr_scene_buffer_set_opacity(
					server->wallpaper_node, 1.0f);
			}
			if (server->shell_bar_node) {
				wlr_scene_buffer_set_opacity(
					server->shell_bar_node, 1.0f);
				wlr_scene_node_set_position(
					&server->shell_bar_node->node,
					0, server->shell_h - VILE_BAR_H);
			}
			if (server->shell_dock_node) {
				wlr_scene_buffer_set_opacity(
					server->shell_dock_node, 1.0f);
				wlr_scene_buffer_set_dest_size(
					server->shell_dock_node,
					server->shell_dock_buf_w,
					server->shell_dock_buf_h);
				wlr_scene_node_set_position(
					&server->shell_dock_node->node,
					server->shell_dock_x - 40,
					server->shell_dock_y - 20);
			}
			wlr_log(WLR_INFO, "Vile: anim: startup done");
		} else {
			float e = vile_anim_ease_out_cubic(p);
			active = true;
			if (server->wallpaper_node) {
				wlr_scene_buffer_set_opacity(
					server->wallpaper_node, e);
			}
			if (server->shell_bar_node) {
				wlr_scene_buffer_set_opacity(
					server->shell_bar_node, e);
				wlr_scene_node_set_position(
					&server->shell_bar_node->node, 0,
					server->shell_h - VILE_BAR_H +
					(int)((1.0f - e) * VILE_BAR_H));
			}
			if (server->shell_dock_node) {
				float s = 0.9f + 0.1f * e;
				wlr_scene_buffer_set_opacity(
					server->shell_dock_node, e);
				wlr_scene_buffer_set_dest_size(
					server->shell_dock_node,
					(int)(server->shell_dock_buf_w * s),
					(int)(server->shell_dock_buf_h * s));
				wlr_scene_node_set_position(
					&server->shell_dock_node->node,
					server->shell_dock_x - 40 +
					(int)((server->shell_dock_buf_w -
						server->shell_dock_buf_w * s) / 2),
					server->shell_dock_y - 20 +
					(int)((server->shell_dock_buf_h -
						server->shell_dock_buf_h * s) / 2) +
					(int)((1.0f - e) * 24));
			}
		}
	}

	/* --- Windows: open fade+scale, border cross-fade. --- */
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		if (t->anim_active) {
			float p = t->anim_dur > 0 ?
				(float)(now - t->anim_ms) / t->anim_dur : 1.0f;
			if (p >= 1.0f) {
				t->anim_active = false;
				struct wlr_scene_buffer *sb =
					vile_toplevel_surface_buffer(t);
				if (sb) {
					struct wlr_surface *surf =
						t->xdg_toplevel->base->surface;
					wlr_scene_buffer_set_opacity(sb, 1.0f);
					wlr_scene_buffer_set_dest_size(sb,
						surf->current.width,
						surf->current.height);
				}
				wlr_scene_node_set_position(
					&t->scene_tree->node,
					t->tile_x, t->tile_y);
			} else {
				float e = vile_anim_ease_out_cubic(p);
				float op = t->anim_from_op +
					(t->anim_to_op - t->anim_from_op) * e;
				float sc = t->anim_from_scale +
					(t->anim_to_scale - t->anim_from_scale) * e;
				struct wlr_scene_buffer *sb =
					vile_toplevel_surface_buffer(t);
				active = true;
				if (sb) {
					struct wlr_surface *surf =
						t->xdg_toplevel->base->surface;
					struct wlr_box geo =
						t->xdg_toplevel->base->geometry;
					wlr_scene_buffer_set_opacity(sb, op);
					wlr_scene_buffer_set_dest_size(sb,
						(int)(surf->current.width * sc),
						(int)(surf->current.height * sc));
					/* Centre the scaled window in its
					 * tile: the visible top-left is the
					 * tree position, and scaling moves it
					 * by geometry * (1 - scale). */
					wlr_scene_node_set_position(
						&t->scene_tree->node,
						t->tile_x +
						(int)((t->tile_w -
							t->tile_w * sc) / 2) +
						(int)(geo.x * (1.0f - sc)),
						t->tile_y +
						(int)((t->tile_h -
							t->tile_h * sc) / 2) +
						(int)(geo.y * (1.0f - sc)));
				}
			}
		}
		if (t->border_anim_active) {
			int bdur = vile_anim_dur_for(150);
			float p = bdur > 0 ?
				(float)(now - t->border_anim_ms) / bdur : 1.0f;
			if (p >= 1.0f) {
				t->border_focus = t->border_to;
				t->border_anim_active = false;
				vile_border_apply(t);
			} else {
				float e = vile_anim_ease_out_cubic(p);
				t->border_focus = t->border_from +
					(t->border_to - t->border_from) * e;
				vile_border_apply(t);
				active = true;
			}
		}
	}

	/* --- Dock hover grow + click pop: re-render the dock buffer
	 * while any icon scale is moving. --- */
	bool dock_moving = false;
	for (int i = 0; i < server->dock_n; i++) {
		if (server->dock_icon_ms[i] != 0) {
			int ddur = vile_anim_dur_for(150);
			float p = ddur > 0 ?
				(float)(now - server->dock_icon_ms[i]) / ddur
				: 1.0f;
			if (p >= 1.0f) {
				server->dock_icon_scale[i] =
					server->dock_icon_to[i];
				server->dock_icon_ms[i] = 0;
			} else {
				float e = vile_anim_ease_out_cubic(p);
				server->dock_icon_scale[i] =
					server->dock_icon_from[i] +
					(server->dock_icon_to[i] -
						server->dock_icon_from[i]) * e;
				dock_moving = true;
			}
		}
	}
	bool pop_active = false;
	if (server->dock_pop_icon >= 0 && server->dock_pop_ms != 0) {
		float p = (float)(now - server->dock_pop_ms) / 220.0f;
		if (p >= 1.0f) {
			server->dock_pop_icon = -1;
			server->dock_pop_ms = 0;
		} else {
			pop_active = true;
		}
	}
	if (dock_moving || pop_active) {
		active = true;
	}
	if (server->shell_created && (dock_moving || pop_active)) {
		/* Re-render only if an effective scale actually changed
		 * since the last render — the tick runs every frame while
		 * any animation is alive, and the dock buffer is cairo
		 * work we do not want to redo for nothing. */
		float eff[VILE_DOCK_MAX];
		for (int i = 0; i < server->dock_n; i++) {
			eff[i] = server->dock_icon_scale[i] *
				vile_dock_pop_scale(server, i);
		}
		bool changed = false;
		for (int i = 0; i < server->dock_n; i++) {
			if (fabsf(eff[i] - server->dock_rendered_scale[i])
					> 0.002f) {
				changed = true;
				break;
			}
		}
		if (changed) {
			for (int i = 0; i < server->dock_n; i++) {
				server->dock_rendered_scale[i] = eff[i];
			}
			vile_shell_render_dock(server);
		}
	}

	/* --- Launch jump (0.5.6 — KDE Plasma style): bounce the
	 * launching icons until each app's window maps, or the 5s
	 * timeout hits. The dock is re-rendered every frame while
	 * any icon bounces (the offset moves every frame, like the
	 * hover/pop path above). Timeout is overridable for tests:
	 * VILE_LAUNCH_TIMEOUT_MS (default 5000). --- */
	int64_t launch_timeout = VILE_LAUNCH_TIMEOUT_MS;
	{
		const char *te = getenv("VILE_LAUNCH_TIMEOUT_MS");
		if (te && *te) {
			long tv = strtol(te, NULL, 10);
			if (tv > 0) {
				launch_timeout = tv;
			}
		}
	}
	bool launch_active = false, launch_changed = false;
	for (int a = 0; a < DOCK_APP_COUNT; a++) {
		if (server->dock_launch_ms[a] == 0) {
			continue;
		}
		int64_t elapsed = now - server->dock_launch_ms[a];
		if (vile_dock_launch_appeared(server, a)) {
			wlr_log(WLR_INFO,
				"Vile: dock: launch done '%s' (%lldms)",
				vile_dock_app_names[a], (long long)elapsed);
			server->dock_launch_ms[a] = 0;
			launch_changed = true;
		} else if (elapsed >= launch_timeout) {
			wlr_log(WLR_INFO,
				"Vile: dock: launch timeout '%s' (%lldms)",
				vile_dock_app_names[a], (long long)elapsed);
			server->dock_launch_ms[a] = 0;
			launch_changed = true;
		} else {
			launch_active = true;
		}
	}
	if (launch_active) {
		active = true;
	}
	if (server->shell_created && (launch_active || launch_changed)) {
		vile_shell_render_dock(server);
	}

	/* --- System menu rise/fade. --- */
	if (server->menu_anim_active) {
		int mdur = vile_anim_dur_for(200);
		float p = mdur > 0 ?
			(float)(now - server->menu_anim_ms) / mdur : 1.0f;
		if (p >= 1.0f) {
			server->menu_op = server->menu_to_op;
			server->menu_dy = server->menu_to_dy;
			server->menu_anim_active = false;
			if (server->menu_closing && server->shell_menu_node) {
				wlr_scene_node_set_enabled(
					&server->shell_menu_node->node, false);
				server->menu_closing = false;
			}
		} else {
			float e = vile_anim_ease_out_cubic(p);
			server->menu_op = server->menu_from_op +
				(server->menu_to_op - server->menu_from_op) * e;
			server->menu_dy = server->menu_from_dy +
				(server->menu_to_dy - server->menu_from_dy) * e;
			active = true;
		}
		if (server->shell_menu_node) {
			wlr_scene_buffer_set_opacity(server->shell_menu_node,
				server->menu_op);
			wlr_scene_node_set_position(
				&server->shell_menu_node->node,
				server->shell_menu_x,
				server->shell_menu_y + (int)server->menu_dy);
		}
	}

	/* --- Lock fade in/out. --- */
	if (server->lock_anim_active) {
		int ldur = vile_anim_dur_for(250);
		float p = ldur > 0 ?
			(float)(now - server->lock_anim_ms) / ldur : 1.0f;
		if (p >= 1.0f) {
			server->lock_op = server->lock_anim_in ? 1.0f : 0.0f;
			server->lock_anim_active = false;
			if (server->lock_bg_node) {
				wlr_scene_buffer_set_opacity(
					server->lock_bg_node, server->lock_op);
			}
			if (server->lock_ui_node) {
				wlr_scene_buffer_set_opacity(
					server->lock_ui_node, server->lock_op);
				wlr_scene_node_set_position(
					&server->lock_ui_node->node, 0, 0);
			}
			if (server->lock_dim) {
				const struct vile_palette *pal = vile_pal(server->theme_light);
				const float dim[4] = { pal->dim_rgb[0] * server->lock_op,
					pal->dim_rgb[1] * server->lock_op,
					pal->dim_rgb[2] * server->lock_op,
					0.30f * server->lock_op };
				wlr_scene_rect_set_color(server->lock_dim, dim);
			}
			if (server->lock_fading_out) {
				server->lock_fading_out = false;
				wlr_log(WLR_INFO,
					"Vile: anim: lock fade-out done, destroying lock");
				vile_lock_destroy(server);
			}
		} else {
			float e = vile_anim_ease_out_cubic(p);
			server->lock_op = server->lock_anim_in ?
				e : 1.0f - e;
			active = true;
			if (server->lock_bg_node) {
				wlr_scene_buffer_set_opacity(
					server->lock_bg_node, server->lock_op);
			}
			if (server->lock_ui_node) {
				wlr_scene_buffer_set_opacity(
					server->lock_ui_node, server->lock_op);
				/* Clock drifts up as the lock appears,
				 * sinks slightly as it leaves. */
				int dy = server->lock_anim_in ?
					(int)((1.0f - e) * 14) :
					(int)(e * 10);
				wlr_scene_node_set_position(
					&server->lock_ui_node->node, 0, dy);
			}
			if (server->lock_dim) {
				const struct vile_palette *pal = vile_pal(server->theme_light);
				const float dim[4] = { pal->dim_rgb[0] * server->lock_op,
					pal->dim_rgb[1] * server->lock_op,
					pal->dim_rgb[2] * server->lock_op,
					0.30f * server->lock_op };
				wlr_scene_rect_set_color(server->lock_dim, dim);
			}
		}
	}

	/* --- Close fade: snapshot fades out over the re-tiled desktop. --- */
	if (server->close_fade_node) {
		int cdur = vile_anim_dur_for(180);
		float p = cdur > 0 ?
			(float)(now - server->close_fade_ms) / cdur : 1.0f;
		if (p >= 1.0f) {
			wlr_scene_node_destroy(&server->close_fade_node->node);
			server->close_fade_node = NULL;
			server->close_fade_ms = 0;
		} else {
			float e = vile_anim_ease_out_cubic(p);
			wlr_scene_buffer_set_opacity(server->close_fade_node,
				1.0f - e);
			active = true;
		}
	}

	return active;
}

/* Lay out every mapped window per the master-stack rule above.
 * Fullscreen: the fullscreen window fills the whole output and
 * every other window (and every border) is hidden until it is
 * toggled off. Never tiles while locked — the lock hides all
 * windows, and vile_lock_destroy re-tiles after unlocking. */
static void vile_tile(struct vile_server *server) {
	if (server->locked) {
		return;
	}
	if (wl_list_empty(&server->outputs)) {
		return;
	}
	struct vile_output *output =
		wl_container_of(server->outputs.next, output, link);
	int out_w = output->wlr_output->width;
	int out_h = output->wlr_output->height;

	if (server->fullscreen_toplevel) {
		struct vile_toplevel *fs = server->fullscreen_toplevel;
		struct vile_toplevel *t;
		wl_list_for_each(t, &server->toplevels, link) {
			bool is_fs = (t == fs);
			wlr_scene_node_set_enabled(&t->scene_tree->node, is_fs);
			if (t->border) {
				wlr_scene_node_set_enabled(&t->border->node, false);
			}
			if (!is_fs) {
				continue;
			}
			fs->tile_x = 0; fs->tile_y = 0;
			fs->tile_w = out_w; fs->tile_h = out_h;
			/* Node position is the VISIBLE top-left — do NOT
			 * subtract geometry here. (Measured: foot's
			 * geometry.y is -26, so the old (x - geo.x,
			 * y - geo.y) put the window 26px too low: the
			 * top border showed 30px and the bottom 14px
			 * ran off-screen. The scene tree already
			 * accounts for geometry.) */
			wlr_scene_node_set_position(&fs->scene_tree->node, 0, 0);
			wlr_xdg_toplevel_set_size(fs->xdg_toplevel, out_w, out_h);
			wlr_log(WLR_INFO, "Vile: tiling: fullscreen '%s' %dx%d",
				fs->xdg_toplevel->title ? fs->xdg_toplevel->title : "?",
				out_w, out_h);
		}
		/* Piece 5: a fullscreen window covers the bar and the dock,
		 * like every other OS — hide the shell tree until it is
		 * toggled off again. */
		if (server->shell_created) {
			wlr_scene_node_set_enabled(&server->shell_tree->node, false);
		}
		vile_schedule_frames(server);
		return;
	}

	/* Piece 5: not fullscreen — the bar and dock are visible again. */
	if (server->shell_created) {
		wlr_scene_node_set_enabled(&server->shell_tree->node, true);
	}

	int n = vile_tile_count(server);
	if (n == 0) {
		vile_schedule_frames(server);
		return;
	}
	/* Stable order: creation sequence, oldest first. */
	struct vile_toplevel **arr = malloc(sizeof(*arr) * (size_t)n);
	if (!arr) {
		return;
	}
	int i = 0;
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		arr[i++] = t;
	}
	qsort(arr, (size_t)n, sizeof(*arr), vile_tile_cmp);
	/* How many windows the layout itself places (floating /
	 * maximized / minimized windows are handled below, not by
	 * the master-stack). */
	int ntile = vile_tileable_count(server);
	int ti = 0; /* index among tileable windows */
	for (i = 0; i < n; i++) {
		t = arr[i];
		if (t->minimized) {
			/* Hidden until restored (dock click or
			 * Super+J/K focus) — the node AND the border
			 * stay off, and it takes no tile slot. */
			wlr_scene_node_set_enabled(&t->scene_tree->node, false);
			if (t->border) {
				wlr_scene_node_set_enabled(&t->border->node, false);
			}
			continue;
		}
		if (t->maximized) {
			/* Fills the work area — the same rect a single
			 * tiled window gets (12px gaps, dock + bar
			 * clear), so the dock stays usable above it. */
			int x, y, w, h;
			vile_tile_rect(0, 1, out_w, out_h, &x, &y, &w, &h);
			t->tile_x = x; t->tile_y = y; t->tile_w = w; t->tile_h = h;
			wlr_scene_node_set_enabled(&t->scene_tree->node, true);
			wlr_scene_node_set_position(&t->scene_tree->node, x, y);
			if (t->border) {
				wlr_scene_node_set_enabled(&t->border->node, false);
			}
			wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, h);
			wlr_log(WLR_INFO, "Vile: tiling: maximized '%s' at %d,%d %dx%d",
				t->xdg_toplevel->title ? t->xdg_toplevel->title : "?",
				x, y, w, h);
			continue;
		}
		if (t->floating) {
			/* Dragged out of the layout: it stays exactly
			 * where it was dropped, and the remaining
			 * tiled windows close ranks around it. */
			int x = t->float_x, y = t->float_y;
			int w = t->float_w, h = t->float_h;
			t->tile_x = x; t->tile_y = y; t->tile_w = w; t->tile_h = h;
			wlr_scene_node_set_enabled(&t->scene_tree->node, true);
			wlr_scene_node_set_position(&t->scene_tree->node, x, y);
			if (t->border) {
				wlr_scene_node_set_enabled(&t->border->node, true);
				wlr_scene_rect_set_size(t->border,
					w + VILE_BORDER * 2, h + VILE_BORDER * 2);
				wlr_scene_node_set_position(&t->border->node,
					x - VILE_BORDER, y - VILE_BORDER);
			}
			wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, h);
			wlr_log(WLR_INFO, "Vile: tiling: floating '%s' at %d,%d %dx%d",
				t->xdg_toplevel->title ? t->xdg_toplevel->title : "?",
				x, y, w, h);
			continue;
		}
		int x, y, w, h;
		vile_tile_rect(ti, ntile, out_w, out_h, &x, &y, &w, &h);
		ti++;
		t->tile_x = x; t->tile_y = y; t->tile_w = w; t->tile_h = h;
		/* Re-enable the window node: the fullscreen branch (and
		 * the lock) disable it, and only this branch puts it
		 * back. Without this, a window that was hidden by
		 * fullscreen stayed invisible after Super+F off — only
		 * its border showed. */
		wlr_scene_node_set_enabled(&t->scene_tree->node, true);
		/* Node position is the VISIBLE top-left — do NOT
		 * subtract geometry (see fullscreen branch above:
		 * foot's geometry.y is -26 and the subtraction
		 * shifted windows 26px down). */
		wlr_scene_node_set_position(&t->scene_tree->node, x, y);
		if (t->border) {
			wlr_scene_node_set_enabled(&t->border->node, true);
			wlr_scene_rect_set_size(t->border,
				w + VILE_BORDER * 2, h + VILE_BORDER * 2);
			wlr_scene_node_set_position(&t->border->node,
				x - VILE_BORDER, y - VILE_BORDER);
		}
		wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, h);
		wlr_log(WLR_INFO, "Vile: tiling: window %d/%d '%s' at %d,%d %dx%d",
			ti, ntile,
			t->xdg_toplevel->title ? t->xdg_toplevel->title : "?",
			x, y, w, h);
	}
	free(arr);
	vile_update_borders(server);
	/* Piece 5: the bar/dock sit above windows — re-raise them after
	 * tiling, or a newly positioned window covers them. */
	vile_shell_raise(server);
	vile_schedule_frames(server);
}

/* Focus the next (delta +1) or previous (delta -1) window in tile
 * (creation) order, wrapping around. */
static void vile_focus_direction(struct vile_server *server, int delta) {
	int n = vile_tile_count(server);
	if (n <= 1) {
		return;
	}
	struct vile_toplevel **arr = malloc(sizeof(*arr) * (size_t)n);
	if (!arr) {
		return;
	}
	int i = 0, idx = -1;
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		arr[i++] = t;
	}
	qsort(arr, (size_t)n, sizeof(*arr), vile_tile_cmp);
	for (i = 0; i < n; i++) {
		if (arr[i] == server->focused_toplevel) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		idx = 0;
	} else {
		idx = (idx + delta + n) % n;
	}
	struct vile_toplevel *next = arr[idx];
	free(arr);
	wlr_log(WLR_INFO, "Vile: keybind focus %s -> window %d/%d",
		delta > 0 ? "next" : "prev", idx + 1, n);
	vile_focus_toplevel(server, next);
}

/* Toggle compositor-side fullscreen on the focused window: it fills
 * the whole output and the other windows are hidden until toggled
 * off. This is a tiling fullscreen, not the xdg fullscreen state —
 * the client just gets resized to the full output. */
static void vile_toggle_fullscreen(struct vile_server *server) {
	struct vile_toplevel *t = server->focused_toplevel;
	if (!t || server->locked) {
		return;
	}
	/* A maximized window unmaximizes on Super+F (the maximize
	 * button's second click does the same) instead of going
	 * fullscreen — Super+F is the restore keybind. */
	if (t->maximized) {
		wlr_log(WLR_INFO, "Vile: keybind Super+F -> unmaximize");
		vile_unmaximize(server, t);
		return;
	}
	if (server->fullscreen_toplevel == t) {
		t->fullscreen = false;
		server->fullscreen_toplevel = NULL;
		wlr_log(WLR_INFO, "Vile: tiling: fullscreen off");
	} else {
		if (server->fullscreen_toplevel) {
			server->fullscreen_toplevel->fullscreen = false;
		}
		t->fullscreen = true;
		server->fullscreen_toplevel = t;
		wlr_log(WLR_INFO, "Vile: tiling: fullscreen on '%s'",
			t->xdg_toplevel->title ? t->xdg_toplevel->title : "?");
	}
	vile_tile(server);
	vile_focus_toplevel(server, t);
}

/* UI polish: open foot with the Vile banner (vile-foot.sh prints
 * it, then execs the user's interactive shell). The script lives
 * next to the compositor: vile-foot.sh in the compositor dir on
 * both this VM and Adnan's laptop. If it is not found, plain foot
 * (no banner) rather than no terminal at all. */
static void vile_spawn_foot(void) {
	const char *cands[] = {
		"vile-foot.sh",
		"compositor/vile-foot.sh",
		NULL,
	};
	for (int i = 0; cands[i]; i++) {
		if (access(cands[i], X_OK) == 0) {
			/* foot execs its shell command via execvp, which
			 * searches PATH, NOT the cwd — a relative path
			 * fails with "No such file or directory" even
			 * though the compositor found it (that is why
			 * every foot launch failed on the first bottom-bar
			 * run). Resolve to an absolute path first. */
			char abs[1100];
			if (cands[i][0] == '/') {
				snprintf(abs, sizeof(abs), "%s", cands[i]);
			} else {
				char cwd[1024];
				if (!getcwd(cwd, sizeof(cwd))) {
					continue;
				}
				snprintf(abs, sizeof(abs), "%s/%s", cwd, cands[i]);
			}
			if (access(abs, X_OK) != 0) {
				continue;
			}
			wlr_log(WLR_INFO, "Vile: opening foot with banner (%s)",
				abs);
			char cmd[1200];
			snprintf(cmd, sizeof(cmd), "foot \"%s\"", abs);
			vile_spawn(cmd);
			return;
		}
	}
	const char *home = getenv("HOME");
	if (home) {
		char path[1024];
		snprintf(path, sizeof(path),
			"%s/vile-os/compositor/vile-foot.sh", home);
		if (access(path, X_OK) == 0) {
			char cmd[1100];
			snprintf(cmd, sizeof(cmd), "foot \"%s\"", path);
			vile_spawn(cmd);
			return;
		}
		snprintf(path, sizeof(path),
			"%s/workspace/vile-os/compositor/vile-foot.sh", home);
		if (access(path, X_OK) == 0) {
			char cmd[1100];
			snprintf(cmd, sizeof(cmd), "foot \"%s\"", path);
			vile_spawn(cmd);
			return;
		}
	}
	wlr_log(WLR_ERROR,
		"Vile: vile-foot.sh not found, opening plain foot (no banner)");
	vile_spawn("foot");
}

/* Fork + run a command with /bin/sh. WAYLAND_DISPLAY is already in
 * the environment (main sets it before the backend starts serving
 * clients, and keybind spawns happen after that too). */
static void vile_spawn(const char *cmd) {
	if (!cmd || !*cmd) {
		return;
	}
	/* Every bare "foot" launch (startup -s foot, Super+Enter,
	 * dock click) goes through the banner script — one place,
	 * so the three paths cannot drift apart. */
	if (strcmp(cmd, "foot") == 0) {
		vile_spawn_foot();
		return;
	}
	wlr_log(WLR_INFO, "Vile: starting '%s'", cmd);
	if (fork() == 0) {
		execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
		_exit(1);
	}
}

/* Piece 7 — first-run Setup wizard ("before-installation
 * questions"). The marker ~/.config/vile/setup-done is written by
 * the wizard when the user finishes (or skips) it; while it is
 * missing, the wizard is the first thing on screen. VILE_NO_SETUP=1
 * skips the wizard (headless tests, which are about the desktop,
 * not first-run). */
static bool vile_setup_needed(void) {
	if (getenv("VILE_NO_SETUP")) {
		return false;
	}
	const char *home = getenv("HOME");
	if (!home) {
		return false;
	}
	char path[1024];
	snprintf(path, sizeof(path), "%s/.config/vile/setup-done", home);
	struct stat st;
	return stat(path, &st) != 0;
}

static void vile_spawn_setup(void) {
	const char *cands[] = {
		"../setup/vile-setup",
		"setup/vile-setup",
		NULL,
	};
	for (int i = 0; cands[i]; i++) {
		if (access(cands[i], X_OK) == 0) {
			wlr_log(WLR_INFO,
				"Vile: first run — opening Setup wizard (%s)",
				cands[i]);
			vile_spawn(cands[i]);
			return;
		}
	}
	const char *home = getenv("HOME");
	if (home) {
		char path[1024];
		snprintf(path, sizeof(path),
			"%s/vile-os/setup/vile-setup", home);
		if (access(path, X_OK) == 0) {
			wlr_log(WLR_INFO,
				"Vile: first run — opening Setup wizard (%s)",
				path);
			vile_spawn(path);
			return;
		}
		snprintf(path, sizeof(path),
			"%s/workspace/vile-os/setup/vile-setup", home);
		if (access(path, X_OK) == 0) {
			wlr_log(WLR_INFO,
				"Vile: first run — opening Setup wizard (%s)",
				path);
			vile_spawn(path);
			return;
		}
	}
	wlr_log(WLR_ERROR,
		"Vile: first run — Setup wizard launcher not found, "
		"going straight to the desktop");
}

/* ------------------------------------------------------------------ */
/* Focus + pointer hit-testing (Piece 3)                               */
/* ------------------------------------------------------------------ */

/* The topmost toplevel at layout coords (lx, ly), or NULL. Follows
 * tinywl's desktop_toplevel_at: wlr_scene_node_at finds the deepest
 * node, then walk up to the tree whose data is our toplevel. */
static struct vile_toplevel *vile_toplevel_at(struct vile_server *server,
		double lx, double ly, struct wlr_surface **surface,
		double *sx, double *sy) {
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer =
		wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}
	*surface = scene_surface->surface;
	struct wlr_scene_tree *tree = node->parent;
	while (tree && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	if (!tree) {
		return NULL;
	}
	return tree->node.data;
}

/* Give a window keyboard focus (NULL = focus nothing). Deactivates the
 * old window, raises the new one to the top, activates it, and tells
 * the seat so keys go to it. The most recently focused window goes
 * first in the list, so closing a window can refocus the previous one. */
static void vile_focus_toplevel(struct vile_server *server,
		struct vile_toplevel *toplevel) {
	/* A minimized window focused from anywhere (dock click,
	 * Super+J/K) comes back first — focus is the restore path. */
	if (toplevel && toplevel->minimized) {
		vile_unminimize(server, toplevel);
	}
	if (server->focused_toplevel == toplevel) {
		return;
	}
	if (server->focused_toplevel) {
		wlr_xdg_toplevel_set_activated(
			server->focused_toplevel->xdg_toplevel, false);
	}
	server->focused_toplevel = toplevel;
	if (!toplevel) {
		wlr_seat_keyboard_notify_clear_focus(server->seat);
		wlr_log(WLR_INFO, "Vile: keyboard focus cleared");
		vile_update_borders(server);
		return;
	}
	wl_list_remove(&toplevel->link);
	wl_list_insert(&server->toplevels, &toplevel->link);
	wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	if (server->locked) {
		vile_lock_raise(server);
	}
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(server->seat,
			toplevel->xdg_toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes,
			&keyboard->modifiers);
	}
	wlr_log(WLR_INFO, "Vile: keyboard focus -> '%s'",
		toplevel->xdg_toplevel->title ?
		toplevel->xdg_toplevel->title : "?");
	/* Piece 4: focus change repaints the borders (focused = purple). */
	vile_update_borders(server);
	/* Piece 5: focusing raises the window above the shell tree —
	 * put the bar/dock back on top. */
	vile_shell_raise(server);
}

/* Pointer moved: update which surface has pointer focus. The cursor
 * image still follows the pointer on the lock screen (so the user can
 * see where they are), but nothing gets focus and no clicks go
 * through while locked. */
static void vile_process_cursor_motion(struct vile_server *server,
		uint32_t time_msec) {
	if (server->locked) {
		return;
	}
	/* Dock reorder drag (Nov 10, 2026 ~16:31): while a reorder is
	 * live the pointer owns the dock — the dragged icon follows
	 * it and the insertion indicator moves. A press that has not
	 * become a reorder yet just tracks whether it moved >12px
	 * (a drag-away cancels the click). */
	if (server->dock_reorder) {
		server->dock_drag_x = server->cursor->x;
		vile_shell_render_dock(server);
		vile_schedule_frames(server);
		return;
	}
	if (server->dock_press_slot >= 0) {
		double dx = server->cursor->x - server->dock_press_x;
		double dy = server->cursor->y - server->dock_press_y;
		if (dx * dx + dy * dy > 144.0) { /* >12px */
			server->dock_press_moved = true;
		}
	}
	/* A pending grab fires once the pointer moves past the drag
	 * threshold; an active grab owns the pointer outright. Below
	 * the threshold the motion is just a hover / click, and the
	 * client keeps getting its pointer events. */
	if (vile_grab_maybe_fire(server)) {
		vile_grab_update(server);
		return;
	}
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct vile_toplevel *toplevel = vile_toplevel_at(server,
		server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	/* Piece 7: dock hover grow — the dock icons are painted into
	 * one buffer, so hover is a hit-test here + a re-render in the
	 * animation tick, not a separate node. */
	if (server->shell_created) {
		int hover = -1;
		int cx = (int)server->cursor->x, cy = (int)server->cursor->y;
		/* The hovered SLOT (0..dock_n-1), not a fixed app. */
		hover = vile_dock_slot_at(server, cx, cy);
		vile_anim_dock_hover(server, hover);
	}
	if (!toplevel) {
		/* No window under the cursor: show the default arrow. This
		 * is what makes the cursor appear when moving over the
		 * wallpaper, not just over windows. */
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr,
			"default");
	}
	if (surface) {
		wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(server->seat, time_msec, sx, sy);
	} else {
		wlr_seat_pointer_notify_clear_focus(server->seat);
	}
}

static void vile_cursor_motion(struct wl_listener *listener, void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. */
	wlr_cursor_move(server->cursor, &event->pointer->base,
		event->delta_x, event->delta_y);
	vile_process_cursor_motion(server, event->time_msec);
}

static void vile_cursor_motion_absolute(struct wl_listener *listener,
		void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base,
		event->x, event->y);
	vile_process_cursor_motion(server, event->time_msec);
}

static void vile_cursor_button(struct wl_listener *listener, void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	if (server->locked) {
		/* Clicks on the lock screen only hit lock UI (gear, settings
		 * panel) — they never reach windows. Only a press counts. */
		if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
			vile_lock_click(server, server->cursor->x,
				server->cursor->y);
		}
		return;
	}
	/* Make sure pointer focus is current before the click is sent. */
	vile_process_cursor_motion(server, event->time_msec);
	wlr_seat_pointer_notify_button(server->seat,
		event->time_msec, event->button, event->state);
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		/* A drag ends when the button comes up — the window
		 * stays exactly where the last motion put it. */
		if (server->grab_mode != 0) {
			vile_grab_end(server);
		} else if (server->pending_grab_mode != 0) {
			/* Plain click: the press never became a drag.
			 * The client already has press + release, so a
			 * CSD button works — just drop the pending grab. */
			vile_grab_disarm(server);
		}
		/* Dock press / reorder drop (no-op when nothing armed). */
		vile_dock_release(server);
		return;
	}
	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		/* Piece 5: the bar / dock / system menu eat the click
		 * first — they are shell territory, not windows. A click
		 * there must not clear window focus either. The button
		 * matters now (left = press-arm / menu, right = Unpin). */
		if (vile_shell_click(server, server->cursor->x,
				server->cursor->y, event->button)) {
			return;
		}
		/* Click focuses (and raises) the window under the cursor;
		 * click on empty space clears keyboard focus. */
		double sx, sy;
		struct wlr_surface *surface = NULL;
		struct vile_toplevel *toplevel = vile_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
		if (toplevel && !toplevel->minimized) {
			int cx = (int)server->cursor->x;
			int cy = (int)server->cursor->y;
			struct wlr_keyboard *kb =
				wlr_seat_get_keyboard(server->seat);
			uint32_t mods = kb ?
				wlr_keyboard_get_modifiers(kb) : 0;
			/* Super (Windows key) + drag anywhere on the
			 * window: left button moves, right button
			 * resizes from the nearest corner. The grab is
			 * armed, not started — a plain Super+click with
			 * no motion still reaches the client. */
			if ((mods & WLR_MODIFIER_LOGO) &&
					event->button == BTN_LEFT) {
				vile_grab_arm(server, toplevel, 1, 0);
				return;
			}
			if ((mods & WLR_MODIFIER_LOGO) &&
					event->button == BTN_RIGHT) {
				uint32_t edges = 0;
				int mx = toplevel->tile_x + toplevel->tile_w / 2;
				int my = toplevel->tile_y + toplevel->tile_h / 2;
				edges |= (cy < my) ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
				edges |= (cx < mx) ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
				vile_grab_arm(server, toplevel, 2, edges);
				return;
			}
			if (event->button == BTN_LEFT) {
				/* Title-bar drag: the top strip of the
				 * window (foot's CSD title bar / GTK
				 * header bar) arms a move. A plain click
				 * here (no motion) moves nothing — the
				 * button still reaches the client, so
				 * minimize / maximize / close keep
				 * working, including on a maximized window
				 * (the unmaximize + re-anchor now happens
				 * only once the drag actually starts). */
				if (cy - toplevel->tile_y < VILE_TITLE_H) {
					vile_grab_arm(server, toplevel, 1, 0);
					return;
				}
				/* Edge / corner drag on a floating window
				 * resizes it. */
				if (toplevel->floating) {
					uint32_t edges = 0;
					int x0 = toplevel->tile_x;
					int y0 = toplevel->tile_y;
					int x1 = x0 + toplevel->tile_w;
					int y1 = y0 + toplevel->tile_h;
					if (cy - y0 < VILE_EDGE_PX) {
						edges |= WLR_EDGE_TOP;
					} else if (y1 - cy < VILE_EDGE_PX) {
						edges |= WLR_EDGE_BOTTOM;
					}
					if (cx - x0 < VILE_EDGE_PX) {
						edges |= WLR_EDGE_LEFT;
					} else if (x1 - cx < VILE_EDGE_PX) {
						edges |= WLR_EDGE_RIGHT;
					}
					if (edges != 0) {
						vile_grab_arm(server,
							toplevel, 2, edges);
						return;
					}
				}
			}
		}
		vile_focus_toplevel(server, toplevel);
	}
}

static void vile_cursor_axis(struct wl_listener *listener, void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(server->seat,
		event->time_msec, event->orientation, event->delta,
		event->delta_discrete, event->source, event->relative_direction);
}

static void vile_cursor_frame(struct wl_listener *listener, void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, cursor_frame);
	wlr_seat_pointer_notify_frame(server->seat);
}

/* A client (e.g. foot) wants to set its own cursor image. Only honour
 * it if that client actually has pointer focus — any client can ask. */
static void vile_seat_request_cursor(struct wl_listener *listener,
		void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(server->cursor, event->surface,
			event->hotspot_x, event->hotspot_y);
	}
}

/* A client wants to set the clipboard selection (e.g. copy in foot).
 * We always honour it. */
static void vile_seat_request_set_selection(struct wl_listener *listener,
		void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

/* A virtual pointer (test client / automation) connected. It is a
 * normal pointer device as far as the cursor is concerned. */
static void vile_server_new_virtual_pointer(struct wl_listener *listener,
		void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, new_virtual_pointer);
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	wlr_cursor_attach_input_device(server->cursor,
		&event->new_pointer->pointer.base);
	wlr_log(WLR_INFO, "Vile: virtual pointer connected");
}

/* Is this client a Vile Files window? The Files title changes with
 * navigation (it is the current folder name), so the title cannot
 * identify it. The client process cmdline is stable: the launcher
 * execs python3 with vile-files.py as argv[1]. Cached on the
 * toplevel at map time (once per window, not once a second). */
static bool vile_client_is_files(struct wl_client *client) {
	pid_t pid;
	uid_t uid;
	gid_t gid;
	wl_client_get_credentials(client, &pid, &uid, &gid);
	if (pid <= 0) {
		return false;
	}
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}
	char buf[1024];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';
	/* cmdline is NUL-separated argv — a plain strstr stops at the
	 * first NUL ("python3") and never sees argv[1], so walk each
	 * token. (First version of this helper had exactly that bug:
	 * the dot never appeared.) */
	size_t off = 0;
	while (off < n) {
		if (strstr(buf + off, "vile-files.py") != NULL) {
			return true;
		}
		off += strlen(buf + off) + 1;
	}
	return false;
}

static void vile_toplevel_map(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel = wl_container_of(listener, toplevel, map);
	struct vile_server *server = toplevel->server;

	wl_list_insert(&server->toplevels, &toplevel->link);
	/* Dock: identify Files windows once, here. Title fallback
	 * covers a folder literally named "Files" (its title IS
	 * "Files" — and it is a Files window). The app_id check is a
	 * second path: GTK reports the script name as the app_id
	 * ("vile-files.py", proven in the dock test log). */
	toplevel->app_files = vile_client_is_files(
			toplevel->xdg_toplevel->base->client->client) ||
		(toplevel->xdg_toplevel->app_id &&
			strcmp(toplevel->xdg_toplevel->app_id,
				"vile-files.py") == 0) ||
		(toplevel->xdg_toplevel->title &&
			strncmp(toplevel->xdg_toplevel->title, "Files", 5) == 0);

	/* Focus the new window (raises it above the wallpaper, marks it
	 * active, tells the seat keys go to it) — unless locked, in which
	 * case windows stay under the lock overlay and unfocused. */
	if (server->locked) {
		/* Lock v2: windows are hidden entirely while locked — a new
		 * window must never appear behind (or above) the lock UI. */
		wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
		vile_lock_raise(server);
	} else {
		/* Piece 4: a new window cancels fullscreen so the user can
		 * see it, then every window is re-tiled (master-stack).
		 * The border is shown; vile_tile positions it. */
		if (server->fullscreen_toplevel) {
			server->fullscreen_toplevel->fullscreen = false;
			server->fullscreen_toplevel = NULL;
			wlr_log(WLR_INFO, "Vile: tiling: new window cancels fullscreen");
		}
		if (toplevel->border) {
			wlr_scene_node_set_enabled(&toplevel->border->node, true);
		}
		vile_tile(server);
		vile_focus_toplevel(server, toplevel);
		vile_shell_update(server);
		/* Piece 7: fade + scale in from the tile centre. */
		vile_anim_start_window_open(server, toplevel);
	}
	wlr_log(WLR_INFO, "Vile: window mapped '%s' app_id='%s' tile %d,%d %dx%d",
		toplevel->xdg_toplevel->title ? toplevel->xdg_toplevel->title : "?",
		toplevel->xdg_toplevel->app_id ? toplevel->xdg_toplevel->app_id : "?",
		toplevel->tile_x, toplevel->tile_y,
		toplevel->tile_w, toplevel->tile_h);
	vile_schedule_frames(server);
}

static void vile_toplevel_unmap(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
	struct vile_server *server = toplevel->server;
	bool was_focused = (server->focused_toplevel == toplevel);
	/* A window destroyed mid-drag must not leave a stale grab
	 * pointing at freed memory. */
	if (server->grab_toplevel == toplevel) {
		vile_grab_end(server);
	}
	if (server->pending_grab_toplevel == toplevel) {
		vile_grab_disarm(server);
	}
	/* Piece 7: snapshot the last frame and fade it out over the
	 * re-tiled desktop. Runs BEFORE the list removal below, while
	 * the tile rect and the surface buffer are still valid. */
	vile_anim_start_close_fade(server, toplevel);
	wl_list_remove(&toplevel->link);
	/* Piece 4: hide its border; if it was fullscreen, clear that
	 * state so the remaining windows can tile again. */
	if (toplevel->border) {
		wlr_scene_node_set_enabled(&toplevel->border->node, false);
	}
	if (toplevel->fullscreen) {
		toplevel->fullscreen = false;
		if (server->fullscreen_toplevel == toplevel) {
			server->fullscreen_toplevel = NULL;
		}
	}
	if (was_focused) {
		server->focused_toplevel = NULL;
		/* Refocus the most recently focused remaining window, if any. */
		if (!wl_list_empty(&server->toplevels)) {
			struct vile_toplevel *next = wl_container_of(
				server->toplevels.next, next, link);
			vile_focus_toplevel(server, next);
		} else {
			wlr_seat_keyboard_notify_clear_focus(server->seat);
		}
	}
	/* Piece 4: closing a window re-tiles the rest to fill the space. */
	if (!server->locked) {
		vile_tile(server);
	}
	/* Piece 5: the running-app dots change when a window closes. */
	vile_shell_update(server);
	wlr_log(WLR_INFO, "Vile: window unmapped");
	vile_schedule_frames(server);
}

static void vile_toplevel_commit(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel = wl_container_of(listener, toplevel, commit);
	struct vile_server *server = toplevel->server;
	if (toplevel->xdg_toplevel->base->initial_commit) {
		/* The client did its first commit and is waiting for us to
		 * tell it how big to be. Piece 4: give it its tile size —
		 * its place in the layout if it were mapped now (it is the
		 * newest window, so it goes last in creation order). */
		int out_w = VILE_OUT_W, out_h = VILE_OUT_H;
		if (!wl_list_empty(&server->outputs)) {
			struct vile_output *output = wl_container_of(
				server->outputs.next, output, link);
			out_w = output->wlr_output->width;
			out_h = output->wlr_output->height;
		}
		int n = vile_tileable_count(server) + 1;
		int x, y, w, h;
		vile_tile_rect(n - 1, n, out_w, out_h, &x, &y, &w, &h);
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, w, h);
		wlr_log(WLR_INFO, "Vile: window initial commit, configured %dx%d (tile %d/%d)",
			w, h, n, n);
		return;
	}
	vile_schedule_frames(toplevel->server);
}

static void vile_toplevel_destroy(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
	if (toplevel->server->focused_toplevel == toplevel) {
		toplevel->server->focused_toplevel = NULL;
	}
	if (toplevel->server->fullscreen_toplevel == toplevel) {
		toplevel->server->fullscreen_toplevel = NULL;
	}
	if (toplevel->server->pending_grab_toplevel == toplevel) {
		vile_grab_disarm(toplevel->server);
	}
	/* Piece 4: the border is our own scene node (not part of the
	 * xdg surface tree), so destroy it explicitly or it lingers. */
	if (toplevel->border) {
		wlr_scene_node_destroy(&toplevel->border->node);
		toplevel->border = NULL;
	}
	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);
	wl_list_remove(&toplevel->request_minimize.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wlr_log(WLR_INFO, "Vile: window destroyed");
	free(toplevel);
}

/* ------------------------------------------------------------------ */
/* Interactive move/resize + minimize/maximize (0.5.4 post-theme      */
/* fixes). A drag makes a tiled window floating at its current rect,  */
/* so the rest re-tile around it; the move itself is live — the node  */
/* position follows the pointer on every motion event, no snapping    */
/* back. The grab ends on button release.                             */
/* ------------------------------------------------------------------ */

#define VILE_FLOAT_MIN_W 200
#define VILE_FLOAT_MIN_H 120
/* A press only becomes a drag after the pointer moves this far
 * (pixels, either axis) — below it, it is a click. */
#define VILE_DRAG_THRESHOLD 4

/* Arm a pending grab on press — see the server struct comment. The
 * window is focused at once (click-focus); nothing moves yet. */
static void vile_grab_arm(struct vile_server *server,
		struct vile_toplevel *t, int mode, uint32_t edges) {
	if (!t || server->locked || server->grab_mode != 0 ||
			server->pending_grab_mode != 0) {
		return;
	}
	if (t->fullscreen || t->minimized) {
		return;
	}
	server->pending_grab_mode = mode;
	server->pending_grab_toplevel = t;
	server->pending_grab_x = server->cursor->x;
	server->pending_grab_y = server->cursor->y;
	server->pending_grab_edges = edges;
	vile_focus_toplevel(server, t);
}

/* Drop a pending grab without starting it (plain click release,
 * window gone, lock). */
static void vile_grab_disarm(struct vile_server *server) {
	server->pending_grab_mode = 0;
	server->pending_grab_toplevel = NULL;
	server->pending_grab_edges = 0;
}

/* Motion with a pending grab armed: fire it once the pointer has
 * moved past the drag threshold. Returns true if a grab is now
 * active (already running, or just fired). */
static bool vile_grab_maybe_fire(struct vile_server *server) {
	if (server->pending_grab_mode == 0) {
		return server->grab_mode != 0;
	}
	struct vile_toplevel *t = server->pending_grab_toplevel;
	double dx = server->cursor->x - server->pending_grab_x;
	double dy = server->cursor->y - server->pending_grab_y;
	double adx = dx < 0 ? -dx : dx;
	double ady = dy < 0 ? -dy : dy;
	if (adx <= VILE_DRAG_THRESHOLD && ady <= VILE_DRAG_THRESHOLD) {
		return false;
	}
	int mode = server->pending_grab_mode;
	uint32_t edges = server->pending_grab_edges;
	double px = server->pending_grab_x, py = server->pending_grab_y;
	vile_grab_disarm(server);
	if (!t) {
		return false;
	}
	if (mode == 1) {
		vile_grab_begin_move(server, t);
	} else {
		vile_grab_begin_resize(server, t, edges);
	}
	if (server->grab_mode != 0) {
		/* Anchor on the press point, not the fire point, so
		 * the window follows the pointer exactly from where
		 * the drag started (no shortfall on the first step). */
		server->grab_cursor_x = px;
		server->grab_cursor_y = py;
	}
	return server->grab_mode != 0;
}

static void vile_grab_end(struct vile_server *server) {
	struct vile_toplevel *t = server->grab_toplevel;
	if (t) {
		/* The authoritative final rect: vile_tile does not run
		 * during a drag, so the tiling log lines go stale
		 * mid-grab — tests read this line, not those. */
		wlr_log(WLR_INFO, "Vile: grab %s end '%s' at %d,%d %dx%d",
			server->grab_mode == 2 ? "resize" : "move",
			t->xdg_toplevel->title ?
			t->xdg_toplevel->title : "?",
			t->float_x, t->float_y, t->float_w, t->float_h);
	}
	if (t && server->grab_mode == 2) {
		wlr_xdg_toplevel_set_resizing(t->xdg_toplevel, false);
	}
	server->grab_mode = 0;
	server->grab_toplevel = NULL;
	server->grab_edges = 0;
}

/* Put a window's visible rect (and its border) at x,y,w,h right
 * now — the live part of a drag. tile_* stays in sync so the open
 * animation and the close fade use the same rect. */
static void vile_toplevel_place(struct vile_toplevel *t,
		int x, int y, int w, int h) {
	t->tile_x = x; t->tile_y = y; t->tile_w = w; t->tile_h = h;
	wlr_scene_node_set_position(&t->scene_tree->node, x, y);
	if (t->border) {
		wlr_scene_rect_set_size(t->border,
			w + VILE_BORDER * 2, h + VILE_BORDER * 2);
		wlr_scene_node_set_position(&t->border->node,
			x - VILE_BORDER, y - VILE_BORDER);
	}
	wlr_xdg_toplevel_set_size(t->xdg_toplevel, w, h);
}

static void vile_grab_begin_move(struct vile_server *server,
		struct vile_toplevel *t) {
	if (!t || server->locked || server->grab_mode != 0) {
		return;
	}
	if (t->fullscreen) {
		return;
	}
	if (t->minimized) {
		return;
	}
	/* Dragging a maximized window unmaximizes it first (like
	 * GNOME): it comes back at its old size, centred on the
	 * cursor's x with the title bar under the pointer, and the
	 * drag continues from there. */
	if (t->maximized) {
		vile_unmaximize(server, t);
		double cx = server->cursor->x, cy = server->cursor->y;
		t->float_x = (int)(cx - t->float_w / 2);
		t->float_y = (int)(cy - VILE_TITLE_H / 2);
		t->floating = true;
		vile_tile(server);
	}
	if (!t->floating) {
		/* Leave the layout: keep the current tile rect as the
		 * float rect, then re-tile the windows left behind. */
		t->float_x = t->tile_x; t->float_y = t->tile_y;
		t->float_w = t->tile_w; t->float_h = t->tile_h;
		t->floating = true;
		vile_tile(server);
	}
	/* A drag during the open animation would fight the tick
	 * (it repositions from tile_* every frame) — finish the
	 * animation now and hand the window to the pointer. */
	if (t->anim_active) {
		t->anim_active = false;
		if (t->surf_buf) {
			wlr_scene_buffer_set_opacity(t->surf_buf, 1.0f);
			wlr_scene_buffer_set_dest_size(t->surf_buf,
				t->tile_w, t->tile_h);
		}
		wlr_scene_node_set_position(&t->scene_tree->node,
			t->tile_x, t->tile_y);
	}
	server->grab_mode = 1;
	server->grab_toplevel = t;
	server->grab_cursor_x = server->cursor->x;
	server->grab_cursor_y = server->cursor->y;
	server->grab_start_x = t->float_x;
	server->grab_start_y = t->float_y;
	server->grab_start_w = t->float_w;
	server->grab_start_h = t->float_h;
	vile_focus_toplevel(server, t);
	wlr_log(WLR_INFO, "Vile: grab move '%s' from %d,%d %dx%d",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?",
		t->float_x, t->float_y, t->float_w, t->float_h);
}

static void vile_grab_begin_resize(struct vile_server *server,
		struct vile_toplevel *t, uint32_t edges) {
	if (!t || server->locked || server->grab_mode != 0 || edges == 0) {
		return;
	}
	if (t->fullscreen || t->maximized || t->minimized) {
		return;
	}
	if (!t->floating) {
		t->float_x = t->tile_x; t->float_y = t->tile_y;
		t->float_w = t->tile_w; t->float_h = t->tile_h;
		t->floating = true;
		vile_tile(server);
	}
	if (t->anim_active) {
		t->anim_active = false;
		if (t->surf_buf) {
			wlr_scene_buffer_set_opacity(t->surf_buf, 1.0f);
			wlr_scene_buffer_set_dest_size(t->surf_buf,
				t->tile_w, t->tile_h);
		}
		wlr_scene_node_set_position(&t->scene_tree->node,
			t->tile_x, t->tile_y);
	}
	server->grab_mode = 2;
	server->grab_toplevel = t;
	server->grab_cursor_x = server->cursor->x;
	server->grab_cursor_y = server->cursor->y;
	server->grab_start_x = t->float_x;
	server->grab_start_y = t->float_y;
	server->grab_start_w = t->float_w;
	server->grab_start_h = t->float_h;
	server->grab_edges = edges;
	wlr_xdg_toplevel_set_resizing(t->xdg_toplevel, true);
	vile_focus_toplevel(server, t);
	wlr_log(WLR_INFO, "Vile: grab resize '%s' edges %u from %d,%d %dx%d",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?",
		edges, t->float_x, t->float_y, t->float_w, t->float_h);
}

/* Pointer moved during a grab: move/resize the window live. Called
 * from the motion path before any normal pointer-focus work. */
static void vile_grab_update(struct vile_server *server) {
	struct vile_toplevel *t = server->grab_toplevel;
	if (!t || server->grab_mode == 0) {
		return;
	}
	int out_w = VILE_OUT_W, out_h = VILE_OUT_H;
	if (!wl_list_empty(&server->outputs)) {
		struct vile_output *output = wl_container_of(
			server->outputs.next, output, link);
		out_w = output->wlr_output->width;
		out_h = output->wlr_output->height;
	}
	int dx = (int)(server->cursor->x - server->grab_cursor_x);
	int dy = (int)(server->cursor->y - server->grab_cursor_y);
	if (server->grab_mode == 1) {
		/* Move: keep the title bar reachable — at least 80px of
		 * the window stays on-screen left/right, and the top
		 * never goes above the screen (or it could never be
		 * grabbed again). */
		int x = server->grab_start_x + dx;
		int y = server->grab_start_y + dy;
		int w = server->grab_start_w, h = server->grab_start_h;
		if (x < -(w - 80)) x = -(w - 80);
		if (x > out_w - 80) x = out_w - 80;
		if (y < 0) y = 0;
		if (y > out_h - VILE_TITLE_H) y = out_h - VILE_TITLE_H;
		t->float_x = x; t->float_y = y;
		t->float_w = w; t->float_h = h;
		vile_toplevel_place(t, x, y, w, h);
	} else {
		/* Resize: edges move independently, minimum 200x120,
		 * clamped inside the output. */
		int x = server->grab_start_x, y = server->grab_start_y;
		int w = server->grab_start_w, h = server->grab_start_h;
		uint32_t e = server->grab_edges;
		if (e & WLR_EDGE_LEFT) {
			x = server->grab_start_x + dx;
			w = server->grab_start_w - dx;
			if (w < VILE_FLOAT_MIN_W) {
				x = server->grab_start_x + server->grab_start_w -
					VILE_FLOAT_MIN_W;
				w = VILE_FLOAT_MIN_W;
			}
			if (x < 0) {
				w += x;
				x = 0;
			}
		}
		if (e & WLR_EDGE_RIGHT) {
			w = server->grab_start_w + dx;
			if (w < VILE_FLOAT_MIN_W) w = VILE_FLOAT_MIN_W;
			if (x + w > out_w) w = out_w - x;
		}
		if (e & WLR_EDGE_TOP) {
			y = server->grab_start_y + dy;
			h = server->grab_start_h - dy;
			if (h < VILE_FLOAT_MIN_H) {
				y = server->grab_start_y + server->grab_start_h -
					VILE_FLOAT_MIN_H;
				h = VILE_FLOAT_MIN_H;
			}
			if (y < 0) {
				h += y;
				y = 0;
			}
		}
		if (e & WLR_EDGE_BOTTOM) {
			h = server->grab_start_h + dy;
			if (h < VILE_FLOAT_MIN_H) h = VILE_FLOAT_MIN_H;
			if (y + h > out_h) h = out_h - y;
		}
		t->float_x = x; t->float_y = y;
		t->float_w = w; t->float_h = h;
		vile_toplevel_place(t, x, y, w, h);
	}
	vile_schedule_frames(server);
}

static void vile_minimize(struct vile_server *server,
		struct vile_toplevel *t) {
	if (!t || t->minimized) {
		return;
	}
	if (server->grab_toplevel == t) {
		vile_grab_end(server);
	}
	/* Minimizing leaves fullscreen first (the button is not
	 * visible in fullscreen, but the protocol path can still
	 * ask). */
	if (t->fullscreen) {
		t->fullscreen = false;
		if (server->fullscreen_toplevel == t) {
			server->fullscreen_toplevel = NULL;
		}
	}
	t->minimized = true;
	wlr_xdg_toplevel_set_activated(t->xdg_toplevel, false);
	if (server->focused_toplevel == t) {
		server->focused_toplevel = NULL;
		/* Focus the most recently focused other visible window,
		 * or clear focus if none is left. */
		struct vile_toplevel *next = NULL, *o;
		wl_list_for_each(o, &server->toplevels, link) {
			if (o != t && !o->minimized) {
				next = o;
				break;
			}
		}
		if (next) {
			vile_focus_toplevel(server, next);
		} else {
			wlr_seat_keyboard_notify_clear_focus(server->seat);
		}
	}
	vile_tile(server);
	vile_shell_update(server);
	wlr_log(WLR_INFO, "Vile: minimized '%s'",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?");
}

static void vile_unminimize(struct vile_server *server,
		struct vile_toplevel *t) {
	if (!t || !t->minimized) {
		return;
	}
	t->minimized = false;
	vile_tile(server);
	vile_shell_update(server);
	wlr_log(WLR_INFO, "Vile: restored '%s'",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?");
}

static void vile_maximize(struct vile_server *server,
		struct vile_toplevel *t) {
	if (!t || t->maximized) {
		return;
	}
	if (server->grab_toplevel == t) {
		vile_grab_end(server);
	}
	if (t->fullscreen) {
		t->fullscreen = false;
		if (server->fullscreen_toplevel == t) {
			server->fullscreen_toplevel = NULL;
		}
	}
	/* Remember where it was, so unmaximize puts it back. */
	t->max_was_floating = t->floating;
	if (t->floating) {
		t->max_prev_x = t->float_x; t->max_prev_y = t->float_y;
		t->max_prev_w = t->float_w; t->max_prev_h = t->float_h;
	} else {
		t->max_prev_x = t->tile_x; t->max_prev_y = t->tile_y;
		t->max_prev_w = t->tile_w; t->max_prev_h = t->tile_h;
	}
	t->maximized = true;
	wlr_xdg_toplevel_set_maximized(t->xdg_toplevel, true);
	vile_tile(server);
	vile_focus_toplevel(server, t);
	wlr_log(WLR_INFO, "Vile: maximized '%s'",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?");
}

static void vile_unmaximize(struct vile_server *server,
		struct vile_toplevel *t) {
	if (!t || !t->maximized) {
		return;
	}
	t->maximized = false;
	if (t->max_was_floating) {
		t->floating = true;
		t->float_x = t->max_prev_x; t->float_y = t->max_prev_y;
		t->float_w = t->max_prev_w; t->float_h = t->max_prev_h;
	} else {
		t->floating = false;
	}
	wlr_xdg_toplevel_set_maximized(t->xdg_toplevel, false);
	vile_tile(server);
	wlr_log(WLR_INFO, "Vile: unmaximized '%s'",
		t->xdg_toplevel->title ? t->xdg_toplevel->title : "?");
}

static void vile_toplevel_request_maximize(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	struct vile_server *server = toplevel->server;
	if (!toplevel->xdg_toplevel->base->initialized) {
		return;
	}
	/* The old stub only scheduled a configure, so foot's maximize
	 * button did nothing visible. Now the window really fills the
	 * work area (and comes back on a second click). */
	bool want = toplevel->xdg_toplevel->requested.maximized;
	if (want) {
		vile_maximize(server, toplevel);
	} else {
		vile_unmaximize(server, toplevel);
	}
}

static void vile_toplevel_request_minimize(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_minimize);
	struct vile_server *server = toplevel->server;
	if (!toplevel->xdg_toplevel->base->initialized) {
		return;
	}
	/* Minimized is not a configure state in xdg-shell, so the
	 * ack is a plain configure — the hide/show is ours. The
	 * window comes back from a dock click or Super+J/K focus. */
	bool want = toplevel->xdg_toplevel->requested.minimized;
	if (want) {
		vile_minimize(server, toplevel);
	} else {
		vile_unminimize(server, toplevel);
	}
	wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
}

static void vile_toplevel_request_move(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_move);
	struct vile_server *server = toplevel->server;
	/* Client-initiated move (foot's title-bar drag, GTK header
	 * bar drag): the compositor takes the pointer and the window
	 * follows it until release. */
	vile_grab_begin_move(server, toplevel);
}

static void vile_toplevel_request_resize(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_resize);
	struct vile_server *server = toplevel->server;
	struct wlr_xdg_toplevel_resize_event *event = data;
	/* Client-initiated resize (window edge drag). */
	vile_grab_begin_resize(server, toplevel, event->edges);
}

static void vile_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
	struct vile_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	struct vile_server *server = toplevel->server;
	if (!toplevel->xdg_toplevel->base->initialized) {
		return;
	}
	/* Client asked for fullscreen (or to leave it): honour it with
	 * our tiling fullscreen — full output, other windows hidden. */
	bool want = toplevel->xdg_toplevel->requested.fullscreen;
	if (want && server->fullscreen_toplevel != toplevel) {
		if (server->fullscreen_toplevel) {
			server->fullscreen_toplevel->fullscreen = false;
		}
		toplevel->fullscreen = true;
		server->fullscreen_toplevel = toplevel;
		wlr_log(WLR_INFO, "Vile: tiling: client requested fullscreen");
	} else if (!want && server->fullscreen_toplevel == toplevel) {
		toplevel->fullscreen = false;
		server->fullscreen_toplevel = NULL;
		wlr_log(WLR_INFO, "Vile: tiling: client left fullscreen");
	}
	vile_tile(server);
	vile_focus_toplevel(server, toplevel);
}

static void vile_server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	struct vile_server *server = wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	struct vile_toplevel *toplevel = calloc(1, sizeof(*toplevel));
	if (!toplevel) {
		return;
	}
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_toplevel;
	/* Piece 4: creation sequence — the stable tile order (oldest =
	 * master). */
	toplevel->seq = server->next_seq++;
	/* The scene tree is added to the scene root AFTER the wallpaper
	 * node, so the window draws on top of the wallpaper. */
	toplevel->scene_tree = wlr_scene_xdg_surface_create(
		&server->scene->tree, xdg_toplevel->base);
	/* Piece 4: the border rect is our own scene-root node, created
	 * hidden and placed BELOW the window tree so it frames the
	 * window instead of covering it. */
	{
		const struct vile_palette *pal = vile_pal(server->theme_light);
		const float dim[4] = { pal->border_dim[0], pal->border_dim[1],
			pal->border_dim[2], 1.0f };
		toplevel->border = wlr_scene_rect_create(
			&server->scene->tree, 8, 8, dim);
		if (toplevel->border) {
			wlr_scene_node_set_enabled(&toplevel->border->node, false);
			wlr_scene_node_place_below(&toplevel->border->node,
				&toplevel->scene_tree->node);
		}
	}
	toplevel->scene_tree->node.data = toplevel;
	xdg_toplevel->base->data = toplevel->scene_tree;

	toplevel->map.notify = vile_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = vile_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->commit.notify = vile_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);
	toplevel->destroy.notify = vile_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);
	toplevel->request_maximize.notify = vile_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize,
		&toplevel->request_maximize);
	toplevel->request_fullscreen.notify = vile_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen,
		&toplevel->request_fullscreen);
	toplevel->request_minimize.notify = vile_toplevel_request_minimize;
	wl_signal_add(&xdg_toplevel->events.request_minimize,
		&toplevel->request_minimize);
	toplevel->request_move.notify = vile_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move,
		&toplevel->request_move);
	toplevel->request_resize.notify = vile_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize,
		&toplevel->request_resize);
	wlr_log(WLR_INFO, "Vile: new xdg toplevel, waiting for its first commit");
}

static void vile_popup_commit(struct wl_listener *listener, void *data) {
	struct vile_popup *popup = wl_container_of(listener, popup, commit);
	if (popup->xdg_popup->base->initial_commit) {
		wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
	}
}

static void vile_popup_destroy(struct wl_listener *listener, void *data) {
	struct vile_popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->destroy.link);
	free(popup);
}

static void vile_server_new_xdg_popup(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup *xdg_popup = data;
	struct vile_popup *popup = calloc(1, sizeof(*popup));
	if (!popup) {
		return;
	}
	popup->xdg_popup = xdg_popup;
	/* Popups belong to their parent surface's scene tree. */
	struct wlr_xdg_surface *parent =
		wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
	assert(parent != NULL);
	struct wlr_scene_tree *parent_tree = parent->data;
	xdg_popup->base->data =
		wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

	popup->commit.notify = vile_popup_commit;
	wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);
	popup->destroy.notify = vile_popup_destroy;
	wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}

/* ------------------------------------------------------------------ */
/* Lock screen (queued after Piece 2)                                  */
/* ------------------------------------------------------------------ */

/* Design v2 (Adnan, Nov 9 ~22:41, video ref @ilyamiro.dev): the v1
 * lock showed the dimmed foot terminal + its title bar bleeding
 * through behind a big dark box — Adnan: take the terminal off the
 * back. So v2 is like the video: the ONLY thing behind the lock UI is
 * the wallpaper, blurred + dimmed, with NO windows at all. Windows are
 * hidden entirely while locked (their scene nodes are disabled, and
 * any window that maps while locked starts hidden). The UI floats
 * directly on the blurred wallpaper — no big box:
 *   - HUGE clock on top (Inter Light ~150px), date below it
 *   - user card below that: avatar circle (purple V placeholder) +
 *     username 'adnan' + LOCKED label + password field to the right,
 *     masked dots, Enter to unlock
 *   - settings gear bottom-right -> panel (Hide password toggle,
 *     Reveal delay, System Reboot / Power Off)
 *   - minimal bottom-centre status text
 * The Nov 9 v1 hyprlock froze on password+Enter — so the unlock path
 * is a REAL PAM check (pam_unix against the user's system password,
 * service "vile" in /etc/pam.d/vile — see pam.d/vile in this repo).
 * There is NO hardcoded password anywhere in Vile.
 *
 * Mechanism: a lock overlay INSIDE the compositor (not
 * ext-session-lock). The overlay reuses the scene graph we already
 * have, and the unlock check is still real PAM. While locked, keys go
 * only to the password handler — never to clients — and clicks only
 * hit lock UI (gear / settings panel). ext-session-lock remains a
 * possible later step.
 *
 * Test path (headless): VILE_LOCK=1 VILE_LOCK_USER=viletest, type the
 * password with `wtype` (virtual keyboard), Enter. Correct password
 * must hide the overlay (log "unlock OK"); wrong password must keep
 * the screen locked and show "Wrong password" (log "unlock FAILED").
 * Screenshots must show NO terminal behind the lock screen. */

/* Lock v2: the UI surface is FULL-SCREEN (the output size, stored in
 * server->lock_out_w/h) — clock, date, user card, settings panel and
 * bottom text are all drawn directly onto it, floating on the blurred
 * wallpaper. There is no panel/box behind them. */

struct vile_keyboard {
	struct wl_list link;
	struct vile_server *server;
	struct wlr_keyboard *wlr_keyboard;
	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

static void vile_schedule_frames(struct vile_server *server);

/* --- text helpers (Cairo + Pango) --- */

static void vile_draw_text(cairo_t *cr, const char *font,
		double r, double g, double b, double y,
		const char *text, int width) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, text, -1);
	/* Centre by hand: measure the text, then draw it at
	 * (width - text_width) / 2. Do NOT also set a layout width with
	 * PANGO_ALIGN_CENTER — that centres it a second time and pushes
	 * the text right, off the edge (that was the first-run bug: the
	 * clock rendered right-aligned and cut off). */
	int tw = 0, th = 0;
	pango_layout_get_pixel_size(layout, &tw, &th);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_move_to(cr, (width - tw) / 2.0, y);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	pango_font_description_free(desc);
}

static void vile_rounded_rect(cairo_t *cr,
		double x, double y, double w, double h, double r) {
	cairo_move_to(cr, x + r, y);
	cairo_line_to(cr, x + w - r, y);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
	cairo_line_to(cr, x + w, y + h - r);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
	cairo_line_to(cr, x + r, y + h);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
	cairo_line_to(cr, x, y + r);
	cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
	cairo_close_path(cr);
}

/* Draw text left-aligned at (x, y) (top-left of the text). */
static void vile_draw_text_at(cairo_t *cr, const char *font,
		double r, double g, double b, double x, double y,
		const char *text) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, text, -1);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_move_to(cr, x, y);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	pango_font_description_free(desc);
}

/* Draw text centred on the point (cx, cy). */
static void vile_draw_text_centre(cairo_t *cr, const char *font,
		double r, double g, double b, double cx, double cy,
		const char *text) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, text, -1);
	int tw = 0, th = 0;
	pango_layout_get_pixel_size(layout, &tw, &th);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_move_to(cr, cx - tw / 2.0, cy - th / 2.0);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	pango_font_description_free(desc);
}

/* Measure text width in pixels at the given font (for layout math). */
static int vile_text_width(cairo_t *cr, const char *font, const char *text) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, text, -1);
	int tw = 0, th = 0;
	pango_layout_get_pixel_size(layout, &tw, &th);
	g_object_unref(layout);
	pango_font_description_free(desc);
	return tw;
}

/* Small padlock glyph, drawn with Cairo (no emoji font needed).
 * (x, y) is the top-left, s is the overall size. */
static void vile_draw_lock_glyph(cairo_t *cr, double x, double y, double s,
		double r, double g, double b) {
	/* cairo_new_path first: cairo_arc draws a line from the CURRENT
	 * point to the arc start — after drawing the username text above,
	 * that current point is up at the text, which painted a stray
	 * vertical line from the name down to the padlock (v2 bug, seen
	 * in vile-lock-10.png). */
	cairo_new_path(cr);
	cairo_set_source_rgb(cr, r, g, b);
	cairo_set_line_width(cr, s * 0.09);
	/* Shackle: an arc above the body. */
	cairo_arc(cr, x + s / 2, y + s * 0.42, s * 0.22, M_PI, 0);
	cairo_stroke(cr);
	/* Body: rounded rect. */
	vile_rounded_rect(cr, x + s * 0.08, y + s * 0.38,
		s * 0.84, s * 0.55, s * 0.08);
	cairo_fill(cr);
	/* Keyhole dot. */
	cairo_set_source_rgb(cr, 0.04, 0.03, 0.08);
	cairo_arc(cr, x + s / 2, y + s * 0.62, s * 0.07, 0, 2 * M_PI);
	cairo_fill(cr);
}

/* Small gear glyph for the settings button (cx, cy centre, r radius). */
static void vile_draw_gear(cairo_t *cr, double cx, double cy, double r,
		double red, double green, double blue) {
	cairo_set_source_rgb(cr, red, green, blue);
	cairo_set_line_width(cr, 2.5);
	for (int i = 0; i < 8; i++) {
		double a = i * M_PI / 4;
		cairo_move_to(cr, cx + cos(a) * r * 0.72, cy + sin(a) * r * 0.72);
		cairo_line_to(cr, cx + cos(a) * r * 1.05, cy + sin(a) * r * 1.05);
	}
	cairo_stroke(cr);
	cairo_arc(cr, cx, cy, r * 0.55, 0, 2 * M_PI);
	cairo_stroke(cr);
}

/* On/off toggle pill at (x, y), size w x h. Knob right = on. */
static void vile_draw_toggle(cairo_t *cr, double x, double y,
		double w, double h, bool on) {
	if (on) {
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
	} else {
		cairo_set_source_rgb(cr, 0.45, 0.45, 0.52);
	}
	vile_rounded_rect(cr, x, y, w, h, h / 2);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	double kx = on ? x + w - h / 2 - 3 : x + h / 2 + 3;
	cairo_arc(cr, kx, y + h / 2, h / 2 - 4, 0, 2 * M_PI);
	cairo_fill(cr);
}

/* Paint a Cairo ARGB32 surface into a wlroots buffer (same pattern as
 * the wallpaper painter). Cairo's ARGB32 bytes match DRM_FORMAT_ARGB8888
 * (premultiplied) on little-endian. */
static struct wlr_buffer *vile_paint_cairo(struct vile_server *server,
		cairo_surface_t *surf, int width, int height) {
	unsigned char *pixels = cairo_image_surface_get_data(surf);
	int stride = cairo_image_surface_get_stride(surf);
	struct wlr_texture *tex = wlr_texture_from_pixels(server->renderer,
		DRM_FORMAT_ARGB8888, stride, width, height, pixels);
	if (!tex) {
		wlr_log(WLR_ERROR, "Vile: lock UI texture failed");
		return NULL;
	}
	const struct wlr_drm_format_set *fmts =
		wlr_renderer_get_render_formats(server->renderer);
	const struct wlr_drm_format *fmt = NULL;
	if (fmts) {
		fmt = wlr_drm_format_set_get(fmts, DRM_FORMAT_ARGB8888);
		if (!fmt) {
			fmt = wlr_drm_format_set_get(fmts, DRM_FORMAT_XRGB8888);
		}
	}
	if (!fmt) {
		wlr_log(WLR_ERROR, "Vile: lock UI: no render format");
		wlr_texture_destroy(tex);
		return NULL;
	}
	struct wlr_buffer *buf = wlr_allocator_create_buffer(server->allocator,
		width, height, fmt);
	if (!buf) {
		wlr_log(WLR_ERROR, "Vile: lock UI buffer alloc failed");
		wlr_texture_destroy(tex);
		return NULL;
	}
	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(server->renderer, buf, NULL);
	if (!pass) {
		wlr_log(WLR_ERROR, "Vile: lock UI render pass failed");
		wlr_buffer_drop(buf);
		wlr_texture_destroy(tex);
		return NULL;
	}
	struct wlr_render_texture_options opts = {
		.texture = tex,
		.dst_box = { .x = 0, .y = 0, .width = width, .height = height },
		.filter_mode = WLR_SCALE_FILTER_BILINEAR,
	};
	wlr_render_pass_add_texture(pass, &opts);
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "Vile: lock UI render pass submit failed");
		wlr_buffer_drop(buf);
		wlr_texture_destroy(tex);
		return NULL;
	}
	wlr_texture_destroy(tex);
	return buf;
}

/* No Arch logo overlay (Nov 10, 2026 ~16:31 — Adnan, twice:
 * the logo must not be stamped on ANY wallpaper. The three
 * overlay functions (load / fit / reload) were deleted
 * entirely; the Arch wallpaper keeps its own baked-in logo,
 * nebula/dunes show clean. The arch-logo.png assets stay
 * on disk, unused.) */

/* Hide or show every application window. Lock v2: while locked, no
 * window is rendered at all — the lock background is the ONLY thing
 * behind the UI. Called from vile_lock_create (hide) and
 * vile_lock_destroy (show), and from the map handler for windows
 * that appear while locked. */
static void vile_lock_set_windows_visible(struct vile_server *server,
		bool visible) {
	struct vile_toplevel *toplevel;
	wl_list_for_each(toplevel, &server->toplevels, link) {
		wlr_scene_node_set_enabled(&toplevel->scene_tree->node,
			visible);
	}
}

/* Settings: load the user's profile picture for the lock avatar.
 * Path: ~/.config/vile/profile.png (written by Vile Settings).
 * Returns true if a picture is cached, false if there is none
 * (lock falls back to the purple V). The file is re-read only when
 * its mtime changes; pictures bigger than 512px are downsampled
 * to at most 512 on the long side so the per-second lock render
 * never has to convert a multi-megapixel photo. */
static bool vile_load_profile_avatar(struct vile_server *server) {
	const char *home = getenv("HOME");
	if (!home) {
		return server->profile_pixels != NULL;
	}
	char path[1024];
	snprintf(path, sizeof(path), "%s/.config/vile/profile.png", home);
	struct stat st;
	if (stat(path, &st) != 0) {
		if (server->profile_pixels) {
			stbi_image_free(server->profile_pixels);
			server->profile_pixels = NULL;
		}
		server->profile_mtime = 0;
		return false;
	}
	if (server->profile_pixels && server->profile_mtime == st.st_mtime) {
		return true;
	}
	int w = 0, h = 0, ch = 0;
	unsigned char *px = stbi_load(path, &w, &h, &ch, 4);
	if (!px) {
		wlr_log(WLR_ERROR, "Vile: could not decode profile picture '%s'",
			path);
		return server->profile_pixels != NULL;
	}
	/* Downsample big photos to <= 512px on the long side. */
	if (w > 512 || h > 512) {
		double scale = 512.0 / (w > h ? w : h);
		int nw = (int)(w * scale), nh = (int)(h * scale);
		unsigned char *small =
			malloc((size_t)nw * nh * 4);
		if (small) {
			for (int y = 0; y < nh; y++) {
				for (int x = 0; x < nw; x++) {
					int sx = (int)(x / scale);
					int sy = (int)(y / scale);
					memcpy(&small[(y * nw + x) * 4],
						&px[(sy * w + sx) * 4], 4);
				}
			}
			stbi_image_free(px);
			px = small;
			w = nw;
			h = nh;
		}
	}
	if (server->profile_pixels) {
		stbi_image_free(server->profile_pixels);
	}
	server->profile_pixels = px;
	server->profile_w = w;
	server->profile_h = h;
	server->profile_mtime = st.st_mtime;
	wlr_log(WLR_INFO, "Vile: profile picture loaded: %dx%d", w, h);
	return true;
}

/* Setup wizard: the display name the user picked ("What should Vile
 * call you?"). Path: ~/.config/vile/display-name (written by
 * vile-setup.py). The lock screen shows this instead of the system
 * username when the file exists; PAM still authenticates the system
 * user (server->lock_user) — this is display only. The file is a
 * few bytes, so it is read fresh on every lock render. */
static const char *vile_lock_display_name(struct vile_server *server) {
	static char name[128];
	const char *home = getenv("HOME");
	name[0] = '\0';
	if (!home) {
		return server->lock_user;
	}
	char path[1024];
	snprintf(path, sizeof(path), "%s/.config/vile/display-name", home);
	FILE *f = fopen(path, "r");
	if (!f) {
		return server->lock_user;
	}
	if (!fgets(name, sizeof(name), f)) {
		fclose(f);
		return server->lock_user;
	}
	fclose(f);
	/* Trim trailing newline / whitespace. */
	size_t len = strlen(name);
	while (len > 0 && (name[len - 1] == '\n' ||
	                   name[len - 1] == '\r' ||
	                   name[len - 1] == ' ' ||
	                   name[len - 1] == '\t')) {
		name[--len] = '\0';
	}
	if (len == 0) {
		return server->lock_user;
	}
	return name;
}

/* Draw the lock-screen avatar circle at (x, y), diameter d.
 * If the user set a profile picture in Settings, the photo is
 * centre-square-cropped and drawn inside the circle; otherwise
 * the purple V placeholder is drawn (the old behaviour). */
static void vile_draw_avatar(struct vile_server *server, cairo_t *cr,
		double x, double y, double d) {
	double acx = x + d / 2, acy = y + d / 2;
	if (vile_load_profile_avatar(server)) {
		int pw = server->profile_w, ph = server->profile_h;
		int s = pw < ph ? pw : ph; /* centre-square crop */
		int sx = (pw - s) / 2, sy = (ph - s) / 2;
		cairo_surface_t *img = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, s, s);
		unsigned char *dst = cairo_image_surface_get_data(img);
		int stride = cairo_image_surface_get_stride(img);
		/* stbi gives non-premultiplied RGBA; Cairo ARGB32 on
		 * little-endian wants premultiplied BGRA in memory. */
		for (int yy = 0; yy < s; yy++) {
			for (int xx = 0; xx < s; xx++) {
				const unsigned char *src =
					&server->profile_pixels[
						((sy + yy) * pw + (sx + xx)) * 4];
				unsigned char *out =
					&dst[yy * stride + xx * 4];
				unsigned a = src[3];
				out[0] = (unsigned char)(src[2] * a / 255); /* B */
				out[1] = (unsigned char)(src[1] * a / 255); /* G */
				out[2] = (unsigned char)(src[0] * a / 255); /* R */
				out[3] = (unsigned char)a;
			}
		}
		cairo_surface_mark_dirty(img);
		cairo_save(cr);
		cairo_new_path(cr);
		cairo_arc(cr, acx, acy, d / 2, 0, 2 * M_PI);
		cairo_clip(cr);
		cairo_translate(cr, x, y);
		cairo_scale(cr, d / s, d / s);
		cairo_set_source_surface(cr, img, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);
		cairo_surface_destroy(img);
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
		cairo_set_line_width(cr, 3);
		cairo_new_path(cr);
		cairo_arc(cr, acx, acy, d / 2 - 2, 0, 2 * M_PI);
		cairo_stroke(cr);
		return;
	}
	/* No custom picture: purple V placeholder. */
	cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
	cairo_new_path(cr);
	cairo_arc(cr, acx, acy, d / 2, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
	cairo_set_line_width(cr, 3);
	cairo_arc(cr, acx, acy, d / 2 - 2, 0, 2 * M_PI);
	cairo_stroke(cr);
	vile_draw_text_centre(cr, "Inter Bold 64", 1.0, 1.0, 1.0,
		acx, acy + 2, "V");
}

/* Draw the whole lock UI into the full-screen scene buffer node.
 * Lock v2 (video-style): the UI floats directly on the blurred
 * wallpaper — huge clock, date, user card (avatar + name + LOCKED +
 * password field), settings gear/panel, bottom status. NO box behind
 * any of it. Called on lock, on every keystroke, and once a second
 * (the clock ticks). */
static void vile_lock_render(struct vile_server *server) {
	if (!server->lock_ui_node) {
		return;
	}
	int W = server->lock_out_w > 0 ? server->lock_out_w : VILE_OUT_W;
	int H = server->lock_out_h > 0 ? server->lock_out_h : VILE_OUT_H;
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, W, H);
	cairo_t *cr = cairo_create(surf);

	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char time_str[16], date_str[64];
	const struct vile_palette *P = vile_pal(server->theme_light);
	strftime(time_str, sizeof(time_str), "%H:%M", &tm);
	strftime(date_str, sizeof(date_str), "%A, %B %d", &tm);

	/* HUGE clock, centred, then the date below it. */
	vile_draw_text_centre(cr, "Inter Light 150", P->text_hi[0], P->text_hi[1], P->text_hi[2],
		W / 2.0, H * 0.40, time_str);
	vile_draw_text_centre(cr, "Inter 30", P->date_hi[0], P->date_hi[1], P->date_hi[2],
		W / 2.0, H * 0.40 + 115, date_str);

	/* --- User card, centred as a group --- */
	double card_w = 640;
	double card_x = (W - card_w) / 2.0;
	double card_y = H * 0.63; /* 680 at 1080p */
	/* Avatar: the user's profile picture if Settings set one,
	 * else the purple V placeholder (drawn by the helper). */
	double av_d = 120, av_x = card_x, av_y = card_y;
	vile_draw_avatar(server, cr, av_x, av_y, av_d);
	/* Username to the right of the avatar — the display name from
	 * the Setup wizard when set, else the system username. */
	double tx = av_x + av_d + 28;
	vile_draw_text_at(cr, "Inter Bold 34", P->text_hi[0], P->text_hi[1], P->text_hi[2],
		tx, card_y + 8, vile_lock_display_name(server));
	/* Padlock glyph + LOCKED label below the username. */
	vile_draw_lock_glyph(cr, tx, card_y + 66, 22, P->text2[0], P->text2[1], P->text2[2]);
	vile_draw_text_at(cr, "Inter Bold 17", P->text2[0], P->text2[1], P->text2[2],
		tx + 30, card_y + 64, "LOCKED");
	/* Password field to the right of the avatar, below the name. */
	double fx = tx, fy = card_y + 108, fw = 360, fh = 60;
	cairo_set_source_rgba(cr, P->field_rgb[0], P->field_rgb[1], P->field_rgb[2], 0.65);
	vile_rounded_rect(cr, fx, fy, fw, fh, 14);
	cairo_fill(cr);
	if (server->lock_failed) {
		cairo_set_source_rgb(cr, 1.0, 0.36, 0.36);
	} else {
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
	}
	cairo_set_line_width(cr, 2.5);
	vile_rounded_rect(cr, fx, fy, fw, fh, 14);
	cairo_stroke(cr);
	/* Password content: masked dots, or the real text when the
	 * Hide-password toggle is off. */
	size_t n = server->lock_password_len;
	if (server->lock_show_password && n > 0) {
		vile_draw_text_centre(cr, "Inter 24", P->white[0], P->white[1], P->white[2],
			fx + fw / 2, fy + fh / 2, server->lock_password);
	} else {
		for (size_t i = 0; i < n; i++) {
			double cx = fx + fw / 2 + (i - (n - 1) / 2.0) * 28.0;
			cairo_set_source_rgb(cr, P->white[0], P->white[1], P->white[2]);
			cairo_arc(cr, cx, fy + fh / 2.0, 8, 0, 2 * M_PI);
			cairo_fill(cr);
		}
	}
	/* Hint / error line, centred under the card. */
	if (server->lock_failed) {
		vile_draw_text_centre(cr, "Inter 22", 1.0, 0.48, 0.48,
			W / 2.0, card_y + 208, "Wrong password — try again");
	} else if (n == 0) {
		vile_draw_text_centre(cr, "Inter 20", P->hint[0], P->hint[1], P->hint[2],
			W / 2.0, card_y + 208, "Type your password and press Enter");
	}

	/* --- Settings gear, bottom-right corner --- */
	double gx = W - 76, gy = H - 76, gs = 52;
	server->lock_gear_rect = (struct wlr_box){
		.x = (int)gx, .y = (int)gy, .width = (int)gs, .height = (int)gs };
	cairo_set_source_rgba(cr, P->field_rgb[0], P->field_rgb[1], P->field_rgb[2], 0.60);
	cairo_arc(cr, gx + gs / 2, gy + gs / 2, gs / 2, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.35);
	cairo_set_line_width(cr, 1.5);
	cairo_arc(cr, gx + gs / 2, gy + gs / 2, gs / 2, 0, 2 * M_PI);
	cairo_stroke(cr);
	vile_draw_gear(cr, gx + gs / 2, gy + gs / 2, 13, P->glyph[0], P->glyph[1], P->glyph[2]);

	/* --- Settings panel (only when the gear opened it) --- */
	server->lock_toggle_rect = (struct wlr_box){ 0 };
	server->lock_reboot_rect = (struct wlr_box){ 0 };
	server->lock_poweroff_rect = (struct wlr_box){ 0 };
	server->lock_close_rect = (struct wlr_box){ 0 };
	if (server->lock_settings_open) {
		double px = W - 24 - 320, py = H * 0.648, pw = 320, ph = 280;
		cairo_set_source_rgba(cr, P->field_rgb[0], P->field_rgb[1], P->field_rgb[2], 0.72);
		vile_rounded_rect(cr, px, py, pw, ph, 16);
		cairo_fill(cr);
		cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.12);
		cairo_set_line_width(cr, 1.5);
		vile_rounded_rect(cr, px, py, pw, ph, 16);
		cairo_stroke(cr);
		double ix = px + 24;
		vile_draw_text_at(cr, "Inter Bold 13", P->muted[0], P->muted[1], P->muted[2],
			ix, py + 18, "SETTINGS");
		vile_draw_text_at(cr, "Inter 26", P->close_x[0], P->close_x[1], P->close_x[2],
			px + pw - 44, py + 10, "×");
		server->lock_close_rect = (struct wlr_box){
			.x = (int)(px + pw - 48), .y = (int)(py + 8),
			.width = 36, .height = 36 };
		vile_draw_text_at(cr, "Inter 20", P->panel_val[0], P->panel_val[1], P->panel_val[2],
			ix, py + 58, "Hide password");
		vile_draw_toggle(cr, px + pw - 24 - 64, py + 56, 64, 30,
			!server->lock_show_password);
		server->lock_toggle_rect = (struct wlr_box){
			.x = (int)(px + pw - 24 - 64), .y = (int)(py + 56),
			.width = 64, .height = 30 };
		vile_draw_text_at(cr, "Inter 18", P->muted[0], P->muted[1], P->muted[2],
			ix, py + 106, "Reveal delay");
		vile_draw_text_at(cr, "Inter 18", P->muted[0], P->muted[1], P->muted[2],
			px + pw - 24 - vile_text_width(cr, "Inter 18", "300 ms"),
			py + 106, "300 ms");
		vile_draw_text_at(cr, "Inter Bold 13", P->muted[0], P->muted[1], P->muted[2],
			ix, py + 150, "SYSTEM");
		vile_draw_text_at(cr, "Inter 20", P->panel_val[0], P->panel_val[1], P->panel_val[2],
			ix, py + 182, "Reboot");
		server->lock_reboot_rect = (struct wlr_box){
			.x = (int)px, .y = (int)(py + 176),
			.width = (int)pw, .height = 36 };
		vile_draw_text_at(cr, "Inter 20", P->panel_val[0], P->panel_val[1], P->panel_val[2],
			ix, py + 222, "Power Off");
		server->lock_poweroff_rect = (struct wlr_box){
			.x = (int)px, .y = (int)(py + 216),
			.width = (int)pw, .height = 36 };
	}

	/* --- Minimal bottom-centre status --- */
	vile_draw_text_centre(cr, "Inter 16", P->status[0], P->status[1], P->status[2],
		W / 2.0, H - 32, "EN  •  Vile OS");

	cairo_destroy(cr);

	struct wlr_buffer *buf = vile_paint_cairo(server, surf, W, H);
	cairo_surface_destroy(surf);
	if (!buf) {
		return;
	}
	struct wlr_buffer *old = server->lock_ui_buffer;
	server->lock_ui_buffer = buf;
	wlr_scene_buffer_set_buffer(server->lock_ui_node, buf);
	wlr_scene_buffer_set_dest_size(server->lock_ui_node, W, H);
	if (old) {
		wlr_buffer_drop(old);
	}
}

static void vile_lock_fit(struct vile_server *server, int out_w, int out_h) {
	server->lock_out_w = out_w;
	server->lock_out_h = out_h;
	if (server->lock_bg_node) {
		wlr_scene_buffer_set_dest_size(server->lock_bg_node,
			out_w, out_h);
	}
	if (server->lock_dim) {
		wlr_scene_rect_set_size(server->lock_dim, out_w, out_h);
	}
	if (server->lock_ui_node) {
		wlr_scene_node_set_position(&server->lock_ui_node->node, 0, 0);
		wlr_scene_buffer_set_dest_size(server->lock_ui_node,
			out_w, out_h);
		/* The UI is laid out for this output size — re-render it. */
		vile_lock_render(server);
	}
}

static void vile_lock_raise(struct vile_server *server) {
	if (server->lock_tree) {
		wlr_scene_node_raise_to_top(&server->lock_tree->node);
	}
}

static int vile_clock_timer(void *data) {
	struct vile_server *server = data;
	if (!server->locked) {
		return 0;
	}
	vile_lock_render(server);
	vile_schedule_frames(server);
	wl_event_source_timer_update(server->clock_timer, 1000);
	return 0;
}

/* Lock background for the current theme: the pre-blurred wallpaper
 * (dark) or its lightened twin (light), falling back to the normal
 * wallpaper buffer. Shared by vile_lock_create and the live theme
 * switch (vile_lock_reload_bg). */
static struct wlr_buffer *vile_paint_lock_bg(struct vile_server *server,
		int *out_w, int *out_h) {
	const char *name = server->theme_light ?
		"vile-purple-arch-blur-light.jpg" : "vile-purple-arch-blur.jpg";
	char p1[1024], p2[1024];
	snprintf(p1, sizeof(p1), "../wallpapers/%s", name);
	snprintf(p2, sizeof(p2), "wallpapers/%s", name);
	struct wlr_buffer *buf = vile_paint_file_to_buffer(server,
		p1, out_w, out_h);
	if (!buf) {
		buf = vile_paint_file_to_buffer(server, p2, out_w, out_h);
	}
	return buf;
}

/* Live lock-background reload (theme switch while locked): swap the
 * buffer on the existing node, keeping its size and position. */
static void vile_lock_reload_bg(struct vile_server *server) {
	if (!server->lock_bg_node) {
		return;
	}
	int w = 0, h = 0;
	struct wlr_buffer *buf = vile_paint_lock_bg(server, &w, &h);
	if (!buf) {
		return;
	}
	struct wlr_buffer *old = server->lock_bg_buffer;
	server->lock_bg_buffer = buf;
	wlr_scene_buffer_set_buffer(server->lock_bg_node, buf);
	wlr_scene_buffer_set_dest_size(server->lock_bg_node,
		server->lock_out_w > 0 ? server->lock_out_w : VILE_OUT_W,
		server->lock_out_h > 0 ? server->lock_out_h : VILE_OUT_H);
	if (old) {
		wlr_buffer_drop(old);
	}
}

static void vile_lock_create(struct vile_server *server) {
	server->lock_tree = wlr_scene_tree_create(&server->scene->tree);
	if (!server->lock_tree) {
		wlr_log(WLR_ERROR, "Vile: lock tree create failed");
		return;
	}
	/* Lock v2: windows are hidden ENTIRELY while locked. No dimmed
	 * terminal, no title bar, nothing can bleed through — the only
	 * thing behind the lock UI is the blurred wallpaper below. */
	vile_lock_set_windows_visible(server, false);

	/* Blurred wallpaper as the lock background (pre-blurred at
	 * build time — a real-time blur every frame would be wasteful).
	 * Theme-aware: light mode uses the lightened twin. Falls back
	 * to the normal wallpaper buffer if the blur file is missing. */
	int bg_w = 0, bg_h = 0;
	server->lock_bg_buffer = vile_paint_lock_bg(server, &bg_w, &bg_h);
	if (server->lock_bg_buffer) {
		server->lock_bg_node = wlr_scene_buffer_create(
			server->lock_tree, server->lock_bg_buffer);
	}
	if (!server->lock_bg_node && server->wallpaper_buffer) {
		server->lock_bg_node = wlr_scene_buffer_create(
			server->lock_tree, server->wallpaper_buffer);
	}
	/* A light extra dim so the white clock reads clearly on the
	 * already-darkened blur. In light mode the dim is white, so
	 * the dark clock reads on the light blur. */
	const struct vile_palette *pal = vile_pal(server->theme_light);
	const float dim[4] = { pal->dim_rgb[0], pal->dim_rgb[1],
		pal->dim_rgb[2], 0.30f };
	server->lock_dim = wlr_scene_rect_create(server->lock_tree,
		VILE_OUT_W, VILE_OUT_H, dim);
	server->lock_ui_node = wlr_scene_buffer_create(server->lock_tree, NULL);
	if (!server->lock_ui_node) {
		wlr_log(WLR_ERROR, "Vile: lock UI node create failed");
		return;
	}
	server->lock_password_len = 0;
	server->lock_password[0] = '\0';
	server->lock_failed = false;
	server->lock_show_password = false;
	server->lock_settings_open = false;
	server->lock_out_w = VILE_OUT_W;
	server->lock_out_h = VILE_OUT_H;
	vile_lock_render(server);
	vile_lock_fit(server, VILE_OUT_W, VILE_OUT_H);
	vile_lock_raise(server);
	struct wl_event_loop *loop =
		wl_display_get_event_loop(server->display);
	server->clock_timer = wl_event_loop_add_timer(loop,
		vile_clock_timer, server);
	wl_event_source_timer_update(server->clock_timer, 1000);
	/* Piece 7: fade in with the clock drifting up. */
	vile_anim_lock_in(server);
	wlr_log(WLR_INFO, "Vile: lock screen on (user '%s', windows hidden)",
		server->lock_user);
}

/* Unlock: tear the overlay down. Called ONLY after PAM says yes. */
static void vile_lock_destroy(struct vile_server *server) {
	if (server->clock_timer) {
		wl_event_source_remove(server->clock_timer);
		server->clock_timer = NULL;
	}
	if (server->lock_tree) {
		wlr_scene_node_destroy(&server->lock_tree->node);
		server->lock_tree = NULL;
		server->lock_dim = NULL;
		server->lock_ui_node = NULL;
		server->lock_bg_node = NULL;
	}
	if (server->lock_bg_buffer) {
		wlr_buffer_drop(server->lock_bg_buffer);
		server->lock_bg_buffer = NULL;
	}
	if (server->lock_ui_buffer) {
		wlr_buffer_drop(server->lock_ui_buffer);
		server->lock_ui_buffer = NULL;
	}
	server->locked = false;
	server->lock_password_len = 0;
	memset(server->lock_password, 0, sizeof(server->lock_password));
	/* Windows come back exactly as they were. */
	vile_lock_set_windows_visible(server, true);
	/* Give keyboard focus back to the most recent window. */
	if (!wl_list_empty(&server->toplevels)) {
		struct vile_toplevel *top = wl_container_of(
			server->toplevels.next, top, link);
		/* focused_toplevel is stale (it was the pre-lock window,
		 * which is the same one) — clear it so the helper runs. */
		server->focused_toplevel = NULL;
		vile_focus_toplevel(server, top);
	}
	/* Piece 4: unlocking re-tiles — windows mapped while locked
	 * never got a tile, and the fullscreen state may have changed. */
	vile_tile(server);
	vile_schedule_frames(server);
}

/* Lock NOW (Super+L): put the overlay up over whatever is on screen.
 * Reuses the same lock screen as startup locking — no second lock. */
static void vile_lock_now(struct vile_server *server) {
	if (server->locked) {
		return;
	}
	/* Piece 5: never leave the system menu open behind the lock. */
	vile_shell_close_menu(server);
	server->locked = true;
	server->focused_toplevel = NULL;
	wlr_seat_keyboard_notify_clear_focus(server->seat);
	wlr_seat_pointer_notify_clear_focus(server->seat);
	vile_lock_create(server);
	vile_schedule_frames(server);
	wlr_log(WLR_INFO, "Vile: lock screen on (Super+L)");
}

/* A click on the lock screen. Only the gear and the settings panel
 * respond — everything else is ignored (clicks never reach windows).
 * (lx, ly) are layout coords; the lock UI is full-screen at (0,0),
 * so they are also UI coords on a single output. */
static void vile_lock_click(struct vile_server *server, double lx, double ly) {
	int x = (int)lx, y = (int)ly;
	struct wlr_box *b;
	b = &server->lock_gear_rect;
	if (x >= b->x && x < b->x + b->width && y >= b->y && y < b->y + b->height) {
		server->lock_settings_open = !server->lock_settings_open;
		wlr_log(WLR_INFO, "Vile: lock settings panel %s",
			server->lock_settings_open ? "opened" : "closed");
		vile_lock_render(server);
		vile_schedule_frames(server);
		return;
	}
	if (!server->lock_settings_open) {
		return;
	}
	b = &server->lock_close_rect;
	if (x >= b->x && x < b->x + b->width && y >= b->y && y < b->y + b->height) {
		server->lock_settings_open = false;
		vile_lock_render(server);
		vile_schedule_frames(server);
		return;
	}
	b = &server->lock_toggle_rect;
	if (x >= b->x && x < b->x + b->width && y >= b->y && y < b->y + b->height) {
		server->lock_show_password = !server->lock_show_password;
		wlr_log(WLR_INFO, "Vile: lock show-password %s",
			server->lock_show_password ? "on" : "off");
		vile_lock_render(server);
		vile_schedule_frames(server);
		return;
	}
	b = &server->lock_reboot_rect;
	if (x >= b->x && x < b->x + b->width && y >= b->y && y < b->y + b->height) {
		wlr_log(WLR_INFO, "Vile: lock Reboot clicked");
		vile_spawn("systemctl reboot");
		return;
	}
	b = &server->lock_poweroff_rect;
	if (x >= b->x && x < b->x + b->width && y >= b->y && y < b->y + b->height) {
		wlr_log(WLR_INFO, "Vile: lock Power Off clicked");
		vile_spawn("systemctl poweroff");
		return;
	}
}

/* --- PAM: the real unlock check --- */

static int vile_pam_conv(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr) {
	struct vile_server *server = appdata_ptr;
	struct pam_response *r = calloc(num_msg, sizeof(*r));
	if (!r) {
		return PAM_BUF_ERR;
	}
	for (int i = 0; i < num_msg; i++) {
		if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
			r[i].resp = strdup(server->lock_password);
			r[i].resp_retcode = 0;
		}
	}
	*resp = r;
	return PAM_SUCCESS;
}

static void vile_lock_attempt(struct vile_server *server) {
	struct pam_conv conv = { vile_pam_conv, server };
	pam_handle_t *pamh = NULL;
	int ret = pam_start("vile", server->lock_user, &conv, &pamh);
	if (ret == PAM_SUCCESS) {
		ret = pam_authenticate(pamh, 0);
	}
	if (ret == PAM_SUCCESS) {
		ret = pam_acct_mgmt(pamh, 0);
	}
	bool ok = (ret == PAM_SUCCESS);
	if (pamh) {
		pam_end(pamh, ret);
	}
	/* Never leave the typed password in memory. */
	memset(server->lock_password, 0, sizeof(server->lock_password));
	server->lock_password_len = 0;
	if (ok) {
		wlr_log(WLR_INFO, "Vile: unlock OK for '%s' (PAM)",
			server->lock_user);
		/* Piece 7: fade out first — the animation tick destroys
		 * the lock overlay when the fade finishes. With animations
		 * off, destroy immediately, as before. */
		if (vile_anim_enabled()) {
			vile_anim_lock_out(server);
		} else {
			vile_lock_destroy(server);
		}
	} else {
		server->lock_failed = true;
		wlr_log(WLR_INFO, "Vile: unlock FAILED for '%s' (PAM: %d)",
			server->lock_user, ret);
		vile_lock_render(server);
	}
	vile_schedule_frames(server);
}

/* ------------------------------------------------------------------ */
/* Piece 5 — Vile look: bottom bar, dock                             */
/* ------------------------------------------------------------------ */

/* Design (from design-research.md: GNOME 3-zone bar + macOS floating
 * pill dock + hyprlock-style huge clock):
 *   - Bottom bar (Nov 10, 2026 — Adnan moved it from the top;
 *     Bar Redesign the same day): 40px frosted glass — a real
 *     blur of the wallpaper strip behind it + dark tint, top
 *     hairline — three zones. Left: rounded purple V tile with
 *     glow + "Vile" in bold. (Workspace pills removed Nov 10,
 *     2026 — Adnan didn't know what they were.) Centre: time in
 *     bold, date beside it in a lighter secondary colour. Right: ONE status
 *     pill (wifi/volume/battery/power + battery % when a battery
 *     exists) that opens ONE system menu, which rises UP from
 *     the bar. Never a solid white bar.
 *   - NO desktop clock (Nov 10, 2026 — the huge 140px clock is
 *     gone; the bar centre carries the small time + date instead).
 *     The LOCK clock is a separate component and stays huge.
 *   - Dock: above the bottom bar, floating pill with a 12px gap
 *     from the bar, translucent dark, rounded, 56px icons, one
 *     shape — foot uses foot-logo-dark.png, Settings uses the
 *     purple V, ScreenFit uses a dark tile with a purple monitor
 *     glyph. A small purple dot marks running apps. Click a
 *     running app to focus it; click foot's icon to open a terminal.
 * Scene order: wallpaper -> windows -> shell tree (bar + dock +
 * menu) -> lock tree. The shell tree is re-raised after every
 * focus/map/tile, because focusing raises a window above it.
 * Fullscreen hides the shell tree (a fullscreen window covers
 * the bar and dock, like every other OS). */

#define VILE_DOCK_ICON 56
#define VILE_DOCK_PAD 12
#define VILE_DOCK_GAP 16
#define VILE_MENU_W 300
#define VILE_MENU_H 352

static bool vile_box_contains(struct wlr_box *b, int x, int y) {
	return x >= b->x && x < b->x + b->width &&
		y >= b->y && y < b->y + b->height;
}

/* Real system status for the menu (read from /sys, never invented):
 * wifi: first interface starting with "wl" — operstate up =
 * Connected, down = Off; none at all = Unavailable. Battery: BAT0
 * capacity + status; missing = No battery. */
static void vile_shell_wifi_status(char *buf, size_t len) {
	DIR *d = opendir("/sys/class/net");
	if (!d) {
		snprintf(buf, len, "Unavailable");
		return;
	}
	struct dirent *e;
	bool found = false, up = false;
	while ((e = readdir(d)) != NULL) {
		if (strncmp(e->d_name, "wl", 2) != 0 &&
				strncmp(e->d_name, "wlan", 4) != 0) {
			continue;
		}
		found = true;
		char path[512];
		snprintf(path, sizeof(path), "/sys/class/net/%s/operstate",
			e->d_name);
		FILE *f = fopen(path, "r");
		if (f) {
			char st[32] = "";
			if (fgets(st, sizeof(st), f) &&
					strncmp(st, "up", 2) == 0) {
				up = true;
			}
			fclose(f);
		}
	}
	closedir(d);
	if (!found) {
		snprintf(buf, len, "Unavailable");
	} else if (up) {
		snprintf(buf, len, "Connected");
	} else {
		snprintf(buf, len, "Off");
	}
}

static int vile_shell_battery_capacity(void) {
	FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
	if (!f) {
		return -1;
	}
	int cap = -1;
	if (fscanf(f, "%d", &cap) != 1) {
		cap = -1;
	}
	fclose(f);
	return cap;
}

static void vile_shell_battery_status(char *buf, size_t len) {
	int cap = vile_shell_battery_capacity();
	if (cap < 0) {
		snprintf(buf, len, "No battery");
		return;
	}
	FILE *f = fopen("/sys/class/power_supply/BAT0/status", "r");
	char st[32] = "";
	if (f) {
		if (!fgets(st, sizeof(st), f)) {
			st[0] = '\0';
		}
		fclose(f);
	}
	size_t n = strlen(st);
	while (n > 0 && (st[n - 1] == '\n' || st[n - 1] == '\r')) {
		st[--n] = '\0';
	}
	if (strcmp(st, "Charging") == 0) {
		snprintf(buf, len, "%d%% · Charging", cap);
	} else if (strcmp(st, "Full") == 0) {
		snprintf(buf, len, "%d%% · Full", cap);
	} else {
		snprintf(buf, len, "%d%%", cap);
	}
}

/* Small status glyphs, drawn with Cairo (no icon font needed). */
static void vile_draw_wifi_glyph(cairo_t *cr, double cx, double cy,
		double r, double red, double green, double blue) {
	cairo_set_source_rgb(cr, red, green, blue);
	cairo_set_line_width(cr, 2.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	for (int i = 0; i < 3; i++) {
		double rr = r * (0.42 + i * 0.30);
		cairo_new_path(cr);
		cairo_arc(cr, cx, cy + r * 0.5, rr, -M_PI * 0.72, -M_PI * 0.28);
		cairo_stroke(cr);
	}
	cairo_new_path(cr);
	cairo_arc(cr, cx, cy + r * 0.5, 2.0, 0, 2 * M_PI);
	cairo_fill(cr);
}

static void vile_draw_volume_glyph(cairo_t *cr, double cx, double cy,
		double r, double red, double green, double blue) {
	cairo_set_source_rgb(cr, red, green, blue);
	cairo_set_line_width(cr, 2.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_new_path(cr);
	cairo_move_to(cr, cx - r * 0.9, cy - r * 0.25);
	cairo_line_to(cr, cx - r * 0.35, cy - r * 0.25);
	cairo_line_to(cr, cx + r * 0.25, cy - r * 0.75);
	cairo_line_to(cr, cx + r * 0.25, cy + r * 0.75);
	cairo_line_to(cr, cx - r * 0.35, cy + r * 0.25);
	cairo_line_to(cr, cx - r * 0.9, cy + r * 0.25);
	cairo_close_path(cr);
	cairo_fill(cr);
	for (int i = 0; i < 2; i++) {
		double rr = r * (0.45 + i * 0.30);
		cairo_new_path(cr);
		cairo_arc(cr, cx + r * 0.15, cy, rr, -M_PI * 0.32, M_PI * 0.32);
		cairo_stroke(cr);
	}
}

static void vile_draw_battery_glyph(cairo_t *cr, double x, double y,
		double w, double h, int pct, double red, double green,
		double blue) {
	cairo_set_source_rgb(cr, red, green, blue);
	cairo_set_line_width(cr, 1.5);
	cairo_new_path(cr);
	vile_rounded_rect(cr, x, y, w, h, 3);
	cairo_stroke(cr);
	cairo_new_path(cr);
	cairo_rectangle(cr, x + w + 1, y + h * 0.3, 2.5, h * 0.4);
	cairo_fill(cr);
	if (pct >= 0) {
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
		double fw = (w - 4) * pct / 100.0;
		if (fw > 0) {
			cairo_new_path(cr);
			vile_rounded_rect(cr, x + 2, y + 2, fw, h - 4, 2);
			cairo_fill(cr);
		}
	}
}

static void vile_draw_power_glyph(cairo_t *cr, double cx, double cy,
		double r, double red, double green, double blue) {
	cairo_set_source_rgb(cr, red, green, blue);
	cairo_set_line_width(cr, 2.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_new_path(cr);
	cairo_arc(cr, cx, cy, r, M_PI * 0.28, M_PI * 1.72);
	cairo_stroke(cr);
	cairo_new_path(cr);
	cairo_move_to(cr, cx, cy - r - 2);
	cairo_line_to(cr, cx, cy - r * 0.15);
	cairo_stroke(cr);
}

/* Draw text right-aligned so it ends at x = `right`. */
static void vile_shell_draw_text_right(cairo_t *cr, const char *font,
		double r, double g, double b, double right, double y,
		const char *text) {
	int w = vile_text_width(cr, font, text);
	vile_draw_text_at(cr, font, r, g, b, right - w, y, text);
}

/* Paint a Cairo surface into a shell scene-buffer node, replacing
 * the previous buffer (same pattern as the lock UI). */
static void vile_shell_set_buffer(struct vile_server *server,
		struct wlr_scene_buffer *node, struct wlr_buffer **slot,
		cairo_surface_t *surf, int w, int h) {
	if (!node) {
		return;
	}
	struct wlr_buffer *buf = vile_paint_cairo(server, surf, w, h);
	if (!buf) {
		return;
	}
	struct wlr_buffer *old = *slot;
	*slot = buf;
	wlr_scene_buffer_set_buffer(node, buf);
	wlr_scene_buffer_set_dest_size(node, w, h);
	if (old) {
		wlr_buffer_drop(old);
	}
}

/* Frosted-glass bar (Bar Redesign, Nov 10, 2026): the bar samples
 * the wallpaper strip directly behind it and blurs it — downscale
 * to 1/16 width + 3 rows, upscale bilinear — a real blur of the
 * live wallpaper, not a fake gradient. Behind the bar is always
 * wallpaper (windows tile above the dock), so the frost is always
 * correct. The result is cached; render_bar runs every minute,
 * the decode does not. */
static cairo_surface_t *vile_shell_build_bar_frost(
		struct vile_server *server, int out_w, int out_h) {
	(void)server;
	const char *path = vile_wallpaper_path();
	if (!path) {
		return NULL;
	}
	int ww = 0, wh = 0, ch = 0;
	unsigned char *px = stbi_load(path, &ww, &wh, &ch, 4);
	if (!px) {
		return NULL;
	}
	/* Cover-fit geometry — the same math as vile_fit_wallpaper, so
	 * the strip matches the wallpaper pixels actually on screen
	 * behind the bar. */
	double scale = fmax((double)out_w / ww, (double)out_h / wh);
	double dw = ww * scale, dh = wh * scale;
	double ox = (out_w - dw) / 2.0, oy = (out_h - dh) / 2.0;
	double sx0 = (0.0 - ox) / scale;
	double sy0 = ((double)out_h - VILE_BAR_H - oy) / scale;
	double sw = (double)out_w / scale;
	double sh = (double)VILE_BAR_H / scale;
	int ix0 = (int)floor(sx0), iy0 = (int)floor(sy0);
	int ix1 = (int)ceil(sx0 + sw), iy1 = (int)ceil(sy0 + sh);
	if (ix0 < 0) ix0 = 0;
	if (iy0 < 0) iy0 = 0;
	if (ix1 > ww) ix1 = ww;
	if (iy1 > wh) iy1 = wh;
	int cw = ix1 - ix0, chh = iy1 - iy0;
	if (cw <= 0 || chh <= 0) {
		stbi_image_free(px);
		return NULL;
	}
	/* Swizzle the crop RGBA -> cairo ARGB32 (BGRA in memory on
	 * little-endian). Only the strip is swizzled, not the whole
	 * image. */
	cairo_surface_t *crop = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, cw, chh);
	if (cairo_surface_status(crop) != CAIRO_STATUS_SUCCESS) {
		stbi_image_free(px);
		return NULL;
	}
	unsigned char *dst = cairo_image_surface_get_data(crop);
	int dstride = cairo_image_surface_get_stride(crop);
	for (int y = 0; y < chh; y++) {
		const unsigned char *src =
			px + ((size_t)(iy0 + y) * ww + ix0) * 4;
		unsigned char *row = dst + (size_t)y * dstride;
		for (int x = 0; x < cw; x++) {
			row[x * 4 + 0] = src[x * 4 + 2]; /* B */
			row[x * 4 + 1] = src[x * 4 + 1]; /* G */
			row[x * 4 + 2] = src[x * 4 + 0]; /* R */
			row[x * 4 + 3] = src[x * 4 + 3]; /* A */
		}
	}
	cairo_surface_mark_dirty(crop);
	stbi_image_free(px);
	/* Blur: downscale hard, then upscale bilinear. */
	int tw = out_w / 16;
	if (tw < 1) tw = 1;
	int th = 3;
	cairo_surface_t *tiny = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, tw, th);
	cairo_t *cr = cairo_create(tiny);
	cairo_scale(cr, (double)tw / cw, (double)th / chh);
	cairo_set_source_surface(cr, crop, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr),
		CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(crop);
	cairo_surface_t *frost = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, out_w, VILE_BAR_H);
	cr = cairo_create(frost);
	cairo_scale(cr, (double)out_w / tw, (double)VILE_BAR_H / th);
	cairo_set_source_surface(cr, tiny, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr),
		CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(tiny);
	return frost;
}

/* Dock frost (Frosted dock, Nov 10, 2026 ~19:52 — macOS-style):
 * the same blur technique as the bar frost, but for the
 * wallpaper rectangle behind the dock pill (output coords
 * x, y, w, h) instead of the full-width bar strip. Returns a
 * w x h blurred surface, or NULL (caller falls back to a plain
 * translucent tint — the dock still reads as a pill). The
 * function is separate from the bar builder on purpose: the
 * bar path is proven and must not change. */
static cairo_surface_t *vile_shell_build_dock_frost(
		struct vile_server *server, int x, int y, int w, int h) {
	(void)server;
	const char *path = vile_wallpaper_path();
	if (!path || w <= 0 || h <= 0) {
		return NULL;
	}
	int ww = 0, wh = 0, ch = 0;
	unsigned char *px = stbi_load(path, &ww, &wh, &ch, 4);
	if (!px) {
		return NULL;
	}
	/* Cover-fit geometry — the same math as vile_fit_wallpaper, so
	 * the crop matches the wallpaper pixels actually on screen
	 * behind the dock. */
	int out_w = server->shell_w, out_h = server->shell_h;
	double scale = fmax((double)out_w / ww, (double)out_h / wh);
	double dw = ww * scale, dh = wh * scale;
	double ox = (out_w - dw) / 2.0, oy = (out_h - dh) / 2.0;
	double sx0 = ((double)x - ox) / scale;
	double sy0 = ((double)y - oy) / scale;
	double sw = (double)w / scale;
	double sh = (double)h / scale;
	int ix0 = (int)floor(sx0), iy0 = (int)floor(sy0);
	int ix1 = (int)ceil(sx0 + sw), iy1 = (int)ceil(sy0 + sh);
	if (ix0 < 0) ix0 = 0;
	if (iy0 < 0) iy0 = 0;
	if (ix1 > ww) ix1 = ww;
	if (iy1 > wh) iy1 = wh;
	int cw = ix1 - ix0, chh = iy1 - iy0;
	if (cw <= 0 || chh <= 0) {
		stbi_image_free(px);
		return NULL;
	}
	/* Swizzle the crop RGBA -> cairo ARGB32 (BGRA in memory on
	 * little-endian). Only the rect is swizzled, not the whole
	 * image. */
	cairo_surface_t *crop = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, cw, chh);
	if (cairo_surface_status(crop) != CAIRO_STATUS_SUCCESS) {
		stbi_image_free(px);
		return NULL;
	}
	unsigned char *dst = cairo_image_surface_get_data(crop);
	int dstride = cairo_image_surface_get_stride(crop);
	for (int yy = 0; yy < chh; yy++) {
		const unsigned char *src =
			px + ((size_t)(iy0 + yy) * ww + ix0) * 4;
		unsigned char *row = dst + (size_t)yy * dstride;
		for (int xx = 0; xx < cw; xx++) {
			row[xx * 4 + 0] = src[xx * 4 + 2]; /* B */
			row[xx * 4 + 1] = src[xx * 4 + 1]; /* G */
			row[xx * 4 + 2] = src[xx * 4 + 0]; /* R */
			row[xx * 4 + 3] = src[xx * 4 + 3]; /* A */
		}
	}
	cairo_surface_mark_dirty(crop);
	stbi_image_free(px);
	/* Blur: downscale hard, then upscale bilinear. */
	int tw = w / 16;
	if (tw < 1) tw = 1;
	int th = h / 16;
	if (th < 1) th = 1;
	cairo_surface_t *tiny = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, tw, th);
	cairo_t *cr = cairo_create(tiny);
	cairo_scale(cr, (double)tw / cw, (double)th / chh);
	cairo_set_source_surface(cr, crop, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr),
		CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(crop);
	cairo_surface_t *frost = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, w, h);
	cr = cairo_create(frost);
	cairo_scale(cr, (double)w / tw, (double)h / th);
	cairo_set_source_surface(cr, tiny, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr),
		CAIRO_FILTER_BILINEAR);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(tiny);
	return frost;
}

/* Dock (Nov 10, 2026 ~16:31 — frosted (v3, Nov 10 ~19:52), ordered, reorderable,
 * unpinnable, with a Setup lifecycle icon). The dock is an ordered
 * list of app ids (dock_order[0..dock_n-1]), NOT four fixed slots.
 * The pinned set persists in ~/.config/vile/dock-pinned (one app id
 * per line, in order); when the file is missing the default is
 * foot, settings, screenfit, files. The Setup icon is appended at
 * the END while first-run setup is incomplete (no setup-done
 * marker) and not unpinned (no dock-setup-hidden marker); it is
 * NOT stored in dock-pinned, so its position never persists. */

/* Path helper: ~/.config/vile/<name>. */
static bool vile_dock_config_path(char *buf, size_t len, const char *name) {
	const char *home = getenv("HOME");
	if (!home) {
		return false;
	}
	snprintf(buf, len, "%s/.config/vile/%s", home, name);
	return true;
}

static int vile_dock_app_from_name(const char *name) {
	for (int i = 0; i < DOCK_APP_COUNT; i++) {
		if (strcmp(name, vile_dock_app_names[i]) == 0) {
			return i;
		}
	}
	return -1;
}

/* First-run setup is incomplete while ~/.config/vile/setup-done
 * does not exist. This does NOT check VILE_NO_SETUP (that env var
 * only stops the wizard auto-opening; the dock icon is about the
 * marker, so it still shows in test harnesses that set it). */
static bool vile_dock_setup_incomplete(void) {
	char path[1024];
	if (!vile_dock_config_path(path, sizeof(path), "setup-done")) {
		return false;
	}
	struct stat st;
	return stat(path, &st) != 0;
}

/* The user unpinned the Setup icon (right-click -> Unpin). */
static bool vile_dock_setup_hidden(void) {
	char path[1024];
	if (!vile_dock_config_path(path, sizeof(path), "dock-setup-hidden")) {
		return false;
	}
	struct stat st;
	return stat(path, &st) == 0;
}

/* Slot of an app in the current order, or -1. */
static int vile_dock_slot_of_app(struct vile_server *server, int app) {
	for (int i = 0; i < server->dock_n; i++) {
		if (server->dock_order[i] == app) {
			return i;
		}
	}
	return -1;
}

/* Drop slot for a reorder drag: the slot whose centre is nearest
 * the pointer x (clamped to the dock). The dragged app moves to
 * this slot; the others shift. */
static int vile_dock_drop_slot(struct vile_server *server, double x) {
	if (server->dock_n <= 0) {
		return 0;
	}
	double rel = x - server->shell_dock_x - VILE_DOCK_PAD
		- VILE_DOCK_ICON / 2.0;
	int slot = (int)(rel / (VILE_DOCK_ICON + VILE_DOCK_GAP) + 0.5);
	if (slot < 0) {
		slot = 0;
	}
	if (slot > server->dock_n - 1) {
		slot = server->dock_n - 1;
	}
	return slot;
}

/* Slot under an output point, or -1 (between icons / outside). */
static int vile_dock_slot_at(struct vile_server *server, int x, int y) {
	for (int i = 0; i < server->dock_n; i++) {
		if (vile_box_contains(&server->shell_dock_rects[i], x, y)) {
			return i;
		}
	}
	return -1;
}

/* Running-app count for a dock app (the shell_dock_*_n fields,
 * kept by the 1s timer). Setup is not an app window: 0. */
static int vile_dock_running_count(struct vile_server *server, int app) {
	switch (app) {
	case DOCK_FOOT:
		return server->shell_dock_foot_n;
	case DOCK_SETTINGS:
		return server->shell_dock_set_n;
	case DOCK_SCREENFIT:
		return server->shell_dock_screenfit_n;
	case DOCK_FILES:
		return server->shell_dock_files_n;
	default:
		return 0;
	}
}

/* Centres log line (the harnesses parse it: 'dock foot icon
 * centre (X,Y)', 'settings centre', 'screenfit centre', 'files
 * centre' — keep those tokens; a setup centre is appended when
 * the Setup icon is shown). An app not on the dock logs (0,0).
 * Logged at shell create and on every dock order change, so a
 * harness can re-parse the latest line. */
static void vile_dock_log_centres(struct vile_server *server) {
	int cx[DOCK_APP_COUNT], cy[DOCK_APP_COUNT];
	for (int a = 0; a < DOCK_APP_COUNT; a++) {
		cx[a] = 0;
		cy[a] = 0;
	}
	for (int i = 0; i < server->dock_n; i++) {
		int app = server->dock_order[i];
		cx[app] = server->shell_dock_rects[i].x
			+ server->shell_dock_rects[i].width / 2;
		cy[app] = server->shell_dock_rects[i].y
			+ server->shell_dock_rects[i].height / 2;
	}
	/* Entry count first (the dock harness checks "dock: N entries"
	 * to prove the Setup icon appeared / disappeared). */
	wlr_log(WLR_INFO, "Vile: dock: %d entries", server->dock_n);
	if (server->dock_setup_shown) {
		wlr_log(WLR_INFO, "Vile: shell: dock foot icon centre "
			"(%d,%d), settings centre (%d,%d), screenfit centre "
			"(%d,%d), files centre (%d,%d), setup centre (%d,%d)",
			cx[DOCK_FOOT], cy[DOCK_FOOT],
			cx[DOCK_SETTINGS], cy[DOCK_SETTINGS],
			cx[DOCK_SCREENFIT], cy[DOCK_SCREENFIT],
			cx[DOCK_FILES], cy[DOCK_FILES],
			cx[DOCK_SETUP], cy[DOCK_SETUP]);
	} else {
		wlr_log(WLR_INFO, "Vile: shell: dock foot icon centre "
			"(%d,%d), settings centre (%d,%d), screenfit centre "
			"(%d,%d), files centre (%d,%d)",
			cx[DOCK_FOOT], cy[DOCK_FOOT],
			cx[DOCK_SETTINGS], cy[DOCK_SETTINGS],
			cx[DOCK_SCREENFIT], cy[DOCK_SCREENFIT],
			cx[DOCK_FILES], cy[DOCK_FILES]);
	}
}

/* Load the pinned set from ~/.config/vile/dock-pinned (one app id
 * per line, in order). Missing file -> the default order. A file
 * that exists but is empty -> an empty dock (the user unpinned
 * everything); a file whose lines are all invalid -> defaults
 * (corrupt). The Setup icon is appended at the end when setup is
 * incomplete and not unpinned. */
static void vile_dock_load(struct vile_server *server) {
	int defs[] = { DOCK_FOOT, DOCK_SETTINGS, DOCK_SCREENFIT,
		DOCK_FILES };
	int n = 0;
	int lines = 0;
	bool have_file = false;
	char path[1024];
	if (vile_dock_config_path(path, sizeof(path), "dock-pinned")) {
		FILE *f = fopen(path, "r");
		if (f) {
			have_file = true;
			char line[64];
			while (fgets(line, sizeof(line), f)
					&& n < VILE_DOCK_MAX - 1) {
				lines++;
				size_t len = strlen(line);
				while (len > 0 && (line[len - 1] == '\n'
						|| line[len - 1] == '\r'
						|| line[len - 1] == ' '
						|| line[len - 1] == '\t')) {
					line[--len] = '\0';
				}
				int app = vile_dock_app_from_name(line);
				if (app < 0 || app == DOCK_SETUP) {
					continue; /* setup is never pinned */
				}
				bool dup = false;
				for (int i = 0; i < n; i++) {
					if (server->dock_order[i] == app) {
						dup = true;
					}
				}
				if (!dup) {
					server->dock_order[n++] = app;
				}
			}
			fclose(f);
		}
	}
	if (!have_file || (lines > 0 && n == 0)) {
		/* Missing, or corrupt (lines but nothing valid): defaults. */
		n = 0;
		for (int i = 0; i < 4; i++) {
			server->dock_order[n++] = defs[i];
		}
	}
	server->dock_n = n;
	server->dock_setup_shown = false;
	if (vile_dock_setup_incomplete() && !vile_dock_setup_hidden()) {
		server->dock_order[server->dock_n++] = DOCK_SETUP;
		server->dock_setup_shown = true;
	}
}

/* Persist the pinned set (setup excluded) in displayed order.
 * Creates ~/.config/vile if needed. */
static void vile_dock_save(struct vile_server *server) {
	const char *home = getenv("HOME");
	if (!home) {
		return;
	}
	char dir[1024];
	snprintf(dir, sizeof(dir), "%s/.config", home);
	mkdir(dir, 0755);
	snprintf(dir, sizeof(dir), "%s/.config/vile", home);
	mkdir(dir, 0755);
	char path[1024];
	if (!vile_dock_config_path(path, sizeof(path), "dock-pinned")) {
		return;
	}
	FILE *f = fopen(path, "w");
	if (!f) {
		wlr_log(WLR_INFO, "Vile: dock: could not save pinned set (%s)",
			path);
		return;
	}
	for (int i = 0; i < server->dock_n; i++) {
		int app = server->dock_order[i];
		if (app == DOCK_SETUP) {
			continue;
		}
		fprintf(f, "%s\n", vile_dock_app_names[app]);
	}
	fclose(f);
	wlr_log(WLR_INFO, "Vile: dock: pinned set saved");
}

/* Dock geometry from dock_n: centred, growing symmetrically from
 * the middle as entries are added. Called by vile_shell_fit and
 * after every order change. */
static void vile_dock_layout(struct vile_server *server) {
	int n = server->dock_n;
	int dock_w = VILE_DOCK_PAD * 2 + VILE_DOCK_ICON * n
		+ VILE_DOCK_GAP * (n > 0 ? n - 1 : 0);
	server->shell_dock_x = (server->shell_w - dock_w) / 2;
	server->shell_dock_y = server->shell_h - VILE_BAR_H
		- VILE_TILE_GAP - VILE_DOCK_H;
	server->shell_dock_rect = (struct wlr_box){
		.x = server->shell_dock_x,
		.y = server->shell_dock_y,
		.width = dock_w,
		.height = VILE_DOCK_H };
	for (int i = 0; i < n; i++) {
		server->shell_dock_rects[i] = (struct wlr_box){
			.x = server->shell_dock_x + VILE_DOCK_PAD
				+ i * (VILE_DOCK_ICON + VILE_DOCK_GAP),
			.y = server->shell_dock_y + VILE_DOCK_PAD,
			.width = VILE_DOCK_ICON,
			.height = VILE_DOCK_ICON };
	}
	if (server->shell_dock_node) {
		wlr_scene_node_set_position(&server->shell_dock_node->node,
			server->shell_dock_x - 40, server->shell_dock_y - 20);
	}
}

/* Setup lifecycle: the wizard writes setup-done, and the dock
 * icon must disappear by itself (within a second, via the 1s
 * timer). Returns true if the order changed. The cached state is
 * updated BEFORE re-laying out, so the nested refresh from
 * vile_shell_fit sees no change (recursion bound to one level). */
static bool vile_dock_refresh(struct vile_server *server) {
	bool want_setup = vile_dock_setup_incomplete()
		&& !vile_dock_setup_hidden();
	if (want_setup == server->dock_setup_shown) {
		return false;
	}
	server->dock_setup_shown = want_setup;
	if (want_setup) {
		if (vile_dock_slot_of_app(server, DOCK_SETUP) < 0) {
			server->dock_order[server->dock_n++] = DOCK_SETUP;
		}
	} else {
		int slot = vile_dock_slot_of_app(server, DOCK_SETUP);
		if (slot >= 0) {
			memmove(&server->dock_order[slot],
				&server->dock_order[slot + 1],
				(server->dock_n - slot - 1) * sizeof(int));
			server->dock_n--;
		}
	}
	vile_dock_layout(server);
	vile_shell_render_dock(server);
	vile_dock_log_centres(server);
	wlr_log(WLR_INFO, "Vile: dock: setup icon %s",
		want_setup ? "shown" : "gone (setup complete)");
	return true;
}

static void vile_shell_render_bar(struct vile_server *server) {
	int W = server->shell_w, H = VILE_BAR_H;
	if (W <= 0) {
		return;
	}
	/* Bottom bar (Nov 10, 2026): the bar node sits at
	 * y = shell_h - VILE_BAR_H, so the click rects below are in
	 * output coords, not surface coords. */
	int bar_y = server->shell_h - VILE_BAR_H;
	const struct vile_palette *P = vile_pal(server->theme_light);
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, W, H);
	cairo_t *cr = cairo_create(surf);
	/* Frosted glass: blurred wallpaper strip + dark tint. The
	 * frost is cached (rebuilt only on wallpaper/size change);
	 * the tint keeps the text readable on bright wallpapers. */
	const char *wpath = vile_wallpaper_path();
	struct stat st;
	time_t mtime = 0;
	if (wpath && stat(wpath, &st) == 0) {
		mtime = st.st_mtime;
	}
	if (!server->shell_bar_frost || W != server->shell_bar_frost_w ||
			H != server->shell_bar_frost_h || !wpath ||
			strcmp(wpath, server->shell_bar_frost_path) != 0 ||
			mtime != server->shell_bar_frost_mtime) {
		cairo_surface_t *frost = vile_shell_build_bar_frost(
			server, W, server->shell_h);
		if (frost) {
			if (server->shell_bar_frost) {
				cairo_surface_destroy(server->shell_bar_frost);
			}
			server->shell_bar_frost = frost;
			snprintf(server->shell_bar_frost_path,
				sizeof(server->shell_bar_frost_path),
				"%s", wpath ? wpath : "");
			server->shell_bar_frost_w = W;
			server->shell_bar_frost_h = H;
			server->shell_bar_frost_mtime = mtime;
		}
	}
	if (server->shell_bar_frost) {
		cairo_set_source_surface(cr, server->shell_bar_frost, 0, 0);
		cairo_paint(cr);
		/* Dark tint over the frost — translucent dark, never a
		 * solid white bar. */
		cairo_set_source_rgba(cr, P->bar_rgb[0], P->bar_rgb[1], P->bar_rgb[2], 0.55);
		cairo_rectangle(cr, 0, 0, W, H);
		cairo_fill(cr);
	} else {
		/* No wallpaper to sample (should not happen — the
		 * default wallpaper always exists): flat translucent
		 * dark, like the old bar. */
		cairo_set_source_rgba(cr, P->panel_rgb[0], P->panel_rgb[1], P->panel_rgb[2], 0.72);
		cairo_rectangle(cr, 0, 0, W, H);
		cairo_fill(cr);
	}
	/* Hairline on the TOP edge — the separator between the
	 * desktop and the bar — with a faint purple rim light just
	 * below it. */
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.14);
	cairo_rectangle(cr, 0, 0, W, 1);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.18);
	cairo_rectangle(cr, 0, 1, W, 1);
	cairo_fill(cr);

	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char time_str[16], date_str[32];
	strftime(time_str, sizeof(time_str), "%H:%M", &tm);
	strftime(date_str, sizeof(date_str), "%a %d %b", &tm);
	double cy = H / 2.0;

	/* Left corner: EMPTY (Nov 10, 2026 ~16:31 — Adnan: "we know
	 * what system we're on"). The glowing V tile + "Vile"
	 * wordmark were deleted; the bar is just the centred clock
	 * and the right status pill. The menu trigger is the status
	 * pill (it already was); the old logo-click -> Settings
	 * branch went with the tile — Settings is still one dock
	 * click away, and the menu has its own Settings row. */

	/* Centre: time in a larger semibold weight, date beside it
	 * in a lighter secondary colour, both vertically centred. */
	int tw = vile_text_width(cr, "Inter Bold 17", time_str);
	int dw = vile_text_width(cr, "Inter 13", date_str);
	double cx0 = W / 2.0 - (tw + 12 + dw) / 2.0;
	vile_draw_text_at(cr, "Inter Bold 17", P->text[0], P->text[1], P->text[2],
		cx0, 10, time_str);
	vile_draw_text_at(cr, "Inter 13", P->text3[0], P->text3[1], P->text3[2],
		cx0 + tw + 12, 13, date_str);

	/* Right: ONE status pill (wifi / volume / battery / power)
	 * that opens ONE system menu — not five separate buttons.
	 * Battery percentage as text when a battery exists. */
	int cap = vile_shell_battery_capacity();
	char batt_pct[16] = "";
	if (cap >= 0) {
		snprintf(batt_pct, sizeof(batt_pct), "%d%%", cap);
	}
	int bw = cap >= 0 ?
		vile_text_width(cr, "Inter Bold 13", batt_pct) : 0;
	int pw = 12 + 18 + 10 + 18 + 10 + 22 +
		(cap >= 0 ? 6 + bw : 0) + 10 + 16 + 12;
	int px = W - 14 - pw;
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.08);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, 6.5, pw - 1, 27, 14);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.12);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, 6.5, pw - 1, 27, 14);
	cairo_stroke(cr);
	double gx = px + 12 + 9;
	vile_draw_wifi_glyph(cr, gx, cy, 9, P->glyph[0], P->glyph[1], P->glyph[2]);
	gx += 9 + 10 + 9;
	vile_draw_volume_glyph(cr, gx, cy, 9, P->glyph[0], P->glyph[1], P->glyph[2]);
	gx += 9 + 10 + 11;
	vile_draw_battery_glyph(cr, gx - 11, cy - 6, 22, 12,
		cap, P->glyph[0], P->glyph[1], P->glyph[2]);
	gx += 11;
	if (cap >= 0) {
		gx += 6;
		vile_draw_text_at(cr, "Inter Bold 13", P->text[0], P->text[1], P->text[2],
			gx, 13, batt_pct);
		gx += bw;
	}
	gx += 10 + 8;
	vile_draw_power_glyph(cr, gx, cy, 8, P->glyph[0], P->glyph[1], P->glyph[2]);
	server->shell_status_rect = (struct wlr_box){
		.x = px, .y = bar_y, .width = pw, .height = H };

	cairo_destroy(cr);
	vile_shell_set_buffer(server, server->shell_bar_node,
		&server->shell_bar_buffer, surf, W, H);
	cairo_surface_destroy(surf);
}

/* Desktop clock REMOVED (Nov 10, 2026 — Adnan moved the bar to the
 * bottom and dropped the big desktop clock; the bar centre now
 * carries the small time + date, same as the old top bar. The
 * LOCK clock is a separate component (vile_lock_render) and is
 * untouched — it stays huge. The resolution-aware scale helper
 * (vile_clock_scale) and the alpha text helper (vile_draw_text_centre_a,
 * only the clock's halo used it) went with the desktop clock. */

/* Draw one dock icon (app) into a cairo context, in a cell whose
 * top-left is (x, y) in SURFACE coords, scaled around the cell
 * centre by `scale`. Inside the transform the coords are local
 * (0..56). Bare glyphs only — no tiles (transparent dock,
 * Nov 10, 2026 ~16:31). */
static void vile_dock_draw_icon(struct vile_server *server, cairo_t *cr,
		int app, double x, double y, double scale) {
	cairo_save(cr);
	cairo_translate(cr, x + VILE_DOCK_ICON / 2.0,
		y + VILE_DOCK_ICON / 2.0);
	cairo_scale(cr, scale, scale);
	cairo_translate(cr, -VILE_DOCK_ICON / 2.0, -VILE_DOCK_ICON / 2.0);
	switch (app) {
	case DOCK_FOOT:
		/* Bare footprint + ">_" + "foot" (foot-logo.png,
		 * transparent). Fallback: the old ">_" text. */
		if (server->shell_foot_icon) {
			cairo_scale(cr, (double)VILE_DOCK_ICON / 1024,
				(double)VILE_DOCK_ICON / 1024);
			cairo_set_source_surface(cr, server->shell_foot_icon, 0, 0);
			cairo_paint(cr);
		} else {
			vile_draw_text_centre(cr, "Inter Bold 20",
				0.19, 0.60, 0.94,
				VILE_DOCK_ICON / 2.0, VILE_DOCK_ICON / 2.0, ">_");
		}
		break;
	case DOCK_SETTINGS:
		/* Bare ribbon V (vile-logo.png, transparent) — the ribbon
		 * stays as the Settings icon (bar corner only went).
		 * Fallback: the old purple circle + V. */
		if (server->shell_vile_icon) {
			cairo_scale(cr, (double)VILE_DOCK_ICON / 1024,
				(double)VILE_DOCK_ICON / 1024);
			cairo_set_source_surface(cr, server->shell_vile_icon, 0, 0);
			cairo_paint(cr);
		} else {
			cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
			cairo_new_path(cr);
			cairo_arc(cr, VILE_DOCK_ICON / 2.0,
				VILE_DOCK_ICON / 2.0, 26, 0, 2 * M_PI);
			cairo_fill(cr);
			vile_draw_text_centre(cr, "Inter Bold 30", 1.0, 1.0, 1.0,
				VILE_DOCK_ICON / 2.0,
				VILE_DOCK_ICON / 2.0 + 1, "V");
		}
		break;
	case DOCK_SCREENFIT:
		/* Monitor glyph, no tile (ScreenFit = display / overscan). */
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
		cairo_set_line_width(cr, 3);
		cairo_new_path(cr);
		vile_rounded_rect(cr, 13, 13, 30, 21, 3);
		cairo_stroke(cr);
		cairo_move_to(cr, 28, 34);
		cairo_line_to(cr, 28, 41);
		cairo_stroke(cr);
		cairo_move_to(cr, 20, 41);
		cairo_line_to(cr, 36, 41);
		cairo_stroke(cr);
		break;
	case DOCK_FILES:
		/* Bare folder + page (files-icon-bare.png, transparent).
		 * Fallback: a cairo folder outline, no tile. */
		if (server->shell_files_icon) {
			cairo_scale(cr, (double)VILE_DOCK_ICON / 512,
				(double)VILE_DOCK_ICON / 512);
			cairo_set_source_surface(cr, server->shell_files_icon, 0, 0);
			cairo_paint(cr);
		} else {
			cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
			cairo_set_line_width(cr, 3);
			cairo_new_path(cr);
			cairo_move_to(cr, 12, 20);
			cairo_line_to(cr, 12, 16);
			cairo_line_to(cr, 24, 16);
			cairo_line_to(cr, 28, 20);
			cairo_line_to(cr, 44, 20);
			cairo_line_to(cr, 44, 42);
			cairo_line_to(cr, 12, 42);
			cairo_close_path(cr);
			cairo_stroke(cr);
		}
		break;
	case DOCK_SETUP:
		/* Gear, no tile — distinct from the Settings ribbon V.
		 * 8 teeth + outer ring + hub. */
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
		cairo_set_line_width(cr, 3);
		for (int t = 0; t < 8; t++) {
			double a = t * M_PI / 4.0;
			double c = cos(a), s = sin(a);
			cairo_move_to(cr, 28 + 19 * c, 28 + 19 * s);
			cairo_line_to(cr, 28 + 26 * c, 28 + 26 * s);
		}
		cairo_stroke(cr);
		cairo_new_path(cr);
		cairo_arc(cr, 28, 28, 15, 0, 2 * M_PI);
		cairo_stroke(cr);
		cairo_new_path(cr);
		cairo_arc(cr, 28, 28, 6, 0, 2 * M_PI);
		cairo_stroke(cr);
		break;
	default:
		break;
	}
	cairo_restore(cr);
}

static void vile_shell_render_dock(struct vile_server *server) {
	/* Frosted dock (Nov 10, 2026 ~19:52 — Adnan, macOS-style:
	 * a clean, slightly translucent frosted pill — you can
	 * faintly see the wallpaper through it, but not too much,
	 * and NOT too black. Dock design v3: 0.5.3 dark solid pill
	 * -> 0.5.4 bare/transparent -> 0.5.5 frosted. The surface
	 * keeps the old size/margins (40px sides, 20px top) so the
	 * node position math in vile_dock_layout is unchanged. The
	 * pill is painted first (frost + tint + hairline); the
	 * icons, running dots, and reorder indicator go on top.
	 * Input is untouched by the frost: hit-testing uses
	 * shell_dock_rect, which is exactly the pill rect. */
	int n = server->dock_n;
	int W = VILE_DOCK_PAD * 2 + VILE_DOCK_ICON * n
		+ VILE_DOCK_GAP * (n > 0 ? n - 1 : 0) + 80;
	int H = VILE_DOCK_H + 60;
	int px = 40, py = 20;
	int dock_w = W - 80;
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, W, H);
	cairo_t *cr = cairo_create(surf);
	/* The surface starts fully transparent outside the pill. */
	const struct vile_palette *P = vile_pal(server->theme_light);
	/* Frost cache: rebuild only when the wallpaper or the dock
	 * rect changes (never per hover frame — the blur loads and
	 * decodes the whole wallpaper). */
	const char *wpath = vile_wallpaper_path();
	struct stat st;
	time_t mtime = 0;
	if (wpath && stat(wpath, &st) == 0) {
		mtime = st.st_mtime;
	}
	if (!server->shell_dock_frost || !wpath ||
			strcmp(wpath, server->shell_dock_frost_path) != 0 ||
			mtime != server->shell_dock_frost_mtime ||
			server->shell_dock_x != server->shell_dock_frost_x ||
			server->shell_dock_y != server->shell_dock_frost_y ||
			dock_w != server->shell_dock_frost_w ||
			VILE_DOCK_H != server->shell_dock_frost_h) {
		cairo_surface_t *frost = vile_shell_build_dock_frost(server,
			server->shell_dock_x, server->shell_dock_y,
			dock_w, VILE_DOCK_H);
		if (frost) {
			if (server->shell_dock_frost) {
				cairo_surface_destroy(server->shell_dock_frost);
			}
			server->shell_dock_frost = frost;
			snprintf(server->shell_dock_frost_path,
				sizeof(server->shell_dock_frost_path),
				"%s", wpath ? wpath : "");
			server->shell_dock_frost_mtime = mtime;
			server->shell_dock_frost_x = server->shell_dock_x;
			server->shell_dock_frost_y = server->shell_dock_y;
			server->shell_dock_frost_w = dock_w;
			server->shell_dock_frost_h = VILE_DOCK_H;
		}
	}
	/* Frosted pill: blurred wallpaper through a rounded rect,
	 * translucent tint on top, hairline edge. Lighter and more
	 * see-through than the bar (whose tint is 0.55): dark theme
	 * is a translucent dark GREY, not near-black; light theme
	 * is a translucent lavender-white. */
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, py + 0.5,
		dock_w - 1, VILE_DOCK_H - 1, 18);
	cairo_clip(cr);
	if (server->shell_dock_frost) {
		cairo_set_source_surface(cr, server->shell_dock_frost,
			px, py);
		cairo_paint(cr);
	}
	if (server->theme_light) {
		cairo_set_source_rgba(cr, 0.988, 0.980, 1.0, 0.55);
	} else {
		cairo_set_source_rgba(cr, 0.16, 0.15, 0.20, 0.38);
	}
	cairo_rectangle(cr, px, py, dock_w, VILE_DOCK_H);
	cairo_fill(cr);
	cairo_reset_clip(cr);
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.15);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, py + 0.5,
		dock_w - 1, VILE_DOCK_H - 1, 18);
	cairo_stroke(cr);
	double dot_y = py + VILE_DOCK_H - 7;
	/* Insertion indicator for a reorder drag: a thin purple bar
	 * at the left edge of the drop slot's cell. */
	if (server->dock_reorder && server->dock_drag_slot >= 0) {
		int to = vile_dock_drop_slot(server, server->dock_drag_x);
		double ix = px + VILE_DOCK_PAD
			+ to * (VILE_DOCK_ICON + VILE_DOCK_GAP)
			- VILE_DOCK_GAP / 2.0;
		cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.9);
		cairo_new_path(cr);
		vile_rounded_rect(cr, ix - 2, py + VILE_DOCK_PAD, 4,
			VILE_DOCK_ICON, 2);
		cairo_fill(cr);
	}
	/* Icons (and their running dots), in slot order. The dragged
	 * icon is drawn last, at the pointer, lifted and slightly
	 * larger — it floats above the others. */
	for (int i = 0; i < n; i++) {
		if (server->dock_reorder && i == server->dock_drag_slot) {
			continue;
		}
		int app = server->dock_order[i];
		double x = px + VILE_DOCK_PAD
			+ i * (VILE_DOCK_ICON + VILE_DOCK_GAP);
		double y = py + VILE_DOCK_PAD;
		/* Launch jump (0.5.6 — KDE Plasma style): while this
		 * app is launching, its icon rises and falls — one
		 * bounce every 500ms, 14px peak, sine-shaped so it
		 * starts and ends at rest. The pill's 20px top margin
		 * fits the rise (icon top reaches y=18 at peak). */
		bool launching = server->dock_launch_ms[app] != 0;
		if (launching) {
			int64_t el = vile_anim_now_ms()
				- server->dock_launch_ms[app];
			int jmp = vile_launch_jump_ms();
			double ph = fmod((double)(el % jmp),
				(double)jmp) / (double)jmp;
			y -= 14.0 * sin(M_PI * ph);
		}
		vile_dock_draw_icon(server, cr, app, x, y,
			server->dock_rendered_scale[i]);
		/* Spinner overlay on the bouncing icon: the circle-spin
		 * loader frame as a purple mask (reads in both themes —
		 * the raw white frames would vanish on the light dock).
		 * 30fps, 28px, centred on the (jumped) icon. */
		if (launching && server->shell_spin_n > 0) {
			int64_t el = vile_anim_now_ms()
				- server->dock_launch_ms[app];
			int fi = (int)((el / 33) % server->shell_spin_n);
			cairo_surface_t *fr = server->shell_spin_frames[fi];
			int fw = cairo_image_surface_get_width(fr);
			int fh = cairo_image_surface_get_height(fr);
			cairo_save(cr);
			cairo_translate(cr,
				x + (VILE_DOCK_ICON - 28.0) / 2.0,
				y + (VILE_DOCK_ICON - 28.0) / 2.0);
			cairo_scale(cr, 28.0 / fw, 28.0 / fh);
			cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
			cairo_mask_surface(cr, fr, 0, 0);
			cairo_restore(cr);
		}
		if (vile_dock_running_count(server, app) > 0) {
			double cx = x + VILE_DOCK_ICON / 2.0;
			cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.30);
			cairo_new_path(cr);
			cairo_arc(cr, cx, dot_y, 7, 0, 2 * M_PI);
			cairo_fill(cr);
			cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
			cairo_new_path(cr);
			cairo_arc(cr, cx, dot_y, 3.5, 0, 2 * M_PI);
			cairo_fill(cr);
		}
	}
	if (server->dock_reorder && server->dock_drag_slot >= 0) {
		/* The dragged icon follows the pointer (surface coords:
		 * the surface starts at dock_x - 40, so subtract that),
		 * lifted 10px and scaled 1.15. */
		double sx = server->dock_drag_x - (server->shell_dock_x - 40)
			- VILE_DOCK_ICON / 2.0;
		double sy = py + VILE_DOCK_PAD - 10;
		vile_dock_draw_icon(server, cr,
			server->dock_order[server->dock_drag_slot],
			sx, sy, 1.15);
	}
	cairo_destroy(cr);
	/* Piece 7: remember the buffer size — the startup dock pop
	 * scales this buffer via dest size and needs the base size. */
	server->shell_dock_buf_w = W;
	server->shell_dock_buf_h = H;
	vile_shell_set_buffer(server, server->shell_dock_node,
		&server->shell_dock_buffer, surf, W, H);
	cairo_surface_destroy(surf);
}

static void vile_shell_render_menu(struct vile_server *server) {
	int W = VILE_MENU_W, H = VILE_MENU_H;
	const struct vile_palette *P = vile_pal(server->theme_light);
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, W, H);
	cairo_t *cr = cairo_create(surf);
	/* Frosted card to match the bar: near-opaque (0.97) so no
	 * window content ghosts through, rounded corners, hairline
	 * edge, soft inner shadows on the left + bottom edges, purple
	 * hairline on the bottom edge (nearest the bar). Rounded
	 * corners leave tiny transparent cutouts — but unlike the old
	 * TOP menu, whose top-right cutout sat over the tiled
	 * window's titlebar buttons, this menu's corners sit over
	 * dark window content / wallpaper (menu top y=720), so
	 * nothing light shows through. A true live blur of the
	 * window behind the menu would need scene-graph blur, which
	 * wlroots does not offer here — the near-opaque tint is the
	 * honest version. */
	cairo_set_source_rgba(cr, P->panel_rgb[0], P->panel_rgb[1], P->panel_rgb[2], 0.97);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 0.5, 0.5, W - 1, H - 1, 16);
	cairo_fill(cr);
	/* Inner shadows: left + bottom edges, clipped to the card. */
	cairo_new_path(cr);
	vile_rounded_rect(cr, 0.5, 0.5, W - 1, H - 1, 16);
	cairo_clip(cr);
	cairo_pattern_t *lsh = cairo_pattern_create_linear(0, 0, 12, 0);
	cairo_pattern_add_color_stop_rgba(lsh, 0, 0, 0, 0, 0.22);
	cairo_pattern_add_color_stop_rgba(lsh, 1, 0, 0, 0, 0);
	cairo_set_source(cr, lsh);
	cairo_rectangle(cr, 0, 0, 12, H);
	cairo_fill(cr);
	cairo_pattern_destroy(lsh);
	cairo_pattern_t *bsh = cairo_pattern_create_linear(0, H - 12, 0, H);
	cairo_pattern_add_color_stop_rgba(bsh, 0, 0, 0, 0, 0);
	cairo_pattern_add_color_stop_rgba(bsh, 1, 0, 0, 0, 0.22);
	cairo_set_source(cr, bsh);
	cairo_rectangle(cr, 0, H - 12, W, 12);
	cairo_fill(cr);
	cairo_pattern_destroy(bsh);
	cairo_reset_clip(cr);
	/* Hairline edge + purple bottom hairline. */
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.12);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 0.5, 0.5, W - 1, H - 1, 16);
	cairo_stroke(cr);
	cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.35);
	cairo_set_line_width(cr, 1.2);
	cairo_new_path(cr);
	cairo_move_to(cr, 18, H - 1.5);
	cairo_line_to(cr, W - 18, H - 1.5);
	cairo_stroke(cr);

	vile_draw_text_at(cr, "Inter Bold 12", P->muted[0], P->muted[1], P->muted[2],
		20, 16, "SYSTEM");

	/* Wi-Fi row (real status from /sys). */
	char wifi[64], batt[64];
	vile_shell_wifi_status(wifi, sizeof(wifi));
	vile_shell_battery_status(batt, sizeof(batt));
	vile_draw_text_at(cr, "Inter 18", P->text[0], P->text[1], P->text[2], 20, 54, "Wi-Fi");
	vile_shell_draw_text_right(cr, "Inter 16", P->text2[0], P->text2[1], P->text2[2],
		W - 20, 56, wifi);
	/* Volume row: the slider is VISUAL ONLY for now (no audio
	 * control is wired in the compositor) — it does not claim a
	 * real level. */
	vile_draw_text_at(cr, "Inter 18", P->text[0], P->text[1], P->text[2], 20, 94, "Volume");
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.15);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 150, 101, 130, 6, 3);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 150, 101, 130 * 0.6, 6, 3);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, P->white[0], P->white[1], P->white[2]);
	cairo_new_path(cr);
	cairo_arc(cr, 150 + 130 * 0.6, 104, 7, 0, 2 * M_PI);
	cairo_fill(cr);
	/* Battery row (real status from /sys). */
	vile_draw_text_at(cr, "Inter 18", P->text[0], P->text[1], P->text[2], 20, 134, "Battery");
	vile_shell_draw_text_right(cr, "Inter 16", P->text2[0], P->text2[1], P->text2[2],
		W - 20, 136, batt);

	/* Separator, then the actions that actually do something. */
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.10);
	cairo_rectangle(cr, 20, 172, W - 40, 1);
	cairo_fill(cr);
	vile_draw_lock_glyph(cr, 20, 190, 20, P->text[0], P->text[1], P->text[2]);
	vile_draw_text_at(cr, "Inter 18", P->text[0], P->text[1], P->text[2], 48, 190, "Lock");
	vile_draw_text_at(cr, "Inter 18", P->text[0], P->text[1], P->text[2], 20, 230, "Settings");
	/* Log Out (0.5.4 post-theme fixes — session switching: back
	 * to the LightDM greeter, or to the text console when Vile
	 * was started from one). Text-only row, like Settings. */
	vile_draw_text_at(cr, "Inter 18", P->text[0], P->text[1], P->text[2], 20, 270, "Log Out");
	vile_draw_power_glyph(cr, 30, 320, 8, P->text[0], P->text[1], P->text[2]);
	vile_draw_text_at(cr, "Inter 18", P->text[0], P->text[1], P->text[2], 48, 310, "Power Off");

	cairo_destroy(cr);
	vile_shell_set_buffer(server, server->shell_menu_node,
		&server->shell_menu_buffer, surf, W, H);
	cairo_surface_destroy(surf);
}

static void vile_shell_raise(struct vile_server *server) {
	if (!server->shell_created || server->locked) {
		return;
	}
	wlr_scene_node_raise_to_top(&server->shell_tree->node);
}

/* Re-render whatever changed: the bar when the minute ticks, the
 * dock when the running-app set changes. */
static void vile_shell_update(struct vile_server *server) {
	if (!server->shell_created) {
		return;
	}
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char minute[16];
	strftime(minute, sizeof(minute), "%H:%M", &tm);
	if (strcmp(minute, server->shell_minute) != 0) {
		snprintf(server->shell_minute, sizeof(server->shell_minute),
			"%s", minute);
		vile_shell_render_bar(server);
		if (server->shell_menu_open) {
			vile_shell_render_menu(server);
		}
	}
	/* Wallpaper hook watcher (Settings wallpaper switching): the
	 * hook file's mtime is cheap to stat once a second, and the
	 * hook only changes when the user picks a wallpaper, so the
	 * reload is effectively instant and costs nothing otherwise.
	 * The env var still wins — vile_wallpaper_path() returns it
	 * first, so a VILE_WALLPAPER launch ignores the hook. */
	char hook_path[1024];
	if (vile_wallpaper_hook_path(hook_path, sizeof(hook_path))) {
		struct stat st;
		if (stat(hook_path, &st) == 0 &&
				st.st_mtime != server->wallpaper_hook_mtime) {
			server->wallpaper_hook_mtime = st.st_mtime;
			vile_reload_wallpaper(server);
			/* Bar Redesign: the bar samples the wallpaper, so
			 * a wallpaper change must re-render the bar (and
			 * its frost) now, not at the next minute tick. */
			vile_shell_render_bar(server);
			/* Frosted dock: same — the dock pill samples the
			 * wallpaper, so re-render it now or the frost
			 * stays the old wallpaper's until the next dock
			 * event (hover/reorder). */
			vile_shell_render_dock(server);
		}
	}
	/* Theme watcher (0.5.4): same pattern as the wallpaper hook —
	 * Settings and the Setup wizard write ~/.config/vile/theme,
	 * the compositor applies it live within a second. VILE_THEME
	 * (test override) wins, so a forced launch ignores the file. */
	if (!getenv("VILE_THEME") || !*getenv("VILE_THEME")) {
		char theme_path[1024];
		if (vile_theme_path(theme_path, sizeof(theme_path))) {
			struct stat st;
			if (stat(theme_path, &st) == 0) {
				if (st.st_mtime != server->theme_mtime) {
					server->theme_mtime = st.st_mtime;
					vile_apply_theme(server,
						vile_read_theme());
				}
			} else if (server->theme_mtime != 0) {
				/* Theme file deleted: back to dark. */
				server->theme_mtime = 0;
				vile_apply_theme(server, vile_read_theme());
			}
		}
	}
	/* Dock refresh (Nov 10, 2026 ~16:31): the Setup icon's
	 * lifecycle — the wizard writes setup-done, and the icon
	 * must disappear by itself within a second. Returns true
	 * when the order changed (it re-renders the dock itself). */
	vile_dock_refresh(server);
	int foot_n = 0, set_n = 0, screenfit_n = 0, files_n = 0;
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		const char *title = t->xdg_toplevel->title;
		if (t->app_files) {
			files_n++;
			continue;
		}
		if (!title) {
			continue;
		}
		if (strncmp(title, "foot", 4) == 0) {
			foot_n++;
		} else if (strncmp(title, "Settings", 8) == 0) {
			set_n++;
		} else if (strncmp(title, "ScreenFit", 9) == 0) {
			screenfit_n++;
		}
	}
	if (foot_n != server->shell_dock_foot_n ||
			set_n != server->shell_dock_set_n ||
			screenfit_n != server->shell_dock_screenfit_n ||
			files_n != server->shell_dock_files_n ||
			server->focused_toplevel != server->shell_dock_focused) {
		server->shell_dock_foot_n = foot_n;
		server->shell_dock_set_n = set_n;
		server->shell_dock_screenfit_n = screenfit_n;
		server->shell_dock_files_n = files_n;
		server->shell_dock_focused = server->focused_toplevel;
		vile_shell_render_dock(server);
	}
	vile_schedule_frames(server);
}

static int vile_shell_timer(void *data) {
	struct vile_server *server = data;
	vile_shell_update(server);
	wl_event_source_timer_update(server->shell_timer, 1000);
	return 0;
}

static void vile_shell_fit(struct vile_server *server, int out_w, int out_h) {
	if (!server->shell_created) {
		return;
	}
	server->shell_w = out_w;
	server->shell_h = out_h;
	/* Dock geometry (centred, width from dock_n): vile_dock_layout
	 * sets dock_x/dock_y, the dock rect, the per-slot rects, and
	 * the dock node position. */
	vile_dock_layout(server);
	/* The menu rises UP from the bottom bar: its bottom edge sits
	 * 8px above the bar top, flush right, never clipped. */
	server->shell_menu_x = out_w - VILE_MENU_W;
	server->shell_menu_y = out_h - VILE_BAR_H - 8 - VILE_MENU_H;
	if (server->shell_bar_node) {
		wlr_scene_node_set_position(&server->shell_bar_node->node,
			0, out_h - VILE_BAR_H);
	}
	if (server->shell_menu_node) {
		wlr_scene_node_set_position(&server->shell_menu_node->node,
			server->shell_menu_x, server->shell_menu_y);
	}
	server->shell_menu_rect = (struct wlr_box){
		.x = server->shell_menu_x, .y = server->shell_menu_y,
		.width = VILE_MENU_W, .height = VILE_MENU_H };
	server->shell_menu_lock_rect = (struct wlr_box){
		.x = server->shell_menu_x, .y = server->shell_menu_y + 180,
		.width = VILE_MENU_W, .height = 40 };
	server->shell_menu_settings_rect = (struct wlr_box){
		.x = server->shell_menu_x, .y = server->shell_menu_y + 220,
		.width = VILE_MENU_W, .height = 40 };
	server->shell_menu_power_rect = (struct wlr_box){
		.x = server->shell_menu_x, .y = server->shell_menu_y + 300,
		.width = VILE_MENU_W, .height = 40 };
	server->shell_menu_logout_rect = (struct wlr_box){
		.x = server->shell_menu_x, .y = server->shell_menu_y + 260,
		.width = VILE_MENU_W, .height = 40 };
	/* Bar Redesign: the bar surface is full output width, so an
	 * output resize must re-render it (the 1-second update only
	 * re-renders on a minute change). */
	vile_shell_render_bar(server);
	vile_shell_update(server);
}

static void vile_shell_create(struct vile_server *server, int out_w, int out_h) {
	if (server->shell_created) {
		return;
	}
	server->shell_created = true;
	server->shell_w = out_w;
	server->shell_h = out_h;
	/* Piece 7: animation state — icons start at scale 1.0, no
	 * hover, no pop, menu closed (dy +16 so the first open rises). */
	server->dock_hover = -1;
	server->dock_pop_icon = -1;
	server->menu_dy = 16.0f;
	server->menu_op = 0.0f;
	for (int i = 0; i < VILE_DOCK_MAX; i++) {
		server->dock_icon_scale[i] = 1.0f;
		server->dock_rendered_scale[i] = 1.0f;
		server->dock_icon_from[i] = 1.0f;
		server->dock_icon_to[i] = 1.0f;
		server->dock_icon_ms[i] = 0;
	}
	/* Dock state (Nov 10, 2026 ~16:31): no press armed, no
	 * reorder live, unpin menu closed. The pinned order is loaded
	 * from ~/.config/vile/dock-pinned (defaults when missing) —
	 * BEFORE fit, so the geometry comes from the loaded order. */
	server->dock_press_slot = -1;
	server->dock_press_down = false;
	server->dock_press_moved = false;
	server->dock_reorder = false;
	server->dock_drag_slot = -1;
	server->dock_drag_x = 0;
	server->shell_unpin_open = false;
	server->shell_unpin_slot = -1;
	vile_dock_load(server);
	/* Shell tree: bar + dock + menu, above the windows. (The
	 * desktop clock used to have its own tree below the windows —
	 * it was removed Nov 10, 2026 with the clock itself.) */
	server->shell_tree = wlr_scene_tree_create(&server->scene->tree);
	if (!server->shell_tree) {
		wlr_log(WLR_ERROR, "Vile: shell: shell tree create failed");
		return;
	}
	server->shell_bar_node =
		wlr_scene_buffer_create(server->shell_tree, NULL);
	server->shell_dock_node =
		wlr_scene_buffer_create(server->shell_tree, NULL);
	server->shell_menu_node =
		wlr_scene_buffer_create(server->shell_tree, NULL);
	server->shell_unpin_node =
		wlr_scene_buffer_create(server->shell_tree, NULL);
	if (server->shell_unpin_node) {
		wlr_scene_node_set_enabled(&server->shell_unpin_node->node,
			false);
	}
	if (server->shell_menu_node) {
		wlr_scene_node_set_enabled(&server->shell_menu_node->node,
			false);
	}
	/* Foot icon for the dock: the BARE footprint (foot-logo.png,
	 * transparent, no tile) — the transparent dock shows bare
	 * glyphs. Fallback to the old dark tile if the bare file is
	 * missing (it still reads fine without a pill behind it). */
	const char *icon_paths[] = {
		"../assets/foot-logo.png",
		"assets/foot-logo.png",
		"../assets/foot-logo-dark.png",
		"assets/foot-logo-dark.png",
		NULL,
	};
	for (int i = 0; icon_paths[i]; i++) {
		cairo_surface_t *img =
			cairo_image_surface_create_from_png(icon_paths[i]);
		if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
			server->shell_foot_icon = img;
			wlr_log(WLR_INFO,
				"Vile: shell: dock foot icon loaded from '%s'",
				icon_paths[i]);
			break;
		}
		cairo_surface_destroy(img);
	}
	if (!server->shell_foot_icon) {
		wlr_log(WLR_ERROR,
			"Vile: shell: foot-logo.png not found — dock falls back to text");
	}
	/* Files icon for the dock: the BARE folder + page
	 * (files-icon-bare.png, 512px, transparent). Fallback to the
	 * old tile. Same candidate paths as the foot logo —
	 * compositor dir on this VM and the ISO layout. */
	const char *files_icon_paths[] = {
		"../assets/files-icon-bare.png",
		"assets/files-icon-bare.png",
		"../assets/files-icon.png",
		"assets/files-icon.png",
		NULL,
	};
	for (int i = 0; files_icon_paths[i]; i++) {
		cairo_surface_t *img =
			cairo_image_surface_create_from_png(files_icon_paths[i]);
		if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
			server->shell_files_icon = img;
			wlr_log(WLR_INFO,
				"Vile: shell: dock files icon loaded from '%s'",
				files_icon_paths[i]);
			break;
		}
		cairo_surface_destroy(img);
	}
	if (!server->shell_files_icon) {
		wlr_log(WLR_ERROR,
			"Vile: shell: files-icon.png not found — dock falls back to a drawn folder");
	}
	/* Vile logo for the dock Settings icon: the BARE ribbon V
	 * (vile-logo.png, transparent) — the ribbon stays as the
	 * Settings icon (only the bar corner went). Fallback to the
	 * dark tile. Same candidate paths as the other dock icons. */
	const char *vile_icon_paths[] = {
		"../assets/vile-logo.png",
		"assets/vile-logo.png",
		"../assets/vile-logo-dark.png",
		"assets/vile-logo-dark.png",
		NULL,
	};
	for (int i = 0; vile_icon_paths[i]; i++) {
		cairo_surface_t *img =
			cairo_image_surface_create_from_png(vile_icon_paths[i]);
		if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
			server->shell_vile_icon = img;
			wlr_log(WLR_INFO,
				"Vile: shell: dock/bar Vile icon loaded from '%s'",
				vile_icon_paths[i]);
			break;
		}
		cairo_surface_destroy(img);
	}
	if (!server->shell_vile_icon) {
		wlr_log(WLR_ERROR,
			"Vile: shell: vile-logo.png not found — dock falls back to the drawn V");
	}
	vile_shell_fit(server, out_w, out_h);
	/* Render the dock once here: vile_shell_update() only re-renders
	 * it when the running-app set CHANGES, and at startup the set is
	 * already empty (0 == 0), so without this the dock node never
	 * gets a buffer and the bare desktop has no dock at all. */
	vile_shell_render_dock(server);
	wlr_log(WLR_INFO, "Vile: shell created: bar %dx%d", out_w, VILE_BAR_H);
	vile_dock_log_centres(server);
}

static void vile_shell_set_menu(struct vile_server *server, bool open) {
	if (!server->shell_created) {
		return;
	}
	server->shell_menu_open = open;
	if (open) {
		vile_shell_render_menu(server);
		if (server->shell_menu_node) {
			wlr_scene_node_set_enabled(&server->shell_menu_node->node,
				true);
		}
		/* Piece 7: rise + fade in from the bottom bar. */
		vile_anim_menu_open(server);
		wlr_log(WLR_INFO,
			"Vile: shell menu opened: Lock (%d,%d %dx%d) Settings (%d,%d %dx%d) Power (%d,%d %dx%d)",
			server->shell_menu_lock_rect.x,
			server->shell_menu_lock_rect.y,
			server->shell_menu_lock_rect.width,
			server->shell_menu_lock_rect.height,
			server->shell_menu_settings_rect.x,
			server->shell_menu_settings_rect.y,
			server->shell_menu_settings_rect.width,
			server->shell_menu_settings_rect.height,
			server->shell_menu_power_rect.x,
			server->shell_menu_power_rect.y,
			server->shell_menu_power_rect.width,
			server->shell_menu_power_rect.height);
	} else {
		/* Piece 7: fade + sink out — the node is disabled by the
		 * animation tick when the fade finishes. With animations
		 * off it disappears immediately, as before. */
		if (vile_anim_enabled()) {
			vile_anim_menu_close(server);
		} else if (server->shell_menu_node) {
			wlr_scene_node_set_enabled(&server->shell_menu_node->node,
				false);
		}
		wlr_log(WLR_INFO, "Vile: shell menu closed");
	}
	vile_schedule_frames(server);
}

static void vile_shell_close_menu(struct vile_server *server) {
	if (server->shell_menu_open) {
		vile_shell_set_menu(server, false);
	}
}

/* Open Vile Settings (post-login only — the lock screen never calls
 * this). The launcher lives next to the compositor: ../settings/
 * from the compositor dir on both this VM and Adnan's laptop. */
static void vile_spawn_settings(struct vile_server *server) {
	(void)server;
	const char *cands[] = {
		"../settings/vile-settings",
		"settings/vile-settings",
		NULL,
	};
	for (int i = 0; cands[i]; i++) {
		if (access(cands[i], X_OK) == 0) {
			wlr_log(WLR_INFO, "Vile: shell: opening Settings (%s)",
				cands[i]);
			vile_spawn(cands[i]);
			return;
		}
	}
	const char *home = getenv("HOME");
	if (home) {
		char path[1024];
		snprintf(path, sizeof(path),
			"%s/vile-os/settings/vile-settings", home);
		if (access(path, X_OK) == 0) {
			wlr_log(WLR_INFO, "Vile: shell: opening Settings (%s)",
				path);
			vile_spawn(path);
			return;
		}
		snprintf(path, sizeof(path),
			"%s/workspace/vile-os/settings/vile-settings", home);
		if (access(path, X_OK) == 0) {
			wlr_log(WLR_INFO, "Vile: shell: opening Settings (%s)",
				path);
			vile_spawn(path);
			return;
		}
	}
	wlr_log(WLR_ERROR, "Vile: shell: Settings launcher not found");
}

/* Open ScreenFit (display / overscan settings, Python+GTK3). On
 * Adnan's laptop it is installed as `screenfit` (makepkg, so it is
 * on PATH); on this VM it lives in
 * ~/workspace/intel-display-center/src/. */
static void vile_spawn_screenfit(struct vile_server *server) {
	(void)server;
	const char *cands[] = {
		"/usr/bin/screenfit",
		"/usr/local/bin/screenfit",
		NULL,
	};
	for (int i = 0; cands[i]; i++) {
		if (access(cands[i], X_OK) == 0) {
			wlr_log(WLR_INFO, "Vile: shell: opening ScreenFit (%s)",
				cands[i]);
			vile_spawn(cands[i]);
			return;
		}
	}
	const char *home = getenv("HOME");
	if (home) {
		char path[1024];
		const char *tails[] = {
			"/.local/bin/screenfit",
			"/workspace/intel-display-center/src/screenfit",
			"/intel-display-center/src/screenfit",
			NULL,
		};
		for (int i = 0; tails[i]; i++) {
			snprintf(path, sizeof(path), "%s%s", home, tails[i]);
			if (access(path, X_OK) == 0) {
				wlr_log(WLR_INFO,
					"Vile: shell: opening ScreenFit (%s)", path);
				vile_spawn(path);
				return;
			}
		}
	}
	/* Last resort: let the shell resolve it from PATH. */
	wlr_log(WLR_INFO, "Vile: shell: opening ScreenFit (PATH lookup)");
	vile_spawn("screenfit");
}

/* Open Vile Files (file manager, Python+GTK3). The launcher lives
 * next to the compositor: ../filemanager/ from the compositor dir
 * on both this VM and Adnan's laptop. */
static void vile_spawn_files(struct vile_server *server) {
	(void)server;
	const char *cands[] = {
		"../filemanager/vile-files",
		"filemanager/vile-files",
		NULL,
	};
	for (int i = 0; cands[i]; i++) {
		if (access(cands[i], X_OK) == 0) {
			wlr_log(WLR_INFO, "Vile: shell: opening Files (%s)",
				cands[i]);
			vile_spawn(cands[i]);
			return;
		}
	}
	const char *home = getenv("HOME");
	if (home) {
		char path[1024];
		snprintf(path, sizeof(path),
			"%s/vile-os/filemanager/vile-files", home);
		if (access(path, X_OK) == 0) {
			wlr_log(WLR_INFO, "Vile: shell: opening Files (%s)",
				path);
			vile_spawn(path);
			return;
		}
		snprintf(path, sizeof(path),
			"%s/workspace/vile-os/filemanager/vile-files", home);
		if (access(path, X_OK) == 0) {
			wlr_log(WLR_INFO, "Vile: shell: opening Files (%s)",
				path);
			vile_spawn(path);
			return;
		}
	}
	/* Last resort: let the shell resolve it from PATH. */
	wlr_log(WLR_INFO, "Vile: shell: opening Files (PATH lookup)");
	vile_spawn("vile-files");
}

/* First window of the app (by title prefix) that is NOT currently
 * focused — so a dock click moves focus to another window of the
 * same app when one exists. Falls back to the most recent window
 * of the app (already focused) when there is only one. */
static struct vile_toplevel *vile_shell_find_app(struct vile_server *server,
		const char *prefix) {
	size_t n = strlen(prefix);
	struct vile_toplevel *first = NULL, *t;
	wl_list_for_each(t, &server->toplevels, link) {
		const char *title = t->xdg_toplevel->title;
		/* Case-insensitive: dock ids are lowercase ("settings")
		 * but window titles are capitalised ("Settings") — a
		 * case-sensitive match never finds Settings/ScreenFit
		 * and a second dock click spawns a duplicate window. */
		if (!title || strncasecmp(title, prefix, n) != 0) {
			continue;
		}
		if (!first) {
			first = t;
		}
		if (t != server->focused_toplevel) {
			return t;
		}
	}
	return first;
}

/* First Files window (by the app_files flag, not the title — the
 * Files title is the current folder name) that is NOT currently
 * focused, else the most recent Files window. */
static struct vile_toplevel *vile_shell_find_files(struct vile_server *server) {
	struct vile_toplevel *first = NULL, *t;
	wl_list_for_each(t, &server->toplevels, link) {
		if (!t->app_files) {
			continue;
		}
		if (!first) {
			first = t;
		}
		if (t != server->focused_toplevel) {
			return t;
		}
	}
	return first;
}

/* Launch jump (0.5.6 — KDE Plasma style): a dock click that spawns
 * an app starts its icon bouncing until the app's window maps (or
 * the 5s timeout). Per-app start times, so two apps can bounce at
 * once. Focusing an already-open app does NOT jump. */

/* Circle-spin loader frames (assets/loaders/circle-spin/, 53
 * transparent PNGs, 30fps) — drawn as a purple mask on a bouncing
 * icon while its app loads. Loaded once, lazily, on the first dock
 * launch; the dock falls back to the bare bounce if they are
 * missing. */
static void vile_shell_load_spin_frames(struct vile_server *server) {
	if (server->shell_spin_n > 0) {
		return;
	}
	const char *paths[] = {
		"../assets/loaders/circle-spin/frame-%03d.png",
		"assets/loaders/circle-spin/frame-%03d.png",
		NULL,
	};
	for (int i = 0; i < VILE_SPIN_MAX; i++) {
		cairo_surface_t *img = NULL;
		for (int p = 0; paths[p]; p++) {
			char buf[256];
			snprintf(buf, sizeof(buf), paths[p], i);
			img = cairo_image_surface_create_from_png(buf);
			if (cairo_surface_status(img)
					== CAIRO_STATUS_SUCCESS) {
				break;
			}
			cairo_surface_destroy(img);
			img = NULL;
		}
		if (!img) {
			break;
		}
		server->shell_spin_frames[server->shell_spin_n++] = img;
	}
	if (server->shell_spin_n > 0) {
		wlr_log(WLR_INFO, "Vile: shell: loaded %d spin frames",
			server->shell_spin_n);
	}
}

static void vile_shell_dock_launch_start(struct vile_server *server,
		int app) {
	if (!vile_anim_enabled()) {
		return; /* VILE_ANIM_OFF=1: no jump, the app just opens. */
	}
	if (app < 0 || app >= DOCK_APP_COUNT) {
		return;
	}
	if (server->dock_launch_ms[app] != 0) {
		return; /* Already launching — a second click must not
			 * re-spawn or restart the bounce. */
	}
	vile_shell_load_spin_frames(server);
	server->dock_launch_ms[app] = vile_anim_now_ms();
	wlr_log(WLR_INFO, "Vile: dock: launch jump start '%s'",
		vile_dock_app_names[app]);
	vile_schedule_frames(server);
}

/* Has the launched app's window appeared yet? Same title matching
 * as the 1s dock refresh, plus the wizard's "Vile Setup". */
static bool vile_dock_launch_appeared(struct vile_server *server,
		int app) {
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		if (app == DOCK_FILES) {
			if (t->app_files) {
				return true;
			}
			continue;
		}
		const char *title = t->xdg_toplevel->title;
		if (!title) {
			continue;
		}
		switch (app) {
		case DOCK_FOOT:
			if (strncasecmp(title, "foot", 4) == 0) {
				return true;
			}
			break;
		case DOCK_SETTINGS:
			if (strncasecmp(title, "Settings", 8) == 0) {
				return true;
			}
			break;
		case DOCK_SCREENFIT:
			if (strncasecmp(title, "ScreenFit", 9) == 0) {
				return true;
			}
			break;
		case DOCK_SETUP:
			if (strncasecmp(title, "Vile Setup", 10) == 0) {
				return true;
			}
			break;
		default:
			break;
		}
	}
	return false;
}

/* Dock activation (Nov 10, 2026 ~16:31 — slot-based): focus the
 * app if it is already open, else spawn it. Setup opens the
 * first-run wizard. */
static void vile_shell_dock_activate_app(struct vile_server *server,
		int app) {
	/* Launch jump (0.5.6): a second click while the app is still
	 * launching must not spawn a duplicate — just pop the icon.
	 * (A window that already exists is focused by the checks
	 * below, which run first.) */
	if (server->dock_launch_ms[app] != 0) {
		int slot = vile_dock_slot_of_app(server, app);
		if (slot >= 0) {
			vile_anim_dock_pop(server, slot);
		}
		return;
	}
	if (app == DOCK_FILES) {
		struct vile_toplevel *f = vile_shell_find_files(server);
		if (f) {
			wlr_log(WLR_INFO, "Vile: dock: focus 'Files'");
			vile_focus_toplevel(server, f);
			return;
		}
		wlr_log(WLR_INFO, "Vile: dock: open Files");
		vile_spawn_files(server);
		vile_shell_dock_launch_start(server, app);
		return;
	}
	if (app == DOCK_SETUP) {
		wlr_log(WLR_INFO, "Vile: dock: open Setup");
		vile_spawn_setup();
		vile_shell_dock_launch_start(server, app);
		return;
	}
	const char *prefix = vile_dock_app_names[app];
	struct vile_toplevel *t = vile_shell_find_app(server, prefix);
	if (t) {
		wlr_log(WLR_INFO, "Vile: dock: focus '%s'", prefix);
		vile_focus_toplevel(server, t);
		return;
	}
	if (app == DOCK_FOOT) {
		wlr_log(WLR_INFO, "Vile: dock: open foot");
		vile_spawn("foot");
	} else if (app == DOCK_SCREENFIT) {
		wlr_log(WLR_INFO, "Vile: dock: open ScreenFit");
		vile_spawn_screenfit(server);
	} else {
		wlr_log(WLR_INFO, "Vile: dock: open Settings");
		vile_spawn_settings(server);
	}
	vile_shell_dock_launch_start(server, app);
}

/* Unpin menu (Nov 10, 2026 ~16:31): a small card above the dock
 * icon with one row, "Unpin". Positioned above the icon, clamped
 * to the output. */
#define VILE_UNPIN_W 200
#define VILE_UNPIN_H 56
static void vile_shell_render_unpin(struct vile_server *server) {
	int W = VILE_UNPIN_W, H = VILE_UNPIN_H;
	const struct vile_palette *P = vile_pal(server->theme_light);
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, W, H);
	cairo_t *cr = cairo_create(surf);
	/* Near-opaque card, same language as the system menu. */
	cairo_set_source_rgba(cr, P->panel_rgb[0], P->panel_rgb[1],
		P->panel_rgb[2], 0.97);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 0.5, 0.5, W - 1, H - 1, 12);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, P->edge[0], P->edge[1], P->edge[2], 0.12);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 0.5, 0.5, W - 1, H - 1, 12);
	cairo_stroke(cr);
	/* One row: "Unpin" + a small pin glyph, left-aligned. */
	vile_draw_text_at(cr, "Inter 15", P->text[0], P->text[1], P->text[2],
		16, 19, "Unpin");
	cairo_destroy(cr);
	vile_shell_set_buffer(server, server->shell_unpin_node,
		&server->shell_unpin_buffer, surf, W, H);
	cairo_surface_destroy(surf);
}

static void vile_shell_set_unpin(struct vile_server *server, bool open,
		int slot) {
	if (!server->shell_created) {
		return;
	}
	if (open) {
		if (slot < 0 || slot >= server->dock_n) {
			return;
		}
		/* Close the system menu — the two menus never overlap. */
		vile_shell_set_menu(server, false);
		server->shell_unpin_open = true;
		server->shell_unpin_slot = slot;
		int cx = server->shell_dock_rects[slot].x
			+ server->shell_dock_rects[slot].width / 2;
		int mx = cx - VILE_UNPIN_W / 2;
		if (mx < 8) {
			mx = 8;
		}
		if (mx + VILE_UNPIN_W > server->shell_w - 8) {
			mx = server->shell_w - 8 - VILE_UNPIN_W;
		}
		int my = server->shell_dock_y - 8 - VILE_UNPIN_H;
		if (my < 0) {
			my = 0;
		}
		server->shell_unpin_rect = (struct wlr_box){
			.x = mx, .y = my, .width = VILE_UNPIN_W,
			.height = VILE_UNPIN_H };
		server->shell_unpin_row_rect = server->shell_unpin_rect;
		vile_shell_render_unpin(server);
		if (server->shell_unpin_node) {
			wlr_scene_node_set_enabled(
				&server->shell_unpin_node->node, true);
			wlr_scene_node_set_position(
				&server->shell_unpin_node->node, mx, my);
			wlr_scene_node_raise_to_top(
				&server->shell_unpin_node->node);
		}
		wlr_log(WLR_INFO, "Vile: shell: unpin menu open for '%s' "
			"at (%d,%d) %dx%d row centre (%d,%d)",
			vile_dock_app_names[server->dock_order[slot]],
			mx, my, VILE_UNPIN_W, VILE_UNPIN_H,
			mx + VILE_UNPIN_W / 2, my + VILE_UNPIN_H / 2);
	} else {
		server->shell_unpin_open = false;
		server->shell_unpin_slot = -1;
		if (server->shell_unpin_node) {
			wlr_scene_node_set_enabled(
				&server->shell_unpin_node->node, false);
		}
	}
}

/* Unpin a dock entry (Nov 10, 2026 ~16:31): remove it from the
 * order and persist. Unpinning Setup marks it hidden (the marker
 * file) so it stays gone even though setup is still incomplete. */
static void vile_dock_unpin(struct vile_server *server, int slot) {
	if (slot < 0 || slot >= server->dock_n) {
		return;
	}
	int app = server->dock_order[slot];
	if (app == DOCK_SETUP) {
		server->dock_setup_shown = false;
		const char *home = getenv("HOME");
		if (home) {
			char dir[1024];
			snprintf(dir, sizeof(dir), "%s/.config", home);
			mkdir(dir, 0755);
			snprintf(dir, sizeof(dir), "%s/.config/vile", home);
			mkdir(dir, 0755);
			char path[1024];
			if (vile_dock_config_path(path, sizeof(path),
					"dock-setup-hidden")) {
				FILE *f = fopen(path, "w");
				if (f) {
					fclose(f);
				}
			}
		}
		wlr_log(WLR_INFO, "Vile: dock: unpinned 'setup' (hidden)");
	} else {
		wlr_log(WLR_INFO, "Vile: dock: unpinned '%s'",
			vile_dock_app_names[app]);
	}
	memmove(&server->dock_order[slot], &server->dock_order[slot + 1],
		(server->dock_n - slot - 1) * sizeof(int));
	server->dock_n--;
	if (app != DOCK_SETUP) {
		vile_dock_save(server);
	}
	vile_dock_layout(server);
	vile_shell_render_dock(server);
	vile_dock_log_centres(server);
}

/* Hold timer (400ms): a press held still on a dock icon becomes a
 * reorder drag. If the pointer moved >12px before the timer fired,
 * it was a drag-away, not a hold — no reorder. */
static int vile_dock_hold_timer_cb(void *data) {
	struct vile_server *server = data;
	if (server->dock_press_slot < 0 || !server->dock_press_down
			|| server->dock_reorder || server->dock_press_moved) {
		return 0;
	}
	int slot = server->dock_press_slot;
	server->dock_reorder = true;
	server->dock_drag_slot = slot;
	server->dock_drag_x = server->cursor->x;
	wlr_log(WLR_INFO, "Vile: dock: reorder drag '%s' (slot %d)",
		vile_dock_app_names[server->dock_order[slot]], slot);
	vile_shell_render_dock(server);
	vile_schedule_frames(server);
	return 0;
}

/* Button release for a dock press / reorder drag. A reorder drops
 * the app at the slot under the pointer (and persists); a quick
 * click with no movement pops + activates the app; a drag-away
 * (moved >12px, no reorder) does nothing. A HOLD with no movement
 * is still a click: a press held past the 400ms hold timer becomes
 * a reorder drag, but if it is released in the same slot without
 * ever moving, it pops + activates — otherwise any slow click
 * (a QEMU HMP click holds the button ~1s; a hesitant human click
 * holds it too) would silently do nothing. That was the 0.5.4
 * dock-click regression. */
static void vile_dock_release(struct vile_server *server) {
	if (server->dock_hold_timer) {
		wl_event_source_timer_update(server->dock_hold_timer, 0);
	}
	server->dock_press_down = false;
	if (server->dock_reorder) {
		int from = server->dock_drag_slot;
		int to = vile_dock_drop_slot(server, server->dock_drag_x);
		int app = server->dock_order[from];
		if (to != from) {
			if (to > from) {
				memmove(&server->dock_order[from],
					&server->dock_order[from + 1],
					(to - from) * sizeof(int));
			} else {
				memmove(&server->dock_order[to + 1],
					&server->dock_order[to],
					(from - to) * sizeof(int));
			}
			server->dock_order[to] = app;
			vile_dock_save(server);
			wlr_log(WLR_INFO, "Vile: dock: reorder '%s' %d -> %d",
				vile_dock_app_names[app], from, to);
		} else {
			wlr_log(WLR_INFO, "Vile: dock: reorder '%s' dropped "
				"in place (slot %d)",
				vile_dock_app_names[app], from);
			/* Hold with no movement: still a click. A real
			 * drag-away moved >12px, so dock_press_moved
			 * tells the two apart. */
			if (!server->dock_press_moved) {
				vile_anim_dock_pop(server, from);
				vile_shell_dock_activate_app(server, app);
			}
		}
		server->dock_reorder = false;
		server->dock_drag_slot = -1;
		server->dock_press_slot = -1;
		vile_dock_layout(server);
		vile_shell_render_dock(server);
		vile_dock_log_centres(server);
		return;
	}
	if (server->dock_press_slot >= 0) {
		int slot = server->dock_press_slot;
		server->dock_press_slot = -1;
		if (!server->dock_press_moved) {
			/* Quick click: pop + activate. */
			vile_anim_dock_pop(server, slot);
			vile_shell_dock_activate_app(server,
				server->dock_order[slot]);
		}
		/* A drag-away (moved >12px, no reorder): nothing. */
	}
}

static bool vile_shell_click(struct vile_server *server, double lx,
		double ly, uint32_t button) {
	if (!server->shell_created || server->locked) {
		return false;
	}
	int x = (int)lx, y = (int)ly;
	/* The unpin menu is topmost when open: its row unpins, a
	 * click elsewhere on it just closes it, a click outside
	 * closes it and falls through to the bar/dock/window
	 * handling below. */
	if (server->shell_unpin_open) {
		if (vile_box_contains(&server->shell_unpin_rect, x, y)) {
			if (vile_box_contains(&server->shell_unpin_row_rect,
					x, y)) {
				int slot = server->shell_unpin_slot;
				vile_shell_set_unpin(server, false, -1);
				vile_dock_unpin(server, slot);
				return true;
			}
			vile_shell_set_unpin(server, false, -1);
			return true;
		}
		vile_shell_set_unpin(server, false, -1);
	}
	/* The menu panel is topmost: its rows act first. */
	if (server->shell_menu_open) {
		if (vile_box_contains(&server->shell_menu_rect, x, y)) {
			if (vile_box_contains(&server->shell_menu_lock_rect, x, y)) {
				vile_shell_set_menu(server, false);
				wlr_log(WLR_INFO, "Vile: shell menu: Lock clicked");
				vile_lock_now(server);
				return true;
			}
			if (vile_box_contains(&server->shell_menu_settings_rect, x, y)) {
				vile_shell_set_menu(server, false);
				wlr_log(WLR_INFO, "Vile: shell menu: Settings clicked");
				vile_spawn_settings(server);
				return true;
			}
			if (vile_box_contains(&server->shell_menu_power_rect, x, y)) {
				vile_shell_set_menu(server, false);
				wlr_log(WLR_INFO, "Vile: shell menu: Power Off clicked");
				vile_spawn("systemctl poweroff");
				return true;
			}
			if (vile_box_contains(&server->shell_menu_logout_rect, x, y)) {
				/* Log Out: exit the compositor cleanly.
				 * From a LightDM session this returns to
				 * the greeter; from a text console it
				 * returns to the login prompt. */
				vile_shell_set_menu(server, false);
				wlr_log(WLR_INFO, "Vile: shell menu: Log Out clicked");
				wl_display_terminate(server->display);
				return true;
			}
			/* Any other row (Wi-Fi / Volume / Battery) is
			 * informational only — close the menu, change
			 * nothing. */
			vile_shell_set_menu(server, false);
			return true;
		}
		/* Click outside the menu: close it. A click on the status
		 * cluster just toggles it shut; anything else falls
		 * through to the bar/dock/window handling below. */
		if (vile_box_contains(&server->shell_status_rect, x, y)) {
			vile_shell_set_menu(server, false);
			return true;
		}
		vile_shell_set_menu(server, false);
	}
	/* Bottom bar (y = shell_h - 32 .. shell_h - 1). The left
	 * corner is empty now (no V tile, no wordmark) — the only
	 * action on the bar is the status pill, which opens the
	 * system menu. */
	if (y >= server->shell_h - VILE_BAR_H && y < server->shell_h) {
		if (vile_box_contains(&server->shell_status_rect, x, y)) {
			wlr_log(WLR_INFO, "Vile: shell: status cluster clicked -> menu");
			vile_shell_set_menu(server, true);
			return true;
		}
		/* Centre clock / empty bar space: no action yet — but the
		 * bar is shell territory, so do not clear window focus. */
		return true;
	}
	/* Dock (transparent, Nov 10, 2026 ~16:31): a press on an icon
	 * ARMS the slot — it does not activate yet. A quick release
	 * pops + activates; a 400ms hold starts a reorder drag;
	 * a right-click opens the Unpin menu. This is what keeps a
	 * hold-drag from fighting a click. */
	if (vile_box_contains(&server->shell_dock_rect, x, y)) {
		int slot = vile_dock_slot_at(server, x, y);
		if (slot >= 0) {
			if (button == BTN_RIGHT) {
				vile_shell_set_unpin(server, true, slot);
				return true;
			}
			server->dock_press_slot = slot;
			server->dock_press_ms = vile_anim_now_ms();
			server->dock_press_x = lx;
			server->dock_press_y = ly;
			server->dock_press_moved = false;
			server->dock_press_down = true;
			if (server->dock_hold_timer) {
				wl_event_source_timer_update(
					server->dock_hold_timer, 400);
			}
			return true;
		}
		/* Between icons: consume, no focus change. */
		return true;
	}
	return false;
}

/* --- minimal keyboard slice (lock screen only; Piece 3 extends it) --- */

static void vile_keyboard_modifiers(struct wl_listener *listener, void *data) {
	struct vile_keyboard *kb = wl_container_of(listener, kb, modifiers);
	wlr_seat_set_keyboard(kb->server->seat, kb->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(kb->server->seat,
		&kb->wlr_keyboard->modifiers);
}

static void vile_keyboard_key(struct wl_listener *listener, void *data) {
	struct vile_keyboard *kb = wl_container_of(listener, kb, key);
	struct vile_server *server = kb->server;
	struct wlr_keyboard_key_event *event = data;
	if (event->state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* Releases are never keybinds and never lock input — but
		 * they DO belong to the focused window when unlocked. */
		if (!server->locked) {
			wlr_seat_keyboard_notify_key(server->seat,
				event->time_msec, event->keycode, event->state);
		}
		return;
	}
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(kb->wlr_keyboard->xkb_state,
		keycode, &syms);
	if (server->locked) {
		/* Locked: keys go ONLY to the password handler, never to
		 * clients. (Same logic as before — the lock screen owns
		 * the keyboard until PAM says yes.) */
		bool changed = false;
		for (int i = 0; i < nsyms; i++) {
			xkb_keysym_t sym = syms[i];
			if (sym == XKB_KEY_BackSpace) {
				if (server->lock_password_len > 0) {
					server->lock_password_len--;
					server->lock_password[server->lock_password_len] = '\0';
					changed = true;
				}
			} else if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
				/* Enter: try the real unlock. This is the step that
				 * froze in v1 — here it always answers (PAM yes/no). */
				vile_lock_attempt(server);
				return;
			} else if (sym == XKB_KEY_Escape) {
				server->lock_password_len = 0;
				server->lock_password[0] = '\0';
				changed = true;
			} else if (sym >= 32 && sym < 127) {
				if (server->lock_password_len < sizeof(server->lock_password) - 1) {
					server->lock_password[server->lock_password_len++] = (char)sym;
					server->lock_password[server->lock_password_len] = '\0';
					changed = true;
				}
			}
		}
		if (changed) {
			/* Typing again clears the "wrong password" message. */
			server->lock_failed = false;
			vile_lock_render(server);
			vile_schedule_frames(server);
		}
		return;
	}

	/* Unlocked: Super (Windows key) + key = compositor keybind.
	 * Anything else is forwarded to the focused window. */
	uint32_t mods = wlr_keyboard_get_modifiers(kb->wlr_keyboard);
	if (mods & WLR_MODIFIER_LOGO) {
		for (int i = 0; i < nsyms; i++) {
			xkb_keysym_t sym = syms[i];
			if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
				wlr_log(WLR_INFO, "Vile: keybind Super+Enter -> open foot");
				vile_spawn("foot");
				return;
			}
			if (sym == XKB_KEY_q) {
				/* Super+Q: close the focused window. */
				if (server->focused_toplevel) {
					wlr_log(WLR_INFO, "Vile: keybind Super+Q -> close window");
					wlr_xdg_toplevel_send_close(
						server->focused_toplevel->xdg_toplevel);
				}
				return;
			}
			if (sym == XKB_KEY_Q) {
				/* Super+Shift+Q (uppercase Q): exit cleanly. */
				wlr_log(WLR_INFO, "Vile: keybind Super+Shift+Q -> exit");
				wl_display_terminate(server->display);
				return;
			}
			if (sym == XKB_KEY_l || sym == XKB_KEY_L) {
				wlr_log(WLR_INFO, "Vile: keybind Super+L -> lock now");
				vile_lock_now(server);
				return;
			}
			if (sym == XKB_KEY_m || sym == XKB_KEY_M) {
				wlr_log(WLR_INFO, "Vile: keybind Super+M -> exit");
				wl_display_terminate(server->display);
				return;
			}
			if (sym == XKB_KEY_j || sym == XKB_KEY_Left) {
				/* Piece 4: Super+J / Super+Left = focus previous
				 * window in tile order. */
				wlr_log(WLR_INFO, "Vile: keybind Super+J -> focus prev");
				vile_focus_direction(server, -1);
				return;
			}
			if (sym == XKB_KEY_k || sym == XKB_KEY_Right) {
				/* Piece 4: Super+K / Super+Right = focus next
				 * window in tile order. */
				wlr_log(WLR_INFO, "Vile: keybind Super+K -> focus next");
				vile_focus_direction(server, 1);
				return;
			}
			if (sym == XKB_KEY_f || sym == XKB_KEY_F) {
				/* Piece 4: Super+F = toggle fullscreen on the
				 * focused window. */
				wlr_log(WLR_INFO, "Vile: keybind Super+F -> fullscreen toggle");
				vile_toggle_fullscreen(server);
				return;
			}
			if (sym == XKB_KEY_Escape) {
				/* Piece 4 follow-up: Super+Escape = clear
				 * keyboard focus. A tiled desktop leaves
				 * almost no clickable wallpaper — only the
				 * 12px outer frame and the 4px wallpaper
				 * strips inside the gaps — so unfocus needs
				 * a keybind, not just a click on empty
				 * space. (Plain Escape without Super is
				 * still forwarded to the focused app.) */
				wlr_log(WLR_INFO, "Vile: keybind Super+Escape -> clear focus");
				vile_focus_toplevel(server, NULL);
				return;
			}
		}
	}

	/* Not a keybind: send the key to the focused window. */
	wlr_seat_set_keyboard(server->seat, kb->wlr_keyboard);
	wlr_seat_keyboard_notify_key(server->seat,
		event->time_msec, event->keycode, event->state);
}

static void vile_keyboard_destroy(struct wl_listener *listener, void *data) {
	struct vile_keyboard *kb = wl_container_of(listener, kb, destroy);
	wl_list_remove(&kb->modifiers.link);
	wl_list_remove(&kb->key.link);
	wl_list_remove(&kb->destroy.link);
	wl_list_remove(&kb->link);
	free(kb);
}

static void vile_keyboard_add(struct vile_server *server,
		struct wlr_keyboard *wlr_keyboard) {
	struct vile_keyboard *kb = calloc(1, sizeof(*kb));
	if (!kb) {
		return;
	}
	kb->server = server;
	kb->wlr_keyboard = wlr_keyboard;
	if (!wlr_keyboard->keymap) {
		/* Default layout ("us"). Virtual keyboards get their keymap
		 * from the client afterwards; wlroots updates it then. */
		struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, NULL,
			XKB_KEYMAP_COMPILE_NO_FLAGS);
		wlr_keyboard_set_keymap(wlr_keyboard, keymap);
		xkb_keymap_unref(keymap);
		xkb_context_unref(ctx);
	}
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);
	kb->modifiers.notify = vile_keyboard_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &kb->modifiers);
	kb->key.notify = vile_keyboard_key;
	wl_signal_add(&wlr_keyboard->events.key, &kb->key);
	/* Destroy: listen on the keyboard's own input-device destroy
	 * signal. This is REQUIRED, not optional: wlr_keyboard_finish()
	 * aborts (assert) if any key/modifier listeners are still
	 * attached when a keyboard goes away — that is exactly what
	 * happened the first time `wtype` exited. The destroy signal
	 * fires from inside wlr_input_device_finish(), before the
	 * asserts, so removing our listeners here keeps them happy.
	 * Works for physical and virtual keyboards alike, because every
	 * wlr_keyboard embeds its wlr_input_device as ->base. */
	kb->destroy.notify = vile_keyboard_destroy;
	wl_signal_add(&wlr_keyboard->base.events.destroy, &kb->destroy);
	wlr_seat_set_keyboard(server->seat, wlr_keyboard);
	wl_list_insert(&server->keyboards, &kb->link);
	/* If a window is already focused (it mapped before any keyboard
	 * existed — e.g. foot started before `wtype` connected), the new
	 * keyboard must re-enter it, or typed keys go nowhere. */
	if (!server->locked && server->focused_toplevel) {
		wlr_seat_keyboard_notify_enter(server->seat,
			server->focused_toplevel->xdg_toplevel->base->surface,
			wlr_keyboard->keycodes, wlr_keyboard->num_keycodes,
			&wlr_keyboard->modifiers);
	}
}

static void vile_server_new_input(struct wl_listener *listener, void *data) {
	struct vile_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
		vile_keyboard_add(server, wlr_keyboard_from_input_device(device));
	} else if (device->type == WLR_INPUT_DEVICE_POINTER) {
		/* All pointer motion goes through the cursor, which
		 * aggregates every pointer device into one position. */
		wlr_cursor_attach_input_device(server->cursor, device);
		wlr_log(WLR_INFO, "Vile: pointer device connected");
	}
}

static void vile_server_new_virtual_keyboard(struct wl_listener *listener,
		void *data) {
	struct vile_server *server =
		wl_container_of(listener, server, new_virtual_keyboard);
	struct wlr_virtual_keyboard_v1 *vkbd = data;
	vile_keyboard_add(server, &vkbd->keyboard);
	wlr_log(WLR_INFO, "Vile: virtual keyboard connected");
}

static int vile_quit_timer(void *data) {
	struct vile_server *server = data;
	wlr_log(WLR_INFO, "Vile: quit timer (%ds) fired, exiting",
		server->quit_after_sec);
	wl_display_terminate(server->display);
	return 0;
}

/* Vile cursor theme (0.5.6 — Adnan, Nov 10 ~22:52): the purple
 * arrow lives in the "vile" XCursor theme (assets/cursor-theme/vile,
 * installed to /usr/share/icons/vile). wlroots' manager falls back
 * to its built-in white arrow SILENTLY when a theme is missing, so
 * check the file exists first — that way the fallback is explicit
 * and logged, not silent. Search order mirrors wlroots' xcursor.c
 * (XCURSOR_PATH, then ~/.icons, ~/.local/share/icons,
 * /usr/share/icons). */
static bool vile_cursor_theme_available(void) {
	char path[1024];
	const char *xc = getenv("XCURSOR_PATH");
	if (xc && *xc) {
		/* wlroots' xcursor_library_path() returns XCURSOR_PATH
		 * ALONE when it is set — the default dirs are NOT
		 * searched. Mirror that exactly, or the fallback test
		 * (XCURSOR_PATH at an empty dir) would still find the
		 * /usr/share/icons theme and never fall back. */
		const char *p = xc;
		while (*p) {
			const char *colon = strchr(p, ':');
			size_t len = colon ? (size_t)(colon - p)
				: strlen(p);
			if (len > 0 && len < sizeof(path) - 32) {
				snprintf(path, sizeof(path),
					"%.*s/vile/cursors/default",
					(int)len, p);
				if (access(path, R_OK) == 0) {
					return true;
				}
			}
			if (!colon) {
				break;
			}
			p = colon + 1;
		}
		return false;
	}
	const char *home = getenv("HOME");
	const char *cands[] = {
		"%s/.icons/vile/cursors/default",
		"%s/.local/share/icons/vile/cursors/default",
		"/usr/share/icons/vile/cursors/default",
		NULL,
	};
	for (int i = 0; cands[i]; i++) {
		if (cands[i][0] == '/') {
			snprintf(path, sizeof(path), "%s", cands[i]);
		} else if (home) {
			snprintf(path, sizeof(path), cands[i], home);
		} else {
			continue;
		}
		if (access(path, R_OK) == 0) {
			return true;
		}
	}
	return false;
}

static int vile_signal_handler(int signal_number, void *data) {
	struct vile_server *server = data;
	wlr_log(WLR_INFO, "Vile: signal %d received, exiting", signal_number);
	wl_display_terminate(server->display);
	return 0;
}

static const char *vile_default_wallpaper(void) {
	static char path[1024];
	const char *candidates[] = {
		"../wallpapers/vile-purple-arch.jpg",
		"wallpapers/vile-purple-arch.jpg",
		NULL, /* $HOME/.config/vile/wallpapers/vile-purple-arch.jpg */
		NULL,
	};
	for (size_t i = 0; i < 2; i++) {
		if (access(candidates[i], R_OK) == 0) {
			return candidates[i];
		}
	}
	const char *home = getenv("HOME");
	if (home) {
		snprintf(path, sizeof(path),
			"%s/.config/vile/wallpapers/vile-purple-arch.jpg", home);
		if (access(path, R_OK) == 0) {
			return path;
		}
	}
	return "../wallpapers/vile-purple-arch.jpg";
}

/* Theme (0.5.4): ~/.config/vile/theme — plain text, "dark" or
 * "light". Missing/unreadable/invalid means dark. VILE_THEME
 * (test override) wins over the file, like VILE_WALLPAPER. */
static bool vile_theme_path(char *buf, size_t len) {
	const char *home = getenv("HOME");
	if (!home) {
		return false;
	}
	snprintf(buf, len, "%s/.config/vile/theme", home);
	return true;
}

static bool vile_read_theme(void) {
	const char *env = getenv("VILE_THEME");
	if (env && *env) {
		return strcmp(env, "light") == 0;
	}
	char path[1024];
	if (!vile_theme_path(path, sizeof(path))) {
		return false;
	}
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}
	char buf[32];
	bool light = false;
	if (fgets(buf, sizeof(buf), f)) {
		light = strncmp(buf, "light", 5) == 0;
	}
	fclose(f);
	return light;
}

/* Apply a theme live: palette flag, letterbox colour, logo variant,
 * lock background (if locked) + lock UI, shell re-render, borders.
 * Called from the 1s shell timer when the theme file changes, and
 * from main() at startup (flag only — nothing exists to re-render
 * yet, so the early call is a no-op beyond the flag). */
static void vile_apply_theme(struct vile_server *server, bool light) {
	bool changed = (light != server->theme_light);
	server->theme_light = light;
	const struct vile_palette *p = vile_pal(server->theme_light);
	if (server->bg_rect) {
		const float bg[4] = { p->bg[0], p->bg[1], p->bg[2], 1.0f };
		wlr_scene_rect_set_color(server->bg_rect, bg);
	}
	if (changed) {
		if (server->locked) {
			vile_lock_reload_bg(server);
			vile_lock_render(server);
		}
	}
	if (server->shell_created) {
		vile_shell_render_bar(server);
		vile_shell_render_dock(server);
		if (server->shell_menu_open) {
			vile_shell_render_menu(server);
		}
	}
	/* Borders: the unfocused dim changes with the theme. */
	vile_update_borders(server);
	vile_schedule_frames(server);
	wlr_log(WLR_INFO, "Vile: theme %s", light ? "light" : "dark");
}

/* Random default wallpaper (Nov 10, 2026 — Adnan: "make both
 * default, each user gets a different one randomly"). When no hook
 * file exists, pick randomly between the purple Arch and nebula
 * wallpapers and write the choice into the hook file immediately,
 * so the pick is stable for that install — random once per user,
 * not reshuffled every boot. If a hook already exists it is
 * respected, and VILE_WALLPAPER still wins over everything.
 * Called once in main(), before the first vile_wallpaper_path(). */
static void vile_ensure_default_wallpaper(void) {
	const char *env = getenv("VILE_WALLPAPER");
	if (env && *env) {
		return;
	}
	const char *home = getenv("HOME");
	if (!home) {
		return;
	}
	char hook[1024];
	snprintf(hook, sizeof(hook), "%s/.config/vile/wallpaper", home);
	FILE *f = fopen(hook, "r");
	if (f) {
		/* A hook already exists — respect it, even if the path
		 * inside is stale (vile_wallpaper_path falls back then). */
		fclose(f);
		return;
	}
	const char *names[] = { "vile-purple-arch.jpg", "vile-nebula.jpg" };
	srand((unsigned)(time(NULL) ^ (getpid() << 16)));
	int pick = rand() % 2;
	/* Resolve the pick (then the other one) through the same
	 * candidate locations as vile_default_wallpaper. */
	const char *dirs[] = { "../wallpapers/", "wallpapers/", NULL };
	char found[1024] = "";
	for (int attempt = 0; attempt < 2 && !found[0]; attempt++) {
		const char *name = names[(pick + attempt) % 2];
		for (int i = 0; dirs[i]; i++) {
			snprintf(found, sizeof(found), "%s%s", dirs[i], name);
			if (access(found, R_OK) == 0) {
				break;
			}
			found[0] = '\0';
		}
		if (!found[0]) {
			snprintf(found, sizeof(found),
				"%s/.config/vile/wallpapers/%s", home, name);
			if (access(found, R_OK) != 0) {
				found[0] = '\0';
			}
		}
	}
	if (!found[0]) {
		wlr_log(WLR_ERROR,
			"Vile: no default wallpaper found, not writing hook");
		return;
	}
	/* Absolute path, so the hook stays valid whatever the CWD is
	 * when the compositor next starts. */
	char abs[1024];
	if (!realpath(found, abs)) {
		snprintf(abs, sizeof(abs), "%s", found);
	}
	/* mkdir -p ~/.config/vile */
	char dir[1024];
	snprintf(dir, sizeof(dir), "%s/.config", home);
	mkdir(dir, 0755);
	snprintf(dir, sizeof(dir), "%s/.config/vile", home);
	mkdir(dir, 0755);
	FILE *out = fopen(hook, "w");
	if (!out) {
		wlr_log(WLR_ERROR, "Vile: cannot write wallpaper hook '%s'",
			hook);
		return;
	}
	fprintf(out, "%s\n", abs);
	fclose(out);
	wlr_log(WLR_INFO,
		"Vile: first run — random default wallpaper '%s' written to hook",
		abs);
}

/* Piece 4: wallpaper hook for Settings. Priority:
 *   1. VILE_WALLPAPER env (test override, wins over everything)
 *   2. ~/.config/vile/wallpaper — plain text file, one path per
 *      file (absolute path; trailing newline/space trimmed).
 *      Settings' wallpaper picker later just writes this file.
 *      A missing/unreadable path falls through to the default.
 *   3. The built-in default (purple Arch). */
static const char *vile_wallpaper_path(void) {
	static char path[1024];
	const char *env = getenv("VILE_WALLPAPER");
	if (env && *env) {
		return env;
	}
	const char *home = getenv("HOME");
	if (home) {
		char hook[1024];
		snprintf(hook, sizeof(hook), "%s/.config/vile/wallpaper", home);
		FILE *f = fopen(hook, "r");
		if (f) {
			if (fgets(path, sizeof(path), f)) {
				size_t n = strlen(path);
				while (n > 0 && (path[n - 1] == '\n' ||
						path[n - 1] == '\r' ||
						path[n - 1] == ' ' ||
						path[n - 1] == '\t')) {
					path[--n] = '\0';
				}
				size_t start = 0;
				while (path[start] == ' ' || path[start] == '\t') {
					start++;
				}
				fclose(f);
				if (path[start] && access(path + start, R_OK) == 0) {
					wlr_log(WLR_INFO, "Vile: wallpaper hook file -> '%s'",
						path + start);
					return path + start;
				}
				wlr_log(WLR_ERROR, "Vile: wallpaper hook '%s' unreadable, using default",
					path + start);
			} else {
				fclose(f);
			}
		}
	}
	return vile_default_wallpaper();
}

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_INFO, NULL);

	/* Piece 2: -s <command> runs once the compositor is up
	 * (e.g. -s foot). VILE_STARTUP does the same thing. */
	const char *startup_cmd = getenv("VILE_STARTUP");
	/* Lock screen: -l / --lock, or VILE_LOCK=1. Starts locked. */
	const char *lock_env = getenv("VILE_LOCK");
	bool locked = lock_env && strcmp(lock_env, "0") != 0;
	int c;
	while ((c = getopt(argc, argv, "s:hl")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		case 'l':
			locked = true;
			break;
		case 'h':
		default:
			printf("Usage: %s [-s startup command] [-l lock on start]\n", argv[0]);
			return 0;
		}
	}

	/* Random default wallpaper: if no hook exists yet, pick one
	 * (purple Arch or nebula) and write it to the hook now, so
	 * the choice is stable for this install. */
	vile_ensure_default_wallpaper();
	const char *wallpaper = vile_wallpaper_path();
	const char *quit_env = getenv("VILE_QUIT_AFTER_SEC");
	int quit_after_sec = quit_env ? atoi(quit_env) : 0;

	struct vile_server server = { 0 };
	server.quit_after_sec = quit_after_sec;
	server.socket_file = getenv("VILE_SOCKET_FILE");
	server.startup_cmd = startup_cmd;
	server.locked = locked;
	/* Who the lock screen checks the password for: the user running
	 * the compositor, unless VILE_LOCK_USER overrides (test only). */
	const char *lock_user = getenv("VILE_LOCK_USER");
	if (!lock_user) {
		struct passwd *pw = getpwuid(getuid());
		lock_user = pw ? pw->pw_name : "root";
	}
	server.lock_user = lock_user;
	/* Theme (0.5.4): read before the wallpaper/logo/lock load, so
	 * every first frame already uses the right palette. theme_mtime
	 * seeds the watcher so the first timer tick doesn't re-apply. */
	server.theme_light = vile_read_theme();
	{
		char tpath[1024];
		if (vile_theme_path(tpath, sizeof(tpath))) {
			struct stat tst;
			server.theme_mtime = (stat(tpath, &tst) == 0) ?
				tst.st_mtime : 0;
		}
	}
	wl_list_init(&server.outputs);
	wl_list_init(&server.toplevels);
	wl_list_init(&server.keyboards);

	server.display = wl_display_create();
	if (!server.display) {
		fprintf(stderr, "Vile: wl_display_create failed\n");
		return 1;
	}

	/* Backend: DRM on hardware, headless/X11 when WLR_BACKENDS says so. */
	server.backend = wlr_backend_autocreate(
		wl_display_get_event_loop(server.display), NULL);
	if (!server.backend) {
		fprintf(stderr, "Vile: failed to create backend\n");
		return 1;
	}

	server.renderer = wlr_renderer_autocreate(server.backend);
	if (!server.renderer) {
		fprintf(stderr, "Vile: failed to create renderer\n");
		return 1;
	}
	wlr_renderer_init_wl_display(server.renderer, server.display);

	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	if (!server.allocator) {
		fprintf(stderr, "Vile: failed to create allocator\n");
		return 1;
	}

	/* Core Wayland globals clients need. */
	wlr_compositor_create(server.display, 6, server.renderer);
	wlr_subcompositor_create(server.display);
	wlr_data_device_manager_create(server.display);
	/* Minimal seat (see struct comment): without this global, clients
	 * like foot will not even try to open a window. No input handling
	 * yet — that is Piece 3. */
	server.seat = wlr_seat_create(server.display, "seat0");
	wlr_seat_set_capabilities(server.seat,
		WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);
	/* Screencopy: lets `grim` capture a real screenshot of our output. */
	wlr_screencopy_manager_v1_create(server.display);

	/* Piece 2: xdg-shell — the protocol application windows use. */
	server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
	server.new_xdg_toplevel.notify = vile_server_new_xdg_toplevel;
	wl_signal_add(&server.xdg_shell->events.new_toplevel,
		&server.new_xdg_toplevel);
	server.new_xdg_popup.notify = vile_server_new_xdg_popup;
	wl_signal_add(&server.xdg_shell->events.new_popup,
		&server.new_xdg_popup);

	server.output_layout = wlr_output_layout_create(server.display);
	/* xdg-output: lets clients (grim, waybar, ...) learn output names,
	 * positions and sizes. Without this, grim cannot target an output. */
	wlr_xdg_output_manager_v1_create(server.display, server.output_layout);

	server.new_output.notify = vile_server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);
	server.new_input.notify = vile_server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);

	/* Virtual keyboards (e.g. `wtype`) — the lock screen test types
	 * through this, and it is also how remote/automation input works. */
	server.vkbd_manager = wlr_virtual_keyboard_manager_v1_create(server.display);
	server.new_virtual_keyboard.notify = vile_server_new_virtual_keyboard;
	wl_signal_add(&server.vkbd_manager->events.new_virtual_keyboard,
		&server.new_virtual_keyboard);

	/* Virtual pointers (test clients / automation) — a normal pointer
	 * device as far as the cursor is concerned. */
	server.vptr_manager = wlr_virtual_pointer_manager_v1_create(server.display);
	server.new_virtual_pointer.notify = vile_server_new_virtual_pointer;
	wl_signal_add(&server.vptr_manager->events.new_virtual_pointer,
		&server.new_virtual_pointer);

	/* Piece 3: cursor. wlr_cursor only tracks the position — it does
	 * not move by itself. We attach the output layout (so motion is
	 * clamped to the screen), load the "default" arrow image, and
	 * listen for the aggregate motion/button/axis/frame events it
	 * re-emits from every pointer device.
	 * Vile cursor (0.5.6): the purple arrow theme, falling back to
	 * the system default when the theme files are missing. */
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
	if (vile_cursor_theme_available()) {
		server.cursor_mgr = wlr_xcursor_manager_create("vile", 24);
		wlr_log(WLR_INFO,
			"Vile: cursor theme 'vile' (purple arrow)");
	} else {
		wlr_log(WLR_ERROR, "Vile: cursor theme 'vile' not found"
			" — falling back to system default cursor");
		server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	}
	if (!wlr_xcursor_manager_load(server.cursor_mgr, 1)) {
		wlr_log(WLR_ERROR, "Vile: xcursor theme failed to load");
	}
	wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");
	/* Start in the middle of the screen. */
	wlr_cursor_warp(server.cursor, NULL, VILE_OUT_W / 2, VILE_OUT_H / 2);
	server.cursor_motion.notify = vile_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = vile_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute,
		&server.cursor_motion_absolute);
	server.cursor_button.notify = vile_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = vile_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = vile_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	/* A client may ask to set its own cursor image (foot does this).
	 * Only honour it if that client actually has pointer focus. */
	server.request_cursor.notify = vile_seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
		&server.request_cursor);
	server.request_set_selection.notify = vile_seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection,
		&server.request_set_selection);

	server.scene = wlr_scene_create();
	if (!server.scene) {
		fprintf(stderr, "Vile: wlr_scene_create failed\n");
		return 1;
	}
	server.scene_layout =
		wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/* Wallpaper FIRST, before any window exists: the screen is never black. */
	if (!vile_load_wallpaper(&server, wallpaper)) {
		fprintf(stderr, "Vile: wallpaper failed, refusing to run black\n");
		return 1;
	}
	snprintf(server.wallpaper_current, sizeof(server.wallpaper_current),
		"%s", wallpaper);
	/* No logo overlay (Nov 10, 2026 ~16:31 — Adnan: no logo
	 * stamped on any wallpaper; the overlay draw was deleted). */
	/* Live-reload baseline: remember the hook file's mtime, so the
	 * 1-second watcher only fires when Settings actually picks a
	 * different wallpaper. */
	char hook_path[1024];
	if (vile_wallpaper_hook_path(hook_path, sizeof(hook_path))) {
		struct stat st;
		server.wallpaper_hook_mtime =
			stat(hook_path, &st) == 0 ? st.st_mtime : 0;
	}

	/* Lock screen on top of everything, if requested. */
	if (server.locked) {
		vile_lock_create(&server);
	}

	const char *socket = wl_display_add_socket_auto(server.display);
	if (!socket) {
		fprintf(stderr, "Vile: wl_display_add_socket_auto failed\n");
		return 1;
	}
	if (server.socket_file) {
		FILE *f = fopen(server.socket_file, "w");
		if (f) {
			fprintf(f, "%s\n", socket);
			fclose(f);
		}
	}

	if (!wlr_backend_start(server.backend)) {
		fprintf(stderr, "Vile: failed to start backend\n");
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, 1);
	wlr_log(WLR_INFO, "Vile compositor running on WAYLAND_DISPLAY=%s", socket);
	wlr_log(WLR_INFO, "Vile: wallpaper '%s' on screen, waiting for frames",
		wallpaper);

	/* Piece 7: first run — the Setup wizard opens FIRST, before the
	 * bare desktop, and the startup command is skipped so the
	 * wizard is the only window. Once the marker exists, boot goes
	 * straight to the desktop + startup command as before. */
	if (vile_setup_needed()) {
		vile_spawn_setup();
	} else if (server.startup_cmd) {
		vile_spawn(server.startup_cmd);
	}

	struct wl_event_loop *loop = wl_display_get_event_loop(server.display);
	if (quit_after_sec > 0) {
		server.quit_timer = wl_event_loop_add_timer(loop,
			vile_quit_timer, &server);
		wl_event_source_timer_update(server.quit_timer,
			quit_after_sec * 1000);
	}
	/* Piece 5: shell clock timer — re-renders the bar + desktop
	 * clock when the minute changes, and the dock dots when the
	 * running-app set changes. */
	server.shell_timer = wl_event_loop_add_timer(loop,
		vile_shell_timer, &server);
	wl_event_source_timer_update(server.shell_timer, 1000);
	/* Dock hold timer (Nov 10, 2026 ~16:31): a press held 400ms
	 * on a dock icon becomes a reorder drag. Created disarmed
	 * (no update call) — a dock press arms it for 400ms. */
	server.dock_hold_timer = wl_event_loop_add_timer(loop,
		vile_dock_hold_timer_cb, &server);
	wl_event_loop_add_signal(loop, SIGINT, vile_signal_handler, &server);
	wl_event_loop_add_signal(loop, SIGTERM, vile_signal_handler, &server);

	wl_display_run(server.display);

	wl_display_destroy_clients(server.display);
	wl_list_remove(&server.new_output.link);
	wl_list_remove(&server.new_input.link);
	wl_list_remove(&server.new_xdg_toplevel.link);
	wl_list_remove(&server.new_xdg_popup.link);
	wl_list_remove(&server.new_virtual_keyboard.link);
	wl_list_remove(&server.new_virtual_pointer.link);
	wl_list_remove(&server.cursor_motion.link);
	wl_list_remove(&server.cursor_motion_absolute.link);
	wl_list_remove(&server.cursor_button.link);
	wl_list_remove(&server.cursor_axis.link);
	wl_list_remove(&server.cursor_frame.link);
	wl_list_remove(&server.request_cursor.link);
	wl_list_remove(&server.request_set_selection.link);
	if (server.cursor_mgr) {
		wlr_xcursor_manager_destroy(server.cursor_mgr);
	}
	if (server.cursor) {
		wlr_cursor_destroy(server.cursor);
	}
	if (server.clock_timer) {
		wl_event_source_remove(server.clock_timer);
	}
	/* Piece 5: shell timer + buffers + foot icon surface. */
	if (server.shell_timer) {
		wl_event_source_remove(server.shell_timer);
	}
	if (server.shell_foot_icon) {
		cairo_surface_destroy(server.shell_foot_icon);
	}
	if (server.shell_files_icon) {
		cairo_surface_destroy(server.shell_files_icon);
	}
	/* Launch-jump spinner frames (0.5.6). */
	for (int i = 0; i < server.shell_spin_n; i++) {
		cairo_surface_destroy(server.shell_spin_frames[i]);
	}
	if (server.shell_bar_buffer) {
		wlr_buffer_drop(server.shell_bar_buffer);
	}
	if (server.shell_bar_frost) {
		cairo_surface_destroy(server.shell_bar_frost);
	}
	if (server.shell_dock_buffer) {
		wlr_buffer_drop(server.shell_dock_buffer);
	}
	if (server.shell_menu_buffer) {
		wlr_buffer_drop(server.shell_menu_buffer);
	}
	if (server.lock_ui_buffer) {
		wlr_buffer_drop(server.lock_ui_buffer);
	}
	if (server.profile_pixels) {
		stbi_image_free(server.profile_pixels);
	}
	wlr_scene_node_destroy(&server.scene->tree.node);
	if (server.wallpaper_buffer) {
		wlr_buffer_drop(server.wallpaper_buffer);
	}
	if (server.wallpaper_texture) {
		wlr_texture_destroy(server.wallpaper_texture);
	}
	if (server.seat) {
		wlr_seat_destroy(server.seat);
	}
	wlr_allocator_destroy(server.allocator);
	wlr_renderer_destroy(server.renderer);
	wlr_backend_destroy(server.backend);
	wl_display_destroy(server.display);
	return 0;
}
