#ifndef _MXPSQL_MPARC_C
/// @brief Header guard
#define _MXPSQL_MPARC_C

/**
  * @file mparc.c
  * @author MXPSQL
  * @brief MPARC, A Dumb Archiver Format C Rewrite Of MPAR. C Source File.
  * @version 0.1
  * @date 2022-09-26
  * 
  * @copyright
  * 
  * Licensed To You Under Teh MIT License and the LGPL-2.1-Or-Later License
  * 
  * MIT License
  * 
  * Copyright (c) 2022 MXPSQL
  * 
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  * 
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  * SOFTWARE.
  * 
  * 
  * MPARC, A rewrite of MPAR IN C, a dumb archiver format
  * Copyright (C) 2022 MXPSQL
  * 
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License as published by the Free Software Foundation; either
  * version 2.1 of the License, or (at your option) any later version.
  * 
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  * 
  * You should have received a copy of the GNU Lesser General Public
  * License along with this library; if not, write to the Free Software
  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/// I want prototypes of my functions, normally not exposed
#define MPARC_WANT_EXTERN_AUX_UTIL_FUNCTIONS
#include "mparc.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <stdint.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifdef MPARC_DEBUG
	#define MPARC_MEM_DEBUG 1

	// verbosity
	#ifdef MPARC_DEBUG_VERBOSE
		// You want verbosity?
		#define MPARC_MEM_DEBUG_VERBOSE 1
	#endif
#endif

// VS Code Special debugs
// #define MPARC_MEM_DEBUG
// #ifndef MPARC_USE_DMALLOC
// 	/// debug for VS CODE
// 	#define MPARC_USE_DMALLOC
// #endif

#ifdef MPARC_MEM_DEBUG
	#ifdef MPARC_USE_DMALLOC
		#include <dmalloc.h>
		/// @ Check yo mem leaks rn
		#define CHECK_LEAKS()
	#else
	#define GC_DEBUG
		#include <gc/gc.h>
		#define malloc(n) GC_MALLOC(n)
		#define calloc(m,n) GC_MALLOC((m)*(n))
		#define free(p) GC_FREE(p)
		#define realloc(p,n) GC_REALLOC((p),(n))
		/// @brief Check yo mem leaks rn
		#define CHECK_LEAKS() GC_gcollect()
	#endif
#else
	/// @brief Check yo mem leaks rn
	#define CHECK_LEAKS()
#endif

#endif

/* defines */

/// @brief Magic number for file format
#define STANKY_MPAR_FILE_FORMAT_MAGIC_NUMBER_25 "MXPSQL's Portable Archive"

/* define for format version number and representation */
/// @brief Version number magic
#define STANKY_MPAR_FILE_FORMAT_VERSION_NUMBER 1
/// @brief False CRC32 Hash number magic
#define STANKY_MPAR_FILE_FORMAT_VERSION_HASH_ADDED 1
// #define STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION MXPSQL_MPARC_uint_repr_t
/// @brief Our version representation
#define STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION unsigned long long

/* special separators, only added here if necessary */
/// @brief Magic separator for checksum and entry
#define MPARC_MAGIC_CHKSM_SEP '%'

// debug stuff defines
/// @brief Where to print to if you want send a message with fprintf debugging
#define MPARC_DEBUG_CONF_PRINTF_FILE stderr

// sort stuff
// broken, but not because of the sorter. If you enable this, you will get MPARC_INTERNAL.
// #define MPARC_QSORT
// sorting mode setups
#ifndef MPARC_QSORT_MODE
/** 
 * Magic sorter mode
 * 
 * @details
 * Sorting mode.
 * 
 * if set to 0, then the entries will be sorted by their checksum values,
 * else if set to 1 sorted by their filename,
 * else randomly sorted.
 * 
 * Default is 1.
 * 
 * That description for mode 0 was actually false (sort of), normally the checksum will sort itself, but if it fails, then the json will sort it
 */
#define MPARC_QSORT_MODE 1
// KLUDGE: sortcmp is sort of broken
#endif

#ifndef MPARC_DIRECTF_MINIMUM
/// @brief Magic threshold number
/// @details
///
/// Fwrite and Fread usage threshold.
/// How much bytes is needed to warant using fwrite or fread, else use fputc or fgetc.
/// 
/// Default is 8 kiloytes,
/// think of as in bytes, not bits.
#define MPARC_DIRECTF_MINIMUM (8000)
#endif

/* not defines */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* Special type declaration */
/**
 * @brief crc_t CRC32 type
 */
typedef uint_fast32_t crc_t;

/**
 * @brief Map internal storage representation
 */
typedef struct MPARC_blob_store{
	/// @brief size of binary
	MXPSQL_MPARC_uint_repr_t binary_size;
	/// @brief blob of binary contents
	unsigned char* binary_blob;
	/// @brief CRC32 checksum of binary
	crc_t binary_crc;
} MPARC_blob_store;

#ifndef MXPSQL_MPARC_NO_B64

/*
 * B64 Section and Copyright notice
 *
 * Unlicensed :)
 * C++ implementation was ripped from tomykaira's gist 
 * https://gist.github.com/tomykaira/f0fd86b6c73063283afe550bc5d77594
 *
 * written by skullchap 
 * https://github.com/skullchap/b64
*/

// used to work, but somehow is broken now :P
// the encoding is the broken part

/* static unsigned char *_b64Encode(unsigned char *psdata, MXPSQL_MPARC_uint_repr_t inlen, MXPSQL_MPARC_uint_repr_t* outplen)
{
    static const char b64e[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/'};

	MXPSQL_MPARC_uint_repr_t outlen = ((((inlen) + 2) / 3) * 4);

	char* data = (char*) psdata;

    char *out = MPARC_calloc(outlen + 1, sizeof(char));
    if (out == NULL) return NULL;
    out[outlen] = '\0';
	if(outplen) *outplen = outlen;
    char *p = out;


    MXPSQL_MPARC_uint_repr_t i;
    for (i = 0; i < inlen - 2; i += 3)
    {
        *p++ = b64e[(data[i] >> 2) & 0x3F];
        *p++ = b64e[((data[i] & 0x3) << 4) | ((data[i + 1] & 0xF0) >> 4)];
        *p++ = b64e[((data[i + 1] & 0xF) << 2) | ((data[i + 2] & 0xC0) >> 6)];
        *p++ = b64e[data[i + 2] & 0x3F];
    }

    if (i < inlen)
    {
        *p++ = b64e[(data[i] >> 2) & 0x3F];
        if (i == (inlen - 1))
        {
            *p++ = b64e[((data[i] & 0x3) << 4)];
            *p++ = '=';
        }
        else
        {
            *p++ = b64e[((data[i] & 0x3) << 4) | ((data[i + 1] & 0xF0) >> 4)];
            *p++ = b64e[((data[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
    }

    return (unsigned char*) out;
} */

// not a problem
/**
 * @internal
 */
static unsigned char *b64Encode(unsigned char *psyudata, MXPSQL_MPARC_uint_repr_t inlen, MXPSQL_MPARC_uint_repr_t* outplen)
{
		static const char b64e[] = {
				'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
				'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
				'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
				'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
				'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
				'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
				'w', 'x', 'y', 'z', '0', '1', '2', '3',
				'4', '5', '6', '7', '8', '9', '+', '/'};

		MXPSQL_MPARC_uint_repr_t outlen = ((((inlen) + 2) / 3) * 4);

		char* data = (char*) psyudata; // ps from the old one, y on accident and u for unsigned

		char *out = MPARC_calloc(outlen + 1, sizeof(char));
		CHECK_LEAKS();
		if (out == NULL) return NULL;
		out[outlen] = '\0';
		char *p = out;
		if(outplen) *outplen = outlen;

	#ifdef MPARC_MEM_DEBUG_VERBOSE
	{
		fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Byte view of data before base64 conversion in btoa with size of %"PRIuFAST64".\n", inlen);
		for(MXPSQL_MPARC_uint_repr_t i = 0; i < inlen; i++){
			fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", data[i]);
		}
		fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
	}
	#endif

		MXPSQL_MPARC_uint_repr_t i;
		for (i = 0; i < inlen - 2; i += 3)
		{
				*p++ = b64e[(data[i] >> 2) & 0x3F];
				*p++ = b64e[((data[i] & 0x3) << 4) | ((data[i + 1] & 0xF0) >> 4)];
				*p++ = b64e[((data[i + 1] & 0xF) << 2) | ((data[i + 2] & 0xC0) >> 6)];
				*p++ = b64e[data[i + 2] & 0x3F];
		}

		if (i < inlen)
		{
				*p++ = b64e[(data[i] >> 2) & 0x3F];
				if (i == (inlen - 1))
				{
						*p++ = b64e[((data[i] & 0x3) << 4)];
						*p++ = '=';
				}
				else
				{
						*p++ = b64e[((data[i] & 0x3) << 4) | ((data[i + 1] & 0xF0) >> 4)];
						*p++ = b64e[((data[i + 1] & 0xF) << 2)];
				}
				*p++ = '=';
		}


		return (unsigned char*) out;
}



// now decoder has problems I see. Nope, no more.
/**
 * @internal
 */
static unsigned char *b64Decode(unsigned char *udata, MXPSQL_MPARC_uint_repr_t inlen, MXPSQL_MPARC_uint_repr_t* outplen)
{
		static const char b64d[] = {
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
				52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
				64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
				15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
				64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
				41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
				64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};

		char* data = (char*) udata;

		if (inlen == 0 || inlen % 4) return NULL;
		MXPSQL_MPARC_uint_repr_t outlen = (((inlen) / 4) * 3);

		if (data[inlen - 1] == '=') outlen--;
		if (data[inlen - 2] == '=') outlen--;

		unsigned char *out = (unsigned char*) MPARC_calloc(outlen, sizeof(unsigned char));
		CHECK_LEAKS();
		if (out == NULL) return NULL;
		*outplen = outlen;

		typedef size_t u32;
		for (MXPSQL_MPARC_uint_repr_t i = 0, j = 0; i < inlen;)
		{
				u32 a = data[i] == '=' ? 0 & i++ : (MXPSQL_MPARC_uint_repr_t) b64d[((MXPSQL_MPARC_uint_repr_t) data[(MXPSQL_MPARC_uint_repr_t) i++])];
				u32 b = data[i] == '=' ? 0 & i++ : (MXPSQL_MPARC_uint_repr_t) b64d[((MXPSQL_MPARC_uint_repr_t) data[(MXPSQL_MPARC_uint_repr_t) i++])];
				u32 c = data[i] == '=' ? 0 & i++ : (MXPSQL_MPARC_uint_repr_t) b64d[((MXPSQL_MPARC_uint_repr_t) data[(MXPSQL_MPARC_uint_repr_t) i++])];
				u32 d = data[i] == '=' ? 0 & i++ : (MXPSQL_MPARC_uint_repr_t) b64d[((MXPSQL_MPARC_uint_repr_t) data[(MXPSQL_MPARC_uint_repr_t) i++])];

				u32 triple = (a << 3 * 6) + (b << 2 * 6) +
													(c << 1 * 6) + (d << 0 * 6);

				if (j < outlen) out[j++] = (triple >> 2 * 8) & 0xFF;
				if (j < outlen) out[j++] = (triple >> 1 * 8) & 0xFF;
				if (j < outlen) out[j++] = (triple >> 0 * 8) & 0xFF;
		}


		return out;
}


// Thanks MIT! (https://web.mit.edu/freebsd/head/contrib/wpa/src/utils/base64.c)
// Here is the header:
/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
// statics:
// static const unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// Documentation:
/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
// static unsigned char * base64_encode(const unsigned char *src, MXPSQL_MPARC_uint_repr_t len, MXPSQL_MPARC_uint_repr_t *out_len)
// {
// 	unsigned char *out, *pos;
// 	const unsigned char *end, *in;
// 	MXPSQL_MPARC_uint_repr_t olen;
// 	int line_len;
// 
// 	olen = len * 4 / 3 + 4; /* 3-byte blocks to 4-byte */
// 	olen += olen / 72; /* line feeds */
// 	olen++; /* nul termination */
// 	if (olen < len)
// 		return NULL; /* integer overflow */
// 	out = MPARC_calloc(olen, sizeof(char));
// 	if (out == NULL)
// 		return NULL;
// 
// 	end = src + len;
// 	in = src;
// 	pos = out;
// 	line_len = 0;
// 	while (end - in >= 3) {
// 		*pos++ = base64_table[in[0] >> 2];
// 		*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
// 		*pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
// 		*pos++ = base64_table[in[2] & 0x3f];
// 		in += 3;
// 		line_len += 4;
// 		if (line_len >= 72) {
// 			*pos++ = '\n';
// 			line_len = 0;
// 		}
// 	}
// 
// 	if (end - in) {
// 		*pos++ = base64_table[in[0] >> 2];
// 		if (end - in == 1) {
// 			*pos++ = base64_table[(in[0] & 0x03) << 4];
// 			*pos++ = '=';
// 		} else {
// 			*pos++ = base64_table[((in[0] & 0x03) << 4) |
// 					      (in[1] >> 4)];
// 			*pos++ = base64_table[(in[1] & 0x0f) << 2];
// 		}
// 		*pos++ = '=';
// 		line_len += 4;
// 	}
// 
// 	if (line_len)
// 		*pos++ = '\n';
// 
// 	*pos = '\0';
// 	if (out_len)
// 		*out_len = pos - out;
// 	return out;
// }
/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
// unsigned char * base64_decode(const unsigned char *src, MXPSQL_MPARC_uint_repr_t len, MXPSQL_MPARC_uint_repr_t *out_len)
// {
// 	unsigned char dtable[256], *out, *pos, block[4], tmp;
// 	size_t i, count, olen;
// 	int pad = 0;
// 
// 	memset(dtable, 0x80, 256);
// 	for (i = 0; i < sizeof(base64_table) - 1; i++)
// 		dtable[base64_table[i]] = (unsigned char) i;
// 	dtable['='] = 0;
// 
// 	count = 0;
// 	for (i = 0; i < len; i++) {
// 		if (dtable[src[i]] != 0x80)
// 			count++;
// 	}
// 
// 	if (count == 0 || count % 4)
// 		return NULL;
// 
// 	olen = count / 4 * 3;
// 	pos = out = MPARC_calloc(olen, sizeof(char));
// 	if (out == NULL)
// 		return NULL;
// 
// 	count = 0;
// 	for (i = 0; i < len; i++) {
// 		tmp = dtable[src[i]];
// 		if (tmp == 0x80)
// 			continue;
// 
// 		if (src[i] == '=')
// 			pad++;
// 		block[count] = tmp;
// 		count++;
// 		if (count == 4) {
// 			*pos++ = (block[0] << 2) | (block[1] >> 4);
// 			*pos++ = (block[1] << 4) | (block[2] >> 2);
// 			*pos++ = (block[2] << 6) | block[3];
// 			count = 0;
// 			if (pad) {
// 				if (pad == 1)
// 					pos--;
// 				else if (pad == 2)
// 					pos -= 2;
// 				else {
// 					/* Invalid padding */
// 					MPARC_free(out);
// 					return NULL;
// 				}
// 				break;
// 			}
// 		}
// 	}
// 
// 	*out_len = pos - out;
// 	return out;
// }

/**
 * @internal
 */
static const struct {
		unsigned char* (*btoa) (unsigned char*, MXPSQL_MPARC_uint_repr_t, MXPSQL_MPARC_uint_repr_t*);
		unsigned char* (*atob) (unsigned char*, MXPSQL_MPARC_uint_repr_t, MXPSQL_MPARC_uint_repr_t*);
} b64 = {b64Encode, b64Decode};




/* END OF B64 SECTION */

#endif


#ifndef MXPSQL_MPARC_NO_PYCRC32

/* crc.h and crc.c by pycrc section */

/**
 * The type of the CRC values.
 *
 * This type must be big enough to contain at least 32 bits.
 */
// not here, moved up


/**
 * Calculate the initial crc value.
 *
 * \return     The initial crc value.
 */
static inline crc_t crc_init(void)
{
	return 0xffffffff;
}


/**
 * Update the crc value with new data.
 *
 * \param[in] crc      The current crc value.
 * \param[in] data     Pointer to a buffer of \a data_len bytes.
 * \param[in] data_len Number of bytes in the \a data buffer.
 * \return             The updated crc value.
 */
static crc_t crc_update(crc_t crc, const void *data, MXPSQL_MPARC_uint_repr_t data_len)
{
	static const crc_t crc_table[256] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
		0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
		0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
		0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
		0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
		0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
		0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
		0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
		0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
		0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
		0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
		0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
		0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
		0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
		0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
		0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
		0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
		0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
		0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
		0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
		0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
		0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
		0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
		0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
		0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
		0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
		0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
		0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
	};

	const unsigned char *d = (const unsigned char *)data;
	unsigned int tbl_idx;

	while (data_len--) {
		tbl_idx = (crc ^ *d) & 0xff;
		crc = (crc_table[tbl_idx] ^ (crc >> 8)) & 0xffffffff;
		d++;
	}
	return crc & 0xffffffff;
}


/**
 * Calculate the final crc value.
 *
 * \param[in] crc  The current crc value.
 * \return     The
 */
static inline crc_t crc_finalize(crc_t crc)
{
	return crc ^ 0xffffffff;
}

/* end of CRC32 section */

#endif


#ifndef MXPSQL_MPARC_NO_CCAN_JSON

/* jsmn.h and json.c section and copyright notice */

/* json.c section */

/*
  Copyright (C) 2011 Joseph A. Adams (joeyadams3.14159@gmail.com)
  All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/**
 * @brief JsonNode type
 */
typedef enum {
	/// NULL
	JSON_NULL,
	/// Boolean
	JSON_BOOL,
	/// char*
	JSON_STRING,
	/// (-)0-9
	JSON_NUMBER,
	/// [true, false, 0, 21, 9, "ez"]
	JSON_ARRAY,
	/// {
	/// 	"array": [true, false, 0, 21, 9, "ez"],
	/// 	"null": null
	/// }
	JSON_OBJECT,
} JsonTag;

/**
 * @brief Json Node representation in a struct
 * 
 */
typedef struct JsonNode JsonNode;

/**
 * @brief Json Node representation in a struct
 * 
 */
struct JsonNode
{
	/// only if parent is an object or array (NULL otherwise)
	JsonNode *parent;
	/// next or previous entry
	JsonNode *prev, *next;
	
	/// only if parent is an object (NULL otherwise)
	/// Must be valid UTF-8.
	char *key; 

	/// JSON Typing
	JsonTag tag;

	/// Storage value
	union {
		/// JSON_BOOL
		bool boole;
		
		/// JSON_STRING
		/// Must be valid UTF-8.
		char *string; 
		
		/// JSON_NUMBER
		double number;
		
		/// JSON_ARRAY 
		/// JSON_OBJECT
		struct {
			JsonNode *head, *tail;
		} children;
	} store;
};
	

/**
 * @brief Called on OOM condition and errors. Suppoused to abort, but that is dumb, so I let the users deal with it.
 */
#define out_of_memory() do {                    \
		/* dumb */ \
		/* fprintf(stderr, "Out of memory.\n"); */   \
		/* exit(EXIT_FAILURE);    */                 \
		; /* Null statement */ \
	} while (0)

/* Sadly, strdup is not portable. */
static char *json_strdup(const char *str)
{
	char *ret = (char*) MPARC_malloc((strlen(str) + 1)*sizeof(char));
	CHECK_LEAKS();
	if (ret == NULL){
		out_of_memory();
		return NULL;
	}
	strcpy(ret, str);
	return ret;
}

/* String buffer */

/**
 * @brief JSON SB internal magic
 */
typedef struct
{
	/// current is now
	char *cur;
	/// end is front/back
	char *end;
	// start is back/front
	char *start;
} SB;

static void sb_init(SB *sb)
{
	sb->start = (char*) MPARC_malloc(17*sizeof(char));
	CHECK_LEAKS();
	if (sb->start == NULL){
		out_of_memory();
		return;
	}
	sb->cur = sb->start;
	sb->end = sb->start + 16;
}

/// sb and need may be evaluated multiple times.
#define sb_need(sb, need) do {                  \
		if ((sb)->end - (sb)->cur < (need))     \
			sb_grow(sb, need);                  \
		if(sb == NULL) break;   \
	} while (0)

static void sb_grow(SB *sb, int need)
{
	size_t length = sb->cur - sb->start;
	size_t alloc = sb->end - sb->start;
	
	do {
		alloc *= 2;
	} while (alloc < length + need);
	
	void* newsb = MPARC_realloc(sb->start, alloc + 1);
	CHECK_LEAKS();
	if (newsb == NULL){
		out_of_memory();
		return;
	}
	sb->start = (char*) newsb;
	sb->cur = sb->start + length;
	sb->end = sb->start + alloc;
}

static void sb_put(SB *sb, const char *bytes, int count)
{
	sb_need(sb, count);
	memcpy(sb->cur, bytes, count);
	sb->cur += count;
}

/// putc 4 json (PUTC4JSON)
#define sb_putc(sb, c) do {         \
		if ((sb)->cur >= (sb)->end) \
			sb_grow(sb, 1);         \
		if(sb == NULL) break;   \
		*(sb)->cur++ = (c);         \
	} while (0)

static void sb_puts(SB *sb, const char *str)
{
	sb_put(sb, str, strlen(str));
}

static char *sb_finish(SB *sb)
{
	*sb->cur = 0;
	assert(sb->start <= sb->cur && strlen(sb->start) == (size_t)(sb->cur - sb->start));
	return sb->start;
}

static void sb_free(SB *sb)
{
	if(sb && sb->start) MPARC_free(sb->start);
}

/**
 * Unicode helper functions
 *
 * These are taken from the ccan/charset module and customized a bit.
 * Putting them here means the compiler can (choose to) inline them,
 * and it keeps ccan/json from having a dependency.
 */

/**
 * @details
 * 
 * 
 * Type for Unicode codepoints.
 * We need our own because wchar_t might be 16 bits.
 */
typedef uint32_t uchar_t;

/**
 * Validate a single UTF-8 character starting at @s.
 * The string must be null-terminated.
 *
 * If it's valid, return its length (1 thru 4).
 * If it's invalid or clipped, return 0.
 *
 * This function implements the syntax given in RFC3629, which is
 * the same as that given in The Unicode Standard, Version 6.0.
 *
 * It has the following properties:
 *
 *  * All codepoints U+0000..U+10FFFF may be encoded,
 *    except for U+D800..U+DFFF, which are reserved
 *    for UTF-16 surrogate pair encoding.
 *  * UTF-8 byte sequences longer than 4 bytes are not permitted,
 *    as they exceed the range of Unicode.
 *  * The sixty-six Unicode "non-characters" are permitted
 *    (namely, U+FDD0..U+FDEF, U+xxFFFE, and U+xxFFFF).
 */
static int utf8_validate_cz(const char *s)
{
	unsigned char c = *s++;
	
	if (c <= 0x7F) {        /* 00..7F */
		return 1;
	} else if (c <= 0xC1) { /* 80..C1 */
		/* Disallow overlong 2-byte sequence. */
		return 0;
	} else if (c <= 0xDF) { /* C2..DF */
		/* Make sure subsequent byte is in the range 0x80..0xBF. */
		if (((unsigned char)*s++ & 0xC0) != 0x80)
			return 0;
		
		return 2;
	} else if (c <= 0xEF) { /* E0..EF */
		/* Disallow overlong 3-byte sequence. */
		if (c == 0xE0 && (unsigned char)*s < 0xA0)
			return 0;
		
		/* Disallow U+D800..U+DFFF. */
		if (c == 0xED && (unsigned char)*s > 0x9F)
			return 0;
		
		/* Make sure subsequent bytes are in the range 0x80..0xBF. */
		if (((unsigned char)*s++ & 0xC0) != 0x80)
			return 0;
		if (((unsigned char)*s++ & 0xC0) != 0x80)
			return 0;
		
		return 3;
	} else if (c <= 0xF4) { /* F0..F4 */
		/* Disallow overlong 4-byte sequence. */
		if (c == 0xF0 && (unsigned char)*s < 0x90)
			return 0;
		
		/* Disallow codepoints beyond U+10FFFF. */
		if (c == 0xF4 && (unsigned char)*s > 0x8F)
			return 0;
		
		/* Make sure subsequent bytes are in the range 0x80..0xBF. */
		if (((unsigned char)*s++ & 0xC0) != 0x80)
			return 0;
		if (((unsigned char)*s++ & 0xC0) != 0x80)
			return 0;
		if (((unsigned char)*s++ & 0xC0) != 0x80)
			return 0;
		
		return 4;
	} else {                /* F5..FF */
		return 0;
	}
}

/* Validate a null-terminated UTF-8 string. */
static bool utf8_validate(const char *s)
{
	int len;
	
	for (; *s != 0; s += len) {
		len = utf8_validate_cz(s);
		if (len == 0)
			return false;
	}
	
	return true;
}

/*
 * Read a single UTF-8 character starting at @s,
 * returning the length, in bytes, of the character read.
 *
 * This function assumes input is valid UTF-8,
 * and that there are enough characters in front of @s.
 */
static int utf8_read_char(const char *s, uchar_t *out)
{
	const unsigned char *c = (const unsigned char*) s;
	
	assert(utf8_validate_cz(s));

	if (c[0] <= 0x7F) {
		/* 00..7F */
		*out = c[0];
		return 1;
	} else if (c[0] <= 0xDF) {
		/* C2..DF (unless input is invalid) */
		*out = ((uchar_t)c[0] & 0x1F) << 6 |
			   ((uchar_t)c[1] & 0x3F);
		return 2;
	} else if (c[0] <= 0xEF) {
		/* E0..EF */
		*out = ((uchar_t)c[0] &  0xF) << 12 |
			   ((uchar_t)c[1] & 0x3F) << 6  |
			   ((uchar_t)c[2] & 0x3F);
		return 3;
	} else {
		/* F0..F4 (unless input is invalid) */
		*out = ((uchar_t)c[0] &  0x7) << 18 |
			   ((uchar_t)c[1] & 0x3F) << 12 |
			   ((uchar_t)c[2] & 0x3F) << 6  |
			   ((uchar_t)c[3] & 0x3F);
		return 4;
	}
}

/*
 * Write a single UTF-8 character to @s,
 * returning the length, in bytes, of the character written.
 *
 * @unicode must be U+0000..U+10FFFF, but not U+D800..U+DFFF.
 *
 * This function will write up to 4 bytes to @out.
 */
static int utf8_write_char(uchar_t unicode, char *out)
{
	unsigned char *o = (unsigned char*) out;
	
	assert(unicode <= 0x10FFFF && !(unicode >= 0xD800 && unicode <= 0xDFFF));

	if (unicode <= 0x7F) {
		/* U+0000..U+007F */
		*o++ = unicode;
		return 1;
	} else if (unicode <= 0x7FF) {
		/* U+0080..U+07FF */
		*o++ = 0xC0 | unicode >> 6;
		*o++ = 0x80 | (unicode & 0x3F);
		return 2;
	} else if (unicode <= 0xFFFF) {
		/* U+0800..U+FFFF */
		*o++ = 0xE0 | unicode >> 12;
		*o++ = 0x80 | (unicode >> 6 & 0x3F);
		*o++ = 0x80 | (unicode & 0x3F);
		return 3;
	} else {
		/* U+10000..U+10FFFF */
		*o++ = 0xF0 | unicode >> 18;
		*o++ = 0x80 | (unicode >> 12 & 0x3F);
		*o++ = 0x80 | (unicode >> 6 & 0x3F);
		*o++ = 0x80 | (unicode & 0x3F);
		return 4;
	}
}

/*
 * Compute the Unicode codepoint of a UTF-16 surrogate pair.
 *
 * @uc should be 0xD800..0xDBFF, and @lc should be 0xDC00..0xDFFF.
 * If they aren't, this function returns false.
 */
static bool from_surrogate_pair(uint16_t uc, uint16_t lc, uchar_t *unicode)
{
	if (uc >= 0xD800 && uc <= 0xDBFF && lc >= 0xDC00 && lc <= 0xDFFF) {
		*unicode = 0x10000 + ((((uchar_t)uc & 0x3FF) << 10) | (lc & 0x3FF));
		return true;
	} else {
		return false;
	}
}

/*
 * Construct a UTF-16 surrogate pair given a Unicode codepoint.
 *
 * @unicode must be U+10000..U+10FFFF.
 */
static void to_surrogate_pair(uchar_t unicode, uint16_t *uc, uint16_t *lc)
{
	uchar_t n;
	
	assert(unicode >= 0x10000 && unicode <= 0x10FFFF);
	
	n = unicode - 0x10000;
	*uc = ((n >> 10) & 0x3FF) | 0xD800;
	*lc = (n & 0x3FF) | 0xDC00;
}

/**
 * @brief What char is it? Is it a space?
 */
#define is_space(c) ((c) == '\t' || (c) == '\n' || (c) == '\r' || (c) == ' ')
/**
 * @brief What char is it? Is it a digit?
 */
#define is_digit(c) ((c) >= '0' && (c) <= '9')


static JsonNode   *json_decode         (const char *json);
static char       *json_encode         (const JsonNode *node);
static char       *json_encode_string  (const char *str);
static char       *json_stringify      (const JsonNode *node, const char *space);
static void        json_delete         (JsonNode *node);
static bool        json_validate       (const char *json);

static bool parse_value     (const char **sp, JsonNode        **out);
static bool parse_string    (const char **sp, char            **out);
static bool parse_number    (const char **sp, double           *out);
static bool parse_array     (const char **sp, JsonNode        **out);
static bool parse_object    (const char **sp, JsonNode        **out);
static bool parse_hex16     (const char **sp, uint16_t         *out);

static bool expect_literal  (const char **sp, const char *str);
static void skip_space      (const char **sp);

static void emit_value              (SB *out, const JsonNode *node);
static void emit_value_indented     (SB *out, const JsonNode *node, const char *space, int indent_level);
static void emit_string             (SB *out, const char *str);
static void emit_number             (SB *out, double num);
static void emit_array              (SB *out, const JsonNode *array);
static void emit_array_indented     (SB *out, const JsonNode *array, const char *space, int indent_level);
static void emit_object             (SB *out, const JsonNode *object);
static void emit_object_indented    (SB *out, const JsonNode *object, const char *space, int indent_level);

static int write_hex16(char *out, uint16_t val);

/*** Lookup and traversal ***/

static JsonNode   *json_find_element   (JsonNode *array, int index);
static JsonNode   *json_find_member    (JsonNode *object, const char *name);

static JsonNode   *json_first_child    (const JsonNode *node);

/*** Construction and manipulation ***/

static JsonNode *json_mknull(void);
static JsonNode *json_mkbool(bool b);
static JsonNode *json_mkstring(const char *s);
static JsonNode *json_mknumber(double n);
static JsonNode *json_mkarray(void);
static JsonNode *json_mkobject(void);
static void json_append_element(JsonNode *array, JsonNode *element);
static void json_prepend_element(JsonNode *array, JsonNode *element);
static void json_append_member(JsonNode *object, const char *key, JsonNode *value);
static void json_prepend_member(JsonNode *object, const char *key, JsonNode *value);
static void json_remove_from_parent(JsonNode *node);

static JsonNode *mknode(JsonTag tag);
static void append_node(JsonNode *parent, JsonNode *child);
static void prepend_node(JsonNode *parent, JsonNode *child);
static void append_member(JsonNode *object, char *key, JsonNode *value);

/* Assertion-friendly validity checks */
static bool tag_is_valid(unsigned int tag);
static bool number_is_valid(const char *num);

/*** Debugging ***/

/*
 * Look for structure and encoding problems in a JsonNode or its descendents.
 *
 * If a problem is detected, return false, writing a description of the problem
 * to errmsg (unless errmsg is NULL).
 */
static bool json_check(const JsonNode *node, char errmsg[256]);

/**
 * @brief For each a json node
 * 
 * @param i name
 * @param object_or_array source
 */
#define json_foreach(i, object_or_array)            \
	for ((i) = json_first_child(object_or_array);   \
		 (i) != NULL;                               \
		 (i) = (i)->next)


static JsonNode *json_decode(const char *json)
{
	const char *s = json;
	JsonNode *ret;
	
	skip_space(&s);
	if (!parse_value(&s, &ret))
		return NULL;
	
	skip_space(&s);
	if (*s != 0) {
		json_delete(ret);
		return NULL;
	}
	
	return ret;
}

static char *json_encode(const JsonNode *node)
{
	return json_stringify(node, NULL);
}

static char *json_encode_string(const char *str)
{
	SB sb;
	sb_init(&sb);
	if(sb.start == NULL) return NULL;
	
	emit_string(&sb, str);
	
	return sb_finish(&sb);
}

static char *json_stringify(const JsonNode *node, const char *space)
{
	SB sb;
	sb_init(&sb);
	if(sb.start == NULL) return NULL;
	
	if (space != NULL)
		emit_value_indented(&sb, node, space, 0);
	else
		emit_value(&sb, node);
	
	return sb_finish(&sb);
}

static void json_delete(JsonNode *node)
{
	if (node != NULL) {
		json_remove_from_parent(node);
		
		switch (node->tag) {
			case JSON_STRING:
				if(node->store.string) MPARC_free(node->store.string);
				break;
			case JSON_ARRAY:
			case JSON_OBJECT:
			{
				JsonNode *child, *next;
				for (child = node->store.children.head; child != NULL; child = next) {
					next = child->next;
					json_delete(child);
				}
				break;
			}
			default:;
		}
		
		if(node) MPARC_free(node);
		node=NULL;
	}
}

static bool json_validate(const char *json)
{
	const char *s = json;
	
	skip_space(&s);
	if (!parse_value(&s, NULL))
		return false;
	
	skip_space(&s);
	if (*s != 0)
		return false;
	
	return true;
}

static JsonNode *json_find_element(JsonNode *array, int index)
{
	JsonNode *element;
	int i = 0;
	
	if (array == NULL || array->tag != JSON_ARRAY)
		return NULL;
	
	json_foreach(element, array) {
		if (i == index)
			return element;
		i++;
	}
	
	return NULL;
}

static JsonNode *json_find_member(JsonNode *object, const char *name)
{
	JsonNode *member;
	
	if (object == NULL || object->tag != JSON_OBJECT)
		return NULL;
	
	json_foreach(member, object)
		if (strcmp(member->key, name) == 0)
			return member;
	
	return NULL;
}

static JsonNode *json_first_child(const JsonNode *node)
{
	if (node != NULL && (node->tag == JSON_ARRAY || node->tag == JSON_OBJECT))
		return node->store.children.head;
	return NULL;
}

static JsonNode *mknode(JsonTag tag)
{
	JsonNode *ret = (JsonNode*) MPARC_calloc(1, sizeof(JsonNode));
	CHECK_LEAKS();
	if (ret == NULL){
		out_of_memory();
		return NULL;
	}
	ret->tag = tag;
	return ret;
}

static JsonNode *json_mknull(void)
{
	return mknode(JSON_NULL);
}

static JsonNode *json_mkbool(bool b)
{
	JsonNode *ret = mknode(JSON_BOOL);
	if(ret == NULL) return NULL;
	ret->store.boole = b;
	return ret;
}

static JsonNode *mkstring(char *s)
{
	JsonNode *ret = mknode(JSON_STRING);
	if(ret == NULL) return NULL;
	ret->store.string = s;
	return ret;
}

static JsonNode *json_mkstring(const char *s)
{
	char* dup = json_strdup(s);
	if(dup == NULL) return NULL;
	else return mkstring(dup);
}

static JsonNode *json_mknumber(double n)
{
	JsonNode *node = mknode(JSON_NUMBER);
	if(node == NULL) return NULL;
	node->store.number = n;
	return node;
}

static JsonNode *json_mkarray(void)
{
	return mknode(JSON_ARRAY);
}

static JsonNode *json_mkobject(void)
{
	return mknode(JSON_OBJECT);
}

static void append_node(JsonNode *parent, JsonNode *child)
{
	child->parent = parent;
	child->prev = parent->store.children.tail;
	child->next = NULL;
	
	if (parent->store.children.tail != NULL)
		parent->store.children.tail->next = child;
	else
		parent->store.children.head = child;
	parent->store.children.tail = child;
}

static void prepend_node(JsonNode *parent, JsonNode *child)
{
	child->parent = parent;
	child->prev = NULL;
	child->next = parent->store.children.head;
	
	if (parent->store.children.head != NULL)
		parent->store.children.head->prev = child;
	else
		parent->store.children.tail = child;
	parent->store.children.head = child;
}

static void append_member(JsonNode *object, char *key, JsonNode *value)
{
	value->key = key;
	append_node(object, value);
}

static void json_append_element(JsonNode *array, JsonNode *element)
{
	assert(array->tag == JSON_ARRAY);
	assert(element->parent == NULL);
	
	append_node(array, element);
}

static void json_prepend_element(JsonNode *array, JsonNode *element)
{
	assert(array->tag == JSON_ARRAY);
	assert(element->parent == NULL);
	
	prepend_node(array, element);
}

static void json_append_member(JsonNode *object, const char *key, JsonNode *value)
{
	assert(object->tag == JSON_OBJECT);
	assert(value->parent == NULL);

	char* k = json_strdup(key);
	if(k == NULL) return;
	
	append_member(object, k, value);
}

static void json_prepend_member(JsonNode *object, const char *key, JsonNode *value)
{
	assert(object->tag == JSON_OBJECT);
	assert(value->parent == NULL);

	char* k = json_strdup(key);
	if(k == NULL) return;

	value->key = k;
	prepend_node(object, value);
}

static void json_remove_from_parent(JsonNode *node)
{
	JsonNode *parent = node->parent;
	
	if (parent != NULL) {
		if (node->prev != NULL)
			node->prev->next = node->next;
		else
			parent->store.children.head = node->next;
		if (node->next != NULL)
			node->next->prev = node->prev;
		else
			parent->store.children.tail = node->prev;
		
		if(node->key) MPARC_free(node->key);
		
		node->parent = NULL;
		node->prev = node->next = NULL;
		node->key = NULL;
	}
}

static bool parse_value(const char **sp, JsonNode **out)
{
	const char *s = *sp;
	
	switch (*s) {
		case 'n':
			if (expect_literal(&s, "null")) {
				if (out)
					*out = json_mknull();
				*sp = s;
				return true;
			}
			return false;
		
		case 'f':
			if (expect_literal(&s, "false")) {
				if (out)
					*out = json_mkbool(false);
				*sp = s;
				return true;
			}
			return false;
		
		case 't':
			if (expect_literal(&s, "true")) {
				if (out)
					*out = json_mkbool(true);
				*sp = s;
				return true;
			}
			return false;
		
		case '"': {
			char *str;
			if (parse_string(&s, out ? &str : NULL)) {
				if (out)
					*out = mkstring(str);
				*sp = s;
				return true;
			}
			return false;
		}
		
		case '[':
			if (parse_array(&s, out)) {
				*sp = s;
				return true;
			}
			return false;
		
		case '{':
			if (parse_object(&s, out)) {
				*sp = s;
				return true;
			}
			return false;
		
		default: {
			double num;
			if (parse_number(&s, out ? &num : NULL)) {
				if (out)
					*out = json_mknumber(num);
				*sp = s;
				return true;
			}
			return false;
		}
	}
}

static bool parse_array(const char **sp, JsonNode **out)
{
	const char *s = *sp;
	JsonNode *ret = out ? json_mkarray() : NULL;
	JsonNode *element;
	
	if (*s++ != '[')
		goto failure;
	skip_space(&s);
	
	if (*s == ']') {
		s++;
		goto success;
	}
	
	for (;;) {
		if (!parse_value(&s, out ? &element : NULL))
			goto failure;
		skip_space(&s);
		
		if (out)
			json_append_element(ret, element);
		
		if (*s == ']') {
			s++;
			goto success;
		}
		
		if (*s++ != ',')
			goto failure;
		skip_space(&s);
	}
	
success:
	*sp = s;
	if (out)
		*out = ret;
	return true;

failure:
	json_delete(ret);
	return false;
}

static bool parse_object(const char **sp, JsonNode **out)
{
	const char *s = *sp;
	JsonNode *ret = out ? json_mkobject() : NULL;
	char *key;
	JsonNode *value;
	
	if (*s++ != '{')
		goto failure;
	skip_space(&s);
	
	if (*s == '}') {
		s++;
		goto success;
	}
	
	for (;;) {
		if (!parse_string(&s, out ? &key : NULL))
			goto failure;
		skip_space(&s);
		
		if (*s++ != ':')
			goto failure_free_key;
		skip_space(&s);
		
		if (!parse_value(&s, out ? &value : NULL))
			goto failure_free_key;
		skip_space(&s);
		
		if (out)
			append_member(ret, key, value);
		
		if (*s == '}') {
			s++;
			goto success;
		}
		
		if (*s++ != ',')
			goto failure;
		skip_space(&s);
	}
	
success:
	*sp = s;
	if (out)
		*out = ret;
	return true;

failure_free_key:
	if (out)
		if(key) MPARC_free(key);
failure:
	json_delete(ret);
	return false;
}

static bool parse_string(const char **sp, char **out)
{
	const char *s = *sp;
	SB sb;
	char throwaway_buffer[4];
		/* enough space for a UTF-8 character */
	char *b;
	
	if (*s++ != '"')
		return false;
	
	if (out) {
		sb_init(&sb);
		if(sb.start == NULL) return false;
		sb_need(&sb, 4);
		b = sb.cur;
	} else {
		b = throwaway_buffer;
	}
	
	while (*s != '"') {
		unsigned char c = *s++;
		
		/* Parse next character, and write it to b. */
		if (c == '\\') {
			c = *s++;
			switch (c) {
				case '"':
				case '\\':
				case '/':
					*b++ = c;
					break;
				case 'b':
					*b++ = '\b';
					break;
				case 'f':
					*b++ = '\f';
					break;
				case 'n':
					*b++ = '\n';
					break;
				case 'r':
					*b++ = '\r';
					break;
				case 't':
					*b++ = '\t';
					break;
				case 'u':
				{
					uint16_t uc, lc;
					uchar_t unicode;
					
					if (!parse_hex16(&s, &uc))
						goto failed;
					
					if (uc >= 0xD800 && uc <= 0xDFFF) {
						/* Handle UTF-16 surrogate pair. */
						if (*s++ != '\\' || *s++ != 'u' || !parse_hex16(&s, &lc))
							goto failed; /* Incomplete surrogate pair. */
						if (!from_surrogate_pair(uc, lc, &unicode))
							goto failed; /* Invalid surrogate pair. */
					} else if (uc == 0) {
						/* Disallow "\u0000". */
						goto failed;
					} else {
						unicode = uc;
					}
					
					b += utf8_write_char(unicode, b);
					break;
				}
				default:
					/* Invalid escape */
					goto failed;
			}
		} else if (c <= 0x1F) {
			/* Control characters are not allowed in string literals. */
			goto failed;
		} else {
			/* Validate and echo a UTF-8 character. */
			int len;
			
			s--;
			len = utf8_validate_cz(s);
			if (len == 0)
				goto failed; /* Invalid UTF-8 character. */
			
			while (len--)
				*b++ = *s++;
		}
		
		/*
		 * Update sb to know about the new bytes,
		 * and set up b to write another character.
		 */
		if (out) {
			sb.cur = b;
			sb_need(&sb, 4);
			b = sb.cur;
		} else {
			b = throwaway_buffer;
		}
	}
	s++;
	
	if (out)
		*out = sb_finish(&sb);
	*sp = s;
	return true;

failed:
	if (out)
		sb_free(&sb);
	return false;
}

/*
 * The JSON spec says that a number shall follow this precise pattern
 * (spaces and quotes added for readability):
 *	 '-'? (0 | [1-9][0-9]*) ('.' [0-9]+)? ([Ee] [+-]? [0-9]+)?
 *
 * However, some JSON parsers are more liberal.  For instance, PHP accepts
 * '.5' and '1.'.  JSON.parse accepts '+3'.
 *
 * This function takes the strict approach.
 */
bool parse_number(const char **sp, double *out)
{
	const char *s = *sp;

	/* '-'? */
	if (*s == '-')
		s++;

	/* (0 | [1-9][0-9]*) */
	if (*s == '0') {
		s++;
	} else {
		if (!is_digit(*s))
			return false;
		do {
			s++;
		} while (is_digit(*s));
	}

	/* ('.' [0-9]+)? */
	if (*s == '.') {
		s++;
		if (!is_digit(*s))
			return false;
		do {
			s++;
		} while (is_digit(*s));
	}

	/* ([Ee] [+-]? [0-9]+)? */
	if (*s == 'E' || *s == 'e') {
		s++;
		if (*s == '+' || *s == '-')
			s++;
		if (!is_digit(*s))
			return false;
		do {
			s++;
		} while (is_digit(*s));
	}

	if (out)
		*out = strtod(*sp, NULL);

	*sp = s;
	return true;
}

static void skip_space(const char **sp)
{
	const char *s = *sp;
	while (is_space(*s))
		s++;
	*sp = s;
}

static void emit_value(SB *out, const JsonNode *node)
{
	assert(tag_is_valid(node->tag));
	switch (node->tag) {
		case JSON_NULL:
			sb_puts(out, "null");
			break;
		case JSON_BOOL:
			sb_puts(out, node->store.boole ? "true" : "false");
			break;
		case JSON_STRING:
			emit_string(out, node->store.string);
			break;
		case JSON_NUMBER:
			emit_number(out, node->store.number);
			break;
		case JSON_ARRAY:
			emit_array(out, node);
			break;
		case JSON_OBJECT:
			emit_object(out, node);
			break;
		default:
			assert(false);
	}
}

static void emit_value_indented(SB *out, const JsonNode *node, const char *space, int indent_level)
{
	assert(tag_is_valid(node->tag));
	switch (node->tag) {
		case JSON_NULL:
			sb_puts(out, "null");
			break;
		case JSON_BOOL:
			sb_puts(out, node->store.boole ? "true" : "false");
			break;
		case JSON_STRING:
			emit_string(out, node->store.string);
			break;
		case JSON_NUMBER:
			emit_number(out, node->store.number);
			break;
		case JSON_ARRAY:
			emit_array_indented(out, node, space, indent_level);
			break;
		case JSON_OBJECT:
			emit_object_indented(out, node, space, indent_level);
			break;
		default:
			assert(false);
	}
}

static void emit_array(SB *out, const JsonNode *array)
{
	const JsonNode *element;
	
	sb_putc(out, '[');
	json_foreach(element, array) {
		emit_value(out, element);
		if (element->next != NULL)
			sb_putc(out, ',');
	}
	sb_putc(out, ']');
}

static void emit_array_indented(SB *out, const JsonNode *array, const char *space, int indent_level)
{
	const JsonNode *element = array->store.children.head;
	int i;
	
	if (element == NULL) {
		sb_puts(out, "[]");
		return;
	}
	
	sb_puts(out, "[\n");
	while (element != NULL) {
		for (i = 0; i < indent_level + 1; i++)
			sb_puts(out, space);
		emit_value_indented(out, element, space, indent_level + 1);
		
		element = element->next;
		sb_puts(out, element != NULL ? ",\n" : "\n");
	}
	for (i = 0; i < indent_level; i++)
		sb_puts(out, space);
	sb_putc(out, ']');
}

static void emit_object(SB *out, const JsonNode *object)
{
	const JsonNode *member;
	
	sb_putc(out, '{');
	json_foreach(member, object) {
		emit_string(out, member->key);
		sb_putc(out, ':');
		emit_value(out, member);
		if (member->next != NULL)
			sb_putc(out, ',');
	}
	sb_putc(out, '}');
}

static void emit_object_indented(SB *out, const JsonNode *object, const char *space, int indent_level)
{
	const JsonNode *member = object->store.children.head;
	int i;
	
	if (member == NULL) {
		sb_puts(out, "{}");
		return;
	}
	
	sb_puts(out, "{\n");
	while (member != NULL) {
		for (i = 0; i < indent_level + 1; i++)
			sb_puts(out, space);
		emit_string(out, member->key);
		sb_puts(out, ": ");
		emit_value_indented(out, member, space, indent_level + 1);
		
		member = member->next;
		sb_puts(out, member != NULL ? ",\n" : "\n");
	}
	for (i = 0; i < indent_level; i++)
		sb_puts(out, space);
	sb_putc(out, '}');
}

static void emit_string(SB *out, const char *str)
{
	bool escape_unicode = false;
	const char *s = str;
	char *b;
	
	assert(utf8_validate(str));
	
	/*
	 * 14 bytes is enough space to write up to two
	 * \uXXXX escapes and two quotation marks.
	 */
	sb_need(out, 14);
	b = out->cur;
	
	*b++ = '"';
	while (*s != 0) {
		unsigned char c = *s++;
		
		/* Encode the next character, and write it to b. */
		switch (c) {
			case '"':
				*b++ = '\\';
				*b++ = '"';
				break;
			case '\\':
				*b++ = '\\';
				*b++ = '\\';
				break;
			case '\b':
				*b++ = '\\';
				*b++ = 'b';
				break;
			case '\f':
				*b++ = '\\';
				*b++ = 'f';
				break;
			case '\n':
				*b++ = '\\';
				*b++ = 'n';
				break;
			case '\r':
				*b++ = '\\';
				*b++ = 'r';
				break;
			case '\t':
				*b++ = '\\';
				*b++ = 't';
				break;
			default: {
				int len;
				
				s--;
				len = utf8_validate_cz(s);
				
				if (len == 0) {
					/*
					 * Handle invalid UTF-8 character gracefully in production
					 * by writing a replacement character (U+FFFD)
					 * and skipping a single byte.
					 *
					 * This should never happen when assertions are enabled
					 * due to the assertion at the beginning of this function.
					 */
					assert(false);
					if (escape_unicode) {
						strcpy(b, "\\uFFFD");
						b += 6;
					} else {
						*b++ = (unsigned char) 0xEF;
						*b++ = (unsigned char) 0xBF;
						*b++ = (unsigned char) 0xBD;
					}
					s++;
				} else if (c < 0x1F || (c >= 0x80 && escape_unicode)) {
					/* Encode using \u.... */
					uint32_t unicode;
					
					s += utf8_read_char(s, &unicode);
					
					if (unicode <= 0xFFFF) {
						*b++ = '\\';
						*b++ = 'u';
						b += write_hex16(b, unicode);
					} else {
						/* Produce a surrogate pair. */
						uint16_t uc, lc;
						assert(unicode <= 0x10FFFF);
						to_surrogate_pair(unicode, &uc, &lc);
						*b++ = '\\';
						*b++ = 'u';
						b += write_hex16(b, uc);
						*b++ = '\\';
						*b++ = 'u';
						b += write_hex16(b, lc);
					}
				} else {
					/* Write the character directly. */
					while (len--)
						*b++ = *s++;
				}
				
				break;
			}
		}
	
		/*
		 * Update *out to know about the new bytes,
		 * and set up b to write another encoded character.
		 */
		out->cur = b;
		sb_need(out, 14);
		b = out->cur;
	}
	*b++ = '"';
	
	out->cur = b;
}

static void emit_number(SB *out, double num)
{
	/*
	 * This isn't exactly how JavaScript renders numbers,
	 * but it should produce valid JSON for reasonable numbers
	 * preserve precision well enough, and avoid some oddities
	 * like 0.3 -> 0.299999999999999988898 .
	 */
	char buf[64];
	sprintf(buf, "%.16g", num);
	
	if (number_is_valid(buf))
		sb_puts(out, buf);
	else
		sb_puts(out, "null");
}

static bool tag_is_valid(unsigned int tag)
{
	return (/* tag >= JSON_NULL && */ tag <= JSON_OBJECT);
}

static bool number_is_valid(const char *num)
{
	return (parse_number(&num, NULL) && *num == '\0');
}

static bool expect_literal(const char **sp, const char *str)
{
	const char *s = *sp;
	
	while (*str != '\0')
		if (*s++ != *str++)
			return false;
	
	*sp = s;
	return true;
}

/*
 * Parses exactly 4 hex characters (capital or lowercase).
 * Fails if any input chars are not [0-9A-Fa-f].
 */
static bool parse_hex16(const char **sp, uint16_t *out)
{
	const char *s = *sp;
	uint16_t ret = 0;
	uint16_t i;
	uint16_t tmp;
	char c;

	for (i = 0; i < 4; i++) {
		c = *s++;
		if (c >= '0' && c <= '9')
			tmp = c - '0';
		else if (c >= 'A' && c <= 'F')
			tmp = c - 'A' + 10;
		else if (c >= 'a' && c <= 'f')
			tmp = c - 'a' + 10;
		else
			return false;

		ret <<= 4;
		ret += tmp;
	}
	
	if (out)
		*out = ret;
	*sp = s;
	return true;
}

/*
 * Encodes a 16-bit number into hexadecimal,
 * writing exactly 4 hex chars.
 */
static int write_hex16(char *out, uint16_t val)
{
	const char *hex = "0123456789ABCDEF";
	
	*out++ = hex[(val >> 12) & 0xF];
	*out++ = hex[(val >> 8)  & 0xF];
	*out++ = hex[(val >> 4)  & 0xF];
	*out++ = hex[ val        & 0xF];
	
	return 4;
}

static bool json_check(const JsonNode *node, char errmsg[256])
{
	/// @brief Json Houston, we have a problem here
	#define problem(...) do { \
			if (errmsg != NULL) \
				snprintf(errmsg, 256, __VA_ARGS__); \
			return false; \
		} while (0)
	
	if (node->key != NULL && !utf8_validate(node->key))
		problem("key contains invalid UTF-8");
	
	if (!tag_is_valid(node->tag))
		problem("tag is invalid (%u)", node->tag);
	
	if (node->tag == JSON_BOOL) {
		if (node->store.boole != false && node->store.boole != true)
			problem("bool_ is neither false (%d) nor true (%d)", (int)false, (int)true);
	} else if (node->tag == JSON_STRING) {
		if (node->store.string == NULL)
			problem("string_ is NULL");
		if (!utf8_validate(node->store.string))
			problem("string_ contains invalid UTF-8");
	} else if (node->tag == JSON_ARRAY || node->tag == JSON_OBJECT) {
		JsonNode *head = node->store.children.head;
		JsonNode *tail = node->store.children.tail;
		
		if (head == NULL || tail == NULL) {
			if (head != NULL)
				problem("tail is NULL, but head is not");
			if (tail != NULL)
				problem("head is NULL, but tail is not");
		} else {
			JsonNode *child;
			JsonNode *last = NULL;
			
			if (head->prev != NULL)
				problem("First child's prev pointer is not NULL");
			
			for (child = head; child != NULL; last = child, child = child->next) {
				if (child == node)
					problem("node is its own child");
				if (child->next == child)
					problem("child->next == child (cycle)");
				if (child->next == head)
					problem("child->next == head (cycle)");
				
				if (child->parent != node)
					problem("child does not point back to parent");
				if (child->next != NULL && child->next->prev != child)
					problem("child->next does not point back to child");
				
				if (node->tag == JSON_ARRAY && child->key != NULL)
					problem("Array element's key is not NULL");
				if (node->tag == JSON_OBJECT && child->key == NULL)
					problem("Object member's key is NULL");
				
				if (!json_check(child, errmsg))
					return false;
			}
			
			if (last != tail)
				problem("tail does not match pointer found by starting at head and following next links");
		}
	}
	
	return true;
	
	#undef problem
}

#endif


#ifndef MXPSQL_MPARC_NO_JSMN

/* jsmn.h part from https://github.com/zserge/jsmn */

/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/// Originaly JSMN_API, but it will always be static, so we call it JSMN_STATIC
#define JSMN_STATIC static



/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	/// undefined behaviour objects
	JSMN_UNDEFINED = 0,
	/// an actual defined behaviour objects
	JSMN_OBJECT = 1 << 0,
	/// an array, not an object
	JSMN_ARRAY = 1 << 1,
	/// a string
	JSMN_STRING = 1 << 2,
	/// a primitive (bool, null, true, false, number)
	JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

/**
 * @brief Error state
 */
enum jsmnerr {
	/// Not enough tokens were provided
	JSMN_ERROR_NOMEM = -1,
	/// Invalid character inside JSON string
	JSMN_ERROR_INVAL = -2,
	/// The string is not a full JSON packet, more bytes expected
	JSMN_ERROR_PART = -3
};

/** @details
 * JSON token description.
 * type		type (object, array, string etc.)
 * start	start position in JSON data string
 * end		end position in JSON data string
 */
typedef struct jsmntok {
	/// Type of item
	jsmntype_t type;
	/// Start position
	int start;
	/// End position
	int end;
	/// Size
	int size;
#ifdef JSMN_PARENT_LINKS
	/// Where is parent?
	int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string.
 */
typedef struct jsmn_parser {
	/// offset in the JSON string
	unsigned int pos;     
	/// next token to allocate
	unsigned int toknext; 
	/// superior token node, e.g. parent object or array
	int toksuper;         
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
JSMN_STATIC void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each
 * describing
 * a single JSON object.
 */
JSMN_STATIC int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
												jsmntok_t *tokens, const unsigned int num_tokens);

/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
																	 const size_t num_tokens) {
	jsmntok_t *tok;
	if (parser->toknext >= num_tokens) {
		return NULL;
	}
	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;
#ifdef JSMN_PARENT_LINKS
	tok->parent = -1;
#endif
	return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
														const int start, const int end) {
	token->type = type;
	token->start = start;
	token->end = end;
	token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
																const size_t len, jsmntok_t *tokens,
																const size_t num_tokens) {
	jsmntok_t *token;
	int start;

	start = parser->pos;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		switch (js[parser->pos]) {
#ifndef JSMN_STRICT
		/* In strict mode primitive must be followed by "," or "}" or "]" */
		case ':':
#endif
		case '\t':
		case '\r':
		case '\n':
		case ' ':
		case ',':
		case ']':
		case '}':
			goto found;
		default:
									 /* to quiet a warning from gcc*/
			break;
		}
		if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
			parser->pos = start;
			return JSMN_ERROR_INVAL;
		}
	}
#ifdef JSMN_STRICT
	/* In strict mode primitive must be followed by a comma/object/array */
	parser->pos = start;
	return JSMN_ERROR_PART;
#endif

found:
	if (tokens == NULL) {
		parser->pos--;
		return 0;
	}
	token = jsmn_alloc_token(parser, tokens, num_tokens);
	if (token == NULL) {
		parser->pos = start;
		return JSMN_ERROR_NOMEM;
	}
	jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
	token->parent = parser->toksuper;
#endif
	parser->pos--;
	return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
														 const size_t len, jsmntok_t *tokens,
														 const size_t num_tokens) {
	jsmntok_t *token;

	int start = parser->pos;
	
	/* Skip starting quote */
	parser->pos++;
	
	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c = js[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			if (tokens == NULL) {
				return 0;
			}
			token = jsmn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				parser->pos = start;
				return JSMN_ERROR_NOMEM;
			}
			jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
			token->parent = parser->toksuper;
#endif
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && parser->pos + 1 < len) {
			int i;
			parser->pos++;
			switch (js[parser->pos]) {
			/* Allowed escaped symbols */
			case '\"':
			case '/':
			case '\\':
			case 'b':
			case 'f':
			case 'r':
			case 'n':
			case 't':
				break;
			/* Allows escaped symbol \uXXXX */
			case 'u':
				parser->pos++;
				for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0';
						 i++) {
					/* If it isn't a hex character we have an error */
					if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) ||   /* 0-9 */
								(js[parser->pos] >= 65 && js[parser->pos] <= 70) ||   /* A-F */
								(js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
						parser->pos = start;
						return JSMN_ERROR_INVAL;
					}
					parser->pos++;
				}
				parser->pos--;
				break;
			/* Unexpected symbol */
			default:
				parser->pos = start;
				return JSMN_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
JSMN_STATIC int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
												jsmntok_t *tokens, const unsigned int num_tokens) {
	int r;
	int i;
	jsmntok_t *token;
	int count = parser->toknext;

	for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
		char c;
		jsmntype_t type;

		c = js[parser->pos];
		switch (c) {
		case '{':
		case '[':
			count++;
			if (tokens == NULL) {
				break;
			}
			token = jsmn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				return JSMN_ERROR_NOMEM;
			}
			if (parser->toksuper != -1) {
				jsmntok_t *t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
				/* In strict mode an object or array can't become a key */
				if (t->type == JSMN_OBJECT) {
					return JSMN_ERROR_INVAL;
				}
#endif
				t->size++;
#ifdef JSMN_PARENT_LINKS
				token->parent = parser->toksuper;
#endif
			}
			token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
			token->start = parser->pos;
			parser->toksuper = parser->toknext - 1;
			break;
		case '}':
		case ']':
			if (tokens == NULL) {
				break;
			}
			type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
			if (parser->toknext < 1) {
				return JSMN_ERROR_INVAL;
			}
			token = &tokens[parser->toknext - 1];
			for (;;) {
				if (token->start != -1 && token->end == -1) {
					if (token->type != type) {
						return JSMN_ERROR_INVAL;
					}
					token->end = parser->pos + 1;
					parser->toksuper = token->parent;
					break;
				}
				if (token->parent == -1) {
					if (token->type != type || parser->toksuper == -1) {
						return JSMN_ERROR_INVAL;
					}
					break;
				}
				token = &tokens[token->parent];
			}
#else
			for (i = parser->toknext - 1; i >= 0; i--) {
				token = &tokens[i];
				if (token->start != -1 && token->end == -1) {
					if (token->type != type) {
						return JSMN_ERROR_INVAL;
					}
					parser->toksuper = -1;
					token->end = parser->pos + 1;
					break;
				}
			}
			/* Error if unmatched closing bracket */
			if (i == -1) {
				return JSMN_ERROR_INVAL;
			}
			for (; i >= 0; i--) {
				token = &tokens[i];
				if (token->start != -1 && token->end == -1) {
					parser->toksuper = i;
					break;
				}
			}
#endif
			break;
		case '\"':
			r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
			if (r < 0) {
				return r;
			}
			count++;
			if (parser->toksuper != -1 && tokens != NULL) {
				tokens[parser->toksuper].size++;
			}
			break;
		case '\t':
		case '\r':
		case '\n':
		case ' ':
			break;
		case ':':
			parser->toksuper = parser->toknext - 1;
			break;
		case ',':
			if (tokens != NULL && parser->toksuper != -1 &&
					tokens[parser->toksuper].type != JSMN_ARRAY &&
					tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
				parser->toksuper = tokens[parser->toksuper].parent;
#else
				for (i = parser->toknext - 1; i >= 0; i--) {
					if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
						if (tokens[i].start != -1 && tokens[i].end == -1) {
							parser->toksuper = i;
							break;
						}
					}
				}
#endif
			}
			break;
#ifdef JSMN_STRICT
		/* In strict mode primitives are: numbers and booleans */
		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 't':
		case 'f':
		case 'n':
			/* And they must not be keys of the object */
			if (tokens != NULL && parser->toksuper != -1) {
				const jsmntok_t *t = &tokens[parser->toksuper];
				if (t->type == JSMN_OBJECT ||
						(t->type == JSMN_STRING && t->size != 0)) {
					return JSMN_ERROR_INVAL;
				}
			}
#else
		/* In non-strict mode every unquoted value is a primitive */
		default:
#endif
			r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
			if (r < 0) {
				return r;
			}
			count++;
			if (parser->toksuper != -1 && tokens != NULL) {
				tokens[parser->toksuper].size++;
			}
			break;

#ifdef JSMN_STRICT
		/* Unexpected char in strict mode */
		default:
			return JSMN_ERROR_INVAL;
#endif
		}
	}

	if (tokens != NULL) {
		for (i = parser->toknext - 1; i >= 0; i--) {
			/* Unmatched opened object or array */
			if (tokens[i].start != -1 && tokens[i].end == -1) {
				return JSMN_ERROR_PART;
			}
		}
	}

	return count;
}

/**
 * Creates a new parser based over a given buffer with an array of tokens
 * available.
 */
JSMN_STATIC void jsmn_init(jsmn_parser *parser) {
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}

#endif

/* END OF jsmn.h and json.h section */


#ifndef MXPSQL_MPARC_NO_RXI_MAP

/* map.h by RXI section and copyright notice 
 * 
 * Sourced from https://github.com/rxi/map
 * 
 * Copyright (c) 2014 rxi
 * 
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

/**
 * @brief Map struct
 * 
 */
struct map_node_t;
/**
 * @brief Map struct typedef
 * 
 */
typedef struct map_node_t map_node_t;

/**
 * @brief Map base
 * 
 */
typedef struct {
	/// node buckets
	map_node_t **buckets;
	/// bucket and node count
	MXPSQL_MPARC_uint_repr_t nbuckets, nnodes;
} map_base_t;

/**
 * @brief Map iterator
 * 
 */
typedef struct {
	/// bucket index
	MXPSQL_MPARC_uint_repr_t bucketidx;
	/// current node
	map_node_t *node;
} map_iter_t;


/**
 * @brief Make map type
 * 
 */
#define map_t(T)\
	struct { map_base_t base; T *ref; T tmp; }

/**
 * @brief Initialize map
 * 
 */
#define map_init(m)\
	memset(m, '\0', sizeof(*(m)))


/**
 * @brief Deinitialize map
 * 
 */
#define map_deinit(m)\
	map_deinit_(&(m)->base)

/**
 * @brief Get map value from key
 * 
 */
#define map_get(m, key)\
	( (m)->ref = map_get_(&(m)->base, key) )

/**
 * @brief Set map value with key
 * 
 */
#define map_set(m, key, value)\
	( (m)->tmp = (value),\
		map_set_(&(m)->base, key, &(m)->tmp, sizeof((m)->tmp)) )

/**
 * @brief Map remove key
 * 
 */
#define map_remove(m, key)\
	map_remove_(&(m)->base, key)

/**
 * @brief Make map iterator
 * 
 */
#define map_iter(m)\
	map_iter_()

/**
 * @brief Next the map iterator
 * 
 */
#define map_next(m, iter)\
	map_next_(&(m)->base, iter)


static void map_deinit_(map_base_t *m);
static void *map_get_(map_base_t *m, const char *key);
static int map_set_(map_base_t *m, const char *key, void *value, int vsize);
static void map_remove_(map_base_t *m, const char *key);
static map_iter_t map_iter_(void);
static const char *map_next_(map_base_t *m, map_iter_t *iter);

/// void map
typedef map_t(void*) map_void_t;
/// char map
typedef map_t(char*) map_str_t;
/// int map
typedef map_t(int) map_int_t;
/// char map
typedef map_t(char) map_char_t;
/// float map
typedef map_t(float) map_float_t;
/// double map
typedef map_t(double) map_double_t;

struct map_node_t {
	MXPSQL_MPARC_uint_repr_t hash;
	void *value;
	map_node_t *next;
	/* char key[]; */
	/* char value[]; */
};


static MXPSQL_MPARC_uint_repr_t map_hash(const char *str) {
	MXPSQL_MPARC_uint_repr_t hash = 5381;
	while (*str) {
		hash = ((hash << 5) + hash) ^ *str++;
	}
	return hash;
}


static map_node_t *map_newnode(const char *key, void *value, int vsize) {
	map_node_t *node;
	int ksize = strlen(key) + 1;
	int voffset = ksize + ((sizeof(void*) - ksize) % sizeof(void*));
	node = MPARC_malloc(sizeof(*node) + voffset + vsize);
	CHECK_LEAKS();
	if (!node) return NULL;
	memcpy(node + 1, key, ksize);
	node->hash = map_hash(key);
	node->value = ((char*) (node + 1)) + voffset;
	memcpy(node->value, value, vsize);
	return node;
}


static int map_bucketidx(map_base_t *m, MXPSQL_MPARC_uint_repr_t hash) {
	/* If the implementation is changed to allow a non-power-of-2 bucket count,
	 * the line below should be changed to use mod instead of AND */
	return hash & (m->nbuckets - 1);
}


static void map_addnode(map_base_t *m, map_node_t *node) {
	int n = map_bucketidx(m, node->hash);
	node->next = m->buckets[n];
	m->buckets[n] = node;
	#ifdef MPARC_MEM_DEBUG_VERBOSE
	{
		// unsafe magic
		MPARC_blob_store* storeptr = m->buckets[n]->value;
		fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Byte view of data after map node is added with size of %"PRIuFAST64" and bucket index of %i.\n", storeptr->binary_size, n);
		for(MXPSQL_MPARC_uint_repr_t i = 0; i < storeptr->binary_size; i++){
			fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", storeptr->binary_blob[i]);
		}
		fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
	}
	#endif
}


static int map_resize(map_base_t *m, int nbuckets) {
	map_node_t *nodes, *node, *next;
	map_node_t **buckets;
	int i; 
	/* Chain all nodes together */
	nodes = NULL;
	i = m->nbuckets;
	while (i--) {
		node = (m->buckets)[i];
		while (node) {
			next = node->next;
			node->next = nodes;
			nodes = node;
			node = next;
		}
	}
	/* Reset buckets */
	buckets = MPARC_realloc(m->buckets, sizeof(*m->buckets) * nbuckets);
	CHECK_LEAKS();
	if (buckets != NULL) {
		m->buckets = buckets;
		m->nbuckets = nbuckets;
	}
	if (m->buckets) {
		memset(m->buckets, 0, sizeof(*m->buckets) * m->nbuckets);
		/* Re-add nodes to buckets */
		node = nodes;
		while (node) {
			next = node->next;
			map_addnode(m, node);
			node = next;
		}
	}
	/* Return error code if MPARC_realloc() failed */
	return (buckets == NULL) ? -1 : 0;
}


static map_node_t **map_getref(map_base_t *m, const char *key) {
	MXPSQL_MPARC_uint_repr_t hash = map_hash(key);
	map_node_t **next;
	if (m->nbuckets > 0) {
		next = &m->buckets[map_bucketidx(m, hash)];
		while (*next) {
			#ifdef MPARC_MEM_DEBUG_VERBOSE
			{
				// unsafe magic
				MPARC_blob_store* storeptr = (MPARC_blob_store*) (*next)->value;
				fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Byte view of unfiltered entry while getting reference to the hashmap bucket with size of %"PRIuFAST64".\n", storeptr->binary_size);
				for(MXPSQL_MPARC_uint_repr_t i = 0; i < storeptr->binary_size; i++){
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c",storeptr->binary_blob[i]);
				}
				fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
			}
			#endif
			if ((*next)->hash == hash && !strcmp((char*) (*next + 1), key)) {
				#ifdef MPARC_MEM_DEBUG_VERBOSE
				{
					// unsafe magic
					MPARC_blob_store* storeptr = (MPARC_blob_store*) m->buckets[map_bucketidx(m, hash)]->value;
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Byte view of filtered %s while getting reference to the hashmap bucket with size of %"PRIuFAST64" and bucket index of %i.\n", key, storeptr->binary_size, map_bucketidx(m, hash));
					for(MXPSQL_MPARC_uint_repr_t i = 0; i < storeptr->binary_size; i++){
						fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c",storeptr->binary_blob[i]);
					}
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
				}
				#endif
				return next;
			}
			next = &(*next)->next;
		}
	}
	return NULL;
}


static void map_deinit_(map_base_t *m) {
	map_node_t *next, *node;
	int i;
	i = m->nbuckets;
	while (i--) {
		node = m->buckets[i];
		while (node) {
			next = node->next;
			if(node) MPARC_free(node);
			node = next;
		}
	}
	if(m && m->buckets) MPARC_free(m->buckets);
	memset(m, '\0', sizeof(*(m)));
}


static void *map_get_(map_base_t *m, const char *key) {
	map_node_t **next = map_getref(m, key);
	return next ? (*next)->value : NULL;
}


static int map_set_(map_base_t *m, const char *key, void *value, int vsize) {
	int n, err;
	map_node_t **next, *node;
	/* Find & replace existing node */
	next = map_getref(m, key);
	if (next) {
		memcpy((*next)->value, value, vsize);
		return 0;
	}
	/* Add new node */
	node = map_newnode(key, value, vsize);
	if (node == NULL) goto fail;
	if (m->nnodes >= m->nbuckets) {
		n = (m->nbuckets > 0) ? (m->nbuckets << 1) : 1;
		err = map_resize(m, n);
		if (err) goto fail;
	}
    #ifdef MPARC_MEM_DEBUG_VERBOSE
    {
		// unsafe magic
		MPARC_blob_store* storeptr = (MPARC_blob_store*) value;
		MPARC_blob_store store = *storeptr;
        fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "View of %s after being added to map with sizeof %"PRIuFAST64":\n", key, store.binary_size);
        for(MXPSQL_MPARC_uint_repr_t i = 0; i < store.binary_size; i++){
                fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", store.binary_blob[i]);
        }
        fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
    }
    #endif
	map_addnode(m, node);
	m->nnodes++;
	return 0;
	fail:
	if (node) MPARC_free(node);
	return -1;
}


static void map_remove_(map_base_t *m, const char *key) {
	map_node_t *node;
	map_node_t **next = map_getref(m, key);
	if (next) {
		node = *next;
		*next = (*next)->next;
		if(node) MPARC_free(node);
		m->nnodes--;
	}
}


static map_iter_t map_iter_(void) {
	map_iter_t iter;
	iter.bucketidx = -1;
	iter.node = NULL;
	return iter;
}


static const char *map_next_(map_base_t *m, map_iter_t *iter) {
	if (iter->node) {
		iter->node = iter->node->next;
		if (iter->node == NULL) goto nextBucket;
	} else {
		nextBucket:
		do {
			if (++iter->bucketidx >= m->nbuckets) {
				return NULL;
			}
			iter->node = m->buckets[iter->bucketidx];
		} while (iter->node == NULL);
	}
	return (char*) (iter->node + 1);
}

#endif

/* end of map.h section */

/* Linked List from https://gist.github.com/meylingtaing/11018042 and modified for memory safety, no output randomly and make it an actual list instead of a stack (it makes a great stack instead of a linked list :P) */

/* struct node {
		void *data;
		struct node *next;
};

typedef struct node * llist;

llist *llist_create(void *new_data)
{
		struct node *new_node;

		llist *new_list = (llist *)MPARC_malloc(sizeof (llist));
		CHECK_LEAKS();
		if(new_list == NULL){
				return NULL;
		}
		*new_list = (struct node *)MPARC_malloc(sizeof (struct node));
		CHECK_LEAKS();
		if(new_list == NULL){
				return NULL;
		}
		
		new_node = *new_list;
		new_node->data = new_data;
		new_node->next = NULL;
		return new_list;
}

void llist_free(llist *list)
{
		struct node *curr = *list;
		struct node *next;

		while (curr != NULL) {
				next = curr->next;
				MPARC_free(curr);
				curr = next;
		}

		MPARC_free(list);
}

// Returns 0 on failure
int llist_add_inorder(void *data, llist *list,
											 int (*comp)(void *, void *))
{
		struct node *new_node;
		struct node *curr;
		struct node *prev = NULL;
		
		if (list == NULL || *list == NULL) {
				// fprintf(stderr, "llist_add_inorder: list is null\n");
				return 0;
		}
		
		curr = *list;
		if (curr->data == NULL) {
				curr->data = data;
				return 1;
		}

		new_node = (struct node *)MPARC_malloc(sizeof (struct node));
		CHECK_LEAKS();
		if(!new_node){
			return 0;
		}
		new_node->data = data;

		// Find spot in linked list to insert new node
		while (curr != NULL && curr->data != NULL && comp(curr->data, data) < 0) {
				prev = curr;
				curr = curr->next;
		}
		new_node->next = curr;

		if (prev == NULL) 
				*list = new_node;
		else 
				prev->next = new_node;

		return 1;
}

// returns 0 on failure
int llist_push(llist *list, void *data)
{
		struct node *head;
		struct node *new_node;
		if (list == NULL || *list == NULL) {
				// fprintf(stderr, "llist_add_inorder: list is null\n");
				return 0;
		}

		head = *list;
		
		// Head is empty node
		if (head->data == NULL){
				head->data = data;
				return 1;
		}

		// Head is not empty, add new node to front
		else {
				new_node = MPARC_malloc(sizeof (struct node));
				CHECK_LEAKS();
				if(new_node == NULL){
						return 0;
				}
				new_node->data = data;
				new_node->next = head;
				*list = new_node;
				return 1;
		}
}

void *llist_peek(llist* list){
		struct node *head = *list;
		if(list == NULL || head->data == NULL)
				return NULL;

		return head->data;
}

void *llist_pop(llist *list)
{
		void *popped_data;
		struct node *head = *list;

		if (list == NULL || head->data == NULL)
				return NULL;
		
		popped_data = llist_peek(list);
		*list = head->next;

		MPARC_free(head);

		return popped_data;
} */

/* end of llist.h and llist.c section */


/* start of LZ77 Section: FastL7 */
/* https://github.com/ariya/FastLZ source */
/* 
 * Have a bowl of this MIT License
 * 
 * FastLZ - Byte-aligned LZ77 compression library
 * Copyright (C) 2005-2020 Ariya Hidayat <ariya.hidayat@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/

// Nah, but I can still put it in here though
// Oh hey, free advertisement!

/* end of LZ77 Section: FastL7 */

/* perfect dynamic list by Attractive Chaos at https://github.com/attractivechaos/klib/blob/master/kvec.h */
/* The MIT License
   Copyright (c) 2008, by Attractive Chaos <attractor@live.co.uk>
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:
   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/
#define kv_roundup32(x) (--(x), (x) |= (x) >> 1, (x) |= (x) >> 2, (x) |= (x) >> 4, (x) |= (x) >> 8, (x) |= (x) >> 16, ++(x))

#define kvec_t(type) \
	struct           \
	{                \
		size_t n, m; \
		type *a;     \
	}
#define kv_init(v) ((v).n = (v).m = 0, (v).a = 0)
#define kv_destroy(v) free((v).a)
#define kv_A(v, i) ((v).a[(i)])
#define kv_pop(v) ((v).a[--(v).n])
#define kv_size(v) ((v).n)
#define kv_max(v) ((v).m)

#define kv_resize(type, v, s) ((v).m = (s), (v).a = (type *)realloc((v).a, sizeof(type) * (v).m))

#define kv_copy(type, v1, v0)                          \
	do                                                 \
	{                                                  \
		if ((v1).m < (v0).n)                           \
			kv_resize(type, v1, (v0).n);               \
		(v1).n = (v0).n;                               \
		memcpy((v1).a, (v0).a, sizeof(type) * (v0).n); \
	} while (0)

#define kv_push(type, v, x)                                       \
	do                                                            \
	{                                                             \
		if ((v).n == (v).m)                                       \
		{                                                         \
			(v).m = (v).m ? (v).m << 1 : 2;                       \
			(v).a = (type *)realloc((v).a, sizeof(type) * (v).m); \
		}                                                         \
		(v).a[(v).n++] = (x);                                     \
	} while (0)

#define kv_pushp(type, v) (((v).n == (v).m) ? ((v).m = ((v).m ? (v).m << 1 : 2),                        \
											   (v).a = (type *)realloc((v).a, sizeof(type) * (v).m), 0) \
											: 0),                                                       \
						  ((v).a + ((v).n++))

/* TINY SNIPPETS THAT WILL BE VERY USEFUL LATER ON OK SECTION */


/* Glibc Strtok, Here's Your LGPL Notice and Why I Dual Licensed it under the LGPL and MIT

		Source: https://codebrowser.dev/glibc/glibc/string/strtok_r.c.html

		Reentrant string tokenizer.  Generic version.
		Copyright (C) 1991-2022 Free Software Foundation, Inc.
		This file is part of the GNU C Library.
		The GNU C Library is free software; you can redistribute it and/or
		modify it under the terms of the GNU Lesser General Public
		License as published by the Free Software Foundation; either
		version 2.1 of the License, or (at your option) any later version.
		The GNU C Library is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
		Lesser General Public License for more details.
		You should have received a copy of the GNU Lesser General Public
		License along with the GNU C Library; if not, see
		<https://www.gnu.org/licenses/>.  */
char* MPARC_strtok_r (char *s, const char *delim, char **save_ptr)
{
	char *end;
	if (s == NULL)
		s = *save_ptr;
	if (*s == '\0')
		{
			*save_ptr = s;
			return NULL;
		}
	/* Scan leading delimiters.  */
	s += strspn (s, delim);
	if (*s == '\0')
		{
			*save_ptr = s;
			return NULL;
		}
	/* Find the end of the token.  */
	end = s + strcspn (s, delim);
	if (*end == '\0')
		{
			*save_ptr = end;
			return s;
		}
	/* Terminate the token and make *SAVE_PTR point past it.  */
	*end = '\0';
	*save_ptr = end + 1;
	return s;
}

/**
 * @brief My strnlen
 * 
 * @param str string to check
 * @param maxlen max length
 * @return size_t length of str or maxlen
 */
size_t MPARC_strnlen(const char* str, size_t maxlen){
	size_t i = 0;

	for(i = 0; i < maxlen; i++,str++ ){
		if(!*str){
			break;
		}
	}

	return i;
}

/**
 * @brief Our own strdup, but for binary arrays.
 * 
 * @param src Source array, interpreted as unsigned char*
 * @param len length of array, interpreted as unsigned char*
 * @return void* New duplicated array
 */
void* MPARC_memdup(const void* src, size_t len){
	unsigned char* ustr;
	const unsigned char* usrc = src;

	ustr = (unsigned char*) MPARC_malloc(len);
	CHECK_LEAKS();
	if(!ustr) return NULL;

	memset(ustr, '\0', len);

	#ifdef MPARC_MEM_DEBUG_VERBOSE
	printf("memdup> ustr = %p (%s), src = %p (%s)\n", ustr, ((ustr == NULL) ? "NULL" : "NOT NULL"), src, (src == NULL) ? "NULL" : "NOT NULL");
	#endif

	return memcpy(ustr, usrc, len);
}

char* MPARC_strndup(const char* src, size_t ilen){
	size_t len = MPARC_strnlen(src, ilen)+1;

	char* dupstr = MPARC_memdup(src, len*sizeof(char));

	return dupstr;
}

char* MPARC_strdup(const char* src){
	return MPARC_strndup(src, strlen(src));
}

#ifdef MPARC_MEM_DEBUG_VERBOSE
#define MPARC_memdup(src, len) MPARC_memdup(src, len); fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "MPARC_memdup %s:%d\n", __FILE__, __LINE__);
#define MPARC_strndup(src, ilen) MPARC_strndup(src, ilen); fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "MPARC_strndup %s:%s", __FILE__, __LINE__);
#define MPARC_strdup(src) MPARC_strdup(src); fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "MPARC_strdup %s:%d\n", __FILE__, __LINE__);
#endif

// bionic's turn to get copy pasted on: https://android.googlesource.com/platform/bionic/+/ics-mr0/libc/string/strlcpy.c
// This is a blatantly obvious attempt to use the strl* family of functions
// static this thing, we use our representation of integers
static MXPSQL_MPARC_uint_repr_t strlcpy(char *dst, const char *src, MXPSQL_MPARC_uint_repr_t siz)
{
	char *d = dst;
	const char *s = src;
	MXPSQL_MPARC_uint_repr_t n = siz;
	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}
	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}
	return(s - src - 1);	/* count does not include NUL */
}

// from glibc from https://github.com/lattera/glibc/blob/master/string/basename.c
char* MPARC_basename (const char *filename)
{
	char* p = NULL;
	#if (defined(_WIN32) || defined(_WIN64)) && !(defined(__CYGWIN__))
	p = strrchr(filename, '/');
	p = p ? p : strrchr(filename, '\\');
	#else
	p = strrchr (filename, '/');
	#endif
	return p ? p + 1 : (char *) filename;
}

// from glibc from https://github.com/lattera/glibc/blob/master/misc/dirname.c
char* MPARC_dirname (char *path)
{
  static const char dot[] = ".";
  char *last_slash;

  /* Find last '/'.  */
  last_slash = path != NULL ? strrchr (path, '/') : NULL;

  if (last_slash != NULL && last_slash != path && last_slash[1] == '\0')
	{
	  /* Determine whether all remaining characters are slashes.  */
	  char *runp;

	  for (runp = last_slash; runp != path; --runp){
		if (runp[-1] != '/'){
		  break;
		}
	  }

	  /* The '/' is the last character, we have to look further.  */
	  if (runp != path)
		last_slash = memchr (path, '/', runp - path);
	}

  if (last_slash != NULL)
	{
	  /* Determine whether all remaining characters are slashes.  */
	  char *runp;

	  for (runp = last_slash; runp != path; --runp){
		if (runp[-1] != '/'){
		  break;
		}
	  }

	  /* Terminate the path.  */
	  if (runp == path)
		{
		  /* The last slash is the first character in the string.  We have to
			 return "/".  As a special case we have to return "//" if there
			 are exactly two slashes at the beginning of the string.  See
			 XBD 4.10 Path Name Resolution for more information.  */
		  if (last_slash == path + 1){
			++last_slash;
		  }
		  else{
			last_slash = path + 1;
		  }
		}
		else{
			last_slash = runp;
		}

	  last_slash[0] = '\0';
	}
  else{
	/* This assignment is ill-designed but the XPG specs require to
	   return a string containing "." in any case no directory part is
	   found and so a static and constant string is required.  */
	path = (char *) dot;
  }

  return path;
}

// original filename stuff
// full_or_not is 0 if only the last part of the extension is needed, else (1,2,3,etc...) will include every thing
char* MPARC_get_extension(const char* fnp, int full_or_not){
	char* fn = MPARC_dirname((char*) fnp);
	char* epos = NULL;
	if(full_or_not == 0){
		epos = strrchr(fn, '.');
	}
	else{
		epos = strchr(fn, '.');
	}

	if(epos){
		epos++;
	}
	return epos;
}

static int MPARC_strncmp (const char *s1, const char *s2, size_t n)
{
	unsigned char c1 = '\0';
	unsigned char c2 = '\0';

  if (n >= 4)
    {
      size_t n4 = n >> 2;
      do
	{
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	} while (--n4 > 0);
      n &= 3;
    }

  while (n > 0)
    {
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
	return c1 - c2;
      n--;
    }

  return c1 - c2;
}

// bsearch and strcmp
// a lie, it does nto use strcmp, it uses memcmp
static int voidstrcmp(const void* str1, const void* str2){
	if(!str1 || !str2){
		return 0;
	}
	#ifdef MPARC_QSORT
	return MPARC_strncmp((const char*) str1, (const char*) str2, 5);
	#else
	// bodging
	((void)str1);
	((void)str2);
	((void)MPARC_strncmp);
	return 0; // bodge
	#endif
}

// future, may not be used
/// @brief endian
static int isLittleEndian(void){
	volatile uint32_t i=0x01234567;
	// return 0 for big endian, 1 for little endian.
	return (*((uint8_t*)(&i))) == 0x67;
}


static unsigned char* XORCipher(const unsigned char* bytes_src, MXPSQL_MPARC_uint_repr_t length, const unsigned char* keys, MXPSQL_MPARC_uint_repr_t keylength){
	((void)isLittleEndian);
	unsigned char* bytes_out = MPARC_memdup(bytes_src, length);
	if(bytes_out){
		for(MXPSQL_MPARC_uint_repr_t i = 0; i < length; i++){
			// check for division by zero
			unsigned char byte = bytes_out[i];
			if(keylength != 0){
				unsigned char tmpbyte = byte ^ keys[length % keylength];
				byte = tmpbyte;
			}
			bytes_out[i] = byte;
		}
	}
	return bytes_out;
}

static unsigned char* ROTCipher(const unsigned char * bytes_src, MXPSQL_MPARC_uint_repr_t length, const int* keys, MXPSQL_MPARC_uint_repr_t keylength){
	unsigned char* bytes_out = MPARC_memdup(bytes_src, length);
	if(bytes_out){
		for(MXPSQL_MPARC_uint_repr_t i = 0; i < length; i++){
			unsigned char byte = bytes_out[i];
			if(keylength != 0){
				unsigned char tmpbyte = byte + (keys[length % keylength]);
				byte = tmpbyte;
			}
			bytes_out[i] = byte;
		}
	}
	return bytes_out;
}

/* END OF SNIPPETS */


/* BEGINNING OF MY SECTION OK */



		/* MY LINKED LIST IMPL OK, UNUSED OK */

		// /**
		//  * @brief linked list node
		//  * 
		//  */
		// struct llist_node {
		// 	/// value
		// 	void* value;
		// 	/// next node
		// 	struct llist_node* next;
		// };

		// /// llist_node typedef'ed as an iterator
		// typedef struct llist_node* llist_iterator;

		// /**
		//  * @brief The linked list entity
		//  * 
		//  */
		// typedef struct {
		// 	/// head pointer
		// 	struct llist_node* head;
		// 	/// type in the list
		// 	char* type;
		// } llist;
 

		// MXPSQL_MPARC_err llist_make(llist** list, char* type){
		// 	if(!list) return MPARC_IVAL;
		// 	llist* ls = calloc(1, sizeof(llist));
		// 	if(ls){
		// 		ls->head = NULL;
		// 		ls->type = type;
		// 	}
		// 	else{
		// 		return MPARC_OOM;
		// 	}
		// 	*list = ls;
		// 	return MPARC_OK;
		// }

		// MXPSQL_MPARC_err llist_iterate(llist* list, llist_iterator* iterator){
		// 	if(!list->head) return MPARC_IVAL;
		// 	if(!iterator) return MPARC_IVAL;
		// 	*iterator = list->head;
		// 	return MPARC_OK;
		// }

		// MXPSQL_MPARC_err llist_traverse(llist_iterator restrict iterator, llist_iterator restrict next){
		// 	*next = (*iterator)->next;
		// 	return MPARC_OK;
		// }

		// MXPSQL_MPARC_err llist_len(llist* list, int* out){
		// 	return MPARC_OK;	
		// }

		// MXPSQL_MPARC_err llist_insert(llist* list, int idx, void* value, char* type){
		// 	if(strcmp(list->type, type) != 0) return MPARC_IVAL;

		// 	struct llist_node* node = calloc(1, sizeof(struct llist_node));
		// 	if(!node) return MPARC_OOM;

		// 	node->value = value;

		// 	{
		// 		llist_iterator iterator;
		// 		llist_iterate(list, &iterator);

		// 		for(int ix = 0; ix >= idx && iterator != NULL; ix++){

 
		// 			llist_iterator next;
		// 			llist_traverse(iterator, next);
		// 			iterator = next;
		// 		}
		// 	}
		// 	return MPARC_OK;
		// }

		// MXPSQL_MPARC_err llist_destroy(llist** list){
		// 	llist_iterator i = NULL;
		// 	llist_iterator tmp = NULL;
		// 	for(llist_iterate(list, &i); i != NULL; i = tmp){
		// 		tmp = i->next;
		// 		free(i);
		// 	}
		// 	free(list);
		// 	return MPARC_OK;
		// }



		

		/* MAIN CODE OK */
		/// Backend storage
		typedef map_t(MPARC_blob_store) map_blob_t;

		/// List for preprocessor
		typedef kvec_t(MXPSQL_MPARC_preprocessor_func_t) MXPSQL_MPARC_preprocessor_list;

#endif

		/// @brief Not the best place to find documentation for this struct, see mparc.h for better info. This however tells you about the members.
		struct MXPSQL_MPARC_t {
				/* separator markers */
				/// separate the 25 character long magic number from the rest of the archive
				char magic_byte_sep; 
				/// separate the version number from implementation specific metadata
				char meta_sep; 
				/// a new line, but usually used to separate entries
				char entry_sep_or_general_marker; 
				/// unused, but can be implemented, other implementations can use this as extension metadata
				char comment_marker; 
				/// indicate beginning of entries
				char begin_entry_marker; 
				/// separate checksum from entry contents
				char entry_elem2_sep_marker_or_magic_sep_marker; 
				/// indicate end of entry
				char end_entry_marker; 
				/// indicate end of file
				char end_file_marker; 

				/* metadata */
				/// version that is used when creating archive
				STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION writerVersion; 
				/// version that indicates what version of the archive is loaded
				STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION loadedVersion; 

				/// storage actually
				map_blob_t globby;

				/// Preprocessors for processing entry (unused)
				MXPSQL_MPARC_preprocessor_list entry_content_preprocessor;

				/// Internal error reporting
				MXPSQL_MPARC_err my_err;


				/* Ciphers for encryption, just basic one;You want strong one? It's a DIY for you, I am not doing it so you do it. */
				/// XOR Cipher stuff: Key, NULL to disable it
				unsigned char* XORKey;
				/// XOR Cipher stuff: Length
				MXPSQL_MPARC_uint_repr_t XORKeyLength;

				/// ROT Cipher stuff: Key, NULL to disable it
				int* ROTKey;
				/// ROT Cipher stuff: Length
				MXPSQL_MPARC_uint_repr_t ROTKeyLength;
				

				
				/// hidden
				va_list vlist;
		};

		/**
		 * @brief Iterator
		*/
		struct MXPSQL_MPARC_iter_t {
			/// current archive
			MXPSQL_MPARC_t* archive;
			/// storage iterator
			map_iter_t itery;
		};


		MXPSQL_MPARC_err MPARC_get_last_error(MXPSQL_MPARC_t** structure, MXPSQL_MPARC_err* out){
			if(!structure || !*structure || !out) return MPARC_NULL;
			MXPSQL_MPARC_err err = (*structure)->my_err;
			*out = err;

			if(err < MPARC_OK) return MPARC_IVAL;
			else return MPARC_OK;
		}

		int MPARC_strerror(MXPSQL_MPARC_err err, char** out){
			switch(err){
				case MPARC_OK:
				*out = MPARC_strdup("it fine, no error.");
				break;

				case MPARC_IDK:
				*out = MPARC_strdup("it fine cause idk, unknown error.");
				return 1;
				case MPARC_INTERNAL:
				*out = MPARC_strdup("Internal error detected. It is a serious error to see this, because that means MPARC has a bug. You should consider aborting the program.");
				return 2;


				case MPARC_IVAL:
				*out = MPARC_strdup("Bad vals or just generic but less generic than MPARC_IDK.");
				break;

				case MPARC_KNOEXIST:
				*out = MPARC_strdup("It does not exist you dumb dumb, basically that key does not exist.");
				break;
				case MPARC_KEXISTS:
				*out = MPARC_strdup("Key exists.");
				break;

				case MPARC_OOM:
				*out = MPARC_strdup("Oh noes I run out of memory, you need to deal with your ram cause there is a problem with your memory due to encountering out of memory condition.");
				break;

				case MPARC_NOTARCHIVE:
				*out = MPARC_strdup("You dumb person what you put in is not an archive by the 25 character long magic number it has or maybe we find out it is not a valid archive by the json or anything else.");
				break;
				case MPARC_ARCHIVETOOSHINY:
				*out = MPARC_strdup("You dumb person the valid archive you put in me is too new for me to process. This archive processor may be version 1, but the archive is version 2, maybe.");
				break;
				case MPARC_CHKSUM:
				*out = MPARC_strdup("My content is gone or I can't write my content properly because it failed the CRC32 test :P");
				break;
				case MPARC_NOCRYPT:
				*out = MPARC_strdup("Haha, you would got garbage data or more likely, checksum errors if I did not stop you. You did not set encryption.");
				break;

				case MPARC_CONSTRUCT_FAIL:
				*out = MPARC_strdup("Failed to construct archive.");
				break;

				case MPARC_OPPART:
				*out = MPARC_strdup("Operation was not complete, continue the operation after giving it the remedy it needs.");
				break;

				case MPARC_FERROR:
				*out = MPARC_strdup("FILE.exe has stopped responding as there is a problem with the FILE IO operation.");
				break;

				default:
				*out = MPARC_strdup("Man what happened here that was not a valid code.");
				return -1;
			}
			return 0;
		}

		int MPARC_sfperror(MXPSQL_MPARC_err err, FILE* filepstream, const char* emsg){
			char* s = "";
			// printf("%d", err);
			int r = MPARC_strerror(err, &s);
			if(s == NULL){
				fprintf(filepstream, "%sFailed to get error message. (%d)\n", emsg, err);
				return r;
			}
			else{
				fprintf(filepstream, "%s%s (%d)\n", emsg, s, err);
				MPARC_free(s);
			}
			return r;
		}

		int MPARC_fperror(MXPSQL_MPARC_err err, FILE* fileptrstream){
			return MPARC_sfperror(err, fileptrstream, "");
		}

		int MPARC_perror(MXPSQL_MPARC_err err){
			return MPARC_fperror(err, stderr);
		}


		static MXPSQL_MPARC_err MPARC_i_push_ufilestr_advancea(MXPSQL_MPARC_t* structure, const char* filename, bool stripdir, bool overwrite, unsigned char* ustringc, MXPSQL_MPARC_uint_repr_t sizy, crc_t crc3){
			MPARC_blob_store blob = {
				sizy,
				MPARC_memdup(ustringc, sizy),
				crc3
			};

			const char* pfilename = NULL;
			if(stripdir){
				pfilename = MPARC_basename(pfilename);
			}
			else{
				pfilename = filename;
			}

			if(!overwrite && MPARC_exists(structure, pfilename) != MPARC_KNOEXIST){
				return MPARC_KEXISTS;
			}

			#ifdef MPARC_MEM_DEBUG_VERBOSE
			{
				fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Printing %s after doing some static advancea magic with size of %"PRIuFAST64":\n", filename, sizy);
				for(MXPSQL_MPARC_uint_repr_t i = 0; i < sizy; i++){
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", ustringc[i]);
				}
				fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
			}
			#endif

			if(map_set(&structure->globby, pfilename, blob) != 0){
				return MPARC_IVAL;
			}

			return MPARC_OK;
		}

		static int MPARC_i_sortcmp(const void* p1, const void* p2){
			if(!p1 || !p2) return 0;
			const char* str1 = NULL;
			const char* str2 = NULL;

			// str1 = *((const char**) p1);
			// str2 = *((const char**) p2);
			str1 = (const char*) p1;
			str2 = (const char*) p2;

			int spress = 0;

			switch(MPARC_QSORT_MODE){
				case 0: {
					// dumb sort
					// most likely checksum will do the work of sorting it
					spress = voidstrcmp(str1, str2);
					break;
				}

				case 1: {
					// smart sort
					char* str1d = NULL;
					char* str2d = NULL;

					{
						str1d = MPARC_strdup(str1);
						if(str1d == NULL) goto me_my_errhandler;
						str2d = MPARC_strdup(str2);
						if(str2d == NULL) goto me_my_errhandler;
					}

					char* sav = NULL;
					char sep[2] = {MPARC_MAGIC_CHKSM_SEP, '\0'};

					char* str1fnam = NULL;
					char* str2fnam = NULL;
					{
						char* tok = MPARC_strtok_r(str1d, sep, &sav);
						if(tok == NULL || strcmp(sav, "") == 0 || sav == NULL){
							goto me_my_errhandler;
						}

						{
							// ease, use json.c
							JsonNode* root = json_decode(sav);
							JsonNode* node = NULL;
							json_foreach(node, root) {
								// ignore non string ones
								if(node->tag == JSON_STRING){
									if(strcmp(node->key, "filename") == 0){
										str1fnam = node->store.string;
									}
								}
							};
						}
					}
					{
						char* tok = MPARC_strtok_r(str2d, sep, &sav);
						if(tok == NULL || strcmp(sav, "") == 0 || sav == NULL){
							goto me_my_errhandler;
						}

						{
							// ease, use json.c
							JsonNode* root = json_decode(sav);
							JsonNode* node = NULL;
							json_foreach(node, root) {
								// ignore non string ones
								if(node->tag == JSON_STRING){
									if(strcmp(node->key, "filename") == 0){
										str2fnam = node->store.string;
									}
								}
							};
						}
					}

					spress = voidstrcmp(str1fnam, str2fnam);
					// fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "FILENAME1(%s)<=>FILENAME2(%s):%d\n", str1fnam, str2fnam, spress);

					goto me_my_errhandler;

					me_my_errhandler:
					{
						if(str1d) MPARC_free(str1d);
						if(str2d) MPARC_free(str2d);
					}

					break;
				}

				default: {
					double difftimet = difftime(time(NULL), time(NULL));
					srand((unsigned int) difftimet);
					spress = (rand() % 30) - 15;
					break;
				}
			};

			if(spress > 0) return 1;
			else if(spress < 0) return -1;
			else if(spress == 0) return 0;
			else return 0;
		}
		
		static MXPSQL_MPARC_err MPARC_i_sort(char** sortedstr){
			if(sortedstr == NULL) return MPARC_IVAL;

			MXPSQL_MPARC_uint_repr_t counter = 0;
			{
				MXPSQL_MPARC_uint_repr_t i = 0;
				for(i = 0; sortedstr[i] != NULL; i++){
					#ifdef MPARC_MEM_DEBUG_VERBOSE
					printf("sort %"PRIuFAST64"> %p (NULL: %d)\n", i, sortedstr[i], (sortedstr[i] == NULL) ? 1 : 0);
					#endif
				}
				counter = i-1;
                if(counter < 1) counter = 1;
			}
			{
				#ifdef MPARC_QSORT
				qsort(sortedstr, counter, sizeof(*sortedstr), MPARC_i_sortcmp);
				qsort(sortedstr, counter, sizeof(*sortedstr), MPARC_i_sortcmp); // double for the fun (and redundancy)
				#else
				((void)MPARC_i_sortcmp);
				#endif
			}
			/* { // I forgot this piece, is this insertion sort? Or is it bubble sort?
				MXPSQL_MPARC_uint_repr_t i = 0, j = 0;
				char* temp = NULL;
				for (i=0;i<counter-1;i++)
				{
					for (j=0;j<counter-i-1;j++)
					{
						if (MPARC_i_sortcmp(sortedstr[j], sortedstr[j + 1] ) > 0)  // more readable with indexing syntax
						{
							temp = sortedstr[j];
							sortedstr[j] = sortedstr[j+1];
							sortedstr[j+1] = temp;
						}
					}
				}
			} */
			return MPARC_OK;
		}

		static char* MPARC_i_construct_header(MXPSQL_MPARC_t* structure){
			JsonNode* nod = json_mkobject();
			if(!nod) return NULL;

			{
				// set flags to nod
				{
					// encryption flag
					JsonNode* ecrypt = json_mkarray();
					if(!ecrypt) return NULL;

					unsigned char* XORStat = NULL;
					int* ROTStat = NULL;
					MPARC_cipher(structure, 
					0, NULL, 0, &XORStat, NULL, 
					0, NULL, 0, &ROTStat, NULL);

					if(XORStat){
						JsonNode* XORNode = json_mkstring("XOR");
						json_append_element(ecrypt, XORNode);
					}
					if(ROTStat){
						JsonNode* ROTNode = json_mkstring("ROT");
						json_append_element(ecrypt, ROTNode);
					}

					json_append_member(nod, "encrypt", ecrypt);
				}
			}

			char* s = json_encode(nod);
			if(!s) return NULL;

			{
				static char* fmt = STANKY_MPAR_FILE_FORMAT_MAGIC_NUMBER_25"%c%llu%c%s%c";
				int sps = snprintf(NULL, 0, fmt, structure->magic_byte_sep, structure->writerVersion, structure->meta_sep, s, structure->begin_entry_marker);
				if(sps < 0){
						return NULL;
				}
				char* alloc = MPARC_calloc(sps+1, sizeof(char));
				CHECK_LEAKS();
				if(alloc == NULL) return NULL;
				if(snprintf(alloc, sps+1, fmt, structure->magic_byte_sep, structure->writerVersion, structure->meta_sep, s, structure->begin_entry_marker) < 0){
						if(alloc) MPARC_free(alloc);
						return NULL;
				}
				else{
					return alloc;
				}
			}
		}

		static char* MPARC_i_construct_entries(MXPSQL_MPARC_t* structure, MXPSQL_MPARC_err* eout){
				char* estring = NULL;

				char** jsonry = NULL;
				MXPSQL_MPARC_uint_repr_t jsonentries;

				{
					MXPSQL_MPARC_err err = MPARC_OK;
					if((err = MPARC_list_array(structure, NULL, &jsonentries)) != MPARC_OK){
						if(eout) *eout = err;
						return NULL;
					}
				}

				jsonry = MPARC_calloc(jsonentries+1, sizeof(char*));
				CHECK_LEAKS();
				jsonry[jsonentries]=NULL;

				const char* nkey;
				MXPSQL_MPARC_iter_t* itery = NULL;
				// map_iter_t itery = map_iter(&structure->globby);
				MXPSQL_MPARC_uint_repr_t indexy = 0;

				{
					MXPSQL_MPARC_err err = MPARC_list_iterator_init(&structure, &itery);
					if(err != MPARC_OK){
						structure->my_err = err;
						if(eout) *eout = MPARC_KNOEXIST;
						MPARC_list_iterator_destroy(&itery);
						goto errhandler;
					}
				}

				while((MPARC_list_iterator_next(&itery, &nkey)) == MPARC_OK){
					printf("%s\n", nkey);
						MPARC_blob_store* bob_the_blob_raw = map_get(&structure->globby, nkey);
						if(!bob_the_blob_raw){
							continue;
						}
						crc_t crc3 = crc_init();
						JsonNode* objectweb = json_mkobject();
						MPARC_blob_store bob_the_blob = *bob_the_blob_raw;
						unsigned char* btob = NULL;
						MXPSQL_MPARC_uint_repr_t sizey = 0;
						{
							unsigned char* blobg = MPARC_memdup(bob_the_blob.binary_blob, bob_the_blob.binary_size);
							{
								// crypt blob
								unsigned char* XORK = NULL;
								MXPSQL_MPARC_uint_repr_t XORL = 0;
								int* ROTK = NULL;
								MXPSQL_MPARC_uint_repr_t ROTL = 0;

								MPARC_cipher(structure, 
								0, NULL, 0, &XORK, &XORL, 
								0, NULL, 0, &ROTK, &ROTL);

								if(XORK){
									unsigned char* tblobg = XORCipher(blobg, bob_the_blob.binary_size, XORK, XORL);
									if(!tblobg) return NULL;
									unsigned char* oblobg = blobg;
									blobg = tblobg;
									MPARC_free(oblobg);
								}

								if(ROTK){
									unsigned char* tblobg = ROTCipher(blobg, bob_the_blob.binary_size, ROTK, ROTL);
									if(!tblobg) return NULL;
									unsigned char* oblobg = blobg;
									blobg = tblobg;
									MPARC_free(oblobg);
								}
							}

							{
								// perform base64
								if(bob_the_blob.binary_size < 1){
									btob = (unsigned char*) MPARC_strdup("");
									sizey = strlen((char*) btob); // redundant, but more programatic
								}
								else{
									btob = b64.btoa(blobg, bob_the_blob.binary_size, &sizey);
								}
							}
						}

						#ifdef MPARC_MEM_DEBUG_VERBOSE
						{
							fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Viewing preb64 bytes of %s with size of %"PRIuFAST64":\n", nkey, bob_the_blob.binary_size);
							for(MXPSQL_MPARC_uint_repr_t i = 0; i < bob_the_blob.binary_size; i++){
								fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", bob_the_blob.binary_blob[i]);
							}
							fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");

							{
								unsigned char* bblob = NULL;
								MXPSQL_MPARC_uint_repr_t bsea = 0;
								bblob = b64.atob(btob, strlen((char*) btob), &bsea);
								fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Viewing unb64 construction bytes of %s with size of %"PRIuFAST64":\n", nkey, bsea);
								for(MXPSQL_MPARC_uint_repr_t i = 0; i < bsea; i++){
									fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", bblob[i]);
								}
								fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
								MPARC_free(bblob);
							}
						}
						#endif

						crc3 = bob_the_blob.binary_crc;
						if(btob == NULL) {
							if(eout) *eout = MPARC_OOM;
							MPARC_list_iterator_destroy(&itery);
							goto errhandler;
						}
						JsonNode* glob64 = json_mkstring((char*) btob);
						JsonNode* filename = json_mkstring(nkey);
						JsonNode* blob_chksum = NULL;
						if(glob64 == NULL || filename == NULL) {
							if(eout) *eout = MPARC_OOM;
							MPARC_list_iterator_destroy(&itery);
							goto errhandler;
						}

						{
							static char* fmter = "%"PRIuFAST32"";
							char* globsum = NULL;
							int size = snprintf(NULL, 0, fmter, crc3);
							if(size < 0){
								if(eout) *eout = MPARC_CONSTRUCT_FAIL;
								if(jsonry) MPARC_free(jsonry);
								goto errhandler;
							}
							// +5 is hacky
							globsum = MPARC_calloc(size+5, sizeof(char));
							CHECK_LEAKS();
							if(globsum == NULL){
								if(eout) *eout = MPARC_OOM;
								MPARC_list_iterator_destroy(&itery);
								goto errhandler;
							}
							int ps = 0;
							if((ps = snprintf(globsum, size+5, fmter, crc3)) < size){
								if(eout) *eout = MPARC_CONSTRUCT_FAIL;
								goto errhandler;
							}
							blob_chksum = json_mkstring(globsum);
							if(blob_chksum == NULL){
								if(eout) *eout = MPARC_OOM;
								MPARC_list_iterator_destroy(&itery);
								goto errhandler;
							}
							if(globsum) MPARC_free(globsum);
						}
						json_append_member(objectweb, "filename", filename);
						json_append_member(objectweb, "blob", glob64);
						json_append_member(objectweb, "crcsum", blob_chksum);
						if(!json_check(objectweb, NULL)){
							if(eout) *eout = MPARC_KNOEXIST;
							MPARC_list_iterator_destroy(&itery);
							goto errhandler;
						}
						char* stringy = json_encode(objectweb);
						if(!stringy) {
							if(eout) *eout = MPARC_CONSTRUCT_FAIL;
							MPARC_list_iterator_destroy(&itery);
							goto errhandler;
						}


						{
							char* crcStringy;
							crc_t crc = crc_init();
							crc = crc_update(crc, stringy, strlen(stringy));
							crc = crc_finalize(crc);
							{
								static char* fmt = "%"PRIuFAST32"%c%s";

								int sp = snprintf(NULL, 0, fmt, crc, structure->entry_elem2_sep_marker_or_magic_sep_marker, stringy)+10; // silly hack workaround, somehow snprintf is kind of broken in this part
								crcStringy = MPARC_calloc((sp+1),sizeof(char));
								CHECK_LEAKS();
								if(crcStringy == NULL){
									if(eout) *eout = MPARC_OOM;
									MPARC_list_iterator_destroy(&itery);
									goto errhandler;
								}
								if(snprintf(crcStringy, sp, fmt, crc, structure->entry_elem2_sep_marker_or_magic_sep_marker, stringy) < 0){
									if(eout) *eout = MPARC_CONSTRUCT_FAIL;
									if(crcStringy) MPARC_free(crcStringy);
									MPARC_list_iterator_destroy(&itery);
									goto errhandler;
								}
								jsonry[indexy] = crcStringy;
							}
						}

						indexy++;
				}

				MPARC_list_iterator_destroy(&itery);

				// ((void)MPARC_i_sort);
				MPARC_i_sort(jsonry); // We qsort this, qsort can be quicksort, insertion sort or BOGOSORT. It is ordered by the checksum instead of the name lmao (or the name, yeah we set to name).

				{
						MXPSQL_MPARC_uint_repr_t iacrurate_snprintf_len = 1;

						for(MXPSQL_MPARC_uint_repr_t i = 0; i < jsonentries; i++){
							if(!jsonry[i]){ // DODGY BODGE
								MPARC_free(jsonry[i]);
								jsonry[i] = MPARC_strdup("");
								if(!jsonry[i]){
									if(eout) *eout = MPARC_OOM;
									goto errhandler;
								}
							}
						}

						for(MXPSQL_MPARC_uint_repr_t i = 0; i < jsonentries; i++){
							iacrurate_snprintf_len += strlen(jsonry[i])+10;
						}

						char* str = MPARC_calloc(iacrurate_snprintf_len+1, sizeof(char));
						CHECK_LEAKS();
						if(str == NULL) {
							if(eout) *eout = MPARC_OOM;
							goto errhandler;
						}
						memset(str, '\0', iacrurate_snprintf_len+1);
						for(MXPSQL_MPARC_uint_repr_t i2 = 0; i2 < jsonentries; i2++){
							MXPSQL_MPARC_uint_repr_t strlcpy_r = 0;
							char* outstr = MPARC_calloc(iacrurate_snprintf_len+1, sizeof(char));
							CHECK_LEAKS();
							if(outstr == NULL){
								if(eout) *eout = MPARC_OOM;
								if(str) MPARC_free(str);
								goto errhandler;
							}
							strlcpy_r = strlcpy(outstr, str, iacrurate_snprintf_len);
							if(strlcpy_r > iacrurate_snprintf_len){
								if(eout) *eout = MPARC_CONSTRUCT_FAIL;
								if(str) MPARC_free(str);
								if(outstr) MPARC_free(outstr);
								goto errhandler;
							}
							int len = snprintf(outstr, iacrurate_snprintf_len, "%s%c%s", str, structure->entry_sep_or_general_marker, jsonry[i2]);
							if(len < 0 || ((MXPSQL_MPARC_uint_repr_t)len) > iacrurate_snprintf_len){
								if(eout) *eout = MPARC_CONSTRUCT_FAIL;
								if(str) MPARC_free(str);
								if(outstr) MPARC_free(outstr);
								goto errhandler;
							}
							strlcpy_r = strlcpy(str, outstr, iacrurate_snprintf_len);
							if(strlcpy_r > iacrurate_snprintf_len){
								if(eout) *eout = MPARC_CONSTRUCT_FAIL;
								if(str) MPARC_free(str);
								if(outstr) MPARC_free(outstr);
								goto errhandler;
							}
						}

						estring = str;
				}

				goto commonexit;


				errhandler:
				estring = NULL;

				goto commonexit;

				commonexit:
				{
					for(MXPSQL_MPARC_uint_repr_t i = 0; i < jsonentries; i++){
							if(jsonry[i]) MPARC_free(jsonry[i]);
					}
					if(jsonry) MPARC_free(jsonry);
				}

				return estring;
		}

		static char* MPARC_i_construct_ender(MXPSQL_MPARC_t* structure){
				static char* format = "%c%c";
				char* charachorder = NULL;
				int charachorder_len = 0;
				charachorder_len = snprintf(NULL, 0, format, structure->end_entry_marker, structure->end_file_marker);
				if(charachorder_len < 0){
						return NULL;
				}
				charachorder = MPARC_calloc(charachorder_len+1, sizeof(char));
				CHECK_LEAKS();
				if(charachorder == NULL){
						return NULL;
				}
				if(snprintf(charachorder, charachorder_len+1, format, structure->end_entry_marker, structure->end_file_marker) < 0){
						if(charachorder) MPARC_free(charachorder);
						return NULL;
				}
				return charachorder;
		}


		static MXPSQL_MPARC_err MPARC_i_verify_ender(MXPSQL_MPARC_t* structure, char* stringy, bool sensitive)  {
			char* movend = strrchr(stringy, structure->end_entry_marker); // less hacky

			if(movend == NULL){
				return MPARC_NOTARCHIVE;
			}

			{
				if(*movend != structure->end_entry_marker){
					return MPARC_NOTARCHIVE;
				}
			}

			{
				char* movfend = movend+1;
				if(*movfend != structure->end_file_marker){
					return MPARC_NOTARCHIVE;
				}

				if(sensitive){
					char* movnulend = movfend+1;
					if(*movnulend != '\0'){
						return MPARC_NOTARCHIVE;
					}
				}
			}

			return MPARC_OK;
		}

		static STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION MPARC_i_parse_version(char* str, int* success){
			STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION lversion = 0;
			/*{
				if(sscanf(str, "%"SCNuFAST64, &lversion) != 1){
					if(success != NULL) *success = 0;
					return 0;
				}
				if(success != NULL) *success = 1;
			}*/
			{
				char* endptr = NULL;
				errno = 0;
				lversion = strtoull(str, &endptr, 2);
				if(!(errno == 0 && str && (!*endptr || *endptr != 0 || *endptr != '\0'))){
					if(success != NULL) success = 0;
					return 0;
				}
				if(success != NULL) *success = 1;
			}
			return lversion;
		}

		static MXPSQL_MPARC_err MPARC_i_parse_header(MXPSQL_MPARC_t* structure, char* Stringy){
			STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION version = structure->writerVersion;

			// not bad implementation
			// this is not broken
			// but not compliant with the note I written
			/* {
				char sep[2] = {structure->begin_entry_marker, '\0'};
				char* saveptr;
				char* btok = MPARC_strtok_r(Stringy, sep, &saveptr);
				if(btok == NULL || strcmp(saveptr, "") == 0){
					return MPARC_NOTARCHIVE;
				}

				{
					char* saveptr2;
					char sep2[2] = {structure->magic_byte_sep, '\0'};
					char* tok = MPARC_strtok_r(btok, sep2, &saveptr2);
					if(tok == NULL || strcmp(STANKY_MPAR_FILE_FORMAT_MAGIC_NUMBER_25, btok) != 0 || strcmp(saveptr2, "") == 0){
						return MPARC_NOTARCHIVE;
					}
					tok = MPARC_strtok_r(NULL, sep2, &saveptr2);
					if(tok == NULL){
						return MPARC_NOTARCHIVE;
					}
					{
						char* nstok;
						char* jstok;
						{
							char* saveptr3 = NULL;
							char sep3[2] = {structure->meta_sep, '\0'};
							nstok = MPARC_strtok_r(tok, sep3, &saveptr3);
							if(nstok == NULL || strcmp(saveptr3, "") == 0){
								return MPARC_NOTARCHIVE;
							}
							jstok = saveptr3;
						}
						{
							STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION lversion = 0;
							{
								int status = 0;
								lversion = MPARC_i_parse_version(nstok, &status);
								if(status != 1){
									return MPARC_NOTARCHIVE;
								}
							}
							if(lversion > version){
								errno = ERANGE;
								return MPARC_ARCHIVETOOSHINY;
							}
							structure->loadedVersion = lversion;
						}
						{
							((void)jstok);
						}
					}
				}
			} */

			// construction note tip compliant method
			// also cleaner
			{
				char magic_sep[] = {structure->magic_byte_sep, '\0'};
				char* magic_saveptr;
				char* magic_tok = MPARC_strtok_r(Stringy, magic_sep, &magic_saveptr);
				if(magic_tok == NULL || strcmp(magic_saveptr, "") == 0){
					return MPARC_NOTARCHIVE;
				}

				{
					char pre_entry_begin_sep[2] = {structure->begin_entry_marker, '\0'};
					char* pre_entry_begin_saveptr;
					char* peb_tok = MPARC_strtok_r(magic_saveptr, pre_entry_begin_sep, &pre_entry_begin_saveptr); // peb means pre entry begin
					if(peb_tok == NULL || strcmp(peb_tok, "") == 0){
						return MPARC_NOTARCHIVE;
					}

					{
						// Metadata parsers
						MXPSQL_MPARC_err status = MPARC_OK;
						char* peb = MPARC_strdup(peb_tok);
						if(!peb) {
							status = MPARC_OOM;
							goto pebxit;
						}

						{
							char* msav = NULL;
							char meta_sep_sep[2] = {structure->meta_sep, '\0'};
							char* toks = MPARC_strtok_r(peb, meta_sep_sep, &msav);
							if(toks == NULL || strcmp(peb_tok, "") == 0 || strcmp(msav, "") == 0){
								status = MPARC_NOTARCHIVE;
								goto pebxit;
							}

							{
								char* js = msav;
								JsonNode* jsnode = json_decode(js);
								if(!js) {
									status = MPARC_NOTARCHIVE;
									goto pebxit;
								}

								{
									bool ecrypt_found = false;
									JsonNode* child_i = NULL;
									json_foreach(child_i, jsnode) {

										if(strcmp(child_i->key, "encrypt") == 0 && child_i->tag == JSON_ARRAY){ // check for encryption

											ecrypt_found = true;
											JsonNode* child_ecrypt = NULL;

											json_foreach(child_ecrypt, child_i) {

												if(child_ecrypt->tag == JSON_STRING && child_ecrypt->key == NULL){
													char* key = child_ecrypt->store.string;
													unsigned char* XORStat = NULL;
													int* ROTStat = NULL;

													MPARC_cipher(structure,
													0, NULL, 0, &XORStat, NULL,
													0, NULL, 0, &ROTStat, NULL);

													if(strcmp(key, "XOR") == 0 && XORStat == NULL){
														status = MPARC_NOCRYPT;
														goto pebxit;
													}

													if(strcmp(key, "ROT") == 0 && ROTStat == NULL){
														status = MPARC_NOCRYPT;
														goto pebxit;
													}
												}
												else{
													status = MPARC_NOTARCHIVE;
													goto pebxit;
												}

											}
										}

									}

									if(!ecrypt_found) {
										status = MPARC_NOTARCHIVE;
										goto pebxit;
									}
								}
							}
						}

						goto pebxit; // redundant IK, but the POWER OF GOTO STATEMENTS WE YOLO AND LEROY THIS ERROR HANDLING

						pebxit:
						MPARC_free(peb);
						if(status != MPARC_OK) return status;
					}

					{
						char meta_sep_sep[2] = {structure->meta_sep, '\0'};
						char* meta_sep_sep_saveptr;
						char* mss_tok = MPARC_strtok_r(peb_tok, meta_sep_sep, &meta_sep_sep_saveptr); // mss means meta sep sep
						if(mss_tok == NULL || strcmp(mss_tok, "") == 0 || strcmp(meta_sep_sep_saveptr, "") == 0){
							return MPARC_NOTARCHIVE;
						}

						{
							STANKY_MPAR_FILE_FORMAT_VERSION_REPRESENTATION lversion = 0;

							{
								int status = 0;
								lversion = MPARC_i_parse_version(mss_tok, &status);
								if(status != 1){
									return MPARC_NOTARCHIVE;
								}

								if(lversion > version){
									return MPARC_ARCHIVETOOSHINY;
								}

								structure->loadedVersion = lversion;
								if(structure->loadedVersion <= 0) { // version 0 is invalid
									return MPARC_NOTARCHIVE;
								}
							}
						}
					}
				}
			}
			return MPARC_OK;
		}

		static MXPSQL_MPARC_err MPARC_i_parse_entries(MXPSQL_MPARC_t* structure, char* Stringy, int erronduplicate, bool sensitive) {
			char** entries = NULL;
			char** json_entries = NULL;
			MXPSQL_MPARC_err err = MPARC_OK;
			MXPSQL_MPARC_uint_repr_t ecount = 0;
			{
				char* entry = NULL;
				
				char* saveptr = NULL;
				char sepsis[2] = {structure->begin_entry_marker, '\0'};
				entry = MPARC_strtok_r(Stringy, sepsis, &saveptr);
				if(entry == NULL) return MPARC_NOTARCHIVE;
				entry = MPARC_strtok_r(NULL, sepsis, &saveptr);
				if(entry == NULL) return MPARC_NOTARCHIVE;
				char* entry2 = entry;
				{ // it works!
					char* endp = strrchr(entry2, structure->end_entry_marker);

					if(endp == NULL) return MPARC_NOTARCHIVE;

					if((err = MPARC_i_verify_ender(structure, endp, sensitive)) != MPARC_OK){
						structure->my_err = err;
						return err;
					}

					*endp = '\0';	

				}
				// printf("e2.%s\n", entry2);
				{
					char* saveptr2 = NULL;
					char septic[2] = {structure->entry_sep_or_general_marker, '\0'};
					{
						char* edup = MPARC_strdup(entry2);
						if(edup == NULL){
							err = MPARC_OOM;
							goto errhandler;
						}
						char* sp3 = NULL;
						char* e = MPARC_strtok_r(edup, septic, &sp3);
						while(e != NULL) {
							ecount += 1;
							e = MPARC_strtok_r(NULL, septic, &sp3);
						}
						if(edup) MPARC_free(edup);
					}
					entries = MPARC_calloc(ecount+1, sizeof(char*));
					json_entries = MPARC_calloc(ecount+1, sizeof(char*));
					CHECK_LEAKS();
					if(json_entries == NULL){
						err = MPARC_OOM;
						goto errhandler;
					}
					if(entries == NULL){
						err = MPARC_OOM;
						goto errhandler;
					}
					json_entries[ecount] = NULL;
					char* entry64 = MPARC_strtok_r(entry2, septic, &saveptr2);
					for(MXPSQL_MPARC_uint_repr_t i = 0; entry64 != NULL; i++){
						entries[i] = MPARC_strdup(entry64);

						// printf("d%s\n", entries[i]);
						if(entries[i] == NULL){
							err = MPARC_OOM;
							goto errhandler;
						}
						entry64 = MPARC_strtok_r(NULL, septic, &saveptr2);
					}
					entries[ecount] = NULL;
				}
			}

			for(MXPSQL_MPARC_uint_repr_t i = 0; i < ecount && entries[i] != NULL; i++){
				if(!entries[i]) break;
				crc_t crc = crc_init();
				crc_t tcrc = crc_init();
				char* entry = entries[i];
				if(entry[0] == structure->comment_marker || (strlen(entry)<=0 || entry[0] == '\0' || strcmp(entry, "") == 0)) { // check for comment or empty line
					json_entries[i] = NULL;
					if(entries[i+1] == NULL) {
						break;
					}
					continue;
				}
				// printf("r%s\n", entry);
				char* sptr = NULL;
				char seps[2] = {structure->entry_elem2_sep_marker_or_magic_sep_marker, '\0'};
				char* crcstr = MPARC_strtok_r(entry, seps, &sptr);
				if(crcstr == NULL || sptr == NULL || strcmp(sptr, "") == 0){
					err = MPARC_NOTARCHIVE;
					goto errhandler;
				}
				if(sscanf(crcstr, "%"SCNuFAST32, &crc) != 1){ // failed to get checksum, we will never know the real checksum
					errno = EILSEQ;
					err = MPARC_CHKSUM;
					goto errhandler;
				}
				char* tok = sptr;
				tcrc = crc_update(tcrc, tok, strlen(tok));
				tcrc = crc_finalize(tcrc);
				if(tcrc != crc){
					#ifdef MPARC_EPARSE_DEBUG_VERBOSE
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "ravioli ala crc32\n");
					#endif
					errno = EILSEQ;
					err = MPARC_CHKSUM;
					goto errhandler;
				}
				json_entries[i] = tok;
			}

			jsmn_parser jsmn;
			jsmn_init(&jsmn);
			for(MXPSQL_MPARC_uint_repr_t i = 0; i < ecount; i++){
				char* filename = NULL;
				char* blob = NULL;
				crc_t crc3 = crc_init();
				bool filename_parsed = false;
				bool blob_parsed = false;
				bool crc3_parsed = false;

				char* jse = json_entries[i];

				if(jse == NULL) continue; // skip if NULL

				if(false){ // disable jsmn for now due to eroneous results (problem with the token ordering)
					static const MXPSQL_MPARC_uint_repr_t jtokens_count = 128; // we only need 4 but we don't expect more than 128, we put more just in case for other metadata
					jsmntok_t jtokens[jtokens_count]; // this says no to C++
					int jsmn_err = jsmn_parse(&jsmn, jse, strlen(jse), jtokens, jtokens_count);
					if(jsmn_err < 0){
						err = MPARC_NOTARCHIVE;
						goto errhandler;
					}
					for(MXPSQL_MPARC_uint_repr_t i_jse = 1; i_jse < 5; i_jse++){ // we only need 4 to scan
						jsmntok_t jtoken = jtokens[i_jse];
						char* tok1 = "";
						{
							char* start = &jse[jtoken.start];
							char* end = &jse[jtoken.end];
							char *substr = (char *)MPARC_calloc(end - start + 1, sizeof(char));
							CHECK_LEAKS();
							if(substr == NULL){
								err = MPARC_OOM;
								goto errhandler;
							}
							memcpy(substr, start, end - start);
							tok1 = substr;
						}
						if(tok1) MPARC_free(tok1);
					}
				}
				else{
					// ((void)json_check);
					((void)json_prepend_member);
					((void)json_prepend_element);
					((void)json_find_member);
					((void)json_find_element);
					((void)json_validate);
					((void)json_encode_string);

					JsonNode* nd = json_decode(jse);
					JsonNode* node = NULL;
					json_foreach(node, nd){
						if(node->tag == JSON_STRING){
							if(structure->loadedVersion >= 1) { // Version 1 stuff
								if(strcmp(node->key, "filename") == 0){
									filename = node->store.string;
									filename_parsed = true;
								}
								else if(strcmp(node->key, "blob") == 0){
									blob = node->store.string;
									blob_parsed = true;
								}
								else if(strcmp(node->key, "crcsum") == 0){
									{
										char* nvalue = node->store.string;
										if(sscanf(nvalue, "%"SCNuFAST32, &crc3) != 1){
											errno = EILSEQ;
											err = MPARC_CHKSUM;
											goto errhandler;
										}
										crc3_parsed = true;
									}
								}
							}

						}
					}
				}

				if(!filename_parsed || !blob_parsed || !crc3_parsed){
					#ifdef MPARC_EPARSE_DEBUG_VERBOSE
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "no ravioli? (filename: %d, blob: %d, crc: %d)\n", filename_parsed, blob_parsed, crc3_parsed);
					#endif
					err = MPARC_NOTARCHIVE;
					goto errhandler;
				}
				
				{
					MXPSQL_MPARC_uint_repr_t bsize = 1;
					unsigned char* pun64_blob = NULL;
					if(strlen(blob) > 1){
						pun64_blob = b64.atob((unsigned char*) blob, strlen(blob), &bsize);
					}
					else{
						pun64_blob = (unsigned char*) MPARC_strdup("");
					}

					if(pun64_blob == NULL){
						err = MPARC_OOM;
						goto errhandler;
					}

					{

						unsigned char* un64_blob = MPARC_memdup(pun64_blob, bsize);
						{
							unsigned char* XORK = NULL;
							int* ROTK = NULL;
							MXPSQL_MPARC_uint_repr_t XORL = 0;
							MXPSQL_MPARC_uint_repr_t ROTL = 0;

							MPARC_cipher(structure,
							0, NULL, 0, &XORK, &XORL,
							0, NULL, 0, &ROTK, &ROTL);

							if(ROTK){
								// VLAs (No C++ Compilation for this :/)
								int RROTK[ROTL]; // Reverse ROT Key

								{
									for(MXPSQL_MPARC_uint_repr_t i = 0; i < ROTL; i++){
										RROTK[i] = (ROTK[i] * -1); // make value negative
									}
								}

								unsigned char* tun64_blob = ROTCipher(un64_blob, bsize, RROTK, ROTL);
								if(!tun64_blob) {
									err = MPARC_OOM;
									goto errhandler;
								}

								unsigned char* oun64_blob = un64_blob;
								un64_blob = tun64_blob;
								MPARC_free(oun64_blob);
							}

							if(XORK){
								unsigned char* tun64_blob = XORCipher(un64_blob, bsize, XORK, XORL);
								if(!tun64_blob) {
									err = MPARC_OOM;
									goto errhandler;
								}

								unsigned char* oun64_blob = un64_blob;
								un64_blob = tun64_blob;
								MPARC_free(oun64_blob);
							}
						}

						{
							MPARC_blob_store store = {
								bsize,
								un64_blob,
								crc3
							};

							#ifdef MPARC_MEM_DEBUG_VERBOSE
							{
								fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Printing unb64 blob after parsing of %s with size of %"PRIuFAST64":\n", filename, store.binary_size);
								for(MXPSQL_MPARC_uint_repr_t i = 0; i < store.binary_size; i++){
									fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", store.binary_blob[i]);
								}
								printf("\n");
							}
							#endif

							crc_t crc = crc_init();
							crc = crc_update(crc, store.binary_blob, store.binary_size);
							crc = crc_finalize(crc);

							/* printf("%"PRIuFAST32" %"PRIuFAST32"\n", crc, crc3); */
							if(crc != crc3){
								// errno = EILSEQ;
								// err = MPARC_CHKSUM;
								// structure->my_err = err;
								// goto errhandler;
							}
							store.binary_crc = crc;

							// map_set(&structure->globby, filename, store);
							err = MPARC_i_push_ufilestr_advancea(structure, filename, 0, erronduplicate, store.binary_blob, store.binary_size, store.binary_crc);
							if(err != MPARC_OK){
								structure->my_err = err;
								goto errhandler;
							}
						}
					}
				}

			}

			goto errhandler; // redundant I know

			errhandler:
			structure->my_err = err;
			for(MXPSQL_MPARC_uint_repr_t i = 0; i < ecount; i++){
				if(entries[i] != NULL) MPARC_free(entries[i]);
				// if(json_entries[i] != NULL) MPARC_free(json_entries[i]);
			}

			return err;
		}

		static MXPSQL_MPARC_err MPARC_i_parse_ender(MXPSQL_MPARC_t* structure, char* Stringy, bool sensitive){
			return MPARC_i_verify_ender(structure, Stringy, sensitive);
		}
		



		MXPSQL_MPARC_err MPARC_init(MXPSQL_MPARC_t** structure){
				if(!(structure == NULL || *structure == NULL)) return MPARC_IVAL;

				void* memalloc = MPARC_calloc(1, sizeof(MXPSQL_MPARC_t));
				CHECK_LEAKS();
				if(memalloc == NULL) return MPARC_OOM;

				MXPSQL_MPARC_t* istructure = (MXPSQL_MPARC_t*) memalloc;
				if(istructure == NULL){
						return MPARC_IVAL;
				}

				map_init(&istructure->globby);

				{
					// shall not change this
					istructure->magic_byte_sep = ';'; 
					istructure->meta_sep = '$'; 
					istructure->entry_sep_or_general_marker = '\n'; 
					istructure->comment_marker = '#';
					istructure->begin_entry_marker = '>';
					istructure->entry_elem2_sep_marker_or_magic_sep_marker = MPARC_MAGIC_CHKSM_SEP;
					istructure->end_entry_marker = '@';
					istructure->end_file_marker = '~';
				}

				{
					istructure->writerVersion = STANKY_MPAR_FILE_FORMAT_VERSION_NUMBER;
					istructure->loadedVersion = STANKY_MPAR_FILE_FORMAT_VERSION_NUMBER; // default same
				}

				{
					istructure->XORKey = NULL;
					istructure->XORKeyLength = 0;

					istructure->ROTKey = NULL;
					istructure->ROTKeyLength = 0;
				}

				*structure = istructure;

				return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_copy(MXPSQL_MPARC_t** structure, MXPSQL_MPARC_t** targetdest){
			MXPSQL_MPARC_err err = MPARC_OK;
			if(structure == NULL || targetdest == NULL) return MPARC_IVAL;
			if(*targetdest != NULL){
				err = MPARC_destroy(targetdest);
				(*structure)->my_err = err;
				if(err != MPARC_OK){
					return err;
				}
			}
			MXPSQL_MPARC_t* cp_archive = NULL;
			err = MPARC_init(&cp_archive);
			(*structure)->my_err = err;
			if(err != MPARC_OK){
				return err;
			}
			// partial deep copy
			{
				char** listy_structure_out = NULL;
				MXPSQL_MPARC_uint_repr_t listy_structure_sizy_sizey_size = 0;
				err = MPARC_list_array(*structure, &listy_structure_out, &listy_structure_sizy_sizey_size);
				if(err != MPARC_OK){
					return err;
				}
				for(MXPSQL_MPARC_uint_repr_t i = 0; i < listy_structure_sizy_sizey_size; i++){
					char* filename = listy_structure_out[i];
					MPARC_blob_store e = {0};
					err = MPARC_peek_file(*structure, filename, &e.binary_blob, &e.binary_size);
					if(err != MPARC_OK) {
						goto my_err_handler;
					}
					err = MPARC_push_ufilestr(cp_archive, filename, e.binary_blob, e.binary_size);
				}

				goto my_err_handler;
				my_err_handler:
				(*structure)->my_err = err;
				if(listy_structure_out) MPARC_list_array_free(&listy_structure_out);
			}
			*targetdest = cp_archive;
			(*structure)->my_err = err;
			return err;
		}

		MXPSQL_MPARC_err MPARC_destroy(MXPSQL_MPARC_t** structure){
				if(structure == NULL || *structure == NULL) return MPARC_IVAL;

				{
					// destroy objects
					MXPSQL_MPARC_err err = MPARC_OK;

					MXPSQL_MPARC_iter_t* iterator = NULL;
					(*structure)->my_err = err;
					if((err = MPARC_list_iterator_init(structure, &iterator)) != MPARC_OK){
						return err;
					}

					{
						const char* key = NULL;
						while((err = MPARC_list_iterator_next(&iterator, &key)) == MPARC_OK){
							unsigned char* blob = NULL;
							MXPSQL_MPARC_uint_repr_t s64 = 0;
							if((err = MPARC_peek_file(*structure, key, &blob, &s64)) != MPARC_OK){
								(*structure)->my_err = err;
								break;
							}
							MPARC_free(blob);
						}

						MPARC_list_iterator_destroy(&iterator);

						if(err != MPARC_KNOEXIST){ // failure conditions
							// return err;
							(*structure)->my_err = MPARC_INTERNAL;
							return MPARC_INTERNAL; // Should never see this;If you see this, the iterator functionality is bugged
						}
					}
				}
				
				{
					// deinit encryption
					if((*structure)->XORKey) free((*structure)->XORKey); // Free XOR encryption keys
					if((*structure)->ROTKey) free((*structure)->ROTKey); // Free ROT encryption keys
				}

				{
					// destroy structure and storage
					map_deinit(&(*structure)->globby);

					if(*structure) MPARC_free(*structure);

					*structure = NULL; // invalidation for security
				}
				
				return MPARC_OK;
		}



		MXPSQL_MPARC_err MPARC_cipher(MXPSQL_MPARC_t* structure, 
    	bool SetXOR, unsigned char* XORKeyIn, MXPSQL_MPARC_uint_repr_t XORKeyLengthIn, unsigned char** XORKeyOut, MXPSQL_MPARC_uint_repr_t* XORKeyLengthOut,
    	bool SetROT, int* ROTKeyIn, MXPSQL_MPARC_uint_repr_t ROTKeyLengthIn, int** ROTKeyOut, MXPSQL_MPARC_uint_repr_t* ROTKeyLengthOut){
			if(!structure) return MPARC_NULL;
			bool fleg = false;
			{
				// DO XOR
				{
					// Set out
					if(XORKeyOut) *XORKeyOut = structure->XORKey;
					if(XORKeyLengthOut) *XORKeyLengthOut = structure->XORKeyLength;
				}

				if(SetXOR){
					// Set in

					structure->XORKey = MPARC_memdup(XORKeyIn, XORKeyLengthIn);
					structure->XORKeyLength = XORKeyLengthIn;
					fleg = true;
				}
				else{
					if(!ROTKeyOut) free(structure->XORKey);
					if(!XORKeyLengthOut) structure->XORKeyLength = 0;
				}
			}
			{
				// DO ROT
				{
					// Set Out
					if(ROTKeyOut) *ROTKeyOut = structure->ROTKey;
					if(ROTKeyLengthOut) *ROTKeyLengthOut = structure->ROTKeyLength;
				}

				if(SetROT){
					// Set in
					structure->ROTKey = MPARC_memdup(ROTKeyIn, ROTKeyLengthIn*sizeof(int));
					structure->ROTKeyLength = XORKeyLengthIn;
					fleg = true;
				}
				else{
					if(!XORKeyOut) free(structure->ROTKey);
					if(!ROTKeyLengthOut) structure->ROTKeyLength = 0;
				}
			}

			return (fleg ? MPARC_OK : MPARC_NOCRYPT);
		}



		MXPSQL_MPARC_err MPARC_list_array(MXPSQL_MPARC_t* structure, char*** listout,	MXPSQL_MPARC_uint_repr_t* length){
				if(structure == NULL) {
						return MPARC_NULL;
				}

				typedef struct anystruct {
						MXPSQL_MPARC_uint_repr_t len;
						const char* nam;
				} abufinfo;

				MXPSQL_MPARC_uint_repr_t lentracker = 0;

				char** listout_structure = NULL;

				const char *key;
				map_iter_t iter = map_iter(&structure->globby);

				while ((key = map_next(&structure->globby, &iter))) {
						// printf("L> %s\n", key);
						// printf("%s\n\n", map_get(&structure->globby, key)->binary_blob);
						lentracker++;
				}

				if(length != NULL){
						*length = lentracker;
				}

				if(listout != NULL){

						MXPSQL_MPARC_uint_repr_t index = 0;
						const char* key2;
						listout_structure = MPARC_calloc(lentracker+1, sizeof(char*));
						CHECK_LEAKS();
						if(!listout_structure || listout_structure == NULL) return MPARC_OOM;

						/* map_iter_t iter2 = map_iter(&structure->globby);

						while ((key2 = map_next(&structure->globby, &iter2))) {
								// printf("L> %s\n", key);
								// printf("%s\n\n", map_get(&structure->globby, key)->binary_blob);
								abufinfo bi = {
										strlen(key2),
										key2
								};

								listout_structure[index] = MPARC_strdup(bi.nam);

								// MPARC_free((char*)bi.nam);

								index++;
						} */

						MXPSQL_MPARC_iter_t* iterator = NULL;
						{
							MXPSQL_MPARC_err err = MPARC_list_iterator_init(&structure, &iterator);
							structure->my_err = err;
							if(err != MPARC_OK){
								if(listout_structure) MPARC_free(listout_structure);
								return err;
							}
						}
						if(iterator == NULL) {
							if(listout_structure) MPARC_free(listout_structure);
							return MPARC_IVAL;
						}
						while((MPARC_list_iterator_next(&iterator, &key2) == MPARC_OK)){
								abufinfo bi = {
										strlen(key2),
										key2
								};

								// printf("L> %s\n", key2);

								// printf("%de\n", key2 == NULL);
								// fflush(stdout);

								listout_structure[index] = MPARC_strdup(bi.nam);

								// MPARC_free((char*)bi.nam);

								index++;
						}

						listout_structure[index] = NULL;
				}

				#ifdef MPARC_QSORT 
				if (listout_structure == NULL || listout_structure[lentracker] != NULL)
				{
					return MPARC_INTERNAL; // Something bad happened (listout_structure shouldn't be NULL and listout_structure[lentracker] should be NULL)
				}
				qsort(listout_structure, lentracker, sizeof(*listout_structure), voidstrcmp);
				#endif

				if(listout != NULL){
						*listout = listout_structure;
				}
				else{
					for(MXPSQL_MPARC_uint_repr_t i = 0; i < lentracker; i++){
						if(listout_structure) {
							if(listout_structure[i]) MPARC_free(listout_structure[i]);
						}
					}

					if(listout_structure) MPARC_free(listout_structure);
				}

				structure->my_err = MPARC_OK;
				return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_list_array_free(char*** list){
			if(list == NULL || *list == NULL) return MPARC_NULL;
			char** l = *list;
			for(MXPSQL_MPARC_uint_repr_t i = 0; l[i] != NULL; i++){
				MPARC_free(l[i]);
			}
			MPARC_free(l);
			return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_list_iterator_init(MXPSQL_MPARC_t** structure, MXPSQL_MPARC_iter_t** iterator){
			if(!(structure == NULL || *structure == NULL || iterator == NULL || *iterator == NULL)) return MPARC_NULL;

			void* memalloc = MPARC_calloc(1, sizeof(MXPSQL_MPARC_iter_t));
			CHECK_LEAKS();
			if(memalloc == NULL) return MPARC_OOM;

			MXPSQL_MPARC_iter_t* iter = (MXPSQL_MPARC_iter_t*) memalloc;
			if(iter == NULL) return MPARC_IVAL;

			iter->archive = *structure;
			iter->itery = map_iter(&(*structure)->globby);

			*iterator = iter;

			(*structure)->my_err = MPARC_OK;
			return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_list_iterator_next(MXPSQL_MPARC_iter_t** iterator, const char** outnam){
			if(iterator == NULL || *iterator == NULL) return MPARC_NULL;

			MXPSQL_MPARC_iter_t* iterye = *iterator;

			const char* nkey = map_next(&iterye->archive->globby, &iterye->itery);

			// printf("eeeee\n");
			// fflush(stdout);

			if(nkey == NULL) return MPARC_KNOEXIST;

			if(outnam != NULL) *outnam = nkey;

			return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_list_iterator_destroy(MXPSQL_MPARC_iter_t** iterator){
			if(iterator == NULL || *iterator == NULL) return MPARC_NULL;
			if(*iterator) MPARC_free(*iterator);
			return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_list_foreach(MXPSQL_MPARC_t* structure, bool* cb_aborted, MXPSQL_MPARC_err (*callback)(MXPSQL_MPARC_t*, const char*, void*), void* cb_ctx){
			MXPSQL_MPARC_err err = MPARC_OK;
			bool cb_status = false;

			MXPSQL_MPARC_iter_t* iterator = NULL;

			err = MPARC_list_iterator_init(&structure, &iterator);
			if(err != MPARC_OK){
				cb_status = 0;
				goto exit_handler;
			}

			{
				MXPSQL_MPARC_err err_l = MPARC_OK;
				const char* key = NULL;
				while((err = MPARC_list_iterator_next(&iterator, &key)) == MPARC_OK){
					err_l = callback(structure, key, cb_ctx);
					if(err_l != MPARC_OK){
						cb_status = true;
						goto atexit_handler;
					}
				}

				if(err == MPARC_KNOEXIST){
					err = MPARC_OK;
				}

				if(err_l != MPARC_OK){
					err = err_l;
				}
			}

			goto atexit_handler; // redundant

			atexit_handler:
			structure->my_err = err;
			MPARC_list_iterator_destroy(&iterator);

			exit_handler:
			if(cb_aborted) *cb_aborted = cb_status;
			return err;
		}

		MXPSQL_MPARC_err MPARC_exists(MXPSQL_MPARC_t* structure, const char* filename){
				if(structure == NULL || filename == NULL) return MPARC_NULL;
				switch(1){
					case 1:
					{
						if((map_get(&structure->globby, filename)) == NULL) {
							structure->my_err = MPARC_KNOEXIST;
							return MPARC_KNOEXIST;
						}
						break;
					}

					case 3:
					{
						map_iter_t iter = map_iter(&structure->globby);
						const char* nkey = NULL;
						while((nkey = map_next(&structure->globby, &iter))){
							if(strcmp(nkey, filename) == 0){
								structure->my_err = MPARC_OK;
								return MPARC_OK;
							}
						}
						structure->my_err = MPARC_KNOEXIST;
						return MPARC_KNOEXIST;
					}
				}
				
				structure->my_err = MPARC_KNOEXIST;
				return MPARC_OK;
		}



		MXPSQL_MPARC_err MPARC_push_ufilestr_advance(MXPSQL_MPARC_t* structure, const char* filename, bool stripdir, bool overwrite, unsigned char* ustringc, MXPSQL_MPARC_uint_repr_t sizy){
			crc_t crc3 = crc_init();
			crc3 = crc_update(crc3, ustringc, sizy);
			crc3 = crc_finalize(crc3);
			// printf("%s> %"PRIuFAST32"\n", filename, crc3);

			#ifdef MPARC_MEM_DEBUG_VERBOSE
			fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Viewing bytes of %s after being pushed on the archive with size of %"PRIuFAST64":\n", filename, sizy);
			for(MXPSQL_MPARC_uint_repr_t i = 0; i < sizy; i++){
				fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", ustringc[i]);
			}
			fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
			#endif

			MPARC_i_push_ufilestr_advancea(structure, filename, stripdir, overwrite, ustringc, sizy, crc3);

			structure->my_err = MPARC_OK;
			return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_push_ufilestr(MXPSQL_MPARC_t* structure, const char* filename, unsigned char* ustringc, MXPSQL_MPARC_uint_repr_t sizy){
			return MPARC_push_ufilestr_advance(structure, filename, 0, 1, ustringc, sizy);
		}

		MXPSQL_MPARC_err MPARC_push_voidfile(MXPSQL_MPARC_t* structure, const char* filename, void* buffer_guffer, MXPSQL_MPARC_uint_repr_t sizey){
			return MPARC_push_ufilestr(structure, filename, (unsigned char*)buffer_guffer, sizey);
		}

		MXPSQL_MPARC_err MPARC_push_filestr(MXPSQL_MPARC_t* structure, const char* filename, char* stringc, MXPSQL_MPARC_uint_repr_t sizey){
				return MPARC_push_ufilestr(structure, filename, (unsigned char*) stringc, sizey);
		}

		MXPSQL_MPARC_err MPARC_push_filename(MXPSQL_MPARC_t* structure, const char* filename){
				if(structure == NULL) {
					return MPARC_NULL;
				}
				if(filename == NULL) {
					structure->my_err = MPARC_FERROR;
					return structure->my_err;
				}

				FILE* fpstream = fopen(filename, "rb");
				if(fpstream == NULL){
					structure->my_err = MPARC_FERROR;
					return structure->my_err;
				}

				MXPSQL_MPARC_err err = MPARC_push_filestream(structure, fpstream, filename);    

				fclose(fpstream);

				return err;
		}

		MXPSQL_MPARC_err MPARC_push_filestream(MXPSQL_MPARC_t* structure, FILE* filestream, const char* filename){
				if(filestream == NULL){
					structure->my_err = MPARC_FERROR;
					return structure->my_err;
				}

				if(filename == NULL){
					structure->my_err = MPARC_IVAL;
					return structure->my_err;
				}
				

				unsigned char* binary = NULL;

				MXPSQL_MPARC_uint_repr_t filesize = 0; // byte count
				if(fseek(filestream, 0, SEEK_SET) != 0){
						structure->my_err = MPARC_FERROR;
					return MPARC_FERROR;
				}

				while(fgetc(filestream) != EOF && !ferror(filestream)){
						filesize += 1;
				}
				if(ferror(filestream)){
						structure->my_err = MPARC_FERROR;
						return MPARC_FERROR;
				}

				clearerr(filestream);

				if(fseek(filestream, 0, SEEK_SET) != 0){
						structure->my_err = MPARC_FERROR;
						return MPARC_FERROR;
				}

				binary = MPARC_calloc(filesize+1, sizeof(unsigned char));
				CHECK_LEAKS();
				if(!binary){
					structure->my_err = MPARC_OOM;
					return MPARC_OOM;
				}

				if(filesize >= MPARC_DIRECTF_MINIMUM){
					if(fread(binary, sizeof(unsigned char), filesize, filestream) < filesize && ferror(filestream)){
						if(binary) MPARC_free(binary);
							structure->my_err = MPARC_FERROR;
						return MPARC_FERROR;
					}
				}
				else{
					int c = 0;
					MXPSQL_MPARC_uint_repr_t i = 0;
					while((c = fgetc(filestream)) != EOF){
						binary[i++] = c;
					}

					if(ferror(filestream)){
						if(binary) MPARC_free(binary);
						structure->my_err = MPARC_FERROR;
						return MPARC_FERROR;
					}
				}

				#ifdef MPARC_MEM_DEBUG_VERBOSE
				{
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "viewing bytes read from %s with size of %"PRIuFAST64":\n", filename, filesize);
					for(MXPSQL_MPARC_uint_repr_t i = 0; i < filesize; i++){
						fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", binary[i]);
					}
					fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
				}
				#endif

				if(!binary) {
					structure->my_err = MPARC_OOM;
					return MPARC_OOM;
				}
				unsigned char* strd = (unsigned char*) MPARC_memdup(binary, filesize*sizeof(unsigned char));
				if(!strd) {
					structure->my_err = MPARC_OOM;
					return MPARC_OOM;
				}
				MXPSQL_MPARC_err err = MPARC_push_ufilestr(structure, filename, strd, filesize);
				
				if(binary) MPARC_free(binary);
				return err;
		}


		MXPSQL_MPARC_err MPARC_rename_file(MXPSQL_MPARC_t* structure, bool overwrite, const char* oldname, const char* newname){
			MXPSQL_MPARC_err err = MPARC_OK;
			if((err = MPARC_exists(structure, oldname)) == MPARC_KNOEXIST){
				structure->my_err = err;
				return err;
			}

			{
				unsigned char* binary = NULL;
				MXPSQL_MPARC_uint_repr_t bsize = 0;

				err = MPARC_peek_file(structure, oldname, &binary, &bsize);
				structure->my_err = err;
				if(err != MPARC_OK){
					return err;
				}

				{
					if(!overwrite && (err = MPARC_exists(structure, newname)) == MPARC_OK){
						structure->my_err = MPARC_KEXISTS;
						return MPARC_KEXISTS;
					}

					err = MPARC_push_ufilestr(structure, newname, binary, bsize);
					if(err != MPARC_OK){
						structure->my_err = err;
						return err;
					}

					{
						err = MPARC_pop_file(structure, oldname);
						if(err != MPARC_OK){
							structure->my_err = err;
							return err;
						}
					}
				}
			}
			return err;
		}

		MXPSQL_MPARC_err MPARC_duplicate_file(MXPSQL_MPARC_t* structure, bool overwrite, const char* srcfile, const char* destfile){
			MXPSQL_MPARC_err err = MPARC_OK;
			if((err = MPARC_exists(structure, srcfile)) == MPARC_KNOEXIST) {
				structure->my_err = err;
				return err;
			}
			{
				unsigned char* binary = NULL;
				MXPSQL_MPARC_uint_repr_t bsize = 0;

				err = MPARC_peek_file(structure, srcfile, &binary, &bsize);
				if(err != MPARC_OK){
					structure->my_err = err;
					return err;
				}

				{
					if(!overwrite && (err = MPARC_exists(structure, destfile)) == MPARC_OK){
						structure->my_err = err;
						return MPARC_KEXISTS;
					}

					err = MPARC_push_ufilestr(structure, destfile, binary, bsize);
					if(err != MPARC_OK){
						structure->my_err = err;
						return err;
					}
				}
			}
			return err;
		}

		MXPSQL_MPARC_err MPARC_swap_file(MXPSQL_MPARC_t* structure, const char* file1, const char* file2){
			MXPSQL_MPARC_err err = MPARC_OK;

			if((err = MPARC_exists(structure, file1)) == MPARC_KNOEXIST) {
				structure->my_err = MPARC_KNOEXIST;
				return err;
			}
			if((err = MPARC_exists(structure, file2)) == MPARC_KNOEXIST) {
				structure->my_err = MPARC_KNOEXIST;
				return err;
			}
			
			{
				unsigned char* binary1 = NULL;
				MXPSQL_MPARC_uint_repr_t bsize1 = 0;

				unsigned char* binary2 = NULL;
				MXPSQL_MPARC_uint_repr_t bsize2 = 0;

				if((err = MPARC_peek_file(structure, file1, &binary1, &bsize1)) != MPARC_OK) {
					structure->my_err = err;
					return err;
				}
				if((err = MPARC_peek_file(structure, file2, &binary2, &bsize2)) != MPARC_OK) {
					structure->my_err = err;
					return err;
				}
				if((err = MPARC_push_ufilestr(structure, file1, binary2, bsize2)) != MPARC_OK) {
					structure->my_err = err;
					return err;
				}
				if((err = MPARC_push_ufilestr(structure, file2, binary1, bsize1)) != MPARC_OK) {
					structure->my_err = err;
					return err;
				}
			}

			return err;
		}


		MXPSQL_MPARC_err MPARC_pop_file(MXPSQL_MPARC_t* structure, const char* filename){
				if(MPARC_exists(structure, filename) == MPARC_KNOEXIST) return MPARC_KNOEXIST;
				map_remove(&structure->globby, filename);
				return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_clear(MXPSQL_MPARC_t* structure){
			char** entryos = NULL;
			MXPSQL_MPARC_uint_repr_t eos_s = 0;
			MXPSQL_MPARC_err err = MPARC_list_array(structure, &entryos, &eos_s);
			structure->my_err = err;
			if(err != MPARC_OK) return err;
			for(MXPSQL_MPARC_uint_repr_t i = 0; i < eos_s; i++){
				MPARC_pop_file(structure, entryos[i]);
			}
			if(entryos) {
				MPARC_list_array_free(&entryos);
			}
			return err;
		}


		static MXPSQL_MPARC_err MPARC_peek_file_advance(MXPSQL_MPARC_t* structure, const char* filename, unsigned char** bout, MXPSQL_MPARC_uint_repr_t* sout, crc_t* crout){ // users don't need to know the crc
				if(MPARC_exists(structure, filename) == MPARC_KNOEXIST) return MPARC_KNOEXIST;
				MPARC_blob_store* storeptr = map_get(&structure->globby, filename);
				if(bout != NULL) {
					*bout = storeptr->binary_blob;
					#ifdef MPARC_MEM_DEBUG_VERBOSE
					{
						fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "Byte view of peeked data with size of %"PRIuFAST64".\n", storeptr->binary_size);
						for(MXPSQL_MPARC_uint_repr_t i = 0; i < storeptr->binary_size; i++){
							fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "%c", storeptr->binary_blob[i]);
						}
						fprintf(MPARC_DEBUG_CONF_PRINTF_FILE, "\n");
					}
					#endif
				}
				if(sout != NULL) *sout = storeptr->binary_size;
				if(crout != NULL) *crout = storeptr->binary_crc;
				return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_peek_file(MXPSQL_MPARC_t* structure, const char* filename, unsigned char** bout, MXPSQL_MPARC_uint_repr_t* sout){
			return MPARC_peek_file_advance(structure, filename, bout, sout, NULL);
		}

		/**
		 * @brief How to construct and parse MPAR archives. No EBNF provided cause I don't know how to represent it.
		 * 
		 * @details
		 * 
		 * SEE THIS TO SEE THE FILE FORMAT OF THE ARCHIVE
		 * THIS PART IS IMPORTANT TO SEE HOW IT IS IMPLEMENTED AND THE FORMAT
		 * 
		 * How is the file constructed (along with little parsing information to make your own parser):
		 * 
		 * 1. Build the header:
		 * 		
		 * 		Format: 
		 * 			MXPSQL's Portable Archive;[VERSION]${JSON_WHATEV_METADATA}>[NEWLINE]
		 * 
		 * 		The ';' character separates the Magic numbers (very long with 25 character I think) from the version number and json metadata
		 * 		
		 * 		The '$' character separates the version and magic numbers from the metadata
		 * 		
		 * 		The '>' character works to indicate the start of entries
		 * 		The newline is an anomaly though, but just put it in there
		 * 		
		 * 		JSON_WHATEV_METADATA have certain mandaroty parameters.  
		 * 		There must be the "encrypt" field to indicate encryption provided.  
		 * 		Standard encryption algorithm is XOR and ROT (ROT13 or anything else like ROT100 or whatever).  
		 * 		Other field specific to each implementation are ignored.
		 * 		
		 * 		Construction note:
		 * 		Make sure to base64 your metadata entries to prevent issues with parsing.
		 * 		
		 * 		Parsing tips:
		 * 			Split ';' from the whole archive to get the magic number first  
		 * 			Then split '>' from to get the special info header  
		 * 			The split '$' from the special info header to get the version and extra metadata.
		 * 
		 * 
		 * 2. Build the entries
		 * 		
		 * 		Format: 
		 * 			[CRC32_OF_JSON]%{"filename":[FILENAME],"blob":[BASE64_BINARY], "crcsum":[CRC32_OF_blob]}[NEWLINE]
		 * 
		 * 		The '%' character is to separate the checksum of the JSON from the JSON itself
		 * 		
		 * 		You can add other metadata like date of creation, but there must be the entries "filename", "blob" and "crcsum" in the JSON
		 * 		This C implementation will ignore any extra metadata.
		 * 		
		 * 		"filename" should contain the filename. It should be a simple null terminated string (don't do any effects and magic on this field called "filename"). It should conform to the environment's valid filename characters (no backslashes or filename called "CON" and "AUX").
		 * 		"blob" should contain the base64 of the binary or text file. (base64 to make it a text file and not binary)
		 * 		"crcsum" should contain the CRC32 checksum of the content of "blob" after converting it back to it's original form. ("blob" but wihtout base64). If encryption is set, the "crcsum" should be the non base64 encoded and unencrypted "blob" ("blob" with no base64 encoding and encruption)
		 * 
		 * 		[NEWLINE] should be EXACTLY '\n' (LF), not '\r\n' (CRLF) or other newlines.
		 * 		
		 * 		Repeat this as required (how many entries are there you repeat)
		 * 		
		 * 		Construction note:
		 * 		The anomaly mention aboved is because the newline is added before the main content.  
		 * 		XOR cipher is applied before ROT cipher, this sequencing is Mandatory for all implementations.
		 * 		
		 * 		Parsing note:
		 * 			When parsing the entries, split from the begin '>' marker first, and then the end '@' marker.
		 * 			Then split each by newlines.
		 * 			Ignore if a line start with '#' (EXACTLY WITH THAT CHARACTER, there must not even be any whitespace before it; You shall not strip it), a comment marker.  
		 * 			Also ignore if a line is empty.
		 * 			Then, foreach split '%' to get the crc and json.
		 * 			Then compare the JSON to the crc.
		 * 			Then parse the JSON as usual.
		 * 
		 * 			Each implementation shall decrypt with ROT before XOR because XOR is applied before ROT during construction, this sequencing is Mandatory. Be prepared to wrap your head with this.
		 * 
		 * 		You could parse extra info in your implementation, but this C Based implementation will ignore extra ones. I repeat this line again.
		 * 
		 * 
		 * 3. Build the footer
		 * 		
		 * 		Format: 
		 * 			@~
		 * 
		 * 		the '@' character is to signify end of entry
		 * 		the '~' character is to signify end of file
		 * 		
		 * 		Parsing note:
		 * 		Make sure to not have anything beyond the footer, not even a newline.
		 * 
		 * 
		 * Final note:
		 * 		This file should not have binary characters.
		 * 		Parsing should fail if BOM or non ASCII character is found. This means MPARC archives should be valid ASCII files, not valid UTF files (No UTF8 and UTF16).
		 * 		Encryption is optional;XOR and ROT (the standard encryption) can be disabled (by default it is disabled).
		 * 
		 * 		 
		 * Follow this (with placeholder) and you get this:
		 * 		
		 * 	MXPSQL's Portable Archive;[VERSION]${JSON_WHATEV_METADATA}>[NEWLINE][CRC32_OF_JSON]%{"filename":[FILENAME],"blob":[BASE64_BINARY], "crcsum":[CRC32_OF_blob]}[NEWLINE]@~
		 * 
		 * 
		 * A real single entried one:
		 * 		
		 * 	MXPSQL's Portable Archive;1${"WhatsThis": "MPARC Logo lmao-Hahahaha", "encrypt": []}>  
		 * 	134131812%{"filename":"./LICENSE.MIT","blob":"TUlUIExpY2Vuc2UKCkNvcHlyaWdodCAoYykgMjAyMiBNWFBTUUwKClBlcm1pc3Npb24gaXMgaGVyZWJ5IGdyYW50ZWQsIGZyZWUgb2YgY2hhcmdlLCB0byBhbnkgcGVyc29uIG9idGFpbmluZyBhIGNvcHkKb2YgdGhpcyBzb2Z0d2FyZSBhbmQgYXNzb2NpYXRlZCBkb2N1bWVudGF0aW9uIGZpbGVzICh0aGUgIlNvZnR3YXJlIiksIHRvIGRlYWwKaW4gdGhlIFNvZnR3YXJlIHdpdGhvdXQgcmVzdHJpY3Rpb24sIGluY2x1ZGluZyB3aXRob3V0IGxpbWl0YXRpb24gdGhlIHJpZ2h0cwp0byB1c2UsIGNvcHksIG1vZGlmeSwgbWVyZ2UsIHB1Ymxpc2gsIGRpc3RyaWJ1dGUsIHN1YmxpY2Vuc2UsIGFuZC9vciBzZWxsCmNvcGllcyBvZiB0aGUgU29mdHdhcmUsIGFuZCB0byBwZXJtaXQgcGVyc29ucyB0byB3aG9tIHRoZSBTb2Z0d2FyZSBpcwpmdXJuaXNoZWQgdG8gZG8gc28sIHN1YmplY3QgdG8gdGhlIGZvbGxvd2luZyBjb25kaXRpb25zOgoKVGhlIGFib3ZlIGNvcHlyaWdodCBub3RpY2UgYW5kIHRoaXMgcGVybWlzc2lvbiBub3RpY2Ugc2hhbGwgYmUgaW5jbHVkZWQgaW4gYWxsCmNvcGllcyBvciBzdWJzdGFudGlhbCBwb3J0aW9ucyBvZiB0aGUgU29mdHdhcmUuCgpUSEUgU09GVFdBUkUgSVMgUFJPVklERUQgIkFTIElTIiwgV0lUSE9VVCBXQVJSQU5UWSBPRiBBTlkgS0lORCwgRVhQUkVTUyBPUgpJTVBMSUVELCBJTkNMVURJTkcgQlVUIE5PVCBMSU1JVEVEIFRPIFRIRSBXQVJSQU5USUVTIE9GIE1FUkNIQU5UQUJJTElUWSwKRklUTkVTUyBGT1IgQSBQQVJUSUNVTEFSIFBVUlBPU0UgQU5EIE5PTklORlJJTkdFTUVOVC4gSU4gTk8gRVZFTlQgU0hBTEwgVEhFCkFVVEhPUlMgT1IgQ09QWVJJR0hUIEhPTERFUlMgQkUgTElBQkxFIEZPUiBBTlkgQ0xBSU0sIERBTUFHRVMgT1IgT1RIRVIKTElBQklMSVRZLCBXSEVUSEVSIElOIEFOIEFDVElPTiBPRiBDT05UUkFDVCwgVE9SVCBPUiBPVEhFUldJU0UsIEFSSVNJTkcgRlJPTSwKT1VUIE9GIE9SIElOIENPTk5FQ1RJT04gV0lUSCBUSEUgU09GVFdBUkUgT1IgVEhFIFVTRSBPUiBPVEhFUiBERUFMSU5HUyBJTiBUSEUKU09GVFdBUkUu","crcsum":"15584406"}@~
		 *
		 * 
		 * A real (much more real) multi entried one:
		 * 		
		 * 	MXPSQL's Portable Archive;1${"encrypt": []}>  
		 * 	3601911152%{"filename":"LICENSE","blob":"U2VlIExJQ0VOU0UuTEdQTCBhbmQgTElDRU5TRS5NSVQgYW5kIGNob29zZSBvbmUgb2YgdGhlbS4KCkxJQ0VOU0UuTEdQTCBjb250YWlucyBMR1BMLTIuMS1vci1sYXRlciBsaWNlbnNlLgpMSUNFTlNFLk1JVCBjb250YWlucyBNSVQgbGljZW5zZS4KCkxJQ0VOU0UuTEdQTCBhbmQgTElDRU5TRS5NSVQgc2hvdWxkIGJlIGRpc3RyaWJ1dGVkIHRvZ2V0aGVyIHdpdGggeW91ciBjb3B5LCBpZiBub3QsIHNvbWV0aGluZyBpcyB3cm9uZy4=","crcsum":"404921597"}  
		 * 	59879441%{"filename":"LICENSE.MIT","blob":"TUlUIExpY2Vuc2UKCkNvcHlyaWdodCAoYykgMjAyMiBNWFBTUUwKClBlcm1pc3Npb24gaXMgaGVyZWJ5IGdyYW50ZWQsIGZyZWUgb2YgY2hhcmdlLCB0byBhbnkgcGVyc29uIG9idGFpbmluZyBhIGNvcHkKb2YgdGhpcyBzb2Z0d2FyZSBhbmQgYXNzb2NpYXRlZCBkb2N1bWVudGF0aW9uIGZpbGVzICh0aGUgIlNvZnR3YXJlIiksIHRvIGRlYWwKaW4gdGhlIFNvZnR3YXJlIHdpdGhvdXQgcmVzdHJpY3Rpb24sIGluY2x1ZGluZyB3aXRob3V0IGxpbWl0YXRpb24gdGhlIHJpZ2h0cwp0byB1c2UsIGNvcHksIG1vZGlmeSwgbWVyZ2UsIHB1Ymxpc2gsIGRpc3RyaWJ1dGUsIHN1YmxpY2Vuc2UsIGFuZC9vciBzZWxsCmNvcGllcyBvZiB0aGUgU29mdHdhcmUsIGFuZCB0byBwZXJtaXQgcGVyc29ucyB0byB3aG9tIHRoZSBTb2Z0d2FyZSBpcwpmdXJuaXNoZWQgdG8gZG8gc28sIHN1YmplY3QgdG8gdGhlIGZvbGxvd2luZyBjb25kaXRpb25zOgoKVGhlIGFib3ZlIGNvcHlyaWdodCBub3RpY2UgYW5kIHRoaXMgcGVybWlzc2lvbiBub3RpY2Ugc2hhbGwgYmUgaW5jbHVkZWQgaW4gYWxsCmNvcGllcyBvciBzdWJzdGFudGlhbCBwb3J0aW9ucyBvZiB0aGUgU29mdHdhcmUuCgpUSEUgU09GVFdBUkUgSVMgUFJPVklERUQgIkFTIElTIiwgV0lUSE9VVCBXQVJSQU5UWSBPRiBBTlkgS0lORCwgRVhQUkVTUyBPUgpJTVBMSUVELCBJTkNMVURJTkcgQlVUIE5PVCBMSU1JVEVEIFRPIFRIRSBXQVJSQU5USUVTIE9GIE1FUkNIQU5UQUJJTElUWSwKRklUTkVTUyBGT1IgQSBQQVJUSUNVTEFSIFBVUlBPU0UgQU5EIE5PTklORlJJTkdFTUVOVC4gSU4gTk8gRVZFTlQgU0hBTEwgVEhFCkFVVEhPUlMgT1IgQ09QWVJJR0hUIEhPTERFUlMgQkUgTElBQkxFIEZPUiBBTlkgQ0xBSU0sIERBTUFHRVMgT1IgT1RIRVIKTElBQklMSVRZLCBXSEVUSEVSIElOIEFOIEFDVElPTiBPRiBDT05UUkFDVCwgVE9SVCBPUiBPVEhFUldJU0UsIEFSSVNJTkcgRlJPTSwKT1VUIE9GIE9SIElOIENPTk5FQ1RJT04gV0lUSCBUSEUgU09GVFdBUkUgT1IgVEhFIFVTRSBPUiBPVEhFUiBERUFMSU5HUyBJTiBUSEUKU09GVFdBUkUu","crcsum":"15584406"}@~
		*/
		MXPSQL_MPARC_err MPARC_construct_str(MXPSQL_MPARC_t* structure, char** output){
				static char* fmt = "%s%s%s";
				MXPSQL_MPARC_err err = MPARC_OK;

				char* top = MPARC_i_construct_header(structure);
				if(top == NULL) {
					structure->my_err = MPARC_CONSTRUCT_FAIL;
					return MPARC_CONSTRUCT_FAIL;
				}
				char* mid = MPARC_i_construct_entries(structure, &err);
				if(mid == NULL) {
					if(top) MPARC_free(top);
					top = NULL;
					structure->my_err = err;
					return err;
				}
				char* bottom = MPARC_i_construct_ender(structure);
				if(bottom == NULL) {
					if(top) MPARC_free(top);
					if(mid) MPARC_free(mid);
					top = NULL;
					mid = NULL;
					structure->my_err = MPARC_CONSTRUCT_FAIL;
					return MPARC_CONSTRUCT_FAIL;
				}

				{
					if(strlen(top) < 1 || strlen(bottom) < 1){ // empty, the unparsable one
						if (top)
							MPARC_free(top);
						if (mid)
							MPARC_free(mid);
						if (bottom)
							MPARC_free(bottom);
						top = NULL;
						mid = NULL;
						bottom = NULL;
						structure->my_err = MPARC_CONSTRUCT_FAIL;
						return MPARC_CONSTRUCT_FAIL;
					}

					// fprintf(stdout, fmt, top, mid, bottom);
					int sizy = snprintf(NULL, 0, fmt, top, mid, bottom);
					if(sizy < 0){
						if(top) MPARC_free(top);
						if(mid) MPARC_free(mid);
						if(bottom) MPARC_free(bottom);
						top = NULL;
						mid = NULL;
						bottom = NULL;
						structure->my_err = MPARC_CONSTRUCT_FAIL;
						return MPARC_CONSTRUCT_FAIL;
					}
					char* alloca_out = MPARC_calloc(sizy+1, sizeof(char));
					CHECK_LEAKS();
					if(alloca_out == NULL){
						if(top) MPARC_free(top);
						if(mid) MPARC_free(mid);
						if(bottom) MPARC_free(bottom);
						top = NULL;
						mid = NULL;
						bottom = NULL;
						structure->my_err = MPARC_OOM;
						return MPARC_OOM;
					}
					if(snprintf(alloca_out, sizy+1, fmt, top, mid, bottom) < 0){
						if(top) MPARC_free(top);
						if(mid) MPARC_free(mid);
						if(bottom) MPARC_free(bottom);
						if(alloca_out) MPARC_free(alloca_out);
						top = NULL;
						mid = NULL;
						bottom = NULL;
						alloca_out = NULL;
						structure->my_err = MPARC_CONSTRUCT_FAIL;
						return structure->my_err;
					}

					if(strlen(alloca_out) < 1) {
						if (top)
							MPARC_free(top);
						if (mid)
							MPARC_free(mid);
						if (bottom)
							MPARC_free(bottom);
						if (alloca_out)
							MPARC_free(alloca_out);
						top = NULL;
						mid = NULL;
						bottom = NULL;
						alloca_out = NULL;
						structure->my_err = MPARC_CONSTRUCT_FAIL;
						return structure->my_err;
					}
					*output = alloca_out;
				}

				if(top) MPARC_free(top);
				if(mid) MPARC_free(mid);
				if(bottom) MPARC_free(bottom);
				top = NULL;
				mid = NULL;
				bottom = NULL;
				structure->my_err = MPARC_OK;
				return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_construct_filename(MXPSQL_MPARC_t* structure, const char* filename){
			FILE* fpstream = fopen(filename, "wb+");
			if(fpstream == NULL){
				structure->my_err = MPARC_FERROR;
				return MPARC_FERROR;
			}
			MXPSQL_MPARC_err err = MPARC_construct_filestream(structure, fpstream);
			fclose(fpstream);
			return err;
		}

		MXPSQL_MPARC_err MPARC_construct_filestream(MXPSQL_MPARC_t* structure, FILE* fpstream){
			if(fpstream == NULL){
				return MPARC_NULL;
			}

			char* archive = NULL;
			MXPSQL_MPARC_err err = MPARC_construct_str(structure, &archive);
			if(err != MPARC_OK){
				if(archive) MPARC_free(archive);
				return err;
			}
			MXPSQL_MPARC_uint_repr_t count = strlen(archive);
			if(count >= MPARC_DIRECTF_MINIMUM){
				if(fwrite(archive, sizeof(char), count, fpstream) < count && ferror(fpstream)){
					if(archive) MPARC_free(archive);
					structure->my_err = MPARC_FERROR;
					return MPARC_FERROR;
				}
			}
			else{
				for(MXPSQL_MPARC_uint_repr_t i = 0; i < count; i++){
					if(fputc(archive[i], fpstream) == EOF){
						if(archive) MPARC_free(archive);
						err = MPARC_FERROR;
						structure->my_err = err;
						return err;
					}
				}
			}
			fflush(fpstream);
			if(archive) MPARC_free(archive);
			return err;
		}

		
		MXPSQL_MPARC_err MPARC_extract_advance(MXPSQL_MPARC_t* structure, const char* destdir, char** dir2make, void (*on_item)(const char*, void*), int (*mk_dir)(char*, void*), void* on_item_ctx, void* mk_dir_ctx){
			{
				char** listy = NULL;
				MXPSQL_MPARC_uint_repr_t listys = 0;
				if(MPARC_list_array(structure, &listy, &listys) != MPARC_OK){
					structure->my_err = MPARC_IVAL;
					return structure->my_err;
				}

				for(MXPSQL_MPARC_uint_repr_t i = 0; i < listys; i++){
					if(dir2make != NULL) *dir2make = NULL;
					char* fname = NULL;
					const char* nkey = listy[i];
					FILE* fps = NULL;
					if(on_item) (*on_item)(nkey, on_item_ctx);
					rmkdir_goto_label_spot:
					{
						{
							fname = MPARC_strdup(nkey);
							if(!fname){
								if(listy) MPARC_free(listy);
								structure->my_err = MPARC_OOM;
								return MPARC_OOM;
							}
							MXPSQL_MPARC_uint_repr_t pathl = strlen(fname)+strlen(nkey)+1;
							void* nfname = MPARC_realloc(fname, pathl+1);
							CHECK_LEAKS();
							if(nfname == NULL){
								if(fname) MPARC_free(fname);
								if(listy) MPARC_free(listy);
								structure->my_err = MPARC_OOM;
								return MPARC_OOM;
							}
							fname = (char*) nfname;
							int splen = snprintf(fname, pathl, "%s/%s", destdir, nkey);
							if(splen < 0 || ((MXPSQL_MPARC_uint_repr_t)splen) > pathl){
								if(fname) MPARC_free(fname);
								if(listy) MPARC_free(listy);
								structure->my_err = MPARC_IVAL;
								return structure->my_err;
							}
						}
						fps = fopen(fname, "wb+");
						if(fps == NULL){
							char* dname = MPARC_dirname((char*)fname);
							#if defined(ENOENT)
							if(errno == ENOENT){
								// this means "I request you to make me a directory and then call me when you are done so I can continue to do my own agenda which is to help you, basically I need your help for me to help you"
								if(mk_dir){
									if((*mk_dir)(dname, mk_dir_ctx) != 0){
										if(fname) MPARC_free(fname);
										if(listy) MPARC_free(listy);
										structure->my_err = MPARC_FERROR;
										return MPARC_FERROR;
									}
									if(fname) MPARC_free(fname);
									if(listy) MPARC_free(listy);
									// i--; // hacky
									// continue;
									goto rmkdir_goto_label_spot; // much better (don't object to this method of using goto and labels, the old one involes decrmenting the index variable and that is a hacky solution)
								}
								else{
									if(dir2make != NULL) *dir2make = dname;
								}
								if(fname) MPARC_free(fname);
								if(listy) MPARC_free(listy);
								structure->my_err = MPARC_OPPART;
								return MPARC_OPPART;
							}
							#else
							((void)mk_dir);
							if(dir2make != NULL) *dir2make = dname;
							#endif
							if(fname) MPARC_free(fname);
							if(listy) MPARC_free(listy);
							structure->my_err = MPARC_IVAL;
							return structure->my_err;
						}
						{
							unsigned char* bout = NULL;
							MXPSQL_MPARC_uint_repr_t sout = 0;
							crc_t crc3 = 0;
							MXPSQL_MPARC_err err = MPARC_peek_file_advance(structure, (char*) nkey, &bout, &sout, &crc3);
							if(err != MPARC_OK){
								if(fname) MPARC_free(fname);
								if(listy) MPARC_free(listy);
								fclose(fps);
								return err;
							}
							if(sout >= MPARC_DIRECTF_MINIMUM){
								if(fwrite(bout, sizeof(unsigned char), sout, fps) < sout){
									if(ferror(fps)){
										if(fname) MPARC_free(fname);
										if(listy) MPARC_free(listy);
										fclose(fps);
										structure->my_err = MPARC_FERROR;
										return MPARC_FERROR;
									}
								}
							}
							else{
								// char abc = {'a', 'b', 'c', '\0'};
								for(MXPSQL_MPARC_uint_repr_t i = 0; i < sout; i++){
									if(fputc(bout[i], fps) == EOF){
										if(fname) MPARC_free(fname);
										if(listy) MPARC_free(listy);
										fclose(fps);
										structure->my_err = MPARC_FERROR;
										return MPARC_FERROR;
									}
								}
							}
							fflush(fps);
							if(fseek(fps, 0, SEEK_SET) != 0){
								if(fname) MPARC_free(fname);
								if(listy) MPARC_free(listy);
								fclose(fps);
								structure->my_err = MPARC_FERROR;
								return MPARC_FERROR;
							}
							unsigned char* binary = MPARC_calloc(sout+1, sizeof(unsigned char));
							CHECK_LEAKS();
							if(binary == NULL){
								if(fname) MPARC_free(fname);
								if(listy) MPARC_free(listy);
								fclose(fps);
								structure->my_err = MPARC_OOM;
								return MPARC_OOM;
							}
							if(sout >= MPARC_DIRECTF_MINIMUM){
								if(fread(binary, sizeof(unsigned char), sout, fps) < sout){
									if(ferror(fps)){
										if(fname) MPARC_free(fname);
										if(binary) MPARC_free(binary);
										if(listy) MPARC_free(listy);
										fclose(fps);
										structure->my_err = MPARC_FERROR;
										return MPARC_FERROR;
									}
								}
							}
							else{
								int c = 0;
								MXPSQL_MPARC_uint_repr_t i = 0;
								while((c = fgetc(fps)) != EOF){
									binary[i++] = c;
								}

								if(ferror(fps)){
									if(binary) MPARC_free(binary);
									structure->my_err = MPARC_FERROR;
									return MPARC_FERROR;
								}

								binary[i] = '\0';
							}
							{
								crc_t crc = crc_init();
								crc = crc_update(crc, binary, sout);
								crc = crc_finalize(crc);
								if(crc != crc3){
									if(fname) MPARC_free(fname);
									if(binary) MPARC_free(binary);
									if(listy) MPARC_free(listy);
									fclose(fps);
									errno = EILSEQ;
									structure->my_err = MPARC_CHKSUM;
									return MPARC_CHKSUM;
								}
							}
						}
						if(fflush(fps) == EOF){
							if(fname) MPARC_free(fname);
							if(listy) MPARC_free(listy);
							fclose(fps);
							structure->my_err = MPARC_FERROR;
							return MPARC_FERROR;
						}
					}

					if(fname) MPARC_free(fname);
					fclose(fps);
				}
				if(listy) MPARC_list_array_free(&listy);
			}
			return MPARC_OK;
		}

		MXPSQL_MPARC_err MPARC_extract(MXPSQL_MPARC_t* structure, const char* destdir, char** dir2make){
			return MPARC_extract_advance(structure, destdir, dir2make, NULL, NULL, NULL, NULL);
		}


		MXPSQL_MPARC_err MPARC_readdir(MXPSQL_MPARC_t* structure, const char* srcdir, bool recursive, int (*listdir)(const char*, bool, char***, void*), void* listdir_ctx){
			if(listdir == NULL) return MPARC_NULL;

			char** flists = NULL;
			MXPSQL_MPARC_err err = MPARC_OK;

			if(listdir(srcdir, recursive, &flists, listdir_ctx) != 0){
				err = MPARC_FERROR;
				goto main_errhandler;
			}

			for(MXPSQL_MPARC_uint_repr_t i = 0; flists[i] != NULL; i++){
				err = MPARC_push_filename(structure, flists[i]);
				if(err != MPARC_OK){
					goto main_errhandler;
				}
			}
 

			goto main_errhandler;
			main_errhandler:
			structure->my_err = err;
			for(MXPSQL_MPARC_uint_repr_t i = 0; flists[i] != NULL; i++){
				if(flists[i] != NULL) {
					MPARC_free(flists[i]);
				}
			}
			if(flists) MPARC_free(flists);

			return err;
		}


		MXPSQL_MPARC_err MPARC_parse_str_advance(MXPSQL_MPARC_t* structure, const char* StegoStringy, bool erronduplicate, bool sensitive){
			MXPSQL_MPARC_err err = MPARC_OK;
			char* Stringy = (char*) StegoStringy;
			if(sensitive){
				Stringy = strstr(Stringy, STANKY_MPAR_FILE_FORMAT_MAGIC_NUMBER_25);
				if(!Stringy) return MPARC_NOTARCHIVE;
			}
			{
				char* s3 = MPARC_strdup(Stringy);
				if(s3 == NULL) {
					err = MPARC_OOM;
					goto endy;
				}
				err = MPARC_i_parse_header(structure, s3);
				if(s3) MPARC_free(s3);
				if(err != MPARC_OK) {
					goto endy;
				}
			}
			{
				char* s3 = MPARC_strdup(Stringy);
				if(s3 == NULL) {
					err = MPARC_OOM;
					goto endy;
				}
				err = MPARC_i_parse_entries(structure, s3, erronduplicate, sensitive);
				if(s3) MPARC_free(s3);
				if(err != MPARC_OK) {
					goto endy;
				}
			}
			{
				char* s3 = MPARC_strdup(Stringy);
				if(s3 == NULL) {
					err = MPARC_OOM;
					goto endy;
				}
				err = MPARC_i_parse_ender(structure, s3, sensitive);
				if(s3) MPARC_free(s3);
				if(err != MPARC_OK) {
					goto endy;
				}
			}

			goto endy; // redundant

			endy:
			structure->my_err = err;
			return err;
		}

		MXPSQL_MPARC_err MPARC_parse_str(MXPSQL_MPARC_t* structure, const char* stringy){
			return MPARC_parse_str_advance(structure, stringy, false, false);
		}

		MXPSQL_MPARC_err MPARC_parse_filestream(MXPSQL_MPARC_t* structure, FILE* fpstream){
			MXPSQL_MPARC_uint_repr_t filesize = 0;
			if(fseek(fpstream, 0, SEEK_SET) != 0){
				structure->my_err = MPARC_FERROR;
				return MPARC_FERROR;
			}

			while(fgetc(fpstream) != EOF && !ferror(fpstream)){
				filesize += 1;
			}
			if(ferror(fpstream)){
				structure->my_err = MPARC_FERROR;
				return MPARC_FERROR;
			}

			clearerr(fpstream);

			if(fseek(fpstream, 0, SEEK_SET) != 0){
					structure->my_err = MPARC_FERROR;
					return MPARC_FERROR;
			}

			char* binary = MPARC_calloc(filesize+1, sizeof(char)); // binary because files are binary, but not unsigned because it is ascii
			CHECK_LEAKS();
			if(binary == NULL){
				structure->my_err = MPARC_FERROR;
				return MPARC_OOM;
			}

			if(filesize >= MPARC_DIRECTF_MINIMUM){
				if(fread(binary, sizeof(unsigned char), filesize, fpstream) < filesize && ferror(fpstream)){
					if(binary) MPARC_free(binary);
					structure->my_err = MPARC_FERROR;
					return MPARC_FERROR;
				}
			}
			else{
				int c = 0;
				MXPSQL_MPARC_uint_repr_t i = 0;
				while((c = fgetc(fpstream)) != EOF){
					binary[i++] = c;
				}

				if(ferror(fpstream)){
					if(binary) MPARC_free(binary);
					structure->my_err = MPARC_FERROR;
					return MPARC_FERROR;
				}

				binary[i] = '\0';
			}

			MXPSQL_MPARC_err err = MPARC_parse_str(structure, binary);
			return err;
		}

		MXPSQL_MPARC_err MPARC_parse_filename(MXPSQL_MPARC_t* structure, const char* filename){
			FILE* filepointerstream = fopen(filename, "r");
			if(filepointerstream == NULL) return MPARC_FERROR;
			MXPSQL_MPARC_err err = MPARC_parse_filestream(structure, filepointerstream);
			fclose(filepointerstream);
			return err;
		}



		void* MPARC_malloc(MXPSQL_MPARC_uint_repr_t size){
			return malloc(size);
		}

		void* MPARC_calloc(MXPSQL_MPARC_uint_repr_t arr_size, size_t el_size){
			return calloc(arr_size, el_size);
		}

		void* MPARC_realloc(void* oldmem, MXPSQL_MPARC_uint_repr_t newsize){
			return realloc(oldmem, newsize);
		}

		void MPARC_free(void* mem){
			free(mem);
		}



		size_t MPARC_MXPSQL_MPARC_t_sizeof(void){
			return sizeof(MXPSQL_MPARC_t);
		}

		size_t MPARC_MXPSQL_MPARC_iter_t_sizeof(void){
			return sizeof(MXPSQL_MPARC_iter_t);
		}
		/* END OF MAIN CODE */

/* END OF MY SECTION */


#endif
