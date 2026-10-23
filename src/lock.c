// the lock screen: drawn by the compositor on top of everything, and while it's
// up no app gets any input

#include "server.h"

void lock_init(struct sose_server *server) {
	server->lock_tree = wlr_scene_tree_create(&server->scene->tree);

	float col[4] = { 0.05f, 0.06f, 0.08f, 1.0f };
	server->lock_bg = wlr_scene_rect_create(server->lock_tree, 0, 0, col);

	/* hidden until the phone gets locked */
	wlr_scene_node_set_enabled(&server->lock_tree->node, false);
}

void lock_show(struct sose_server *server) {
	/* cover the whole screen */
	struct wlr_box box;
	wlr_output_layout_get_box(server->output_layout, NULL, &box);
	wlr_scene_rect_set_size(server->lock_bg, box.width, box.height);

	/* on top of every app, then turn it on */
	wlr_scene_node_raise_to_top(&server->lock_tree->node);
	wlr_scene_node_set_enabled(&server->lock_tree->node, true);
	server->locked = true;
}

void lock_hide(struct sose_server *server) {
	wlr_scene_node_set_enabled(&server->lock_tree->node, false);
	server->locked = false;
}
