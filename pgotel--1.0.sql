\echo Use "CREATE EXTENSION pgotel" to load this file. \quit

CREATE FUNCTION pgotel_counter(counter_name TEXT, value FLOAT8)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C;

-- { "dbname": "foo", "schemaname": "bar", "relname": "baz" }
CREATE FUNCTION pgotel_counter(counter_name TEXT, value FLOAT8, labels JSONB)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C;
