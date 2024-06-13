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
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"

#define LOGGER_NAME "pg_otlp_logger"
#define LOGGER_LIBRARY_NAME "pg_otlp_logger"
#define LOGGER_LIBRARY_VERSION "1.0"

using namespace std;
namespace nostd     = opentelemetry::nostd;
namespace otlp      = opentelemetry::exporter::otlp;
namespace logs      = opentelemetry::logs;
namespace logs_sdk  = opentelemetry::sdk::logs;

// good example: https://github.com/open-telemetry/opentelemetry-cpp-contrib/blob/main/exporters/fluentd/example/log/main.cc
namespace pg_otlp
{
	opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions options;
	nostd::shared_ptr<logs::Logger> _logger;

	void InitLogger()
	{
		auto exporter  = otlp::OtlpHttpLogRecordExporterFactory::Create(options);
		auto processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
	
		std::shared_ptr<logs::LoggerProvider> provider =
      		logs_sdk::LoggerProviderFactory::Create(std::move(processor));

		opentelemetry::logs::Provider::SetLoggerProvider(provider);

		_logger = provider->GetLogger(LOGGER_NAME, LOGGER_LIBRARY_NAME, LOGGER_LIBRARY_VERSION);
	}

	void Log(nostd::string_view message)
	{
		_logger->Log(logs::Severity::kInfo, message);

		_logger->Enabled
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

PG_FUNCTION_INFO_V1(pg_otlp_log);

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

	pg_otlp::Log(edata->message);
}

Datum
pg_otlp_log(PG_FUNCTION_ARGS)
{
	char *url = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char *message = text_to_cstring(PG_GETARG_TEXT_PP(1));

	pg_otlp::options.url = url;
	pg_otlp::Log(message);

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
	pg_otlp::options.url = "localhost:4318";

	pg_otlp::InitLogger();

	prev_log_hook = emit_log_hook;
	emit_log_hook = send_log_to_otel_collector;
}

/*
 * _PG_fini
 * Exit point unloading hooks
 */
void
_PG_fini(void)
{
	emit_log_hook = prev_log_hook;
}

/* declaration out of file scope */
StaticAssertDecl(true, "declaration assert test");

#ifdef __cplusplus
}
#endif
