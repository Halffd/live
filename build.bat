@echo off
gcc src/*.c -o build/live.exe -I S:/msys64/mingw64/include -I lib -L S:/msys64/mingw64/lib -lcurl