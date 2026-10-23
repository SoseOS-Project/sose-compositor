// app windows: opening, closing, size, focus, and the border around them

#include "server.h"

double now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/* how tall the screen is, in pixels */
int screen_height(struct sose_server *server) {
	struct wlr_box box;
	wlr_output_layout_get_box(server->output_layout, NULL, &box);
	return box.height;
}

void focus_toplevel(struct sose_toplevel *toplevel) {
	/* Note: this function only deals with keyboard focus. */
	if (toplevel == NULL) {
		return;
	}
	/* coming back from home: turn it back on and slide it up like a fresh open */
	if (toplevel->hidden) {
		toplevel->hidden = false;
		wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
		slide_toplevel(toplevel, screen_height(toplevel->server), 0, false);
	}
	struct sose_server *server = toplevel->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (prev_surface) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel != NULL) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	/* Move the toplevel to the front */
	wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	wl_list_remove(&toplevel->link);
	wl_list_insert(&server->toplevels, &toplevel->link);
	/* Activate the new surface */
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(seat, surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	}
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct sose_toplevel *toplevel = wl_container_of(listener, toplevel, map);

	wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

	/* open animation: slide up from below the screen */
	slide_toplevel(toplevel, screen_height(toplevel->server), 0, false);

	focus_toplevel(toplevel);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct sose_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

	struct sose_server *server = toplevel->server;

	/* close animation, unless it was already off screen at home */
	if (!toplevel->hidden) {
		animate_close(toplevel);

		/* a swipe on this app can't go on without it, or it would drag
		 * whatever app is underneath */
		server->swiping_home = false;
	}

	wl_list_remove(&toplevel->link);

	/* the app underneath is on screen now, give it the keyboard. otherwise
	 * typing goes nowhere until you click it */
	focus_toplevel(top_app(server));
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
	/* Called when a new surface state is committed. */
	struct sose_toplevel *toplevel = wl_container_of(listener, toplevel, commit);

	if (toplevel->xdg_toplevel->base->initial_commit) {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface.
		 * phone: every app is the size of the whole screen. */
		struct wlr_box screen;
		wlr_output_layout_get_box(toplevel->server->output_layout, NULL, &screen);
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, screen.width, screen.height);

		/* "server side" = the compositor draws decorations. it draws none,
		 * so the app has no title bar. can only be sent once the window is
		 * set up, which is this first commit */
		if (toplevel->decoration) {
			wlr_xdg_toplevel_decoration_v1_set_mode(toplevel->decoration,
				WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
		}
	}

	/* runs on every commit, so the border follows the window's size.
	 * 2 pixels bigger on each side */
	struct wlr_box geo = toplevel->xdg_toplevel->base->geometry;
	wlr_scene_rect_set_size(toplevel->border, geo.width + 4, geo.height + 4);
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	struct sose_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);

	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);

	/* stop listening before the memory is freed, or wlroots could call
	 * into it later */
	if (toplevel->decoration) {
		wl_list_remove(&toplevel->decoration_destroy.link);
	}

	free(toplevel);
}

static void xdg_toplevel_request_move(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user dragged its title bar.
	 * phone: apps stay where they are, so this is ignored. */
}

static void xdg_toplevel_request_resize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user dragged its edge.
	 * phone: apps are always the size of the screen, so this is ignored. */
}

static void xdg_toplevel_request_maximize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on client-side
	 * decorations. we don't support maximization, but to conform to
	 * xdg-shell protocol we still must send a configure.
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply.
	 * However, if the request was sent before an initial commit, we don't do
	 * anything and let the client finish the initial surface setup. */
	struct sose_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
}

static void xdg_toplevel_request_fullscreen(
		struct wl_listener *listener, void *data) {
	/* Just as with request_maximize, we must send a configure here. */
	struct sose_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
}

void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	/* This event is raised when a client creates a new toplevel (application window). */
	struct sose_server *server = wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	/* Allocate a sose_toplevel for this surface */
	struct sose_toplevel *toplevel = calloc(1, sizeof(*toplevel));
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_toplevel;
	toplevel->scene_tree =
		wlr_scene_xdg_surface_create(&toplevel->server->scene->tree, xdg_toplevel->base);

	/* the border: a rectangle inside the window's node, so it moves with it.
	 * silver = dom0. 0x0 for now, xdg_toplevel_commit gives it the real size */
	float silver[4] = {0.76f, 0.79f, 0.84f, 1.0f};
	toplevel->border = wlr_scene_rect_create(toplevel->scene_tree, 0, 0, silver);

	/* behind the window, sticking out 2 pixels on each side */
	wlr_scene_node_lower_to_bottom(&toplevel->border->node);
	wlr_scene_node_set_position(&toplevel->border->node, -2, -2);

	toplevel->scene_tree->node.data = toplevel;
	xdg_toplevel->base->data = toplevel->scene_tree;

	/* Listen to the various events it can emit */
	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

	/* cotd */
	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
}
