MODULE_big = pgotel

EXTENSION = pgotel

REGRESS = pgotel
REGRESS_OPTS = --temp-instance=tmp_check

DATA = pgotel--1.0.sql
OBJS = metrics_grpc.o pgotel.o
PGFILEDESC = "pgotel - POC to interact with OpenTelemetry"

SHLIB_LINK := $(LDFLAGS) \
	-lopentelemetry_common \
	-lopentelemetry_metrics \
	-lopentelemetry_resources \
	-lopentelemetry_otlp_recordable \
	-lopentelemetry_exporter_otlp_grpc_metrics

PG_CPPFLAGS = -I/usr/local/include

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

format:
	find . -regex '.*\.\(c\|h\|cpp\)' | xargs clang-format -i

.PHONY: format