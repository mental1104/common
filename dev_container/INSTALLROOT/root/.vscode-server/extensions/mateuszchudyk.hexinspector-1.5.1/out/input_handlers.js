'use strict';
Object.defineProperty(exports, "__esModule", { value: true });
exports.createInputHandler = void 0;
const converters = require("./converters");
const utils = require("./utils");
function createFormsMap(forms) {
    let availableFormsMap = {
        'binary': function (bytes) {
            return utils.addSeparatorToNumber(converters.toBinary(bytes), ' ', 8);
        },
        'ascii': converters.toAscii,
        'decimal': function (bytes) {
            let asUnsigned = utils.addSeparatorToNumber(converters.toDecimalUnsigned(bytes), ',', 3);
            let asSigned = utils.addSeparatorToNumber(converters.toDecimalSigned(bytes), ',', 3);
            return asUnsigned + (asSigned != asUnsigned ? ' / ' + asSigned : '');
        },
        'float16': converters.toFloat16,
        'float32': converters.toFloat32,
        'float64': converters.toFloat64,
        'hexadecimal': function (bytes) {
            return utils.addSeparatorToNumber(converters.toHexadecimal(bytes), ' ', 2);
        },
        'size': converters.toSize,
        'bits set': converters.toBitSet,
    };
    let result = {};
    for (const form of forms) {
        if (!(form in availableFormsMap))
            continue;
        result[form] = availableFormsMap[form];
    }
    return result;
}
class InputHandler {
}
class InputHandlerBinary extends InputHandler {
    parse(str) {
        let regexes = [
            '0b([0-1]+)',
        ];
        for (let regex of regexes) {
            let re = new RegExp('^' + regex + '$');
            let match = re.exec(str);
            if (match) {
                return match[1];
            }
        }
    }
    convert(str, isLittleEndian) {
        return converters.fromBinary(str, isLittleEndian);
    }
    getFormsMap() {
        return createFormsMap([
            'ascii',
            'decimal',
            'float16',
            'float32',
            'float64',
            'hexadecimal',
            'size',
            'bits set',
        ]);
    }
}
class InputHandlerDecimal extends InputHandler {
    parse(str) {
        let regexes = [
            '([0-9]+)(?:[uU])?(?:[lL])?(?:[lL])?',
        ];
        for (let regex of regexes) {
            let re = new RegExp('^' + regex + '$');
            let match = re.exec(str);
            if (match) {
                return match[1];
            }
        }
    }
    convert(str, isLittleEndian) {
        return converters.fromDecimal(str, isLittleEndian);
    }
    getFormsMap() {
        return createFormsMap([
            'ascii',
            'binary',
            'float16',
            'float32',
            'float64',
            'hexadecimal',
            'size',
            'bits set',
        ]);
    }
}
class InputHandlerHexadecimal extends InputHandler {
    parse(str) {
        let regexes = [
            '0x([0-9a-fA-F]+)(?:[uU])?(?:[lL])?(?:[lL])?',
            '#([0-9a-fA-F]+)'
        ];
        for (let regex of regexes) {
            let re = new RegExp('^' + regex + '$');
            let match = re.exec(str);
            if (match) {
                return match[1];
            }
        }
    }
    convert(str, isLittleEndian) {
        return converters.fromHexadecimal(str, isLittleEndian);
    }
    getFormsMap() {
        return createFormsMap([
            'ascii',
            'binary',
            'decimal',
            'float16',
            'float32',
            'float64',
            'size',
            'bits set',
        ]);
    }
}
function createInputHandler(name) {
    let map = {
        'binary': new InputHandlerBinary,
        'decimal': new InputHandlerDecimal,
        'hexadecimal': new InputHandlerHexadecimal,
    };
    return map[name];
}
exports.createInputHandler = createInputHandler;
//# sourceMappingURL=input_handlers.js.map