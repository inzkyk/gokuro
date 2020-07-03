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
    const globalMacroDefinition = /^#\+MACRO: (.*?) (.*)$/
    const localMacroDefinition = /^#\+MACRO_LOCAL: (.*?) (.*)$/
    const macroCall = /{{{(.+?)(?:\((.*?)\))?}}}/
    let globalMacro = {}
    let localMacro = {}

    var reader = require('readline').createInterface({
        input: inputStream,
    });

    reader.on('line', function (line) {
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

            const lastCallBegin = line.lastIndexOf("{{{")
            if (lastCallBegin == -1) {
                // no more macro to expand.
                break
            }
            const macroCallMatch = line.slice(lastCallBegin).match(macroCall)
            if (macroCallMatch == null) {
                // "{{{" is found, but this line does not have a macro call.
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
                line = line.replace(macroCallMatch[0], body)
                continue
            }

            // macro with arguments
            macroCallMatch[2] = macroCallMatch[2].replace(RegExp("\\$", "g"), "@$@")
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
            line = line.replace(RegExp("@\\$@", "g"), "$")
        }

        if (lineType == LINE_TYPE_NORMAL) {
            outputStream.write(line + "\n")
        }

        if (lineType != LINE_TYPE_LOCAL_MACRO_DEFINITION) {
            localMacro = {}
        }
    })
}

gokuro(process.stdin, process.stdout)
