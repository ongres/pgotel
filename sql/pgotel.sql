CREATE EXTENSION pgotel;

SELECT pgotel_counter('foo', -1);
SELECT pgotel_counter('foo', 0, '{}');
SELECT pgotel_counter('foo', 0, NULL);
SELECT pgotel_counter('foo', NULL);
SELECT pgotel_counter(NULL, 0);
SELECT pgotel_counter(NULL, NULL);
SELECT pgotel_counter('foo', 0, '{"dbname": "foo", "schemaname": "bar", "relname": "baz"}');