#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "proto.h"

#define FW_VERSION "0.1.0"

/* Accepts "N", "N.0" or "N.5". Returns 0 on success, or a negative code:
 * -1 syntax, -2 range, -3 step. */
static int parse_half_db(const char *s, int *out)
{
	int whole = 0;
	int digits = 0;
	int half;

	if (*s == '-') {
		/* A sign is not a valid attenuation, so this is malformed input
		 * rather than a value outside the range. */
		return -1;
	}

	while (*s >= '0' && *s <= '9') {
		if (++digits > 3) {
			/* Too many digits to ever be in range. */
			return -2;
		}
		whole = whole * 10 + (*s++ - '0');
	}

	if (digits == 0) {
		return -1;
	}

	half = whole * 2;

	if (*s == '.') {
		s++;
		if (*s == '5') {
			half += 1;
		} else if (*s == '0') {
			/* No change. */
		} else if (*s >= '1' && *s <= '9') {
			/* Hardware has no finer than 0.5 dB resolution. */
			return -3;
		} else {
			return -1;
		}
		s++;
	}

	if (*s != '\0') {
		return -1;
	}

	if (half > ATT_MAX_HALF_DB) {
		return -2;
	}

	*out = half;
	return 0;
}

static void reply(char *out, size_t n, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

static void reply(char *out, size_t n, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(out, n, fmt, ap);
	va_end(ap);
}

void proto_handle_line(char *line, char *out, size_t n)
{
	size_t len = strlen(line);

	/* Strip trailing padding. The host writes "\n" but a tty in raw mode or
	 * a CRLF-terminating client both arrive here. */
	while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' ||
			   line[len - 1] == '\r')) {
		line[--len] = '\0';
	}

	/* Strip leading padding so "  ATT 20" and "ATT 20" behave the same. */
	while (*line == ' ' || *line == '\t') {
		line++;
	}

	if (strcmp(line, "*IDN?") == 0) {
		reply(out, n, "OK rf-atten fw " FW_VERSION);
		return;
	}

	if (strcmp(line, "ATT?") == 0) {
		int h;

		if (!dsa_is_ready()) {
			/* Do not report a cached value the hardware never got.
			 * An RF path believed to be attenuated but is not is the
			 * dangerous direction to fail. */
			reply(out, n, "ERR hw");
			return;
		}

		h = dsa_get_half_db();
		reply(out, n, "OK %d.%d", h / 2, (h % 2) * 5);
		return;
	}

	/* "ATT" with no argument is malformed, not an unknown command. */
	if (strcmp(line, "ATT") == 0) {
		reply(out, n, "ERR syntax");
		return;
	}

	if (strncmp(line, "ATT ", 4) == 0) {
		int h;
		int rc = parse_half_db(line + 4, &h);

		if (rc == 0) {
			rc = dsa_set_half_db(h) ? -4 : 0;
		}

		switch (rc) {
		case 0:
			reply(out, n, "OK");
			break;
		case -1:
			reply(out, n, "ERR syntax");
			break;
		case -2:
			reply(out, n, "ERR range");
			break;
		case -3:
			reply(out, n, "ERR step");
			break;
		default:
			reply(out, n, "ERR hw");
			break;
		}
		return;
	}

	reply(out, n, "ERR unknown_cmd");
}
