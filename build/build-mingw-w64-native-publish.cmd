@echo off
rem * This script builds a paq8px-lite executable for all x86/x64 CPUs - no questions asked
rem * Remark: SIMD-specific functions are dispatched at runtime to match the runtime processor.
rem * If the build fails see compiler errors in _error2_paq.txt

rem * Set your mingw-w64 path below
set path=%path%;c:/Program Files/mingw-w64/winlibs-x86_64-posix-seh-gcc-9.3.0-llvm-10.0.0-mingw-w64-7.0.0-r4/bin

rem * The following settings are for a release build.
rem * For a debug build remove -DNDEBUG to enable asserts and array bound checks and add -Wall to show compiler warnings.
set options=-DNDEBUG -O3 -m64 -march=native -mtune=native -flto -fwhole-program 

del _error2_paq.txt  >nul 2>&1
del paq8px-lite-t1.exe       >nul 2>&1

g++.exe -s -static -fno-rtti -std=gnu++1z %options% ../file/*.cpp ../model/*.cpp ../*.cpp -opaq8px-lite-t1.exe    2>_error2_paq.txt
IF %ERRORLEVEL% NEQ 0 goto end

pause
