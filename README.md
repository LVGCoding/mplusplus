# mplusplus

M++ (bundled with the M++ Editor) is a work-in-progress programming language written in c++ that combines the visibility of LaTeX with the functionality of raw code.

See the [project UML diagram](UML.md) for the editor and math-node structure.

## Usage


### Flags
- ```--debug | --d``` = Run the program in debug mode.

## Dependencies

- [raylib](https://www.raylib.com/) 5.5 (CMake downloads and builds it automatically)
- [clay](https://github.com/nicbarker/clay) 0.14 (vendored in `src/lib/clay.h`)
- [Native File Dialog - Extended](https://github.com/btzy/nativefiledialog-extended) 1.4.1

## Building

You need CMake 3.20+ and a C++20 compiler (GCC 10+, Clang 12+, or Visual Studio 2022).
M++ includes an automated build script ```build.sh``` that compiles and runs the program.

```sh
cmake --preset debug          # or: cmake --preset release
cmake --build --preset debug
./build/debug/mplusplus       # Windows (Visual Studio generator): build\debug\Debug\mplusplus.exe
```

Without presets:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Platform notes

- **Linux:** raylib needs the X11/Wayland and OpenGL headers. On Debian/Ubuntu:
  `sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libwayland-dev libxkbcommon-dev`.
  On Arch: `sudo pacman -S libx11 libxrandr libxinerama libxcursor libxi mesa wayland libxkbcommon`.
- **macOS:** install the Xcode command line tools (`xcode-select --install`) and CMake (`brew install cmake`).
- **Windows:** open the folder in Visual Studio 2022 (it reads `CMakePresets.json`), or run the commands above from a Developer PowerShell.

