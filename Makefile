MODULE_big = pgotel

EXTENSION = pgotel

DATA = pgotel--1.0.sql
OBJS = pgotel.o
PGFILEDESC = "pgotel - POC to interact with OpenTelemetry"

SHLIB_LINK := $(LDFLAGS) \
	-lopentelemetry_common \
	-lopentelemetry_logs \
	-lopentelemetry_otlp_recordable \
	-lopentelemetry_resources \
	-lopentelemetry_exporter_otlp_grpc_log

PG_CPPFLAGS = -I/usr/local/include

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

format:
	find . -regex '.*\.\(c\|h\|cpp\)' | xargs clang-format -i

.PHONY: format