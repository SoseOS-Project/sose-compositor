// title bars: apps are told the compositor draws them, then none are drawn

#include "server.h"

static void decoration_destroy(struct wl_listener *listener, void *data) {
	/* the app's decoration object is going away, forget it */
	struct sose_toplevel *toplevel = wl_container_of(listener, toplevel, decoration_destroy);
	wl_list_remove(&toplevel->decoration_destroy.link);
	toplevel->decoration = NULL;
}

void server_new_decoration(struct wl_listener *listener, void *data) {
	/* an app asked about decorations. find its toplevel and remember the
	 * decoration, xdg_toplevel_commit answers it on the first commit */
	struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
	struct wlr_scene_tree *tree = decoration->toplevel->base->data;
	struct sose_toplevel *toplevel = tree->node.data;

	toplevel->decoration = decoration;
	toplevel->decoration_destroy.notify = decoration_destroy;
	wl_signal_add(&decoration->events.destroy, &toplevel->decoration_destroy);
}
