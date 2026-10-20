@echo off
rem Super Mario Galaxy for the PlayStation 5 - the builder's own tools for Windows
rem (x64): nebula-recomp, PS5_Vulkan's ps5-native-tool, the builder and its app, into out\,
rem with the C runtime linked in (no Visual C++ redistributable needed). Run in
rem the folder ps5/builder/build-tools-windows.sh sends, on a PC with the Visual
rem Studio build tools (C++), LLVM (clang-cl) and Rust.
rem
rem SPDX-License-Identifier: GPL-3.0-only
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS (echo no Visual Studio C++ build tools & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%ProgramFiles%\LLVM\bin;%PATH%"
if not exist out mkdir out
if not exist obj mkdir obj

echo === nebula-recomp
set "RUSTFLAGS=-C target-feature=+crt-static"
pushd nebula
cargo build --release --locked -p nebula-recomp || exit /b 1
popd
copy /y nebula\target\release\nebula-recomp.exe out\ >nul || exit /b 1

echo === zlib
pushd obj
clang-cl /nologo /O2 /MT /c /I ..\zlib ..\zlib\adler32.c ..\zlib\compress.c ..\zlib\crc32.c ..\zlib\deflate.c ^
    ..\zlib\gzclose.c ..\zlib\gzlib.c ..\zlib\gzread.c ..\zlib\gzwrite.c ..\zlib\infback.c ..\zlib\inffast.c ^
    ..\zlib\inflate.c ..\zlib\inftrees.c ..\zlib\trees.c ..\zlib\uncompr.c ..\zlib\zutil.c || exit /b 1
llvm-lib /nologo /out:zlib.lib adler32.obj compress.obj crc32.obj deflate.obj gzclose.obj gzlib.obj gzread.obj ^
    gzwrite.obj infback.obj inffast.obj inflate.obj inftrees.obj trees.obj uncompr.obj zutil.obj || exit /b 1
popd

echo === ps5-native-tool
clang-cl /nologo /std:c++20 /O2 /MT /EHsc /W3 /I zlib native\native_app_builder.cpp native\self_container.cpp ^
    native\elf_object.cpp native\sce_module_writer.cpp obj\zlib.lib /Fo:obj\ /Fe:out\ps5-native-tool.exe || exit /b 1

echo === galaxy-builder
clang-cl /nologo /std:c++20 /O2 /MT /EHsc /W3 builder\builder.cpp /Fo:obj\ /Fe:out\galaxy-builder.exe || exit /b 1
echo === the app
"%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe /optimize+ /win32icon:app\app.ico ^
    /r:System.Windows.Forms.dll /r:System.Drawing.dll "/out:out\Super Mario Galaxy PS5 Builder.exe" app\Builder.cs || exit /b 1
echo built
