// gestures. for now: swipe up from the bottom edge to go home. works with the
// mouse while testing on the pc, touch comes later. input.c calls these first,
// and if they return true the click or move belongs to the gesture, not an app

#include "server.h"

/* how close to the bottom edge a swipe has to start, in pixels */
#define EDGE 30

/* how far up you have to pull before letting go counts as "go home",
 * as a share of the screen height */
#define HOME_DISTANCE 0.15

/* the app you're looking at: the frontmost one that isn't at home.
 * NULL when nothing is on screen */
struct sose_toplevel *top_app(struct sose_server *server) {
	struct sose_toplevel *toplevel;
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (!toplevel->hidden) {
			return toplevel;
		}
	}
	return NULL;
}

/* button pressed: start a swipe if it's at the bottom edge and an app is open */
bool gesture_press(struct sose_server *server) {
	double y = server->cursor->y;
	if (y < screen_height(server) - EDGE) {
		return false;
	}

	struct sose_toplevel *app = top_app(server);
	if (app == NULL) {
		return false;
	}

	server->swiping_home = true;
	server->swipe_start_y = y;
	/* the finger is in charge now, stop any slide that's still running */
	app->sliding = false;
	return true;
}

/* mouse moved: the app follows the finger up (never down past where it was) */
bool gesture_motion(struct sose_server *server) {
	if (!server->swiping_home) {
		return false;
	}

	struct sose_toplevel *app = top_app(server);
	if (app != NULL) {
		int dy = (int)(server->cursor->y - server->swipe_start_y);
		if (dy > 0) {
			dy = 0;
		}
		wlr_scene_node_set_position(&app->scene_tree->node, 0, dy);
	}
	return true;
}

/* button released: far enough up goes home, otherwise the app slides back */
bool gesture_release(struct sose_server *server) {
	if (!server->swiping_home) {
		return false;
	}
	server->swiping_home = false;

	struct sose_toplevel *app = top_app(server);
	if (app == NULL) {
		return true;
	}

	double pulled = server->swipe_start_y - server->cursor->y;
	if (pulled > screen_height(server) * HOME_DISTANCE) {
		go_home(server);
	} else {
		slide_toplevel(app, app->scene_tree->node.y, 0, false);
	}
	return true;
}

/* go home: the app on screen slides up and away, every other open app is
 * hidden right away. they all keep running, alt+f1 brings one back */
void go_home(struct sose_server *server) {
	struct sose_toplevel *app = top_app(server);
	if (app == NULL) {
		return;
	}

	app->hidden = true;
	wlr_xdg_toplevel_set_activated(app->xdg_toplevel, false);
	slide_toplevel(app, app->scene_tree->node.y, -screen_height(server), true);

	struct sose_toplevel *toplevel;
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (!toplevel->hidden) {
			toplevel->hidden = true;
			wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
		}
	}

	/* nothing on screen, so no app gets the keyboard */
	wlr_seat_keyboard_notify_clear_focus(server->seat);
}
