# Changelog

## Version 1.1.1: Signed Enum Fix

Fixed enums with negative values being generated with an unsigned underlying type.

### Changes

- The enum underlying type is now signed whenever at least one enumerator is negative, for example `enum class DOTASlotType_t : std::int32_t { DOTA_LOADOUT_TYPE_INVALID = -0x1, ... }`

## Version 1.1.0: Split Layouts

Added the `--split` option, which selects how the generated SDK is distributed across files. Every emit type supports four layouts. The default output layout is unchanged, but IDA output now goes to its own directory.

### Changes

- Added `-s` / `--split` for `cpp`, `ida_c`, and `ida_cpp` with four layouts: `per_file` (one header per class or enum), `module` (one header per schema module), `dual` (both layouts in one tree), and `single` (one header for the whole SDK)
- `cpp` keeps generating one header per type by default, and IDA types keep generating a single file
- C++ module headers include the module headers whose complete definitions they need and define classes in dependency order; when modules depend on each other, the classes needed to break the cycle are moved to `sdk/__cycle_<n>.hpp`
- In the `dual` layout, each module header is placed in its module directory next to the per-type headers, for example `sdk/client/client.hpp`
- The C++ `single` layout writes `shade/shade.hpp`, which includes `sdk/types.hpp` and defines every enum and class
- IDA output moved from `<output>/shade.hpp` to the `<output>/shade_ida/` directory; the default single-file header is now `shade_ida/shade.hpp`
- Split IDA layouts do not use `#include`: atomics are written to `sdk/types.hpp`, enums to `sdk/enums.hpp`, and each class or module header starts with a `Requires` comment listing the files to import first, in order; types referenced only through pointers are forward-declared
