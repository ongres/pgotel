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
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/meter.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"

#include "opentelemetry/sdk/metrics/metric_reader.h"

using namespace std;

namespace metric_sdk = opentelemetry::sdk::metrics;
namespace common = opentelemetry::common;
namespace metrics_api = opentelemetry::metrics;
namespace otlp_exporter = opentelemetry::exporter::otlp;

namespace pgotel
{
	otlp_exporter::OtlpGrpcMetricExporterOptions options;
	std::unique_ptr<metric_sdk::MetricReader> reader;

	void
	InitMetrics(std::string endpoint, std::chrono::milliseconds interval,
				std::chrono::milliseconds timeout)
	{
		pgotel::options.endpoint = endpoint;
		auto exporter = otlp_exporter::OtlpGrpcMetricExporterFactory::Create(options);

		std::string version{ "1.2.0" };
		std::string schema{ "https://opentelemetry.io/schemas/1.2.0" };

		// Initialize and set the global MeterProvider
		metric_sdk::PeriodicExportingMetricReaderOptions reader_options;
		reader_options.export_interval_millis = std::chrono::milliseconds(interval);
		reader_options.export_timeout_millis = std::chrono::milliseconds(timeout);

		reader = metric_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter),
																		  reader_options);

		auto context = metric_sdk::MeterContextFactory::Create();
		context->AddMetricReader(std::move(reader));

		auto u_provider = metric_sdk::MeterProviderFactory::Create(std::move(context));
		std::shared_ptr<opentelemetry::metrics::MeterProvider> provider(std::move(u_provider));

		metrics_api::Provider::SetMeterProvider(provider);
	}

	void
	CleanupMetrics()
	{
		std::shared_ptr<metrics_api::MeterProvider> none;
		metrics_api::Provider::SetMeterProvider(none);
	}

	void
	RestartMetrics(std::string endpoint, std::chrono::milliseconds interval,
				   std::chrono::milliseconds timeout)
	{
		CleanupMetrics();
		InitMetrics(endpoint, interval, timeout);
	}

	void
	counter(const std::string &name, double value, std::map<std::string, std::string> labels)
	{
		std::string counter_name = "counter." + name;
		auto provider = metrics_api::Provider::GetMeterProvider();
		opentelemetry::nostd::shared_ptr<metrics_api::Meter> meter =
			provider->GetMeter(name, "1.2.0");
		auto double_counter = meter->CreateDoubleCounter(counter_name);
		auto labelkv = common::KeyValueIterableView<decltype(labels)>{ labels };
		double_counter->Add(value, labelkv);
	}

} // namespace pgotel

#ifdef __cplusplus
extern "C"
{
#endif
#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/jsonb.h"
#include "storage/ipc.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pgotel_counter);

void _PG_init(void);

/* GUC variables */
static bool pgotel_enabled = false;
static int pgotel_interval = 2000;
static int pgotel_timeout = 500;
static char *pgotel_endpoint = NULL;

Datum
pgotel_counter(PG_FUNCTION_ARGS)
{
	char *counter_name = PG_ARGISNULL(0) ? NULL : text_to_cstring(PG_GETARG_TEXT_PP(0));
	float8 value = PG_ARGISNULL(0) ? -1 : PG_GETARG_FLOAT8(1);
	Jsonb *labels = PG_ARGISNULL(2) ? NULL : PG_GETARG_JSONB_P(2);
	std::map<std::string, std::string> labels_map;

	if (!counter_name)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("counter name cannot be NULL")));

	if (value < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("counter value cannot be negative")));

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

	pgotel::counter(counter_name, value, labels_map);

	PG_RETURN_NULL();
}

static void
pgotel_assign_interval_guc(int newval, void *extra)
{
	elog(DEBUG1, "pgotel_interval: %d", newval);
	elog(DEBUG1, "pgotel_timeout: %d", pgotel_timeout);

	pgotel::RestartMetrics(pgotel_endpoint,
						   std::chrono::milliseconds(newval),
						   std::chrono::milliseconds(pgotel_timeout));
}

static void
pgotel_assign_timeout_guc(int newval, void *extra)
{
	elog(DEBUG1, "pgotel_interval: %d", pgotel_interval);
	elog(DEBUG1, "pgotel_timeout: %d", newval);

	pgotel::RestartMetrics(pgotel_endpoint,
						   std::chrono::milliseconds(pgotel_interval),
						   std::chrono::milliseconds(newval));
}

static void
load_params(void)
{
	DefineCustomStringVariable("pgotel.endpoint",
							   "Endpoint of the OTEL-Collector",
							   NULL,
							   &pgotel_endpoint,
							   "localhost:4317",
							   PGC_USERSET,
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
							PGC_USERSET,
							0,
							NULL,
							pgotel_assign_interval_guc,
							NULL);

	DefineCustomIntVariable("pgotel.timeout",
							"Timeout to send metrics to OTEL-Collector",
							NULL,
							&pgotel_timeout,
							500,
							100,
							INT_MAX,
							PGC_USERSET,
							0,
							NULL,
							pgotel_assign_timeout_guc,
							NULL);
}

/*
 * _PG_init
 * Entry point loading hooks
 */
void
_PG_init(void)
{
	load_params();
	elog(DEBUG1, "endpoint: %s", pgotel_endpoint);
	pgotel::InitMetrics(pgotel_endpoint,
						std::chrono::milliseconds(pgotel_interval),
						std::chrono::milliseconds(pgotel_timeout));
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
