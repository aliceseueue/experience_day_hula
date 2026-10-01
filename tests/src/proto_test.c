/*
 * Copyright (c) 2024 rf-atten
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <string.h>
#include "proto.h"

/* Stubs for the DSA driver so the parser and dispatcher can be tested
 * without GPIO hardware. The driver itself is exercised on real hardware. */
static int stub_half = ATT_MAX_HALF_DB;
static bool stub_ready = true;
static int stub_fail_set;

int dsa_set_half_db(int half_db)
{
	if (stub_fail_set) {
		return -1;
	}
	stub_half = half_db;
	return 0;
}

int dsa_get_half_db(void)
{
	return stub_half;
}

int dsa_init(void)
{
	return 0;
}

bool dsa_is_ready(void)
{
	return stub_ready;
}

static void setup(void)
{
	stub_half = ATT_MAX_HALF_DB;
	stub_ready = true;
	stub_fail_set = 0;
}

/* Run one line through the dispatcher and return the reply. */
static const char *run(const char *input)
{
	static char line[64];
	static char out[64];

	strcpy(line, input);
	proto_handle_line(line, out, sizeof(out));
	return out;
}

ZTEST(proto, test_idn)
{
	ZTEST_ASSERT_STR_EQ(run("*IDN?"), "OK rf-atten fw 0.1.0");
}

ZTEST(proto, test_get_db_formats_half_step)
{
	stub_half = 5;
	ZTEST_ASSERT_STR_EQ(run("ATT?"), "OK 2.5");

	stub_half = 0;
	ZTEST_ASSERT_STR_EQ(run("ATT?"), "OK 0.0");

	stub_half = ATT_MAX_HALF_DB;
	ZTEST_ASSERT_STR_EQ(run("ATT?"), "OK 63.0");
}

ZTEST(proto, test_set_accepts_documented_forms)
{
	ZTEST_ASSERT_STR_EQ(run("ATT 20"), "OK");
	ZTEST_ASSERT_STR_EQ(run("ATT 20.0"), "OK");
	ZTEST_ASSERT_STR_EQ(run("ATT 20.5"), "OK");
	ZTEST_ASSERT_STR_EQ(run("ATT 0.5"), "OK");
	ZTEST_ASSERT_STR_EQ(run("ATT 63"), "OK");
}

ZTEST(proto, test_set_stores_half_db)
{
	ZTEST_ASSERT_STR_EQ(run("ATT 20.5"), "OK");
	ZTEST_ASSERT_STR_EQ(stub_half, 41);

	ZTEST_ASSERT_STR_EQ(run("ATT 63"), "OK");
	ZTEST_ASSERT_STR_EQ(stub_half, ATT_MAX_HALF_DB);
}

ZTEST(proto, test_range_errors)
{
	ZTEST_ASSERT_STR_EQ(run("ATT 64"), "ERR range");
	ZTEST_ASSERT_STR_EQ(run("ATT 63.5"), "ERR range");
	ZTEST_ASSERT_STR_EQ(run("ATT 999"), "ERR range");
	ZTEST_ASSERT_STR_EQ(run("ATT 1000"), "ERR range");
}

ZTEST(proto, test_step_errors)
{
	ZTEST_ASSERT_STR_EQ(run("ATT 20.3"), "ERR step");
	ZTEST_ASSERT_STR_EQ(run("ATT 0.2"), "ERR step");
}

ZTEST(proto, test_syntax_errors)
{
	/* A leading sign is malformed, not merely out of range. */
	ZTEST_ASSERT_STR_EQ(run("ATT -1"), "ERR syntax");
	ZTEST_ASSERT_STR_EQ(run("ATT +1"), "ERR syntax");
	ZTEST_ASSERT_STR_EQ(run("ATT abc"), "ERR syntax");
	ZTEST_ASSERT_STR_EQ(run("ATT 1.2.3"), "ERR syntax");
	ZTEST_ASSERT_STR_EQ(run("ATT 20x"), "ERR syntax");
	ZTEST_ASSERT_STR_EQ(run("ATT 20."), "ERR syntax");
	ZTEST_ASSERT_STR_EQ(run("ATT"), "ERR syntax");
}

ZTEST(proto, test_range_error_leaves_state_alone)
{
	ZTEST_ASSERT_STR_EQ(run("ATT 10"), "OK");
	ZTEST_ASSERT_STR_EQ(run("ATT 99"), "ERR range");
	ZTEST_ASSERT_STR_EQ(stub_half, 20);
}

ZTEST(proto, test_unknown_command)
{
	ZTEST_ASSERT_STR_EQ(run("FOO"), "ERR unknown_cmd");
	ZTEST_ASSERT_STR_EQ(run("AT 10"), "ERR unknown_cmd");
	/* Bare "ATT?" is a query, but "ATTX" is not a command. */
	ZTEST_ASSERT_STR_EQ(run("ATTX 10"), "ERR unknown_cmd");
}

ZTEST(proto, test_surrounding_whitespace_is_tolerated)
{
	ZTEST_ASSERT_STR_EQ(run("  ATT 20  "), "OK");
	ZTEST_ASSERT_STR_EQ(run("\tATT?\t"), "OK 63.0");
	ZTEST_ASSERT_STR_EQ(run(" ATT 20 \r"), "OK");
	ZTEST_ASSERT_STR_EQ(run("  *IDN?  "), "OK rf-atten fw 0.1.0");
}

ZTEST(proto, test_hardware_write_failure_reported)
{
	stub_fail_set = 1;
	ZTEST_ASSERT_STR_EQ(run("ATT 20"), "ERR hw");
}

ZTEST(proto, test_get_db_reports_hw_error_when_not_ready)
{
	stub_ready = false;
	/* Must not claim a cached attenuation the hardware never received. */
	ZTEST_ASSERT_STR_EQ(run("ATT?"), "ERR hw");

	/* Setting is still accepted by the parser, but the write fails. */
	ZTEST_ASSERT_STR_EQ(run("ATT 20"), "ERR hw");
}

ZTEST(proto, test_reply_is_truncated_to_buffer)
{
	static char out[8];

	char line[64] = "*IDN?";

	proto_handle_line(line, out, sizeof(out));
	/* Must be NUL terminated and never exceed the buffer. */
	ZTEST_ASSERT(strlen(out) < sizeof(out), "reply not truncated");
}

ZTEST_SUITE(proto, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE_MAIN(proto);
