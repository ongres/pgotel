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

#include "opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/metrics/provider.h"

#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"

#include "opentelemetry/sdk/metrics/push_metric_exporter.h"

using namespace std;

namespace metric_sdk    = opentelemetry::sdk::metrics;
namespace common        = opentelemetry::common;
namespace metrics_api   = opentelemetry::metrics;
namespace otlp_exporter = opentelemetry::exporter::otlp;

namespace pgotel
{
	otlp_exporter::OtlpGrpcMetricExporterOptions options;

	void InitMetrics()
	{
		auto exporter = otlp_exporter::OtlpGrpcMetricExporterFactory::Create(options);

		std::string version{"1.2.0"};
		std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

		// Initialize and set the global MeterProvider
		metric_sdk::PeriodicExportingMetricReaderOptions reader_options;
		reader_options.export_interval_millis = std::chrono::milliseconds(2000);
		reader_options.export_timeout_millis  = std::chrono::milliseconds(500);

		auto reader =
			metric_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), reader_options);

		auto context = metric_sdk::MeterContextFactory::Create();
		context->AddMetricReader(std::move(reader));

		auto u_provider = metric_sdk::MeterProviderFactory::Create(std::move(context));
		std::shared_ptr<opentelemetry::metrics::MeterProvider> provider(std::move(u_provider));

		metrics_api::Provider::SetMeterProvider(provider);
	}

	void CleanupMetrics()
	{
		std::shared_ptr<metrics_api::MeterProvider> none;
		metrics_api::Provider::SetMeterProvider(none);
	}

	void counter(const std::string &name, double value)
	{
		std::string counter_name = name + "_counter";
		auto provider            = metrics_api::Provider::GetMeterProvider();
		opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter = provider->GetMeter(name, "1.2.0");
		auto double_counter = meter->CreateDoubleCounter(counter_name);
		double_counter->Add(value);
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

PG_FUNCTION_INFO_V1(pgotel_counter);

void _PG_init(void);

/* GUC variables */ 
static bool pgotel_enabled = false;

Datum
pgotel_counter(PG_FUNCTION_ARGS)
{
	char *endpoint = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char *counter_name = text_to_cstring(PG_GETARG_TEXT_PP(1));
	float8 value = PG_GETARG_FLOAT8(2);
	elog(DEBUG1, "Endpoint: %s", endpoint);
	elog(DEBUG1, "Counter name: %s", counter_name);
	elog(DEBUG1, "Value: %f", value);

	pgotel::options.endpoint = endpoint;
	pgotel::counter(counter_name, value);

	PG_RETURN_NULL();
}

static void
load_params(void)
{
	/* Enable/Disable */
	DefineCustomBoolVariable("pgotel.enabled",
							 "Enable/Disable Send Logs to OTEL-Collector",
							NULL,
							&pgotel_enabled,
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
	pgotel::InitMetrics();
}

/*
 * _PG_fini
 * Exit point unloading hooks
 */
void
_PG_fini(void)
{
	pgotel::CleanupMetrics();
}

/* declaration out of file scope */
StaticAssertDecl(true, "declaration assert test");

#ifdef __cplusplus
}
#endif
