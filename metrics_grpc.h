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

#pragma once

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
	void InitMetrics(std::string endpoint, std::chrono::milliseconds interval,
					 std::chrono::milliseconds timeout);

	void CleanupMetrics();

	void RestartMetrics(std::string endpoint, std::chrono::milliseconds interval,
						std::chrono::milliseconds timeout);
	void Counter(const std::string &name, double value, std::map<std::string, std::string> labels);
} // namespace pgotel