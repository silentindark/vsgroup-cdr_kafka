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
 * \brief Kafka CDR Backend
 *
 * \author Wazo Communication Inc.
 */

/*** MODULEINFO
	<depend>res_kafka</depend>
	<support_level>core</support_level>
 ***/

/*** DOCUMENTATION
	<configInfo name="cdr_kafka" language="en_US">
		<synopsis>Kafka CDR Backend</synopsis>
		<configFile name="cdr_kafka.conf">
			<configObject name="global">
				<synopsis>Global configuration settings</synopsis>
				<configOption name="loguniqueid">
					<synopsis>Determines whether to log the uniqueid for calls</synopsis>
					<description>
						<para>Default is no.</para>
					</description>
				</configOption>
				<configOption name="loguserfield">
					<synopsis>Determines whether to log the user field for calls</synopsis>
					<description>
						<para>Default is no.</para>
					</description>
				</configOption>
				<configOption name="connection">
					<synopsis>Name of the connection from kafka.conf to use</synopsis>
					<description>
						<para>Specifies the name of the connection from kafka.conf to use</para>
					</description>
				</configOption>
				<configOption name="topic">
					<synopsis>Name of the topic to publish to</synopsis>
					<description>
						<para>Defaults to asterisk_cdr</para>
					</description>
				</configOption>
				<configOption name="key">
					<synopsis>CDR field to use as Kafka message key</synopsis>
					<description>
						<para>Name of the CDR field whose value is sent as the
						Kafka message key for partitioning. Valid values:
						linkedid, uniqueid, channel, dstchannel, accountcode,
						src, dst, dcontext, tenantid.
						Empty (default) means no key.</para>
					</description>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
 ***/

#include "asterisk.h"

#include "asterisk/cdr.h"
#include "asterisk/config_options.h"
#include "asterisk/json.h"
#include "asterisk/module.h"
#include "asterisk/kafka.h"
#include "asterisk/paths.h"
#include "asterisk/stringfields.h"
#include "asterisk/utils.h"

#define CDR_NAME "Kafka"
#define CONF_FILENAME "cdr_kafka.conf"

/*! \brief global config structure */
struct cdr_kafka_global_conf {
	AST_DECLARE_STRING_FIELDS(
		/*! \brief connection name */
		AST_STRING_FIELD(connection);
		/*! \brief topic name */
		AST_STRING_FIELD(topic);
		/*! \brief CDR field name to use as Kafka key */
		AST_STRING_FIELD(key);
	);
	/*! \brief whether to log the unique id */
	int loguniqueid;
	/*! \brief whether to log the user field */
	int loguserfield;

};

/*! \brief cdr_kafka configuration */
struct cdr_kafka_conf {
	struct cdr_kafka_global_conf *global;
};

/*! \brief Locking container for safe configuration access. */
static AO2_GLOBAL_OBJ_STATIC(confs);

/*! \brief Cached Kafka producer for fast access from CDR threads. */
static AO2_GLOBAL_OBJ_STATIC(cached_producer);

static struct aco_type global_option = {
	.type = ACO_GLOBAL,
	.name = "global",
	.item_offset = offsetof(struct cdr_kafka_conf, global),
	.category = "^global$",
	.category_match = ACO_WHITELIST,
};

static struct aco_type *global_options[] = ACO_TYPES(&global_option);

static void conf_global_dtor(void *obj)
{
	struct cdr_kafka_global_conf *global = obj;
	ast_string_field_free_memory(global);
}

static struct cdr_kafka_global_conf *conf_global_create(void)
{
	RAII_VAR(struct cdr_kafka_global_conf *, global, NULL, ao2_cleanup);

	global = ao2_alloc(sizeof(*global), conf_global_dtor);
	if (!global) {
		return NULL;
	}

	if (ast_string_field_init(global, 64) != 0) {
		return NULL;
	}

	aco_set_defaults(&global_option, "global", global);

	return ao2_bump(global);
}

/*! \brief The conf file that's processed for the module. */
static struct aco_file conf_file = {
	/*! The config file name. */
	.filename = CONF_FILENAME,
	/*! The mapping object types to be processed. */
	.types = ACO_TYPES(&global_option),
};

static void conf_dtor(void *obj)
{
	struct cdr_kafka_conf *conf = obj;

	ao2_cleanup(conf->global);
}

static void *conf_alloc(void)
{
	RAII_VAR(struct cdr_kafka_conf *, conf, NULL, ao2_cleanup);

	conf = ao2_alloc_options(sizeof(*conf), conf_dtor,
		AO2_ALLOC_OPT_LOCK_NOLOCK);
	if (!conf) {
		return NULL;
	}

	conf->global = conf_global_create();
	if (!conf->global) {
		return NULL;
	}

	return ao2_bump(conf);
}

static int setup_kafka(void);

CONFIG_INFO_STANDARD(cfg_info, confs, conf_alloc,
	.files = ACO_FILES(&conf_file),
	.pre_apply_config = setup_kafka,
);

static int setup_kafka(void)
{
	struct cdr_kafka_conf *conf = aco_pending_config(&cfg_info);

	if (!conf) {
		return 0;
	}

	if (!conf->global) {
		ast_log(LOG_ERROR, "Invalid cdr_kafka.conf\n");
		return -1;
	}

	return 0;
}

static int setup_cached_producer(void)
{
	RAII_VAR(struct cdr_kafka_conf *, conf, ao2_global_obj_ref(confs), ao2_cleanup);

	if (!conf || !conf->global || ast_strlen_zero(conf->global->connection)) {
		ast_log(LOG_WARNING, "No Kafka connection configured\n");
		return -1;
	}

	struct ast_kafka_producer *producer = ast_kafka_get_producer(conf->global->connection);
	if (!producer) {
		ast_log(LOG_ERROR, "Failed to get Kafka producer for connection '%s'\n",
			conf->global->connection);
		return -1;
	}

	ao2_global_obj_replace_unref(cached_producer, producer);
	ao2_cleanup(producer);
	return 0;
}

/*!
 * \brief Extract the value of a named CDR field for use as Kafka key.
 *
 * \param cdr The CDR record.
 * \param field_name The CDR field name (e.g. "linkedid", "src").
 * \return The field value string, or NULL if field_name is empty/unknown.
 */
const char *cdr_get_key_value(struct ast_cdr *cdr, const char *field_name)
{
	if (ast_strlen_zero(field_name)) {
		return NULL;
	}

	if (!strcasecmp(field_name, "linkedid")) {
		return cdr->linkedid;
	} else if (!strcasecmp(field_name, "uniqueid")) {
		return cdr->uniqueid;
	} else if (!strcasecmp(field_name, "channel")) {
		return cdr->channel;
	} else if (!strcasecmp(field_name, "dstchannel")) {
		return cdr->dstchannel;
	} else if (!strcasecmp(field_name, "accountcode")) {
		return cdr->accountcode;
	} else if (!strcasecmp(field_name, "src")) {
		return cdr->src;
	} else if (!strcasecmp(field_name, "dst")) {
		return cdr->dst;
	} else if (!strcasecmp(field_name, "dcontext")) {
		return cdr->dcontext;
	} else if (!strcasecmp(field_name, "tenantid")) {
		return cdr->tenantid;
	}

	return NULL;
}

/*!
 * \brief CDR handler for Kafka.
 *
 * \param cdr CDR to log.
 * \return 0 on success.
 * \return -1 on error.
 */
static int kafka_cdr_log(struct ast_cdr *cdr)
{
	RAII_VAR(struct cdr_kafka_conf *, conf, NULL, ao2_cleanup);
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	RAII_VAR(char *, str, NULL, ast_json_free);
	int res;

	conf = ao2_global_obj_ref(confs);

	ast_assert(conf && conf->global && conf->global->connection);

	struct ast_kafka_producer *producer = ao2_global_obj_ref(cached_producer);
	if (!producer) {
		/* Fallback: try to acquire if not yet cached */
		producer = ast_kafka_get_producer(conf->global->connection);
		if (!producer) {
			ast_log(LOG_ERROR, "Failed to get a Kafka producer\n");
			return -1;
		}
	}

	json = ast_json_pack("{"
		/* clid, src, dst, dcontext */
		"s: s, s: s, s: s, s: s,"
		/* channel, dstchannel, lastapp, lastdata */
		"s: s, s: s, s: s, s: s,"
		/* start, answer, end, duration */
		"s: o, s: o, s: o, s: i"
		/* billsec, disposition, accountcode, amaflags */
		"s: i, s: s, s: s, s: s"
		/* peeraccount, linkedid and sequence*/
		"s: s, s: s, s: i }",

		"clid", cdr->clid,
		"src", cdr->src,
		"dst", cdr->dst,
		"dcontext", cdr->dcontext,

		"channel", cdr->channel,
		"dstchannel", cdr->dstchannel,
		"lastapp", cdr->lastapp,
		"lastdata", cdr->lastdata,

		"start", ast_json_timeval(cdr->start, NULL),
		"answer", ast_json_timeval(cdr->answer, NULL),
		"end", ast_json_timeval(cdr->end, NULL),
		"durationsec", cdr->duration,

		"billsec", cdr->billsec,
		"disposition", ast_cdr_disp2str(cdr->disposition),
		"accountcode", cdr->accountcode,
		"amaflags", ast_channel_amaflags2string(cdr->amaflags),

		"peeraccount", cdr->peeraccount,
		"linkedid", cdr->linkedid,
		"sequence", cdr->sequence);
	if (!json) {
		ao2_cleanup(producer);
		return -1;
	}

	/* Inject system identification */
	{
		char eid_str[20];
		ast_eid_to_str(eid_str, sizeof(eid_str), &ast_eid_default);
		ast_json_object_set(json, "EntityID", ast_json_string_create(eid_str));
	}
	if (!ast_strlen_zero(ast_config_AST_SYSTEM_NAME)) {
		ast_json_object_set(json, "SystemName",
			ast_json_string_create(ast_config_AST_SYSTEM_NAME));
	}

	struct ast_var_t *var_entry;

	AST_LIST_TRAVERSE(&cdr->varshead, var_entry, entries) {
		ast_json_object_set(json,var_entry->name, ast_json_string_create(var_entry->value));
        }

	/* Set optional fields */
	if (conf->global->loguniqueid) {
		ast_json_object_set(json,
			"uniqueid", ast_json_string_create(cdr->uniqueid));
	}

	if (conf->global->loguserfield) {
		ast_json_object_set(json,
			"userfield", ast_json_string_create(cdr->userfield));
	}

	/* Dump the JSON to a string for publication */
	str = ast_json_dump_string(json);
	if (!str) {
		ast_log(LOG_ERROR, "Failed to build string from JSON\n");
		ao2_cleanup(producer);
		return -1;
	}

	res = ast_kafka_produce(producer,
		conf->global->topic,
		cdr_get_key_value(cdr, conf->global->key),
		str,
		strlen(str));

	ao2_cleanup(producer);

	if (res != 0) {
		ast_log(LOG_ERROR, "Error publishing CDR to Kafka\n");
		return -1;
	}

	return 0;
}


static int load_config(int reload)
{
	RAII_VAR(struct cdr_kafka_conf *, conf, NULL, ao2_cleanup);

	switch (aco_process_config(&cfg_info, reload)) {
	case ACO_PROCESS_ERROR:
		return -1;
	case ACO_PROCESS_OK:
	case ACO_PROCESS_UNCHANGED:
		break;
	}

	conf = ao2_global_obj_ref(confs);
	if (!conf || !conf->global) {
		ast_log(LOG_ERROR, "Error obtaining config from cdr_kafka.conf\n");
		return -1;
	}

	return 0;
}

static int load_module(void)
{
	if (aco_info_init(&cfg_info) != 0) {
		ast_log(LOG_ERROR, "Failed to initialize config");
		aco_info_destroy(&cfg_info);
		return -1;
	}

	aco_option_register(&cfg_info, "loguniqueid", ACO_EXACT,
		global_options, "no", OPT_BOOL_T, 1,
		FLDSET(struct cdr_kafka_global_conf, loguniqueid));
	aco_option_register(&cfg_info, "loguserfield", ACO_EXACT,
		global_options, "no", OPT_BOOL_T, 1,
		FLDSET(struct cdr_kafka_global_conf, loguserfield));
	aco_option_register(&cfg_info, "connection", ACO_EXACT,
		global_options, "", OPT_STRINGFIELD_T, 0,
		STRFLDSET(struct cdr_kafka_global_conf, connection));
	aco_option_register(&cfg_info, "topic", ACO_EXACT,
		global_options, "asterisk_cdr", OPT_STRINGFIELD_T, 0,
		STRFLDSET(struct cdr_kafka_global_conf, topic));
	aco_option_register(&cfg_info, "key", ACO_EXACT,
		global_options, "", OPT_STRINGFIELD_T, 0,
		STRFLDSET(struct cdr_kafka_global_conf, key));

	if (load_config(0) != 0) {
		ast_log(LOG_WARNING, "Configuration failed to load\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	setup_cached_producer();

	if (ast_cdr_register(CDR_NAME, ast_module_info->description, kafka_cdr_log) != 0) {
		ast_log(LOG_ERROR, "Could not register CDR backend\n");
		return AST_MODULE_LOAD_FAILURE;
	}

	ast_log(LOG_NOTICE, "CDR Kafka logging enabled\n");
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ao2_global_obj_release(cached_producer);
	aco_info_destroy(&cfg_info);
	ao2_global_obj_release(confs);
	if (ast_cdr_unregister(CDR_NAME) != 0) {
		return -1;
	}

	return 0;
}

static int reload_module(void)
{
	int res = load_config(1);
	if (res == 0) {
		setup_cached_producer();
	}
	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Kafka CDR Backend",
		.support_level = AST_MODULE_SUPPORT_CORE,
		.load = load_module,
		.unload = unload_module,
		.reload = reload_module,
		.load_pri = AST_MODPRI_CDR_DRIVER,
	);
