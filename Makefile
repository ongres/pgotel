MODULE_big = pg_otlp

EXTENSION = pg_otlp
#CXX_SRCS = pg_otlp.cc
DATA = pg_otlp--1.0.sql
OBJS = pg_otlp.o
PGFILEDESC = "pg_otlp - Minimal extension template in C++"

SHLIB_LINK := $(LDFLAGS) \
	-lopentelemetry_trace \
	-lopentelemetry_common \
	-lopentelemetry_otlp_recordable \
	-lopentelemetry_resources \
	-lopentelemetry_exporter_otlp_grpc

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
