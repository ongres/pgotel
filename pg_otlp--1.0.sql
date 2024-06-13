\echo Use "CREATE EXTENSION pg_otlp" to load this file. \quit

CREATE FUNCTION pg_otlp_log(url TEXT, message TEXT)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;
