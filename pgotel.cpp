/*-------------------------------------------------------------------------
 *
 * pgotel.cc
 *		POC to interact with OpenTelemetry
 *
 * Copyright (c) 1996-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pgotel/pgotel.cpp
 *
 *-------------------------------------------------------------------------
 */

#include "metrics_grpc.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include "postgres.h"

#include "fmgr.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/jsonb.h"
#include "utils/snapmgr.h"

/* Those are necessary for bgworker */
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pgotel_counter);

void _PG_init(void);
PGDLLEXPORT void pgotel_worker_main(Datum main_arg) pg_attribute_noreturn();

/* GUC variables */
static bool pgotel_enabled = false;
static int pgotel_interval = 2000;
static int pgotel_timeout = 500;
static char *pgotel_endpoint = NULL;
static int pgotel_worker_idle_time = 100;
static char *pgotel_dbname = NULL;

/* Signal handling */
static volatile sig_atomic_t got_sigterm = false;
static volatile sig_atomic_t got_sighup = false;

Datum
pgotel_counter(PG_FUNCTION_ARGS)
{
	char *counter_name = PG_ARGISNULL(0) ? NULL : text_to_cstring(PG_GETARG_TEXT_PP(0));
	float8 value = PG_ARGISNULL(1) ? -1 : PG_GETARG_FLOAT8(1);
	Jsonb *labels = PG_ARGISNULL(2) ? NULL : PG_GETARG_JSONB_P(2);
	std::map<std::string, std::string> labels_map;

	if (!counter_name)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("counter name cannot be NULL")));

	if (value < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("counter value cannot be negative")));

	if (labels == NULL && PG_NARGS() == 3)
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("labels cannot be NULL")));

	/* iterate over the labels JSONB to include key/values to the C++ map data structure */
	if (labels != NULL)
	{
		JsonbIterator *it = JsonbIteratorInit(&labels->root);
		JsonbValue v;
		JsonbIteratorToken type;
		while ((type = JsonbIteratorNext(&it, &v, true)) != WJB_DONE)
		{
			if (type == WJB_KEY)
			{
				char *key = pnstrdup(v.val.string.val, v.val.string.len);
				type = JsonbIteratorNext(&it, &v, true);
				if (type == WJB_VALUE)
				{
					char *val = pnstrdup(v.val.string.val, v.val.string.len);
					elog(DEBUG1, "Label: %s = %s", key, val);
					labels_map[key] = val;
				}
			}
		}
	}

	elog(DEBUG1, "labels_map size: %d", (int) labels_map.size());
	if (labels != NULL && (int) labels_map.size() == 0)
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid labels JSONB")));

	/* Send the counter to the OTEL-Collector */
	pgotel::InitMetrics(pgotel_endpoint,
						std::chrono::milliseconds(pgotel_interval),
						std::chrono::milliseconds(pgotel_timeout));
	pgotel::Counter(counter_name, value, labels_map);

	PG_RETURN_NULL();
}

static void
pgotel_define_gucs(void)
{
	DefineCustomStringVariable("pgotel.endpoint",
							   "Endpoint of the OTEL-Collector",
							   NULL,
							   &pgotel_endpoint,
							   "localhost:4317",
							   PGC_SIGHUP,
							   0,
							   NULL,
							   NULL,
							   NULL);

	DefineCustomStringVariable("pgotel.dbname",
							   "Database for the worker collect information to send to the "
							   "OTEL-Collector",
							   NULL,
							   &pgotel_dbname,
							   "postgres",
							   PGC_SIGHUP,
							   0,
							   NULL,
							   NULL,
							   NULL);

	/* Enable/Disable */
	DefineCustomBoolVariable("pgotel.enabled",
							 "Enable/Disable Send Logs to OTEL-Collector",
							 NULL,
							 &pgotel_enabled,
							 false,
							 PGC_SIGHUP,
							 0,
							 NULL,
							 NULL,
							 NULL);

	DefineCustomIntVariable("pgotel.interval",
							"Interval to send metrics to OTEL-Collector",
							NULL,
							&pgotel_interval,
							2000,
							1000,
							INT_MAX,
							PGC_SIGHUP,
							0,
							NULL,
							NULL,
							NULL);

	DefineCustomIntVariable("pgotel.timeout",
							"Timeout to send metrics to OTEL-Collector",
							NULL,
							&pgotel_timeout,
							500,
							100,
							INT_MAX,
							PGC_SIGHUP,
							0,
							NULL,
							NULL,
							NULL);
}

static void
pgotel_worker_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;

	got_sigterm = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);
	errno = save_errno;
}

static void
pgotel_worker_sighup(SIGNAL_ARGS)
{
	int save_errno = errno;

	got_sighup = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);
	errno = save_errno;
}

void
pgotel_worker_main(Datum main_arg)
{
	StringInfoData buf;

	/* Register functions for SIGTERM/SIGHUP management */
	pqsignal(SIGHUP, pgotel_worker_sighup);
	pqsignal(SIGTERM, pgotel_worker_sigterm);

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/* Connect to a database */
	BackgroundWorkerInitializeConnection(pgotel_dbname, NULL, 0);

	elog(LOG, "pgotel_worker: started and connected to %s database", pgotel_dbname);

	/* Build the query string */
	initStringInfo(&buf);
	appendStringInfo(&buf, "\
		SELECT 	pgotel_counter( \
				'idx_scan', idx_scan, \
				jsonb_build_object('dbname', current_database(), 'schemaname', schemaname, 'relname', relname)) \
		FROM 	pg_catalog.pg_stat_user_tables \
		WHERE 	coalesce(idx_scan, 0) > 0 \
		UNION ALL \
		SELECT 	pgotel_counter( \
				'seq_scan', seq_scan, \
				jsonb_build_object('dbname', current_database(), 'schemaname', schemaname, 'relname', relname)) \
		FROM 	pg_catalog.pg_stat_user_tables \
		WHERE 	coalesce(seq_scan, 0) > 0");

	/* Initialize OpenTelemetry Metrics SDK */
	pgotel::InitMetrics(pgotel_endpoint,
						std::chrono::milliseconds(pgotel_interval),
						std::chrono::milliseconds(pgotel_timeout));

	while (!got_sigterm)
	{
		/* Wait necessary amount of time */
		WaitLatch(&MyProc->procLatch,
				  WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
				  pgotel_worker_idle_time * 1L,
				  PG_WAIT_EXTENSION);
		ResetLatch(&MyProc->procLatch);

		/* Process signals */
		if (got_sighup)
		{
			/* Process config file */
			ProcessConfigFile(PGC_SIGHUP);
			got_sighup = false;

			/* Restart OpenTelemetry Metrics SDK */
			pgotel::CleanupMetrics();
			pgotel::InitMetrics(pgotel_endpoint,
								std::chrono::milliseconds(pgotel_interval),
								std::chrono::milliseconds(pgotel_timeout));

			ereport(LOG, (errmsg("%s: processed SIGHUP", "pgotel_worker")));
		}

		if (got_sigterm)
		{
			/* Simply exit */
			ereport(LOG, (errmsg("%s: processed SIGTERM", "pgotel_worker")));
			break;
		}

		/* Update NOW() to return correct timestamp */
		SetCurrentStatementStartTimestamp();

		/* Show query status in pg_stat_activity */
		pgstat_report_activity(STATE_RUNNING, "pgotel_worker_main");
		StartTransactionCommand();
		SPI_connect();
		PushActiveSnapshot(GetTransactionSnapshot());

		/* Execute NOTIFY requests */
		int ret = SPI_execute(buf.data, false, 0);
		if (ret != SPI_OK_SELECT)
			elog(FATAL, "pgotel_worker: SPI_execute failed with error code %d", ret);
		// elog(LOG, "pgotel_worker: executed " UINT64_FORMAT, SPI_processed);

		/* Terminate transaction */
		SPI_finish();
		PopActiveSnapshot();

		/* Notifications are sent by the transaction commit */
		CommitTransactionCommand();

		pgstat_report_activity(STATE_IDLE, NULL);
	}

	/* No problems, so clean exit */
	pgotel::CleanupMetrics();
	proc_exit(0);
}

static void
pgotel_register_worker()
{
	BackgroundWorker worker;

	/* Worker parameter and registration */
	MemSet(&worker, 0, sizeof(BackgroundWorker));
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	snprintf(worker.bgw_library_name, BGW_MAXLEN, "pgotel");
	snprintf(worker.bgw_function_name, BGW_MAXLEN, "pgotel_worker_main");
	snprintf(worker.bgw_name, BGW_MAXLEN, "%s", "PGOTEL Worker");

	/* Wait 10 seconds for restart before crash */
	worker.bgw_restart_time = 10;
	worker.bgw_main_arg = (Datum) 0;
	worker.bgw_notify_pid = 0;
	RegisterBackgroundWorker(&worker);
}

/*
 * _PG_init
 * Entry point loading hooks
 */
void
_PG_init(void)
{
	pgotel_define_gucs();
	pgotel_register_worker();
}

/* declaration out of file scope */
StaticAssertDecl(true, "declaration assert test");

#ifdef __cplusplus
}
#endif
