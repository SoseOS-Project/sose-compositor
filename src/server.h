// shared by every file: the structs for the compositor, its screens, windows,
// popups and keyboards, and the functions one file calls in another.

#ifndef SOSE_SERVER_H
#define SOSE_SERVER_H

#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

/* For brevity's sake, struct members are annotated where they are used. */
enum sose_cursor_mode {
	SOSE_CURSOR_PASSTHROUGH,
	SOSE_CURSOR_MOVE,
	SOSE_CURSOR_RESIZE,
};

struct sose_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;

	/* lets apps know the compositor handles decorations, so they skip their title bar */
	struct wlr_xdg_decoration_manager_v1 *decoration_manager;
	struct wl_listener new_decoration;

	struct wl_list toplevels;

	/* closed apps sliding away, see animate.c */
	struct wl_list closing;

	/* swipe up from the bottom edge to go home, see gestures.c */
	bool swiping_home;
	double swipe_start_y;

	/* lock screen, see lock.c */
	bool locked;
	struct wlr_scene_tree *lock_tree; /* everything the lock screen draws */
	struct wlr_scene_rect *lock_bg; /* the dark background */

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener pointer_focus_change;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
	enum sose_cursor_mode cursor_mode;
	struct sose_toplevel *grabbed_toplevel;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;
};

struct sose_output {
	struct wl_list link;
	struct sose_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

struct sose_toplevel {
	struct wl_list link;
	struct sose_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;

	/* colored frame behind the window, later the color of the vm it's from */
	struct wlr_scene_rect *border;

	/* the app's decoration object, NULL if it never asked about decorations */
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener decoration_destroy;

	/* slide animation, see animate.c: y moves from slide_from to slide_to */
	bool sliding;
	double slide_start;
	int slide_from, slide_to;
	bool hide_after_slide;   /* sent home: turn invisible once it's off screen */

	/* at home: the app keeps running but isn't on screen */
	bool hidden;
};

/* what's left of a closed app: a copy of its last picture, sliding down off the
 * screen. the app itself is already gone, see animate.c */
struct sose_closing {
	struct wl_list link;
	struct wlr_scene_tree *tree;
	double start;
	int from_y;
};

struct sose_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

struct sose_keyboard {
	struct wl_list link;
	struct sose_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

/* lock.c */
void lock_init(struct sose_server *server);
void lock_show(struct sose_server *server);
void lock_hide(struct sose_server *server);

/* toplevel.c */
double now_ms(void);
int screen_height(struct sose_server *server);
void focus_toplevel(struct sose_toplevel *toplevel);
void server_new_xdg_toplevel(struct wl_listener *listener, void *data);

/* animate.c */
void slide_toplevel(struct sose_toplevel *toplevel, int from, int to, bool hide_after);
void animate_close(struct sose_toplevel *toplevel);
bool animate_step(struct sose_server *server);

/* gestures.c */
bool gesture_press(struct sose_server *server);
bool gesture_motion(struct sose_server *server);
bool gesture_release(struct sose_server *server);
void go_home(struct sose_server *server);
struct sose_toplevel *top_app(struct sose_server *server);

/* popup.c */
void server_new_xdg_popup(struct wl_listener *listener, void *data);

/* decoration.c */
void server_new_decoration(struct wl_listener *listener, void *data);

/* output.c */
void server_new_output(struct wl_listener *listener, void *data);

/* keybindings.c */
void spawn(const char *cmd);
bool handle_keybinding(struct sose_server *server, xkb_keysym_t sym);

/* input.c */
void server_new_input(struct wl_listener *listener, void *data);
void seat_request_cursor(struct wl_listener *listener, void *data);
void seat_pointer_focus_change(struct wl_listener *listener, void *data);
void seat_request_set_selection(struct wl_listener *listener, void *data);
void reset_cursor_mode(struct sose_server *server);
void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);

#endif
