/*-------------------------------------------------------------------------
 *
 * configure.c: - manage backup catalog.
 *
 * Copyright (c) 2017-2022, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */

#include "pg_probackup.h"

#include <unistd.h>

#include "utils/configuration.h"
#include "utils/json.h"


static void assign_log_level_console(ConfigOption *opt, const char *arg);
static void assign_log_level_file(ConfigOption *opt, const char *arg);
static void assign_log_format_console(ConfigOption *opt, const char *arg);
static void assign_log_format_file(ConfigOption *opt, const char *arg);
static void assign_compress_alg(ConfigOption *opt, const char *arg);

static char *get_log_level_console(ConfigOption *opt);
static char *get_log_level_file(ConfigOption *opt);
static char *get_log_format_console(ConfigOption *opt);
static char *get_log_format_file(ConfigOption *opt);
static char *get_compress_alg(ConfigOption *opt);

static void show_configure_start(void);
static void show_configure_end(void);

static void show_configure_plain(ConfigOption *opt);
static void show_configure_json(ConfigOption *opt);

#define RETENTION_REDUNDANCY_DEFAULT	0
#define RETENTION_WINDOW_DEFAULT		0

#define OPTION_INSTANCE_GROUP	"Backup instance information"
#define OPTION_CONN_GROUP		"Connection parameters"
#define OPTION_ARCHIVE_GROUP	"Archive parameters"
#define OPTION_LOG_GROUP		"Logging parameters"
#define OPTION_RETENTION_GROUP	"Retention parameters"
#define OPTION_COMPRESS_GROUP	"Compression parameters"
#define OPTION_REMOTE_GROUP		"Remote access parameters"

/* dummy placeholder for obsolete options to store in following instance_options[] */
static char	*obsolete_option_placeholder = NULL;

/*
 * Short name should be non-printable ASCII character.
 */
ConfigOption instance_options[] =
{
	/* Instance options */
	{
		's', 'D', "pgdata",
		&instance_config.pgdata, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_INSTANCE_GROUP, 0, option_get_value
	},
	{
		'U', 199, "system-identifier",
		&instance_config.system_identifier, SOURCE_FILE_STRICT, 0,
		OPTION_INSTANCE_GROUP, 0, option_get_value
	},
	{
		'u', 200, "xlog-seg-size",
		&instance_config.xlog_seg_size, SOURCE_FILE_STRICT, 0,
		OPTION_INSTANCE_GROUP, 0, option_get_value
	},
	{
		'u', 201, "server-major-version",
		&instance_config.server_major_version, SOURCE_FILE_STRICT, 0,
		OPTION_INSTANCE_GROUP, 0, option_get_value
	},
	/* Connection options */
	{
		's', 'd', "pgdatabase",
		&instance_config.conn_opt.pgdatabase, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_CONN_GROUP, 0, option_get_value
	},
	{
		's', 'h', "pghost",
		&instance_config.conn_opt.pghost, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_CONN_GROUP, 0, option_get_value
	},
	{
		's', 'p', "pgport",
		&instance_config.conn_opt.pgport, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_CONN_GROUP, 0, option_get_value
	},
	{
		's', 'U', "pguser",
		&instance_config.conn_opt.pguser, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_CONN_GROUP, 0, option_get_value
	},
	/* Obsolete options */
	{
		's', 202, "master-db",
		&obsolete_option_placeholder, SOURCE_FILE_STRICT, SOURCE_CONST, "", 0, option_get_value
	},
	{
		's', 203, "master-host",
		&obsolete_option_placeholder, SOURCE_FILE_STRICT, SOURCE_CONST, "", 0, option_get_value
	},
	{
		's', 204, "master-port",
		&obsolete_option_placeholder, SOURCE_FILE_STRICT, SOURCE_CONST, "", 0, option_get_value
	},
	{
		's', 205, "master-user",
		&obsolete_option_placeholder, SOURCE_FILE_STRICT, SOURCE_CONST, "", 0, option_get_value
	},
	{
		's', 206, "replica-timeout",
		&obsolete_option_placeholder, SOURCE_FILE_STRICT, SOURCE_CONST, "", 0, option_get_value
	},
	/* Archive options */
	{
		'u', 207, "archive-timeout",
		&instance_config.archive_timeout, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_ARCHIVE_GROUP, OPTION_UNIT_S, option_get_value
	},
	{
		's', 208, "archive-host",
		&instance_config.archive.host, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_ARCHIVE_GROUP, 0, option_get_value
	},
	{
		's', 209, "archive-port",
		&instance_config.archive.port, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_ARCHIVE_GROUP, 0, option_get_value
	},
	{
		's', 210, "archive-user",
		&instance_config.archive.user, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_ARCHIVE_GROUP, 0, option_get_value
	},
	{
		's', 211, "restore-command",
		&instance_config.restore_command, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_ARCHIVE_GROUP, 0, option_get_value
	},
	/* Logging options */
	{
		'f', 212, "log-level-console",
		assign_log_level_console, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, 0, get_log_level_console
	},
	{
		'f', 213, "log-level-file",
		assign_log_level_file, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, 0, get_log_level_file
	},
	{
		'f', 214, "log-format-console",
		assign_log_format_console, SOURCE_CMD_STRICT, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, 0, get_log_format_console
	},
	{
		'f', 215, "log-format-file",
		assign_log_format_file, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, 0, get_log_format_file
	},
	{
		's', 216, "log-filename",
		&instance_config.logger.log_filename, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, 0, option_get_value
	},
	{
		's', 217, "error-log-filename",
		&instance_config.logger.error_log_filename, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, 0, option_get_value
	},
	{
		's', 218, "log-directory",
		&instance_config.logger.log_directory, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, 0, option_get_value
	},
	{
		'U', 219, "log-rotation-size",
		&instance_config.logger.log_rotation_size, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, OPTION_UNIT_KB, option_get_value
	},
	{
		'U', 220, "log-rotation-age",
		&instance_config.logger.log_rotation_age, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_LOG_GROUP, OPTION_UNIT_MS, option_get_value
	},
	/* Retention options */
	{
		'u', 221, "retention-redundancy",
		&instance_config.retention_redundancy, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_RETENTION_GROUP, 0, option_get_value
	},
	{
		'u', 222, "retention-window",
		&instance_config.retention_window, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_RETENTION_GROUP, 0, option_get_value
	},
	{
		'u', 223, "wal-depth",
		&instance_config.wal_depth, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_RETENTION_GROUP, 0, option_get_value
	},
	/* Compression options */
	{
		'f', 224, "compress-algorithm",
		assign_compress_alg, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_COMPRESS_GROUP, 0, get_compress_alg
	},
	{
		'u', 225, "compress-level",
		&instance_config.compress_level, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_COMPRESS_GROUP, 0, option_get_value
	},
	/* Remote backup options */
	{
		's', 226, "remote-proto",
		&instance_config.remote.proto, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_REMOTE_GROUP, 0, option_get_value
	},
	{
		's', 227, "remote-host",
		&instance_config.remote.host, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_REMOTE_GROUP, 0, option_get_value
	},
	{
		's', 228, "remote-port",
		&instance_config.remote.port, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_REMOTE_GROUP, 0, option_get_value
	},
	{
		's', 229, "remote-path",
		&instance_config.remote.path, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_REMOTE_GROUP, 0, option_get_value
	},
	{
		's', 230, "remote-user",
		&instance_config.remote.user, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_REMOTE_GROUP, 0, option_get_value
	},
	{
		's', 231, "ssh-options",
		&instance_config.remote.ssh_options, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_REMOTE_GROUP, 0, option_get_value
	},
	{
		's', 232, "ssh-config",
		&instance_config.remote.ssh_config, SOURCE_CMD, SOURCE_DEFAULT,
		OPTION_REMOTE_GROUP, 0, option_get_value
	},
	{ 0 }
};

/* An instance configuration with default options */
InstanceConfig instance_config;

static PQExpBufferData show_buf;
static int32 json_level = 0;
static const char *current_group = NULL;

/*
 * Show configure options including default values.
 */
void
do_show_config(void)
{
	int			i;

	show_configure_start();

	for (i = 0; instance_options[i].type; i++)
	{
		if (show_format == SHOW_PLAIN)
			show_configure_plain(&instance_options[i]);
		else
			show_configure_json(&instance_options[i]);
	}

	show_configure_end();
}

/*
 * Save configure options into BACKUP_CATALOG_CONF_FILE. Do not save default
 * values into the file.
 */
void
do_set_config(InstanceState *instanceState, bool missing_ok)
{
	char		path_temp[MAXPGPATH];
	FILE	   *fp;
	int			i;

	snprintf(path_temp, sizeof(path_temp), "%s.tmp", instanceState->instance_config_path);

	if (!missing_ok && !fileExists(instanceState->instance_config_path, FIO_LOCAL_HOST))
		elog(ERROR, "Configuration file \"%s\" doesn't exist", instanceState->instance_config_path);

	fp = fopen(path_temp, "wt");
	if (fp == NULL)
		elog(ERROR, "Cannot create configuration file \"%s\": %s",
			 BACKUP_CATALOG_CONF_FILE, strerror(errno));

	current_group = NULL;

	for (i = 0; instance_options[i].type; i++)
	{
		int rc = 0;
		ConfigOption *opt = &instance_options[i];
		char	   *value;

		/* Save only options from command line */
		if (opt->source != SOURCE_CMD &&
			/* ...or options from the previous configure file */
			opt->source != SOURCE_FILE && opt->source != SOURCE_FILE_STRICT)
			continue;

		value = opt->get_value(opt);
		if (value == NULL)
			continue;

		if (current_group == NULL || strcmp(opt->group, current_group) != 0)
		{
			current_group = opt->group;
			fprintf(fp, "# %s\n", current_group);
		}

		if (strchr(value, ' '))
			rc = fprintf(fp, "%s = '%s'\n", opt->lname, value);
		else
			rc = fprintf(fp, "%s = %s\n", opt->lname, value);

		if (rc < 0)
			elog(ERROR, "Cannot write to configuration file: \"%s\"", path_temp);

		pfree(value);
	}

	if (ferror(fp) || fflush(fp))
		elog(ERROR, "Cannot write to configuration file: \"%s\"", path_temp);

	if (fclose(fp))
		elog(ERROR, "Cannot close configuration file: \"%s\"", path_temp);

	if (fio_sync(FIO_LOCAL_HOST, path_temp) != 0)
		elog(ERROR, "Failed to sync temp configuration file \"%s\": %s",
			 path_temp, strerror(errno));

	if (rename(path_temp, instanceState->instance_config_path) < 0)
	{
		int			errno_temp = errno;
		unlink(path_temp);
		elog(ERROR, "Cannot rename configuration file \"%s\" to \"%s\": %s",
			 path_temp, instanceState->instance_config_path, strerror(errno_temp));
	}
}

void
init_config(InstanceConfig *config, const char *instance_name)
{
	MemSet(config, 0, sizeof(InstanceConfig));

	config->archive_timeout = ARCHIVE_TIMEOUT_DEFAULT;

	/* Copy logger defaults */
	config->logger = logger_config;

	config->retention_redundancy = RETENTION_REDUNDANCY_DEFAULT;
	config->retention_window = RETENTION_WINDOW_DEFAULT;
	config->wal_depth = 0;

	config->compress_alg = COMPRESS_ALG_DEFAULT;
	config->compress_level = COMPRESS_LEVEL_DEFAULT;

	config->remote.proto = (char*)"ssh";
}

/*
 * read instance config from file
 */
InstanceConfig *
readInstanceConfigFile(InstanceState *instanceState)
{
	InstanceConfig   *instance = pgut_new(InstanceConfig);
	char	   *log_level_console = NULL;
	char	   *log_level_file = NULL;
	char	   *log_format_console = NULL;
	char	   *log_format_file = NULL;
	char	   *compress_alg = NULL;
	int			parsed_options;

	ConfigOption instance_options[] =
	{
		/* Instance options */
		{
			's', 'D', "pgdata",
			&instance->pgdata, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_INSTANCE_GROUP, 0, option_get_value
		},
		{
			'U', 200, "system-identifier",
			&instance->system_identifier, SOURCE_FILE_STRICT, 0,
			OPTION_INSTANCE_GROUP, 0, option_get_value
		},
		{
			'u', 201, "xlog-seg-size",
			&instance->xlog_seg_size, SOURCE_FILE_STRICT, 0,
			OPTION_INSTANCE_GROUP, 0, option_get_value
		},
		/* Connection options */
		{
			's', 'd', "pgdatabase",
			&instance->conn_opt.pgdatabase, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_CONN_GROUP, 0, option_get_value
		},
		{
			's', 'h', "pghost",
			&instance->conn_opt.pghost, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_CONN_GROUP, 0, option_get_value
		},
		{
			's', 'p', "pgport",
			&instance->conn_opt.pgport, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_CONN_GROUP, 0, option_get_value
		},
		{
			's', 'U', "pguser",
			&instance->conn_opt.pguser, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_CONN_GROUP, 0, option_get_value
		},
		/* Archive options */
		{
			'u', 207, "archive-timeout",
			&instance->archive_timeout, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_ARCHIVE_GROUP, OPTION_UNIT_S, option_get_value
		},
		{
			's', 208, "archive-host",
			&instance_config.archive.host, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_ARCHIVE_GROUP, 0, option_get_value
		},
		{
			's', 209, "archive-port",
			&instance_config.archive.port, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_ARCHIVE_GROUP, 0, option_get_value
		},
		{
			's', 210, "archive-user",
			&instance_config.archive.user, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_ARCHIVE_GROUP, 0, option_get_value
		},
		{
			's', 211, "restore-command",
			&instance->restore_command, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_ARCHIVE_GROUP, 0, option_get_value
		},

		/* Instance options */
		{
			's', 'D', "pgdata",
			&instance->pgdata, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_INSTANCE_GROUP, 0, option_get_value
		},

		/* Logging options */
		{
			's', 212, "log-level-console",
			&log_level_console, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			's', 213, "log-level-file",
			&log_level_file, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			's', 214, "log-format-console",
			&log_format_console, SOURCE_CMD_STRICT, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			's', 215, "log-format-file",
			&log_format_file, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			's', 216, "log-filename",
			&instance->logger.log_filename, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			's', 217, "error-log-filename",
			&instance->logger.error_log_filename, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			's', 218, "log-directory",
			&instance->logger.log_directory, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			'U', 219, "log-rotation-size",
			&instance->logger.log_rotation_size, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, OPTION_UNIT_KB, option_get_value
		},
		{
			'U', 220, "log-rotation-age",
			&instance->logger.log_rotation_age, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, OPTION_UNIT_MS, option_get_value
		},
		/* Retention options */
		{
			'u', 221, "retention-redundancy",
			&instance->retention_redundancy, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_RETENTION_GROUP, 0, option_get_value
		},
		{
			'u', 222, "retention-window",
			&instance->retention_window, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_RETENTION_GROUP, 0, option_get_value
		},
		{
			'u', 223, "wal-depth",
			&instance->wal_depth, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_RETENTION_GROUP, 0, option_get_value
		},
		/* Compression options */
		{
			's', 224, "compress-algorithm",
			&compress_alg, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_LOG_GROUP, 0, option_get_value
		},
		{
			'u', 225, "compress-level",
			&instance->compress_level, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_COMPRESS_GROUP, 0, option_get_value
		},
		/* Remote backup options */
		{
			's', 226, "remote-proto",
			&instance->remote.proto, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_REMOTE_GROUP, 0, option_get_value
		},
		{
			's', 227, "remote-host",
			&instance->remote.host, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_REMOTE_GROUP, 0, option_get_value
		},
		{
			's', 228, "remote-port",
			&instance->remote.port, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_REMOTE_GROUP, 0, option_get_value
		},
		{
			's', 229, "remote-path",
			&instance->remote.path, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_REMOTE_GROUP, 0, option_get_value
		},
		{
			's', 230, "remote-user",
			&instance->remote.user, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_REMOTE_GROUP, 0, option_get_value
		},
		{
			's', 231, "ssh-options",
			&instance->remote.ssh_options, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_REMOTE_GROUP, 0, option_get_value
		},
		{
			's', 232, "ssh-config",
			&instance->remote.ssh_config, SOURCE_CMD, SOURCE_DEFAULT,
			OPTION_REMOTE_GROUP, 0, option_get_value
		},
		{ 0 }
	};


	init_config(instance, instanceState->instance_name);

	if (fio_access(FIO_BACKUP_HOST, instanceState->instance_config_path, F_OK) != 0)
	{
		elog(WARNING, "Control file \"%s\" doesn't exist", instanceState->instance_config_path);
		pfree(instance);
		return NULL;
	}

	parsed_options = config_read_opt(instanceState->instance_config_path,
									 instance_options, WARNING, true, true);

	if (parsed_options == 0)
	{
		elog(WARNING, "Control file \"%s\" is empty", instanceState->instance_config_path);
		pfree(instance);
		return NULL;
	}

	if (log_level_console)
		instance->logger.log_level_console = parse_log_level(log_level_console);

	if (log_level_file)
		instance->logger.log_level_file = parse_log_level(log_level_file);

	if (log_format_console)
		instance->logger.log_format_console = parse_log_format(log_format_console);

	if (log_format_file)
		instance->logger.log_format_file = parse_log_format(log_format_file);

	if (compress_alg)
		instance->compress_alg = parse_compress_alg(compress_alg);

	return instance;
}

static void
assign_log_level_console(ConfigOption *opt, const char *arg)
{
	instance_config.logger.log_level_console = parse_log_level(arg);
}

static void
assign_log_level_file(ConfigOption *opt, const char *arg)
{
	instance_config.logger.log_level_file = parse_log_level(arg);
}

static void
assign_log_format_console(ConfigOption *opt, const char *arg)
{
	instance_config.logger.log_format_console = parse_log_format(arg);
}

static void
assign_log_format_file(ConfigOption *opt, const char *arg)
{
	instance_config.logger.log_format_file = parse_log_format(arg);
}

static void
assign_compress_alg(ConfigOption *opt, const char *arg)
{
	instance_config.compress_alg = parse_compress_alg(arg);
}

static char *
get_log_level_console(ConfigOption *opt)
{
	return pstrdup(deparse_log_level(instance_config.logger.log_level_console));
}

static char *
get_log_level_file(ConfigOption *opt)
{
	return pstrdup(deparse_log_level(instance_config.logger.log_level_file));
}

static char *
get_log_format_console(ConfigOption *opt)
{
	return pstrdup(deparse_log_format(instance_config.logger.log_format_console));
}

static char *
get_log_format_file(ConfigOption *opt)
{
	return pstrdup(deparse_log_format(instance_config.logger.log_format_file));
}

static char *
get_compress_alg(ConfigOption *opt)
{
	return pstrdup(deparse_compress_alg(instance_config.compress_alg));
}

/*
 * Initialize configure visualization.
 */
static void
show_configure_start(void)
{
	initPQExpBuffer(&show_buf);

	if (show_format == SHOW_PLAIN)
		current_group = NULL;
	else
	{
		json_level = 0;
		json_add(&show_buf, JT_BEGIN_OBJECT, &json_level);
	}
}

/*
 * Finalize configure visualization.
 */
static void
show_configure_end(void)
{
	if (show_format == SHOW_PLAIN)
		current_group = NULL;
	else
	{
		json_add(&show_buf, JT_END_OBJECT, &json_level);
		appendPQExpBufferChar(&show_buf, '\n');
	}

	fputs(show_buf.data, stdout);
	termPQExpBuffer(&show_buf);
}

/*
 * Plain output.
 */

static void
show_configure_plain(ConfigOption *opt)
{
	char	   *value;

	value = opt->get_value(opt);
	if (value == NULL)
		return;

	if (current_group == NULL || strcmp(opt->group, current_group) != 0)
	{
		current_group = opt->group;
		appendPQExpBuffer(&show_buf, "# %s\n", current_group);
	}

	appendPQExpBuffer(&show_buf, "%s = %s\n", opt->lname, value);
	pfree(value);
}

/*
 * Json output.
 */

static void
show_configure_json(ConfigOption *opt)
{
	char	   *value;

	value = opt->get_value(opt);
	if (value == NULL)
		return;

	json_add_value(&show_buf, opt->lname, value, json_level,
				   true);
	pfree(value);
}
