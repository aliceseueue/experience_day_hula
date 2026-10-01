#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <errno.h>
#include "proto.h"

#define USER DT_PATH(zephyr_user)

/* SERIN, CLK and LE of the daisy-chained HMC1122 pair. */
static const struct gpio_dt_spec clk  = GPIO_DT_SPEC_GET(USER, dsa_clk_gpios);
static const struct gpio_dt_spec data = GPIO_DT_SPEC_GET(USER, dsa_data_gpios);
static const struct gpio_dt_spec le   = GPIO_DT_SPEC_GET(USER, dsa_le_gpios);

/* HMC1122: 6-bit, 0.5 dB LSB, 0..31.5 dB. Two cascaded parts give 0..63 dB,
 * so the programmable range is 0..126 half-dB. U1 is a fixed attenuator and
 * is not represented here. */
#define DSA_CHIP_MAX_HALF_DB 63

BUILD_ASSERT(ATT_MAX_HALF_DB == 2 * DSA_CHIP_MAX_HALF_DB,
	     "ATT_MAX_HALF_DB must be two cascaded HMC1122 ranges");

/* Boot state is maximum attenuation. Set explicitly rather than relying on the
 * initialiser so that a failed init leaves a known, safe cached value. */
static int cur_half_db = ATT_MAX_HALF_DB;

/* True once every pin is configured and the shift path has been exercised. */
static bool dsa_ready;

/* Shift one 6-bit code MSB first on the rising edge of CLK.
 *
 * SEROUT is delayed six clock cycles, so a two-chip daisy chain shifted as
 * shift6(u3); shift6(u2) leaves u3 in the first chip and u2 in the second.
 * LE must be low for the whole shift or the part masks CLK and ignores it. */
static void shift6(uint8_t v)
{
	for (int i = 5; i >= 0; i--) {
		gpio_pin_set_dt(&data, (v >> i) & 1U);
		/* Data setup time before the rising edge. */
		k_busy_wait(1);
		gpio_pin_set_dt(&clk, 1);
		k_busy_wait(1);
		gpio_pin_set_dt(&clk, 0);
	}
}

int dsa_set_half_db(int h)
{
	int u3;
	int u2;

	if (h < 0 || h > ATT_MAX_HALF_DB) {
		return -EINVAL;
	}
	if (!dsa_ready) {
		/* Pins are not configured. Shifting now would drive floating
		 * levels into the parts, so refuse rather than write garbage. */
		return -ENODEV;
	}

	/* Fill the first chip to its ceiling before spending the second's range,
	 * so the split stays monotonic and u2 never exceeds the chip range. */
	u3 = MIN(h, DSA_CHIP_MAX_HALF_DB);
	u2 = h - u3;

	gpio_pin_set_dt(&le, 0);
	shift6((uint8_t)u3);
	shift6((uint8_t)u2);
	/* Latch is active high and must be toggled, not held. */
	gpio_pin_set_dt(&le, 1);
	k_busy_wait(1);
	gpio_pin_set_dt(&le, 0);

	/* RF settling time is 250 ns per the datasheet. */
	k_busy_wait(1);

	cur_half_db = h;
	return 0;
}

int dsa_get_half_db(void)
{
	return cur_half_db;
}

int dsa_init(void)
{
	int err;

	if (!gpio_is_ready_dt(&clk) || !gpio_is_ready_dt(&data) || !gpio_is_ready_dt(&le)) {
		return -ENODEV;
	}

	/* All three idle low. CLK low and LE low is the only state in which the
	 * HMC1122 will accept shifted data, so boot must not pulse LE. */
	err = gpio_pin_configure_dt(&clk, GPIO_OUTPUT_INACTIVE);
	if (err) {
		return err;
	}
	err = gpio_pin_configure_dt(&data, GPIO_OUTPUT_INACTIVE);
	if (err) {
		return err;
	}
	err = gpio_pin_configure_dt(&le, GPIO_OUTPUT_INACTIVE);
	if (err) {
		return err;
	}

	/* Pins are good, so the shift path is safe to use. */
	dsa_ready = true;

	/* Safe state at boot: maximum attenuation. On failure clear dsa_ready so
	 * the caller reports the error rather than a cached value that the
	 * hardware never received. */
	err = dsa_set_half_db(ATT_MAX_HALF_DB);
	if (err) {
		dsa_ready = false;
		cur_half_db = ATT_MAX_HALF_DB;
		return err;
	}

	return 0;
}

bool dsa_is_ready(void)
{
	return dsa_ready;
}
