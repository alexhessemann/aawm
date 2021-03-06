#ifndef UTFCONV_H
#define UTFCONV_H

#include <stdint.h>
#include <sys/types.h>

ssize_t utf32_to_utf16( uint32_t utf32, uint16_t* utf16 );
ssize_t utf32_to_utf8(unsigned int utf32, unsigned char* utf8);
ssize_t utf16_to_utf32(unsigned short a_utf16, unsigned int* a_utf32);
ssize_t utf8_to_utf32( uint8_t* utf8, size_t utf8_len, uint32_t *utf32 );

ssize_t alloc_utf16_from_utf32( uint32_t *a_utf32, size_t a_utf32_len, uint16_t **a_utf16 );
ssize_t alloc_utf32_from_utf8( char *a_utf8, size_t a_utf8_len, uint32_t **a_utf32 );
ssize_t alloc_utf16_from_utf8( char *a_utf8, size_t a_utf8_len, uint16_t **a_utf16 );

#endif
