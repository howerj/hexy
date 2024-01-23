/* Project: Generic Hexdump routines
 * Missing: Grouping bytes and endianess conversion, turn into library, 
 * signed printing, header-only library, unit tests, 
 * customizable separators strings (not chars, NULL = default), colors, undump,
 * fix bugs, prefix with hexy_, turn off FILE* support,
 * C++ integration, overflow checks, BUILD_BUG_ON,
 * more assertions, export helper functions as well, upper case hex chars, ...
 * Author: Richard James Howe */

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

#define HEXY_AUTHOR "Richard James Howe"
#define HEXY_EMAIL "howe.r.j.89@gmail.com"
#define HEXY_VERSION "v0.1"
#define HEXY_REPO "https://github.com/howerj/hexy"
#define HEXY_LICENSE "Public Domain / The Unlicense"

#ifndef HEXY_API
#define HEXY_API static
#endif

#ifndef HEXY_EXTERN
#define HEXY_EXTERN extern
#endif

#if 0
HEXY_EXTERN void hexy_reverse(char * const r, const size_t length);
HEXY_EXTERN int hexy_isgraph(const int ch);
HEXY_EXTERN int hexy_islower(const int ch);
HEXY_EXTERN int hexy_toupper(const int ch);
HEXY_EXTERN int hexy_unum_to_string(char buf[/*static HEXY_PNUM_BUF_SIZE*/], hexy_unum_t in, hexy_unum_t base);
HEXY_EXTERN int hexy_buffer_get(void *in);
HEXY_EXTERN int hexy_buffer_put(void *out, const int ch);
HEXY_EXTERN int hexy_file_get(void *in);
HEXY_EXTERN int hexy_file_put(void *out, int ch);
HEXY_EXTERN int hexy_get(hexy_io_t *io);
HEXY_EXTERN int hexy_put(hexy_io_t *io, const int ch);
HEXY_EXTERN int hexy_puts(hexy_io_t *io, const char *s);
HEXY_EXTERN int hexy_print_number(hexy_io_t *io, hexy_unum_t u, hexy_unum_t base, int char_count, int char_to_repeat, bool left_align);
HEXY_EXTERN int hexy_unsigned_integer_logarithm(hexy_unum_t n, hexy_unum_t base);
HEXY_EXTERN int hexy_getopt(hexy_getopt_t *opt, const int argc, char *const argv[], const char *fmt);
HEXY_EXTERN int hexy(hexy_t *h);
HEXY_EXTERN int hexy_convert(const char *n, int base, long *out);
HEXY_EXTERN int hexy_flag(const char *v);
HEXY_EXTERN int hexy_options_set(hexy_options_t *os, size_t olen, char *kv, FILE *error);
#endif



//
//#ifdef HEXY_IMPLEMENTATION
//#ifdef HEXY_UNIT_TESTS
//#ifdef HEXY_DEFINE_MAIN
//
#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */


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
#define HEXY_MAX_NCOLS (64)
#endif

#define HEXY_MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define HEXY_MAX(X, Y) ((X) < (Y) ? (Y) : (X))
#define HEXY_NELEMS(X) (sizeof(X) / sizeof ((X)[0]))

#ifndef HEXY_ELINE /* This is used for returning a line dependent error code; a crude and simple way to debug */
#define HEXY_ELINE (-__LINE__) 
#endif

typedef unsigned long hexy_unum_t;

#define HEXY_UNUM_MAX (ULONG_MAX) /* maximum value in `hexy_unum_t` */

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
	uint8_t buf[HEXY_MAX_NCOLS]; /* Buffer used to store characters for `chars_on` */

        /* Each line looks like this: "XXXX: XX XX XX XX |....|",
	 * where "X" is a digit it is possible to change what is printed out at 
	 * different points in the line. The macros "HEXY_SEP_*" contain the
	 * default  */
	char *sep_adr,               /* Separator for address and bytes */
	     *sep_eol,               /* End of line separator, usually "\n" */
	     *sep_byt,               /* Separator between each of the bytes */
	     *sep_ch1,               /* Separator for area between end of bytes and start of character */
	     *sep_ch2;               /* Separator for area between end of character printing and newline */

	bool init,                   /* Has this structure been initialized? */
	     chars_off,              /* Turn off: character printing on, implies newlines are on */
	     addresses_off,          /* Turn off: address printing, implies newlines are on */
	     newlines_off,           /* Turn off: Print a new line, columnizing output */
	     hex_uppercase_on;       /* If true: use upper case hex digits */

	int base,                    /* Base to print, if 0 auto-select, otherwise valid bases are between 2 and 36 */
	    abase,                   /* Base to print addresses in, if 0 used `base` */
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

enum {
	HEXY_OPTIONS_INVALID_E,
	HEXY_OPTIONS_BOOL_E,
	HEXY_OPTIONS_LONG_E,
	HEXY_OPTIONS_STRING_E,
};

typedef struct { /* Used for parsing key=value strings (strings must be modifiable and persistent) */
	char *opt,  /* key; name of option */
	     *help; /* help string for option */
	union { /* pointers to values to set */
		bool *b; 
		long *n; 
		char **s; 
	} v;
	int type; /* type of value, in following union, e.g. HEXY_OPTIONS_LONG_E. */
} hexy_options_t; /* N.B. This could be used for saving configurations as well as setting them */

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

HEXY_API int hexy_toupper(const int ch) {
	return hexy_islower(ch) ? ch ^ 0x20 : ch;
}

HEXY_API int hexy_unum_to_string(char buf[/*static HEXY_PNUM_BUF_SIZE*/], hexy_unum_t in, hexy_unum_t base) {
	assert(buf);
	size_t i = 0;
	hexy_unum_t dv = in;
	if (!hexy_is_valid_base(base))
		return HEXY_ELINE;
	do {
		static const char *hexy_string_digits = "0123456789abcdefghijklmnopqrstuvwxyz";
		const int ch = hexy_string_digits[dv % base];
		buf[i++] = /*h->hex_uppercase_on ? hexy_toupper(ch) : */ch;
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

HEXY_API int hexy_print_number(hexy_io_t *io, hexy_unum_t u, hexy_unum_t base, int char_count, int char_to_repeat, bool left_align) {
	assert(io);
	if (left_align)
		for (int i = 0; i < char_count; i++)
			if (hexy_put(io, char_to_repeat) < 0)
				return HEXY_ELINE;
	char buf[HEXY_PNUM_BUF_SIZE] = { 0, };
	if (hexy_unum_to_string(buf, u, base) < 0)
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



HEXY_API int hexy_aligned_print_number(hexy_io_t *io, hexy_unum_t u, hexy_unum_t base, hexy_unum_t max, int leading_zeros, int leading_char) {
	assert(io);
	const int mint = hexy_unsigned_integer_logarithm(max, base) - hexy_unsigned_integer_logarithm(u, base);
	leading_zeros = HEXY_MAX(0, leading_zeros);
	leading_zeros = HEXY_MIN(mint, leading_zeros);
	return hexy_print_number(io, u, base, leading_zeros, leading_char, true);
}

HEXY_API bool hexy_newlines_enabled(hexy_t *h) {
	if (!h->newlines_off)
		return true;
	if (!h->chars_off)
		return true;
	if (!h->addresses_off)
		return true;
	return false;
}

HEXY_API int hexy_newline(hexy_t *h) {
	assert(h);
	if (hexy_newlines_enabled(h))
		if (hexy_puts(&h->io, h->sep_eol) < 0)
			return HEXY_ELINE;
	return 0;
}

HEXY_API int hexy_space(hexy_t *h) {
	assert(h);
	if (hexy_put(&h->io, ' ') < 0)
		return HEXY_ELINE;
	return 0;
}

HEXY_API int hexy_spaces(hexy_t *h, int spaces) {
	assert(h);
	spaces = HEXY_MAX(spaces, 0);
	for (int i = 0; i < spaces; i++)
		if (hexy_space(h) < 0)
			return HEXY_ELINE;
	return 0;
}

HEXY_API int hexy_print_byte(hexy_t *h, int ch) {
	assert(h);
	ch = hexy_isgraph(ch) ? ch : HEXY_NON_GRAPHIC_REPLACEMENT_CHAR;
	if (hexy_put(&h->io, ch) < 0)
		return HEXY_ELINE;
	return 0;
}

HEXY_API int hexy_print_address(hexy_t *h, hexy_unum_t addr) {
	assert(h);
	if (h->addresses_off)
		return 0;
	if (hexy_aligned_print_number(&h->io, addr, h->abase, 65535, 4, ' ') < 0)
		return HEXY_ELINE;
	if (hexy_puts(&h->io, h->sep_adr) < 0)
		return HEXY_ELINE;
	return 0;
}

HEXY_API int hexy_validate(hexy_t *h) {
	assert(h);
	if (h->io.error)
		return HEXY_ELINE;
	if (!hexy_is_valid_base(h->base))
		return HEXY_ELINE;
	if (h->ncols < 1 || h->ncols > HEXY_MAX_NCOLS)
		return HEXY_ELINE;
	if (h->buf_used > sizeof (h->buf))
		return HEXY_ELINE;
	return 0;
}

HEXY_API int hexy_config_default(hexy_t *h) {
	assert(h);
	h->base  = h->base  ? h->base  : 16 ;
	h->ncols = h->ncols ? h->ncols : 16 ;
	h->abase = h->abase ? h->abase : h->base;

	h->sep_adr = h->sep_adr ? h->sep_adr : HEXY_SEP_ADR;
	h->sep_eol = h->sep_eol ? h->sep_eol : HEXY_SEP_EOL;
	h->sep_ch1 = h->sep_ch1 ? h->sep_ch1 : HEXY_SEP_CH1;
	h->sep_ch2 = h->sep_ch2 ? h->sep_ch2 : HEXY_SEP_CH2 ;
	h->sep_byt = h->sep_byt ? h->sep_byt : HEXY_SEP_BYT ;

	h->init = true;

	return hexy_validate(h);
}

HEXY_API int hexy(hexy_t *h) {
	assert(h);
	hexy_io_t *io = &h->io;
	if (hexy_config_default(h) < 0)
		return HEXY_ELINE;

	const int byte_align = hexy_unsigned_integer_logarithm(255, h->base);

	for (int end = 0; !end;) {
		h->buf_used = 0;
		for (int i = 0; i < h->ncols; i++) {
			const int ch = hexy_get(io);
			if (ch < 0)
				end = 1;
			if (end && !i)
				goto done;
			if (end)
				break;
			h->buf[h->buf_used++] = ch;
		}
		if (hexy_print_address(h, h->address) < 0)
			return HEXY_ELINE;
		for (size_t i = 0; i < h->buf_used; i++) {
			if (hexy_aligned_print_number(io, h->buf[i], h->base, 255, byte_align, '0') < 0)
				return HEXY_ELINE;
			if (hexy_puts(io, h->sep_byt) < 0)
				return HEXY_ELINE;
		}
		if (!h->chars_off) {
			const int missing = h->ncols - h->buf_used;
			const int bseplen = strlen(h->sep_byt);
			if (hexy_spaces(h, (byte_align + bseplen) * missing) < 0) /* TODO: Use h->sep_byt? */
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
		if ((h->address + h->buf_used) <= h->address)
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
		if (fprintf(out, "- `%s`=[%s] %s\n", o->opt, type, o->help ? o->help : "") < 0)
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
			if (o == '#')
				if (hexy_convert(opt->arg, 0, &opt->narg) < 0)
					return BADNUM_E;
		} else if (argc <= ++opt->index) { /* no arg */
			opt->place = "";
			if (HEXY_GETOPT_NEEDS_ARG(*fmt))
				return BADARG_E;
			if (opt->error)
				if (fprintf(opt->error, "option requires an argument -- %c\n", opt->option) < 0)
					return BADIO_E;
			return BADCH_E;
		} else	{ /* white space */
			opt->arg = argv[opt->index];
			if (o == '#')
				if (hexy_convert(opt->arg, 0, &opt->narg) < 0)
					return BADNUM_E;
		}
		opt->place = "";
		opt->index++;
	}
#undef HEXY_GETOPT_NEEDS_ARG
	return opt->option; /* dump back option letter */
}

#ifdef HEXY_IMPLEMENTATION

HEXY_API int hexy_help(FILE *out, const char *arg0) {
	assert(out);
	assert(arg0);
	const char *fmt = "Usage: %s [-bBn #] [-h] files...\n\n\
Author:  " HEXY_AUTHOR "\n\
Repo:    " HEXY_REPO "\n\
Email:   " HEXY_EMAIL "\n\
License: " HEXY_LICENSE "\n\
Version: " HEXY_VERSION "\n\n\
A customizable hex-dump library and utility.\n\n\
Options:\n\n\
\t-h\tPrint this help message and exit.\n\
\t-b #\tSet base for output, valid ranges are from 2 to 36.\n\
\t-B #\tSet base for address printing, uses same base as byte output if not set.\n\
\t-n #\tSet number of columns of values to print out.\n\
\t-o k=v\tSet a number of key-value pair options.\n\
\n\
Options settable by `-o` flag:\n\n\
 * `eol`=string:  Set end of line string\n\
 * `adr`=string: Set string to print after address\n\
 * `byt`=string: Set string to print in between bytes \n\
 * `ch1`=string: Set string to print after bytes and before character view\n\
 * `ch2`=string: Set string to print after character view and before newline \n\
 * `chars-off`=bool: Turn character printing off\n\
 * `address-off`=bool: Turn address printing off\n\
 * `newlines-off`=bool: Turn newline printing off\n\
 * `uppercase-hex-on`=bool: Turn on upper case printing of hex values\n\
\n\
This program returns zero on success and non-zero on failure.\n\n";
	return fprintf(out, fmt, arg0);
}

HEXY_API int hexy_sf(hexy_t *h, const char *in, FILE *out) {
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
		{  .opt  =  "eol",               .v.s  =  &h->sep_eol,           .type = HEXY_OPTIONS_STRING_E, },
		{  .opt  =  "adr",               .v.s  =  &h->sep_adr,           .type = HEXY_OPTIONS_STRING_E, },
		{  .opt  =  "byt",               .v.s  =  &h->sep_byt,           .type = HEXY_OPTIONS_STRING_E, },
		{  .opt  =  "ch1",               .v.s  =  &h->sep_ch1,           .type = HEXY_OPTIONS_STRING_E, },
		{  .opt  =  "ch2",               .v.s  =  &h->sep_ch2,           .type = HEXY_OPTIONS_STRING_E, },
		{  .opt  =  "chars-off",         .v.b  =  &h->chars_off,         .type = HEXY_OPTIONS_BOOL_E,   },
		{  .opt  =  "address-off",       .v.b  =  &h->addresses_off,     .type = HEXY_OPTIONS_BOOL_E,   },
		{  .opt  =  "newlines-off",      .v.b  =  &h->newlines_off,      .type = HEXY_OPTIONS_BOOL_E,   },
		{  .opt  =  "uppercase-hex-on",  .v.b  =  &h->hex_uppercase_on,  .type = HEXY_OPTIONS_BOOL_E,   },
	};

	for (int ch = 0; (ch = hexy_getopt(&opt, argc, argv, "hb#B#n#s:o:")) != -1;) {
		switch (ch) {
		case 'h': return hexy_help(stderr, argv[0]) < 0;
		case 'b': h->base = opt.narg; break;
		case 'B': h->abase = opt.narg; break;
		case 'n': h->ncols = opt.narg; break;
		case 'o': if (hexy_options_set(&kv[0], HEXY_NELEMS(kv), opt.arg, stderr) < 0) return 1; break;
		case 's': if (hexy_sf(h, opt.arg, stdout) < 0) return 1; break;
		default: (void)fprintf(stderr, "Invalid option '%c', consult help (-h)\n", ch); return 1;
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

#endif /* HEXY_IMPLEMENTATION */
#endif /* HEXY_H */
