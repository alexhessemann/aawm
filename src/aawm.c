#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>

int setupscreen( xcb_connection_t *a_conn, xcb_drawable_t a_root )
{
	int i;
	xcb_get_window_attributes_reply_t *attr;

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


void map_request_passthrough( xcb_connection_t* a_conn, xcb_map_request_event_t *a_ev )
{
	xcb_map_window( a_conn, a_ev->window );
	xcb_flush( a_conn );
}

void map_request_reparent( xcb_connection_t* a_conn, xcb_map_request_event_t *a_ev, xcb_screen_t *a_screen )
{
	uint32_t values[] = { 0x0FF, 0x0FF00 };
	uint32_t values2[] = { 0xFF0000 };
	xcb_window_t wid = xcb_generate_id( a_conn );
	xcb_get_geometry_reply_t *wgeom = xcb_get_geometry_reply( a_conn, xcb_get_geometry( a_conn, a_ev->window ), NULL );
	/*xcb_void_cookie_t cookie =*/ xcb_create_window( a_conn, XCB_COPY_FROM_PARENT, wid, a_screen->root, wgeom->x, wgeom->y, wgeom->width + 2 * wgeom->border_width, wgeom->height + 2 * wgeom->border_width + 15, 5 /*border_width*/, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL, values );

	xcb_window_t bid = xcb_generate_id( a_conn );
	xcb_create_window( a_conn, XCB_COPY_FROM_PARENT, bid, wid, 0, 0, 9, 9, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, values2 );
	xcb_map_window( a_conn, bid );

	bid = xcb_generate_id( a_conn );
	xcb_create_window( a_conn, XCB_COPY_FROM_PARENT, bid, wid, 10, 0, 9, 9, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, values2 );
	xcb_map_window( a_conn, bid );

	bid = xcb_generate_id( a_conn );
	xcb_create_window( a_conn, XCB_COPY_FROM_PARENT, bid, wid, wgeom->width - 9, 0, 9, 9, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, values2 );
	xcb_map_window( a_conn, bid );

	xcb_reparent_window( a_conn, a_ev->window, wid, 0, 10 );
	xcb_map_window( a_conn, a_ev->window );
	xcb_map_window( a_conn, wid );
	
	xcb_flush( a_conn );
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


void events( xcb_connection_t* a_conn, xcb_screen_t *a_screen )
{
	while (1) {
		xcb_generic_event_t *ev = xcb_wait_for_event( a_conn );
		switch (ev->response_type & ~0x80) {

			// Substructure redirect mask

			case XCB_MAP_REQUEST:
			{
				xcb_map_request_event_t *e = (xcb_map_request_event_t *) ev;
				printf( "Received map request%s from 0x%X for 0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window );
//				map_request_passthrough( a_conn, e );
				map_request_reparent( a_conn, e, a_screen );
			}
			break;

			case XCB_CONFIGURE_REQUEST:
			{
				xcb_configure_request_event_t *e = (xcb_configure_request_event_t *) ev;
				printf( "Received configure request%s from 0x%X for 0x%X, %dx%d+%d+%d, mask 0x%04x\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window, e->width, e->height, e->x, e->y, e->value_mask );
				configure_window_passthrough( a_conn, e );
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
				resize_request_passthrough( a_conn, e, a_screen );
			}
			break;

			// FocusChangeMask

			case XCB_FOCUS_IN:
			{
				xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) ev;
				printf( "Received focus in%s for 0x%X, detail %d, mode %d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->detail, e->mode );
			}
			break;

			case XCB_FOCUS_OUT:
			{
				xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) ev;
				printf( "Received focus out%s for 0x%X, detail %d, mode %d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->detail, e->mode );
			}
			break;

			// EnterWindowMask

			case XCB_ENTER_NOTIFY:
			{
				xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) ev;
				printf( "Received enter notify%s for 0x%X (+%d+%d), child 0x%X, root 0x%X (+%d+%d) at time %d, detail %d, mode %d, same_screen_focus=0x%X, state=0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->child, e->root, e->root_x, e->root_y, e->time, e->detail, e->mode, e->same_screen_focus, e->state );
			}
			break;

			// LeaveWindowMask

			case XCB_LEAVE_NOTIFY:
			{
				xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) ev;
				printf( "Received leave notify%s for 0x%X (+%d+%d), child 0x%X, root 0x%X (+%d+%d) at time %d, detail %d, mode %d, same_screen_focus=0x%X, state=0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->child, e->root, e->root_x, e->root_y, e->time, e->detail, e->mode, e->same_screen_focus, e->state );
			}
			break;

			// Other: These have no corresponding masks, so we receive them in any case.

			case XCB_CLIENT_MESSAGE:
			{
				xcb_client_message_event_t *e = (xcb_client_message_event_t *) ev;
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_conn, e->type );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *name = xcb_get_atom_name_name( reply );
				printf( "Received client message%s from 0x%X, atom %s format %d\n", (e->response_type & 0x80) ? " (S)" : "", e->window, name, e->format );

				const char* action = e->data.data32[0] == 0 ? "_NET_WM_STATE_REMOVE" :  e->data.data32[0] == 1 ? "_NET_WM_STATE_ADD" : e->data.data32[0] == 2 ? "_NET_WM_STATE_TOGGLE" : "<unknown>";

				cookie = xcb_get_atom_name( a_conn, e->data.data32[1] );
				reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *prop1 = name = xcb_get_atom_name_name( reply );

				char *prop2;
				if (e->data.data32[2]) {
					xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_conn, e->data.data32[2] );
					xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
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
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_conn, e->selection );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *name = xcb_get_atom_name_name( reply );
				printf( "Received selection clear%s from 0x%X, at time %d, atom %s\n", (e->response_type & 0x80) ? " (S)" : "", e->owner, e->time, name );
			}
			break;

			case XCB_SELECTION_NOTIFY:
			{
				xcb_selection_notify_event_t* e = (xcb_selection_notify_event_t *) ev;
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_conn, e->selection );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *selection_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_conn, e->target );
				reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *type_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_conn, e->property );
				reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *prop_name = xcb_get_atom_name_name( reply );
				printf( "Received selection notify%s from 0x%X, at time %d, selection %s, target type %s, in property %s\n", (e->response_type & 0x80) ? " (S)" : "", e->requestor, e->time, selection_name, type_name, prop_name );
			}
			break;

			case XCB_SELECTION_REQUEST:
			{
				xcb_selection_request_event_t* e = (xcb_selection_request_event_t *) ev;
				xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name( a_conn, e->selection );
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *selection_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_conn, e->target );
				reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
				char *type_name = xcb_get_atom_name_name( reply );
				cookie = xcb_get_atom_name( a_conn, e->property );
				reply = xcb_get_atom_name_reply( a_conn, cookie, NULL );
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
	int scrno = 0;
	xcb_connection_t *conn = xcb_connect( NULL, &scrno );

	printf("screen no set to %d\n", scrno );

	/* Find our screen. */
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(conn));
	for (int i = 0; i < scrno; ++ i)
	{
		xcb_screen_next(&iter);
	}

	xcb_screen_t *screen = iter.data;
	xcb_drawable_t root = screen->root;
	printf( "Screen size: %dx%d\nRoot window: %d\n", screen->width_in_pixels, screen->height_in_pixels, screen->root );

	setupscreen( conn, root );

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
		| XCB_EVENT_MASK_ENTER_WINDOW
		| XCB_EVENT_MASK_LEAVE_WINDOW
		| XCB_EVENT_MASK_FOCUS_CHANGE;

	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( conn, root, mask, values );
	xcb_generic_error_t *error = xcb_request_check(conn, cookie);

	xcb_flush( conn );

	if (NULL != error) {
		printf( "Request failed.\n" );
	}

	events( conn, screen );

	xcb_disconnect( conn );
	return 0;
}

