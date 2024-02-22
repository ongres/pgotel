/*-------------------------------------------------------------------------
 *
 * pg_otlp.cc
 *		POC to interact with OpenTelemetry
 *
 * Copyright (c) 1996-2021, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_otlp/pg_otlp.cpp
 *
 *-------------------------------------------------------------------------
 */

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/logs/logger.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/logger_context_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"

using namespace std;
namespace nostd     = opentelemetry::nostd;
namespace otlp      = opentelemetry::exporter::otlp;
namespace logs      = opentelemetry::logs;
namespace logs_sdk  = opentelemetry::sdk::logs;

// good example: https://github.com/open-telemetry/opentelemetry-cpp-contrib/blob/main/exporters/fluentd/example/log/main.cc
namespace
{
	opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOptions _log_opts;
	// nostd::shared_ptr<logs::Logger> _logger;

	void InitLogger()
	{
		auto exporter  = otlp::OtlpGrpcLogRecordExporterFactory::Create(_log_opts);
		auto processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
	
		std::vector<std::unique_ptr<logs_sdk::LogRecordProcessor>> processors;
		processors.push_back(std::move(processor));
	
		auto context = logs_sdk::LoggerContextFactory::Create(std::move(processors));
		std::shared_ptr<logs::LoggerProvider> provider = logs_sdk::LoggerProviderFactory::Create(std::move(context));
	
		opentelemetry::logs::Provider::SetLoggerProvider(provider);
	}

	void Log(nostd::string_view message)
	{
		auto provider = logs::Provider::GetLoggerProvider();
		auto logger = provider->GetLogger("pg_otlp_logger", "pg_otlp", OPENTELEMETRY_SDK_VERSION);
		logger->Debug(message);
	}

}  // namespace


#ifdef __cplusplus
extern "C" {
#endif
#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "storage/ipc.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_otlp);

void _PG_init(void);
void _PG_fini(void);

/* Hold previous logging hook */
static emit_log_hook_type prev_log_hook = NULL;

/* GUC variables */ 
static bool pg_otlp_enabled = false;

/*
 * send_log_to_otel_collector
 * offloading postgres logs to OTEL-Collector
 */
static void
send_log_to_otel_collector(ErrorData *edata)
{
	if (!pg_otlp_enabled)
		return;

	Log(edata->message);
}

Datum
pg_otlp(PG_FUNCTION_ARGS)
{
	char *endpoint = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char *message = text_to_cstring(PG_GETARG_TEXT_PP(1));
	elog(NOTICE, "Endpoint: %s", endpoint);
	elog(NOTICE, "Message: %s", message);

	_log_opts.endpoint = endpoint;

	Log(message);

	PG_RETURN_NULL();
}

static void
load_params(void)
{
	/* Enable/Disable */
	DefineCustomBoolVariable("pg_otlp.enabled",
							 "Enable/Disable Send Logs to OTEL-Collector",
							NULL,
							&pg_otlp_enabled,
							false,
							PGC_SIGHUP,
							0, NULL, NULL, NULL);
}

/*
 * _PG_init
 * Entry point loading hooks
 */
void
_PG_init(void)
{
	load_params();
	_log_opts.endpoint = "localhost:4317";

	InitLogger();

	prev_log_hook = emit_log_hook;
	emit_log_hook = send_log_to_otel_collector;

	// on_proc_exit(cleanup, 0);
}

/*
 * _PG_fini
 * Exit point unloading hooks
 */
void
_PG_fini(void)
{
	emit_log_hook = prev_log_hook;
	// cleanup(0, 0);
}

/* declaration out of file scope */
StaticAssertDecl(true, "declaration assert test");

#ifdef __cplusplus
}
#endif
