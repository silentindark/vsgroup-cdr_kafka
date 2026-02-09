/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright 2026 VSGroup (Virtual Sistemas e Tecnologia Ltda)  (see the AUTHORS file)
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Tests for cdr_kafka
 *
 * \author VSGroup
 */

/*** MODULEINFO
	<depend>TEST_FRAMEWORK</depend>
	<depend>cdr_kafka</depend>
	<depend>res_kafka</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/test.h"
#include "asterisk/cdr.h"
#include "asterisk/json.h"
#include "asterisk/kafka.h"
#include "asterisk/utils.h"
#include "asterisk/strings.h"

#define TEST_CATEGORY "/cdr/kafka/"

/*! \brief Imported from cdr_kafka.c */
extern const char *cdr_get_key_value(struct ast_cdr *cdr,
	const char *field_name);

/*!
 * \brief Build a fake CDR record with known values for testing.
 *
 * All string fields are populated with deterministic test values.
 * The caller must pass a pointer to a zero-initialized ast_cdr.
 */
static void build_test_cdr(struct ast_cdr *cdr)
{
	memset(cdr, 0, sizeof(*cdr));

	ast_copy_string(cdr->clid, "\"Test User\" <1001>", sizeof(cdr->clid));
	ast_copy_string(cdr->src, "1001", sizeof(cdr->src));
	ast_copy_string(cdr->dst, "2001", sizeof(cdr->dst));
	ast_copy_string(cdr->dcontext, "from-internal", sizeof(cdr->dcontext));
	ast_copy_string(cdr->channel, "PJSIP/1001-00000001", sizeof(cdr->channel));
	ast_copy_string(cdr->dstchannel, "PJSIP/2001-00000002", sizeof(cdr->dstchannel));
	ast_copy_string(cdr->lastapp, "Dial", sizeof(cdr->lastapp));
	ast_copy_string(cdr->lastdata, "PJSIP/2001,30", sizeof(cdr->lastdata));
	ast_copy_string(cdr->accountcode, "acct-100", sizeof(cdr->accountcode));
	ast_copy_string(cdr->peeraccount, "acct-200", sizeof(cdr->peeraccount));
	ast_copy_string(cdr->uniqueid, "1700000000.1", sizeof(cdr->uniqueid));
	ast_copy_string(cdr->linkedid, "1700000000.1", sizeof(cdr->linkedid));
	ast_copy_string(cdr->userfield, "custom-data", sizeof(cdr->userfield));
	ast_copy_string(cdr->tenantid, "tenant-01", sizeof(cdr->tenantid));

	cdr->disposition = AST_CDR_ANSWERED;
	cdr->amaflags = AST_AMA_DOCUMENTATION;
	cdr->duration = 120;
	cdr->billsec = 115;
	cdr->sequence = 1;
}

/* ---- Key extraction tests ---- */

AST_TEST_DEFINE(key_linkedid)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_linkedid";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for linkedid field";
		info->description =
			"Verifies cdr_get_key_value() returns the correct value "
			"for the linkedid field.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "linkedid");
	if (!val || strcmp(val, "1700000000.1") != 0) {
		ast_test_status_update(test,
			"Expected '1700000000.1', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_uniqueid)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_uniqueid";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for uniqueid field";
		info->description =
			"Verifies cdr_get_key_value() returns the correct value "
			"for the uniqueid field.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "uniqueid");
	if (!val || strcmp(val, "1700000000.1") != 0) {
		ast_test_status_update(test,
			"Expected '1700000000.1', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_channel)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_channel";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for channel field";
		info->description =
			"Verifies cdr_get_key_value() returns the correct value "
			"for the channel field.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "channel");
	if (!val || strcmp(val, "PJSIP/1001-00000001") != 0) {
		ast_test_status_update(test,
			"Expected 'PJSIP/1001-00000001', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_src_dst)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_src_dst";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for src and dst fields";
		info->description =
			"Verifies cdr_get_key_value() returns correct values "
			"for src and dst fields.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "src");
	if (!val || strcmp(val, "1001") != 0) {
		ast_test_status_update(test,
			"src: expected '1001', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	val = cdr_get_key_value(&cdr, "dst");
	if (!val || strcmp(val, "2001") != 0) {
		ast_test_status_update(test,
			"dst: expected '2001', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_accountcode)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_accountcode";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for accountcode field";
		info->description =
			"Verifies cdr_get_key_value() returns the correct value "
			"for the accountcode field.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "accountcode");
	if (!val || strcmp(val, "acct-100") != 0) {
		ast_test_status_update(test,
			"Expected 'acct-100', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_dstchannel)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_dstchannel";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for dstchannel field";
		info->description =
			"Verifies cdr_get_key_value() returns the correct value "
			"for the dstchannel field.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "dstchannel");
	if (!val || strcmp(val, "PJSIP/2001-00000002") != 0) {
		ast_test_status_update(test,
			"Expected 'PJSIP/2001-00000002', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_dcontext)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_dcontext";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for dcontext field";
		info->description =
			"Verifies cdr_get_key_value() returns the correct value "
			"for the dcontext field.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "dcontext");
	if (!val || strcmp(val, "from-internal") != 0) {
		ast_test_status_update(test,
			"Expected 'from-internal', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_tenantid)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_tenantid";
		info->category = TEST_CATEGORY;
		info->summary = "Key extraction for tenantid field";
		info->description =
			"Verifies cdr_get_key_value() returns the correct value "
			"for the tenantid field.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "tenantid");
	if (!val || strcmp(val, "tenant-01") != 0) {
		ast_test_status_update(test,
			"Expected 'tenant-01', got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

/* ---- Key edge cases ---- */

AST_TEST_DEFINE(key_case_insensitive)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_case_insensitive";
		info->category = TEST_CATEGORY;
		info->summary = "Key lookup is case-insensitive";
		info->description =
			"Verifies cdr_get_key_value() matches field names "
			"regardless of case (e.g. 'LinkedID', 'SRC').";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "LinkedID");
	if (!val || strcmp(val, "1700000000.1") != 0) {
		ast_test_status_update(test,
			"'LinkedID' lookup failed: got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	val = cdr_get_key_value(&cdr, "SRC");
	if (!val || strcmp(val, "1001") != 0) {
		ast_test_status_update(test,
			"'SRC' lookup failed: got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	val = cdr_get_key_value(&cdr, "ACCOUNTCODE");
	if (!val || strcmp(val, "acct-100") != 0) {
		ast_test_status_update(test,
			"'ACCOUNTCODE' lookup failed: got '%s'\n",
			val ? val : "(null)");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_empty_field)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_empty_field";
		info->category = TEST_CATEGORY;
		info->summary = "Empty or NULL field name returns NULL";
		info->description =
			"Verifies cdr_get_key_value() returns NULL when the "
			"field name is empty or NULL.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "");
	if (val != NULL) {
		ast_test_status_update(test,
			"Empty field should return NULL, got '%s'\n", val);
		return AST_TEST_FAIL;
	}

	val = cdr_get_key_value(&cdr, NULL);
	if (val != NULL) {
		ast_test_status_update(test,
			"NULL field should return NULL, got '%s'\n", val);
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

AST_TEST_DEFINE(key_unknown_field)
{
	struct ast_cdr cdr;
	const char *val;

	switch (cmd) {
	case TEST_INIT:
		info->name = "key_unknown_field";
		info->category = TEST_CATEGORY;
		info->summary = "Unknown field name returns NULL";
		info->description =
			"Verifies cdr_get_key_value() returns NULL for an "
			"unrecognized field name.";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	build_test_cdr(&cdr);

	val = cdr_get_key_value(&cdr, "nonexistent_field");
	if (val != NULL) {
		ast_test_status_update(test,
			"Unknown field should return NULL, got '%s'\n", val);
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

/* ---- CDR backend registration test ---- */

AST_TEST_DEFINE(backend_registered)
{
	switch (cmd) {
	case TEST_INIT:
		info->name = "backend_registered";
		info->category = TEST_CATEGORY;
		info->summary = "CDR Kafka backend is registered";
		info->description =
			"Verifies that the Kafka CDR backend is registered "
			"by checking ast_cdr_is_enabled().";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	if (!ast_cdr_is_enabled()) {
		ast_test_status_update(test,
			"CDR system is not enabled\n");
		return AST_TEST_FAIL;
	}

	return AST_TEST_PASS;
}

/* ---- Module lifecycle ---- */

static int load_module(void)
{
	AST_TEST_REGISTER(key_linkedid);
	AST_TEST_REGISTER(key_uniqueid);
	AST_TEST_REGISTER(key_channel);
	AST_TEST_REGISTER(key_src_dst);
	AST_TEST_REGISTER(key_accountcode);
	AST_TEST_REGISTER(key_dstchannel);
	AST_TEST_REGISTER(key_dcontext);
	AST_TEST_REGISTER(key_tenantid);
	AST_TEST_REGISTER(key_case_insensitive);
	AST_TEST_REGISTER(key_empty_field);
	AST_TEST_REGISTER(key_unknown_field);
	AST_TEST_REGISTER(backend_registered);

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	AST_TEST_UNREGISTER(key_linkedid);
	AST_TEST_UNREGISTER(key_uniqueid);
	AST_TEST_UNREGISTER(key_channel);
	AST_TEST_UNREGISTER(key_src_dst);
	AST_TEST_UNREGISTER(key_accountcode);
	AST_TEST_UNREGISTER(key_dstchannel);
	AST_TEST_UNREGISTER(key_dcontext);
	AST_TEST_UNREGISTER(key_tenantid);
	AST_TEST_UNREGISTER(key_case_insensitive);
	AST_TEST_UNREGISTER(key_empty_field);
	AST_TEST_UNREGISTER(key_unknown_field);
	AST_TEST_UNREGISTER(backend_registered);

	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Kafka CDR Backend Tests",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
	.requires = "cdr_kafka",
);
