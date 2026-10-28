# VCXRelative

Experimental tool that turns CMake-generated Visual Studio solutions (`.sln`, `.slnx`, `.vcxproj`) into portable solutions.

- Replaces hardcoded absolute paths with relative paths and `$(ProjectDir)` macros.
- Strips CMake regeneration hooks and meta-projects (`ZERO_CHECK`, `ALL_BUILD`).

## Build

Requires CMake 3.15+ and a C++20 compiler.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Usage

```bash
vcxrelative <build_dir> [source_dir...]
```

Example:
```bash
vcxrelative ./build .
```

## License

MIT
