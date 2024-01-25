#ifndef HEXY_H
#define HEXY_H

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HEXY_AUTHOR  "Richard James Howe"
#define HEXY_EMAIL   "howe.r.j.89@gmail.com"
#define HEXY_VERSION "v0.2"
#define HEXY_REPO    "https://github.com/howerj/hexy"
#define HEXY_LICENSE "Public Domain / The Unlicense"

/* This project contains a set of routines for printing out arrays of
 * bytes as a hexdump with an optional character view and address
 * output, as well as a customizable output base. There are also routines
 * that are used to implement a command line utility that utilizes this
 * library and an optional "main()" definition. The library contains more
 * functions than are strictly needed as it has been used as a test-bed
 * for those functions.
 *
 * The project is in the form of a header-only library (see
 * <https://en.wikipedia.org/wiki/Header-only>, and
 * <https://stackoverflow.com/questions/12671383>). Also see
 * <https://github.com/howerj/localely> for another example of a header
 * only library.
 *
 * It is easy to port this library to new platforms, even micro-controllers,
 * which may lack access to standard input and output. This is achieved via
 * a few callbacks that abstract I/O out.
 *
 * To use this library the following macros can be defined depending on
 * how you want to use it. They are:
 *
 * - `HEXY_API`: Applied to the function definitions.
 * - `HEXY_EXTERN`: Applied to the function declarations.
 * - `HEXY_IMPLEMENTATION`: If defined the definitions and not
 *   just the declarations of functions are made.
 * - `HEXY_DEFINE_MAIN`: If defined the example "main()" function is
 *   defined, which can be used to test the library out.
 * - `HEXY_UNIT_TESTS`: If defined then the unit tests are compiled
 *   in, otherwise the unit test function will be part of the API, it
 *   will just always return success.
 *
 * There are other macros defined however they are used for options to
 * control the behavior of the library and are not used to control how
 * the library is built, it is possible to make all the exported functions
 * `static inline` or `extern` with the right incantations.
 *
 * This library makes heavy use of asserts to ensure correctness,
 * to turn these off use `NDEBUG` as usual.
 *
 * Some example scenarios might help to illustrate the use of
 * this library, imagine two files, this header file (`hexy.h`)
 * and a test file `test.c`.
 *
 * If `test.c` contains:
 *
 * 	#define HEXY_IMPLEMENTATION
 * 	#define HEXY_DEFINE_MAIN
 * 	#define HEXY_EXTERN static inline
 * 	#define HEXY_API HEXY_EXTERN
 * 	#include "hexy.h"
 *
 * Then we can compile the program and run the program like so:
 *
 * 	cc test.c -o test
 * 	./test file.bin
 *
 * All functions that would usually be exported are defined as
 * being `static inline` which means that any functions not used
 * are optimized out, and more optimizations can be done by the
 * compiler (for example more aggressive and accurate dead code
 * elimination).
 *
 * If you would like to use the hex routines as a library because
 * they are used in multiple places throughout your larger program
 * and project then we use the following definitions in `test.c`:
 *
 *	#define HEXY_IMPLEMENTATION
 *	#include "hexy.h"
 *
 * And compile like so:
 *
 *	cc test.c -c -o hexy.o
 *	ar rcs libhexy.a hexy.o
 *	
 * Which you can link against normally, and so something similar
 * to produce a dynamic library (either an ".so" or a ".dll" file).
 *
 * You can also include this library multiple times within the same
 * program by defining the following prior to including `hexy.h`.
 *
 *	#define HEXY_API static inline
 *	#define HEXY_EXTERN HEXY_API
 *	#include "hexy.h"
 *
 * This is useful for temporarily including these functions for
 * debugging purposes. "main()" will not be defined.
 *
 *
 * TODO: signed printing, unit tests, colors, undump, 
 * optional FILE* support, escape character support,
 * C++ integration, overflow checks, BUILD_BUG_ON,
 * more assertions, help section and explain this
 * library, ...
 */

#ifndef HEXY_API
#define HEXY_API
#endif

#ifndef HEXY_EXTERN
#define HEXY_EXTERN extern
#endif

#ifndef HEXY_INTERNAL
#define HEXY_INTERNAL static inline
#endif

#ifdef HEXY_DEFINE_MAIN
#define HEXY_UNIT_TESTS
#endif

#define HEXY_PNUM_BUF_SIZE (64/*64 bit number in base 2*/ + 1/* for '+'/'-' */ + 1/*NUL terminator*/)

#define HEXY_SEP_ADR ":\t"
#define HEXY_SEP_EOL "\n"
#define HEXY_SEP_CH1 "  |"
#define HEXY_SEP_CH2 "|"
#define HEXY_SEP_BYT " "

#ifndef HEXY_NON_GRAPHIC_REPLACEMENT_CHAR
#define HEXY_NON_GRAPHIC_REPLACEMENT_CHAR '.'
#endif

#ifndef HEXY_MAX_NCOLS
#define HEXY_MAX_NCOLS (32)
#endif

#ifndef HEXY_MAX_GROUP
#define HEXY_MAX_GROUP (8)
#endif

#define HEXY_MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define HEXY_MAX(X, Y) ((X) < (Y) ? (Y) : (X))
#define HEXY_NELEMS(X) (sizeof(X) / sizeof ((X)[0]))

#ifndef HEXY_ELINE /* This is used for returning a line dependent error code; a crude and simple way to debug */
#define HEXY_ELINE (-__LINE__) 
#endif

typedef unsigned long hexy_unum_t;

#define HEXY_UNUM_MAX (ULONG_MAX) /* maximum value in `hexy_unum_t` */

#define HEXY_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#define hexy_implies(P, Q) assert(!(P) || (Q))
#define hexy_mutal(P, Q) do { hexy_implies(P, Q); hexy_implies(Q, P); } while (0)
#define hexy_never assert(0)

typedef struct {
	uint8_t *b;    /* data buffer */
	size_t used,   /* number of bytes used in `b` */
	       length; /* length of `b` in bytes */
} hexy_buffer_t; /* generic data buffer, used for I/O to/from a buffer */

typedef int (*hexy_get_fn)(void *in);          /* callback for retrieving a byte, similar to `fgetc`. */
typedef int (*hexy_put_fn)(void *out, int ch); /* callback for outputting a byte, similar to `fputc`. */

typedef struct {
	hexy_get_fn get;               /* return negative on error, a byte (0-255) otherwise */
	hexy_put_fn put;               /* return ch on no error, or negative on error */
	void *in, *out;                /* passed to 'get' and 'put' respectively */
	size_t read, wrote;            /* read only, bytes 'get' and 'put' respectively */
	int error;                     /* an error has occurred */
} hexy_io_t; /* I/O abstraction, use to redirect to wherever you want... */

typedef struct {
	hexy_io_t io;                /* Hexdump I/O abstraction layer */
	uint64_t address;            /* Address to print if enabled, auto-incremented */
	size_t buf_used;             /* Number of bytes in buf used */
	uint8_t buf[HEXY_MAX_NCOLS * HEXY_MAX_GROUP]; /* Buffer used to store characters for `chars_on` / data */

        /* Each line looks like this: "XXXX: XX XX XX XX |....|",
	 * where "X" is a digit it is possible to change what is printed out at 
	 * different points in the line. The macros "HEXY_SEP_*" contain the
	 * default. Accepting a format string (which would replace these
	 * `sep_*` values) and handling escape characters are two possible extensions. */
	char *sep_adr,               /* Separator for address and bytes */
	     *sep_eol,               /* End of line separator, usually "\n" */
	     *sep_byt,               /* Separator between each of the bytes */
	     *sep_ch1,               /* Separator for area between end of bytes and start of character */
	     *sep_ch2;               /* Separator for area between end of character printing and newline */

	bool init,                   /* Has this structure been initialized? */
	     chars_off,              /* Turn off: character printing on, implies newlines are on */
	     addresses_off,          /* Turn off: address printing, implies newlines are on */
	     newlines_off,           /* Turn off: Print a new line, columnizing output */
	     uppercase_on,           /* If true: use upper case hex digits */
	     rev_grp_on;             /* If true; reverse the group before printing (effectively changing endianess) */

	int base,                    /* Base to print, if 0 auto-select, otherwise valid bases are between 2 and 36 */
	    abase,                   /* Base to print addresses in, if 0 used `base` */
	    group,                   /* Group bytes together in groups of X */
	    ncols;                   /* Number of columns to print, must not exceed HEXY_MAX_NCOLS, if 0 auto-select*/
} hexy_t; /* Hexdump structure, for all your hex-dumping needs */

typedef struct {
	char *arg;   /* parsed argument */
	long narg;   /* converted argument for '#' */
	int index,   /* index into argument list */
	    option,  /* parsed option */
	    reset;   /* set to reset */
	FILE *error, /* error stream to print to (set to NULL to turn off */
	     *help;  /* if set, print out all options and return */
	char *place; /* internal use: scanner position */
	int  init;   /* internal use: initialized or not */
} hexy_getopt_t;     /* getopt clone; with a few modifications */

enum { /* used with `hexy_options_t` structure */
	HEXY_OPTIONS_INVALID_E, /* default to invalid if option type not set */
	HEXY_OPTIONS_BOOL_E,    /* select boolean `b` value in union `v` */
	HEXY_OPTIONS_LONG_E,    /* select numeric long `n` value in union `v` */
	HEXY_OPTIONS_STRING_E,  /* select string `s` value in union `v` */
};

typedef struct { /* Used for parsing key=value strings (strings must be modifiable and persistent) */
	char *opt,  /* key; name of option */
	     *help; /* help string for option */
	union { /* pointers to values to set */
		bool *b; 
		long *n; 
		char **s; 
	} v; /* union of possible values, selected on `type` */
	int type; /* type of value, in following union, e.g. HEXY_OPTIONS_LONG_E. */
} hexy_options_t; /* N.B. This could be used for saving configurations as well as setting them */

/* If function returns and `int` then negative indicates failure */
HEXY_EXTERN void hexy_reverse(char * const r, const size_t length);
HEXY_EXTERN bool hexy_is_valid_base(hexy_unum_t base);
HEXY_EXTERN int hexy_isgraph(const int ch);
HEXY_EXTERN int hexy_islower(const int ch);
HEXY_EXTERN int hexy_toupper(const int ch);
HEXY_EXTERN int hexy_tolower(const int ch);
HEXY_EXTERN int hexy_isupper(const int ch);
HEXY_EXTERN int hexy_isxdigit(const int ch);
HEXY_EXTERN int hexy_isdigit(const int ch);
HEXY_EXTERN int hexy_unum_to_string(char buf[/*static HEXY_PNUM_BUF_SIZE*/], hexy_unum_t in, hexy_unum_t base, bool upper);
HEXY_EXTERN int hexy_buffer_get(void *in);
HEXY_EXTERN int hexy_buffer_put(void *out, const int ch);
HEXY_EXTERN int hexy_file_get(void *in);
HEXY_EXTERN int hexy_file_put(void *out, int ch);
HEXY_EXTERN int hexy_get(hexy_io_t *io);
HEXY_EXTERN int hexy_put(hexy_io_t *io, const int ch);
HEXY_EXTERN int hexy_puts(hexy_io_t *io, const char *s);
HEXY_EXTERN int hexy_unsigned_integer_logarithm(hexy_unum_t n, hexy_unum_t base);
HEXY_EXTERN int hexy(hexy_t *h);
HEXY_EXTERN int hexy_convert(const char *n, int base, long *out);
HEXY_EXTERN int hexy_flag(const char *v);
HEXY_EXTERN int hexy_unescape(char *r, size_t length);

HEXY_EXTERN int hexy_getopt(hexy_getopt_t *opt, const int argc, char *const argv[], const char *fmt);
HEXY_EXTERN int hexy_options_set(hexy_options_t *os, size_t olen, char *kv, FILE *error);
HEXY_EXTERN int hexy_options_help(hexy_options_t *os, size_t olen, FILE *out);

HEXY_EXTERN int hexy_unit_tests(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef HEXY_IMPLEMENTATION

HEXY_API bool hexy_is_valid_base(hexy_unum_t base) {
	return base >=2 && base <= 36;
}

HEXY_API void hexy_reverse(char * const r, const size_t length) {
	const size_t last = length - 1;
	for (size_t i = 0; i < length/2ul; i++) {
		const size_t t = r[i];
		r[i] = r[last - i];
		r[last - i] = t;
	}
}

/* Nested functions, even without solving the upwards or downward
 * funarg problems and just banning references to variables in the
 * containing scope, would be useful for these small functions. */
HEXY_API int hexy_isgraph(const int ch) { /* avoiding the use of locale dependent functions */
	return ch > 32 && ch < 127;
}

HEXY_API int hexy_islower(const int ch) { 
	return ch >= 97 && ch <= 122; 
}

HEXY_API int hexy_isupper(const int ch) { 
	return ch >= 65 && ch <= 90;
}

HEXY_API int hexy_isdigit(const int ch) { 
	return ch >= 48 && ch <= 57;
}

HEXY_API int hexy_isxdigit(const int ch) {
	return (ch >= 65 && ch <= 70) || (ch >= 97 && ch <= 102) || hexy_isdigit(ch);
}

HEXY_API int hexy_toupper(const int ch) {
	return hexy_islower(ch) ? ch ^ 0x20 : ch;
}

HEXY_API int hexy_tolower(const int ch) {
	return hexy_isupper(ch) ? ch ^ 0x20 : ch;
}

HEXY_API int hexy_unum_to_string(char buf[/*static HEXY_PNUM_BUF_SIZE*/], hexy_unum_t in, hexy_unum_t base, bool upper) {
	assert(buf);
	size_t i = 0;
	hexy_unum_t dv = in;
	if (!hexy_is_valid_base(base))
		return HEXY_ELINE;
	do {
		static const char *hexy_string_digits = "0123456789abcdefghijklmnopqrstuvwxyz";
		const int ch = hexy_string_digits[dv % base];
		buf[i++] = upper ? hexy_toupper(ch) : ch;
	} while ((dv /= base));
	buf[i] = 0;
	hexy_reverse(buf, i);
	return 0;
}

HEXY_API int hexy_buffer_get(void *in) {
	hexy_buffer_t *b = in;
	assert(b);
	assert(b->b);
	if (b->used >= b->length)
		return HEXY_ELINE;
	return b->b[b->used++];
}

HEXY_API int hexy_buffer_put(void *out, const int ch) {
	hexy_buffer_t *b = out;
	assert(b);
	assert(b->b);
	if (b->used >= b->length)
		return HEXY_ELINE;
	return b->b[b->used++] = ch;
}

HEXY_API int hexy_file_get(void *in) {
	assert(in);
	return fgetc(in);
}

HEXY_API int hexy_file_put(void *out, int ch) {
	assert(out);
	return fputc(ch, (FILE*)out);
}

HEXY_API int hexy_get(hexy_io_t *io) {
	assert(io);
	if (io->error)
		return HEXY_ELINE;
	const int r = io->get(io->in);
	assert(r <= 255);
	io->read += r >= 0;
	return r;
}

HEXY_API int hexy_put(hexy_io_t *io, const int ch) {
	assert(io);
	if (io->error)
		return HEXY_ELINE;
	const int r = io->put(io->out, ch);
	assert(r <= 255);
	io->wrote += r >= 0;
	if (r < 0)
		io->error = -1;
	return r;
}

HEXY_API int hexy_puts(hexy_io_t *io, const char *s) {
	for (int ch = 0; (ch = *s++);)
		if (hexy_put(io, ch) < 0)
			return HEXY_ELINE;
	return 0;
}

HEXY_INTERNAL int hexy_hex_char_to_nibble(int c) {
	c = hexy_tolower(c);
	if ('a' <= c && c <= 'f')
		return 0xa + c - 'a';
	return c - '0';
}

/* converts up to two characters and returns number of characters converted */
HEXY_INTERNAL int hexy_hex_str_to_int(const char *str, int *const val) {
	assert(str);
	assert(val);
	*val = 0;
	if (!hexy_isxdigit(*str))
		return 0;
	*val = hexy_hex_char_to_nibble(*str++);
	if (!hexy_isxdigit(*str))
		return 1;
	*val = (*val << 4) + hexy_hex_char_to_nibble(*str);
	return 2;
}

HEXY_API int hexy_unescape(char *r, size_t length) { /* N.B. In place un-escaping! */
	assert(r);
	if (!length)
		return -1;
	size_t k = 0;
	for (size_t j = 0, ch = 0; (ch = r[j]) && k < length; j++, k++) {
		if (ch == '\\') {
			j++;
			switch (r[j]) {
			case '\0': return -1;
			case '\n': k--;         break; /* multi-line hack (Unix line-endings only) */
			case '\\': r[k] = '\\'; break;
			case  'a': r[k] = '\a'; break;
			case  'b': r[k] = '\b'; break;
			case  'e': r[k] = 27;   break;
			case  'f': r[k] = '\f'; break;
			case  'n': r[k] = '\n'; break;
			case  'r': r[k] = '\r'; break;
			case  't': r[k] = '\t'; break;
			case  'v': r[k] = '\v'; break;
			case  'x': {
				int val = 0;
				const int pos = hexy_hex_str_to_int(&r[j + 1], &val);
				if (pos < 1)
					return -2;
				j += pos;
				r[k] = val;
				break;
			}
			default:
				r[k] = r[j]; break;
			}
		} else {
			r[k] = ch;
		}
	}
	r[k] = '\0';
	return k;
}

HEXY_INTERNAL int hexy_print_number(hexy_io_t *io, hexy_unum_t u, hexy_unum_t base, int char_count, int char_to_repeat, bool left_align, bool upper) {
	assert(io);
	if (left_align)
		for (int i = 0; i < char_count; i++)
			if (hexy_put(io, char_to_repeat) < 0)
				return HEXY_ELINE;
	char buf[HEXY_PNUM_BUF_SIZE] = { 0, };
	if (hexy_unum_to_string(buf, u, base, upper) < 0)
		return HEXY_ELINE;
	for (size_t i = 0; i < HEXY_PNUM_BUF_SIZE && buf[i]; i++)
		if (hexy_put(io, buf[i]) < 0)
			return HEXY_ELINE;
	if (!left_align)
		for (int i = 0; i < char_count; i++)
			if (hexy_put(io, char_to_repeat) < 0)
				return HEXY_ELINE;
	return 0;
}

HEXY_API int hexy_unsigned_integer_logarithm(hexy_unum_t n, hexy_unum_t base) {
	int r = 0;
	do r++; while ((n /= base));
	return r;
}

HEXY_INTERNAL int hexy_aligned_print_number(hexy_io_t *io, hexy_unum_t u, hexy_unum_t base, hexy_unum_t max, int leading_zeros, int leading_char, bool upper) {
	assert(io);
	const int mint = hexy_unsigned_integer_logarithm(max, base) - hexy_unsigned_integer_logarithm(u, base);
	leading_zeros = HEXY_MAX(0, leading_zeros);
	leading_zeros = HEXY_MIN(mint, leading_zeros);
	return hexy_print_number(io, u, base, leading_zeros, leading_char, true, upper);
}

HEXY_INTERNAL bool hexy_newlines_enabled(hexy_t *h) {
	if (!h->newlines_off)
		return true;
	if (!h->chars_off)
		return true;
	if (!h->addresses_off)
		return true;
	return false;
}

HEXY_INTERNAL int hexy_newline(hexy_t *h) {
	assert(h);
	if (hexy_newlines_enabled(h))
		if (hexy_puts(&h->io, h->sep_eol) < 0)
			return HEXY_ELINE;
	return 0;
}

HEXY_INTERNAL int hexy_space(hexy_t *h) {
	assert(h);
	if (hexy_put(&h->io, ' ') < 0)
		return HEXY_ELINE;
	return 0;
}

HEXY_INTERNAL int hexy_spaces(hexy_t *h, int spaces) {
	assert(h);
	spaces = HEXY_MAX(spaces, 0);
	for (int i = 0; i < spaces; i++)
		if (hexy_space(h) < 0)
			return HEXY_ELINE;
	return 0;
}

HEXY_INTERNAL int hexy_print_byte(hexy_t *h, int ch) {
	assert(h);
	ch = hexy_isgraph(ch) ? ch : HEXY_NON_GRAPHIC_REPLACEMENT_CHAR;
	if (hexy_put(&h->io, ch) < 0)
		return HEXY_ELINE;
	return 0;
}

HEXY_INTERNAL int hexy_print_address(hexy_t *h, hexy_unum_t addr) {
	assert(h);
	if (h->addresses_off)
		return 0;
	if (hexy_aligned_print_number(&h->io, addr, h->abase, 65535, 4, ' ', h->uppercase_on) < 0)
		return HEXY_ELINE;
	if (hexy_puts(&h->io, h->sep_adr) < 0)
		return HEXY_ELINE;
	return 0;
}

HEXY_INTERNAL int hexy_validate(hexy_t *h) {
	assert(h);
	HEXY_BUILD_BUG_ON(HEXY_MAX_GROUP < 1);
	HEXY_BUILD_BUG_ON(HEXY_MAX_NCOLS < 1);
	HEXY_BUILD_BUG_ON(HEXY_PNUM_BUF_SIZE < 66);

	if (h->io.error)
		return HEXY_ELINE;
	if (!hexy_is_valid_base(h->base))
		return HEXY_ELINE;
	if (h->ncols < 1 || h->ncols > HEXY_MAX_NCOLS)
		return HEXY_ELINE;
	if (h->buf_used > sizeof (h->buf))
		return HEXY_ELINE;
	if (h->group < 1 || h->group > HEXY_MAX_GROUP)
		return HEXY_ELINE;
	return 0;
}

HEXY_INTERNAL int hexy_config_default(hexy_t *h) {
	assert(h);
	if (h->init)
		return hexy_validate(h);

	h->base  = h->base  ? h->base  : 16 ;
	h->ncols = h->ncols ? h->ncols : 16 ;
	h->abase = h->abase ? h->abase : h->base;

	h->sep_adr = h->sep_adr ? h->sep_adr : HEXY_SEP_ADR;
	h->sep_eol = h->sep_eol ? h->sep_eol : HEXY_SEP_EOL;
	h->sep_ch1 = h->sep_ch1 ? h->sep_ch1 : HEXY_SEP_CH1;
	h->sep_ch2 = h->sep_ch2 ? h->sep_ch2 : HEXY_SEP_CH2 ;
	h->sep_byt = h->sep_byt ? h->sep_byt : HEXY_SEP_BYT ;

	h->group = h->group ? h->group : 1;

	/* // This requires the separator strings to be modifiable, the default strings are not.
	if (hexy_unescape(h->sep_adr, strlen(h->sep_adr) + 1) < 0)
		return -1;
	if (hexy_unescape(h->sep_byt, strlen(h->sep_byt) + 1) < 0)
		return -1;
	if (hexy_unescape(h->sep_ch1, strlen(h->sep_ch1) + 1) < 0)
		return -1;
	if (hexy_unescape(h->sep_ch2, strlen(h->sep_ch2) + 1) < 0)
		return -1;
	if (hexy_unescape(h->sep_eol, strlen(h->sep_eol) + 1) < 0)
		return -1;*/

	if (hexy_validate(h) < 0)
		return -1;

	h->init = true;

	return 0;
}

HEXY_API int hexy(hexy_t *h) {
	assert(h);
	hexy_io_t *io = &h->io;
	if (hexy_config_default(h) < 0)
		return HEXY_ELINE;

	const int byte_align = hexy_unsigned_integer_logarithm(255, h->base);
	const int length = h->ncols * h->group;
	assert(length > 0 && length >= h->ncols);

	for (int end = 0; !end;) {
		h->buf_used = 0;
		for (int i = 0; i < length; i++) {
			const int ch = hexy_get(io);
			if (ch < 0)
				end = 1;
			if (end && !i)
				goto done;
			if (end)
				break;
			assert(h->buf_used < sizeof(h->buf));
			h->buf[h->buf_used++] = ch;
		}
		if (hexy_print_address(h, h->address) < 0)
			return HEXY_ELINE;
		if (h->rev_grp_on) { /* XYZ -> ZYX */
			/* There is a special case when reversing groups of
			 * bytes if the last group is incomplete (e.g. we are
			 * asked to reverse groups of four bytes but a multiple
			 * of four bytes is not provided).
			 *
			 * Currently we do not reverse the last group.
			 *
			 * We could optionally reverse last and zero fill missing bytes,
			 * alternatively an error could be returned, or the
			 * remaining bytes in the group could be reversed as a
			 * smaller unit. */
			const int limit = h->buf_used / h->group; /* skip reversing last group of not evenly divisible */
			for (int i = 0; i < limit; i++) {
				const size_t idx = i * h->group;
				assert(idx < sizeof (h->buf));
				hexy_reverse((char*)&h->buf[idx], h->group);
			}
		}

		for (size_t i = 0; i < (size_t)h->ncols; i++) {
			for (size_t j = 0; j < (size_t)h->group; j++) {
				const size_t idx = (i * h->group) + j;
				assert(idx < sizeof(h->buf));
				if (idx >= h->buf_used)
					break;
				if (hexy_aligned_print_number(io, h->buf[idx], h->base, 255, byte_align, '0', h->uppercase_on) < 0)
					return HEXY_ELINE;
			}
			if (hexy_puts(io, h->sep_byt) < 0)
				return HEXY_ELINE;
		}

		if (!h->chars_off) {
			const int elements = h->ncols * h->group;
			const int missing = elements - h->buf_used;
			assert(missing >= 0);
			assert(missing < 1024);
			if (hexy_spaces(h, byte_align * missing) < 0)
				return HEXY_ELINE;
			if (hexy_puts(io, h->sep_ch1) < 0)
				return HEXY_ELINE;
			for (size_t i = 0; i < h->buf_used; i++)
				if (hexy_print_byte(h, h->buf[i]) < 0)
					return HEXY_ELINE;
			if (hexy_spaces(h, missing) < 0)
				return HEXY_ELINE;
			if (hexy_puts(io, h->sep_ch2) < 0)
				return HEXY_ELINE;
		}
		if (hexy_newline(h) < 0)
			return HEXY_ELINE;
		if ((h->address + h->buf_used) <= h->address) /* overflow */
			goto fail;
		h->address += h->buf_used;
		h->buf_used = 0;
	}
	if (hexy_newline(h) < 0)
		return HEXY_ELINE;
done:
	return 0;
fail:
	io->error = -1;
	return HEXY_ELINE;
}

HEXY_API int hexy_flag(const char *v) {
	assert(v);

	static char *y[] = { "yes", "on", "true", };
	static char *n[] = { "no",  "off", "false", };

	for (size_t i = 0; i < HEXY_NELEMS(y); i++) {
		if (!strcmp(y[i], v))
			return 1;
		if (!strcmp(n[i], v))
			return 0;
	}
	return -1;
}

HEXY_API int hexy_convert(const char *n, int base, long *out) {
	assert(n);
	assert(out);
	*out = 0;
	char *endptr = NULL;
	errno = 0;
	const long r = strtol(n, &endptr, base);
	if (*endptr)
		return -1;
	if (errno == ERANGE)
		return -1;
	*out = r;
	return 0;
}

HEXY_API int hexy_options_help(hexy_options_t *os, size_t olen, FILE *out) {
	assert(os);
	assert(out);
	for (size_t i = 0; i < olen; i++) {
		hexy_options_t *o = &os[i];
		assert(o->opt);
		const char *type = "unknown";
		switch (o->type) {
		case HEXY_OPTIONS_BOOL_E: type = "bool"; break;
		case HEXY_OPTIONS_LONG_E: type = "long"; break;
		case HEXY_OPTIONS_STRING_E: type = "string"; break;
		case HEXY_OPTIONS_INVALID_E: /* fall-through */
		default: type = "invalid"; break;
		}
		if (fprintf(out, " * `%s`=%s: %s\n", o->opt, type, o->help ? o->help : "") < 0)
			return -1;
	}
	return 0;
}

HEXY_API int hexy_options_set(hexy_options_t *os, size_t olen, char *kv, FILE *error) {
	assert(os);
	char *k = kv, *v = NULL;
	if ((v = strchr(kv, '=')) == NULL || *v == '\0') {
		if (error)
			(void)fprintf(error, "invalid key-value format: %s\n", kv);
		return -1;
	}
	*v++ = '\0'; /* Assumes `kv` is writeable! */

	hexy_options_t *o = NULL;
	for (size_t i = 0; i < olen; i++) {
		hexy_options_t *p = &os[i];
		if (!strcmp(p->opt, k)) { o = p; break; }
	}
	if (!o) {
		if (error)
			(void)fprintf(error, "option `%s` not found\n", k);
		return -1;
	}

	switch (o->type) {
	case HEXY_OPTIONS_BOOL_E: {
		const int r = hexy_flag(v);
		assert(r == 0 || r == 1 || r == -1);
		if (r < 0) {
			if (error)
				(void)fprintf(error, "invalid flag in option `%s`: `%s`\n", k, v);
			return -1;
		}
		*o->v.b = !!r;
		break;
	}
	case HEXY_OPTIONS_LONG_E: { 
		const int r = hexy_convert(v, 0, o->v.n); 
		if (r < 0) {
			if (error)
				(void)fprintf(error, "invalid number in option `%s`: `%s`\n", k, v);
			return -1;
		}
		break; 
	}
	case HEXY_OPTIONS_STRING_E: { *o->v.s = v; /* Assumes `kv` is persistent! */ break; }
	default: return -1;
	}
	
	return 0;
}

/* Adapted from: <https://stackoverflow.com/questions/10404448>, this
 * could be extended to accept an array of options instead, or
 * perhaps it could be turned into a variadic functions,
 * that is not needed here. The function and structure should be turned
 * into a header only library. 
 *
 * This version handles parsing numbers with '#' and strings with ':'.
 *
 * Return value:
 *
 * - "-1": Finished parsing (end of options or "--" option encountered).
 * - ":": Missing argument (either number or string).
 * - "?": Bad option.
 * - "!": Bad I/O (e.g. `printf` failed).
 * - "#": Bad numeric argument (out of range, not a number, ...)
 *
 * Any other value should correspond to an option.
 *
 */
HEXY_API int hexy_getopt(hexy_getopt_t *opt, const int argc, char *const argv[], const char *fmt) {
	assert(opt);
	assert(fmt);
	assert(argv);
	enum { BADARG_E = ':', BADCH_E = '?', BADIO_E = '!', BADNUM_E = '#', OPTEND_E = -1, };

#define HEXY_GETOPT_NEEDS_ARG(X) ((X) == ':' || (X) == '#')

	if (opt->help) {
		for (int ch = 0; (ch = *fmt++);) {
			if (fprintf(opt->help, "\t-%c ", ch) < 0)
				return BADIO_E; 
			if (HEXY_GETOPT_NEEDS_ARG(*fmt)) {
				if (fprintf(opt->help, "%s", *fmt == ':' ? "<string>" : "<number>") < 0)
					return BADIO_E;
				fmt++;
			}
			if (fputs("\n", opt->help) < 0)
				return BADIO_E;
		}
		return OPTEND_E;
	}

	if (!(opt->init)) {
		opt->place = ""; /* option letter processing */
		opt->init  = 1;
		opt->index = 1;
	}

	if (opt->reset || !*opt->place) { /* update scanning pointer */
		opt->reset = 0;
		if (opt->index >= argc || *(opt->place = argv[opt->index]) != '-') {
			opt->place = "";
			return OPTEND_E;
		}
		if (opt->place[1] && *++opt->place == '-') { /* found "--" */
			opt->index++;
			opt->place = "";
			return OPTEND_E;
		}
	}

	const char *oli = NULL; /* option letter list index */
	opt->option = *opt->place++;
	if (HEXY_GETOPT_NEEDS_ARG(opt->option) || !(oli = strchr(fmt, opt->option))) { /* option letter okay? */
		 /* if the user didn't specify '-' as an option, assume it means -1.  */
		if (opt->option == '-')
			return OPTEND_E;
		if (!*opt->place)
			opt->index++;
		if (opt->error && !HEXY_GETOPT_NEEDS_ARG(*fmt))
			if (fprintf(opt->error, "illegal option -- %c\n", opt->option) < 0)
				return BADIO_E;
		return BADCH_E;
	}

	const int o = *++oli;
	if (!HEXY_GETOPT_NEEDS_ARG(o)) {
		opt->arg = NULL;
		if (!*opt->place)
			opt->index++;
	} else {  /* need an argument */
		if (*opt->place) { /* no white space */
			opt->arg = opt->place;
			if (o == '#') {
				if (hexy_convert(opt->arg, 0, &opt->narg) < 0) {
					if (opt->error)
						if (fprintf(opt->error, "option requires numeric value -- %s\n", opt->arg) < 0)
							return BADIO_E;
					return BADNUM_E;
				}
			}
		} else if (argc <= ++opt->index) { /* no arg */
			opt->place = "";
			if (HEXY_GETOPT_NEEDS_ARG(*fmt)) {
				return BADARG_E;
			}
			if (opt->error)
				if (fprintf(opt->error, "option requires an argument -- %c\n", opt->option) < 0)
					return BADIO_E;
			return BADCH_E;
		} else	{ /* white space */
			opt->arg = argv[opt->index];
			if (o == '#') {
				if (hexy_convert(opt->arg, 0, &opt->narg) < 0) {
					if (opt->error)
						if (fprintf(opt->error, "option requires numeric value -- %s\n", opt->arg) < 0)
							return BADIO_E;
					return BADNUM_E;
				}
			}
		}
		opt->place = "";
		opt->index++;
	}
#undef HEXY_GETOPT_NEEDS_ARG
	return opt->option; /* dump back option letter */
}

#ifdef HEXY_DEFINE_MAIN
#define HEXY_UNIT_TESTS
#endif

#ifdef HEXY_UNIT_TESTS
HEXY_API int hexy_unit_tests(void) {
	for (size_t i = 0; i < 256; i++) {
		if (hexy_isupper(hexy_tolower(i)))
			return -1;
		if (hexy_islower(hexy_toupper(i)))
			return -1;
	}

	return 0;
}
#else
HEXY_API int hexy_unit_tests(void) {
	/* Unit tests turned off, function still available */
	return 0;
}
#endif

#ifdef HEXY_DEFINE_MAIN

HEXY_INTERNAL int hexy_help(FILE *out, const char *arg0, hexy_options_t *kv, size_t kvlen) {
	assert(out);
	assert(arg0);
	const char *fmt = "Usage: %s [-bBng #] [-h] [-s string] files...\n\n\
Author:  " HEXY_AUTHOR "\n\
Repo:    " HEXY_REPO "\n\
Email:   " HEXY_EMAIL "\n\
License: " HEXY_LICENSE "\n\
Version: " HEXY_VERSION "\n\n\
A customizable hex-dump library and utility. This program returns zero\n\
on success and non-zero on failure. If `-r` is specified and a multiple\n\
of the bytes specified by `-g` is not provided then the last group is left\n\
unreversed.\n\n\
Options:\n\n\
\t-h\tPrint this help message and exit.\n\
\t-t\tRun built in self tests and exit (zero indicates success).\n\
\t-b #\tSet base for output, valid ranges are from 2 to 36.\n\
\t-B #\tSet base for address printing, uses same base as byte output if not set.\n\
\t-n #\tSet number of columns of values to print out.\n\
\t-g #\tGroup bytes together in the given number of bytes. (default = 1).\n\
\t-s str\tPerform a hexdump on the given string and then exit.\n\
\t-o k=v\tSet a number of key-value pair options.\n\
\t-r\tReverse byte order, no effect if `-g` option is 1.\n\
\t-R\tRaw mode; turn off printing everything except bytes.\n\
\n\
Options settable by `-o` flag:\n\n";
	const int r1 = fprintf(out, fmt, arg0);
	const int r2 = hexy_options_help(kv, kvlen, out);
	const int r3 = fputc('\n', out);
	return r1 < 0 || r2 < 0 || r3 != '\n' ? -1 : 0;
}

HEXY_INTERNAL int hexy_sf(hexy_t *h, const char *in, FILE *out) {
	assert(h);
	assert(in);
	assert(out);
	hexy_buffer_t b = { .b = (unsigned char*)in, .length = strlen(in), };
	hexy_io_t io = { .get = hexy_buffer_get, .put = hexy_file_put, .out = stdout, .in  = &b, };
	h->io = io;
	return hexy(h);
}

int main(int argc, char **argv) {
	hexy_io_t io = { .get = hexy_file_get, .put = hexy_file_put, .out = stdout, .in  = NULL, };
	hexy_t hexy_s = { .init = false, }, *h = &hexy_s;
	hexy_getopt_t opt = { .error = stderr, };
	hexy_options_t kv[] = {
		{ .opt = "sep-eol",      .v.s = &h->sep_eol,       .type = HEXY_OPTIONS_STRING_E, .help = "Set string to print at the end of line", },
		{ .opt = "sep-address",  .v.s = &h->sep_adr,       .type = HEXY_OPTIONS_STRING_E, .help = "Set string to print after printing address", },
		{ .opt = "sep-bytes",    .v.s = &h->sep_byt,       .type = HEXY_OPTIONS_STRING_E, .help = "Set string to print in between printing bytes", },
		{ .opt = "sep-ch1",      .v.s = &h->sep_ch1,       .type = HEXY_OPTIONS_STRING_E, .help = "Set string to print after bytes and before character view", },
		{ .opt = "sep-ch2",      .v.s = &h->sep_ch2,       .type = HEXY_OPTIONS_STRING_E, .help = "Set string to print after character view and before newline", },
		{ .opt = "chars-off",    .v.b = &h->chars_off,     .type = HEXY_OPTIONS_BOOL_E,   .help = "Turn character view off", },
		{ .opt = "address-off",  .v.b = &h->addresses_off, .type = HEXY_OPTIONS_BOOL_E,   .help = "Turn address printing off", },
		{ .opt = "newlines-off", .v.b = &h->newlines_off,  .type = HEXY_OPTIONS_BOOL_E,   .help = "Turn newline printing off", },
		{ .opt = "uppercase",    .v.b = &h->uppercase_on,  .type = HEXY_OPTIONS_BOOL_E,   .help = "Turn on printing upcase hex values", },
		{ .opt = "reverse",      .v.b = &h->rev_grp_on,    .type = HEXY_OPTIONS_BOOL_E,   .help = "Reverse the order of byte groups", },
	};

	for (int ch = 0; (ch = hexy_getopt(&opt, argc, argv, "hb#B#n#g#s:o:rRt")) != -1;) {
		switch (ch) {
		case 'h': return hexy_help(stderr, argv[0], &kv[0], HEXY_NELEMS(kv)) < 0;
		case 'b': h->base = opt.narg; break;
		case 'B': h->abase = opt.narg; break;
		case 'n': h->ncols = opt.narg; break;
		case 'g': h->group = opt.narg; break;
		case 'r': h->rev_grp_on = true; break;
		case 'R': h->chars_off = true; h->newlines_off = true; h->addresses_off = true; break;
		case 'o': if (hexy_options_set(&kv[0], HEXY_NELEMS(kv), opt.arg, stderr) < 0) return 1; break;
		case 's': if (hexy_sf(h, opt.arg, stdout) < 0) return 1; break;
		case 't': return hexy_unit_tests() < 0;
		default: return 1;
		}
	}

	h->io = io; /* -s option sets I/O struct if used, it needs to be reset here. */
	for (int i = opt.index; i < argc; i++) {
		errno = 0;
		FILE *f = fopen(argv[i], "rb");
		if (!f) {
			(void)fprintf(stderr, "Cannot open file %s (mode %s): %s\n", argv[i], "rb", strerror(errno));
			return 1;
		}
		h->io.in = f;
		const int r = hexy(h);
		errno = 0;
		if (fclose(f) < 0) {
			(void)fprintf(stderr, "fclose failed: %s\n", strerror(errno));
			return 1;
		}
		if (r < 0) {
			(void)fprintf(stderr, "hexdump failed: %d\n", r);
			return 1;
		}
	}
	return 0;
}

#endif /* HEXY_DEFINE_MAIN */
#endif /* HEXY_IMPLEMENTATION */
#endif /* HEXY_H */

