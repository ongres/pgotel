\echo Use "CREATE EXTENSION pgotel" to load this file. \quit

-- CREATE FUNCTION pgotel_log(endpoint TEXT, message TEXT)
-- RETURNS text
-- AS 'MODULE_PATHNAME'
-- LANGUAGE C;

CREATE FUNCTION pgotel_counter(counter_name TEXT, value FLOAT8)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;
