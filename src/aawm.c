#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/shape.h>
#include <xcb/xcb_icccm.h>

#include "aawm_ctx.h"
#include "aawm_window.h"
//#include "circle_menu.xbm"
#include "get_constant_string.h"
#include "utfconv.h"

int setupscreen( xcb_connection_t *a_conn, xcb_drawable_t a_root )
{
	int i;
//	xcb_get_window_attributes_reply_t *attr;

	/* Get all children. */
	xcb_query_tree_reply_t *reply = xcb_query_tree_reply( a_conn,
		xcb_query_tree( a_conn, a_root ), 0);

	int len = xcb_query_tree_children_length( reply );    
	xcb_window_t *children = xcb_query_tree_children( reply );

	printf( "%d windows:\n", len );

	for (i = 0; i < len; i ++)
	{
		printf( "window %d\n", children[i] );
//		attr = xcb_get_window_attributes_reply( a_conn,
//			xcb_get_window_attributes( a_conn, children[i] ),
//			NULL );
//		free( attr );
	}

	return 0;
}


int setupshape( struct aawm_ctx* a_ctx )
{
	const xcb_query_extension_reply_t *extension = xcb_get_extension_data( a_ctx->conn, &xcb_shape_id );
	if (!extension->present) {
		printf( "No shape extension.\n" );
		return -1;
	}

	a_ctx->shape_base = extension->first_event;
	printf( "Shape base is %d\n", a_ctx->shape_base );

	return a_ctx->shape_base;
}


void map_request_passthrough( struct aawm_ctx* a_ctx, xcb_map_request_event_t *a_ev )
{
	xcb_map_window( a_ctx->conn, a_ev->window );
	xcb_flush( a_ctx->conn );
}

void map_request_reparent( struct aawm_ctx* a_ctx, xcb_map_request_event_t *a_ev )
{
	char *name = NULL;
	int name_len;
	int utf8_string = 0;

	// Create internal structures for child windows

	xcb_window_t xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *frame_win = aawm_allocate_window( xid, AAWM_ROLE_FRAME, a_ctx->screen->root );
	map_add( a_ctx->windows_list, xid, frame_win );
	aawm_window_t *client_win = aawm_allocate_window( a_ev->window, AAWM_ROLE_CLIENT, frame_win->wid );
	map_add( a_ctx->windows_list, a_ev->window, client_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *close_win = aawm_allocate_window( xid, AAWM_ROLE_CLOSE, frame_win->wid );
	map_add( a_ctx->windows_list, xid, close_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *util_win = aawm_allocate_window( xid, AAWM_ROLE_UTILITY, frame_win->wid );
	map_add( a_ctx->windows_list, xid, util_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *minmax_win = aawm_allocate_window( xid, AAWM_ROLE_MINMAX, frame_win->wid );
	map_add( a_ctx->windows_list, xid, minmax_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *resize_win = aawm_allocate_window( xid, AAWM_ROLE_RESIZE, frame_win->wid );
	map_add( a_ctx->windows_list, xid, resize_win );

	aawm_window_add_child( frame_win, client_win->wid );
	aawm_window_add_child( frame_win, close_win->wid );
	aawm_window_add_child( frame_win, util_win->wid );
	aawm_window_add_child( frame_win, minmax_win->wid );
	aawm_window_add_child( frame_win, resize_win->wid );

	// Read, parse and display properties of the client

	xcb_list_properties_reply_t * props = xcb_list_properties_reply( a_ctx->conn, xcb_list_properties( a_ctx->conn, a_ev->window ), NULL );
	int prop_count =  xcb_list_properties_atoms_length( props );
	printf( "Window 0x%X defines %d properties:\n", a_ev->window, prop_count );
	xcb_atom_t *prop_atoms = xcb_list_properties_atoms( props );
	int i;
	for (i = 0; i < prop_count; i++) {
		xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, xcb_get_atom_name( a_ctx->conn, prop_atoms[i]), NULL );
		char *aname = xcb_get_atom_name_name( reply );
		int aname_len = xcb_get_atom_name_name_length( reply );
		char *namez = malloc( aname_len + 1 );
		namez[aname_len] = 0;
		memcpy( namez, aname, aname_len );
		xcb_get_property_reply_t *prop_reply = xcb_get_property_reply( a_ctx->conn, xcb_get_property( a_ctx->conn, false, a_ev->window, prop_atoms[i], XCB_GET_PROPERTY_TYPE_ANY, 0, 40 ), NULL );
		xcb_get_atom_name_reply_t *type_reply = xcb_get_atom_name_reply( a_ctx->conn, xcb_get_atom_name( a_ctx->conn, prop_reply->type), NULL );
		char *type_name = xcb_get_atom_name_name( type_reply );
		int type_name_len = xcb_get_atom_name_name_length( type_reply );
		char *type_namez = malloc( type_name_len + 1 );
		type_namez[type_name_len] = 0;
		memcpy( type_namez, type_name, type_name_len );
		if (!strcmp( type_namez, "UTF8_STRING" )) {
			utf8_string = prop_reply->type;
		} 
		
		printf( "\t%d %s type %s value length %d - %d", prop_atoms[i], namez, type_namez, xcb_get_property_value_length( prop_reply), prop_reply->value_len );
		switch (prop_reply->type) {
			case XCB_ATOM_STRING:
//			case XCB_ATOM_UTF8_STRING:
				printf( " value \"%.*s\"", prop_reply->value_len, (char*)xcb_get_property_value( prop_reply ) );
				break;
			case XCB_ATOM_WINDOW:
				{
					xcb_window_t* win = (xcb_window_t*) xcb_get_property_value( prop_reply );
					printf( " 0x%X", *win );
				}
				break;
			case XCB_ATOM_ATOM:
				{
					int j;
					xcb_atom_t* atoms = (xcb_atom_t*) xcb_get_property_value( prop_reply );
					for (j=0; j< prop_reply->value_len; j++) {
						xcb_get_atom_name_reply_t *atom_reply = xcb_get_atom_name_reply( a_ctx->conn, xcb_get_atom_name( a_ctx->conn, atoms[j]), NULL );
						char *name = xcb_get_atom_name_name( atom_reply );
						int name_len = xcb_get_atom_name_name_length( atom_reply );
						printf( " %d %.*s", atoms[j], name_len, name );
					}
				}
				break;
			case XCB_ATOM_WM_HINTS:
				{
					xcb_icccm_wm_hints_t* hints = (xcb_icccm_wm_hints_t*) xcb_get_property_value( prop_reply );
					printf( " flags 0x%X input %d initial state %d icon pixmap 0x%X icon window 0x%X icon position [%d,%d] icon mask 0x%X, window group 0x%X", hints->flags, hints->input, hints->initial_state, hints->icon_pixmap, hints->icon_window, hints->icon_x, hints->icon_y, hints->icon_mask, hints->window_group );
				}
				break;
			case XCB_ATOM_WM_SIZE_HINTS:
				{
					xcb_size_hints_t* hints = (xcb_size_hints_t*) xcb_get_property_value( prop_reply );
					printf( " flags 0x%X geom [%d,%d,%d,%d+%d]x[%d,%d,%d,%d+%d]+%d+%d aspect ratio [%d/%d, %d/%d] gravity %d", hints->flags, hints->min_width, hints->base_width, hints->width, hints->max_width, hints->width_inc, hints->min_height, hints->base_height, hints->height, hints->max_height, hints->height_inc, hints->x, hints->y, hints->min_aspect_num, hints->min_aspect_den, hints->max_aspect_num, hints->max_aspect_den, hints->win_gravity );
				}
				break;
			default:
				if (utf8_string != 0 && prop_reply->type == utf8_string) {
					printf( " value \"%.*s\"", prop_reply->value_len, (char*)xcb_get_property_value( prop_reply ) );
					if (!strcmp( namez, "_NET_WM_NAME" )) {
						printf( " - setting _NET_WM_NAME\n" );
						name = (char*)xcb_get_property_value( prop_reply );
						name_len = prop_reply->value_len;
					}
				}
		}
		printf( "\n" );
	}

	uint32_t event_mask = XCB_CW_EVENT_MASK;
	uint32_t event_mask_values = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
		| XCB_EVENT_MASK_ENTER_WINDOW
		| XCB_EVENT_MASK_LEAVE_WINDOW
		| XCB_EVENT_MASK_FOCUS_CHANGE
		| XCB_EVENT_MASK_PROPERTY_CHANGE
		| XCB_EVENT_MASK_BUTTON_PRESS
		| XCB_EVENT_MASK_BUTTON_RELEASE
		| XCB_EVENT_MASK_BUTTON_1_MOTION;

	// used later
	uint32_t mask;
	uint32_t values_m[3];

	uint32_t values[] = { 0x0FF, 0x0FF00 };
	uint32_t values2[] = { 0xFF0000 };
	uint32_t values3[] = { 0xFF0000, XCB_EVENT_MASK_BUTTON_PRESS|XCB_EVENT_MASK_BUTTON_RELEASE };
	xcb_get_geometry_reply_t *wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, a_ev->window ), NULL );

	/*xcb_void_cookie_t cookie =*/ xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, frame_win->wid, a_ctx->screen->root, wgeom->x - 4/*(border_width-1)*/, wgeom->y - 4, wgeom->width + 2 * wgeom->border_width, wgeom->height + 2 * wgeom->border_width + 30, 5 /*border_width*/, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL, values );

	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( a_ctx->conn, frame_win->wid, event_mask, &event_mask_values );
	/*xcb_generic_error_t *error =*/ xcb_request_check( a_ctx->conn, cookie );
	xcb_flush( a_ctx->conn );

	uint32_t prop_test = XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_request_check( a_ctx->conn, xcb_change_window_attributes_checked( a_ctx->conn, client_win->wid, XCB_CW_EVENT_MASK, &prop_test ) );
	xcb_flush( a_ctx->conn );

	xcb_generic_error_t * error;
	xcb_font_t fid = xcb_generate_id( a_ctx->conn );
//	error = xcb_request_check( a_ctx->conn, xcb_open_font_checked( a_ctx->conn, fid, 3, "6x9" ) );
	error = xcb_request_check( a_ctx->conn, xcb_open_font_checked( a_ctx->conn, fid, 56, "-adobe-times-medium-r-normal--0-0-100-100-p-0-iso10646-1" ) );
	if (error) {
		printf( "OpenFont ERROR type %d, code %d\n", error->response_type, error->error_code );
	}

	xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
	uint32_t gc_values[3];
	uint32_t gc_mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
	gc_values[0] = a_ctx->screen->white_pixel;
	gc_values[1] = a_ctx->screen->black_pixel;
	gc_values[2] = fid;
	error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, frame_win->wid, gc_mask, gc_values ) );
	if (error != NULL) {
		printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
	}
//	error = xcb_request_check( a_ctx->conn, xcb_poly_text_8_checked( a_ctx->conn, frame_win->wid, gc, 30, 10, 4, (const unsigned char *)"plop" ) );

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, util_win->wid, frame_win->wid, 0, 0, 19, 19, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values3 );
	xcb_map_window( a_ctx->conn, util_win->wid );

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, minmax_win->wid, frame_win->wid, 20, 0, 19, 19, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, values2 );
	xcb_map_window( a_ctx->conn, minmax_win->wid );

	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	values_m[0] = 0xFF0000;
	values_m[1] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_BUTTON_PRESS | XCB_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_1_MOTION;
	values_m[2] = a_ctx->sizing;

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, resize_win->wid, frame_win->wid, wgeom->width - 19, wgeom->height + 21, 19, 9, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, mask, values_m );
	xcb_map_window( a_ctx->conn, resize_win->wid );

	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	values_m[0] = 0xFF0000;
	values_m[1] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_BUTTON_RELEASE;
	values_m[2] = a_ctx->pirate;

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, close_win->wid, frame_win->wid, wgeom->width - 19, 0, 19, 19, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, mask, values_m );
	xcb_map_window( a_ctx->conn, close_win->wid );

	xcb_reparent_window( a_ctx->conn, client_win->wid, frame_win->wid, 0, 20 );
	xcb_map_window( a_ctx->conn, client_win->wid );
	xcb_map_window( a_ctx->conn, frame_win->wid );
	
	if (name) {
	/*	error = xcb_request_check( a_ctx->conn, xcb_image_text_8_checked( a_ctx->conn, 4, frame_win->wid, gc, 30, 10, "plop" ) );
		if (error) {
			printf( "ImageText8 ERROR type %d, code %d\n", error->response_type, error->error_code );
		}*/
		printf( "Printing _NET_WM_NAME\n" );
		uint16_t *name16;
		ssize_t name16_len = alloc_utf16_from_utf8( name, name_len, &name16 );
		int k;
		char *alt_name = (char*) name16;
		for (k = 0; k < name16_len; k++) {
			char tmp = alt_name[2*k];
			alt_name[2*k] = alt_name[2*k+1];
			alt_name[2*k+1] = tmp;
		}
		for (k = 0; k < name16_len; k++) {
			printf( "0x%04X ", name16[k] );
		}
		printf( "\n" );
//		unsigned char polybuffer[7];
		unsigned char *polybuffer = malloc( 2 * name16_len + 2 );
		polybuffer[0] = name16_len; // 5; // len
		polybuffer[1] = 0; // delta
//		memcpy( &polybuffer[2], "plouf", 5 );
		memcpy( &polybuffer[2], name16, 2 * name16_len );
		error = xcb_request_check( a_ctx->conn, xcb_poly_text_16_checked( a_ctx->conn, frame_win->wid, gc, 45, 15, 2*name16_len+2 /*7*/, polybuffer ) );
		if (error) {
			printf( "PolyText8 ERROR type %d, code %d\n", error->response_type, error->error_code );
		}
	}

	xcb_flush( a_ctx->conn );
}


void configure_window_passthrough( xcb_connection_t* a_conn, xcb_configure_request_event_t *a_ev )
{
	printf( "Requested configuration: %d%sx%d%s+%d%s+%d%s, border %d%s, stacking %d%s to sibling %d%s\n", a_ev->width, (a_ev->value_mask & 4) ? "!" : "", a_ev->height, (a_ev->value_mask & 8) ? "!" : "", a_ev->x, (a_ev->value_mask & 1) ? "!" : "", a_ev->y, (a_ev->value_mask & 2) ? "!" : "", a_ev->border_width, (a_ev->value_mask & 16) ? "!" : "", a_ev->stack_mode, (a_ev->value_mask & 64) ? "!" : "", a_ev->sibling, (a_ev->value_mask & 32) ? "!" : "" );

	int value_count = 0;
	int count_mask;
	for (count_mask = 1; count_mask < 256; count_mask <<= 1) {
		if (a_ev->value_mask & count_mask) value_count++;
	}

	uint32_t *values = malloc( value_count * sizeof( uint32_t ) );

	int idx = 0;
	if (a_ev->value_mask & 1) values[idx++] = a_ev->x;
	if (a_ev->value_mask & 2) values[idx++] = a_ev->y;
	if (a_ev->value_mask & 4) values[idx++] = a_ev->width;
	if (a_ev->value_mask & 8) values[idx++] = a_ev->height;
	if (a_ev->value_mask & 16) values[idx++] = a_ev->border_width;
	if (a_ev->value_mask & 32) values[idx++] = a_ev->sibling;
	if (a_ev->value_mask & 64) values[idx++] = a_ev->stack_mode;

	xcb_configure_window( a_conn, a_ev->window, a_ev->value_mask, values );
	xcb_flush( a_conn );
	free( values );
}


void resize_request_passthrough( xcb_connection_t* a_conn, xcb_resize_request_event_t *a_ev, xcb_screen_t *a_screen )
{
	uint32_t values[] = { a_ev->width, a_ev->height };
	xcb_configure_window( a_conn, a_screen->root, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values );
	xcb_flush( a_conn );
}


void create_cursors( struct aawm_ctx *a_ctx )
{
	a_ctx->cursor_font = xcb_generate_id( a_ctx->conn );
	xcb_open_font( a_ctx->conn, a_ctx->cursor_font, strlen( "cursor" ), "cursor" );

	a_ctx->fleur = xcb_generate_id( a_ctx->conn );
	xcb_create_glyph_cursor( a_ctx->conn, a_ctx->fleur, a_ctx->cursor_font, a_ctx->cursor_font, 53, 52, 0,0,0,0,0,0 );

	a_ctx->pirate = xcb_generate_id( a_ctx->conn );
	xcb_create_glyph_cursor( a_ctx->conn, a_ctx->pirate, a_ctx->cursor_font, a_ctx->cursor_font, 89, 88, 0,0,0,0,0,0 );

	a_ctx->sizing = xcb_generate_id( a_ctx->conn );
	xcb_create_glyph_cursor( a_ctx->conn, a_ctx->sizing, a_ctx->cursor_font, a_ctx->cursor_font, 121, 120, 0,0,0,0,0,0 );

	xcb_flush( a_ctx->conn );
}


void events( struct aawm_ctx *a_ctx )
{
	while (1) {
		xcb_generic_event_t *ev = xcb_wait_for_event( a_ctx->conn );
		switch (ev->response_type & ~0x80) {

			case 0:
			{
				xcb_generic_error_t *e = (xcb_generic_error_t *) ev;
				printf( "Received ERROR%s %d\n", (e->response_type & 0x80) ? " (S)" : "", e->error_code );
			}
			break;

			// Substructure redirect mask

			case XCB_MAP_REQUEST:
			{
				xcb_map_request_event_t *e = (xcb_map_request_event_t *) ev;
				printf( "Received map request%s from 0x%X for 0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window );
//				map_request_passthrough( a_ctx, e );
				map_request_reparent( a_ctx, e );
			}
			break;

			case XCB_CONFIGURE_REQUEST:
			{
				xcb_configure_request_event_t *e = (xcb_configure_request_event_t *) ev;
				printf( "Received configure request%s from 0x%X for 0x%X, %dx%d+%d+%d, mask 0x%04x\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window, e->width, e->height, e->x, e->y, e->value_mask );
				configure_window_passthrough( a_ctx->conn, e );
			}
			break;

			case XCB_CIRCULATE_REQUEST:
			{
				xcb_circulate_request_event_t *e = (xcb_circulate_request_event_t *)ev;
				printf("Received circulate request%s from 0x%X for 0x%X, ignoring\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window );
			}
			break;

			// (Sub)Structure notify mask

			case XCB_CONFIGURE_NOTIFY:
			{
				xcb_configure_notify_event_t *e = (xcb_configure_notify_event_t *) ev;
				printf( "Received configure notify%s from 0x%X for 0x%X, %dx%d+%d+%d, above_sibling=0x%X, border_width=%d, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->width, e->height, e->x, e->y, e->above_sibling, e->border_width, e->override_redirect );
			}
			break;

			case XCB_CREATE_NOTIFY:
			{
				xcb_create_notify_event_t *e = (xcb_create_notify_event_t *) ev;
				printf( "Received create notify%s from 0x%X for 0x%X, %dx%d+%d+%d, border_width=%d, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window, e->width, e->height, e->x, e->y, e->border_width, e->override_redirect );
			}
			break;

			case XCB_DESTROY_NOTIFY:
			{
				xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
				printf( "Received destroy notify%s from 0x%X for 0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window );
			}
			break;

			case XCB_MAP_NOTIFY:
			{
				xcb_map_notify_event_t *e = (xcb_map_notify_event_t *) ev;
				printf( "Received map notify%s from 0x%X for 0x%X, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->override_redirect );
			}
			break;

			case XCB_REPARENT_NOTIFY:
			{
				xcb_reparent_notify_event_t *e = (xcb_reparent_notify_event_t *) ev;
				printf( "Received reparent notify%s from 0x%X for 0x%X to be reparented by 0x%X at +%d+%d, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->parent, e->x, e->y, e->override_redirect );
			}
			break;

			case XCB_UNMAP_NOTIFY:
			{
				// We receive synthesized of these ones even with only XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT in the mask. Is it because XSendEvent is targeted ?
				xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *) ev;
				printf( "Received unmap notify%s from 0x%X for 0x%X from_configure(%d)\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->from_configure );
			}
			break;

			// ResizeRedirectMask

			case XCB_RESIZE_REQUEST:
			{
				xcb_resize_request_event_t *e = (xcb_resize_request_event_t *) ev;
				printf( "Reveived resize request%s from 0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->window );
				resize_request_passthrough( a_ctx->conn, e, a_ctx->screen );
			}
			break;

			// FocusChangeMask

			case XCB_FOCUS_IN:
			{
				xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) ev;
				printf( "Received focus in%s for 0x%X, detail %s, mode %d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, get_detail_string( e->detail ), e->mode );
			}
			break;

			case XCB_FOCUS_OUT:
			{
				xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) ev;
				printf( "Received focus out%s for 0x%X, detail %s, mode %d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, get_detail_string( e->detail ), e->mode );
			}
			break;

			// PropertyChangeMask

			case XCB_PROPERTY_NOTIFY:
			{
				xcb_property_notify_event_t *e = (xcb_property_notify_event_t *) ev;
				printf( "Received property notify%s for 0x%X, atom %d, state %d, at time %d\n", (e->response_type & 0x80) ? " (S)" : "", e->window, e->atom, e->state, e->time );
			}
			break;

			// EnterWindowMask

			case XCB_ENTER_NOTIFY:
			{
				xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) ev;
				printf( "Received enter notify%s for 0x%X (+%d+%d), child 0x%X, root 0x%X (+%d+%d) at time %d, detail %s, mode %d, same_screen_focus=0x%X, state=0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->child, e->root, e->root_x, e->root_y, e->time, get_detail_string( e->detail ), e->mode, e->same_screen_focus, e->state );
				xcb_get_input_focus_reply_t *focus = xcb_get_input_focus_reply( a_ctx->conn, xcb_get_input_focus( a_ctx->conn ), NULL );
				printf( "Focus currently on %d, revert to %d\n", focus->focus, focus->revert_to );
				
				if (e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
					uint32_t mask = XCB_CW_BORDER_PIXEL;
					uint32_t value = 0xFFFFFF;
					xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value );
					/*xcb_generic_error_t *error =*/ xcb_request_check( a_ctx->conn, cookie );
					value = XCB_STACK_MODE_ABOVE;
					xcb_configure_window( a_ctx->conn, e->event, XCB_CONFIG_WINDOW_STACK_MODE, &value );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// LeaveWindowMask

			case XCB_LEAVE_NOTIFY:
			{
				xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) ev;
				printf( "Received leave notify%s for 0x%X (+%d+%d), child 0x%X, root 0x%X (+%d+%d) at time %d, detail %s, mode %d, same_screen_focus=0x%X, state=0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->child, e->root, e->root_x, e->root_y, e->time, get_detail_string( e->detail ), e->mode, e->same_screen_focus, e->state );
				if (e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
					uint32_t mask = XCB_CW_BORDER_PIXEL;
					uint32_t value = 0xFF00;
					xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value );
					/*xcb_generic_error_t *error =*/ xcb_request_check( a_ctx->conn, cookie );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// ButtonPressMask

			case XCB_BUTTON_PRESS:
			{
				xcb_button_press_event_t *e = (xcb_button_press_event_t *) ev;
				printf( "Received button press%s from 0x%X (+%d+%d), with root 0x%X (+%d+%d) and child 0x%X, at time %d, button %d, state=%d, same_screen=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->root, e->root_x, e->root_y, e->child, e->time, e->detail, e->state, e->same_screen );
				aawm_window_t *win = map_lookup( a_ctx->windows_list, e->event );
				if (win) {
					switch (win->role) {
					case AAWM_ROLE_CLIENT: printf( "Button press in client window\n" ); break;
					case AAWM_ROLE_ROOT: printf( "Button press in root window\n" ); break;
					case AAWM_ROLE_FRAME: printf( "Button press in frame window\n" ); break;
					case AAWM_ROLE_CLOSE: printf( "Button press in close gadget\n" ); break;
					case AAWM_ROLE_UTILITY: printf( "Button press in utility gadget\n" ); break;
					case AAWM_ROLE_MINMAX: printf( "Button press in minmax gadget\n" ); break;
					case AAWM_ROLE_RESIZE: printf( "Button press in resize gadget\n" ); break;
					default: printf( "Button press in unknown type of window\n" );
					}
				} else {
					printf( "Button press in unknown window\n" );
					debug_map_list( a_ctx->windows_list );
				}
				if (e->detail == 1) {
					if (win && (win->role == AAWM_ROLE_RESIZE || win->role == AAWM_ROLE_FRAME)) {
						a_ctx->motion_origin_x = e->root_x;
						a_ctx->motion_origin_y = e->root_y;
						xcb_get_geometry_reply_t *wgeom;
						if (win && win->role == AAWM_ROLE_RESIZE) {
							aawm_window_t *frame_win = map_lookup( a_ctx->windows_list, win->parent );
							if (frame_win && frame_win->role == AAWM_ROLE_FRAME) {
								wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, win->parent ), NULL );
							} else {
								printf( "Couldn't find frame window, crash imminent\n" );
							}
						} else {
							wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, e->event ), NULL );
						}
						a_ctx->window_origin_x = wgeom->x;
						a_ctx->window_origin_y = wgeom->y;
						a_ctx->window_origin_width = wgeom->width;
						a_ctx->window_origin_height = wgeom->height;
						// e->event_x and e->event_y are useless for resize because relative to the gadget, not the frame window
						a_ctx->motion_rel_x = e->root_x - wgeom->x;
						a_ctx->motion_rel_y = e->root_y - wgeom->y;
						// Need to take border into account for these offsets
						a_ctx->window_width_offset = wgeom->x + wgeom->width - e->root_x;
						a_ctx->window_height_offset = wgeom->y + wgeom->height - e->root_y;
						printf( "Relative coordinates: (%d,%d), to the bottom-right corner: (%d,%d)\n", a_ctx->motion_rel_x, a_ctx->motion_rel_y, a_ctx->window_width_offset, a_ctx->window_height_offset );
					} else if (win && win->role == AAWM_ROLE_UTILITY) {
						xcb_get_geometry_reply_t *wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, e->event ), NULL );
						aawm_window_t *win = map_lookup( a_ctx->windows_list, e->event );
						xcb_get_geometry_reply_t *pgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, win->parent ), NULL );

						xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
						xcb_pixmap_t circle_pix = xcb_generate_id( a_ctx->conn );
						xcb_window_t menu = xcb_generate_id( a_ctx->conn );
						int pix_value = 0xffff00;
						xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, menu, a_ctx->screen->root, (pgeom->x + wgeom->x + wgeom->width / 2) - 50, (pgeom->y + wgeom->y + wgeom->height / 2) - 50, 100, 100, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, &pix_value );
						xcb_generic_error_t * error = xcb_request_check( a_ctx->conn, xcb_create_pixmap_checked( a_ctx->conn, 1, circle_pix, a_ctx->screen->root, 100, 100 ) );
						if (error != NULL) {
							printf( "CreatePixmap ERROR type %d, code %d\n", error->response_type, error->error_code );
						}
						error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, circle_pix /*a_ctx->screen->root*/, 0, NULL ) );
						if (error != NULL) {
							printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
						}
						int gc_value = 0;
						xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
						xcb_rectangle_t rect = { 0, 0, 100, 100 };
						xcb_poly_fill_rectangle( a_ctx->conn, circle_pix, gc, 1, &rect );
						gc_value = 1;
						xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
						xcb_arc_t arc = { 0, 0, 100, 100, 0, 360*64 };
						xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc );
						gc_value = 0;
						xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
						xcb_arc_t arc2 = { 40, 40, 20, 20, 0, 360*64 };
						xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc2 );
						
						xcb_shape_mask( a_ctx->conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, menu, 0, 0, circle_pix );
						xcb_map_window( a_ctx->conn, menu );
						xcb_flush( a_ctx->conn );
					}
				} else if (e->detail == 3) {
					xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
					xcb_pixmap_t circle_pix = xcb_generate_id( a_ctx->conn );
					xcb_window_t menu = xcb_generate_id( a_ctx->conn );
					int pix_value = 0xffff00;
					xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, menu, a_ctx->screen->root, 200, 200, 100, 100, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, &pix_value );
					xcb_generic_error_t * error = xcb_request_check( a_ctx->conn, xcb_create_pixmap_checked( a_ctx->conn, 1, circle_pix, a_ctx->screen->root, 100, 100 ) );
					if (error != NULL) {
						printf( "CreatePixmap ERROR type %d, code %d\n", error->response_type, error->error_code );
					}
					error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, circle_pix /*a_ctx->screen->root*/, 0, NULL ) );
					if (error != NULL) {
						printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
					}
					int gc_value = 0;
					xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
					xcb_rectangle_t rect = { 0, 0, 100, 100 };
					xcb_poly_fill_rectangle( a_ctx->conn, circle_pix, gc, 1, &rect );
					gc_value = 1;
					xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
					xcb_arc_t arc = { 0, 0, 100, 100, 0, 360*64 };
					xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc );
					gc_value = 0;
					xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
					xcb_arc_t arc2 = { 40, 40, 20, 20, 0, 360*64 };
					xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc2 );
					
//					error = xcb_request_check( a_ctx->conn, xcb_put_image_checked( a_ctx->conn, XCB_IMAGE_FORMAT_XY_BITMAP, circle_pix, gc, 96 /*100*/, 96 /*100*/, 0, 0, 0, 1, 1152 /*1300*/ /*sizeof( circle_menu_bits )*/, circle_menu_bits ) );
/*					if (error != NULL) {
						printf( "PutImage ERROR type %d, code %d\n", error->response_type, error->error_code );
					}*/
					xcb_shape_mask( a_ctx->conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, menu, 0, 0, circle_pix );
					xcb_map_window( a_ctx->conn, menu );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// ButtonReleaseMask

			case XCB_BUTTON_RELEASE:
			{
				xcb_button_press_event_t *e = (xcb_button_press_event_t *) ev;
				printf( "Received button release%s from 0x%X (+%d+%d), with root 0x%X (+%d+%d) and child 0x%X, at time %d, button %d, state=%d, same_screen=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->root, e->root_x, e->root_y, e->child, e->time, e->detail, e->state, e->same_screen );

				if (a_ctx->moving) {
					a_ctx->moving = false;
					// then unset pointer
					uint32_t mask = XCB_CW_CURSOR;
					uint32_t value = 0; // None
					xcb_request_check( a_ctx->conn, xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value ) );
				}
			}
			break;

			// Button1MotionMask

			case XCB_MOTION_NOTIFY:
			{
				xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *) ev;
				printf( "Received motion notify%s from 0x%X (+%d+%d), with root 0x%X (+%d+%d) and child 0x%X, at time %d, is_hint %d, state=%d, same_screen=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->root, e->root_x, e->root_y, e->child, e->time, e->detail, e->state, e->same_screen );
				aawm_window_t *win = map_lookup( a_ctx->windows_list, e->event );
				if (win) {
					switch (win->role) {
					case AAWM_ROLE_CLIENT: printf( "Motion in client window\n" ); break;
					case AAWM_ROLE_ROOT: printf( "Motion in root window\n" ); break;
					case AAWM_ROLE_FRAME: printf( "Motion in frame window\n" ); break;
					case AAWM_ROLE_CLOSE: printf( "Motion in close gadget\n" ); break;
					case AAWM_ROLE_UTILITY: printf( "Motion in utility gadget\n" ); break;
					case AAWM_ROLE_MINMAX: printf( "Motion in minmax gadget\n" ); break;
					case AAWM_ROLE_RESIZE: printf( "Motion in resize gadget\n" ); break;
					default: printf( "Motion in unknown type of window\n" );
					}
				} else {
					printf( "Motion in unknown window\n" );
					debug_map_list( a_ctx->windows_list );
				}

				if (win && win->role == AAWM_ROLE_FRAME) {
					if (!a_ctx->moving) {
						a_ctx->moving = true;
						uint32_t mask = XCB_CW_CURSOR;
						uint32_t value = a_ctx->fleur;
						xcb_request_check( a_ctx->conn, xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value ) );
					}

					uint32_t values[] = { a_ctx->window_origin_x + e->root_x - a_ctx->motion_origin_x , a_ctx->window_origin_y + e->root_y - a_ctx->motion_origin_y };
					xcb_configure_window( a_ctx->conn, e->event, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values );
					xcb_flush( a_ctx->conn );
				} else if (win && win->role == AAWM_ROLE_RESIZE) {
					aawm_window_t *parent = map_lookup( a_ctx->windows_list, win->parent );
					int i;
					int found = 0;
					aawm_window_t *temp, *close, *client;
					for (i = 0; found != 2 && i < parent->children_count; i++) {
						temp = map_lookup( a_ctx->windows_list, parent->children[i] );
						if (temp->role == AAWM_ROLE_CLOSE) { close = temp; found++; }
						if (temp->role == AAWM_ROLE_CLIENT) { client = temp; found++; }
					}
					if (found) {
						printf( "Close found at index %d, ID 0x%X / 0x%X\n", i, parent->children[i], close->wid );
					} else {
						printf( "Close not found!\n" );
					}
					// wgeom->width + 2 * wgeom->border_width, wgeom->height + 2 * wgeom->border_width + 30
					uint32_t values1[] = { e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x, e->root_y + a_ctx->window_height_offset - a_ctx->window_origin_y };
					uint32_t values2[] = { e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x - 19, e->root_y + a_ctx->window_height_offset - a_ctx->window_origin_y - 9 };
					uint32_t value3 = e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x - 19;
					uint32_t values4[] = { e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x, e->root_y + a_ctx->window_height_offset - a_ctx->window_origin_y - 30 };
					xcb_configure_window( a_ctx->conn, win->parent, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values1 );
					xcb_configure_window( a_ctx->conn, e->event, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values2 );
					xcb_configure_window( a_ctx->conn, close->wid, XCB_CONFIG_WINDOW_X, &value3 );
					xcb_configure_window( a_ctx->conn, client->wid, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values4 );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// Other: These have no corresponding masks, so we receive them in any case.

			case XCB_CLIENT_MESSAGE:
			{
				xcb_client_message_event_t *e = (xcb_client_message_event_t *) ev;
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_ctx->conn, e->type );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *name = xcb_get_atom_name_name( reply );
				printf( "Received client message%s from 0x%X, atom %s format %d\n", (e->response_type & 0x80) ? " (S)" : "", e->window, name, e->format );

				const char* action = e->data.data32[0] == 0 ? "_NET_WM_STATE_REMOVE" :  e->data.data32[0] == 1 ? "_NET_WM_STATE_ADD" : e->data.data32[0] == 2 ? "_NET_WM_STATE_TOGGLE" : "<unknown>";

				cookie = xcb_get_atom_name( a_ctx->conn, e->data.data32[1] );
				reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *prop1 = name = xcb_get_atom_name_name( reply );

				char *prop2;
				if (e->data.data32[2]) {
					xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_ctx->conn, e->data.data32[2] );
					xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
					prop2 = name = xcb_get_atom_name_name( reply );
				} else {
					prop2 = "(none)";
				}

				const char* source = e->data.data32[3] == 0 ? "unspecified" :  e->data.data32[3] == 1 ? "application" : e->data.data32[3] == 2 ? "direct user action" : "<unknown>";

				printf( "\taction: %s, property 1: %s, property 2: %s, source: %s\n", action, prop1, prop2, source );

			}
			break;

			case XCB_MAPPING_NOTIFY:
			{
				xcb_mapping_notify_event_t* e = (xcb_mapping_notify_event_t*) ev;
				const char* reqtype = e->request == 0 ? "MappingModifier" : e->request == 1 ? "MappingKeyboard" : e->request == 2 ? "MappingPointer" : "<unsupported>";
				printf( "Received mapping notify%s, request type %s\n", (e->response_type & 0x80) ? " (S)" : "", reqtype );
				if (e->request == 1) {
					printf( "\tFirst keycode %d, count %d\n", e->first_keycode, e->count );
				}
			}
			break;

			case XCB_SELECTION_CLEAR:
			{
				xcb_selection_clear_event_t* e = (xcb_selection_clear_event_t*) ev;
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_ctx->conn, e->selection );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *name = xcb_get_atom_name_name( reply );
				printf( "Received selection clear%s from 0x%X, at time %d, atom %s\n", (e->response_type & 0x80) ? " (S)" : "", e->owner, e->time, name );
			}
			break;

			case XCB_SELECTION_NOTIFY:
			{
				xcb_selection_notify_event_t* e = (xcb_selection_notify_event_t *) ev;
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_ctx->conn, e->selection );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *selection_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_ctx->conn, e->target );
				reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *type_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_ctx->conn, e->property );
				reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *prop_name = xcb_get_atom_name_name( reply );
				printf( "Received selection notify%s from 0x%X, at time %d, selection %s, target type %s, in property %s\n", (e->response_type & 0x80) ? " (S)" : "", e->requestor, e->time, selection_name, type_name, prop_name );
			}
			break;

			case XCB_SELECTION_REQUEST:
			{
				xcb_selection_request_event_t* e = (xcb_selection_request_event_t *) ev;
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_ctx->conn, e->selection );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *selection_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_ctx->conn, e->target );
				reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *type_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_ctx->conn, e->property );
				reply = xcb_get_atom_name_reply( a_ctx->conn, cookie, NULL );
				char *prop_name = xcb_get_atom_name_name( reply );
				printf( "Received selection request%s from 0x%X for 0x%X, at time %d, selection %s, target type %s, in property %s\n", (e->response_type & 0x80) ? " (S)" : "", e->requestor, e->owner, e->time, selection_name, type_name, prop_name );
			}
			break;

			// Unhandled

			default:
				printf( "Received unhandled event type %s%d\n", (ev->response_type & 0x80) ? "S" : "", ev->response_type & ~0x80 );
			break;
		}

		free( ev );
	}
}


int main()
{
	struct aawm_ctx ctx;
	ctx.windows_list = map_create();
	
	int scrno = 0;
	ctx.conn = xcb_connect( NULL, &scrno );

	printf("screen no set to %d\n", scrno );

	/* Find our screen. */
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator( xcb_get_setup( ctx.conn ) );
	for (int i = 0; i < scrno; ++ i)
	{
		xcb_screen_next( &iter );
	}

	ctx.screen = iter.data;
	xcb_drawable_t root = ctx.screen->root;
	printf( "Screen size: %dx%d\nRoot window: 0x%X\n", ctx.screen->width_in_pixels, ctx.screen->height_in_pixels, ctx.screen->root );

	setupscreen( ctx.conn, root );
	setupshape( &ctx );

	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[2];

	/*	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT: equivalent to Xlib's SubstructureRedirectMask,
			covers events CirculateRequest (Z order),
			ConfigureRequest (size, position, border, Z order)
			and MapRequest (request to actually display a window)
		XCB_EVENT_MASK_STRUCTURE_NOTIFY: equivalent to Xlib's StructureNotifyMask,
			covers events CirculateNotify, ConfigureNotify, DestroyNotify, GravityNotify,
			MapNotify, ReparentNotify and UnmapNotify and applies to the target window.
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY: equivalent to Xlib's SubstructureNotifyMask,
			covers event CreateNotify in addition to StructureNotifyMask ones,
			and applies to the subwindows of the target window.
	*/

	values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		| XCB_EVENT_MASK_STRUCTURE_NOTIFY
		| XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
//		| XCB_EVENT_MASK_ENTER_WINDOW
//		| XCB_EVENT_MASK_LEAVE_WINDOW
		| XCB_EVENT_MASK_FOCUS_CHANGE;

	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( ctx.conn, root, mask, values );
	xcb_generic_error_t *error = xcb_request_check( ctx.conn, cookie );
	xcb_flush( ctx.conn );

	if (NULL != error) {
		printf( "Request failed.\n" );
	}

	create_cursors( &ctx );
	events( &ctx );

	xcb_disconnect( ctx.conn );
	return 0;
}

