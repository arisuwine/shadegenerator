# Shade Generator

SDK generator for Source 2 games.

Generated Source 2 SDKs: https://github.com/arisuwine/shade-source2-sdk

## Table of Contents

- [Features](#features)
- [Usage](#usage)
- [Emit Types](#emit-types)
- [Generated SDK Usage](#generated-sdk-usage)
- [Building](#building)
- [Project Design](#project-design)
- [Credits](#credits)
- [Contributing](#contributing)
- [Dependencies](#dependencies)

## Features
- Collects classes and enums from the Source 2 Schema System
- Generates a self-contained, header-only SDK
- Supports multiple Source 2 games
- Supports three emit types

## Usage
The generator supports only 64-bit Windows. Launch the compiled `.exe` file and wait for generation to complete.

> [!NOTE]
> The generator does not require a running game process!
---

You can pass additional arguments to the generator:
- `-h`, `--help` - print the help message
- `-v`, `--version` - print the generator version
- `-t`, `--type` - select an emit type (cpp, ida_c, ida_cpp)
- `-s`, `--split` - select the file layout (`per_file`, `module`, `dual`, or `single`; defaults to `per_file` for `cpp` and `single` for IDA types)
- `-p`, `--path` - specify the game root directory
- `-o`, `--output` - specify the output directory (without this argument, the SDK will be generated in the directory containing the executable)

```powershell
.\shadegenerator.exe --path "C:\Game Path" --output "C:\Output Path" --type cpp
```

---

The generator will try to detect the game path automatically. If it cannot, use `-p`, `--path` to specify the game directory.

## Emit Types
The generator supports three emit types:
- `cpp` - standard `C++`-style header files
- `ida_c` - `C`-style `IDA`-compatible header files (no C++ inheritance)
- `ida_cpp` - `C++`-style `IDA`-compatible header files (with C++ inheritance)

## Split Modes
Every emit type supports four file layouts, selected with `--split`:
- `per_file` - one header per class or enum
- `module` - one header per schema module
- `dual` - both of the above in one tree
- `single` - one header for the whole SDK

`cpp` output is written to `<output>/shade`, and IDA output is written to `<output>/shade_ida`. Use a fresh output directory when changing layouts because the generator does not remove files left by previous runs.

## Generated SDK Usage
### C++

By default, the `cpp` emitter uses `--split per_file` and produces the following directory structure:

```text
shade/
├── CMakeLists.txt
└── sdk/
    ├── types.hpp
    └── <module>/
        ├── <class>.hpp
        └── <enum>.hpp
```

Module names depend on the selected game and include directories such as `client`, `animationsystem`, and `schemasystem`. Records without a module are written directly to `sdk/`.

`--split module` writes one header per schema module instead, for example `shade/sdk/client.hpp`; records without a module use `shade/sdk/__global.hpp`. A module header includes `types.hpp` and the other module headers whose complete definitions it needs, and it defines its enums and classes in dependency order. Some modules need complete definitions from each other (in CS2, `client` and `server` do). For such a cycle, the generator orders the modules and moves only the classes that an earlier module header needs from a later one, plus the definitions those classes need, into `shade/sdk/__cycle_<n>.hpp`. Every module header of the cycle includes that file. An enum needed before its module header is declared opaquely instead. A cycle between individual class definitions cannot be emitted and stops generation before writing output.

`--split dual` writes both layouts into one `shade` tree. Each module header is placed in its module directory next to the individual headers, for example `sdk/client/client.hpp` and `sdk/client/C_BaseEntity.hpp`; both layouts share `sdk/types.hpp` and one `CMakeLists.txt`. Module headers contain their own definitions, so choose one header layout per translation unit to avoid defining the same class twice.

`--split single` writes `shade/shade.hpp`, which includes `sdk/types.hpp` and defines every enum and class.

You can add your own atomic types in `types.hpp`, but they must match the names and sizes of the generated types.

---

You can easily add the generated SDK to your project via CMake:

```cmake
add_subdirectory(path/to/shade)
target_link_libraries(your_target PRIVATE shade)
```

### IDA C++ / IDA C
To import a generated header into `IDA Pro`, go to `File` -> `Load file` -> `Parse C header file`.

By default, the `ida` emitters use `--split single` and generate `shade_ida/shade.hpp`, which contains the whole SDK.

The other layouts never use `#include`; import each file separately instead:

```text
shade_ida/
└── sdk/
    ├── types.hpp          # atomic types; import first
    ├── enums.hpp          # all enums; import second
    ├── <module>.hpp       # --split module
    ├── __cycle_<n>.hpp    # --split module or dual, when module headers form a cycle
    └── <module>/
        ├── <class>.hpp    # --split per_file or dual
        └── <module>.hpp   # --split dual
```

Each class or module header begins with a `Requires` comment that lists every file to import before it, in import order. A header declares the types it only references through pointers, so those types do not need to be imported first.

## Building
### Requirements

- Windows x64
- A C++23-compatible compiler
- CMake 3.23 or newer
- Git
- Visual Studio 2022 (or newer) or Ninja



### Supported Games
Currently supported games:
- `CS2`
- `DOTA2`
- `DEADLOCK`



### Specifying Target
The target game is selected using the `SHADEGENERATOR_GAME` CMake option (note that `CS2` is the default). The supported values are listed above.

---

### Visual Studio

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DSHADEGENERATOR_GAME=<GAME>
cmake --build build --config Release
```

### Ninja

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSHADEGENERATOR_GAME=<GAME>
cmake --build build
```

Replace `<GAME>` with supported game name. Replace `Release` with `Debug` to create a Debug build.

## Project Design

Shade Generator separates schema collection from output generation. The complete generation pipeline is:

```text
Source 2 Modules -> Schema System -> Schema Model -> Dependency Analysis -> Formatter and Emitter -> Generated SDK
```

### Source Layout

```text
.
├── CMakeLists.txt              # Build configuration
├── README.md
└── shadegenerator/
    ├── codegen/                # Formatters, emitters, and source generation
    ├── game/                   # Game detection and module loading
    ├── schema/                 # Format-independent schema model
    ├── sdk/                    # Source 2 interfaces used by the generator
    ├── tools/
    │   ├── collector/          # Schema System data collection
    │   ├── dependencies.*      # Type dependency analysis
    │   └── sort.*              # Dependency ordering
    ├── utils/                  # Logging
    ├── config.hpp              # Generator configuration and emit types
    ├── generation.cpp          # Generation pipeline orchestration
    └── main.cpp                # CLI and application entry point
```

---

### Game Setup

The selected game is determined at compile time. On startup, the generator uses the path supplied through `--path` or locates the game through the Steam installation. It then loads the required game and engine modules and installs their Schema System bindings.

---

### Schema Collection

`CSchemaCollector` reads the available type scopes from the Source 2 Schema System and converts classes, enums, atomic types, fields, inheritance information, and memory layouts into `CSchemaModel`. This model is independent of a particular output format and acts as the intermediate representation used by the rest of the generator.

---

### Dependency Analysis

Before emitting declarations, the generator analyzes relationships between collected types. It distinguishes dependencies that require complete definitions from those that can use forward declarations. Every layout uses it to define classes in a valid order. C++ output also uses it to generate the required includes for each header, while split IDA output lists the files to import first and declares types referenced only through pointers.

---

### Code Generation

Type formatters convert schema types and declarations into the syntax required by the selected target. Emitters use the formatted types together with `CGenerator`, which handles low-level source construction, indentation, declarations, and comments.

The `cpp` emitter creates a `shade` directory containing `types.hpp`, headers in the selected split layout, and a CMake interface target. The `ida_c` and `ida_cpp` emitters instead create a `shade_ida` directory with flattened declarations suitable for importing into IDA Pro: one `shade.hpp` by default, or include-free headers in the selected split layout.

---

### Adding an Output Format

Collection and output generation are intentionally separated, so a new target language can reuse `CSchemaCollector`, `CSchemaModel`, and the dependency-analysis tools. Adding one requires implementing the appropriate formatter and emitter, using a different low-level generator if the target syntax requires it, and registering the new emit type in the CLI and `GenerateSdk`. Available output formats are compiled into the executable rather than loaded as runtime plugins; the user selects one of them at runtime through `--type`.

## Credits
- [halflifefan](https://github.com/halflifefan) - inspiration and assistance in developing this project
- Neverlose Source2Gen - idea and some code snippets

## Contributing
I would be very grateful for your help in developing this project. Please feel free to make pull requests.

## Dependencies
- [CMake](https://github.com/Kitware/CMake)
- [CLI11](https://github.com/CLIUtils/CLI11)
- [ValveFileVDF](https://github.com/TinyTinni/ValveFileVDF)
