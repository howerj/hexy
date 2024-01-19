/* Project: Generic Hexdump routines
 * Missing: Grouping bytes and endianess conversion, turn into library, 
 * signed printing, header-only library, unit tests, CLI options,
 * customizable separators strings (not chars, NULL = default), colors, undump,
 * fix bugs, prefix with hex_, left/right align, turn off FILE* support,
 * C++ integration, help text in header, overflow checks, BUILD_BUG_ON,
 * more assertions, export helper functions as well, upper case hex chars, ...
 * Author: Richard James Howe */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PNUM_BUF_SIZE (64/*64 bit number in base 2*/ + 1/* for '+'/'-' */ + 1/*NUL terminator*/)

#define HEX_SEP_ADR ":\t"
#define HEX_SEP_EOL "\n"
#define HEX_SEP_CH2 "\t|"
#define HEX_SEP_CH1 "|"
#define HEX_SEP_BYT " "
#define HEX_NON_GRAPHIC_REPLACEMENT_CHAR ' '

#ifndef HEX_MAX_NCOLS
#define HEX_MAX_NCOLS (64)
#endif

#ifndef MIN
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y) ((X) < (Y) ? (Y) : (X))
#endif

typedef unsigned long unumber_t;
typedef long number_t;

#define UNUMBER_MAX (ULONG_MAX)

typedef struct {
	unsigned char *b;
	size_t used, length;
} buffer_t;

typedef struct {
	int (*get)(void *in);          /* return negative on error, a byte (0-255) otherwise */
	int (*put)(void *out, int ch); /* return ch on no error */
	void *in, *out;                /* passed to 'get' and 'put' respectively */
	size_t read, wrote;            /* read only, bytes 'get' and 'put' respectively */
	int error;                     /* an error has occurred */
} io_t; /* I/O abstraction, use to redirect to wherever you want... */

typedef struct {
	io_t io;                    /* Hexdump I/O abstraction layer */
	uint64_t address;           /* Address to print if enabled, auto-incremented */
	size_t buf_used;            /* Number of bytes in buf used */
	uint8_t buf[HEX_MAX_NCOLS]; /* Buffer used to store characters for `chars_on` */

        /* XXXX: XX XX XX XX |....| */
	char *sep_adr,              /* */
	     *sep_eol,              /* */
	     *sep_byt,              /* */
	     *sep_ch1,              /* */
	     *sep_ch2;              /* */

	bool chars_on,              /* Turn character printing on, implies newlines_on */
	     addresses_on,          /* Turn address printing on, implies newlines_on */
	     newlines_on;           /* Print a new line, columnizing output */

	int base,                   /* Base to print, if 0 auto-select, otherwise valid bases are between 2 and 36 */
	    ncols;                  /* Number of columns to print, must not exceed HEX_MAX_NCOLS, if 0 auto-select*/
} hexdump_t; /* Hexdump structure, for all your hex-dumping needs */

static const char *hex_string_digits = "0123456789abcdefghijklmnopqrstuvwxyz";

static bool hex_is_valid_base(number_t base) {
	return base >=2 && base <= 36;
}

static inline void hex_reverse(char * const r, const size_t length) {
	const size_t last = length - 1;
	for (size_t i = 0; i < length/2ul; i++) {
		const size_t t = r[i];
		r[i] = r[last - i];
		r[last - i] = t;
	}
}

static int hex_number_to_string(char buf[static 64/*base 2*/ + 1/*'+'/'-'*/ + 1/*NUL*/], number_t in, number_t base) {
	assert(buf);
	int negate = 0;
	size_t i = 0;
	unumber_t dv = in;
	if (!hex_is_valid_base(base))
		return -1;
	if (in < 0) {
		dv     = -(unumber_t)in;
		negate = 1;
	}
	do
		buf[i++] = hex_string_digits[dv % base];
	while ((dv /= base));
	if (negate)
		buf[i++] = '-';
	buf[i] = 0;
	hex_reverse(buf, i);
	return 0;
}

static int hex_unumber_to_string(char buf[static 64/*base 2*/ + 1/*'+'/'-'*/ + 1/*NUL*/], unumber_t in, unumber_t base) {
	assert(buf);
	size_t i = 0;
	unumber_t dv = in;
	if (!hex_is_valid_base(base))
		return -1;
	do
		buf[i++] = hex_string_digits[dv % base];
	while ((dv /= base));
	buf[i] = 0;
	hex_reverse(buf, i);
	return 0;
}

static int buffer_get(void *in) {
	buffer_t *b = in;
	assert(b);
	assert(b->b);
	if (b->used >= b->length)
		return -1;
	return b->b[b->used++];
}

static int buffer_put(const int ch, void *out) {
	buffer_t *b = out;
	assert(b);
	assert(b->b);
	if (b->used >= b->length)
		return -1;
	return b->b[b->used++] = ch;
}

static int file_get(void *in) {
	assert(in);
	return fgetc(in);
}

static int file_put(void *out, int ch) {
	assert(out);
	return fputc(ch, (FILE*)out);
}

static int get(io_t *io) {
	assert(io);
	if (io->error)
		return -1;
	const int r = io->get(io->in);
	assert(r <= 255);
	io->read += r >= 0;
	return r;
}

static int put(io_t *io, const int ch) {
	assert(io);
	if (io->error)
		return -1;
	const int r = io->put(io->out, ch);
	assert(r <= 255);
	io->wrote += r >= 0;
	if (r < 0)
		io->error = -1;
	return r;
}

static int hex_puts(io_t *io, const char *s) {
	for (int ch = 0; (ch = *s++);)
		if (put(io, ch) < 0)
			return -1;
	return 0;
}

static int print_number(io_t *io, unumber_t u, unumber_t base, int leading_zeros, int leading_char) {
	assert(io);
	for (int i = 0; i < leading_zeros; i++)
		if (put(io, leading_char) < 0)
			return -1;
	char buf[PNUM_BUF_SIZE] = { 0, };
	if (hex_unumber_to_string(buf, u, base) < 0)
		return -1;
	for (size_t i = 0; i < PNUM_BUF_SIZE && buf[i]; i++)
		if (put(io, buf[i]) < 0)
			return -1;
	return 0;
}

static int uilog(unumber_t n, unumber_t base) {
	int r = 0;
	do r++; while ((n /= base));
	return r;
}

static int aligned_print_number(io_t *io, unumber_t u, unumber_t base, unumber_t max, int leading_zeros, int leading_char) {
	assert(io);
	const int mint = uilog(max, base) - uilog(u, base);
	leading_zeros = MAX(0, leading_zeros);
	leading_zeros = MIN(mint, leading_zeros);
	return print_number(io, u, base, leading_zeros, leading_char);
}

static bool hex_newlines_enabled(hexdump_t *h) {
	if (h->newlines_on)
		return true;
	if (h->chars_on)
		return true;
	if (h->addresses_on)
		return true;
	return false;
}

static int newline(hexdump_t *h) {
	assert(h);
	if (hex_newlines_enabled(h))
		if (put(&h->io, '\n') < 0)
			return -1;
	return 0;
}

static int space(hexdump_t *h) {
	assert(h);
	if (put(&h->io, ' ') < 0)
		return -1;
	return 0;
}

static int spaces(hexdump_t *h, int spaces) {
	assert(h);
	spaces = MAX(spaces, 0);
	for (int i = 0; i < spaces; i++)
		if (space(h) < 0)
			return -1;
	return 0;
}

static int tab(hexdump_t *h) {
	assert(h);
	if (put(&h->io, '\t') < 0)
		return -1;
	return 0;
}

static int column(hexdump_t *h) {
	assert(h);
	if (put(&h->io, '|') < 0)
		return -1;
	return 0;
}

static int hex_isgraph(const int ch) { /* avoiding the use of locale dependent functions */
	return ch > 32 && ch < 127;
}

static int hex_pchar(hexdump_t *h, int ch) {
	assert(h);
	ch = hex_isgraph(ch) ? ch : HEX_NON_GRAPHIC_REPLACEMENT_CHAR;
	if (put(&h->io, ch) < 0)
		return -1;
	return 0;
}

static int hex_paddr(hexdump_t *h, unumber_t addr) {
	assert(h);
	if (!h->addresses_on)
		return 0;
	if (aligned_print_number(&h->io, addr, h->base, UNUMBER_MAX, 4, ' ') < 0)
		return -1;
	if (put(&h->io, ':') < 0)
		return -1;
	if (tab(h) < 0)
		return -1;
	return 0;
}

static int hexdump_validate(hexdump_t *h) {
	assert(h);
	if (h->io.error)
		return -1;
	if (!hex_is_valid_base(h->base))
		return -1;
	if (h->ncols < 1 || h->ncols > HEX_MAX_NCOLS)
		return -1;
	if (h->buf_used > sizeof (h->buf))
		return -1;
	return 0;
}

static int hexdump_config_default(hexdump_t *h) {
	assert(h);
	h->base  = h->base  ? h->base  : 16 ;
	h->ncols = h->ncols ? h->ncols : 16 ;

	h->sep_adr = h->sep_adr ? h->sep_adr : HEX_SEP_ADR;
	h->sep_eol = h->sep_eol ? h->sep_eol : HEX_SEP_EOL;
	h->sep_ch1 = h->sep_ch1 ? h->sep_ch1 : HEX_SEP_CH1;
	h->sep_ch2 = h->sep_ch2 ? h->sep_ch2 : HEX_SEP_CH2 ;
	h->sep_byt = h->sep_byt ? h->sep_byt : HEX_SEP_BYT ;

	return hexdump_validate(h);
}

static int hexdump(hexdump_t *h) {
	assert(h);
	io_t *io = &h->io;
	if (hexdump_config_default(h) < 0)
		return -1;

	const int byte_align = 2; // TODO: BUG: Dependent on base!

	for (int end = 0; !end;) {
		h->buf_used = 0;
		for (int i = 0; i < h->ncols; i++) {
			const int ch = get(io);
			if (ch < 0)
				end = 1;
			if (end && !i)
				goto done;
			if (end)
				break;
			h->buf[h->buf_used++] = ch;
		}
		if (hex_paddr(h, h->address) < 0)
			return -1;
		for (size_t i = 0; i < h->buf_used; i++) {
			if (aligned_print_number(io, h->buf[i], h->base, 255, byte_align, '0') < 0)
				return -1;
			if (space(h) < 0)
				return -1;
		}
		if (h->chars_on) {
			int missing = h->ncols - h->buf_used;
			if (spaces(h, (byte_align + 1) * missing) < 0)
				return -1;
			if (tab(h) < 0)
				return -1;
			if (column(h) < 0)
				return -1;
			for (size_t i = 0; i < h->buf_used; i++)
				if (hex_pchar(h, h->buf[i]) < 0)
					return -1;
			if (spaces(h, missing) < 0)
				return -1;
			if (column(h) < 0)
				return -1;
		}
		if (newline(h) < 0)
			return -1;
		if ((h->address + h->buf_used) <= h->address)
			goto fail;
		h->address += h->buf_used;
		h->buf_used = 0;
	}
	if (newline(h) < 0)
		return -1;
done:
	return 0;
fail:
	io->error = -1;
	return -1;
}

typedef struct {
	char *arg;   /* parsed argument */
	int error,   /* turn error reporting on/off */
	    index,   /* index into argument list */
	    option,  /* parsed option */
	    reset;   /* set to reset */
	char *place; /* internal use: scanner position */
	int  init;   /* internal use: initialized or not */
} hex_getopt_t;      /* getopt clone; with a few modifications */

/* Adapted from: <https://stackoverflow.com/questions/10404448>, this
 * could be extended to parse out numeric values, and do other things, but
 * that is not needed here. The function and structure should be turned
 * into a header only library. */
static int hex_getopt(hex_getopt_t *opt, const int argc, char *const argv[], const char *fmt) {
	assert(opt);
	assert(fmt);
	assert(argv);
	enum { BADARG_E = ':', BADCH_E = '?', BADIO_E = '!', };

	if (!(opt->init)) {
		opt->place = ""; /* option letter processing */
		opt->init  = 1;
		opt->index = 1;
	}

	if (opt->reset || !*opt->place) { /* update scanning pointer */
		opt->reset = 0;
		if (opt->index >= argc || *(opt->place = argv[opt->index]) != '-') {
			opt->place = "";
			return -1;
		}
		if (opt->place[1] && *++opt->place == '-') { /* found "--" */
			opt->index++;
			opt->place = "";
			return -1;
		}
	}

	const char *oli = NULL; /* option letter list index */
	if ((opt->option = *opt->place++) == ':' || !(oli = strchr(fmt, opt->option))) { /* option letter okay? */
		 /* if the user didn't specify '-' as an option, assume it means -1.  */
		if (opt->option == '-')
			return -1;
		if (!*opt->place)
			opt->index++;
		if (opt->error && *fmt != ':')
			if (fprintf(stderr, "illegal option -- %c\n", opt->option) < 0)
				return BADIO_E;
		return BADCH_E;
	}

	if (*++oli != ':') { /* don't need argument */
		opt->arg = NULL;
		if (!*opt->place)
			opt->index++;
	} else {  /* need an argument */
		if (*opt->place) { /* no white space */
			opt->arg = opt->place;
		} else if (argc <= ++opt->index) { /* no arg */
			opt->place = "";
			if (*fmt == ':')
				return BADARG_E;
			if (opt->error)
				if (fprintf(stderr, "option requires an argument -- %c\n", opt->option) < 0)
					return BADIO_E;
			return BADCH_E;
		} else	{ /* white space */
			opt->arg = argv[opt->index];
		}
		opt->place = "";
		opt->index++;
	}
	return opt->option; /* dump back option letter */
}

int main(int argc, char **argv) {
	hexdump_t h = {
		.io = { .get = file_get, .put = file_put, .out = stdout, .in = NULL, }, 
		// TODO: Change to _off, so defaults are better
		.addresses_on = true,
		.chars_on = true,
		.newlines_on = true,
	};
	for (int i = 1; i < argc; i++) {
		errno = 0;
		FILE *f = fopen(argv[i], "rb");
		if (!f) {
			(void)fprintf(stderr, "Cannot open file %s (mode %s):%s\n", argv[i], "rb", strerror(errno));
			return 1;
		}
		h.io.in = f;
		const int r = hexdump(&h);
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
