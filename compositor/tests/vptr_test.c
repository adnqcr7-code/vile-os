/* vptr_test — minimal virtual-pointer client for Vile input tests.
 *
 * Moves the compositor cursor to absolute layout coords (x, y) and
 * optionally left-clicks there, using the wlr-virtual-pointer
 * protocol (wlroots' virtual pointer manager). This is how we test
 * pointer input headless: libinput has no physical mouse here.
 *
 * Usage: vptr_test <x> <y> [click]
 *   x, y   absolute coords in the output layout (1920x1080 here)
 *   click  if present, press + release BTN_LEFT at (x, y)
 *
 * Build: gcc vptr_test.c -o vptr_test $(pkg-config --cflags --libs wayland-client)
 * The client header is generated from wlroots' protocol XML:
 *   wayland-scanner client-header \
 *     /tmp/vile-wlroots/wlroots/protocol/wlr-virtual-pointer-unstable-v1.xml \
 *     wlr-virtual-pointer-unstable-v1-client-protocol.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

static struct wl_seat *seat;
static struct zwlr_virtual_pointer_manager_v1 *vptr_mgr;

static void registry_global(void *data, struct wl_registry *reg,
		uint32_t name, const char *iface, uint32_t version) {
	(void)data;
	if (strcmp(iface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(reg, name, &wl_seat_interface, 7);
	} else if (strcmp(iface,
			zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
		vptr_mgr = wl_registry_bind(reg, name,
			&zwlr_virtual_pointer_manager_v1_interface, 2);
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

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage: %s <x> <y> [click]\n", argv[0]);
		return 1;
	}
	int x = atoi(argv[1]);
	int y = atoi(argv[2]);
	int click = argc > 3 && strcmp(argv[3], "click") == 0;

	struct wl_display *display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "vptr_test: cannot connect to Wayland\n");
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!seat || !vptr_mgr) {
		fprintf(stderr, "vptr_test: no seat (%p) or vptr manager (%p)\n",
			(void *)seat, (void *)vptr_mgr);
		return 1;
	}
	struct zwlr_virtual_pointer_v1 *vptr =
		zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
			vptr_mgr, seat);
	uint32_t t = now_ms();
	/* Absolute motion over the 1920x1080 layout. */
	zwlr_virtual_pointer_v1_motion_absolute(vptr, t,
		wl_fixed_from_int(x), wl_fixed_from_int(y),
		wl_fixed_from_int(1920), wl_fixed_from_int(1080));
	zwlr_virtual_pointer_v1_frame(vptr);
	wl_display_roundtrip(display);
	if (click) {
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x110 /* BTN_LEFT */, 1);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x110 /* BTN_LEFT */, 0);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
	}
	printf("vptr_test: moved to (%d,%d)%s\n", x, y,
		click ? ", clicked" : "");
	zwlr_virtual_pointer_v1_destroy(vptr);
	wl_display_disconnect(display);
	return 0;
}
