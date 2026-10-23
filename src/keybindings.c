// compositor shortcuts. these run while alt is held

#include "server.h"

bool handle_keybinding(struct sose_server *server, xkb_keysym_t sym) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 *
	 * This function assumes Alt is held down.
	 */
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_F1:
		/* Cycle to the next toplevel. with one app open that's the app
		 * itself, which brings it back from home */
		if (wl_list_empty(&server->toplevels)) {
			break;
		}
		struct sose_toplevel *next_toplevel =
			wl_container_of(server->toplevels.prev, next_toplevel, link);
		focus_toplevel(next_toplevel);
		break;
	case XKB_KEY_Return:
		/* alt+enter: open a terminal. the forked copy turns into foot,
		 * the compositor itself keeps running */
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", "foot", (void *)NULL);
		}
		break;
	default:
		return false;
	}
	return true;
}
