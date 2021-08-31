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

#include "opentelemetry/exporters/otlp/otlp_grpc_exporter.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/provider.h"

namespace trace    = opentelemetry::trace;
namespace nostd    = opentelemetry::nostd;
namespace sdktrace = opentelemetry::sdk::trace;
namespace otlp     = opentelemetry::exporter::otlp;

namespace
{
	opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
	
	nostd::shared_ptr<trace::Tracer> get_tracer()
	{
  		auto provider = trace::Provider::GetTracerProvider();
  		return provider->GetTracer("pg_otlp");
	}

	void InitTracer()
	{
		// Create OTLP exporter instance
		auto exporter  = std::unique_ptr<sdktrace::SpanExporter>(new otlp::OtlpGrpcExporter(opts));
		auto processor = std::unique_ptr<sdktrace::SpanProcessor>(
			new sdktrace::SimpleSpanProcessor(std::move(exporter)));
		auto provider =
			nostd::shared_ptr<trace::TracerProvider>(new sdktrace::TracerProvider(std::move(processor)));
		// Set the global trace provider
		trace::Provider::SetTracerProvider(provider);
	}
}  // namespace


#ifdef __cplusplus
extern "C" {
#endif
#include "postgres.h"
#include "fmgr.h"

#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_otlp);

Datum
pg_otlp(PG_FUNCTION_ARGS)
{
	char *endpoint = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char *spanname =  text_to_cstring(PG_GETARG_TEXT_PP(1));
	elog(NOTICE, "Endpoint: %s", endpoint);

	opts.endpoint = endpoint;

	InitTracer();

	auto scoped_span = trace::Scope(get_tracer()->StartSpan(spanname));

	PG_RETURN_NULL();
}

/* declaration out of file scope */
StaticAssertDecl(true, "declaration assert test");

#ifdef __cplusplus
}
#endif
