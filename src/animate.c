// animations: windows sliding up when they open, away when you go home, and
// down when they close. output_frame calls animate_step once every frame

#include "server.h"

/* how long each animation takes, in milliseconds */
#define SLIDE_MS 300.0
#define CLOSE_MS 260.0

/* ease out: starts fast, slows down at the end. t goes from 0 to 1 */
static double ease_out(double t) {
	return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
}

/* ask every screen for a new frame, so output_frame runs and moves things */
static void schedule_frames(struct sose_server *server) {
	struct sose_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_output_schedule_frame(output->wlr_output);
	}
}

/* start sliding a window's y from `from` to `to`. hide_after makes it
 * invisible once it gets there (used when it's sent home) */
void slide_toplevel(struct sose_toplevel *toplevel, int from, int to, bool hide_after) {
	toplevel->sliding = true;
	toplevel->slide_start = now_ms();
	toplevel->slide_from = from;
	toplevel->slide_to = to;
	toplevel->hide_after_slide = hide_after;

	/* jump to the start right away, so it never shows one frame in the wrong spot */
	wlr_scene_node_set_position(&toplevel->scene_tree->node, 0, from);
	schedule_frames(toplevel->server);
}

/* copies every picture and rectangle in a scene tree into another tree. the
 * copies keep the app's last frame alive after the app itself is gone */
static void copy_nodes(struct wlr_scene_tree *from, struct wlr_scene_tree *to) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &from->children, link) {
		switch (node->type) {
		case WLR_SCENE_NODE_TREE: {
			/* trees get copied even when hidden: the app's own tree is already
			 * turned off by the time it closes, its pictures inside are not */
			struct wlr_scene_tree *child = wlr_scene_tree_create(to);
			wlr_scene_node_set_position(&child->node, node->x, node->y);
			copy_nodes(wlr_scene_tree_from_node(node), child);
			break;
		}
		case WLR_SCENE_NODE_RECT: {
			if (!node->enabled) {
				break;
			}
			struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
			struct wlr_scene_rect *copy =
				wlr_scene_rect_create(to, rect->width, rect->height, rect->color);
			wlr_scene_node_set_position(&copy->node, node->x, node->y);
			break;
		}
		case WLR_SCENE_NODE_BUFFER: {
			struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
			if (!node->enabled || buffer->buffer == NULL) {
				break;
			}
			/* the copy holds on to the same picture, wlroots keeps it alive */
			struct wlr_scene_buffer *copy = wlr_scene_buffer_create(to, buffer->buffer);
			wlr_scene_buffer_set_source_box(copy, &buffer->src_box);
			wlr_scene_buffer_set_dest_size(copy, buffer->dst_width, buffer->dst_height);
			wlr_scene_buffer_set_transform(copy, buffer->transform);
			wlr_scene_node_set_position(&copy->node, node->x, node->y);
			break;
		}
		}
	}
}

/* an app is closing: take a copy of how it looked and slide that down.
 * called from xdg_toplevel_unmap, while the last frame is still there */
void animate_close(struct sose_toplevel *toplevel) {
	struct sose_server *server = toplevel->server;

	struct sose_closing *closing = calloc(1, sizeof(*closing));
	closing->tree = wlr_scene_tree_create(&server->scene->tree);
	closing->start = now_ms();
	closing->from_y = toplevel->scene_tree->node.y;

	copy_nodes(toplevel->scene_tree, closing->tree);
	wlr_scene_node_set_position(&closing->tree->node, 0, closing->from_y);
	wlr_scene_node_raise_to_top(&closing->tree->node);

	wl_list_insert(&server->closing, &closing->link);
	schedule_frames(server);
}

/* moves everything that's animating one step. returns true while something is
 * still moving, so output_frame keeps asking for frames */
bool animate_step(struct sose_server *server) {
	bool moving = false;
	double now = now_ms();

	struct sose_toplevel *toplevel;
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (!toplevel->sliding) {
			continue;
		}

		/* how far along it is, 0 to 1 */
		double t = (now - toplevel->slide_start) / SLIDE_MS;
		if (t >= 1.0) {
			t = 1.0;
			toplevel->sliding = false;
		} else {
			moving = true;
		}

		int y = toplevel->slide_from +
			(int)((toplevel->slide_to - toplevel->slide_from) * ease_out(t));
		wlr_scene_node_set_position(&toplevel->scene_tree->node, 0, y);

		if (!toplevel->sliding && toplevel->hide_after_slide) {
			wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
		}
	}

	int height = screen_height(server);
	struct sose_closing *closing, *tmp;
	wl_list_for_each_safe(closing, tmp, &server->closing, link) {
		double t = (now - closing->start) / CLOSE_MS;
		if (t >= 1.0) {
			/* off the screen: throw the copy away */
			wlr_scene_node_destroy(&closing->tree->node);
			wl_list_remove(&closing->link);
			free(closing);
			continue;
		}
		moving = true;

		int y = closing->from_y + (int)((height - closing->from_y) * ease_out(t));
		wlr_scene_node_set_position(&closing->tree->node, 0, y);
	}

	return moving;
}
