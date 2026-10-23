// compositor shortcuts. these run while alt is held

#include "server.h"

#include <sys/wait.h>

/* starts a program. a plain fork would leave a zombie process behind every time
 * the program exits, since the compositor never waits for it. so a middle
 * process starts it and quits right away, and the program gets adopted by init,
 * which cleans up after it */
void spawn(const char *cmd) {
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
			_exit(1);
		}
		_exit(0);
	}
	if (pid > 0) {
		waitpid(pid, NULL, 0);
	}
}

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
		/* alt+enter: open a terminal */
		spawn("foot");
		break;
	default:
		return false;
	}
	return true;
}
