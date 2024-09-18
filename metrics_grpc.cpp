/*-------------------------------------------------------------------------
 *
 * metrics_grpc.cc
 *		OpenTelemetry metrics implementation using gRPC
 *
 * Copyright (c) 1996-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pgotel/metrics_grpc.cpp
 *
 *-------------------------------------------------------------------------
 */

#include "metrics_grpc.h"

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
	Counter(const std::string &name, double value, std::map<std::string, std::string> labels)
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
