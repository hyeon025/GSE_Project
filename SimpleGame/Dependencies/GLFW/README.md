# Vendored GLFW 3.4

Upstream source: https://github.com/glfw/glfw/releases/tag/3.4

`glfw-3.4/` is the unmodified official 3.4 source distribution. Its license is
`glfw-3.4/LICENSE.md`. Headers, all platform sources, CMake files and supporting
files are included in Git; no fetch/submodule/install step is required.

`GLFW.vcxproj` builds the official Win32 backend as `glfw3.lib` using the selected
Win32/x64 and Debug/Release configuration, including matching MSVC CRT settings.
SimpleGame references this project so Visual Studio/MSBuild builds and links it
automatically. No prebuilt GLFW binary or DLL is used.

Generated binaries and intermediate objects live in `build/` and are ignored.
Do not change vendor files for application-specific behavior.
