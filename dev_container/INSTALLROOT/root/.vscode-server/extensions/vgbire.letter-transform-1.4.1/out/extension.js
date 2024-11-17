"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = exports.addSeparator = exports.pascal = exports.camelize = void 0;
const vscode_1 = require("vscode");
const camelizeRE = /[_-\s]+(\w)/g;
const camelize = (str) => {
    let result = str.replace(camelizeRE, (_, c) => (c ? c.toUpperCase() : ''));
    result = result[0].toLowerCase() + result.slice(1);
    return result;
};
exports.camelize = camelize;
const pascal = (str) => {
    let result = str.replace(camelizeRE, (_, c) => (c ? c.toUpperCase() : ''));
    result = result[0].toUpperCase() + result.slice(1);
    return result;
};
exports.pascal = pascal;
const underlineRE = /\B([A-Z])/g;
const addSeparator = (str, separator) => {
    // 先统一转成驼峰之后统一处理
    return (0, exports.camelize)(str)
        .replace(underlineRE, separator + '$1')
        .toLowerCase()
        .replace('/s/g', '_');
};
exports.addSeparator = addSeparator;
function activate(context) {
    context.subscriptions.push(vscode_1.commands.registerTextEditorCommand('case-transform.camel', (textEditor, edit) => {
        textEditor.selections.forEach((item) => {
            edit.replace(item, (0, exports.camelize)(textEditor.document.getText(item)));
        });
    }));
    context.subscriptions.push(vscode_1.commands.registerTextEditorCommand('case-transform.underline', (textEditor, edit) => {
        textEditor.selections.forEach((item) => {
            edit.replace(item, (0, exports.addSeparator)(textEditor.document.getText(item), '_'));
        });
    }));
    context.subscriptions.push(vscode_1.commands.registerTextEditorCommand('case-transform.kebab', (textEditor, edit) => {
        textEditor.selections.forEach((item) => {
            edit.replace(item, (0, exports.addSeparator)(textEditor.document.getText(item), '-'));
        });
    }));
    context.subscriptions.push(vscode_1.commands.registerTextEditorCommand('case-transform.upper', (textEditor, edit) => {
        textEditor.selections.forEach((item) => {
            edit.replace(item, textEditor.document.getText(item).toUpperCase());
        });
    }));
    context.subscriptions.push(vscode_1.commands.registerTextEditorCommand('case-transform.lower', (textEditor, edit) => {
        textEditor.selections.forEach((item) => {
            edit.replace(item, textEditor.document.getText(item).toLowerCase());
        });
    }));
    context.subscriptions.push(vscode_1.commands.registerTextEditorCommand('case-transform.pascal', (textEditor, edit) => {
        textEditor.selections.forEach((item) => {
            edit.replace(item, (0, exports.pascal)(textEditor.document.getText(item)));
        });
    }));
    context.subscriptions.push(vscode_1.commands.registerCommand('case-transform.cls', () => __awaiter(this, void 0, void 0, function* () {
        var _a, _b;
        const editor = vscode_1.window.activeTextEditor;
        const document = editor.document;
        for (let index = 0; index < editor.selections.length; index++) {
            const selection = editor.selections[index];
            const lineNumber = selection.active.line;
            const isMaxLine = lineNumber + 1 >= document.lineCount;
            let text = document.getText(selection);
            const lineText = document.lineAt(lineNumber).text;
            if (selection.isEmpty) {
                const before = lineText.slice(0, selection.start.character);
                const after = lineText.slice(selection.end.character);
                text = (((_a = before.match(/[\w]+$/)) === null || _a === void 0 ? void 0 : _a[0]) || '') + (((_b = after.match(/^[\w]+/)) === null || _b === void 0 ? void 0 : _b[0]) || '');
            }
            yield editor.edit((editBuilder) => {
                editBuilder.insert(new vscode_1.Position(isMaxLine ? document.lineCount : lineNumber + 1, 0), `${isMaxLine ? '\n' : ''}${lineText.match(/^\s+/) || ''}console.log(${text})\n`);
            });
        }
    })));
}
exports.activate = activate;
//# sourceMappingURL=extension.js.map