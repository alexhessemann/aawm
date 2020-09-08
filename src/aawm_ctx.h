#ifndef AAWM_CTX_H
#define AAWM_CTX_H

#include <stdbool.h>

struct aawm_ctx {
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	int shape_base;
	// Awful storage for now
	size_t managed_len;
	struct decoration* managed_win;
	// Not wise to put this here either
	bool moving;
	int motion_origin_x;
	int motion_origin_y;
	int window_origin_x;
	int window_origin_y;
	xcb_font_t cursor_font;
	xcb_cursor_t pirate;
	xcb_cursor_t fleur;
};

#endif
