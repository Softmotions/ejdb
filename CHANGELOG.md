# [v2.99] - 2026-11-13

- Updated to iwnet v1.6.2
- Autark: Updated to Autark 0.10.0.
- Autark: External projects used by the build are kept in a build wide store shared by the whole build tree,
  so a resource requested by several build units is fetched only once.
- Autark: Added the `autark fetch-plan` command, `.autark/fetch_resource.sh` resolves the actual fetch dir with it.
- Autark: Installed files and directories are made readable by everyone and installed executables are made
  executable by everyone. The `install exact` mode preserves source permissions as is.

# [v2.98] - 2026-11-07

- Updated to iwnet v1.6.0
- `Changelog` migrated to the Markdown `CHANGELOG.md` format used by autark.dev projects.
- Autark: `CHANGELOG.md` is processed by `changelog_md.sh`.
- scripts/git-release.sh: Release notes are extracted from the Markdown section of `CHANGELOG.md`.

# [v2.97] - 2026-11-04

- Updated to iwnet v1.5.0

# [v2.96] - 2026-10-30

- Updated to iwnet v1.4.2

# [v2.95] - 2026-10-22

- Updated to iwnet v1.4.1: Fix of bad pthread spinlock detection

# [v2.94] - 2026-10-20

- ejdb2_android: Modernized and tested.
- Fixed broken ejdb.org link in project sources

# [v2.93] - 2026-10-20

- ejdb2_node,ejdb2_jni: Modernized and tested.
- Migrated to iowow v1.7.0, iwnet v1.4.0
- iwtests.h: Migrated to iowow test framework.
- Fixed a lot of memory/resources leaks in failed execution paths.

# [v2.92] - 2026-10-11

- tests: Added basic fuzzing tests.
- Refacroring: Struct type aliases are replaced by unaliased `struct` type clauses.
- jql.c: Queries like `/[name = Anton and not age = 20]` where different object fields are in brackets are properly matched.
- jql.c: Fixed incorrect query result when negation expressions are used in queries like: `/name and not /age`
- Autark: peg-0.1.19 is now included in source tree
- src/jql/inc/jqpx.c: Fixed UB in _jqp_unescape_json_string()
- Autark: Tested and fixed  -DENABLE_ASAN=1 -DENABLE_UBSAN=1 modes.
- Autark: Project versions now track Changelog.
- src/Autark: Autark shared builds on macOS - #395

# [v2.91] - 2026-09-08

- build.sh: updated (critical fixes)

# [v2.90] - 2026-08-30

- Adapted to iwnet v1.3.0

# [v2.81] - 2026-07-16

- jbi: fix 1-byte heap buffer overflow in jbi_node_expr_matched() #394

# [v2.80] - 2025-07-19

- Migration to Autark build system.
- Project cleanup. Only java and nodejs bindings are maintained.

# [v2.74] - 2022-11-17

- Naming refactoring.
- ejdb_swift, ejdb_react_native, ejdb_android, ejdb_dart, ejdb_flutter
- Updated to the current iowow API. Naming refactoring.
- Fix:JQL Incorrect floating point match against integers (#380)
- Fix: Incorrect parsing of JQL JSON with floats in scientific format (#378)
- Fix: windows build errors
- Fix: Updated iwnet fixed wrong handling of fs error events (iwn_poller)
- Chore: BearSSL updated to the latest version.
- Fix: Delete ejdb crash as realloc(): invalid next size #366

# [v2.73] - 2022-11-14

- Fixed ERR_STREAM_PREMATURE_CLOSE on Node 18 (#346)
- Resolved security issuies with nodejs packages
- ejdb2 json module migrated to iowow
- Upgraded to the stable verion of iwnet lib.
- Updated EJDB2Swift submodule
- Replaced old iowow regexp engine with new (iwre.h) based on https://github.com/jserv/cregex

# [v2.72] - 2022-02-23

- Upgraded to the stable verion of iwnet lib.

# [v2.71] - 2022-02-18

- Fixed wrong format of printf like function calls.
- Query placeholders with the same name can be specified multiply times.
- Query placeholders API is supported in projections.
- Removed potential memory leaks in `jql_set_xx` query API (jql.h)
- Added BearSSL include files as part of ejdb2 distribution

# [v2.70] - 2022-02-14

- Network API is now powered by IWNET https://github.com/Softmotions/iwnet
- Correct handling of `vsnprintf` return value. (jbl.c)
- Moved `lwre` regexp library into `iowow` project.
- Java: Added JSON.clone() method.
- Java: Added JSON ObjectBuilder.move() method.

# [v2.62] - 2021-10-26

- Fixed incorrect query results on array values (#331)
- Fixed error in parsing FP JSON value (#333)
- Java: com.softmotions.ejdb2.JSON.ValueType.getTypeOf set to be public
- Flutter: Fixed #305
- Java: com.softmotions.ejdb2.JSON typo fixed
- Java: com.softmotions.ejdb2.JSON added Comparable support
- Java: com.softmotions.ejdb2.JSON added better JSON.equals() implementation

# [v2.61] - 2021-06-22

- Fixed incorrect arguments check in `jbn_merge_patch`

# [v2.0.60] - 2021-05-10

- Upgraded to iowow v1.4.11
- Fixed Macos build error (#313)
- Added typescript generic to JBDOC in node binding (#311)
- Removed -fvisibility=hidden compiler flag from facil.io lib

# [v2.0.59] - 2021-03-21

- Fixed some uninitialized data issues with `jbl_clone()`
- Added ejdb_put_new_jbn() (ejdb2.h)
- jbs: Default database file name changed from db.jb to ejdb2.db
- Added jbl_from_json_printf_va(), jbn_from_json_printf_va() (jbl.h)
- Project code reformatted using uncrustify
- Updated copyright headers
- Removed dependency on some glibc specific features
- Added jbl_object_copy_to() (ejdb2.h)
- Added new convenient methods (ejdb2.h)

# [v2.0.58] - 2020-12-22

- New versioning scheme is used for ejdb2 bindings.
- Fixed pub publish issues with dart and flutter bindings.
- Upgraded to iwkv v1.4.10

# [v2.0.57] - 2020-12-17

- Added new non standard JSON `swap` operation.
- bugfix: Incorrect behavior of JSON patch `move` operation.
- Added --trylock option to jbs
- jbs server CLI should be able to load access token from file (#219)
- bugfix: Updated to iowow v1.4.10 with fix https://github.com/Softmotions/iowow/issues/35
- enhancement: EJDB2 Java binding improvements.

# [v2.0.56] - 2020-12-10

- EJDB2 Java binding improvements.
- Added EJDB2 Maven example project and maven repository deployment.

# [v2.0.55] - 2020-12-06

- EJDB2 Java binding now in Ubuntu ppa:adamansky/ejdb2
- Build scripts refactoring.
- Reduced number of WS server threads to half of available CPU Cores.

# [v2.0.54] - 2020-11-19

- CRITICAL: Fixed incorrect behavior of `_jbl_cmp_atomic_values`
- Transfered `-DASAN` to `AddIOWOW` CMake module.

# [v2.0.53] - 2020-11-18

- Upgraded to iowow v1.4.9
- Eliminate duplicated documents from non-inique index scanning
- Add prefix matching operator ~ (#292)

# [v2.0.52] - 2020-11-08

- Fixed serios bug concerning non-unique indexes, details: (#291)
- Dart binding: support of the latest Dart pub
- Added `jbn_add_item_null()` (jbl.h)
- SIGSEGV, Segmentation fault (#287)
- Upgraded to facil.io 0.7.5
- Code cleanup

# [v2.0.51] - 2020-09-21

- Upgraded to iowow v1.4.7
- Added `jbl_from_json_printf()` and `jbn_from_json_printf()` (jbl.h)
- Added JQL `upsert` operation
- Typo fixed (jbl.c)
- `jbl_get_str()` now returns `const char*` (jbl.h)
- Added `jbl_set_user_data()` (jbl.h)
- Removed disable int compression feature from binn implementation (binn.c)

# [v2.0.50] - 2020-06-24

- Better floating point numbers comparison in `jbn_compare_nodes()` (jbl.h)
- Fixed error in `jbn_copy_path()` if some source property doesn't exist (jbl.h)
- Added `jbn_length()` (jbl.h)
- Added `lwre.h` to set of public headers
- Added `node_out` for every `jbn_add_item_X()`  (jbl.h)
- Fixed compilation error on clang-10
- Fixed ejdb2_jni to be java-8 compatible
- Fixed ejdb2_jni to be headless JDK compatible

# [v2.0.49] - 2020-05-17

- Added ability to specify array of primary key values in `/=:?` query clause.

# [v2.0.48] - 2020-05-15

- Implemented collection JOINs (#280)

# [v2.0.47] - 2020-05-08

- Added ability to specify constraints on internal primary key in query (#281)
- Added `jbn_copy_paths` (jbl.h)

# [v2.0.46] - 2020-05-02

- Upgraded to iowow v1.4.4
- Added `jbn_paths_compare()` (jbl.h)
- Added `no_src_clone` parameter `jbn_copy_path` (jbl.h)

# [v2.0.45] - 2020-04-24

- Fixed errors found by PVS studio
- Added two variants of `jbn_detach` (jbl.h)
- Added non standard JSON patch operation: `add_create` (jbl.h)
- Added `jbl_from_node`    (jbl.h)
- Added `jbn_copy_path`    (jbl.h)
- Added `jbn_add_item_obj` (jbl.h)
- Added `jbn_add_item_arr` (jbl.h)
- Minor refactoring of jbl module
- Added a set of jbn_add_x (jbl.h)

# [v2.0.44] - 2020-04-20

- Fixed missing parent pointer link in `_jbl_add_item` (jbl_json.c).
- Added `jbn_clone()` (jbl.h)
- Added checks for max nesting level of JSON objects, 1000 by default.
- Added `jbl_clone(JBL src, JBL *targetp)` (jbl.h)
- Added `jbl_merge_patch_jbl(JBL jbl, JBL patch)` (jbl.h)
- Added `jbl_set_string_printf()` (jbl.h)
- Added `jbl_structure_size()` (jbl.h)
- Better error handling of `kh_put()`
- Added `jbl_clone_into_pool(JBL src, IWPOOL *pool, JBL *targetp)` (jbl.h)
- Added  jbn_path_compare, jbn_path_compare_str, jbn_path_compare_bool,

# [v2.0.43] - 2020-03-07

- Upgraded to iowow v1.4.1

# [v2.0.42] - 2020-03-07

- Upgraded to iowow v1.4.0

# [v2.0.41] - 2020-02-18

- Fixed race condition on database open on slow devices

# [v2.0.40] - 2020-02-17

- Added `jbl_object_get_i64`, `jbl_object_get_f64`,
- Minor fixes in index selection rules
- Added `jbl_type_t jbl_object_get_type(JBL jbl, const char *key)` (jbl.h)
- Upgraded to iowow 1.3.37

# [v2.0.39] - 2020-01-22

- Added iwrc jbn_at(JBL_NODE node, const char *path, JBL_NODE *res)
- Added iwrc jbn_at2(JBL_NODE node, JBL_PTR jp, JBL_NODE *res)
- Added `jbl_set_int64`, `jbl_set_f64`, `jbl_set_string`, `jbl_set_nested` (jbl.h)
- Added `jbl_set_bool`, `jbl_set_null` (jbl.h)
- Added `jbl_set_empty_array`, `jbl_set_empty_object` (jbl.h)
- Safer `jql_destroy` (jql.h)
- Added `jql_set_json_jbl` (jql.h)
- Added `ejdb_count` (ejdb2.h)
- Fixed `_jbl_create_patch` issues

# [v2.0.38] - 2020-01-17

- FIXED: In the case of apply/projection `ejdb_list` returned old (not updated) documents
- Added `iwrc ejdb_get_iwkv(EJDB db, IWKV *kvp)` (ejdb2.h)
- Added non standart JSON patch extension: `increment`

# [v2.0.37] - 2020-01-15

- Upgraded to iowow v1.3.35
- Fixed wal related durability issue

# [v2.0.36] - 2020-01-06

- Implemented Swift language binding #267
- Ported to iOS
- Upgraded to iowow 1.3.34
- Upgraded to facil.io 0.7.4
- Improved ejdb2.h documentation

# [v2.0.35] - 2019-12-10

- FIXED: JQL apply ignores value set by placeholder (#269)

# [v2.0.34] - 2019-12-05

- Reduced library size by 400K: stripped off unused utf8proc data

# [v2.0.33] - 2019-12-04

- Added ejdb_merge_or_put atomic method #268
- Added getOrNull to node/react bindings

# [v2.0.32] - 2019-12-04

- Upgraded to iowow v1.3.31

# [v2.0.31] - 2019-11-19

- Upgraded to iowow v1.3.30

# [v2.0.30] - 2019-11-15

- Upgraded to iowow v1.3.29

# [v2.0.29] - 2019-11-04

- Upgraded to iowow v1.3.27
- React Native binding implemented (Android)

# [v2.0.28] - 2019-09-27

- Upgraded to facil.io v0.7.3
- JBS server functions removed from ejdb2 target libs

# [v2.0.27] - 2019-08-29

- Upgraded to iowow v1.3.25 with critical fixes

# [v2.0.26] - 2019-08-22

- Upgraded to iowow v1.3.24

# [v2.0.25] - 2019-08-17

- ejdb2_dart: Fixed memory leak in dart binding
- Upgraded to iowow 1.3.23

# [v2.0.24] - 2019-07-25

- Strip shared libraries for release builds.

# [v2.0.23] - 2019-07-19

- BUG: Fixed incorrect behavior of `jbi_ftoa`

# [v2.0.22] - 2019-07-18

- Limited windows support without HTTP/Websocket network API (#237)
- `static_assert` is  set to `_Static_assert` if not defined
- All uses of `iwftoa` replaced by `jbi_ftoa`

# [v2.0.21] - 2019-07-12

- Added `ejdb_rename_collection` method  (#254)

# [v2.0.20] - 2019-07-03

- Node.js binding (#248)
- Added git revision hash getter `ejdb_git_revision()` (ejdb2.h)
- Better error reporting on failed memory allocations

# [v2.0.19] - 2019-06-13

- Upgraded to iowow v1.3.20 with critical fixes

# [v2.0.18] - 2019-06-12

- Limit one time file allocation step to 2G iowow v1.3.18
- Added Docker image (#249)
- Better qsort_t detection, build ok with `musl`

# [v2.0.17] - 2019-06-05

- Added `inverse` JQL query option.

# [v2.0.16] - 2019-06-05

- Allowed dash and underscope chars in collection names specified in query: `[@collection_name]/query`.
- Fixed minor typo in ejdb2_jni/src/CMakeLists.txt

# [v2.0.15] - 2019-05-27

- Security: a heap-overflow vulnerability was fixed in the WebSocket parser of facil.io library

# [v2.0.14] - 2019-05-17

- Android support
- Test cases for Android JNI binding

# [v2.0.13] - 2019-05-03

- Upgraded to iowow_1.3.17 with critical fixes

# [v2.0.12] - 2019-05-03

- Upgraded to iowow_1.3.16 with critical fixes

# [v2.0.11] - 2019-05-01

- Upgraded to iowow_1.3.15 with critical fixes

# [v2.0.10] - 2019-04-27

- CRITICAL: Fixed unexpected database file truncation and data loss on close.
- Upgraded to iowow 1.3.14

# [v2.0.9] - 2019-04-25

- Upgraded to iowow 1.3.13
- Building Native JNI binding on Android
- ejdb2_dart: Added notFound,invalidQuery to EJDB2Error

# [v2.0.8] - 2019-04-21

- Remove redundant memory coping in ejdb_put()

# [v2.0.7] - 2019-04-21

- Fixed deadlock condition with OSX pthread rwlocks.
- Phreads `PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP` is used by default

# [v2.0.6] - 2019-04-19

- Java JNI binding implemented (#243)
- Stability: `jql_create` takes own copy of `coll` argument
- Documentation improvements

# [v2.0.5] - 2019-04-15

- Library ported to OSX
- Library ported to Android NDK (#244)

# [v2.0.4] - 2019-04-09

- Landed DartVM binding  https://pub.dartlang.org/packages/ejdb2_dart
- Minors API enhancements

# [v2.0.3] - 2019-04-03

- Fixed #238 Disable network access for PPA Lunchpad builds
- Removed gcc `-fvisibility=hidden` from shared lib flag

# [v2.0.1] - 2019-04-02

- Because we are static linked to iowow its header files included under `include/ejdb2/iowow`

# [v2.0.0] - 2019-04-01

- Initial beta release
