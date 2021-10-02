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
                globalMacro[name] = body.replaceAll(/(\$+)([^0-9])/g, "@$1@$2")
                break
            }

            const localMacroMatch = line.match(localMacroDefinition)
            if (localMacroMatch != null) {
                lineType = LINE_TYPE_LOCAL_MACRO_DEFINITION
                const name = localMacroMatch[1]
                const body = localMacroMatch[2]
                localMacro[name] = body.replaceAll(/(\$+)([^0-9])/g, "@$1@$2")
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
                body = body.replaceAll(/@(\$+)@/g, "$1$1")
                head = line.slice(0, lastCallBegin)
                tail = line.slice(lastCallBegin).replace(macroCallMatch[0], body)
                line = head + tail
                continue
            }

            // macro with arguments
            const args_text = macroCallMatch[2].replaceAll("$", "@$@")
            const args = parseMacroArgs(args_text)
            body = body.replaceAll("\$0", args_text)
            for (let j = 0; j < args.length; j++) {
                const dollar_num = "$" + (j + 1).toString()
                body = body.replaceAll(dollar_num, args[j])
            }
            for (let j = args.length; j < 10; j++) {
                const dollar_num = "$" + (j + 1).toString()
                body = body.replaceAll(dollar_num, "")
            }

            body = body.replaceAll(/@(\$+)@/g, "$1$1")
            line = line.replaceAll(macroCallMatch[0], body)
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
