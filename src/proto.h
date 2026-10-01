#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Two cascaded HMC1122 attenuators: 0..31.5 dB each in 0.5 dB steps.
 * All internal values are in half-dB, so 0..63 dB is 0..126. */
#define ATT_MAX_HALF_DB 126

int dsa_set_half_db(int half_db);
int dsa_get_half_db(void);
int dsa_init(void);

/* False if dsa_init() failed. dsa_get_half_db() is still safe to call and
 * returns ATT_MAX_HALF_DB, but the hardware did not receive that value. */
bool dsa_is_ready(void);

/* Consumes and mutates line (trailing whitespace is stripped). Writes a
 * NUL-terminated reply into out, truncated to out_len. */
void proto_handle_line(char *line, char *out, size_t out_len);
