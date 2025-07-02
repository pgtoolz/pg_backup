/*-------------------------------------------------------------------------
 *
 * init.c: - initialize backup catalog.
 *
 * Portions Copyright (c) 2009-2011, NIPPON TELEGRAPH AND TELEPHONE CORPORATION
 * Portions Copyright (c) 2015-2019, Postgres Professional
 *
 *-------------------------------------------------------------------------
 */

#include "pg_probackup.h"

#include <unistd.h>
#include <sys/stat.h>

/*
 * Initialize backup catalog.
 */
int
do_init(CatalogState *catalogState)
{
	int			results;

	results = pg_check_dir(catalogState->catalog_path);

	if (results == 4)	/* exists and not empty*/
		elog(ERROR, "backup catalog already exist and it's not empty");
	else if (results == -1) /*trouble accessing directory*/
	{
		int errno_tmp = errno;
		elog(ERROR, "cannot open backup catalog directory \"%s\": %s",
			catalogState->catalog_path, strerror(errno_tmp));
	}

	/* create backup catalog root directory */
	fio_mkdir(FIO_BACKUP_HOST, catalogState->catalog_path, DIR_PERMISSION, false);

	/* create backup catalog data directory */
	fio_mkdir(FIO_BACKUP_HOST, catalogState->backup_subdir_path, DIR_PERMISSION, false);

	/* create backup catalog wal directory */
	fio_mkdir(FIO_BACKUP_HOST, catalogState->wal_subdir_path, DIR_PERMISSION, false);

	elog(INFO, "Backup catalog '%s' successfully inited", catalogState->catalog_path);
	return 0;
}

int
do_add_instance(InstanceState *instanceState, InstanceConfig *instance)
{
	struct stat st;
	ControlFileData *ControlFile = NULL;
	CatalogState *catalogState = instanceState->catalog_state;

	/* PGDATA is always required */
	if (instance->pgdata == NULL)
		elog(ERROR, "Required parameter not specified: PGDATA "
						 "(-D, --pgdata)");

	/* read PG_VERSION */
	instance->server_major_version = get_pg_version(FIO_DB_HOST, instance->pgdata);
	// bail out if major version is below PG10
	if (instance->server_major_version < 10)
		elog(ERROR, "PostgreSQL version must be 10 or higher");
	/* Read system_identifier from PGDATA */
	ControlFile = get_ControlFileData(FIO_DB_HOST, instance->pgdata, false);
	instance->system_identifier = ControlFile->system_identifier;

	if (instance->server_major_version >= 11)
		instance->xlog_seg_size = ControlFile->xlog_seg_size;
	else
		instance->xlog_seg_size = DEFAULT_XLOG_SEG_SIZE;

	/* Ensure that all root directories already exist */
	/* TODO maybe call do_init() here instead of error?*/
	if (access(catalogState->catalog_path, F_OK) != 0)
		elog(ERROR, "Directory does not exist: '%s'", catalogState->catalog_path);

	if (access(catalogState->backup_subdir_path, F_OK) != 0)
		elog(ERROR, "Directory does not exist: '%s'", catalogState->backup_subdir_path);

	if (access(catalogState->wal_subdir_path, F_OK) != 0)
		elog(ERROR, "Directory does not exist: '%s'", catalogState->wal_subdir_path);

	if (stat(instanceState->instance_backup_subdir_path, &st) == 0 && S_ISDIR(st.st_mode))
		elog(ERROR, "Instance '%s' backup directory already exists: '%s'",
			instanceState->instance_name, instanceState->instance_backup_subdir_path);

	/*
	 * Create directory for wal files of this specific instance.
	 * Existence check is extra paranoid because if we don't have such a
	 * directory in data dir, we shouldn't have it in wal as well.
	 */
	if (stat(instanceState->instance_wal_subdir_path, &st) == 0 && S_ISDIR(st.st_mode))
		elog(ERROR, "Instance '%s' WAL archive directory already exists: '%s'",
				instanceState->instance_name, instanceState->instance_wal_subdir_path);

	/* Create directory for data files of this specific instance */
	fio_mkdir(FIO_BACKUP_HOST, instanceState->instance_backup_subdir_path, DIR_PERMISSION, false);
	fio_mkdir(FIO_BACKUP_HOST, instanceState->instance_wal_subdir_path, DIR_PERMISSION, false);

	/*
	 * Write initial configuration file.
	 * system-identifier, xlog-seg-size, server-major-version and pgdata are set in init subcommand
	 * and will never be updated.
	 *
	 * We need to manually set options source to save them to the configuration
	 * file.
	 */
	config_set_opt(instance_options, &instance->system_identifier,
				   SOURCE_FILE);
	config_set_opt(instance_options, &instance->xlog_seg_size,
				   SOURCE_FILE);
	config_set_opt(instance_options, &instance->server_major_version,
				   SOURCE_FILE);

	/* Kludge: do not save remote options into config */
	config_set_opt(instance_options, &instance_config.remote.host,
				   SOURCE_DEFAULT);
	config_set_opt(instance_options, &instance_config.remote.proto,
				   SOURCE_DEFAULT);
	config_set_opt(instance_options, &instance_config.remote.port,
				   SOURCE_DEFAULT);
	config_set_opt(instance_options, &instance_config.remote.path,
				   SOURCE_DEFAULT);
	config_set_opt(instance_options, &instance_config.remote.user,
				   SOURCE_DEFAULT);
	config_set_opt(instance_options, &instance_config.remote.ssh_options,
				   SOURCE_DEFAULT);
	config_set_opt(instance_options, &instance_config.remote.ssh_config,
				   SOURCE_DEFAULT);

	/* pgdata was set through command line */
	do_set_config(instanceState, true);

	elog(INFO, "Instance '%s' successfully inited", instanceState->instance_name);
	return 0;
}
