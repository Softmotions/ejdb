# JQL `order by` path ending in `~` triggers a heap out-of-bounds read (via iowow)

## Summary

A JQL query whose `order by` path value ends in `~` (e.g. `| asc /~`) makes the
JQL layer build a JSON-pointer path `"/" + value` ending in `~` and hand it to
iowow's JSON-pointer parser (`jbl_ptr_alloc_pool()` -> `_jbl_ptr_pool()`). There
the RFC 6901 `~0`/`~1` decoder advanced its index unconditionally, stepped over
the string's NUL terminator and read one byte past the heap buffer (CWE-125).

```
/a |asc /~            (10-byte query)
READ of size 1
    #0 _jbl_ptr_pool   iowow src/json/iwjson.c:1116
    #1 _jqp_finish     ejdb  src/jql/inc/jqpx.c:961
    #2 yy_6_QUERY      ejdb  src/jql/jqp.leg:86
    #3 yyparsefrom_r / yyparsefrom
SUMMARY: AddressSanitizer: heap-buffer-overflow src/json/iwjson.c in _jbl_ptr_pool
```

## Fix

The defect and its fix live in iowow (`_jbl_ptr_pool()`); see the companion
iowow change. This repository gains a regression input so the JQL path is
exercised: `src/jql/tests/data/023.jql` (`/a |asc /~`) with expected output
`023.expected.jql` (`/a\n| asc /~`).

- Affected: EJDB2 `9edee710` (`v2.93-dev`) with iowow `v1.6.1` (unfixed).
- Not affected once iowow carries the `_jbl_ptr_pool()` index fix.
