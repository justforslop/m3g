# TODO

## High priority
- Fix the fail in display color pattern in the demo

```bash
make debug 
./build/debug ./assets/90.m3g
```
- Add embedded documentation in the header file, requires the doxygen style.
  - This allows the direct generation for the documentation files.
- Fix file naming, mainly in include/

## Mid priority

- merge the headers file into single header.
- adopt the STB-style documentaion style for telling users what to include and
  declare for using this library.

```c
/*
 * module_helper.h - one-line purpose.
 *
 * Do this:
 *     #define MODULE_HELPER_IMPL
 * before you include this file in *one* C or C++ file to create the
 * implementation (see tests/impl.c and src/impl.c).
 *
 * In the same place define and include dependencies (order matters if A
 * calls into B's impl):
 *     #define OTHER_HELPER_IMPL
 *     #include "lib/other_helper.h"
 *
 * In every other translation unit:
 *     #include "lib/module_helper.h"
 *
 * Repository root must be on the compiler include path. Or add lib/ and use
 *     #include "module_helper.h"
 *
 * Link the one MODULE_HELPER_IMPL translation unit into the final binary.
 *
 * Public API: module_parse(), module_free(). …brief how to call…
 *
 * Ownership:
 * - …what the caller free()s vs what module_free() releases…
 * - Call module_free() when finished (note NULL / zeroed / double-free
 *   safety if you guarantee it).
 */
```

## Low priority

- Make this library into single header library
