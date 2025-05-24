# NextGen Lite - Advanced DLL Injector

![NextGen Lite Logo](nextgenlogo.bmp)

NextGen Lite is an advanced DLL injection tool developed for the Windows operating system. Its user-friendly interface allows you to easily inject DLL files into running processes.

## Features

- View a list of running processes.
- Select a specific DLL file.
- Inject the selected DLL into the chosen process.
- Filter to list only visible desktop applications instead of system services and background processes.
- Simple and modern Win32 API interface.
- Non-resizable window.

## Requirements

- Windows Operating System
- Visual Studio (with C++ desktop development environment installed)
- Windows SDK

## Building the Project

1. Clone or download this repository.
2. Open the `.sln` file in Visual Studio.
3. Set the build configuration to `Release` (optional, but recommended).
4. Click on `Build` > `Build Solution` or `Rebuild Solution`.

The compiled `nextgen-lite.exe` executable will be generated in your project's `x64/Release` (or another folder depending on your build settings) directory.

## Usage

1. Locate the compiled `nextgen-lite.exe` file.
2. **Copy `nextgenlogo.ico` and `nextgenlogo.bmp` to the SAME directory as `nextgen-lite.exe`.**
3. Run the `nextgen-lite.exe` file.
4. Click the "Browse..." button to select the `.dll` file you want to inject.
5. Select the target process from the list.
6. Click the "Inject DLL" button.

If the process list appears empty, click the "Refresh Process List" button to update the list. Check the "Show All Processes" box to view all processes (including system services).

## Filtering Logic

By default, the application only lists processes that have a visible window and are not in the list of known system processes. This ensures that the user is presented with only desktop applications they can interact with.

## Developer

This project is developed by [Ali Akgun](https://github.com/AliAkgun0).

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT).
