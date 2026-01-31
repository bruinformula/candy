# Candy

Candy is a library containing CAN backend stuff.

---

## Building the Project

### Prerequisites

- CMake 3.26+
- C++20 compiler (GCC, Clang, or MSVC)

The project manages 2 main dependencies:
- **SQLite** - Lightweight database engine (can use local installation or be automatically built)
- **Boost Spirit** - Parser framework for DBC file parsing (can use local installation or be automatically built)

---

### Building with VSCode

1. Open the project folder in VSCode.
2. Install the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) and [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) extensions.
3. Ensure your Qt installation is discoverable by CMake (see below).
4. Use the VSCode build tasks (`CMake: Configure` and `CMake: Build`) or the status bar buttons.

---

### Building with CMake (Command Line)

1. Create a build directory:
    ```sh
    mkdir build
    cd build
    ```
2. Configure the project with CMake, specifying the Qt path if needed:
    ```sh
    cmake ..
    ```
3. Build the project:
    ```sh
    cmake --build .
    ```

---

## Testing

The project includes unit tests for working components:

```sh
cd build
ctest
```

Or run individual test executables:
- `./test/` - Tests CAN and DBC parsing

---