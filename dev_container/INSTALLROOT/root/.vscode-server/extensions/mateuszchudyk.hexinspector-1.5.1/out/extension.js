'use strict';
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = require("vscode");
const input_handlers = require("./input_handlers");
function activate(context) {
    var hover = vscode.languages.registerHoverProvider({ scheme: '*', language: '*' }, {
        provideHover(document, position, token) {
            var word = document.getText(document.getWordRangeAtPosition(position));
            let inputDataTypes = vscode.workspace.getConfiguration('hexinspector').get('inputDataTypes');
            let forms = vscode.workspace.getConfiguration('hexinspector').get('hoverContent');
            let endianness = vscode.workspace.getConfiguration('hexinspector').get('endianness');
            endianness = endianness.charAt(0).toUpperCase() + endianness.slice(1).toLowerCase() + ' Endian';
            if (inputDataTypes.length == 0 || forms.length == 0) {
                return undefined;
            }
            let bytes;
            let formsMap;
            for (let inputDataType of inputDataTypes) {
                let inputHandler = input_handlers.createInputHandler(inputDataType);
                if (!inputHandler)
                    continue;
                let parsed = inputHandler.parse(word);
                if (!parsed)
                    continue;
                bytes = inputHandler.convert(parsed, endianness == 'Little Endian');
                formsMap = inputHandler.getFormsMap();
            }
            if (bytes) {
                let formMaxLength = 0;
                for (let form of forms) {
                    if (form in formsMap)
                        formMaxLength = Math.max(formMaxLength, form.length);
                }
                let length = bytes.length;
                let message = 'HexInspector: ' + word + ' (' + length + 'B)\n\n';
                for (let form of forms) {
                    if (!(form in formsMap))
                        continue;
                    let result = formsMap[form](bytes);
                    if (result == '')
                        continue;
                    message += form.charAt(0).toUpperCase() + form.slice(1) + ':  ';
                    message += ' '.repeat(formMaxLength - form.length) + result + '\n';
                }
                message += '\n' + endianness;
                return new vscode.Hover({ language: 'hexinspector', value: message });
            }
        }
    });
    if (hover) {
        context.subscriptions.push(hover);
    }
}
exports.activate = activate;
function deactivate() { }
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map