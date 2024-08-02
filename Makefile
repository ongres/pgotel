MODULE_big = pgotel

EXTENSION = pgotel

DATA = pgotel--1.0.sql
OBJS = pgotel.o
PGFILEDESC = "pgotel - POC to interact with OpenTelemetry"

SHLIB_LINK := $(LDFLAGS) \
	-lopentelemetry_common \
	-lopentelemetry_otlp_recordable \
	-lopentelemetry_metrics \
	-lopentelemetry_exporter_otlp_grpc_metrics

PG_CPPFLAGS = -I/usr/local/include

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
