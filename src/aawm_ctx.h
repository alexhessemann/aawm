#ifndef AAWM_CTX_H
#define AAWM_CTX_H

struct aawm_ctx {
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	// Awful storage for now
	size_t managed_len;
	struct decoration* managed_win;
	// Not wise to put this here either
	int motion_origin_x;
	int motion_origin_y;
	int window_origin_x;
	int window_origin_y;
};

#endif
