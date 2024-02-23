# Postgres Open Telemetry Extension

The `pgotel` project is a Postgres extension that allows other extensions to export observability signals (metrics, traces, logs, etc) with [Open Telemetry](https://opentelemetry.io/). `pgotel` includes and exposes in a more Postgres-way (and in plain C, not C++) the [Open Telemetry C++ SDK](https://github.com/open-telemetry/opentelemetry-cpp).

## Protocol considerations

The [OpenTelemetry Protocol](https://opentelemetry.io/docs/specs/otlp/) (OTLP in short) defines two possible transports: gRPC and HTTP (supporting both 1.1 and 2.0 for the latter). The HTTP transport was added to OTLP after the gRPC transport with one of the main motivations being that "_gRPC is a relatively big dependency, which some clients are not willing to take. Plain HTTP is a smaller dependency and is built in the standard libraries of many programming languages_" ([source](https://github.com/open-telemetry/oteps/blob/main/text/0099-otlp-http.md)). For this reason, only the HTTP transport has been chosen to be implemented into this extension.

