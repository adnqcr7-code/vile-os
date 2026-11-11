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
 *        vptr_test drag <x1> <y1> <x2> <y2> [hold_ms]
 *   drag   press BTN_LEFT at (x1,y1), move to (x2,y2) in 12 steps,
 *          optionally hold there for hold_ms (so the test can take a
 *          screenshot mid-drag), then release. One process does the
 *          whole drag — the button stays held across the steps.
 *        vptr_test rdrag <x1> <y1> <x2> <y2> [hold_ms]
 *   rdrag  same as drag, but with BTN_RIGHT (for Super+right-drag
 *          resize tests).
 *        vptr_test rclick <x> <y>
 *   rclick move to (x, y), press + release BTN_RIGHT there (dock
 *          context menu / unpin tests).
 *        vptr_test holddrag <x1> <y1> <x2> <y2>
 *   holddrag press BTN_LEFT at (x1,y1), HOLD 600ms without moving
 *          (so a long-press / hold timer fires), then move to
 *          (x2,y2) in 12 steps and release. Dock reorder drags.
 *        vptr_test holdclick <x> <y> [hold_ms]
 *   holdclick move to (x,y), press BTN_LEFT, HOLD hold_ms (default
 *          600 — past the compositor's 400ms dock hold timer),
 *          then release WITHOUT moving. A QEMU-style slow click:
 *          the HMP click in QEMU holds the button ~1s, so this is
 *          what the booted ISO's dock clicks actually are.
 *
 * Output extent: absolute motion is sent with extent VPTR_W x
 * VPTR_H from the environment (default 1920x1080), so the same
 * binary works at 1280x800 — set VPTR_W=1280 VPTR_H=800 there.
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
#include <unistd.h>
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

/* Absolute-motion extent (VPTR_W x VPTR_H, default 1920x1080):
 * the compositor scales absolute motion by this extent, so the
 * extent must match the output size or clicks land scaled. */
static int vptr_extent_w(void) {
	const char *e = getenv("VPTR_W");
	return e ? atoi(e) : 1920;
}

static int vptr_extent_h(void) {
	const char *e = getenv("VPTR_H");
	return e ? atoi(e) : 1080;
}

int main(int argc, char *argv[]) {
	/* rclick mode: vptr_test rclick <x> <y> */
	if (argc >= 4 && strcmp(argv[1], "rclick") == 0) {
		int x = atoi(argv[2]), y = atoi(argv[3]);
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
		zwlr_virtual_pointer_v1_motion_absolute(vptr, t,
			wl_fixed_from_int(x), wl_fixed_from_int(y),
			wl_fixed_from_int(vptr_extent_w()), wl_fixed_from_int(vptr_extent_h()));
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x111 /* BTN_RIGHT */, 1);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		usleep(100000);
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x111 /* BTN_RIGHT */, 0);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		printf("vptr_test: moved to (%d,%d), right-clicked\n", x, y);
		zwlr_virtual_pointer_v1_destroy(vptr);
		wl_display_disconnect(display);
		return 0;
	}
	/* holddrag mode: press, hold 600ms, then drag (dock reorder). */
	if (argc >= 6 && strcmp(argv[1], "holddrag") == 0) {
		int x1 = atoi(argv[2]), y1 = atoi(argv[3]);
		int x2 = atoi(argv[4]), y2 = atoi(argv[5]);
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
		zwlr_virtual_pointer_v1_motion_absolute(vptr, t,
			wl_fixed_from_int(x1), wl_fixed_from_int(y1),
			wl_fixed_from_int(vptr_extent_w()), wl_fixed_from_int(vptr_extent_h()));
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		usleep(100000);
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x110 /* BTN_LEFT */, 1);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		/* Hold still: the compositor's 400ms hold timer fires
		 * here, arming the reorder drag before any motion. */
		usleep(600000);
		for (int i = 1; i <= 12; i++) {
			int x = x1 + (x2 - x1) * i / 12;
			int y = y1 + (y2 - y1) * i / 12;
			t = now_ms();
			zwlr_virtual_pointer_v1_motion_absolute(vptr, t,
				wl_fixed_from_int(x), wl_fixed_from_int(y),
				wl_fixed_from_int(vptr_extent_w()), wl_fixed_from_int(vptr_extent_h()));
			zwlr_virtual_pointer_v1_frame(vptr);
			wl_display_roundtrip(display);
			usleep(40000);
		}
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x110 /* BTN_LEFT */, 0);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		printf("vptr_test: hold-dragged (%d,%d) -> (%d,%d)\n", x1, y1, x2, y2);
		zwlr_virtual_pointer_v1_destroy(vptr);
		wl_display_disconnect(display);
		return 0;
	}
	/* holdclick mode: vptr_test holdclick <x> <y> [hold_ms]
	 * Move to (x,y), press BTN_LEFT, hold still for hold_ms
	 * (default 600 — past the compositor's 400ms dock hold
	 * timer), then release WITHOUT moving. A QEMU-style slow
	 * click: the HMP click in QEMU holds the button ~1s. */
	if (argc >= 4 && strcmp(argv[1], "holdclick") == 0) {
		int x = atoi(argv[2]), y = atoi(argv[3]);
		int hold_ms = argc > 4 ? atoi(argv[4]) : 600;
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
		zwlr_virtual_pointer_v1_motion_absolute(vptr, t,
			wl_fixed_from_int(x), wl_fixed_from_int(y),
			wl_fixed_from_int(vptr_extent_w()),
			wl_fixed_from_int(vptr_extent_h()));
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		usleep(100000);
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x110 /* BTN_LEFT */, 1);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		/* Hold still: the compositor's 400ms hold timer fires
		 * here; the release that follows is a hold-release,
		 * not a drag. */
		usleep((useconds_t)hold_ms * 1000);
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, 0x110 /* BTN_LEFT */, 0);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		printf("vptr_test: hold-clicked (%d,%d) for %dms\n",
			x, y, hold_ms);
		zwlr_virtual_pointer_v1_destroy(vptr);
		wl_display_disconnect(display);
		return 0;
	}
	/* drag / rdrag mode: vptr_test drag|rdrag x1 y1 x2 y2 [hold_ms] */
	if (argc >= 6 && (strcmp(argv[1], "drag") == 0 ||
			strcmp(argv[1], "rdrag") == 0)) {
		int button = strcmp(argv[1], "rdrag") == 0 ?
			0x111 /* BTN_RIGHT */ : 0x110 /* BTN_LEFT */;
		int x1 = atoi(argv[2]), y1 = atoi(argv[3]);
		int x2 = atoi(argv[4]), y2 = atoi(argv[5]);
		int hold_ms = argc > 6 ? atoi(argv[6]) : 0;
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
		zwlr_virtual_pointer_v1_motion_absolute(vptr, t,
			wl_fixed_from_int(x1), wl_fixed_from_int(y1),
			wl_fixed_from_int(vptr_extent_w()), wl_fixed_from_int(vptr_extent_h()));
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		usleep(100000);
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, button, 1);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		for (int i = 1; i <= 12; i++) {
			int x = x1 + (x2 - x1) * i / 12;
			int y = y1 + (y2 - y1) * i / 12;
			t = now_ms();
			zwlr_virtual_pointer_v1_motion_absolute(vptr, t,
				wl_fixed_from_int(x), wl_fixed_from_int(y),
				wl_fixed_from_int(vptr_extent_w()), wl_fixed_from_int(vptr_extent_h()));
			zwlr_virtual_pointer_v1_frame(vptr);
			wl_display_roundtrip(display);
			usleep(40000);
		}
		if (hold_ms > 0) {
			usleep((useconds_t)hold_ms * 1000);
		}
		t = now_ms();
		zwlr_virtual_pointer_v1_button(vptr, t, button, 0);
		zwlr_virtual_pointer_v1_frame(vptr);
		wl_display_roundtrip(display);
		printf("vptr_test: dragged (%d,%d) -> (%d,%d)\n", x1, y1, x2, y2);
		zwlr_virtual_pointer_v1_destroy(vptr);
		wl_display_disconnect(display);
		return 0;
	}
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
		wl_fixed_from_int(vptr_extent_w()), wl_fixed_from_int(vptr_extent_h()));
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
