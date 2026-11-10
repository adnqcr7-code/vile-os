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
static bool vile_load_logo(struct vile_server *server);
static void vile_fit_logo(struct vile_server *server, int out_w, int out_h);
static void vile_tile(struct vile_server *server);
static void vile_update_borders(struct vile_server *server);
/* Piece 5 — Vile look (bottom bar, dock). The huge desktop clock
 * was removed Nov 10, 2026 — the bar centre carries the small
 * clock now; the lock clock is separate and untouched. */
static void vile_shell_create(struct vile_server *server, int out_w, int out_h);
static void vile_shell_fit(struct vile_server *server, int out_w, int out_h);
static void vile_shell_raise(struct vile_server *server);
static void vile_shell_update(struct vile_server *server);
static bool vile_shell_click(struct vile_server *server, double lx, double ly);
static void vile_shell_close_menu(struct vile_server *server);
static void vile_shell_set_menu(struct vile_server *server, bool open);
static int vile_shell_timer(void *data);
static void vile_shell_render_dock(struct vile_server *server);
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

/* Vile design tokens (same as the rest of Vile OS). */
#define VILE_BG_R 0.059f   /* #0F0B1A */
#define VILE_BG_G 0.043f
#define VILE_BG_B 0.102f
#define VILE_OUT_W 1920
#define VILE_OUT_H 1080
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
	cairo_surface_t *shell_foot_icon;
	cairo_surface_t *shell_files_icon;
	cairo_surface_t *shell_vile_icon;
	int shell_dock_x, shell_dock_y;
	int shell_menu_x, shell_menu_y;
	bool shell_menu_open;
	int shell_dock_foot_n, shell_dock_set_n, shell_dock_screenfit_n,
		shell_dock_files_n;
	struct vile_toplevel *shell_dock_focused;
	/* Click targets on the shell (output coords). */
	struct wlr_box shell_logo_rect;
	struct wlr_box shell_status_rect;
	struct wlr_box shell_dock_rect;
	struct wlr_box shell_dock_foot_rect;
	struct wlr_box shell_dock_settings_rect;
	struct wlr_box shell_dock_screenfit_rect;
	struct wlr_box shell_dock_files_rect;
	struct wlr_box shell_menu_rect;
	struct wlr_box shell_menu_lock_rect;
	struct wlr_box shell_menu_settings_rect;
	struct wlr_box shell_menu_power_rect;

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
	int dock_hover; /* -1 none, 0 foot, 1 Settings, 2 ScreenFit, 3 Files */
	float dock_icon_scale[4]; /* current per-icon scale (1.0 normal) */
	float dock_rendered_scale[4]; /* scale last painted into the dock buffer */
	float dock_icon_from[4];
	float dock_icon_to[4];
	int64_t dock_icon_ms[4]; /* when each icon's scale anim started */
	int dock_pop_icon; /* -1 none */
	int64_t dock_pop_ms;
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
	/* Arch logo overlay (Nov 10, 2026 — Adnan: the logo must stay
	 * when the wallpaper changes): the transparent Arch logo drawn
	 * centred over the wallpaper, under windows and under the
	 * shell, so it reads as part of the desktop. The lock screen
	 * is untouched — its blurred Arch wallpaper already has the
	 * logo, and the lock tree covers this node while locked. */
	struct wlr_scene_buffer *logo_node;
	struct wlr_buffer *logo_buffer;
	/* Live reload (UI polish): the 1-second shell timer watches
	 * ~/.config/vile/wallpaper; when its mtime changes, the
	 * compositor swaps the wallpaper without a restart.
	 * wallpaper_current is the resolved path now on screen (the
	 * VILE_WALLPAPER env override, the hook file, or the default);
	 * wallpaper_hook_mtime is the hook file's mtime at last check
	 * (0 = no hook file). */
	time_t wallpaper_hook_mtime;
	char wallpaper_current[1024];

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
	const float bg[4] = { VILE_BG_R, VILE_BG_G, VILE_BG_B, 1.0f };
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
	/* The logo overlay tracks the same output size. */
	vile_fit_logo(server, out_w, out_h);
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
	/* Piece 4: tiling — border rect behind the window (a scene-root
	 * rect, kept below the window), creation sequence (stable tile
	 * order), fullscreen state, and the last tile rect given. */
	struct wlr_scene_rect *border;
	uint64_t seq;
	bool fullscreen;
	int tile_x, tile_y, tile_w, tile_h;
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

static int vile_tile_count(struct vile_server *server) {
	int n = 0;
	struct vile_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		n++;
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
	const float dim[4] = { 0.25f, 0.24f, 0.33f, 1.0f };
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
		bool show = (server->fullscreen_toplevel == NULL);
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
	for (int i = 0; i < 4; i++) {
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
		const float dim0[4] = { 0.04f, 0.03f, 0.07f, 0.0f };
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
	if (server->logo_node) {
		wlr_scene_buffer_set_opacity(server->logo_node, 0.0f);
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
			if (server->logo_node) {
				wlr_scene_buffer_set_opacity(
					server->logo_node, 0.9f);
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
			if (server->logo_node) {
				wlr_scene_buffer_set_opacity(
					server->logo_node, 0.9f * e);
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
	for (int i = 0; i < 4; i++) {
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
		float eff[4];
		for (int i = 0; i < 4; i++) {
			eff[i] = server->dock_icon_scale[i] *
				vile_dock_pop_scale(server, i);
		}
		bool changed = false;
		for (int i = 0; i < 4; i++) {
			if (fabsf(eff[i] - server->dock_rendered_scale[i])
					> 0.002f) {
				changed = true;
				break;
			}
		}
		if (changed) {
			for (int i = 0; i < 4; i++) {
				server->dock_rendered_scale[i] = eff[i];
			}
			vile_shell_render_dock(server);
		}
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
				const float dim[4] = { 0.04f, 0.03f, 0.07f,
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
				const float dim[4] = { 0.04f, 0.03f, 0.07f,
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
	for (i = 0; i < n; i++) {
		t = arr[i];
		int x, y, w, h;
		vile_tile_rect(i, n, out_w, out_h, &x, &y, &w, &h);
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
			i + 1, n,
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
		if (vile_box_contains(&server->shell_dock_foot_rect, cx, cy)) {
			hover = 0;
		} else if (vile_box_contains(&server->shell_dock_settings_rect,
				cx, cy)) {
			hover = 1;
		} else if (vile_box_contains(&server->shell_dock_screenfit_rect,
				cx, cy)) {
			hover = 2;
		} else if (vile_box_contains(&server->shell_dock_files_rect,
				cx, cy)) {
			hover = 3;
		}
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
	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		/* Piece 5: the bar / dock / system menu eat the click
		 * first — they are shell territory, not windows. A click
		 * there must not clear window focus either. */
		if (vile_shell_click(server, server->cursor->x,
				server->cursor->y)) {
			return;
		}
		/* Click focuses (and raises) the window under the cursor;
		 * click on empty space clears keyboard focus. */
		double sx, sy;
		struct wlr_surface *surface = NULL;
		struct vile_toplevel *toplevel = vile_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
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
		int n = vile_tile_count(server) + 1;
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
	wlr_log(WLR_INFO, "Vile: window destroyed");
	free(toplevel);
}

static void vile_toplevel_request_maximize(struct wl_listener *listener, void *data) {
	/* Piece 2 has no maximize support yet — but the protocol still
	 * needs a configure reply or the client will wait forever. */
	struct vile_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
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
		const float dim[4] = { 0.25f, 0.24f, 0.33f, 1.0f };
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

/* Arch logo overlay (Nov 10, 2026 — Adnan: "when you changed the
 * bg wallpaper the logo is gone, not like in Arch"). The transparent
 * Arch logo is drawn centred over the wallpaper, under windows and
 * under the shell, so it stays visible no matter which wallpaper is
 * set — like the logo baked into the Arch wallpaper, except it no
 * longer disappears when the wallpaper changes. The node is created
 * in main() right after the wallpaper node and before any window
 * exists, so scene order puts it between the wallpaper and the
 * windows. Failure is non-fatal: the desktop works without it.
 * (Adnan briefly asked for the Vile V here instead, then reversed
 * it: "Nv leav it as its" — Arch overlay stands; the ribbon V stays
 * in the bar tile, dock icon, and wizard welcome only.) */
static bool vile_load_logo(struct vile_server *server) {
	const char *paths[] = {
		"../assets/arch-logo.png",
		"assets/arch-logo.png",
		NULL,
	};
	cairo_surface_t *img = NULL;
	for (int i = 0; paths[i]; i++) {
		img = cairo_image_surface_create_from_png(paths[i]);
		if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS) {
			wlr_log(WLR_INFO,
				"Vile: Arch logo overlay loaded from '%s'",
				paths[i]);
			break;
		}
		cairo_surface_destroy(img);
		img = NULL;
	}
	if (!img) {
		const char *home = getenv("HOME");
		if (home) {
			char p[1024];
			const char *homes[] = {
				"%s/vile-os/assets/arch-logo.png",
				"%s/workspace/vile-os/assets/arch-logo.png",
				NULL,
			};
			for (int i = 0; homes[i]; i++) {
				snprintf(p, sizeof(p), homes[i], home);
				img = cairo_image_surface_create_from_png(p);
				if (cairo_surface_status(img) ==
						CAIRO_STATUS_SUCCESS) {
					wlr_log(WLR_INFO,
						"Vile: Arch logo overlay loaded from '%s'",
						p);
					break;
				}
				cairo_surface_destroy(img);
				img = NULL;
			}
		}
	}
	if (!img) {
		wlr_log(WLR_ERROR,
			"Vile: arch-logo.png not found — desktop has no logo overlay");
		return false;
	}
	int w = cairo_image_surface_get_width(img);
	int h = cairo_image_surface_get_height(img);
	struct wlr_buffer *buf = vile_paint_cairo(server, img, w, h);
	cairo_surface_destroy(img);
	if (!buf) {
		wlr_log(WLR_ERROR, "Vile: logo buffer paint failed");
		return false;
	}
	server->logo_buffer = buf;
	server->logo_node = wlr_scene_buffer_create(&server->scene->tree, buf);
	if (!server->logo_node) {
		wlr_log(WLR_ERROR, "Vile: logo scene node failed");
		wlr_buffer_drop(buf);
		server->logo_buffer = NULL;
		return false;
	}
	/* 0.9 opacity: the logo reads as part of the wallpaper, not a
	 * sticker on top of it. The startup animation fades it in
	 * from 0 alongside the wallpaper. */
	wlr_scene_buffer_set_opacity(server->logo_node, 0.9f);
	wlr_log(WLR_INFO, "Vile: Arch logo overlay node in scene (%dx%d)", w, h);
	return true;
}

static void vile_fit_logo(struct vile_server *server, int out_w, int out_h) {
	if (!server->logo_node) {
		return;
	}
	/* ~45% of output height, square — resolution-aware, the same
	 * scale-with-height rule as the desktop clock. */
	int size = (int)(0.45 * out_h + 0.5);
	if (size < 1) {
		size = 1;
	}
	wlr_scene_buffer_set_dest_size(server->logo_node, size, size);
	wlr_scene_node_set_position(&server->logo_node->node,
		(out_w - size) / 2, (out_h - size) / 2);
}

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
	strftime(time_str, sizeof(time_str), "%H:%M", &tm);
	strftime(date_str, sizeof(date_str), "%A, %B %d", &tm);

	/* HUGE clock, centred, then the date below it. */
	vile_draw_text_centre(cr, "Inter Light 150", 0.96, 0.95, 1.0,
		W / 2.0, H * 0.40, time_str);
	vile_draw_text_centre(cr, "Inter 30", 0.79, 0.75, 0.95,
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
	vile_draw_text_at(cr, "Inter Bold 34", 0.96, 0.95, 1.0,
		tx, card_y + 8, vile_lock_display_name(server));
	/* Padlock glyph + LOCKED label below the username. */
	vile_draw_lock_glyph(cr, tx, card_y + 66, 22, 0.72, 0.69, 0.82);
	vile_draw_text_at(cr, "Inter Bold 17", 0.72, 0.69, 0.82,
		tx + 30, card_y + 64, "LOCKED");
	/* Password field to the right of the avatar, below the name. */
	double fx = tx, fy = card_y + 108, fw = 360, fh = 60;
	cairo_set_source_rgba(cr, 0.04, 0.03, 0.08, 0.65);
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
		vile_draw_text_centre(cr, "Inter 24", 1.0, 1.0, 1.0,
			fx + fw / 2, fy + fh / 2, server->lock_password);
	} else {
		for (size_t i = 0; i < n; i++) {
			double cx = fx + fw / 2 + (i - (n - 1) / 2.0) * 28.0;
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_arc(cr, cx, fy + fh / 2.0, 8, 0, 2 * M_PI);
			cairo_fill(cr);
		}
	}
	/* Hint / error line, centred under the card. */
	if (server->lock_failed) {
		vile_draw_text_centre(cr, "Inter 22", 1.0, 0.48, 0.48,
			W / 2.0, card_y + 208, "Wrong password — try again");
	} else if (n == 0) {
		vile_draw_text_centre(cr, "Inter 20", 0.54, 0.50, 0.66,
			W / 2.0, card_y + 208, "Type your password and press Enter");
	}

	/* --- Settings gear, bottom-right corner --- */
	double gx = W - 76, gy = H - 76, gs = 52;
	server->lock_gear_rect = (struct wlr_box){
		.x = (int)gx, .y = (int)gy, .width = (int)gs, .height = (int)gs };
	cairo_set_source_rgba(cr, 0.04, 0.03, 0.08, 0.60);
	cairo_arc(cr, gx + gs / 2, gy + gs / 2, gs / 2, 0, 2 * M_PI);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.35);
	cairo_set_line_width(cr, 1.5);
	cairo_arc(cr, gx + gs / 2, gy + gs / 2, gs / 2, 0, 2 * M_PI);
	cairo_stroke(cr);
	vile_draw_gear(cr, gx + gs / 2, gy + gs / 2, 13, 0.9, 0.9, 0.95);

	/* --- Settings panel (only when the gear opened it) --- */
	server->lock_toggle_rect = (struct wlr_box){ 0 };
	server->lock_reboot_rect = (struct wlr_box){ 0 };
	server->lock_poweroff_rect = (struct wlr_box){ 0 };
	server->lock_close_rect = (struct wlr_box){ 0 };
	if (server->lock_settings_open) {
		double px = W - 24 - 320, py = H * 0.648, pw = 320, ph = 280;
		cairo_set_source_rgba(cr, 0.04, 0.03, 0.08, 0.72);
		vile_rounded_rect(cr, px, py, pw, ph, 16);
		cairo_fill(cr);
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
		cairo_set_line_width(cr, 1.5);
		vile_rounded_rect(cr, px, py, pw, ph, 16);
		cairo_stroke(cr);
		double ix = px + 24;
		vile_draw_text_at(cr, "Inter Bold 13", 0.55, 0.52, 0.62,
			ix, py + 18, "SETTINGS");
		vile_draw_text_at(cr, "Inter 26", 0.85, 0.84, 0.9,
			px + pw - 44, py + 10, "×");
		server->lock_close_rect = (struct wlr_box){
			.x = (int)(px + pw - 48), .y = (int)(py + 8),
			.width = 36, .height = 36 };
		vile_draw_text_at(cr, "Inter 20", 0.92, 0.91, 0.95,
			ix, py + 58, "Hide password");
		vile_draw_toggle(cr, px + pw - 24 - 64, py + 56, 64, 30,
			!server->lock_show_password);
		server->lock_toggle_rect = (struct wlr_box){
			.x = (int)(px + pw - 24 - 64), .y = (int)(py + 56),
			.width = 64, .height = 30 };
		vile_draw_text_at(cr, "Inter 18", 0.55, 0.52, 0.62,
			ix, py + 106, "Reveal delay");
		vile_draw_text_at(cr, "Inter 18", 0.55, 0.52, 0.62,
			px + pw - 24 - vile_text_width(cr, "Inter 18", "300 ms"),
			py + 106, "300 ms");
		vile_draw_text_at(cr, "Inter Bold 13", 0.55, 0.52, 0.62,
			ix, py + 150, "SYSTEM");
		vile_draw_text_at(cr, "Inter 20", 0.92, 0.91, 0.95,
			ix, py + 182, "Reboot");
		server->lock_reboot_rect = (struct wlr_box){
			.x = (int)px, .y = (int)(py + 176),
			.width = (int)pw, .height = 36 };
		vile_draw_text_at(cr, "Inter 20", 0.92, 0.91, 0.95,
			ix, py + 222, "Power Off");
		server->lock_poweroff_rect = (struct wlr_box){
			.x = (int)px, .y = (int)(py + 216),
			.width = (int)pw, .height = 36 };
	}

	/* --- Minimal bottom-centre status --- */
	vile_draw_text_centre(cr, "Inter 16", 0.60, 0.58, 0.70,
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
	 * Falls back to the normal wallpaper buffer if the blur file
	 * is missing. */
	int bg_w = 0, bg_h = 0;
	server->lock_bg_buffer = vile_paint_file_to_buffer(server,
		"../wallpapers/vile-purple-arch-blur.jpg", &bg_w, &bg_h);
	if (!server->lock_bg_buffer) {
		server->lock_bg_buffer = vile_paint_file_to_buffer(server,
			"wallpapers/vile-purple-arch-blur.jpg", &bg_w, &bg_h);
	}
	if (server->lock_bg_buffer) {
		server->lock_bg_node = wlr_scene_buffer_create(
			server->lock_tree, server->lock_bg_buffer);
	}
	if (!server->lock_bg_node && server->wallpaper_buffer) {
		server->lock_bg_node = wlr_scene_buffer_create(
			server->lock_tree, server->wallpaper_buffer);
	}
	/* A light extra dim so the white clock reads clearly on the
	 * already-darkened blur. */
	const float dim[4] = { 0.04f, 0.03f, 0.07f, 0.30f };
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
#define VILE_MENU_H 312

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

static void vile_shell_render_bar(struct vile_server *server) {
	int W = server->shell_w, H = VILE_BAR_H;
	if (W <= 0) {
		return;
	}
	/* Bottom bar (Nov 10, 2026): the bar node sits at
	 * y = shell_h - VILE_BAR_H, so the click rects below are in
	 * output coords, not surface coords. */
	int bar_y = server->shell_h - VILE_BAR_H;
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
		cairo_set_source_rgba(cr, 0.06, 0.04, 0.11, 0.55);
		cairo_rectangle(cr, 0, 0, W, H);
		cairo_fill(cr);
	} else {
		/* No wallpaper to sample (should not happen — the
		 * default wallpaper always exists): flat translucent
		 * dark, like the old bar. */
		cairo_set_source_rgba(cr, 0.05, 0.035, 0.09, 0.72);
		cairo_rectangle(cr, 0, 0, W, H);
		cairo_fill(cr);
	}
	/* Hairline on the TOP edge — the separator between the
	 * desktop and the bar — with a faint purple rim light just
	 * below it. */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.14);
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

	/* Left: V tile + "Vile". (Workspace pills removed Nov 10, 2026 —
	 * Adnan didn't know what they were and asked for them gone.) */
	/* Soft purple glow behind the tile. */
	cairo_pattern_t *glow = cairo_pattern_create_radial(
		28, cy, 4, 28, cy, 30);
	cairo_pattern_add_color_stop_rgba(glow, 0,
		0.43, 0.29, 1.0, 0.35);
	cairo_pattern_add_color_stop_rgba(glow, 1,
		0.43, 0.29, 1.0, 0.0);
	cairo_set_source(cr, glow);
	cairo_rectangle(cr, 0, 0, 60, H);
	cairo_fill(cr);
	cairo_pattern_destroy(glow);
	/* Drop shadow, then the tile: the real ribbon V on a dark
	 * #171228 tile (vile-logo-dark.png) — NOT the ribbon on a
	 * purple tile, purple-on-purple has no contrast. Fallback
	 * when the asset is missing: the old drawn purple tile + V,
	 * so the bar never shows a blank. */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 14, 8, 28, 28, 9);
	cairo_fill(cr);
	if (server->shell_vile_icon) {
		cairo_save(cr);
		cairo_translate(cr, 14, 6);
		cairo_scale(cr, 28.0 / 1024, 28.0 / 1024);
		cairo_set_source_surface(cr, server->shell_vile_icon, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);
	} else {
		cairo_pattern_t *tile = cairo_pattern_create_linear(0, 6, 0, 34);
		cairo_pattern_add_color_stop_rgb(tile, 0, 0.55, 0.42, 1.0);
		cairo_pattern_add_color_stop_rgb(tile, 1, 0.43, 0.29, 1.0);
		cairo_set_source(cr, tile);
		cairo_new_path(cr);
		vile_rounded_rect(cr, 14, 6, 28, 28, 9);
		cairo_fill(cr);
		cairo_pattern_destroy(tile);
		vile_draw_text_centre(cr, "Inter Bold 17", 1.0, 1.0, 1.0,
			28, 21, "V");
	}
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 14.5, 6.5, 27, 27, 8.5);
	cairo_stroke(cr);
	server->shell_logo_rect = (struct wlr_box){
		.x = 14, .y = bar_y + 6, .width = 28, .height = 28 };
	vile_draw_text_at(cr, "Inter Bold 16", 0.93, 0.91, 0.98,
		52, 11, "Vile");

	/* Centre: time in a larger semibold weight, date beside it
	 * in a lighter secondary colour, both vertically centred. */
	int tw = vile_text_width(cr, "Inter Bold 17", time_str);
	int dw = vile_text_width(cr, "Inter 13", date_str);
	double cx0 = W / 2.0 - (tw + 12 + dw) / 2.0;
	vile_draw_text_at(cr, "Inter Bold 17", 0.93, 0.91, 0.98,
		cx0, 10, time_str);
	vile_draw_text_at(cr, "Inter 13", 0.68, 0.65, 0.78,
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
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, 6.5, pw - 1, 27, 14);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, 6.5, pw - 1, 27, 14);
	cairo_stroke(cr);
	double gx = px + 12 + 9;
	vile_draw_wifi_glyph(cr, gx, cy, 9, 0.9, 0.9, 0.95);
	gx += 9 + 10 + 9;
	vile_draw_volume_glyph(cr, gx, cy, 9, 0.9, 0.9, 0.95);
	gx += 9 + 10 + 11;
	vile_draw_battery_glyph(cr, gx - 11, cy - 6, 22, 12,
		cap, 0.9, 0.9, 0.95);
	gx += 11;
	if (cap >= 0) {
		gx += 6;
		vile_draw_text_at(cr, "Inter Bold 13", 0.93, 0.91, 0.98,
			gx, 13, batt_pct);
		gx += bw;
	}
	gx += 10 + 8;
	vile_draw_power_glyph(cr, gx, cy, 8, 0.9, 0.9, 0.95);
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

static void vile_shell_render_dock(struct vile_server *server) {
	/* The surface is bigger than the pill: a soft drop shadow and
	 * a faint purple ambient glow live around it (the pill itself
	 * is 296x80 at offset (40,20): four 56px icons). */
	int W = VILE_DOCK_PAD * 2 + VILE_DOCK_ICON * 4 + VILE_DOCK_GAP * 3 + 80;
	int H = VILE_DOCK_H + 60;
	int px = 40, py = 20;
	int pw = VILE_DOCK_PAD * 2 + VILE_DOCK_ICON * 4 + VILE_DOCK_GAP * 3;
	int ph = VILE_DOCK_H;
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, W, H);
	cairo_t *cr = cairo_create(surf);
	/* Ambient purple glow under the pill. */
	cairo_pattern_t *glow = cairo_pattern_create_radial(
		W / 2.0, py + ph - 6, 10, W / 2.0, py + ph - 6, 110);
	cairo_pattern_add_color_stop_rgba(glow, 0, 0.43, 0.29, 1.0, 0.20);
	cairo_pattern_add_color_stop_rgba(glow, 1, 0.43, 0.29, 1.0, 0.0);
	cairo_set_source(cr, glow);
	cairo_rectangle(cr, 0, 0, W, H);
	cairo_fill(cr);
	cairo_pattern_destroy(glow);
	/* Drop shadow under the pill. */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 2, py + 6, pw, ph, 20);
	cairo_fill(cr);
	/* Floating pill: translucent dark, rounded, hairline edge. */
	cairo_set_source_rgba(cr, 0.05, 0.035, 0.09, 0.72);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, py + 0.5, pw - 1, ph - 1, 20);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, px + 0.5, py + 0.5, pw - 1, ph - 1, 20);
	cairo_stroke(cr);
	/* Faint purple rim light along the pill's top edge. */
	cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.35);
	cairo_set_line_width(cr, 1.2);
	cairo_new_path(cr);
	cairo_move_to(cr, px + 22, py + 1.5);
	cairo_line_to(cr, px + pw - 22, py + 1.5);
	cairo_stroke(cr);

	/* Foot icon: the clear v2 logo tile, 56px. Piece 7: scaled
	 * around its cell centre by the hover/pop scale. */
	double fx = px + VILE_DOCK_PAD, fy = py + (ph - VILE_DOCK_ICON) / 2.0;
	double fsc = server->dock_rendered_scale[0];
	if (server->shell_foot_icon) {
		cairo_save(cr);
		cairo_translate(cr, fx + VILE_DOCK_ICON / 2.0,
			fy + VILE_DOCK_ICON / 2.0);
		cairo_scale(cr, fsc, fsc);
		cairo_translate(cr, -VILE_DOCK_ICON / 2.0,
			-VILE_DOCK_ICON / 2.0);
		cairo_scale(cr, (double)VILE_DOCK_ICON / 1024,
			(double)VILE_DOCK_ICON / 1024);
		cairo_set_source_surface(cr, server->shell_foot_icon, 0, 0);
		cairo_paint(cr);
		cairo_restore(cr);
	} else {
		vile_draw_text_centre(cr, "Inter Bold 20", 0.19, 0.60, 0.94,
			fx + VILE_DOCK_ICON / 2.0, fy + VILE_DOCK_ICON / 2.0, ">_");
	}
	/* Settings icon: the real ribbon V on a dark tile — same
	 * tile language as the foot logo, never the foot logo
	 * itself. Piece 7: the whole tile scales around the cell
	 * centre with the hover/pop scale. Fallback: the old
	 * purple circle + V. */
	double sx = px + VILE_DOCK_PAD + VILE_DOCK_ICON + VILE_DOCK_GAP;
	double ssc = server->dock_rendered_scale[1];
	cairo_save(cr);
	cairo_translate(cr, sx + VILE_DOCK_ICON / 2.0,
		fy + VILE_DOCK_ICON / 2.0);
	cairo_scale(cr, ssc, ssc);
	cairo_translate(cr, -VILE_DOCK_ICON / 2.0, -VILE_DOCK_ICON / 2.0);
	if (server->shell_vile_icon) {
		cairo_scale(cr, (double)VILE_DOCK_ICON / 1024,
			(double)VILE_DOCK_ICON / 1024);
		cairo_set_source_surface(cr, server->shell_vile_icon, 0, 0);
		cairo_paint(cr);
	} else {
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
		cairo_new_path(cr);
		cairo_arc(cr, VILE_DOCK_ICON / 2.0, VILE_DOCK_ICON / 2.0,
			26, 0, 2 * M_PI);
		cairo_fill(cr);
		vile_draw_text_centre(cr, "Inter Bold 30", 1.0, 1.0, 1.0,
			VILE_DOCK_ICON / 2.0, VILE_DOCK_ICON / 2.0 + 1, "V");
	}
	cairo_restore(cr);

	/* ScreenFit icon: a dark tile with a purple monitor glyph —
	 * the same tile language as the foot logo, never the foot logo
	 * itself. (ScreenFit = display / overscan settings.) Piece 7:
	 * the whole tile + glyph scales around the cell centre. */
	double tx = px + VILE_DOCK_PAD + VILE_DOCK_ICON + VILE_DOCK_GAP +
		VILE_DOCK_ICON + VILE_DOCK_GAP;
	double tsc = server->dock_rendered_scale[2];
	cairo_save(cr);
	cairo_translate(cr, tx + VILE_DOCK_ICON / 2.0,
		fy + VILE_DOCK_ICON / 2.0);
	cairo_scale(cr, tsc, tsc);
	cairo_translate(cr, -VILE_DOCK_ICON / 2.0, -VILE_DOCK_ICON / 2.0);
	/* NOTE: inside this transform the coords are LOCAL to the cell
	 * (0..56) — the foot icon above draws its logo at (0,0) for
	 * the same reason. The first Part A version drew with the
	 * ABSOLUTE (tx, fy) coords here, which put the tile ~976px too
	 * far right (off-screen) — the ScreenFit glyph was missing from
	 * every anim-test shot while look-01 (pre-Part-A) had it. */
	cairo_set_source_rgb(cr, 0.09, 0.07, 0.16); /* #171228 tile */
	cairo_new_path(cr);
	vile_rounded_rect(cr, 0, 0, VILE_DOCK_ICON, VILE_DOCK_ICON, 14);
	cairo_fill(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
	cairo_set_line_width(cr, 1);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 0.5, 0.5,
		VILE_DOCK_ICON - 1, VILE_DOCK_ICON - 1, 14);
	cairo_stroke(cr);
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
	cairo_restore(cr);

	/* Files icon: the files-icon.png tile (512px), drawn like the
	 * foot logo tile — same hover/pop scale around the cell centre.
	 * Fallback when the asset is missing: a cairo folder outline,
	 * so the dock never shows a blank cell. */
	double ix = px + VILE_DOCK_PAD + VILE_DOCK_ICON + VILE_DOCK_GAP +
		VILE_DOCK_ICON + VILE_DOCK_GAP +
		VILE_DOCK_ICON + VILE_DOCK_GAP;
	double isc = server->dock_rendered_scale[3];
	cairo_save(cr);
	cairo_translate(cr, ix + VILE_DOCK_ICON / 2.0,
		fy + VILE_DOCK_ICON / 2.0);
	cairo_scale(cr, isc, isc);
	cairo_translate(cr, -VILE_DOCK_ICON / 2.0, -VILE_DOCK_ICON / 2.0);
	if (server->shell_files_icon) {
		cairo_scale(cr, (double)VILE_DOCK_ICON / 512,
			(double)VILE_DOCK_ICON / 512);
		cairo_set_source_surface(cr, server->shell_files_icon, 0, 0);
		cairo_paint(cr);
	} else {
		/* Folder outline, local coords (0..56) like the ScreenFit
		 * tile above: tab + body, purple stroke on a dark tile. */
		cairo_set_source_rgb(cr, 0.09, 0.07, 0.16); /* #171228 */
		cairo_new_path(cr);
		vile_rounded_rect(cr, 0, 0, VILE_DOCK_ICON, VILE_DOCK_ICON, 14);
		cairo_fill(cr);
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0); /* #6D4AFF */
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
	cairo_restore(cr);

	/* Purple dots under running apps, each with its own halo. */
	double dot_y = py + ph - 7;
	if (server->shell_dock_foot_n > 0) {
		cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.30);
		cairo_new_path(cr);
		cairo_arc(cr, fx + VILE_DOCK_ICON / 2.0, dot_y, 7,
			0, 2 * M_PI);
		cairo_fill(cr);
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
		cairo_new_path(cr);
		cairo_arc(cr, fx + VILE_DOCK_ICON / 2.0, dot_y, 3.5,
			0, 2 * M_PI);
		cairo_fill(cr);
	}
	if (server->shell_dock_set_n > 0) {
		cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.30);
		cairo_new_path(cr);
		cairo_arc(cr, sx + VILE_DOCK_ICON / 2.0, dot_y, 7,
			0, 2 * M_PI);
		cairo_fill(cr);
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
		cairo_new_path(cr);
		cairo_arc(cr, sx + VILE_DOCK_ICON / 2.0, dot_y, 3.5,
			0, 2 * M_PI);
		cairo_fill(cr);
	}
	if (server->shell_dock_screenfit_n > 0) {
		cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.30);
		cairo_new_path(cr);
		cairo_arc(cr, tx + VILE_DOCK_ICON / 2.0, dot_y, 7,
			0, 2 * M_PI);
		cairo_fill(cr);
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
		cairo_new_path(cr);
		cairo_arc(cr, tx + VILE_DOCK_ICON / 2.0, dot_y, 3.5,
			0, 2 * M_PI);
		cairo_fill(cr);
	}
	if (server->shell_dock_files_n > 0) {
		cairo_set_source_rgba(cr, 0.43, 0.29, 1.0, 0.30);
		cairo_new_path(cr);
		cairo_arc(cr, ix + VILE_DOCK_ICON / 2.0, dot_y, 7,
			0, 2 * M_PI);
		cairo_fill(cr);
		cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
		cairo_new_path(cr);
		cairo_arc(cr, ix + VILE_DOCK_ICON / 2.0, dot_y, 3.5,
			0, 2 * M_PI);
		cairo_fill(cr);
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
	cairo_set_source_rgba(cr, 0.05, 0.035, 0.09, 0.97);
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
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
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

	vile_draw_text_at(cr, "Inter Bold 12", 0.55, 0.52, 0.62,
		20, 16, "SYSTEM");

	/* Wi-Fi row (real status from /sys). */
	char wifi[64], batt[64];
	vile_shell_wifi_status(wifi, sizeof(wifi));
	vile_shell_battery_status(batt, sizeof(batt));
	vile_draw_text_at(cr, "Inter 18", 0.93, 0.91, 0.98, 20, 54, "Wi-Fi");
	vile_shell_draw_text_right(cr, "Inter 16", 0.72, 0.69, 0.82,
		W - 20, 56, wifi);
	/* Volume row: the slider is VISUAL ONLY for now (no audio
	 * control is wired in the compositor) — it does not claim a
	 * real level. */
	vile_draw_text_at(cr, "Inter 18", 0.93, 0.91, 0.98, 20, 94, "Volume");
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.15);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 150, 101, 130, 6, 3);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 0.43, 0.29, 1.0);
	cairo_new_path(cr);
	vile_rounded_rect(cr, 150, 101, 130 * 0.6, 6, 3);
	cairo_fill(cr);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_new_path(cr);
	cairo_arc(cr, 150 + 130 * 0.6, 104, 7, 0, 2 * M_PI);
	cairo_fill(cr);
	/* Battery row (real status from /sys). */
	vile_draw_text_at(cr, "Inter 18", 0.93, 0.91, 0.98, 20, 134, "Battery");
	vile_shell_draw_text_right(cr, "Inter 16", 0.72, 0.69, 0.82,
		W - 20, 136, batt);

	/* Separator, then the actions that actually do something. */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
	cairo_rectangle(cr, 20, 172, W - 40, 1);
	cairo_fill(cr);
	vile_draw_lock_glyph(cr, 20, 190, 20, 0.93, 0.91, 0.98);
	vile_draw_text_at(cr, "Inter 18", 0.93, 0.91, 0.98, 48, 190, "Lock");
	vile_draw_text_at(cr, "Inter 18", 0.93, 0.91, 0.98, 20, 230, "Settings");
	vile_draw_power_glyph(cr, 30, 280, 8, 0.93, 0.91, 0.98);
	vile_draw_text_at(cr, "Inter 18", 0.93, 0.91, 0.98, 48, 270, "Power Off");

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
		}
	}
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
	int dock_w = VILE_DOCK_PAD * 2 + VILE_DOCK_ICON * 4 + VILE_DOCK_GAP * 3;
	server->shell_dock_x = (out_w - dock_w) / 2;
	/* Bottom-bar layout: the dock pill sits 12px above the 40px
	 * bottom bar (dock bottom = bar top - 12). */
	server->shell_dock_y = out_h - VILE_BAR_H - VILE_TILE_GAP - VILE_DOCK_H;
	/* The menu rises UP from the bottom bar: its bottom edge sits
	 * 8px above the bar top, flush right, never clipped. */
	server->shell_menu_x = out_w - VILE_MENU_W;
	server->shell_menu_y = out_h - VILE_BAR_H - 8 - VILE_MENU_H;
	if (server->shell_bar_node) {
		wlr_scene_node_set_position(&server->shell_bar_node->node,
			0, out_h - VILE_BAR_H);
	}
	if (server->shell_dock_node) {
		/* The dock surface is bigger than the pill (glow + shadow
		 * around it), so the node sits 40px left and 20px above
		 * the pill rect. */
		wlr_scene_node_set_position(&server->shell_dock_node->node,
			server->shell_dock_x - 40, server->shell_dock_y - 20);
	}
	if (server->shell_menu_node) {
		wlr_scene_node_set_position(&server->shell_menu_node->node,
			server->shell_menu_x, server->shell_menu_y);
	}
	server->shell_dock_rect = (struct wlr_box){
		.x = server->shell_dock_x, .y = server->shell_dock_y,
		.width = dock_w, .height = VILE_DOCK_H };
	server->shell_dock_foot_rect = (struct wlr_box){
		.x = server->shell_dock_x + VILE_DOCK_PAD,
		.y = server->shell_dock_y + (VILE_DOCK_H - VILE_DOCK_ICON) / 2,
		.width = VILE_DOCK_ICON, .height = VILE_DOCK_ICON };
	server->shell_dock_settings_rect = (struct wlr_box){
		.x = server->shell_dock_x + VILE_DOCK_PAD + VILE_DOCK_ICON + VILE_DOCK_GAP,
		.y = server->shell_dock_y + (VILE_DOCK_H - VILE_DOCK_ICON) / 2,
		.width = VILE_DOCK_ICON, .height = VILE_DOCK_ICON };
	server->shell_dock_screenfit_rect = (struct wlr_box){
		.x = server->shell_dock_x + VILE_DOCK_PAD + VILE_DOCK_ICON + VILE_DOCK_GAP +
			VILE_DOCK_ICON + VILE_DOCK_GAP,
		.y = server->shell_dock_y + (VILE_DOCK_H - VILE_DOCK_ICON) / 2,
		.width = VILE_DOCK_ICON, .height = VILE_DOCK_ICON };
	server->shell_dock_files_rect = (struct wlr_box){
		.x = server->shell_dock_x + VILE_DOCK_PAD + VILE_DOCK_ICON + VILE_DOCK_GAP +
			VILE_DOCK_ICON + VILE_DOCK_GAP +
			VILE_DOCK_ICON + VILE_DOCK_GAP,
		.y = server->shell_dock_y + (VILE_DOCK_H - VILE_DOCK_ICON) / 2,
		.width = VILE_DOCK_ICON, .height = VILE_DOCK_ICON };
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
	for (int i = 0; i < 4; i++) {
		server->dock_icon_scale[i] = 1.0f;
		server->dock_rendered_scale[i] = 1.0f;
		server->dock_icon_from[i] = 1.0f;
		server->dock_icon_to[i] = 1.0f;
		server->dock_icon_ms[i] = 0;
	}
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
	if (server->shell_menu_node) {
		wlr_scene_node_set_enabled(&server->shell_menu_node->node,
			false);
	}
	/* Foot icon for the dock: the clear v2 logo tile. */
	const char *icon_paths[] = {
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
			"Vile: shell: foot-logo-dark.png not found — dock falls back to text");
	}
	/* Files icon for the dock: the files-icon.png tile (512px,
	 * dark tile, purple folder). Same candidate paths as the foot
	 * logo — compositor dir on this VM and the ISO layout. */
	const char *files_icon_paths[] = {
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
	/* Vile logo for the bar tile + dock Settings icon: the real
	 * ribbon V on a dark #171228 tile (vile-logo-dark.png).
	 * Same candidate paths as the other dock icons. */
	const char *vile_icon_paths[] = {
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
			"Vile: shell: vile-logo-dark.png not found — bar/dock fall back to the drawn V");
	}
	vile_shell_fit(server, out_w, out_h);
	/* Render the dock once here: vile_shell_update() only re-renders
	 * it when the running-app set CHANGES, and at startup the set is
	 * already empty (0 == 0), so without this the dock node never
	 * gets a buffer and the bare desktop has no dock at all. */
	vile_shell_render_dock(server);
	wlr_log(WLR_INFO,
		"Vile: shell created: bar %dx%d, dock foot icon centre (%d,%d), settings centre (%d,%d), screenfit centre (%d,%d), files centre (%d,%d)",
		out_w, VILE_BAR_H,
		server->shell_dock_foot_rect.x +
			server->shell_dock_foot_rect.width / 2,
		server->shell_dock_foot_rect.y +
			server->shell_dock_foot_rect.height / 2,
		server->shell_dock_settings_rect.x +
			server->shell_dock_settings_rect.width / 2,
		server->shell_dock_settings_rect.y +
			server->shell_dock_settings_rect.height / 2,
		server->shell_dock_screenfit_rect.x +
			server->shell_dock_screenfit_rect.width / 2,
		server->shell_dock_screenfit_rect.y +
			server->shell_dock_screenfit_rect.height / 2,
		server->shell_dock_files_rect.x +
			server->shell_dock_files_rect.width / 2,
		server->shell_dock_files_rect.y +
			server->shell_dock_files_rect.height / 2);
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
		if (!title || strncmp(title, prefix, n) != 0) {
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

static void vile_shell_dock_activate(struct vile_server *server,
		const char *prefix) {
	if (strcmp(prefix, "Files") == 0) {
		struct vile_toplevel *f = vile_shell_find_files(server);
		if (f) {
			wlr_log(WLR_INFO, "Vile: dock: focus 'Files'");
			vile_focus_toplevel(server, f);
			return;
		}
		wlr_log(WLR_INFO, "Vile: dock: open Files");
		vile_spawn_files(server);
		return;
	}
	struct vile_toplevel *t = vile_shell_find_app(server, prefix);
	if (t) {
		wlr_log(WLR_INFO, "Vile: dock: focus '%s'", prefix);
		vile_focus_toplevel(server, t);
		return;
	}
	if (strcmp(prefix, "foot") == 0) {
		wlr_log(WLR_INFO, "Vile: dock: open foot");
		vile_spawn("foot");
	} else if (strcmp(prefix, "ScreenFit") == 0) {
		wlr_log(WLR_INFO, "Vile: dock: open ScreenFit");
		vile_spawn_screenfit(server);
	} else {
		wlr_log(WLR_INFO, "Vile: dock: open Settings");
		vile_spawn_settings(server);
	}
}

/* A click on the shell (bar / dock / system menu). Returns true if
 * the click was consumed here — the caller must then NOT change
 * window focus. Returns false when the click was outside the shell
 * (or closed the menu and landed on a window), so normal focus
 * handling runs. */
static bool vile_shell_click(struct vile_server *server, double lx, double ly) {
	if (!server->shell_created || server->locked) {
		return false;
	}
	int x = (int)lx, y = (int)ly;
	struct wlr_box *b;
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
	/* Bottom bar (y = shell_h - 32 .. shell_h - 1). */
	if (y >= server->shell_h - VILE_BAR_H && y < server->shell_h) {
		if (vile_box_contains(&server->shell_logo_rect, x, y)) {
			wlr_log(WLR_INFO, "Vile: shell: Vile logo clicked -> Settings");
			vile_spawn_settings(server);
			return true;
		}
		if (vile_box_contains(&server->shell_status_rect, x, y)) {
			wlr_log(WLR_INFO, "Vile: shell: status cluster clicked -> menu");
			vile_shell_set_menu(server, true);
			return true;
		}
		/* Centre clock / empty bar space: no action yet — but the
		 * bar is shell territory, so do not clear window focus. */
		return true;
	}
	/* Dock pill. */
	if (vile_box_contains(&server->shell_dock_rect, x, y)) {
		if (vile_box_contains(&server->shell_dock_foot_rect, x, y)) {
			/* Piece 7: small pop on click. */
			vile_anim_dock_pop(server, 0);
			vile_shell_dock_activate(server, "foot");
			return true;
		}
		if (vile_box_contains(&server->shell_dock_settings_rect, x, y)) {
			vile_anim_dock_pop(server, 1);
			vile_shell_dock_activate(server, "Settings");
			return true;
		}
		if (vile_box_contains(&server->shell_dock_screenfit_rect, x, y)) {
			vile_anim_dock_pop(server, 2);
			vile_shell_dock_activate(server, "ScreenFit");
			return true;
		}
		if (vile_box_contains(&server->shell_dock_files_rect, x, y)) {
			vile_anim_dock_pop(server, 3);
			vile_shell_dock_activate(server, "Files");
			return true;
		}
		/* Pill background: consume, no focus change. */
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
	 * re-emits from every pointer device. */
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
	server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
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
	/* Arch logo overlay: centred over the wallpaper, under windows.
	 * Failure is non-fatal — the desktop still works, just without
	 * the logo. */
	vile_load_logo(&server);
	/* Live-reload baseline: remember what is on screen and the
	 * hook file's mtime, so the 1-second watcher only fires when
	 * Settings actually picks a different wallpaper. */
	snprintf(server.wallpaper_current, sizeof(server.wallpaper_current),
		"%s", wallpaper);
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
