#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


/*	Converts a Unicode codepoint to the equivalent UTF-16 sequence.
	At least two 16-bit slots must be available in utf16
	Errors:
	-1: Invalid UTF-32 (surrogate)
	-2: Invalid UTF-32 (more than 21 bits)
	TODO: add flags for UCS-2
*/
ssize_t utf32_to_utf16( uint32_t utf32, uint16_t* utf16 )
{
	ssize_t result = 0;
	uint16_t *next_code = utf16;

	if (utf32 >= 0xD800 && utf32 < 0xE000) {
		result = -1;
	} else if (utf32 < 0x10000) {
		*next_code = utf32 & 0xFFFF;
		result = 1;
	} else if (utf32 >= 0x110000 /*0x200000*/) {
		result = -2;
	} else {
		utf32 -= 0x10000;
		*next_code++ = 0xD800 | ((utf32 >> 10) & 0x3FF); // bits 19-10
		*next_code = 0xDC00 | (utf32 & 0x3FF); // bits 9-0
		result = 2;
	}
	return result;
}


/* Converts an UTF-32 codepoint to its UTF-8 sequence.
	Returns the number of bytes written.
		TODO: Fail on invalid codepoints (those reserved for surrogates, and > U+10FFFF
 */
size_t utf32_to_utf8(unsigned int utf32, unsigned char* utf8)
{
	if (! (utf32 & 0xFFFFFF80) ) {
		*utf8 = utf32;
		return 1;
	}

	if (! (utf32 & 0xFFFFF800) ) {
		*utf8++ = 0xC0 | (utf32 >> 6);
		*utf8   = 0x80 | (utf32 & 0x0000003F);
		return 2;
	}

	if (! (utf32 & 0xFFFF0000) ) {
		*utf8++ = 0xE0 | (utf32 >> 12);
		*utf8++ = 0x80 | ((utf32 & 0x00000FC0) >> 6);
		*utf8   = 0x80 |  (utf32 & 0x0000003F);
		return 3;
	}

	*utf8++ = 0xF0 | (utf32 >> 18);
	*utf8++ = 0x80 | ((utf32 & 0x0003F000) >> 12);
	*utf8++ = 0x80 | ((utf32 & 0x00000FC0) >>  6);
	*utf8   = 0x80 |  (utf32 & 0x0000003F);
	return 4;
}


// TODO: UTF-16 (and UCS-2) to UTF-8


/*	Converts UTF-16 code units to UTF-32 codepoints, one UTF-16 code unit at a time.
	Returns:
		 0 if a_utf16 is a high surrogate (and fills a_utf32 with it)
			TODO: should fail when there are two consecutive high surrogates, or a high surrogate not followed by a low surrogate.
		 1 if a_utf16 is an independant codepoint
		 2 if a_utf16 is a low surrogate (and a_utf32 contained a high surrogate)
		-1 if a_utf16 is a low surrogate (and a_utf32 did not contain a high surrogate)
	NOTA: Valid UCS-2 strings always return 1.
*/
size_t utf16_to_utf32(unsigned short a_utf16, unsigned int* a_utf32)
{
	if (a_utf16 >= 0xD800 && a_utf16 <= 0xDBFF) {
		*a_utf32 = a_utf16;
		return 0;
	}

	if (a_utf16 >= 0xDC00 && a_utf16 <= 0xDFFF) {
		if (*a_utf32 >= 0xD800 && *a_utf32 <= 0xDBFF) {
			*a_utf32 = ((*a_utf32) << 10) + a_utf16
				+ 0x10000 - (0xD800 << 10) - 0xDC00;
			return 2;
		}

		*a_utf32 = 0;
		return -1;
	}

	*a_utf32 = a_utf16;
	return 1;
}


/*	Extracts the next code point from the UTF-8 stream.
	Returns the number of byte read, or a negative value on error:
	-1 on short read
	-2 on missing size selection byte
	-3 on deprecated size selection byte
	-4 on unfinished sequence after the 1st byte
	-5 ... 2nd byte
	-6 ... 3rd byte
*/
ssize_t utf8_to_utf32( char* utf8, size_t utf8_len, uint32_t *utf32 )
{
	assert( utf8 );
	assert( utf8_len );
	assert( utf32 );

	ssize_t result;
	uint8_t *next_byte = (uint8_t *) utf8;
	uint32_t cp = 0;

	if ((*next_byte & 0x80) == 0x00) { // single byte, ≤ 7 bits
		cp = *next_byte;
		result = 1;
	} else if ((*next_byte & 0xC0) == 0x80) {
		// Missing size selection byte
		result = -2;
	} else if ((*next_byte & 0xE0) == 0xC0) { // 2-byte, ≤ 11 bits
		if (utf8_len < 2) {
			result = -1;
		} else {
			cp = (*next_byte++ & 0x1F) << 6;
			if ((*next_byte & 0xC0) == 0x80) {
				cp |= *next_byte & 0x3F;
				result = 2;
			} else {
				result = -4;
			}
		}
	} else if ((*next_byte & 0xF0) == 0xE0) { // 3-byte, ≤ 16 bits
		if (utf8_len < 3) {
			result = -1;
		} else {
			cp = (*next_byte++ & 0x0F) << 12;
			if ((*next_byte & 0xC0) == 0x80) {
				cp |= (*next_byte++ & 0x3F) << 6;
				if ((*next_byte & 0xC0) == 0x80) {
					cp |= *next_byte & 0x3F;
					result = 3;
				} else {
					result = -5;
				}
			} else {
				result = -4;
			}
		}
	} else if ((*next_byte & 0xF8) == 0xF0) { // 4-byte, ≤ 21 bits
		if (utf8_len < 4) {
			result = -1;
		} else {
			cp = (*next_byte & 0x07) << 18;
			if ((*next_byte & 0xC0) == 0x80) {
				cp |= (*next_byte++ & 0x3F) << 12;
				if ((*next_byte & 0xC0) == 0x80) {
					cp |= (*next_byte++ & 0x3F) << 6;
					if ((*next_byte & 0xC0) == 0x80) {
						cp |= *next_byte & 0x3F;
						result = 4;
					} else {
						result = -6;
					}
				} else {
					result = -5;
				}
			} else {
				result = -4;
			}
		}
	} else {
		// 5+-byte codes have been deprecated
		result = -3;
	}
	if (result >= 0) { *utf32 = cp; }
	return result;
}


// TODO: UTF-8 to UTF-16 (and UCS-2)


/*	Allocates an UTF-16 string from a source UTF-32 string.
	A code unit is seen as an uint16_t, and a code point as an uint32_t, which means that the endianness is the one of the target architecture.
	Arguments:
		IN a_utf32: source UTF-32 string.
		IN a_utf32_len: length of the source string, in code points.
		OUT a_utf16: allocated destination UTF-16 string.
	Return value:
		length of the destination string, in UTF-16 code units (?).
*/
ssize_t alloc_utf16_from_utf32( uint32_t *a_utf32, size_t a_utf32_len, uint16_t **a_utf16 )
{
	ssize_t result = 0;
	uint16_t *utf16 = malloc( sizeof( uint32_t ) * a_utf32_len );

	if (utf16) {
		uint32_t *src_ptr = a_utf32;
		size_t src_rmn_len = a_utf32_len;
		uint16_t *dst_ptr = utf16;
		size_t dst_len = 0;

		while (result >= 0 && src_rmn_len > 0) {
			if (*src_ptr >= 0x10000) {
				fprintf( stderr, "Warning: U+%X not in UCS-2\n", *src_ptr );
			}
			result = utf32_to_utf16( *src_ptr, dst_ptr );
			if (result < 0) {
				fprintf( stderr, " utf32_to_utf16 - error %zd\n", result );
			} else {
				src_ptr++;
				src_rmn_len--;
				dst_ptr+=result;
				dst_len+=result;
			}
		}

		*a_utf16 = utf16;
		result = dst_len;
	} else {
		fprintf( stderr, "malloc failed.\n" );
	}

	return result;
}


// TODO: Allocate UTF-8 string from UTF-32 string.
// TODO: Allocate UTF-8 string from UTF-16 string.
// TODO: Allocate UTF-32 string from UTF-16 string.


/*	Allocates an UTF-32 string from a source UTF-8 string.
	A code point is seen as an uint32_t, which means that the endianness is the one of the target architecture.
	Arguments:
		IN a_utf8: source UTF-8 string.
		IN a_utf8_len: length of the source string, in bytes.
		OUT a_utf32: allocated destination UTF-32 string.
	Return value:
		length of the destination string, in UTF-32 code points (?).
*/
ssize_t alloc_utf32_from_utf8( char *a_utf8, size_t a_utf8_len, uint32_t **a_utf32 )
{
	ssize_t result = 0;
	uint32_t *utf32 = malloc( sizeof( uint32_t ) * a_utf8_len );

	if (utf32) {
		char *src_ptr = a_utf8;
		size_t src_rmn_len = a_utf8_len;
		uint32_t *dst_ptr = utf32;
		size_t dst_len = 0;

		while (result >= 0 && src_rmn_len > 0) {
			result = utf8_to_utf32( src_ptr, src_rmn_len, dst_ptr );
			if (result < 0) {
				fprintf( stderr, " utf8_to_utf32 - error %zd - %zd was remaining\n", result, src_rmn_len );
			} else {
				src_ptr += result;
				src_rmn_len -= result;
				dst_ptr++;
				dst_len++;
			}
		}

		*a_utf32 = utf32;
		result = dst_len;
	} else {
		fprintf( stderr, "malloc failed.\n" );
	}

	return result;
}


/*	Allocates an UTF-16 string from a source UTF-8 string.
	A code unit is seen as an uint16_t, which means that the endianness is the one of the target architecture.
	Arguments:
		IN a_utf8: source UTF-8 string.
		IN a_utf8_len: length of the source string, in bytes.
		OUT a_utf16: allocated destination UTF-16 string.
	Return value:
		length of the destination string, in UTF-16 code units (?).
*/
ssize_t alloc_utf16_from_utf8( char *a_utf8, size_t a_utf8_len, uint16_t **a_utf16 )
{
	uint32_t *utf32;
	// Direct conversion (without UTF-32 intermediate) would be better
	printf( "Converting %zd bytes in UTF8 stream\n", a_utf8_len );
	ssize_t len = alloc_utf32_from_utf8( a_utf8, a_utf8_len, &utf32 );
	len = alloc_utf16_from_utf32( utf32, len, a_utf16 );
	free( utf32 );
	return len;
}
