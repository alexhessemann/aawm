#include "get_constant_string.h"

const char* get_detail_string( xcb_notify_detail_t a_detail )
{
	const char *result;

	switch (a_detail) {
		case XCB_NOTIFY_DETAIL_ANCESTOR: result = "ANCESTOR"; break;
		case XCB_NOTIFY_DETAIL_VIRTUAL: result = "VIRTUAL"; break;
		case XCB_NOTIFY_DETAIL_INFERIOR: result = "INFERIOR"; break;
		case XCB_NOTIFY_DETAIL_NONLINEAR: result = "NONLINEAR"; break;
		case XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL: result = "NONLINEAR_VIRTUAL"; break;
		case XCB_NOTIFY_DETAIL_POINTER: result = "POINTER"; break;
		case XCB_NOTIFY_DETAIL_POINTER_ROOT: result = "POINTER_ROOT"; break;
		case XCB_NOTIFY_DETAIL_NONE: result = "NONE"; break;
		default: result = NULL;
	}
	return result;
}

