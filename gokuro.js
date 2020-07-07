function parseMacroArgs(s) {
    let argIdx = 0
    let argBegin = 0
    let argEnd = 0
    let answer = []
    while (argEnd < s.length) {
        if (s[argEnd] == ",") {
            answer[argIdx] = s.slice(argBegin, argEnd)
            argBegin = argEnd + 1
            argEnd = argEnd + 1
            argIdx++
            continue
        }
        if ((s[argEnd] == '\\') && (s[argEnd + 1] == ',')) {
            // escaped comma
            s = s.replace("\\,", ",")
            argEnd += 2
            continue
        }
        argEnd++
    }

    answer[argIdx] = s.slice(argBegin, argEnd)
    return answer
}

const regexDollar = RegExp("\\$", "g")
function escapeDollar(s) {
    return s.replace(regexDollar, "@$@")
}

const regexEscapedDollar = RegExp("@\\$@", "g")
function unescapeDollar(s) {
    return s.replace(regexEscapedDollar, "$")
}

function gokuro(inputStream, outputStream) {
    const globalMacroDefinition = /^#\+MACRO: (.*?) (.*)\n$/
    const localMacroDefinition = /^#\+MACRO_LOCAL: (.*?) (.*)\n$/
    const macroCall = /<<<(.+?)(?:\((.*?)\))?>>>/
    let globalMacro = {}
    let localMacro = {}

    var reader = require('readline').createInterface({
        input: inputStream,
    });

    reader.on('line', function (line) {
        line = line + "\n"
	const LINE_TYPE_NORMAL = 0
	const LINE_TYPE_GLOBAL_MACRO_DEFINITION = 1
	const LINE_TYPE_LOCAL_MACRO_DEFINITION = 2
        let lineType = LINE_TYPE_NORMAL
        while (true) {
            const globalMacroMatch = line.match(globalMacroDefinition)
            if (globalMacroMatch != null) {
                lineType = LINE_TYPE_GLOBAL_MACRO_DEFINITION
                const name = globalMacroMatch[1]
                const body = globalMacroMatch[2]
                globalMacro[name] = body
                break
            }

            const localMacroMatch = line.match(localMacroDefinition)
            if (localMacroMatch != null) {
                lineType = LINE_TYPE_LOCAL_MACRO_DEFINITION
                const name = localMacroMatch[1]
                const body = localMacroMatch[2]
                localMacro[name] = body
                break
            }

            let macroCallMatch = null
            let num_consective_open = 0
            for (j = line.length - 1; j--; j >= 0) {
                if (line[j] != '<') {
                    continue
                }
                num_consective_open = num_consective_open + 1
                if (num_consective_open < 3) {
                    continue
                }
                macroCallMatch = line.slice(j).match(macroCall)
                if (macroCallMatch != null) {
                    var lastCallBegin = j
                    break
                }
            }

            if (macroCallMatch == null) {
                break
            }
            const name = macroCallMatch[1]
            let isConstant = (macroCallMatch[2] == undefined)
            let body = localMacro[name]
            if (body == undefined) {
                body = globalMacro[name]
                if (body == undefined) {
                    body = ""
                    isConstant = true
                }
            }

            if (isConstant) {
                head = line.slice(0, lastCallBegin)
                tail = line.slice(lastCallBegin).replace(macroCallMatch[0], body)
                line = head + tail
                continue
            }

            // macro with arguments
            macroCallMatch[2] = escapeDollar(macroCallMatch[2])
            const args = parseMacroArgs(macroCallMatch[2])
            body = body.replace(RegExp("\\$0", "g"), macroCallMatch[2])
            for (let j = 0; j < args.length; j++) {
                const dollar_num = "\\$" + (j + 1).toString()
                body = body.replace(RegExp(dollar_num, "g"), args[j])
            }
            for (let j = args.length; j < 10; j++) {
                const dollar_num = "\\$" + (j + 1).toString()
                body = body.replace(RegExp(dollar_num, "g"), "")
            }
            line = line.replace(macroCallMatch[0], body)
            line = unescapeDollar(line)
        }

        if (lineType == LINE_TYPE_NORMAL) {
            outputStream.write(line)
        }

        if (lineType != LINE_TYPE_LOCAL_MACRO_DEFINITION) {
            localMacro = {}
        }
    })
}

gokuro(process.stdin, process.stdout)
