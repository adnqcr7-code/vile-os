/* vkey_test — minimal virtual-keyboard client for Vile input tests.
 *
 * Presses a chord: holds every named key down in order, then releases
 * them in reverse — so `vkey_test Super_L Return` is a real
 * Super+Return: the Logo modifier is still held when Return is
 * pressed, and the compositor sees the keybind.
 *
 * Why this exists: `wtype -k Super_L -k Return` does NOT do this.
 * wtype presses and releases each -k keysym in turn, so Super_L is
 * already up when Return is pressed — the compositor sees a plain
 * Return (proved Nov 10: the literal `q` from a `wtype -k Super_L
 * -k q` attempt landed in the foot prompt, no keybind fired).
 * wtype is still fine for plain typing; chords need this client.
 *
 * Second gotcha (also proved Nov 10, in wlroots' source): virtual
 * keyboard key events are sent with `.update_state = false`
 * (`wlr_virtual_keyboard_v1.c`), so pressing a key NEVER updates
 * the compositor's keyboard modifier state — the client MUST send
 * the modifier state explicitly via the `modifiers` request after
 * each press/release. A first version of this client sent only key
 * events, and the keybinds still did not fire (the `q` from
 * `Super_L q` leaked into foot as text). This version mirrors the
 * key state locally with libxkbcommon and sends `.modifiers`
 * after every press and release, exactly like wtype does.
 *
 * Usage: vkey_test <keysym> [<keysym> ...]
 *   keysym names: Super_L, Return, q, l, m, Escape, ...
 *   e.g. vkey_test Super_L Return   (Super+Enter -> open foot)
 *        vkey_test Super_L q        (Super+Q -> close window)
 *        vkey_test Super_L l        (Super+L -> lock now)
 *
 * Keymap: compiled with libxkbcommon (evdev / pc105 / us), sent to
 * the compositor via the virtual-keyboard protocol; keysyms are
 * resolved to evdev keycodes through that keymap.
 *
 * Build: wayland-scanner client-header \
 *          /tmp/vile-wlroots/wlroots/protocol/virtual-keyboard-unstable-v1.xml \
 *          zwp-virtual-keyboard-unstable-v1-client-protocol.h
 *        wayland-scanner private-code \
 *          /tmp/vile-wlroots/wlroots/protocol/virtual-keyboard-unstable-v1.xml \
 *          zwp-virtual-keyboard-unstable-v1-client-protocol.c
 *        gcc vkey_test.c zwp-virtual-keyboard-unstable-v1-client-protocol.c \
 *          -o vkey_test $(pkg-config --cflags --libs wayland-client xkbcommon)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "zwp-virtual-keyboard-unstable-v1-client-protocol.h"

static struct wl_seat *seat;
static struct zwp_virtual_keyboard_manager_v1 *vkb_mgr;
static struct zwp_virtual_keyboard_v1 *vkbd;
static struct xkb_keymap *keymap;
static struct xkb_state *xkb_state;

/* Mirror the pressed keys in a local xkb state and tell the
 * compositor the resulting modifier state. The compositor will not
 * work it out from key events alone (update_state = false). */
static void send_modifiers(void) {
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(xkb_state,
		XKB_STATE_MODS_DEPRESSED);
	xkb_mod_mask_t latched = xkb_state_serialize_mods(xkb_state,
		XKB_STATE_MODS_LATCHED);
	xkb_mod_mask_t locked = xkb_state_serialize_mods(xkb_state,
		XKB_STATE_MODS_LOCKED);
	xkb_layout_index_t group = xkb_state_serialize_layout(xkb_state,
		XKB_STATE_LAYOUT_EFFECTIVE);
	zwp_virtual_keyboard_v1_modifiers(vkbd,
		(uint32_t)depressed, (uint32_t)latched,
		(uint32_t)locked, (uint32_t)group);
}

static void registry_global(void *data, struct wl_registry *reg,
		uint32_t name, const char *iface, uint32_t version) {
	(void)data; (void)version;
	if (strcmp(iface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(reg, name, &wl_seat_interface, 7);
	} else if (strcmp(iface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
		vkb_mgr = wl_registry_bind(reg, name,
			&zwp_virtual_keyboard_manager_v1_interface, 1);
	}
}

static void registry_global_remove(void *data, struct wl_registry *reg,
		uint32_t name) {
	(void)data; (void)reg; (void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static uint32_t now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Find the evdev keycode whose keysym (level 0) matches `name`. */
static int keysym_to_evdev(const char *name) {
	xkb_keysym_t want = xkb_keysym_from_name(name, 0);
	if (want == XKB_KEY_NoSymbol) {
		return -1;
	}
	for (xkb_keycode_t kc = 8; kc < 256; kc++) {
		const xkb_keysym_t *syms;
		int n = xkb_keymap_key_get_syms_by_level(keymap, kc, 0, 0, &syms);
		for (int i = 0; i < n; i++) {
			if (syms[i] == want) {
				return (int)kc - 8; /* xkb keycode -> evdev keycode */
			}
		}
	}
	return -1;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <keysym> [<keysym> ...]\n", argv[0]);
		return 1;
	}
	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "vkey_test: cannot connect to Wayland display\n");
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!seat || !vkb_mgr) {
		fprintf(stderr, "vkey_test: no seat or virtual-keyboard manager\n");
		return 1;
	}

	/* Compile a US keymap and send it to the compositor. */
	struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_rule_names names = {
		.rules = "evdev", .model = "pc105", .layout = "us",
		.variant = NULL, .options = NULL,
	};
	keymap = xkb_keymap_new_from_names(ctx, &names,
		XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		fprintf(stderr, "vkey_test: cannot compile keymap\n");
		return 1;
	}
	const char *keymap_str = xkb_keymap_get_as_string(keymap,
		XKB_KEYMAP_USE_ORIGINAL_FORMAT);
	xkb_state = xkb_state_new(keymap);
	if (!xkb_state) {
		fprintf(stderr, "vkey_test: cannot create xkb state\n");
		return 1;
	}
	size_t keymap_len = strlen(keymap_str) + 1;
	int fd = memfd_create("vkey-keymap", MFD_CLOEXEC);
	if (fd < 0) {
		perror("memfd_create");
		return 1;
	}
	if (write(fd, keymap_str, keymap_len) != (ssize_t)keymap_len) {
		perror("write keymap");
		return 1;
	}
	lseek(fd, 0, SEEK_SET);

	vkbd = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
		vkb_mgr, seat);
	zwp_virtual_keyboard_v1_keymap(vkbd,
		WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, (uint32_t)keymap_len);
	wl_display_roundtrip(display);
	close(fd);

	/* Resolve all keysyms first, so a typo fails before pressing. */
	int nkeys = argc - 1;
	int *keycodes = calloc((size_t)nkeys, sizeof(int));
	for (int i = 0; i < nkeys; i++) {
		keycodes[i] = keysym_to_evdev(argv[i + 1]);
		if (keycodes[i] < 0) {
			fprintf(stderr, "vkey_test: unknown keysym '%s'\n", argv[i + 1]);
			return 1;
		}
	}

	/* Press in order (modifier stays held), release in reverse.
	 * After every press/release, update the local xkb state and
	 * send the modifier state — without this the compositor's
	 * keyboard modifiers never change (see header comment). */
	for (int i = 0; i < nkeys; i++) {
		zwp_virtual_keyboard_v1_key(vkbd, now_ms(),
			(uint32_t)keycodes[i], WL_KEYBOARD_KEY_STATE_PRESSED);
		xkb_state_update_key(xkb_state, (xkb_keycode_t)keycodes[i] + 8,
			XKB_KEY_DOWN);
		send_modifiers();
		wl_display_flush(display);
		wl_display_roundtrip(display);
		usleep(50000);
	}
	for (int i = nkeys - 1; i >= 0; i--) {
		zwp_virtual_keyboard_v1_key(vkbd, now_ms(),
			(uint32_t)keycodes[i], WL_KEYBOARD_KEY_STATE_RELEASED);
		xkb_state_update_key(xkb_state, (xkb_keycode_t)keycodes[i] + 8,
			XKB_KEY_UP);
		send_modifiers();
		wl_display_flush(display);
		wl_display_roundtrip(display);
		usleep(50000);
	}

	printf("vkey_test: chord");
	for (int i = 0; i < nkeys; i++) {
		printf(" %s", argv[i + 1]);
	}
	printf(" sent (keycodes");
	for (int i = 0; i < nkeys; i++) {
		printf(" %d", keycodes[i]);
	}
	printf(")\n");

	zwp_virtual_keyboard_v1_destroy(vkbd);
	wl_display_roundtrip(display);
	wl_display_disconnect(display);
	return 0;
}
