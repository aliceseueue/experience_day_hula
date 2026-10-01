#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <stdbool.h>
#include <stdint.h>
#include "proto.h"

/* Longest accepted input line is sizeof(line) - 1 characters plus the NUL.
 * The longest reply is "OK rf-atten fw 0.1.0" at 22 bytes. */
#define LINE_BUF_SIZE 48
#define OUT_BUF_SIZE 48

/* Bounds the length of a single command. Anything longer is rejected with
 * ERR too_long and discarded rather than executed. */
#define ATT_LINE_MAX (LINE_BUF_SIZE - 1)

static void send(const struct device *u, const char *s)
{
	for (; *s != '\0'; s++) {
		uart_poll_out(u, *s);
	}
	uart_poll_out(u, '\n');
}

int main(void)
{
	const struct device *uart = DEVICE_DTS_GET_ONE(zephyr_cdc_acm_uart);
	char line[LINE_BUF_SIZE];
	char out[OUT_BUF_SIZE];
	size_t len = 0;
	bool overflow = false;
	bool dsa_ok;
	int err;
	uint8_t c;

	err = dsa_init();
	dsa_ok = (err == 0);

	/* USB is already enabled by the CDC ACM stack at boot
	 * (CONFIG_CDC_ACM_SERIAL_INITIALIZE_AT_BOOT). */

	if (!device_is_ready(uart)) {
		/* Nothing to report on: the transport itself is gone. Spin so
		 * the fault handler does not write into a dead link. */
		while (1) {
			k_msleep(1000);
		}
	}

	/* Acknowledge init failure once the link is up, so a host connecting
	 * after boot still learns the attenuators are not under control. */
	if (!dsa_ok) {
		send(uart, "ERR hw");
	}

	while (1) {
		if (uart_poll_in(uart, &c) != 0) {
			/* No data. uart_poll_in also reports errors here (for
			 * example while the host has not opened the port yet),
			 * which is the same "retry" case. */
			k_msleep(1);
			continue;
		}

		if (c == '\n' || c == '\r') {
			if (overflow) {
				send(uart, "ERR too_long");
			} else if (len > 0) {
				line[len] = '\0';
				proto_handle_line(line, out, sizeof(out));
				send(uart, out);
			}
			len = 0;
			overflow = false;
			continue;
		}

		if (len < ATT_LINE_MAX) {
			line[len++] = (char)c;
		} else {
			/* Keep consuming until end of line so the tail of an
			 * over-long command cannot be read as a new command. */
			overflow = true;
		}
	}
}