#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

#include "aawm_ctx.h"
#include "atoms.h"

// For the predefined, could have used <xcb/xcb_atom.h>
// (package libxcb-util0-dev in Debian)

static const char * atom_string[] = {
	"NONE/ANY", // 0
	"PRIMARY",
	"SECONDARY",
	"ARC",
	"ATOM",
	"BITMAP",
	"CARDINAL",
	"COLORMAP",
	"CURSOR",
	"CUT_BUFFER0",
	"CUT_BUFFER1", // 10
	"CUT_BUFFER2",
	"CUT_BUFFER3",
	"CUT_BUFFER4",
	"CUT_BUFFER5",
	"CUT_BUFFER6",
	"CUT_BUFFER7",
	"DRAWABLE",
	"FONT",
	"INTEGER",
	"PIXMAP", // 20
	"POINT",
	"RECTANGLE",
	"RESOURCE_MANAGER",
	"RGB_COLOR_MAP",
	"RGB_BEST_MAP",
	"RGB_BLUE_MAP",
	"RGB_DEFAULT_MAP",
	"RGB_GRAY_MAP",
	"RGB_GREEN_MAP",
	"RGB_RED_MAP", // 30
	"STRING",
	"VISUALID",
	"WINDOW",
	"WM_COMMAND",
	"WM_HINTS",
	"WM_CLIENT_MACHINE",
	"WM_ICON_NAME",
	"WM_ICON_SIZE",
	"WM_NAME",
	"WM_NORMAL_HINTS", // 40
	"WM_SIZE_HINTS",
	"WM_ZOOM_HINTS",
	"MIN_SPACE",
	"NORM_SPACE",
	"MAX_SPACE",
	"END_SPACE",
	"SUPERSCRIPT_X",
	"SUPERSCRIPT_Y",
	"SUBSCRIPT_X",
	"SUBSCRIPT_Y", // 50
	"UNDERLINE_POSITION",
	"UNDERLINE_THICKNESS",
	"STRIKEOUT_ASCENT",
	"STRIKEOUT_DESCENT",
	"ITALIC_ANGLE",
	"X_HEIGHT",
	"QUAD_WIDTH",
	"WEIGHT",
	"POINT_SIZE",
	"RESOLUTION", // 60
	"COPYRIGHT",
	"NOTICE",
	"FONT_NAME",
	"FAMILY_NAME",
	"FULL_NAME",
	"CAP_HEIGHT",
	"WM_CLASS",
	"WM_TRANSIENT_FOR", // 68

	// Following strings aren't pre-defined and need to be mapped

	"WM_PROTOCOLS",
	"WM_TAKE_FOCUS", // 70
	"WM_SAVE_YOURSELF",
	"WM_DELETE_WINDOW",
	"WM_CLIENT_LEADER",
	"WM_WINDOW_ROLE",
	"UTF8_STRING",
	"_NET_SUPPORTED",
	"_NET_CLIENT_LIST",
	"_NET_CLIENT_LIST_STACKING", // +10
	"_NET_NUMBER_OF_DESKTOPS",
	"_NET_DESKTOP_GEOMETRY", // 80
	"_NET_DESKTOP_VIEWPORT",
	"_NET_CURRENT_DESKTOP",
	"_NET_DESKTOP_NAMES",
	"_NET_ACTIVE_WINDOW",
	"_NET_WORKAREA",
	"_NET_SUPPORTING_WM_CHECK",
	"_NET_VIRTUAL_ROOTS",
	"_NET_DESKTOP_LAYOUT", // +20
	"_NET_SHOWING_DESKTOP",
	"_NET_CLOSE_WINDOW", // 90
	"_NET_MOVERESIZE_WINDOW",
	"_NET_WM_MOVERESIZE",
	"_NET_RESTACK_WINDOW",
	"_NET_REQUEST_FRAME_EXTENTS",
	"_NET_WM_NAME",
	"_NET_WM_VISIBLE_NAME",
	"_NET_WM_ICON_NAME",
	"_NET_WM_VISIBLE_ICON_NAME", // +30
	"_NET_WM_DESKTOP",
	"_NET_WM_WINDOW_TYPE", // 100
	"_NET_WM_WINDOW_TYPE_DESKTOP",
	"_NET_WM_WINDOW_TYPE_DOCK",
	"_NET_WM_WINDOW_TYPE_TOOLBAR",
	"_NET_WM_WINDOW_TYPE_MENU",
	"_NET_WM_WINDOW_TYPE_UTILITY",
	"_NET_WM_WINDOW_TYPE_SPLASH",
	"_NET_WM_WINDOW_TYPE_DIALOG",
	"_NET_WM_WINDOW_TYPE_NORMAL", // +40
	"_NET_WM_STATE",
	"_NET_WM_STATE_MODAL", // 110
	"_NET_WM_STATE_STICKY",
	"_NET_WM_STATE_MAXIMIZED_VERT",
	"_NET_WM_STATE_MAXIMIZED_HORZ",
	"_NET_WM_STATE_SHADED",
	"_NET_WM_STATE_SKIP_TASKBAR",
	"_NET_WM_STATE_SKIP_PAGER",
	"_NET_WM_STATE_HIDDEN",
	"_NET_WM_STATE_FULLSCREEN", // +50
	"_NET_WM_STATE_ABOVE",
	"_NET_WM_STATE_BELOW", // 120
	"_NET_WM_STATE_DEMANDS_ATTENTION",
	"_NET_WM_ALLOWED_ACTIONS",
	"_NET_WM_ACTION_MOVE",
	"_NET_WM_ACTION_RESIZE",
	"_NET_WM_ACTION_MINIMIZE",
	"_NET_WM_ACTION_SHADE",
	"_NET_WM_ACTION_STICK",
	"_NET_WM_ACTION_MAXIMIZE_HORZ", // +60
	"_NET_WM_ACTION_MAXIMIZE_VERT",
	"_NET_WM_ACTION_FULLSCREEN", // 130
	"_NET_WM_ACTION_CHANGE_DESKTOP",
	"_NET_WM_ACTION_CLOSE",
	"_NET_WM_STRUT",
	"_NET_WM_STRUT_PARTIAL",
	"_NET_WM_ICON_GEOMETRY",
	"_NET_WM_ICON",
	"_NET_WM_PID",
	"_NET_WM_HANDLED_ICONS", // +70
	"_NET_WM_USER_TIME",
	"_NET_FRAME_EXTENTS", // 140
	"_NET_WM_PING",
	"_NET_WM_SYNC_REQUEST",
	"_NET_WM_SYNC_REQUEST_COUNTER" // 143, +75
};

struct request {
	size_t idx;
	xcb_get_atom_name_cookie_t cookie;
};

// Map is for a given connection, changing the connection without purging the cache might return wrong results.

// Note that if map_add fails, we leak the memory pointed by atom_name_z.
const char * get_atom_name( struct aawm_ctx * a_ctx, xcb_atom_t a_atom )
{
	const char *result;
	
	if (a_atom <= AAWM_LAST_X11_PREDEFINED_ATOM) {
		result = atom_string[a_atom];
	} else if (!(result = map_lookup( a_ctx->atom_names, a_atom ))) {
		// test clause was a little tricky, we enter here if the atom wasn't found
		xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, xcb_get_atom_name( a_ctx->conn, a_atom ), NULL );
		int atom_name_len = xcb_get_atom_name_name_length( reply );
		char *atom_name = xcb_get_atom_name_name( reply );
		char *atom_name_z = malloc( atom_name_len + 1 );
		if (atom_name_z) {
			atom_name_z[atom_name_len] = 0;
			memcpy( atom_name_z, atom_name, atom_name_len );
			map_add( a_ctx->atom_names, a_atom, atom_name_z );
			fprintf( stderr, "Added name \"%s\" for atom %d\n", atom_name_z, a_atom );
		}
		result = atom_name_z;
	}

	return result;
}

// It sucks that xcb_get_atom_name_cookie_t isn't a scalar, but we won't cheat and won't use xcb_get_atom_name_cookie_t.sequence
// Returns true if an async request has been sent.
static bool get_atom_name_async( struct aawm_ctx * a_ctx, xcb_atom_t a_atom, const char** a_name, xcb_get_atom_name_cookie_t * a_cookie )
{
	bool result = false;
	
	if (a_atom <= AAWM_LAST_X11_PREDEFINED_ATOM) {
		*a_name = atom_string[a_atom];
	} else if (!(*a_name = map_lookup( a_ctx->atom_names, a_atom ))) {
		// test clause was a little tricky, we enter here if the atom wasn't found
		result = true;
		*a_cookie = xcb_get_atom_name( a_ctx->conn, a_atom );
	}

	return result;
}


// It is assumed that each element in a_list is unique.
const char ** alloc_atom_names_list( struct aawm_ctx * a_ctx, size_t a_len, xcb_atom_t *a_list )
{
	const char ** name_list = malloc( a_len * sizeof( const char * ) );
	if (name_list) {
		struct request *request_list = malloc( a_len * sizeof( struct request ) );
		if (request_list) {
			size_t idx;
			size_t request_list_len = 0;
			for (idx = 0; idx < a_len; idx++) {
				bool async = get_atom_name_async( a_ctx, a_list[idx], &name_list[idx], &request_list[request_list_len].cookie );
				if (async) {
					request_list[request_list_len++].idx = idx;
				}
			}
			for (idx = 0; idx < request_list_len; idx++) {
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, request_list[idx].cookie, NULL );
				int atom_name_len = xcb_get_atom_name_name_length( reply );
				char *atom_name = xcb_get_atom_name_name( reply );
				char *atom_name_z = malloc( atom_name_len + 1 );
				if (atom_name_z) {
					atom_name_z[atom_name_len] = 0;
					memcpy( atom_name_z, atom_name, atom_name_len );
					map_add( a_ctx->atom_names, a_list[request_list[idx].idx], atom_name_z );
					fprintf( stderr, "Added name \"%s\" for atom %d\n", atom_name_z, a_list[request_list[idx].idx] );
				}
				name_list[request_list[idx].idx] = atom_name_z;
			}
		} else {
			free( name_list );
			name_list = NULL;
		}
	}
	return name_list;
}


xcb_atom_t get_atom_from_symbol( struct aawm_ctx * a_ctx, aawm_atom_enum_t a_symbol )
{
	xcb_atom_t result;
	if (a_symbol <= AAWM_LAST_X11_PREDEFINED_ATOM) {
		result = a_symbol; // For convenience, we also accept pre-defined atoms
	} else if (a_symbol > AAWM_LAST_MAPPED_ATOM) {
		result = XCB_ATOM_NONE;
	} else if (!(result = a_ctx->atom_map[a_symbol - AAWM_LAST_X11_PREDEFINED_ATOM - 1])) {
		// test clause was a little tricky, we enter here if the atom wasn't found
		xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply( a_ctx->conn, xcb_intern_atom( a_ctx->conn, false, strlen( atom_string[a_symbol] ), atom_string[a_symbol] ), NULL );
		a_ctx->atom_map[a_symbol - AAWM_LAST_X11_PREDEFINED_ATOM] = reply->atom;
		result = reply->atom;
	}

	return result;
}


aawm_atom_enum_t get_symbol_from_atom( struct aawm_ctx * a_ctx, xcb_atom_t a_atom )
{
	aawm_atom_enum_t result = AAWM_ATOM_UNKNOWN;

	if (a_atom < AAWM_LAST_X11_PREDEFINED_ATOM) {
		result = a_atom;
	} else {
		int i;
		for (i = 0; result == AAWM_ATOM_UNKNOWN && i < (AAWM_LAST_MAPPED_ATOM - AAWM_LAST_X11_PREDEFINED_ATOM); i++) {
			if (a_ctx->atom_map[i] == a_atom) {
				result = i + AAWM_LAST_X11_PREDEFINED_ATOM + 1;
			}
		}
		if (result == AAWM_ATOM_UNKNOWN) {
			const char *atom_name = get_atom_name( a_ctx, a_atom );
			for (i = 0; result == AAWM_ATOM_UNKNOWN && i < (AAWM_LAST_MAPPED_ATOM - AAWM_LAST_X11_PREDEFINED_ATOM); i++) {
				if (a_ctx->atom_map[i] == 0) {
					if (!strcmp( atom_name, atom_string[i + AAWM_LAST_X11_PREDEFINED_ATOM + 1] )) {
						a_ctx->atom_map[i] = a_atom;
						result = i + AAWM_LAST_X11_PREDEFINED_ATOM + 1;
					}
				}
			}
		}
	}
	return result;
}

