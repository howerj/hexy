/* Project: Generic Hexdump routines
 * Missing: Grouping bytes and endianess conversion, turn into library, 
 * signed printing, header-only library, unit tests, CLI options,
 * customizable separators strings (not chars, NULL = default), colors, undump,
 * fix bugs, prefix with hexy_, left/right align, turn off FILE* support,
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

#define HEXY_PNUM_BUF_SIZE (64/*64 bit number in base 2*/ + 1/* for '+'/'-' */ + 1/*NUL terminator*/)

#define HEXY_SEP_ADR ":\t"
#define HEXY_SEP_EOL "\n"
#define HEXY_SEP_CH1 "\t|"
#define HEXY_SEP_CH2 "|"
#define HEXY_SEP_BYT " "
#define HEXY_NON_GRAPHIC_REPLACEMENT_CHAR ' '

#ifndef HEXY_MAX_NCOLS
#define HEXY_MAX_NCOLS (64)
#endif

#ifndef MIN
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y) ((X) < (Y) ? (Y) : (X))
#endif

typedef unsigned long hexy_unum_t;

#define HEXY_UNUM_MAX (ULONG_MAX)

typedef struct {
	unsigned char *b;
	size_t used, length;
} hexy_buffer_t;

typedef struct {
	int (*get)(void *in);          /* return negative on error, a byte (0-255) otherwise */
	int (*put)(void *out, int ch); /* return ch on no error */
	void *in, *out;                /* passed to 'get' and 'put' respectively */
	size_t read, wrote;            /* read only, bytes 'get' and 'put' respectively */
	int error;                     /* an error has occurred */
} hexy_io_t; /* I/O abstraction, use to redirect to wherever you want... */

typedef struct {
	hexy_io_t io;                    /* Hexdump I/O abstraction layer */
	uint64_t address;           /* Address to print if enabled, auto-incremented */
	size_t buf_used;            /* Number of bytes in buf used */
	uint8_t buf[HEXY_MAX_NCOLS]; /* Buffer used to store characters for `chars_on` */

        /* XXXX: XX XX XX XX |....| */
	char *sep_adr,              /* */
	     *sep_eol,              /* */
	     *sep_byt,              /* */
	     *sep_ch1,              /* */
	     *sep_ch2;              /* */

	bool chars_off,             /* Turn off: character printing on, implies newlines_on */
	     addresses_off,         /* Turn off: address printing, implies newlines_on */
	     newlines_off;          /* Turn off: Print a new line, columnizing output */

	int base,                   /* Base to print, if 0 auto-select, otherwise valid bases are between 2 and 36 */
	    ncols;                  /* Number of columns to print, must not exceed HEXY_MAX_NCOLS, if 0 auto-select*/
} hexy_t; /* Hexdump structure, for all your hex-dumping needs */

static const char *hexy_string_digits = "0123456789abcdefghijklmnopqrstuvwxyz";

static bool hexy_is_valid_base(hexy_unum_t base) {
	return base >=2 && base <= 36;
}

static inline void hexy_reverse(char * const r, const size_t length) {
	const size_t last = length - 1;
	for (size_t i = 0; i < length/2ul; i++) {
		const size_t t = r[i];
		r[i] = r[last - i];
		r[last - i] = t;
	}
}

static int hexy_hexy_unum_to_string(char buf[static 64/*base 2*/ + 1/*'+'/'-'*/ + 1/*NUL*/], hexy_unum_t in, hexy_unum_t base) {
	assert(buf);
	size_t i = 0;
	hexy_unum_t dv = in;
	if (!hexy_is_valid_base(base))
		return -1;
	do
		buf[i++] = hexy_string_digits[dv % base];
	while ((dv /= base));
	buf[i] = 0;
	hexy_reverse(buf, i);
	return 0;
}

static int hexy_buffer_get(void *in) {
	hexy_buffer_t *b = in;
	assert(b);
	assert(b->b);
	if (b->used >= b->length)
		return -1;
	return b->b[b->used++];
}

static int hexy_buffer_put(const int ch, void *out) {
	hexy_buffer_t *b = out;
	assert(b);
	assert(b->b);
	if (b->used >= b->length)
		return -1;
	return b->b[b->used++] = ch;
}

static int hexy_file_get(void *in) {
	assert(in);
	return fgetc(in);
}

static int hexy_file_put(void *out, int ch) {
	assert(out);
	return fputc(ch, (FILE*)out);
}

static int hexy_get(hexy_io_t *io) {
	assert(io);
	if (io->error)
		return -1;
	const int r = io->get(io->in);
	assert(r <= 255);
	io->read += r >= 0;
	return r;
}

static int hexy_put(hexy_io_t *io, const int ch) {
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

static int hexy_puts(hexy_io_t *io, const char *s) {
	for (int ch = 0; (ch = *s++);)
		if (hexy_put(io, ch) < 0)
			return -1;
	return 0;
}

static int hexy_print_number(hexy_io_t *io, hexy_unum_t u, hexy_unum_t base, int leading_zeros, int leading_char) {
	assert(io);
	for (int i = 0; i < leading_zeros; i++)
		if (hexy_put(io, leading_char) < 0)
			return -1;
	char buf[HEXY_PNUM_BUF_SIZE] = { 0, };
	if (hexy_hexy_unum_to_string(buf, u, base) < 0)
		return -1;
	for (size_t i = 0; i < HEXY_PNUM_BUF_SIZE && buf[i]; i++)
		if (hexy_put(io, buf[i]) < 0)
			return -1;
	return 0;
}

static int hexy_uilog(hexy_unum_t n, hexy_unum_t base) {
	int r = 0;
	do r++; while ((n /= base));
	return r;
}

static int hexy_aligned_print_number(hexy_io_t *io, hexy_unum_t u, hexy_unum_t base, hexy_unum_t max, int leading_zeros, int leading_char) {
	assert(io);
	const int mint = hexy_uilog(max, base) - hexy_uilog(u, base);
	leading_zeros = MAX(0, leading_zeros);
	leading_zeros = MIN(mint, leading_zeros);
	return hexy_print_number(io, u, base, leading_zeros, leading_char);
}

static bool hexy_newlines_enabled(hexy_t *h) {
	if (!h->newlines_off)
		return true;
	if (!h->chars_off)
		return true;
	if (!h->addresses_off)
		return true;
	return false;
}

static int hexy_newline(hexy_t *h) {
	assert(h);
	if (hexy_newlines_enabled(h))
		if (hexy_puts(&h->io, h->sep_eol) < 0)
			return -1;
	return 0;
}

static int hexy_space(hexy_t *h) {
	assert(h);
	if (hexy_put(&h->io, ' ') < 0)
		return -1;
	return 0;
}

static int hexy_spaces(hexy_t *h, int spaces) {
	assert(h);
	spaces = MAX(spaces, 0);
	for (int i = 0; i < spaces; i++)
		if (hexy_space(h) < 0)
			return -1;
	return 0;
}

static int hexy_isgraph(const int ch) { /* avoiding the use of locale dependent functions */
	return ch > 32 && ch < 127;
}

static int hexy_pchar(hexy_t *h, int ch) {
	assert(h);
	ch = hexy_isgraph(ch) ? ch : HEXY_NON_GRAPHIC_REPLACEMENT_CHAR;
	if (hexy_put(&h->io, ch) < 0)
		return -1;
	return 0;
}

static int hexy_paddr(hexy_t *h, hexy_unum_t addr) {
	assert(h);
	if (h->addresses_off)
		return 0;
	if (hexy_aligned_print_number(&h->io, addr, h->base, HEXY_UNUM_MAX, 4, ' ') < 0)
		return -1;
	if (hexy_puts(&h->io, h->sep_adr) < 0)
		return -1;
	return 0;
}

static int hexy_validate(hexy_t *h) {
	assert(h);
	if (h->io.error)
		return -1;
	if (!hexy_is_valid_base(h->base))
		return -1;
	if (h->ncols < 1 || h->ncols > HEXY_MAX_NCOLS)
		return -1;
	if (h->buf_used > sizeof (h->buf))
		return -1;
	return 0;
}

static int hexy_config_default(hexy_t *h) {
	assert(h);
	h->base  = h->base  ? h->base  : 16 ;
	h->ncols = h->ncols ? h->ncols : 16 ;

	h->sep_adr = h->sep_adr ? h->sep_adr : HEXY_SEP_ADR;
	h->sep_eol = h->sep_eol ? h->sep_eol : HEXY_SEP_EOL;
	h->sep_ch1 = h->sep_ch1 ? h->sep_ch1 : HEXY_SEP_CH1;
	h->sep_ch2 = h->sep_ch2 ? h->sep_ch2 : HEXY_SEP_CH2 ;
	h->sep_byt = h->sep_byt ? h->sep_byt : HEXY_SEP_BYT ;

	return hexy_validate(h);
}

static int hexy(hexy_t *h) {
	assert(h);
	hexy_io_t *io = &h->io;
	if (hexy_config_default(h) < 0)
		return -1;

	const int byte_align = 2; // TODO: BUG: Dependent on base!

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
		if (hexy_paddr(h, h->address) < 0)
			return -1;
		for (size_t i = 0; i < h->buf_used; i++) {
			if (hexy_aligned_print_number(io, h->buf[i], h->base, 255, byte_align, '0') < 0)
				return -1;
			if (hexy_puts(io, h->sep_byt) < 0)
				return -1;
		}
		if (!h->chars_off) {
			const int missing = h->ncols - h->buf_used;
			const int bseplen = strlen(h->sep_byt);
			if (hexy_spaces(h, (byte_align + bseplen) * missing) < 0) /* TODO: Use h->sep_byt? */
				return -1;
			if (hexy_puts(io, h->sep_ch1) < 0)
				return -1;
			for (size_t i = 0; i < h->buf_used; i++)
				if (hexy_pchar(h, h->buf[i]) < 0)
					return -1;
			if (hexy_puts(io, h->sep_ch2) < 0)
				return -1;
		}
		if (hexy_newline(h) < 0)
			return -1;
		if ((h->address + h->buf_used) <= h->address)
			goto fail;
		h->address += h->buf_used;
		h->buf_used = 0;
	}
	if (hexy_newline(h) < 0)
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
} hexy_getopt_t;     /* getopt clone; with a few modifications */

/* Adapted from: <https://stackoverflow.com/questions/10404448>, this
 * could be extended to parse out numeric values, and do other things, but
 * that is not needed here. The function and structure should be turned
 * into a header only library. */
static int hexy_getopt(hexy_getopt_t *opt, const int argc, char *const argv[], const char *fmt) {
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
	hexy_t h = {
		.io = { .get = hexy_file_get, .put = hexy_file_put, .out = stdout, .in = NULL, }, 
	};
	for (int i = 1; i < argc; i++) {
		errno = 0;
		FILE *f = fopen(argv[i], "rb");
		if (!f) {
			(void)fprintf(stderr, "Cannot open file %s (mode %s):%s\n", argv[i], "rb", strerror(errno));
			return 1;
		}
		h.io.in = f;
		const int r = hexy(&h);
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

