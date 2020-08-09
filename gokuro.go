package main

import (
	"bufio"
	"io"
	"os"
	"regexp"
	"strconv"
	"strings"
)

func minInt(a int, b int) int {
	if a < b {
		return a
	}
	return b
}

func parseMacroArguments(s string) (ans []string) {
	currentArgumentIndex := 0
	argumentStart := 0
	argumentEnd := 0
	for argumentEnd < len(s) {
		if s[argumentEnd] == ',' {
			ans = append(ans, s[argumentStart:argumentEnd])
			argumentStart = argumentEnd + 1
			argumentEnd = argumentEnd + 1
			currentArgumentIndex++
			continue
		}

		// escaped comma.
		if s[argumentEnd] == '\\' &&
			argumentEnd < len(s)-1 &&
			s[argumentEnd+1] == ',' {
			s = strings.Replace(s, "\\,", ",", 1)
			argumentEnd = minInt(argumentEnd+2, len(s))
			continue
		}

		argumentEnd++
	}
	ans = append(ans, s[argumentStart:argumentEnd])
	return ans
}

func quoteDollar(s string) string {
	return strings.ReplaceAll(s, "$", "$@")
}

func unquoteDollar(s string) string {
	return strings.ReplaceAll(s, "$@", "$")
}

var regexGlobalMacroDef = regexp.MustCompile(`^#\+MACRO: ([^\s]+) (.*)\n$`)
var regexLocalMacroDef = regexp.MustCompile(`^#\+MACRO_LOCAL: ([^\s]+) (.*)\n$`)
var regexMacroCall = regexp.MustCompile(`<<<(.+?)(\((.*?)\))?>>>`)

func gokuro(in io.Reader, out io.Writer) {
	r := bufio.NewReaderSize(in, 1024*16)
	w := bufio.NewWriterSize(out, 1024*16)
	defer w.Flush()

	globalMacros := make(map[string]string)
	localMacros := make(map[string]string)

	for {
		line, err := r.ReadString('\n')
		if err != nil {
			if line == "" {
				break
			}
			line += "\n"
		}

		const LINE_TYPE_NORMAL = 0
		const LINE_TYPE_GLOBAL_MACRO_DEFINITION = 1
		const LINE_TYPE_LOCAL_MACRO_DEFINITION = 2
		lineType := LINE_TYPE_NORMAL
		for {
			mightBeMacroDefinition := strings.HasPrefix(line, "#+")
			if mightBeMacroDefinition {
				matchOneLineMacroDef := regexGlobalMacroDef.FindStringSubmatch(line)
				if matchOneLineMacroDef != nil {
					macroName := matchOneLineMacroDef[1]
					macroBody := matchOneLineMacroDef[2]
					globalMacros[macroName] = macroBody
					lineType = LINE_TYPE_GLOBAL_MACRO_DEFINITION
					break
				}

				matchLocalMacroDef := regexLocalMacroDef.FindStringSubmatch(line)
				if matchLocalMacroDef != nil {
					macroName := matchLocalMacroDef[1]
					macroBody := matchLocalMacroDef[2]
					localMacros[macroName] = macroBody
					lineType = LINE_TYPE_LOCAL_MACRO_DEFINITION
					break
				}
			}

			// process the last macro call (so that nested calls are treated properly).
			// locate the last macro call
			var matchMacroCall []string = nil
			lastCallBegin := -1
			num_consective_open := 0
			for i := len(line) - 1; i >= 0; i-- {
				if line[i] != '<' {
					continue
				}
				num_consective_open = num_consective_open + 1
				if num_consective_open < 3 {
					continue
				}
				matchMacroCall = regexMacroCall.FindStringSubmatch(line[i:])
				if matchMacroCall != nil {
					lastCallBegin = i
					break
				}
			}

			if matchMacroCall == nil {
				// no more macro calls.
				break
			}

			lastCallEnd := lastCallBegin + len(matchMacroCall[0])

			// look up the definition of the macro (local macro first).
			macroName := matchMacroCall[1]
			macroBody, ok := localMacros[macroName]
			if !ok {
				// the macro is not defined locally, it may be a global macro.
				macroBody = globalMacros[macroName]
			}

			macroDefined := macroBody != ""
			if !macroDefined {
				// delete the macro call.
				line = line[:lastCallBegin] + line[lastCallEnd:]
				continue
			}

			// calculate the replacement for the macro call.
			constantMacro := matchMacroCall[2] == ""
			if constantMacro {
				line = line[:lastCallBegin] + macroBody + line[lastCallEnd:]
				continue
			}

			// macro with arguments
			macroArgumentsText := quoteDollar(matchMacroCall[3])
			macroArguments := parseMacroArguments(macroArgumentsText)
			macroBody = strings.ReplaceAll(macroBody, "$0", macroArgumentsText)
			for i := 0; i < len(macroArguments); i++ {
				target := "$" + strconv.Itoa(i+1)
				macroBody = strings.ReplaceAll(macroBody, target, macroArguments[i])
			}
			for i := len(macroArguments); i <= 9; i++ {
				target := "$" + strconv.Itoa(i+1)
				macroBody = strings.ReplaceAll(macroBody, target, "")
			}
			line = line[:lastCallBegin] + unquoteDollar(macroBody) + line[lastCallEnd:]
		}

		if lineType == LINE_TYPE_NORMAL {
			w.WriteString(line)
		}

		if lineType != LINE_TYPE_LOCAL_MACRO_DEFINITION {
			localMacros = make(map[string]string)
		}

	}
}

func main() {
	gokuro(os.Stdin, os.Stdout)
}
