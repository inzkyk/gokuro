@echo off

set test_files=empty.org hello.org no_newline.org long_line.org multi_line.org macro.org macro_error.org macro_args.org macro_dollar.org macro_escaped_comma.org macro_local.org macro_defined_by_macro.org macro_combined.org

if "%~1" == "all" (
call :TestC
call :TestGo
call :TestJS
exit /b
)

if "%~1" == "c" (
call :TestC
exit /b
)

if "%~1" == "go" (
call :TestGo
exit /b
)

if "%~1" == "js" (
call :TestJS
exit /b
)

if "%~1" == "clean" (
del *.exe
exit /b
)

echo "Please specify command."
exit /b

:TestC
IF NOT EXIST test\temp mkdir test\temp
clang gokuro.c -o gokuro_c.exe -Weverything -Werror -std=c99
for %%f in (%test_files%) do (
gokuro_c.exe < test\input\%%f > test\temp\%%f
diff test\expected\%%f test\temp\%%f
)
call :cleanup
exit /b

:TestGo
IF NOT EXIST test\temp mkdir test\temp
go build -o gokuro_go.exe gokuro.go
for %%f in (%test_files%) do (
gokuro_go.exe < test\input\%%f > test\temp\%%f
diff test\expected\%%f test\temp\%%f
)
call :cleanup
exit /b

:TestJS
IF NOT EXIST test\temp mkdir test\temp
for %%f in (%test_files%) do (
node gokuro.js < test\input\%%f > test\temp\%%f
diff test\expected\%%f test\temp\%%f
)
call :cleanup
exit /b

:cleanup
rd test\temp /s /q
exit /b
