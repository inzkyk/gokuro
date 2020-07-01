# gokuro

gokuro is a simple macro language.

This repository has several implementations of gokuro.

## build

c

```
$ gcc gokuro.c -o gokuro_c.exe -O3
$ clang gokuro.c -o gokuro_c.exe -O3
$ cl gokuro.c /Ox /link /out:gokuro_c.exe
```

go

```
$ go build -o gokuro_go.exe gokuro.go
```

nodejs

(build not required)

## run

c

```
$ gokuro_c.exe < INPUT_FILE
```

go

```
$ gokuro_go.exe < INPUT_FILE
```

nodejs

```
$ node gokuro.js < INPUT_FILE
```

## test

(windows only)

```
$ .\build.bat all
```
