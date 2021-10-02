@echo off

set test_files=empty.gokuro hello.gokuro no_newline.gokuro long_line.gokuro multi_line.gokuro macro.gokuro macro_error.gokuro macro_args.gokuro macro_dollar.gokuro macro_escaped_comma.gokuro macro_local.gokuro macro_defined_by_macro.gokuro macro_combined.gokuro macro_japanese.gokuro math.gokuro

if "%~1" == "all" (
call :TestC
call :TestGo
call :TestJS
call :TestJulia
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

if "%~1" == "jl" (
call :TestJulia
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

:TestJulia
IF NOT EXIST test\temp mkdir test\temp
for %%f in (%test_files%) do (
julia gokuro.jl < test\input\%%f > test\temp\%%f
diff test\expected\%%f test\temp\%%f
)
call :cleanup
exit /b

:cleanup
rd test\temp /s /q
exit /b
