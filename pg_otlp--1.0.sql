\echo Use "CREATE EXTENSION pg_otlp" to load this file. \quit

CREATE FUNCTION pg_otlp(endpoint TEXT, spanname TEXT)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;
