clang gokuro_c.c -o gokuro_c.exe -Weverything -Werror

@echo off

if %errorlevel% NEQ 0 (exit /b)

mkdir test\temp

set files=empty.org hello.org no_newline.org long_line.org multi_line.org macro.org macro_error.org macro_args.org

if "%~1" == "check" (
  @echo on
  for %%f in (%files%) do (
    gokuro_c.exe < test\input\%%f 2> nul
  )
  goto last
)

if "%~1" == "save" (
  for %%f in (%files%) do (
    gokuro_c.exe < test\input\%%f > test\output\%%f 2> nul
  )
  goto last
)

if "%~1" == "diff" (
  for %%f in (%files%) do (
    gokuro_c.exe < test\input\%%f > test\temp\%%f 2> nul
    diff test\output\%%f test\temp\%%f
  )
  goto last
)

:last
rd test\temp /s /q
