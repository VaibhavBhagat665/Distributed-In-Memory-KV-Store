# Build Instructions

## Prerequisites

### C++ Toolchain

The C++ storage engine requires:
- CMake 3.15+
- C++17-capable compiler:
  - **Linux**: GCC 9+ or Clang 10+
  - **macOS**: Xcode Command Line Tools or Clang 10+
  - **Windows**: Visual Studio 2019+ or MinGW-w64

### Go Toolchain

The Go node process requires:
- Go 1.21 or later

## Building on Linux/macOS

```bash
# Build C++ engine
cd engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run C++ tests
cd build
ctest

# Build Go node
cd ../node
go build ./...

# Run Go tests
go test ./...
```

## Building on Windows

### Option 1: Visual Studio

```powershell
# Build C++ engine
cd engine
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release

# Build Go node
cd ..\node
go build .\...
```

### Option 2: MinGW

```powershell
# Build C++ engine
cd engine
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build Go node
cd ..\node
go build .\...
```

## Current Development Environment

This project is currently being developed on Windows without build toolchains installed.

**Required installations for this environment:**
1. **C++ Compiler** - Install one of:
   - Visual Studio 2022 Community Edition (includes MSVC) - https://visualstudio.microsoft.com/
   - MinGW-w64 via MSYS2 - https://www.msys2.org/
   - Or use WSL2 with Ubuntu

2. **Go** - Install from https://go.dev/dl/ (version 1.21+)

3. **CMake** - Install from https://cmake.org/download/ (version 3.15+)

The project structure and build configuration are complete and ready to build once toolchains are installed.

**Alternative**: Use WSL2 (Windows Subsystem for Linux) for a complete Linux development environment on Windows.

## Testing Without Building

The specification, design, and task plan are complete and can be reviewed in:
- `.kiro/specs/distributed-kv-store/requirements.md`
- `.kiro/specs/distributed-kv-store/design.md`
- `.kiro/specs/distributed-kv-store/tasks.md`
