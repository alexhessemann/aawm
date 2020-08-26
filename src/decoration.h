#ifndef DECORATION_H
#define DECORATION_H

struct decoration {
	xcb_window_t parent;
	xcb_window_t client;
	// This is how not to do it: using (key id, windows id) pairs is more flexible.
	xcb_window_t close;
	xcb_window_t utility;
	xcb_window_t resize;
};

#endif
