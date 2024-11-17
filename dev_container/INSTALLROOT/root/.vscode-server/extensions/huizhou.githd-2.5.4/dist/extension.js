"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __typeError = (msg) => {
  throw TypeError(msg);
};
var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
var __commonJS = (cb, mod) => function __require() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);
var __decorateClass = (decorators, target, key, kind) => {
  var result = kind > 1 ? void 0 : kind ? __getOwnPropDesc(target, key) : target;
  for (var i = decorators.length - 1, decorator; i >= 0; i--)
    if (decorator = decorators[i])
      result = (kind ? decorator(target, key, result) : decorator(result)) || result;
  if (kind && result) __defProp(target, key, result);
  return result;
};
var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);
var __accessCheck = (obj, member, msg) => member.has(obj) || __typeError("Cannot " + msg);
var __privateGet = (obj, member, getter) => (__accessCheck(obj, member, "read from private field"), getter ? getter.call(obj) : member.get(obj));
var __privateAdd = (obj, member, value) => member.has(obj) ? __typeError("Cannot add the same private member more than once") : member instanceof WeakSet ? member.add(obj) : member.set(obj, value);
var __privateSet = (obj, member, value, setter) => (__accessCheck(obj, member, "write to private field"), setter ? setter.call(obj, value) : member.set(obj, value), value);
var __privateMethod = (obj, member, method) => (__accessCheck(obj, member, "access private method"), method);
var __privateWrapper = (obj, member, setter, getter) => ({
  set _(value) {
    __privateSet(obj, member, value, setter);
  },
  get _() {
    return __privateGet(obj, member, getter);
  }
});

// node_modules/lodash/lodash.js
var require_lodash = __commonJS({
  "node_modules/lodash/lodash.js"(exports2, module2) {
    (function() {
      var undefined2;
      var VERSION = "4.17.21";
      var LARGE_ARRAY_SIZE = 200;
      var CORE_ERROR_TEXT = "Unsupported core-js use. Try https://npms.io/search?q=ponyfill.", FUNC_ERROR_TEXT = "Expected a function", INVALID_TEMPL_VAR_ERROR_TEXT = "Invalid `variable` option passed into `_.template`";
      var HASH_UNDEFINED = "__lodash_hash_undefined__";
      var MAX_MEMOIZE_SIZE = 500;
      var PLACEHOLDER = "__lodash_placeholder__";
      var CLONE_DEEP_FLAG = 1, CLONE_FLAT_FLAG = 2, CLONE_SYMBOLS_FLAG = 4;
      var COMPARE_PARTIAL_FLAG = 1, COMPARE_UNORDERED_FLAG = 2;
      var WRAP_BIND_FLAG = 1, WRAP_BIND_KEY_FLAG = 2, WRAP_CURRY_BOUND_FLAG = 4, WRAP_CURRY_FLAG = 8, WRAP_CURRY_RIGHT_FLAG = 16, WRAP_PARTIAL_FLAG = 32, WRAP_PARTIAL_RIGHT_FLAG = 64, WRAP_ARY_FLAG = 128, WRAP_REARG_FLAG = 256, WRAP_FLIP_FLAG = 512;
      var DEFAULT_TRUNC_LENGTH = 30, DEFAULT_TRUNC_OMISSION = "...";
      var HOT_COUNT = 800, HOT_SPAN = 16;
      var LAZY_FILTER_FLAG = 1, LAZY_MAP_FLAG = 2, LAZY_WHILE_FLAG = 3;
      var INFINITY = 1 / 0, MAX_SAFE_INTEGER = 9007199254740991, MAX_INTEGER = 17976931348623157e292, NAN = 0 / 0;
      var MAX_ARRAY_LENGTH = 4294967295, MAX_ARRAY_INDEX = MAX_ARRAY_LENGTH - 1, HALF_MAX_ARRAY_LENGTH = MAX_ARRAY_LENGTH >>> 1;
      var wrapFlags = [
        ["ary", WRAP_ARY_FLAG],
        ["bind", WRAP_BIND_FLAG],
        ["bindKey", WRAP_BIND_KEY_FLAG],
        ["curry", WRAP_CURRY_FLAG],
        ["curryRight", WRAP_CURRY_RIGHT_FLAG],
        ["flip", WRAP_FLIP_FLAG],
        ["partial", WRAP_PARTIAL_FLAG],
        ["partialRight", WRAP_PARTIAL_RIGHT_FLAG],
        ["rearg", WRAP_REARG_FLAG]
      ];
      var argsTag = "[object Arguments]", arrayTag = "[object Array]", asyncTag = "[object AsyncFunction]", boolTag = "[object Boolean]", dateTag = "[object Date]", domExcTag = "[object DOMException]", errorTag = "[object Error]", funcTag = "[object Function]", genTag = "[object GeneratorFunction]", mapTag = "[object Map]", numberTag = "[object Number]", nullTag = "[object Null]", objectTag = "[object Object]", promiseTag = "[object Promise]", proxyTag = "[object Proxy]", regexpTag = "[object RegExp]", setTag = "[object Set]", stringTag = "[object String]", symbolTag = "[object Symbol]", undefinedTag = "[object Undefined]", weakMapTag = "[object WeakMap]", weakSetTag = "[object WeakSet]";
      var arrayBufferTag = "[object ArrayBuffer]", dataViewTag = "[object DataView]", float32Tag = "[object Float32Array]", float64Tag = "[object Float64Array]", int8Tag = "[object Int8Array]", int16Tag = "[object Int16Array]", int32Tag = "[object Int32Array]", uint8Tag = "[object Uint8Array]", uint8ClampedTag = "[object Uint8ClampedArray]", uint16Tag = "[object Uint16Array]", uint32Tag = "[object Uint32Array]";
      var reEmptyStringLeading = /\b__p \+= '';/g, reEmptyStringMiddle = /\b(__p \+=) '' \+/g, reEmptyStringTrailing = /(__e\(.*?\)|\b__t\)) \+\n'';/g;
      var reEscapedHtml = /&(?:amp|lt|gt|quot|#39);/g, reUnescapedHtml = /[&<>"']/g, reHasEscapedHtml = RegExp(reEscapedHtml.source), reHasUnescapedHtml = RegExp(reUnescapedHtml.source);
      var reEscape = /<%-([\s\S]+?)%>/g, reEvaluate = /<%([\s\S]+?)%>/g, reInterpolate = /<%=([\s\S]+?)%>/g;
      var reIsDeepProp = /\.|\[(?:[^[\]]*|(["'])(?:(?!\1)[^\\]|\\.)*?\1)\]/, reIsPlainProp = /^\w*$/, rePropName = /[^.[\]]+|\[(?:(-?\d+(?:\.\d+)?)|(["'])((?:(?!\2)[^\\]|\\.)*?)\2)\]|(?=(?:\.|\[\])(?:\.|\[\]|$))/g;
      var reRegExpChar = /[\\^$.*+?()[\]{}|]/g, reHasRegExpChar = RegExp(reRegExpChar.source);
      var reTrimStart = /^\s+/;
      var reWhitespace = /\s/;
      var reWrapComment = /\{(?:\n\/\* \[wrapped with .+\] \*\/)?\n?/, reWrapDetails = /\{\n\/\* \[wrapped with (.+)\] \*/, reSplitDetails = /,? & /;
      var reAsciiWord = /[^\x00-\x2f\x3a-\x40\x5b-\x60\x7b-\x7f]+/g;
      var reForbiddenIdentifierChars = /[()=,{}\[\]\/\s]/;
      var reEscapeChar = /\\(\\)?/g;
      var reEsTemplate = /\$\{([^\\}]*(?:\\.[^\\}]*)*)\}/g;
      var reFlags = /\w*$/;
      var reIsBadHex = /^[-+]0x[0-9a-f]+$/i;
      var reIsBinary = /^0b[01]+$/i;
      var reIsHostCtor = /^\[object .+?Constructor\]$/;
      var reIsOctal = /^0o[0-7]+$/i;
      var reIsUint = /^(?:0|[1-9]\d*)$/;
      var reLatin = /[\xc0-\xd6\xd8-\xf6\xf8-\xff\u0100-\u017f]/g;
      var reNoMatch = /($^)/;
      var reUnescapedString = /['\n\r\u2028\u2029\\]/g;
      var rsAstralRange = "\\ud800-\\udfff", rsComboMarksRange = "\\u0300-\\u036f", reComboHalfMarksRange = "\\ufe20-\\ufe2f", rsComboSymbolsRange = "\\u20d0-\\u20ff", rsComboRange = rsComboMarksRange + reComboHalfMarksRange + rsComboSymbolsRange, rsDingbatRange = "\\u2700-\\u27bf", rsLowerRange = "a-z\\xdf-\\xf6\\xf8-\\xff", rsMathOpRange = "\\xac\\xb1\\xd7\\xf7", rsNonCharRange = "\\x00-\\x2f\\x3a-\\x40\\x5b-\\x60\\x7b-\\xbf", rsPunctuationRange = "\\u2000-\\u206f", rsSpaceRange = " \\t\\x0b\\f\\xa0\\ufeff\\n\\r\\u2028\\u2029\\u1680\\u180e\\u2000\\u2001\\u2002\\u2003\\u2004\\u2005\\u2006\\u2007\\u2008\\u2009\\u200a\\u202f\\u205f\\u3000", rsUpperRange = "A-Z\\xc0-\\xd6\\xd8-\\xde", rsVarRange = "\\ufe0e\\ufe0f", rsBreakRange = rsMathOpRange + rsNonCharRange + rsPunctuationRange + rsSpaceRange;
      var rsApos = "['\u2019]", rsAstral = "[" + rsAstralRange + "]", rsBreak = "[" + rsBreakRange + "]", rsCombo = "[" + rsComboRange + "]", rsDigits = "\\d+", rsDingbat = "[" + rsDingbatRange + "]", rsLower = "[" + rsLowerRange + "]", rsMisc = "[^" + rsAstralRange + rsBreakRange + rsDigits + rsDingbatRange + rsLowerRange + rsUpperRange + "]", rsFitz = "\\ud83c[\\udffb-\\udfff]", rsModifier = "(?:" + rsCombo + "|" + rsFitz + ")", rsNonAstral = "[^" + rsAstralRange + "]", rsRegional = "(?:\\ud83c[\\udde6-\\uddff]){2}", rsSurrPair = "[\\ud800-\\udbff][\\udc00-\\udfff]", rsUpper = "[" + rsUpperRange + "]", rsZWJ = "\\u200d";
      var rsMiscLower = "(?:" + rsLower + "|" + rsMisc + ")", rsMiscUpper = "(?:" + rsUpper + "|" + rsMisc + ")", rsOptContrLower = "(?:" + rsApos + "(?:d|ll|m|re|s|t|ve))?", rsOptContrUpper = "(?:" + rsApos + "(?:D|LL|M|RE|S|T|VE))?", reOptMod = rsModifier + "?", rsOptVar = "[" + rsVarRange + "]?", rsOptJoin = "(?:" + rsZWJ + "(?:" + [rsNonAstral, rsRegional, rsSurrPair].join("|") + ")" + rsOptVar + reOptMod + ")*", rsOrdLower = "\\d*(?:1st|2nd|3rd|(?![123])\\dth)(?=\\b|[A-Z_])", rsOrdUpper = "\\d*(?:1ST|2ND|3RD|(?![123])\\dTH)(?=\\b|[a-z_])", rsSeq = rsOptVar + reOptMod + rsOptJoin, rsEmoji = "(?:" + [rsDingbat, rsRegional, rsSurrPair].join("|") + ")" + rsSeq, rsSymbol = "(?:" + [rsNonAstral + rsCombo + "?", rsCombo, rsRegional, rsSurrPair, rsAstral].join("|") + ")";
      var reApos = RegExp(rsApos, "g");
      var reComboMark = RegExp(rsCombo, "g");
      var reUnicode = RegExp(rsFitz + "(?=" + rsFitz + ")|" + rsSymbol + rsSeq, "g");
      var reUnicodeWord = RegExp([
        rsUpper + "?" + rsLower + "+" + rsOptContrLower + "(?=" + [rsBreak, rsUpper, "$"].join("|") + ")",
        rsMiscUpper + "+" + rsOptContrUpper + "(?=" + [rsBreak, rsUpper + rsMiscLower, "$"].join("|") + ")",
        rsUpper + "?" + rsMiscLower + "+" + rsOptContrLower,
        rsUpper + "+" + rsOptContrUpper,
        rsOrdUpper,
        rsOrdLower,
        rsDigits,
        rsEmoji
      ].join("|"), "g");
      var reHasUnicode = RegExp("[" + rsZWJ + rsAstralRange + rsComboRange + rsVarRange + "]");
      var reHasUnicodeWord = /[a-z][A-Z]|[A-Z]{2}[a-z]|[0-9][a-zA-Z]|[a-zA-Z][0-9]|[^a-zA-Z0-9 ]/;
      var contextProps = [
        "Array",
        "Buffer",
        "DataView",
        "Date",
        "Error",
        "Float32Array",
        "Float64Array",
        "Function",
        "Int8Array",
        "Int16Array",
        "Int32Array",
        "Map",
        "Math",
        "Object",
        "Promise",
        "RegExp",
        "Set",
        "String",
        "Symbol",
        "TypeError",
        "Uint8Array",
        "Uint8ClampedArray",
        "Uint16Array",
        "Uint32Array",
        "WeakMap",
        "_",
        "clearTimeout",
        "isFinite",
        "parseInt",
        "setTimeout"
      ];
      var templateCounter = -1;
      var typedArrayTags = {};
      typedArrayTags[float32Tag] = typedArrayTags[float64Tag] = typedArrayTags[int8Tag] = typedArrayTags[int16Tag] = typedArrayTags[int32Tag] = typedArrayTags[uint8Tag] = typedArrayTags[uint8ClampedTag] = typedArrayTags[uint16Tag] = typedArrayTags[uint32Tag] = true;
      typedArrayTags[argsTag] = typedArrayTags[arrayTag] = typedArrayTags[arrayBufferTag] = typedArrayTags[boolTag] = typedArrayTags[dataViewTag] = typedArrayTags[dateTag] = typedArrayTags[errorTag] = typedArrayTags[funcTag] = typedArrayTags[mapTag] = typedArrayTags[numberTag] = typedArrayTags[objectTag] = typedArrayTags[regexpTag] = typedArrayTags[setTag] = typedArrayTags[stringTag] = typedArrayTags[weakMapTag] = false;
      var cloneableTags = {};
      cloneableTags[argsTag] = cloneableTags[arrayTag] = cloneableTags[arrayBufferTag] = cloneableTags[dataViewTag] = cloneableTags[boolTag] = cloneableTags[dateTag] = cloneableTags[float32Tag] = cloneableTags[float64Tag] = cloneableTags[int8Tag] = cloneableTags[int16Tag] = cloneableTags[int32Tag] = cloneableTags[mapTag] = cloneableTags[numberTag] = cloneableTags[objectTag] = cloneableTags[regexpTag] = cloneableTags[setTag] = cloneableTags[stringTag] = cloneableTags[symbolTag] = cloneableTags[uint8Tag] = cloneableTags[uint8ClampedTag] = cloneableTags[uint16Tag] = cloneableTags[uint32Tag] = true;
      cloneableTags[errorTag] = cloneableTags[funcTag] = cloneableTags[weakMapTag] = false;
      var deburredLetters = {
        // Latin-1 Supplement block.
        "\xC0": "A",
        "\xC1": "A",
        "\xC2": "A",
        "\xC3": "A",
        "\xC4": "A",
        "\xC5": "A",
        "\xE0": "a",
        "\xE1": "a",
        "\xE2": "a",
        "\xE3": "a",
        "\xE4": "a",
        "\xE5": "a",
        "\xC7": "C",
        "\xE7": "c",
        "\xD0": "D",
        "\xF0": "d",
        "\xC8": "E",
        "\xC9": "E",
        "\xCA": "E",
        "\xCB": "E",
        "\xE8": "e",
        "\xE9": "e",
        "\xEA": "e",
        "\xEB": "e",
        "\xCC": "I",
        "\xCD": "I",
        "\xCE": "I",
        "\xCF": "I",
        "\xEC": "i",
        "\xED": "i",
        "\xEE": "i",
        "\xEF": "i",
        "\xD1": "N",
        "\xF1": "n",
        "\xD2": "O",
        "\xD3": "O",
        "\xD4": "O",
        "\xD5": "O",
        "\xD6": "O",
        "\xD8": "O",
        "\xF2": "o",
        "\xF3": "o",
        "\xF4": "o",
        "\xF5": "o",
        "\xF6": "o",
        "\xF8": "o",
        "\xD9": "U",
        "\xDA": "U",
        "\xDB": "U",
        "\xDC": "U",
        "\xF9": "u",
        "\xFA": "u",
        "\xFB": "u",
        "\xFC": "u",
        "\xDD": "Y",
        "\xFD": "y",
        "\xFF": "y",
        "\xC6": "Ae",
        "\xE6": "ae",
        "\xDE": "Th",
        "\xFE": "th",
        "\xDF": "ss",
        // Latin Extended-A block.
        "\u0100": "A",
        "\u0102": "A",
        "\u0104": "A",
        "\u0101": "a",
        "\u0103": "a",
        "\u0105": "a",
        "\u0106": "C",
        "\u0108": "C",
        "\u010A": "C",
        "\u010C": "C",
        "\u0107": "c",
        "\u0109": "c",
        "\u010B": "c",
        "\u010D": "c",
        "\u010E": "D",
        "\u0110": "D",
        "\u010F": "d",
        "\u0111": "d",
        "\u0112": "E",
        "\u0114": "E",
        "\u0116": "E",
        "\u0118": "E",
        "\u011A": "E",
        "\u0113": "e",
        "\u0115": "e",
        "\u0117": "e",
        "\u0119": "e",
        "\u011B": "e",
        "\u011C": "G",
        "\u011E": "G",
        "\u0120": "G",
        "\u0122": "G",
        "\u011D": "g",
        "\u011F": "g",
        "\u0121": "g",
        "\u0123": "g",
        "\u0124": "H",
        "\u0126": "H",
        "\u0125": "h",
        "\u0127": "h",
        "\u0128": "I",
        "\u012A": "I",
        "\u012C": "I",
        "\u012E": "I",
        "\u0130": "I",
        "\u0129": "i",
        "\u012B": "i",
        "\u012D": "i",
        "\u012F": "i",
        "\u0131": "i",
        "\u0134": "J",
        "\u0135": "j",
        "\u0136": "K",
        "\u0137": "k",
        "\u0138": "k",
        "\u0139": "L",
        "\u013B": "L",
        "\u013D": "L",
        "\u013F": "L",
        "\u0141": "L",
        "\u013A": "l",
        "\u013C": "l",
        "\u013E": "l",
        "\u0140": "l",
        "\u0142": "l",
        "\u0143": "N",
        "\u0145": "N",
        "\u0147": "N",
        "\u014A": "N",
        "\u0144": "n",
        "\u0146": "n",
        "\u0148": "n",
        "\u014B": "n",
        "\u014C": "O",
        "\u014E": "O",
        "\u0150": "O",
        "\u014D": "o",
        "\u014F": "o",
        "\u0151": "o",
        "\u0154": "R",
        "\u0156": "R",
        "\u0158": "R",
        "\u0155": "r",
        "\u0157": "r",
        "\u0159": "r",
        "\u015A": "S",
        "\u015C": "S",
        "\u015E": "S",
        "\u0160": "S",
        "\u015B": "s",
        "\u015D": "s",
        "\u015F": "s",
        "\u0161": "s",
        "\u0162": "T",
        "\u0164": "T",
        "\u0166": "T",
        "\u0163": "t",
        "\u0165": "t",
        "\u0167": "t",
        "\u0168": "U",
        "\u016A": "U",
        "\u016C": "U",
        "\u016E": "U",
        "\u0170": "U",
        "\u0172": "U",
        "\u0169": "u",
        "\u016B": "u",
        "\u016D": "u",
        "\u016F": "u",
        "\u0171": "u",
        "\u0173": "u",
        "\u0174": "W",
        "\u0175": "w",
        "\u0176": "Y",
        "\u0177": "y",
        "\u0178": "Y",
        "\u0179": "Z",
        "\u017B": "Z",
        "\u017D": "Z",
        "\u017A": "z",
        "\u017C": "z",
        "\u017E": "z",
        "\u0132": "IJ",
        "\u0133": "ij",
        "\u0152": "Oe",
        "\u0153": "oe",
        "\u0149": "'n",
        "\u017F": "s"
      };
      var htmlEscapes = {
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;"
      };
      var htmlUnescapes = {
        "&amp;": "&",
        "&lt;": "<",
        "&gt;": ">",
        "&quot;": '"',
        "&#39;": "'"
      };
      var stringEscapes = {
        "\\": "\\",
        "'": "'",
        "\n": "n",
        "\r": "r",
        "\u2028": "u2028",
        "\u2029": "u2029"
      };
      var freeParseFloat = parseFloat, freeParseInt = parseInt;
      var freeGlobal = typeof global == "object" && global && global.Object === Object && global;
      var freeSelf = typeof self == "object" && self && self.Object === Object && self;
      var root = freeGlobal || freeSelf || Function("return this")();
      var freeExports = typeof exports2 == "object" && exports2 && !exports2.nodeType && exports2;
      var freeModule = freeExports && typeof module2 == "object" && module2 && !module2.nodeType && module2;
      var moduleExports = freeModule && freeModule.exports === freeExports;
      var freeProcess = moduleExports && freeGlobal.process;
      var nodeUtil = function() {
        try {
          var types = freeModule && freeModule.require && freeModule.require("util").types;
          if (types) {
            return types;
          }
          return freeProcess && freeProcess.binding && freeProcess.binding("util");
        } catch (e) {
        }
      }();
      var nodeIsArrayBuffer = nodeUtil && nodeUtil.isArrayBuffer, nodeIsDate = nodeUtil && nodeUtil.isDate, nodeIsMap = nodeUtil && nodeUtil.isMap, nodeIsRegExp = nodeUtil && nodeUtil.isRegExp, nodeIsSet = nodeUtil && nodeUtil.isSet, nodeIsTypedArray = nodeUtil && nodeUtil.isTypedArray;
      function apply(func, thisArg, args) {
        switch (args.length) {
          case 0:
            return func.call(thisArg);
          case 1:
            return func.call(thisArg, args[0]);
          case 2:
            return func.call(thisArg, args[0], args[1]);
          case 3:
            return func.call(thisArg, args[0], args[1], args[2]);
        }
        return func.apply(thisArg, args);
      }
      function arrayAggregator(array, setter, iteratee, accumulator) {
        var index = -1, length = array == null ? 0 : array.length;
        while (++index < length) {
          var value = array[index];
          setter(accumulator, value, iteratee(value), array);
        }
        return accumulator;
      }
      function arrayEach(array, iteratee) {
        var index = -1, length = array == null ? 0 : array.length;
        while (++index < length) {
          if (iteratee(array[index], index, array) === false) {
            break;
          }
        }
        return array;
      }
      function arrayEachRight(array, iteratee) {
        var length = array == null ? 0 : array.length;
        while (length--) {
          if (iteratee(array[length], length, array) === false) {
            break;
          }
        }
        return array;
      }
      function arrayEvery(array, predicate) {
        var index = -1, length = array == null ? 0 : array.length;
        while (++index < length) {
          if (!predicate(array[index], index, array)) {
            return false;
          }
        }
        return true;
      }
      function arrayFilter(array, predicate) {
        var index = -1, length = array == null ? 0 : array.length, resIndex = 0, result = [];
        while (++index < length) {
          var value = array[index];
          if (predicate(value, index, array)) {
            result[resIndex++] = value;
          }
        }
        return result;
      }
      function arrayIncludes(array, value) {
        var length = array == null ? 0 : array.length;
        return !!length && baseIndexOf(array, value, 0) > -1;
      }
      function arrayIncludesWith(array, value, comparator) {
        var index = -1, length = array == null ? 0 : array.length;
        while (++index < length) {
          if (comparator(value, array[index])) {
            return true;
          }
        }
        return false;
      }
      function arrayMap(array, iteratee) {
        var index = -1, length = array == null ? 0 : array.length, result = Array(length);
        while (++index < length) {
          result[index] = iteratee(array[index], index, array);
        }
        return result;
      }
      function arrayPush(array, values) {
        var index = -1, length = values.length, offset = array.length;
        while (++index < length) {
          array[offset + index] = values[index];
        }
        return array;
      }
      function arrayReduce(array, iteratee, accumulator, initAccum) {
        var index = -1, length = array == null ? 0 : array.length;
        if (initAccum && length) {
          accumulator = array[++index];
        }
        while (++index < length) {
          accumulator = iteratee(accumulator, array[index], index, array);
        }
        return accumulator;
      }
      function arrayReduceRight(array, iteratee, accumulator, initAccum) {
        var length = array == null ? 0 : array.length;
        if (initAccum && length) {
          accumulator = array[--length];
        }
        while (length--) {
          accumulator = iteratee(accumulator, array[length], length, array);
        }
        return accumulator;
      }
      function arraySome(array, predicate) {
        var index = -1, length = array == null ? 0 : array.length;
        while (++index < length) {
          if (predicate(array[index], index, array)) {
            return true;
          }
        }
        return false;
      }
      var asciiSize = baseProperty("length");
      function asciiToArray(string) {
        return string.split("");
      }
      function asciiWords(string) {
        return string.match(reAsciiWord) || [];
      }
      function baseFindKey(collection, predicate, eachFunc) {
        var result;
        eachFunc(collection, function(value, key, collection2) {
          if (predicate(value, key, collection2)) {
            result = key;
            return false;
          }
        });
        return result;
      }
      function baseFindIndex(array, predicate, fromIndex, fromRight) {
        var length = array.length, index = fromIndex + (fromRight ? 1 : -1);
        while (fromRight ? index-- : ++index < length) {
          if (predicate(array[index], index, array)) {
            return index;
          }
        }
        return -1;
      }
      function baseIndexOf(array, value, fromIndex) {
        return value === value ? strictIndexOf(array, value, fromIndex) : baseFindIndex(array, baseIsNaN, fromIndex);
      }
      function baseIndexOfWith(array, value, fromIndex, comparator) {
        var index = fromIndex - 1, length = array.length;
        while (++index < length) {
          if (comparator(array[index], value)) {
            return index;
          }
        }
        return -1;
      }
      function baseIsNaN(value) {
        return value !== value;
      }
      function baseMean(array, iteratee) {
        var length = array == null ? 0 : array.length;
        return length ? baseSum(array, iteratee) / length : NAN;
      }
      function baseProperty(key) {
        return function(object) {
          return object == null ? undefined2 : object[key];
        };
      }
      function basePropertyOf(object) {
        return function(key) {
          return object == null ? undefined2 : object[key];
        };
      }
      function baseReduce(collection, iteratee, accumulator, initAccum, eachFunc) {
        eachFunc(collection, function(value, index, collection2) {
          accumulator = initAccum ? (initAccum = false, value) : iteratee(accumulator, value, index, collection2);
        });
        return accumulator;
      }
      function baseSortBy(array, comparer) {
        var length = array.length;
        array.sort(comparer);
        while (length--) {
          array[length] = array[length].value;
        }
        return array;
      }
      function baseSum(array, iteratee) {
        var result, index = -1, length = array.length;
        while (++index < length) {
          var current = iteratee(array[index]);
          if (current !== undefined2) {
            result = result === undefined2 ? current : result + current;
          }
        }
        return result;
      }
      function baseTimes(n, iteratee) {
        var index = -1, result = Array(n);
        while (++index < n) {
          result[index] = iteratee(index);
        }
        return result;
      }
      function baseToPairs(object, props) {
        return arrayMap(props, function(key) {
          return [key, object[key]];
        });
      }
      function baseTrim(string) {
        return string ? string.slice(0, trimmedEndIndex(string) + 1).replace(reTrimStart, "") : string;
      }
      function baseUnary(func) {
        return function(value) {
          return func(value);
        };
      }
      function baseValues(object, props) {
        return arrayMap(props, function(key) {
          return object[key];
        });
      }
      function cacheHas(cache, key) {
        return cache.has(key);
      }
      function charsStartIndex(strSymbols, chrSymbols) {
        var index = -1, length = strSymbols.length;
        while (++index < length && baseIndexOf(chrSymbols, strSymbols[index], 0) > -1) {
        }
        return index;
      }
      function charsEndIndex(strSymbols, chrSymbols) {
        var index = strSymbols.length;
        while (index-- && baseIndexOf(chrSymbols, strSymbols[index], 0) > -1) {
        }
        return index;
      }
      function countHolders(array, placeholder) {
        var length = array.length, result = 0;
        while (length--) {
          if (array[length] === placeholder) {
            ++result;
          }
        }
        return result;
      }
      var deburrLetter = basePropertyOf(deburredLetters);
      var escapeHtmlChar = basePropertyOf(htmlEscapes);
      function escapeStringChar(chr) {
        return "\\" + stringEscapes[chr];
      }
      function getValue(object, key) {
        return object == null ? undefined2 : object[key];
      }
      function hasUnicode(string) {
        return reHasUnicode.test(string);
      }
      function hasUnicodeWord(string) {
        return reHasUnicodeWord.test(string);
      }
      function iteratorToArray(iterator) {
        var data, result = [];
        while (!(data = iterator.next()).done) {
          result.push(data.value);
        }
        return result;
      }
      function mapToArray(map) {
        var index = -1, result = Array(map.size);
        map.forEach(function(value, key) {
          result[++index] = [key, value];
        });
        return result;
      }
      function overArg(func, transform) {
        return function(arg) {
          return func(transform(arg));
        };
      }
      function replaceHolders(array, placeholder) {
        var index = -1, length = array.length, resIndex = 0, result = [];
        while (++index < length) {
          var value = array[index];
          if (value === placeholder || value === PLACEHOLDER) {
            array[index] = PLACEHOLDER;
            result[resIndex++] = index;
          }
        }
        return result;
      }
      function setToArray(set) {
        var index = -1, result = Array(set.size);
        set.forEach(function(value) {
          result[++index] = value;
        });
        return result;
      }
      function setToPairs(set) {
        var index = -1, result = Array(set.size);
        set.forEach(function(value) {
          result[++index] = [value, value];
        });
        return result;
      }
      function strictIndexOf(array, value, fromIndex) {
        var index = fromIndex - 1, length = array.length;
        while (++index < length) {
          if (array[index] === value) {
            return index;
          }
        }
        return -1;
      }
      function strictLastIndexOf(array, value, fromIndex) {
        var index = fromIndex + 1;
        while (index--) {
          if (array[index] === value) {
            return index;
          }
        }
        return index;
      }
      function stringSize(string) {
        return hasUnicode(string) ? unicodeSize(string) : asciiSize(string);
      }
      function stringToArray(string) {
        return hasUnicode(string) ? unicodeToArray(string) : asciiToArray(string);
      }
      function trimmedEndIndex(string) {
        var index = string.length;
        while (index-- && reWhitespace.test(string.charAt(index))) {
        }
        return index;
      }
      var unescapeHtmlChar = basePropertyOf(htmlUnescapes);
      function unicodeSize(string) {
        var result = reUnicode.lastIndex = 0;
        while (reUnicode.test(string)) {
          ++result;
        }
        return result;
      }
      function unicodeToArray(string) {
        return string.match(reUnicode) || [];
      }
      function unicodeWords(string) {
        return string.match(reUnicodeWord) || [];
      }
      var runInContext = function runInContext2(context) {
        context = context == null ? root : _.defaults(root.Object(), context, _.pick(root, contextProps));
        var Array2 = context.Array, Date2 = context.Date, Error2 = context.Error, Function2 = context.Function, Math2 = context.Math, Object2 = context.Object, RegExp2 = context.RegExp, String = context.String, TypeError2 = context.TypeError;
        var arrayProto = Array2.prototype, funcProto = Function2.prototype, objectProto = Object2.prototype;
        var coreJsData = context["__core-js_shared__"];
        var funcToString = funcProto.toString;
        var hasOwnProperty = objectProto.hasOwnProperty;
        var idCounter = 0;
        var maskSrcKey = function() {
          var uid = /[^.]+$/.exec(coreJsData && coreJsData.keys && coreJsData.keys.IE_PROTO || "");
          return uid ? "Symbol(src)_1." + uid : "";
        }();
        var nativeObjectToString = objectProto.toString;
        var objectCtorString = funcToString.call(Object2);
        var oldDash = root._;
        var reIsNative = RegExp2(
          "^" + funcToString.call(hasOwnProperty).replace(reRegExpChar, "\\$&").replace(/hasOwnProperty|(function).*?(?=\\\()| for .+?(?=\\\])/g, "$1.*?") + "$"
        );
        var Buffer2 = moduleExports ? context.Buffer : undefined2, Symbol2 = context.Symbol, Uint8Array2 = context.Uint8Array, allocUnsafe = Buffer2 ? Buffer2.allocUnsafe : undefined2, getPrototype = overArg(Object2.getPrototypeOf, Object2), objectCreate = Object2.create, propertyIsEnumerable = objectProto.propertyIsEnumerable, splice = arrayProto.splice, spreadableSymbol = Symbol2 ? Symbol2.isConcatSpreadable : undefined2, symIterator = Symbol2 ? Symbol2.iterator : undefined2, symToStringTag = Symbol2 ? Symbol2.toStringTag : undefined2;
        var defineProperty = function() {
          try {
            var func = getNative(Object2, "defineProperty");
            func({}, "", {});
            return func;
          } catch (e) {
          }
        }();
        var ctxClearTimeout = context.clearTimeout !== root.clearTimeout && context.clearTimeout, ctxNow = Date2 && Date2.now !== root.Date.now && Date2.now, ctxSetTimeout = context.setTimeout !== root.setTimeout && context.setTimeout;
        var nativeCeil = Math2.ceil, nativeFloor = Math2.floor, nativeGetSymbols = Object2.getOwnPropertySymbols, nativeIsBuffer = Buffer2 ? Buffer2.isBuffer : undefined2, nativeIsFinite = context.isFinite, nativeJoin = arrayProto.join, nativeKeys = overArg(Object2.keys, Object2), nativeMax = Math2.max, nativeMin = Math2.min, nativeNow = Date2.now, nativeParseInt = context.parseInt, nativeRandom = Math2.random, nativeReverse = arrayProto.reverse;
        var DataView = getNative(context, "DataView"), Map2 = getNative(context, "Map"), Promise2 = getNative(context, "Promise"), Set2 = getNative(context, "Set"), WeakMap2 = getNative(context, "WeakMap"), nativeCreate = getNative(Object2, "create");
        var metaMap = WeakMap2 && new WeakMap2();
        var realNames = {};
        var dataViewCtorString = toSource(DataView), mapCtorString = toSource(Map2), promiseCtorString = toSource(Promise2), setCtorString = toSource(Set2), weakMapCtorString = toSource(WeakMap2);
        var symbolProto = Symbol2 ? Symbol2.prototype : undefined2, symbolValueOf = symbolProto ? symbolProto.valueOf : undefined2, symbolToString = symbolProto ? symbolProto.toString : undefined2;
        function lodash(value) {
          if (isObjectLike(value) && !isArray(value) && !(value instanceof LazyWrapper)) {
            if (value instanceof LodashWrapper) {
              return value;
            }
            if (hasOwnProperty.call(value, "__wrapped__")) {
              return wrapperClone(value);
            }
          }
          return new LodashWrapper(value);
        }
        var baseCreate = /* @__PURE__ */ function() {
          function object() {
          }
          return function(proto) {
            if (!isObject(proto)) {
              return {};
            }
            if (objectCreate) {
              return objectCreate(proto);
            }
            object.prototype = proto;
            var result2 = new object();
            object.prototype = undefined2;
            return result2;
          };
        }();
        function baseLodash() {
        }
        function LodashWrapper(value, chainAll) {
          this.__wrapped__ = value;
          this.__actions__ = [];
          this.__chain__ = !!chainAll;
          this.__index__ = 0;
          this.__values__ = undefined2;
        }
        lodash.templateSettings = {
          /**
           * Used to detect `data` property values to be HTML-escaped.
           *
           * @memberOf _.templateSettings
           * @type {RegExp}
           */
          "escape": reEscape,
          /**
           * Used to detect code to be evaluated.
           *
           * @memberOf _.templateSettings
           * @type {RegExp}
           */
          "evaluate": reEvaluate,
          /**
           * Used to detect `data` property values to inject.
           *
           * @memberOf _.templateSettings
           * @type {RegExp}
           */
          "interpolate": reInterpolate,
          /**
           * Used to reference the data object in the template text.
           *
           * @memberOf _.templateSettings
           * @type {string}
           */
          "variable": "",
          /**
           * Used to import variables into the compiled template.
           *
           * @memberOf _.templateSettings
           * @type {Object}
           */
          "imports": {
            /**
             * A reference to the `lodash` function.
             *
             * @memberOf _.templateSettings.imports
             * @type {Function}
             */
            "_": lodash
          }
        };
        lodash.prototype = baseLodash.prototype;
        lodash.prototype.constructor = lodash;
        LodashWrapper.prototype = baseCreate(baseLodash.prototype);
        LodashWrapper.prototype.constructor = LodashWrapper;
        function LazyWrapper(value) {
          this.__wrapped__ = value;
          this.__actions__ = [];
          this.__dir__ = 1;
          this.__filtered__ = false;
          this.__iteratees__ = [];
          this.__takeCount__ = MAX_ARRAY_LENGTH;
          this.__views__ = [];
        }
        function lazyClone() {
          var result2 = new LazyWrapper(this.__wrapped__);
          result2.__actions__ = copyArray(this.__actions__);
          result2.__dir__ = this.__dir__;
          result2.__filtered__ = this.__filtered__;
          result2.__iteratees__ = copyArray(this.__iteratees__);
          result2.__takeCount__ = this.__takeCount__;
          result2.__views__ = copyArray(this.__views__);
          return result2;
        }
        function lazyReverse() {
          if (this.__filtered__) {
            var result2 = new LazyWrapper(this);
            result2.__dir__ = -1;
            result2.__filtered__ = true;
          } else {
            result2 = this.clone();
            result2.__dir__ *= -1;
          }
          return result2;
        }
        function lazyValue() {
          var array = this.__wrapped__.value(), dir = this.__dir__, isArr = isArray(array), isRight = dir < 0, arrLength = isArr ? array.length : 0, view = getView(0, arrLength, this.__views__), start = view.start, end = view.end, length = end - start, index = isRight ? end : start - 1, iteratees = this.__iteratees__, iterLength = iteratees.length, resIndex = 0, takeCount = nativeMin(length, this.__takeCount__);
          if (!isArr || !isRight && arrLength == length && takeCount == length) {
            return baseWrapperValue(array, this.__actions__);
          }
          var result2 = [];
          outer:
            while (length-- && resIndex < takeCount) {
              index += dir;
              var iterIndex = -1, value = array[index];
              while (++iterIndex < iterLength) {
                var data = iteratees[iterIndex], iteratee2 = data.iteratee, type = data.type, computed = iteratee2(value);
                if (type == LAZY_MAP_FLAG) {
                  value = computed;
                } else if (!computed) {
                  if (type == LAZY_FILTER_FLAG) {
                    continue outer;
                  } else {
                    break outer;
                  }
                }
              }
              result2[resIndex++] = value;
            }
          return result2;
        }
        LazyWrapper.prototype = baseCreate(baseLodash.prototype);
        LazyWrapper.prototype.constructor = LazyWrapper;
        function Hash(entries) {
          var index = -1, length = entries == null ? 0 : entries.length;
          this.clear();
          while (++index < length) {
            var entry = entries[index];
            this.set(entry[0], entry[1]);
          }
        }
        function hashClear() {
          this.__data__ = nativeCreate ? nativeCreate(null) : {};
          this.size = 0;
        }
        function hashDelete(key) {
          var result2 = this.has(key) && delete this.__data__[key];
          this.size -= result2 ? 1 : 0;
          return result2;
        }
        function hashGet(key) {
          var data = this.__data__;
          if (nativeCreate) {
            var result2 = data[key];
            return result2 === HASH_UNDEFINED ? undefined2 : result2;
          }
          return hasOwnProperty.call(data, key) ? data[key] : undefined2;
        }
        function hashHas(key) {
          var data = this.__data__;
          return nativeCreate ? data[key] !== undefined2 : hasOwnProperty.call(data, key);
        }
        function hashSet(key, value) {
          var data = this.__data__;
          this.size += this.has(key) ? 0 : 1;
          data[key] = nativeCreate && value === undefined2 ? HASH_UNDEFINED : value;
          return this;
        }
        Hash.prototype.clear = hashClear;
        Hash.prototype["delete"] = hashDelete;
        Hash.prototype.get = hashGet;
        Hash.prototype.has = hashHas;
        Hash.prototype.set = hashSet;
        function ListCache(entries) {
          var index = -1, length = entries == null ? 0 : entries.length;
          this.clear();
          while (++index < length) {
            var entry = entries[index];
            this.set(entry[0], entry[1]);
          }
        }
        function listCacheClear() {
          this.__data__ = [];
          this.size = 0;
        }
        function listCacheDelete(key) {
          var data = this.__data__, index = assocIndexOf(data, key);
          if (index < 0) {
            return false;
          }
          var lastIndex = data.length - 1;
          if (index == lastIndex) {
            data.pop();
          } else {
            splice.call(data, index, 1);
          }
          --this.size;
          return true;
        }
        function listCacheGet(key) {
          var data = this.__data__, index = assocIndexOf(data, key);
          return index < 0 ? undefined2 : data[index][1];
        }
        function listCacheHas(key) {
          return assocIndexOf(this.__data__, key) > -1;
        }
        function listCacheSet(key, value) {
          var data = this.__data__, index = assocIndexOf(data, key);
          if (index < 0) {
            ++this.size;
            data.push([key, value]);
          } else {
            data[index][1] = value;
          }
          return this;
        }
        ListCache.prototype.clear = listCacheClear;
        ListCache.prototype["delete"] = listCacheDelete;
        ListCache.prototype.get = listCacheGet;
        ListCache.prototype.has = listCacheHas;
        ListCache.prototype.set = listCacheSet;
        function MapCache(entries) {
          var index = -1, length = entries == null ? 0 : entries.length;
          this.clear();
          while (++index < length) {
            var entry = entries[index];
            this.set(entry[0], entry[1]);
          }
        }
        function mapCacheClear() {
          this.size = 0;
          this.__data__ = {
            "hash": new Hash(),
            "map": new (Map2 || ListCache)(),
            "string": new Hash()
          };
        }
        function mapCacheDelete(key) {
          var result2 = getMapData(this, key)["delete"](key);
          this.size -= result2 ? 1 : 0;
          return result2;
        }
        function mapCacheGet(key) {
          return getMapData(this, key).get(key);
        }
        function mapCacheHas(key) {
          return getMapData(this, key).has(key);
        }
        function mapCacheSet(key, value) {
          var data = getMapData(this, key), size2 = data.size;
          data.set(key, value);
          this.size += data.size == size2 ? 0 : 1;
          return this;
        }
        MapCache.prototype.clear = mapCacheClear;
        MapCache.prototype["delete"] = mapCacheDelete;
        MapCache.prototype.get = mapCacheGet;
        MapCache.prototype.has = mapCacheHas;
        MapCache.prototype.set = mapCacheSet;
        function SetCache(values2) {
          var index = -1, length = values2 == null ? 0 : values2.length;
          this.__data__ = new MapCache();
          while (++index < length) {
            this.add(values2[index]);
          }
        }
        function setCacheAdd(value) {
          this.__data__.set(value, HASH_UNDEFINED);
          return this;
        }
        function setCacheHas(value) {
          return this.__data__.has(value);
        }
        SetCache.prototype.add = SetCache.prototype.push = setCacheAdd;
        SetCache.prototype.has = setCacheHas;
        function Stack2(entries) {
          var data = this.__data__ = new ListCache(entries);
          this.size = data.size;
        }
        function stackClear() {
          this.__data__ = new ListCache();
          this.size = 0;
        }
        function stackDelete(key) {
          var data = this.__data__, result2 = data["delete"](key);
          this.size = data.size;
          return result2;
        }
        function stackGet(key) {
          return this.__data__.get(key);
        }
        function stackHas(key) {
          return this.__data__.has(key);
        }
        function stackSet(key, value) {
          var data = this.__data__;
          if (data instanceof ListCache) {
            var pairs = data.__data__;
            if (!Map2 || pairs.length < LARGE_ARRAY_SIZE - 1) {
              pairs.push([key, value]);
              this.size = ++data.size;
              return this;
            }
            data = this.__data__ = new MapCache(pairs);
          }
          data.set(key, value);
          this.size = data.size;
          return this;
        }
        Stack2.prototype.clear = stackClear;
        Stack2.prototype["delete"] = stackDelete;
        Stack2.prototype.get = stackGet;
        Stack2.prototype.has = stackHas;
        Stack2.prototype.set = stackSet;
        function arrayLikeKeys(value, inherited) {
          var isArr = isArray(value), isArg = !isArr && isArguments(value), isBuff = !isArr && !isArg && isBuffer(value), isType = !isArr && !isArg && !isBuff && isTypedArray(value), skipIndexes = isArr || isArg || isBuff || isType, result2 = skipIndexes ? baseTimes(value.length, String) : [], length = result2.length;
          for (var key in value) {
            if ((inherited || hasOwnProperty.call(value, key)) && !(skipIndexes && // Safari 9 has enumerable `arguments.length` in strict mode.
            (key == "length" || // Node.js 0.10 has enumerable non-index properties on buffers.
            isBuff && (key == "offset" || key == "parent") || // PhantomJS 2 has enumerable non-index properties on typed arrays.
            isType && (key == "buffer" || key == "byteLength" || key == "byteOffset") || // Skip index properties.
            isIndex(key, length)))) {
              result2.push(key);
            }
          }
          return result2;
        }
        function arraySample(array) {
          var length = array.length;
          return length ? array[baseRandom(0, length - 1)] : undefined2;
        }
        function arraySampleSize(array, n) {
          return shuffleSelf(copyArray(array), baseClamp(n, 0, array.length));
        }
        function arrayShuffle(array) {
          return shuffleSelf(copyArray(array));
        }
        function assignMergeValue(object, key, value) {
          if (value !== undefined2 && !eq(object[key], value) || value === undefined2 && !(key in object)) {
            baseAssignValue(object, key, value);
          }
        }
        function assignValue(object, key, value) {
          var objValue = object[key];
          if (!(hasOwnProperty.call(object, key) && eq(objValue, value)) || value === undefined2 && !(key in object)) {
            baseAssignValue(object, key, value);
          }
        }
        function assocIndexOf(array, key) {
          var length = array.length;
          while (length--) {
            if (eq(array[length][0], key)) {
              return length;
            }
          }
          return -1;
        }
        function baseAggregator(collection, setter, iteratee2, accumulator) {
          baseEach(collection, function(value, key, collection2) {
            setter(accumulator, value, iteratee2(value), collection2);
          });
          return accumulator;
        }
        function baseAssign(object, source) {
          return object && copyObject(source, keys(source), object);
        }
        function baseAssignIn(object, source) {
          return object && copyObject(source, keysIn(source), object);
        }
        function baseAssignValue(object, key, value) {
          if (key == "__proto__" && defineProperty) {
            defineProperty(object, key, {
              "configurable": true,
              "enumerable": true,
              "value": value,
              "writable": true
            });
          } else {
            object[key] = value;
          }
        }
        function baseAt(object, paths) {
          var index = -1, length = paths.length, result2 = Array2(length), skip = object == null;
          while (++index < length) {
            result2[index] = skip ? undefined2 : get(object, paths[index]);
          }
          return result2;
        }
        function baseClamp(number, lower, upper) {
          if (number === number) {
            if (upper !== undefined2) {
              number = number <= upper ? number : upper;
            }
            if (lower !== undefined2) {
              number = number >= lower ? number : lower;
            }
          }
          return number;
        }
        function baseClone(value, bitmask, customizer, key, object, stack) {
          var result2, isDeep = bitmask & CLONE_DEEP_FLAG, isFlat = bitmask & CLONE_FLAT_FLAG, isFull = bitmask & CLONE_SYMBOLS_FLAG;
          if (customizer) {
            result2 = object ? customizer(value, key, object, stack) : customizer(value);
          }
          if (result2 !== undefined2) {
            return result2;
          }
          if (!isObject(value)) {
            return value;
          }
          var isArr = isArray(value);
          if (isArr) {
            result2 = initCloneArray(value);
            if (!isDeep) {
              return copyArray(value, result2);
            }
          } else {
            var tag = getTag(value), isFunc = tag == funcTag || tag == genTag;
            if (isBuffer(value)) {
              return cloneBuffer(value, isDeep);
            }
            if (tag == objectTag || tag == argsTag || isFunc && !object) {
              result2 = isFlat || isFunc ? {} : initCloneObject(value);
              if (!isDeep) {
                return isFlat ? copySymbolsIn(value, baseAssignIn(result2, value)) : copySymbols(value, baseAssign(result2, value));
              }
            } else {
              if (!cloneableTags[tag]) {
                return object ? value : {};
              }
              result2 = initCloneByTag(value, tag, isDeep);
            }
          }
          stack || (stack = new Stack2());
          var stacked = stack.get(value);
          if (stacked) {
            return stacked;
          }
          stack.set(value, result2);
          if (isSet(value)) {
            value.forEach(function(subValue) {
              result2.add(baseClone(subValue, bitmask, customizer, subValue, value, stack));
            });
          } else if (isMap(value)) {
            value.forEach(function(subValue, key2) {
              result2.set(key2, baseClone(subValue, bitmask, customizer, key2, value, stack));
            });
          }
          var keysFunc = isFull ? isFlat ? getAllKeysIn : getAllKeys : isFlat ? keysIn : keys;
          var props = isArr ? undefined2 : keysFunc(value);
          arrayEach(props || value, function(subValue, key2) {
            if (props) {
              key2 = subValue;
              subValue = value[key2];
            }
            assignValue(result2, key2, baseClone(subValue, bitmask, customizer, key2, value, stack));
          });
          return result2;
        }
        function baseConforms(source) {
          var props = keys(source);
          return function(object) {
            return baseConformsTo(object, source, props);
          };
        }
        function baseConformsTo(object, source, props) {
          var length = props.length;
          if (object == null) {
            return !length;
          }
          object = Object2(object);
          while (length--) {
            var key = props[length], predicate = source[key], value = object[key];
            if (value === undefined2 && !(key in object) || !predicate(value)) {
              return false;
            }
          }
          return true;
        }
        function baseDelay(func, wait, args) {
          if (typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          return setTimeout2(function() {
            func.apply(undefined2, args);
          }, wait);
        }
        function baseDifference(array, values2, iteratee2, comparator) {
          var index = -1, includes2 = arrayIncludes, isCommon = true, length = array.length, result2 = [], valuesLength = values2.length;
          if (!length) {
            return result2;
          }
          if (iteratee2) {
            values2 = arrayMap(values2, baseUnary(iteratee2));
          }
          if (comparator) {
            includes2 = arrayIncludesWith;
            isCommon = false;
          } else if (values2.length >= LARGE_ARRAY_SIZE) {
            includes2 = cacheHas;
            isCommon = false;
            values2 = new SetCache(values2);
          }
          outer:
            while (++index < length) {
              var value = array[index], computed = iteratee2 == null ? value : iteratee2(value);
              value = comparator || value !== 0 ? value : 0;
              if (isCommon && computed === computed) {
                var valuesIndex = valuesLength;
                while (valuesIndex--) {
                  if (values2[valuesIndex] === computed) {
                    continue outer;
                  }
                }
                result2.push(value);
              } else if (!includes2(values2, computed, comparator)) {
                result2.push(value);
              }
            }
          return result2;
        }
        var baseEach = createBaseEach(baseForOwn);
        var baseEachRight = createBaseEach(baseForOwnRight, true);
        function baseEvery(collection, predicate) {
          var result2 = true;
          baseEach(collection, function(value, index, collection2) {
            result2 = !!predicate(value, index, collection2);
            return result2;
          });
          return result2;
        }
        function baseExtremum(array, iteratee2, comparator) {
          var index = -1, length = array.length;
          while (++index < length) {
            var value = array[index], current = iteratee2(value);
            if (current != null && (computed === undefined2 ? current === current && !isSymbol(current) : comparator(current, computed))) {
              var computed = current, result2 = value;
            }
          }
          return result2;
        }
        function baseFill(array, value, start, end) {
          var length = array.length;
          start = toInteger(start);
          if (start < 0) {
            start = -start > length ? 0 : length + start;
          }
          end = end === undefined2 || end > length ? length : toInteger(end);
          if (end < 0) {
            end += length;
          }
          end = start > end ? 0 : toLength(end);
          while (start < end) {
            array[start++] = value;
          }
          return array;
        }
        function baseFilter(collection, predicate) {
          var result2 = [];
          baseEach(collection, function(value, index, collection2) {
            if (predicate(value, index, collection2)) {
              result2.push(value);
            }
          });
          return result2;
        }
        function baseFlatten(array, depth, predicate, isStrict, result2) {
          var index = -1, length = array.length;
          predicate || (predicate = isFlattenable);
          result2 || (result2 = []);
          while (++index < length) {
            var value = array[index];
            if (depth > 0 && predicate(value)) {
              if (depth > 1) {
                baseFlatten(value, depth - 1, predicate, isStrict, result2);
              } else {
                arrayPush(result2, value);
              }
            } else if (!isStrict) {
              result2[result2.length] = value;
            }
          }
          return result2;
        }
        var baseFor = createBaseFor();
        var baseForRight = createBaseFor(true);
        function baseForOwn(object, iteratee2) {
          return object && baseFor(object, iteratee2, keys);
        }
        function baseForOwnRight(object, iteratee2) {
          return object && baseForRight(object, iteratee2, keys);
        }
        function baseFunctions(object, props) {
          return arrayFilter(props, function(key) {
            return isFunction(object[key]);
          });
        }
        function baseGet(object, path6) {
          path6 = castPath(path6, object);
          var index = 0, length = path6.length;
          while (object != null && index < length) {
            object = object[toKey(path6[index++])];
          }
          return index && index == length ? object : undefined2;
        }
        function baseGetAllKeys(object, keysFunc, symbolsFunc) {
          var result2 = keysFunc(object);
          return isArray(object) ? result2 : arrayPush(result2, symbolsFunc(object));
        }
        function baseGetTag(value) {
          if (value == null) {
            return value === undefined2 ? undefinedTag : nullTag;
          }
          return symToStringTag && symToStringTag in Object2(value) ? getRawTag(value) : objectToString(value);
        }
        function baseGt(value, other) {
          return value > other;
        }
        function baseHas(object, key) {
          return object != null && hasOwnProperty.call(object, key);
        }
        function baseHasIn(object, key) {
          return object != null && key in Object2(object);
        }
        function baseInRange(number, start, end) {
          return number >= nativeMin(start, end) && number < nativeMax(start, end);
        }
        function baseIntersection(arrays, iteratee2, comparator) {
          var includes2 = comparator ? arrayIncludesWith : arrayIncludes, length = arrays[0].length, othLength = arrays.length, othIndex = othLength, caches = Array2(othLength), maxLength = Infinity, result2 = [];
          while (othIndex--) {
            var array = arrays[othIndex];
            if (othIndex && iteratee2) {
              array = arrayMap(array, baseUnary(iteratee2));
            }
            maxLength = nativeMin(array.length, maxLength);
            caches[othIndex] = !comparator && (iteratee2 || length >= 120 && array.length >= 120) ? new SetCache(othIndex && array) : undefined2;
          }
          array = arrays[0];
          var index = -1, seen = caches[0];
          outer:
            while (++index < length && result2.length < maxLength) {
              var value = array[index], computed = iteratee2 ? iteratee2(value) : value;
              value = comparator || value !== 0 ? value : 0;
              if (!(seen ? cacheHas(seen, computed) : includes2(result2, computed, comparator))) {
                othIndex = othLength;
                while (--othIndex) {
                  var cache = caches[othIndex];
                  if (!(cache ? cacheHas(cache, computed) : includes2(arrays[othIndex], computed, comparator))) {
                    continue outer;
                  }
                }
                if (seen) {
                  seen.push(computed);
                }
                result2.push(value);
              }
            }
          return result2;
        }
        function baseInverter(object, setter, iteratee2, accumulator) {
          baseForOwn(object, function(value, key, object2) {
            setter(accumulator, iteratee2(value), key, object2);
          });
          return accumulator;
        }
        function baseInvoke(object, path6, args) {
          path6 = castPath(path6, object);
          object = parent(object, path6);
          var func = object == null ? object : object[toKey(last(path6))];
          return func == null ? undefined2 : apply(func, object, args);
        }
        function baseIsArguments(value) {
          return isObjectLike(value) && baseGetTag(value) == argsTag;
        }
        function baseIsArrayBuffer(value) {
          return isObjectLike(value) && baseGetTag(value) == arrayBufferTag;
        }
        function baseIsDate(value) {
          return isObjectLike(value) && baseGetTag(value) == dateTag;
        }
        function baseIsEqual(value, other, bitmask, customizer, stack) {
          if (value === other) {
            return true;
          }
          if (value == null || other == null || !isObjectLike(value) && !isObjectLike(other)) {
            return value !== value && other !== other;
          }
          return baseIsEqualDeep(value, other, bitmask, customizer, baseIsEqual, stack);
        }
        function baseIsEqualDeep(object, other, bitmask, customizer, equalFunc, stack) {
          var objIsArr = isArray(object), othIsArr = isArray(other), objTag = objIsArr ? arrayTag : getTag(object), othTag = othIsArr ? arrayTag : getTag(other);
          objTag = objTag == argsTag ? objectTag : objTag;
          othTag = othTag == argsTag ? objectTag : othTag;
          var objIsObj = objTag == objectTag, othIsObj = othTag == objectTag, isSameTag = objTag == othTag;
          if (isSameTag && isBuffer(object)) {
            if (!isBuffer(other)) {
              return false;
            }
            objIsArr = true;
            objIsObj = false;
          }
          if (isSameTag && !objIsObj) {
            stack || (stack = new Stack2());
            return objIsArr || isTypedArray(object) ? equalArrays(object, other, bitmask, customizer, equalFunc, stack) : equalByTag(object, other, objTag, bitmask, customizer, equalFunc, stack);
          }
          if (!(bitmask & COMPARE_PARTIAL_FLAG)) {
            var objIsWrapped = objIsObj && hasOwnProperty.call(object, "__wrapped__"), othIsWrapped = othIsObj && hasOwnProperty.call(other, "__wrapped__");
            if (objIsWrapped || othIsWrapped) {
              var objUnwrapped = objIsWrapped ? object.value() : object, othUnwrapped = othIsWrapped ? other.value() : other;
              stack || (stack = new Stack2());
              return equalFunc(objUnwrapped, othUnwrapped, bitmask, customizer, stack);
            }
          }
          if (!isSameTag) {
            return false;
          }
          stack || (stack = new Stack2());
          return equalObjects(object, other, bitmask, customizer, equalFunc, stack);
        }
        function baseIsMap(value) {
          return isObjectLike(value) && getTag(value) == mapTag;
        }
        function baseIsMatch(object, source, matchData, customizer) {
          var index = matchData.length, length = index, noCustomizer = !customizer;
          if (object == null) {
            return !length;
          }
          object = Object2(object);
          while (index--) {
            var data = matchData[index];
            if (noCustomizer && data[2] ? data[1] !== object[data[0]] : !(data[0] in object)) {
              return false;
            }
          }
          while (++index < length) {
            data = matchData[index];
            var key = data[0], objValue = object[key], srcValue = data[1];
            if (noCustomizer && data[2]) {
              if (objValue === undefined2 && !(key in object)) {
                return false;
              }
            } else {
              var stack = new Stack2();
              if (customizer) {
                var result2 = customizer(objValue, srcValue, key, object, source, stack);
              }
              if (!(result2 === undefined2 ? baseIsEqual(srcValue, objValue, COMPARE_PARTIAL_FLAG | COMPARE_UNORDERED_FLAG, customizer, stack) : result2)) {
                return false;
              }
            }
          }
          return true;
        }
        function baseIsNative(value) {
          if (!isObject(value) || isMasked(value)) {
            return false;
          }
          var pattern = isFunction(value) ? reIsNative : reIsHostCtor;
          return pattern.test(toSource(value));
        }
        function baseIsRegExp(value) {
          return isObjectLike(value) && baseGetTag(value) == regexpTag;
        }
        function baseIsSet(value) {
          return isObjectLike(value) && getTag(value) == setTag;
        }
        function baseIsTypedArray(value) {
          return isObjectLike(value) && isLength(value.length) && !!typedArrayTags[baseGetTag(value)];
        }
        function baseIteratee(value) {
          if (typeof value == "function") {
            return value;
          }
          if (value == null) {
            return identity;
          }
          if (typeof value == "object") {
            return isArray(value) ? baseMatchesProperty(value[0], value[1]) : baseMatches(value);
          }
          return property(value);
        }
        function baseKeys(object) {
          if (!isPrototype(object)) {
            return nativeKeys(object);
          }
          var result2 = [];
          for (var key in Object2(object)) {
            if (hasOwnProperty.call(object, key) && key != "constructor") {
              result2.push(key);
            }
          }
          return result2;
        }
        function baseKeysIn(object) {
          if (!isObject(object)) {
            return nativeKeysIn(object);
          }
          var isProto = isPrototype(object), result2 = [];
          for (var key in object) {
            if (!(key == "constructor" && (isProto || !hasOwnProperty.call(object, key)))) {
              result2.push(key);
            }
          }
          return result2;
        }
        function baseLt(value, other) {
          return value < other;
        }
        function baseMap(collection, iteratee2) {
          var index = -1, result2 = isArrayLike(collection) ? Array2(collection.length) : [];
          baseEach(collection, function(value, key, collection2) {
            result2[++index] = iteratee2(value, key, collection2);
          });
          return result2;
        }
        function baseMatches(source) {
          var matchData = getMatchData(source);
          if (matchData.length == 1 && matchData[0][2]) {
            return matchesStrictComparable(matchData[0][0], matchData[0][1]);
          }
          return function(object) {
            return object === source || baseIsMatch(object, source, matchData);
          };
        }
        function baseMatchesProperty(path6, srcValue) {
          if (isKey(path6) && isStrictComparable(srcValue)) {
            return matchesStrictComparable(toKey(path6), srcValue);
          }
          return function(object) {
            var objValue = get(object, path6);
            return objValue === undefined2 && objValue === srcValue ? hasIn(object, path6) : baseIsEqual(srcValue, objValue, COMPARE_PARTIAL_FLAG | COMPARE_UNORDERED_FLAG);
          };
        }
        function baseMerge(object, source, srcIndex, customizer, stack) {
          if (object === source) {
            return;
          }
          baseFor(source, function(srcValue, key) {
            stack || (stack = new Stack2());
            if (isObject(srcValue)) {
              baseMergeDeep(object, source, key, srcIndex, baseMerge, customizer, stack);
            } else {
              var newValue = customizer ? customizer(safeGet(object, key), srcValue, key + "", object, source, stack) : undefined2;
              if (newValue === undefined2) {
                newValue = srcValue;
              }
              assignMergeValue(object, key, newValue);
            }
          }, keysIn);
        }
        function baseMergeDeep(object, source, key, srcIndex, mergeFunc, customizer, stack) {
          var objValue = safeGet(object, key), srcValue = safeGet(source, key), stacked = stack.get(srcValue);
          if (stacked) {
            assignMergeValue(object, key, stacked);
            return;
          }
          var newValue = customizer ? customizer(objValue, srcValue, key + "", object, source, stack) : undefined2;
          var isCommon = newValue === undefined2;
          if (isCommon) {
            var isArr = isArray(srcValue), isBuff = !isArr && isBuffer(srcValue), isTyped = !isArr && !isBuff && isTypedArray(srcValue);
            newValue = srcValue;
            if (isArr || isBuff || isTyped) {
              if (isArray(objValue)) {
                newValue = objValue;
              } else if (isArrayLikeObject(objValue)) {
                newValue = copyArray(objValue);
              } else if (isBuff) {
                isCommon = false;
                newValue = cloneBuffer(srcValue, true);
              } else if (isTyped) {
                isCommon = false;
                newValue = cloneTypedArray(srcValue, true);
              } else {
                newValue = [];
              }
            } else if (isPlainObject(srcValue) || isArguments(srcValue)) {
              newValue = objValue;
              if (isArguments(objValue)) {
                newValue = toPlainObject(objValue);
              } else if (!isObject(objValue) || isFunction(objValue)) {
                newValue = initCloneObject(srcValue);
              }
            } else {
              isCommon = false;
            }
          }
          if (isCommon) {
            stack.set(srcValue, newValue);
            mergeFunc(newValue, srcValue, srcIndex, customizer, stack);
            stack["delete"](srcValue);
          }
          assignMergeValue(object, key, newValue);
        }
        function baseNth(array, n) {
          var length = array.length;
          if (!length) {
            return;
          }
          n += n < 0 ? length : 0;
          return isIndex(n, length) ? array[n] : undefined2;
        }
        function baseOrderBy(collection, iteratees, orders) {
          if (iteratees.length) {
            iteratees = arrayMap(iteratees, function(iteratee2) {
              if (isArray(iteratee2)) {
                return function(value) {
                  return baseGet(value, iteratee2.length === 1 ? iteratee2[0] : iteratee2);
                };
              }
              return iteratee2;
            });
          } else {
            iteratees = [identity];
          }
          var index = -1;
          iteratees = arrayMap(iteratees, baseUnary(getIteratee()));
          var result2 = baseMap(collection, function(value, key, collection2) {
            var criteria = arrayMap(iteratees, function(iteratee2) {
              return iteratee2(value);
            });
            return { "criteria": criteria, "index": ++index, "value": value };
          });
          return baseSortBy(result2, function(object, other) {
            return compareMultiple(object, other, orders);
          });
        }
        function basePick(object, paths) {
          return basePickBy(object, paths, function(value, path6) {
            return hasIn(object, path6);
          });
        }
        function basePickBy(object, paths, predicate) {
          var index = -1, length = paths.length, result2 = {};
          while (++index < length) {
            var path6 = paths[index], value = baseGet(object, path6);
            if (predicate(value, path6)) {
              baseSet(result2, castPath(path6, object), value);
            }
          }
          return result2;
        }
        function basePropertyDeep(path6) {
          return function(object) {
            return baseGet(object, path6);
          };
        }
        function basePullAll(array, values2, iteratee2, comparator) {
          var indexOf2 = comparator ? baseIndexOfWith : baseIndexOf, index = -1, length = values2.length, seen = array;
          if (array === values2) {
            values2 = copyArray(values2);
          }
          if (iteratee2) {
            seen = arrayMap(array, baseUnary(iteratee2));
          }
          while (++index < length) {
            var fromIndex = 0, value = values2[index], computed = iteratee2 ? iteratee2(value) : value;
            while ((fromIndex = indexOf2(seen, computed, fromIndex, comparator)) > -1) {
              if (seen !== array) {
                splice.call(seen, fromIndex, 1);
              }
              splice.call(array, fromIndex, 1);
            }
          }
          return array;
        }
        function basePullAt(array, indexes) {
          var length = array ? indexes.length : 0, lastIndex = length - 1;
          while (length--) {
            var index = indexes[length];
            if (length == lastIndex || index !== previous) {
              var previous = index;
              if (isIndex(index)) {
                splice.call(array, index, 1);
              } else {
                baseUnset(array, index);
              }
            }
          }
          return array;
        }
        function baseRandom(lower, upper) {
          return lower + nativeFloor(nativeRandom() * (upper - lower + 1));
        }
        function baseRange(start, end, step, fromRight) {
          var index = -1, length = nativeMax(nativeCeil((end - start) / (step || 1)), 0), result2 = Array2(length);
          while (length--) {
            result2[fromRight ? length : ++index] = start;
            start += step;
          }
          return result2;
        }
        function baseRepeat(string, n) {
          var result2 = "";
          if (!string || n < 1 || n > MAX_SAFE_INTEGER) {
            return result2;
          }
          do {
            if (n % 2) {
              result2 += string;
            }
            n = nativeFloor(n / 2);
            if (n) {
              string += string;
            }
          } while (n);
          return result2;
        }
        function baseRest(func, start) {
          return setToString(overRest(func, start, identity), func + "");
        }
        function baseSample(collection) {
          return arraySample(values(collection));
        }
        function baseSampleSize(collection, n) {
          var array = values(collection);
          return shuffleSelf(array, baseClamp(n, 0, array.length));
        }
        function baseSet(object, path6, value, customizer) {
          if (!isObject(object)) {
            return object;
          }
          path6 = castPath(path6, object);
          var index = -1, length = path6.length, lastIndex = length - 1, nested = object;
          while (nested != null && ++index < length) {
            var key = toKey(path6[index]), newValue = value;
            if (key === "__proto__" || key === "constructor" || key === "prototype") {
              return object;
            }
            if (index != lastIndex) {
              var objValue = nested[key];
              newValue = customizer ? customizer(objValue, key, nested) : undefined2;
              if (newValue === undefined2) {
                newValue = isObject(objValue) ? objValue : isIndex(path6[index + 1]) ? [] : {};
              }
            }
            assignValue(nested, key, newValue);
            nested = nested[key];
          }
          return object;
        }
        var baseSetData = !metaMap ? identity : function(func, data) {
          metaMap.set(func, data);
          return func;
        };
        var baseSetToString = !defineProperty ? identity : function(func, string) {
          return defineProperty(func, "toString", {
            "configurable": true,
            "enumerable": false,
            "value": constant(string),
            "writable": true
          });
        };
        function baseShuffle(collection) {
          return shuffleSelf(values(collection));
        }
        function baseSlice(array, start, end) {
          var index = -1, length = array.length;
          if (start < 0) {
            start = -start > length ? 0 : length + start;
          }
          end = end > length ? length : end;
          if (end < 0) {
            end += length;
          }
          length = start > end ? 0 : end - start >>> 0;
          start >>>= 0;
          var result2 = Array2(length);
          while (++index < length) {
            result2[index] = array[index + start];
          }
          return result2;
        }
        function baseSome(collection, predicate) {
          var result2;
          baseEach(collection, function(value, index, collection2) {
            result2 = predicate(value, index, collection2);
            return !result2;
          });
          return !!result2;
        }
        function baseSortedIndex(array, value, retHighest) {
          var low = 0, high = array == null ? low : array.length;
          if (typeof value == "number" && value === value && high <= HALF_MAX_ARRAY_LENGTH) {
            while (low < high) {
              var mid = low + high >>> 1, computed = array[mid];
              if (computed !== null && !isSymbol(computed) && (retHighest ? computed <= value : computed < value)) {
                low = mid + 1;
              } else {
                high = mid;
              }
            }
            return high;
          }
          return baseSortedIndexBy(array, value, identity, retHighest);
        }
        function baseSortedIndexBy(array, value, iteratee2, retHighest) {
          var low = 0, high = array == null ? 0 : array.length;
          if (high === 0) {
            return 0;
          }
          value = iteratee2(value);
          var valIsNaN = value !== value, valIsNull = value === null, valIsSymbol = isSymbol(value), valIsUndefined = value === undefined2;
          while (low < high) {
            var mid = nativeFloor((low + high) / 2), computed = iteratee2(array[mid]), othIsDefined = computed !== undefined2, othIsNull = computed === null, othIsReflexive = computed === computed, othIsSymbol = isSymbol(computed);
            if (valIsNaN) {
              var setLow = retHighest || othIsReflexive;
            } else if (valIsUndefined) {
              setLow = othIsReflexive && (retHighest || othIsDefined);
            } else if (valIsNull) {
              setLow = othIsReflexive && othIsDefined && (retHighest || !othIsNull);
            } else if (valIsSymbol) {
              setLow = othIsReflexive && othIsDefined && !othIsNull && (retHighest || !othIsSymbol);
            } else if (othIsNull || othIsSymbol) {
              setLow = false;
            } else {
              setLow = retHighest ? computed <= value : computed < value;
            }
            if (setLow) {
              low = mid + 1;
            } else {
              high = mid;
            }
          }
          return nativeMin(high, MAX_ARRAY_INDEX);
        }
        function baseSortedUniq(array, iteratee2) {
          var index = -1, length = array.length, resIndex = 0, result2 = [];
          while (++index < length) {
            var value = array[index], computed = iteratee2 ? iteratee2(value) : value;
            if (!index || !eq(computed, seen)) {
              var seen = computed;
              result2[resIndex++] = value === 0 ? 0 : value;
            }
          }
          return result2;
        }
        function baseToNumber(value) {
          if (typeof value == "number") {
            return value;
          }
          if (isSymbol(value)) {
            return NAN;
          }
          return +value;
        }
        function baseToString(value) {
          if (typeof value == "string") {
            return value;
          }
          if (isArray(value)) {
            return arrayMap(value, baseToString) + "";
          }
          if (isSymbol(value)) {
            return symbolToString ? symbolToString.call(value) : "";
          }
          var result2 = value + "";
          return result2 == "0" && 1 / value == -INFINITY ? "-0" : result2;
        }
        function baseUniq(array, iteratee2, comparator) {
          var index = -1, includes2 = arrayIncludes, length = array.length, isCommon = true, result2 = [], seen = result2;
          if (comparator) {
            isCommon = false;
            includes2 = arrayIncludesWith;
          } else if (length >= LARGE_ARRAY_SIZE) {
            var set2 = iteratee2 ? null : createSet(array);
            if (set2) {
              return setToArray(set2);
            }
            isCommon = false;
            includes2 = cacheHas;
            seen = new SetCache();
          } else {
            seen = iteratee2 ? [] : result2;
          }
          outer:
            while (++index < length) {
              var value = array[index], computed = iteratee2 ? iteratee2(value) : value;
              value = comparator || value !== 0 ? value : 0;
              if (isCommon && computed === computed) {
                var seenIndex = seen.length;
                while (seenIndex--) {
                  if (seen[seenIndex] === computed) {
                    continue outer;
                  }
                }
                if (iteratee2) {
                  seen.push(computed);
                }
                result2.push(value);
              } else if (!includes2(seen, computed, comparator)) {
                if (seen !== result2) {
                  seen.push(computed);
                }
                result2.push(value);
              }
            }
          return result2;
        }
        function baseUnset(object, path6) {
          path6 = castPath(path6, object);
          object = parent(object, path6);
          return object == null || delete object[toKey(last(path6))];
        }
        function baseUpdate(object, path6, updater, customizer) {
          return baseSet(object, path6, updater(baseGet(object, path6)), customizer);
        }
        function baseWhile(array, predicate, isDrop, fromRight) {
          var length = array.length, index = fromRight ? length : -1;
          while ((fromRight ? index-- : ++index < length) && predicate(array[index], index, array)) {
          }
          return isDrop ? baseSlice(array, fromRight ? 0 : index, fromRight ? index + 1 : length) : baseSlice(array, fromRight ? index + 1 : 0, fromRight ? length : index);
        }
        function baseWrapperValue(value, actions) {
          var result2 = value;
          if (result2 instanceof LazyWrapper) {
            result2 = result2.value();
          }
          return arrayReduce(actions, function(result3, action) {
            return action.func.apply(action.thisArg, arrayPush([result3], action.args));
          }, result2);
        }
        function baseXor(arrays, iteratee2, comparator) {
          var length = arrays.length;
          if (length < 2) {
            return length ? baseUniq(arrays[0]) : [];
          }
          var index = -1, result2 = Array2(length);
          while (++index < length) {
            var array = arrays[index], othIndex = -1;
            while (++othIndex < length) {
              if (othIndex != index) {
                result2[index] = baseDifference(result2[index] || array, arrays[othIndex], iteratee2, comparator);
              }
            }
          }
          return baseUniq(baseFlatten(result2, 1), iteratee2, comparator);
        }
        function baseZipObject(props, values2, assignFunc) {
          var index = -1, length = props.length, valsLength = values2.length, result2 = {};
          while (++index < length) {
            var value = index < valsLength ? values2[index] : undefined2;
            assignFunc(result2, props[index], value);
          }
          return result2;
        }
        function castArrayLikeObject(value) {
          return isArrayLikeObject(value) ? value : [];
        }
        function castFunction(value) {
          return typeof value == "function" ? value : identity;
        }
        function castPath(value, object) {
          if (isArray(value)) {
            return value;
          }
          return isKey(value, object) ? [value] : stringToPath(toString(value));
        }
        var castRest = baseRest;
        function castSlice(array, start, end) {
          var length = array.length;
          end = end === undefined2 ? length : end;
          return !start && end >= length ? array : baseSlice(array, start, end);
        }
        var clearTimeout2 = ctxClearTimeout || function(id) {
          return root.clearTimeout(id);
        };
        function cloneBuffer(buffer, isDeep) {
          if (isDeep) {
            return buffer.slice();
          }
          var length = buffer.length, result2 = allocUnsafe ? allocUnsafe(length) : new buffer.constructor(length);
          buffer.copy(result2);
          return result2;
        }
        function cloneArrayBuffer(arrayBuffer) {
          var result2 = new arrayBuffer.constructor(arrayBuffer.byteLength);
          new Uint8Array2(result2).set(new Uint8Array2(arrayBuffer));
          return result2;
        }
        function cloneDataView(dataView, isDeep) {
          var buffer = isDeep ? cloneArrayBuffer(dataView.buffer) : dataView.buffer;
          return new dataView.constructor(buffer, dataView.byteOffset, dataView.byteLength);
        }
        function cloneRegExp(regexp) {
          var result2 = new regexp.constructor(regexp.source, reFlags.exec(regexp));
          result2.lastIndex = regexp.lastIndex;
          return result2;
        }
        function cloneSymbol(symbol) {
          return symbolValueOf ? Object2(symbolValueOf.call(symbol)) : {};
        }
        function cloneTypedArray(typedArray, isDeep) {
          var buffer = isDeep ? cloneArrayBuffer(typedArray.buffer) : typedArray.buffer;
          return new typedArray.constructor(buffer, typedArray.byteOffset, typedArray.length);
        }
        function compareAscending(value, other) {
          if (value !== other) {
            var valIsDefined = value !== undefined2, valIsNull = value === null, valIsReflexive = value === value, valIsSymbol = isSymbol(value);
            var othIsDefined = other !== undefined2, othIsNull = other === null, othIsReflexive = other === other, othIsSymbol = isSymbol(other);
            if (!othIsNull && !othIsSymbol && !valIsSymbol && value > other || valIsSymbol && othIsDefined && othIsReflexive && !othIsNull && !othIsSymbol || valIsNull && othIsDefined && othIsReflexive || !valIsDefined && othIsReflexive || !valIsReflexive) {
              return 1;
            }
            if (!valIsNull && !valIsSymbol && !othIsSymbol && value < other || othIsSymbol && valIsDefined && valIsReflexive && !valIsNull && !valIsSymbol || othIsNull && valIsDefined && valIsReflexive || !othIsDefined && valIsReflexive || !othIsReflexive) {
              return -1;
            }
          }
          return 0;
        }
        function compareMultiple(object, other, orders) {
          var index = -1, objCriteria = object.criteria, othCriteria = other.criteria, length = objCriteria.length, ordersLength = orders.length;
          while (++index < length) {
            var result2 = compareAscending(objCriteria[index], othCriteria[index]);
            if (result2) {
              if (index >= ordersLength) {
                return result2;
              }
              var order = orders[index];
              return result2 * (order == "desc" ? -1 : 1);
            }
          }
          return object.index - other.index;
        }
        function composeArgs(args, partials, holders, isCurried) {
          var argsIndex = -1, argsLength = args.length, holdersLength = holders.length, leftIndex = -1, leftLength = partials.length, rangeLength = nativeMax(argsLength - holdersLength, 0), result2 = Array2(leftLength + rangeLength), isUncurried = !isCurried;
          while (++leftIndex < leftLength) {
            result2[leftIndex] = partials[leftIndex];
          }
          while (++argsIndex < holdersLength) {
            if (isUncurried || argsIndex < argsLength) {
              result2[holders[argsIndex]] = args[argsIndex];
            }
          }
          while (rangeLength--) {
            result2[leftIndex++] = args[argsIndex++];
          }
          return result2;
        }
        function composeArgsRight(args, partials, holders, isCurried) {
          var argsIndex = -1, argsLength = args.length, holdersIndex = -1, holdersLength = holders.length, rightIndex = -1, rightLength = partials.length, rangeLength = nativeMax(argsLength - holdersLength, 0), result2 = Array2(rangeLength + rightLength), isUncurried = !isCurried;
          while (++argsIndex < rangeLength) {
            result2[argsIndex] = args[argsIndex];
          }
          var offset = argsIndex;
          while (++rightIndex < rightLength) {
            result2[offset + rightIndex] = partials[rightIndex];
          }
          while (++holdersIndex < holdersLength) {
            if (isUncurried || argsIndex < argsLength) {
              result2[offset + holders[holdersIndex]] = args[argsIndex++];
            }
          }
          return result2;
        }
        function copyArray(source, array) {
          var index = -1, length = source.length;
          array || (array = Array2(length));
          while (++index < length) {
            array[index] = source[index];
          }
          return array;
        }
        function copyObject(source, props, object, customizer) {
          var isNew = !object;
          object || (object = {});
          var index = -1, length = props.length;
          while (++index < length) {
            var key = props[index];
            var newValue = customizer ? customizer(object[key], source[key], key, object, source) : undefined2;
            if (newValue === undefined2) {
              newValue = source[key];
            }
            if (isNew) {
              baseAssignValue(object, key, newValue);
            } else {
              assignValue(object, key, newValue);
            }
          }
          return object;
        }
        function copySymbols(source, object) {
          return copyObject(source, getSymbols(source), object);
        }
        function copySymbolsIn(source, object) {
          return copyObject(source, getSymbolsIn(source), object);
        }
        function createAggregator(setter, initializer) {
          return function(collection, iteratee2) {
            var func = isArray(collection) ? arrayAggregator : baseAggregator, accumulator = initializer ? initializer() : {};
            return func(collection, setter, getIteratee(iteratee2, 2), accumulator);
          };
        }
        function createAssigner(assigner) {
          return baseRest(function(object, sources) {
            var index = -1, length = sources.length, customizer = length > 1 ? sources[length - 1] : undefined2, guard = length > 2 ? sources[2] : undefined2;
            customizer = assigner.length > 3 && typeof customizer == "function" ? (length--, customizer) : undefined2;
            if (guard && isIterateeCall(sources[0], sources[1], guard)) {
              customizer = length < 3 ? undefined2 : customizer;
              length = 1;
            }
            object = Object2(object);
            while (++index < length) {
              var source = sources[index];
              if (source) {
                assigner(object, source, index, customizer);
              }
            }
            return object;
          });
        }
        function createBaseEach(eachFunc, fromRight) {
          return function(collection, iteratee2) {
            if (collection == null) {
              return collection;
            }
            if (!isArrayLike(collection)) {
              return eachFunc(collection, iteratee2);
            }
            var length = collection.length, index = fromRight ? length : -1, iterable = Object2(collection);
            while (fromRight ? index-- : ++index < length) {
              if (iteratee2(iterable[index], index, iterable) === false) {
                break;
              }
            }
            return collection;
          };
        }
        function createBaseFor(fromRight) {
          return function(object, iteratee2, keysFunc) {
            var index = -1, iterable = Object2(object), props = keysFunc(object), length = props.length;
            while (length--) {
              var key = props[fromRight ? length : ++index];
              if (iteratee2(iterable[key], key, iterable) === false) {
                break;
              }
            }
            return object;
          };
        }
        function createBind(func, bitmask, thisArg) {
          var isBind = bitmask & WRAP_BIND_FLAG, Ctor = createCtor(func);
          function wrapper() {
            var fn = this && this !== root && this instanceof wrapper ? Ctor : func;
            return fn.apply(isBind ? thisArg : this, arguments);
          }
          return wrapper;
        }
        function createCaseFirst(methodName) {
          return function(string) {
            string = toString(string);
            var strSymbols = hasUnicode(string) ? stringToArray(string) : undefined2;
            var chr = strSymbols ? strSymbols[0] : string.charAt(0);
            var trailing = strSymbols ? castSlice(strSymbols, 1).join("") : string.slice(1);
            return chr[methodName]() + trailing;
          };
        }
        function createCompounder(callback) {
          return function(string) {
            return arrayReduce(words(deburr(string).replace(reApos, "")), callback, "");
          };
        }
        function createCtor(Ctor) {
          return function() {
            var args = arguments;
            switch (args.length) {
              case 0:
                return new Ctor();
              case 1:
                return new Ctor(args[0]);
              case 2:
                return new Ctor(args[0], args[1]);
              case 3:
                return new Ctor(args[0], args[1], args[2]);
              case 4:
                return new Ctor(args[0], args[1], args[2], args[3]);
              case 5:
                return new Ctor(args[0], args[1], args[2], args[3], args[4]);
              case 6:
                return new Ctor(args[0], args[1], args[2], args[3], args[4], args[5]);
              case 7:
                return new Ctor(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
            }
            var thisBinding = baseCreate(Ctor.prototype), result2 = Ctor.apply(thisBinding, args);
            return isObject(result2) ? result2 : thisBinding;
          };
        }
        function createCurry(func, bitmask, arity) {
          var Ctor = createCtor(func);
          function wrapper() {
            var length = arguments.length, args = Array2(length), index = length, placeholder = getHolder(wrapper);
            while (index--) {
              args[index] = arguments[index];
            }
            var holders = length < 3 && args[0] !== placeholder && args[length - 1] !== placeholder ? [] : replaceHolders(args, placeholder);
            length -= holders.length;
            if (length < arity) {
              return createRecurry(
                func,
                bitmask,
                createHybrid,
                wrapper.placeholder,
                undefined2,
                args,
                holders,
                undefined2,
                undefined2,
                arity - length
              );
            }
            var fn = this && this !== root && this instanceof wrapper ? Ctor : func;
            return apply(fn, this, args);
          }
          return wrapper;
        }
        function createFind(findIndexFunc) {
          return function(collection, predicate, fromIndex) {
            var iterable = Object2(collection);
            if (!isArrayLike(collection)) {
              var iteratee2 = getIteratee(predicate, 3);
              collection = keys(collection);
              predicate = function(key) {
                return iteratee2(iterable[key], key, iterable);
              };
            }
            var index = findIndexFunc(collection, predicate, fromIndex);
            return index > -1 ? iterable[iteratee2 ? collection[index] : index] : undefined2;
          };
        }
        function createFlow(fromRight) {
          return flatRest(function(funcs) {
            var length = funcs.length, index = length, prereq = LodashWrapper.prototype.thru;
            if (fromRight) {
              funcs.reverse();
            }
            while (index--) {
              var func = funcs[index];
              if (typeof func != "function") {
                throw new TypeError2(FUNC_ERROR_TEXT);
              }
              if (prereq && !wrapper && getFuncName(func) == "wrapper") {
                var wrapper = new LodashWrapper([], true);
              }
            }
            index = wrapper ? index : length;
            while (++index < length) {
              func = funcs[index];
              var funcName = getFuncName(func), data = funcName == "wrapper" ? getData(func) : undefined2;
              if (data && isLaziable(data[0]) && data[1] == (WRAP_ARY_FLAG | WRAP_CURRY_FLAG | WRAP_PARTIAL_FLAG | WRAP_REARG_FLAG) && !data[4].length && data[9] == 1) {
                wrapper = wrapper[getFuncName(data[0])].apply(wrapper, data[3]);
              } else {
                wrapper = func.length == 1 && isLaziable(func) ? wrapper[funcName]() : wrapper.thru(func);
              }
            }
            return function() {
              var args = arguments, value = args[0];
              if (wrapper && args.length == 1 && isArray(value)) {
                return wrapper.plant(value).value();
              }
              var index2 = 0, result2 = length ? funcs[index2].apply(this, args) : value;
              while (++index2 < length) {
                result2 = funcs[index2].call(this, result2);
              }
              return result2;
            };
          });
        }
        function createHybrid(func, bitmask, thisArg, partials, holders, partialsRight, holdersRight, argPos, ary2, arity) {
          var isAry = bitmask & WRAP_ARY_FLAG, isBind = bitmask & WRAP_BIND_FLAG, isBindKey = bitmask & WRAP_BIND_KEY_FLAG, isCurried = bitmask & (WRAP_CURRY_FLAG | WRAP_CURRY_RIGHT_FLAG), isFlip = bitmask & WRAP_FLIP_FLAG, Ctor = isBindKey ? undefined2 : createCtor(func);
          function wrapper() {
            var length = arguments.length, args = Array2(length), index = length;
            while (index--) {
              args[index] = arguments[index];
            }
            if (isCurried) {
              var placeholder = getHolder(wrapper), holdersCount = countHolders(args, placeholder);
            }
            if (partials) {
              args = composeArgs(args, partials, holders, isCurried);
            }
            if (partialsRight) {
              args = composeArgsRight(args, partialsRight, holdersRight, isCurried);
            }
            length -= holdersCount;
            if (isCurried && length < arity) {
              var newHolders = replaceHolders(args, placeholder);
              return createRecurry(
                func,
                bitmask,
                createHybrid,
                wrapper.placeholder,
                thisArg,
                args,
                newHolders,
                argPos,
                ary2,
                arity - length
              );
            }
            var thisBinding = isBind ? thisArg : this, fn = isBindKey ? thisBinding[func] : func;
            length = args.length;
            if (argPos) {
              args = reorder(args, argPos);
            } else if (isFlip && length > 1) {
              args.reverse();
            }
            if (isAry && ary2 < length) {
              args.length = ary2;
            }
            if (this && this !== root && this instanceof wrapper) {
              fn = Ctor || createCtor(fn);
            }
            return fn.apply(thisBinding, args);
          }
          return wrapper;
        }
        function createInverter(setter, toIteratee) {
          return function(object, iteratee2) {
            return baseInverter(object, setter, toIteratee(iteratee2), {});
          };
        }
        function createMathOperation(operator, defaultValue) {
          return function(value, other) {
            var result2;
            if (value === undefined2 && other === undefined2) {
              return defaultValue;
            }
            if (value !== undefined2) {
              result2 = value;
            }
            if (other !== undefined2) {
              if (result2 === undefined2) {
                return other;
              }
              if (typeof value == "string" || typeof other == "string") {
                value = baseToString(value);
                other = baseToString(other);
              } else {
                value = baseToNumber(value);
                other = baseToNumber(other);
              }
              result2 = operator(value, other);
            }
            return result2;
          };
        }
        function createOver(arrayFunc) {
          return flatRest(function(iteratees) {
            iteratees = arrayMap(iteratees, baseUnary(getIteratee()));
            return baseRest(function(args) {
              var thisArg = this;
              return arrayFunc(iteratees, function(iteratee2) {
                return apply(iteratee2, thisArg, args);
              });
            });
          });
        }
        function createPadding(length, chars) {
          chars = chars === undefined2 ? " " : baseToString(chars);
          var charsLength = chars.length;
          if (charsLength < 2) {
            return charsLength ? baseRepeat(chars, length) : chars;
          }
          var result2 = baseRepeat(chars, nativeCeil(length / stringSize(chars)));
          return hasUnicode(chars) ? castSlice(stringToArray(result2), 0, length).join("") : result2.slice(0, length);
        }
        function createPartial(func, bitmask, thisArg, partials) {
          var isBind = bitmask & WRAP_BIND_FLAG, Ctor = createCtor(func);
          function wrapper() {
            var argsIndex = -1, argsLength = arguments.length, leftIndex = -1, leftLength = partials.length, args = Array2(leftLength + argsLength), fn = this && this !== root && this instanceof wrapper ? Ctor : func;
            while (++leftIndex < leftLength) {
              args[leftIndex] = partials[leftIndex];
            }
            while (argsLength--) {
              args[leftIndex++] = arguments[++argsIndex];
            }
            return apply(fn, isBind ? thisArg : this, args);
          }
          return wrapper;
        }
        function createRange(fromRight) {
          return function(start, end, step) {
            if (step && typeof step != "number" && isIterateeCall(start, end, step)) {
              end = step = undefined2;
            }
            start = toFinite(start);
            if (end === undefined2) {
              end = start;
              start = 0;
            } else {
              end = toFinite(end);
            }
            step = step === undefined2 ? start < end ? 1 : -1 : toFinite(step);
            return baseRange(start, end, step, fromRight);
          };
        }
        function createRelationalOperation(operator) {
          return function(value, other) {
            if (!(typeof value == "string" && typeof other == "string")) {
              value = toNumber(value);
              other = toNumber(other);
            }
            return operator(value, other);
          };
        }
        function createRecurry(func, bitmask, wrapFunc, placeholder, thisArg, partials, holders, argPos, ary2, arity) {
          var isCurry = bitmask & WRAP_CURRY_FLAG, newHolders = isCurry ? holders : undefined2, newHoldersRight = isCurry ? undefined2 : holders, newPartials = isCurry ? partials : undefined2, newPartialsRight = isCurry ? undefined2 : partials;
          bitmask |= isCurry ? WRAP_PARTIAL_FLAG : WRAP_PARTIAL_RIGHT_FLAG;
          bitmask &= ~(isCurry ? WRAP_PARTIAL_RIGHT_FLAG : WRAP_PARTIAL_FLAG);
          if (!(bitmask & WRAP_CURRY_BOUND_FLAG)) {
            bitmask &= ~(WRAP_BIND_FLAG | WRAP_BIND_KEY_FLAG);
          }
          var newData = [
            func,
            bitmask,
            thisArg,
            newPartials,
            newHolders,
            newPartialsRight,
            newHoldersRight,
            argPos,
            ary2,
            arity
          ];
          var result2 = wrapFunc.apply(undefined2, newData);
          if (isLaziable(func)) {
            setData(result2, newData);
          }
          result2.placeholder = placeholder;
          return setWrapToString(result2, func, bitmask);
        }
        function createRound(methodName) {
          var func = Math2[methodName];
          return function(number, precision) {
            number = toNumber(number);
            precision = precision == null ? 0 : nativeMin(toInteger(precision), 292);
            if (precision && nativeIsFinite(number)) {
              var pair = (toString(number) + "e").split("e"), value = func(pair[0] + "e" + (+pair[1] + precision));
              pair = (toString(value) + "e").split("e");
              return +(pair[0] + "e" + (+pair[1] - precision));
            }
            return func(number);
          };
        }
        var createSet = !(Set2 && 1 / setToArray(new Set2([, -0]))[1] == INFINITY) ? noop : function(values2) {
          return new Set2(values2);
        };
        function createToPairs(keysFunc) {
          return function(object) {
            var tag = getTag(object);
            if (tag == mapTag) {
              return mapToArray(object);
            }
            if (tag == setTag) {
              return setToPairs(object);
            }
            return baseToPairs(object, keysFunc(object));
          };
        }
        function createWrap(func, bitmask, thisArg, partials, holders, argPos, ary2, arity) {
          var isBindKey = bitmask & WRAP_BIND_KEY_FLAG;
          if (!isBindKey && typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          var length = partials ? partials.length : 0;
          if (!length) {
            bitmask &= ~(WRAP_PARTIAL_FLAG | WRAP_PARTIAL_RIGHT_FLAG);
            partials = holders = undefined2;
          }
          ary2 = ary2 === undefined2 ? ary2 : nativeMax(toInteger(ary2), 0);
          arity = arity === undefined2 ? arity : toInteger(arity);
          length -= holders ? holders.length : 0;
          if (bitmask & WRAP_PARTIAL_RIGHT_FLAG) {
            var partialsRight = partials, holdersRight = holders;
            partials = holders = undefined2;
          }
          var data = isBindKey ? undefined2 : getData(func);
          var newData = [
            func,
            bitmask,
            thisArg,
            partials,
            holders,
            partialsRight,
            holdersRight,
            argPos,
            ary2,
            arity
          ];
          if (data) {
            mergeData(newData, data);
          }
          func = newData[0];
          bitmask = newData[1];
          thisArg = newData[2];
          partials = newData[3];
          holders = newData[4];
          arity = newData[9] = newData[9] === undefined2 ? isBindKey ? 0 : func.length : nativeMax(newData[9] - length, 0);
          if (!arity && bitmask & (WRAP_CURRY_FLAG | WRAP_CURRY_RIGHT_FLAG)) {
            bitmask &= ~(WRAP_CURRY_FLAG | WRAP_CURRY_RIGHT_FLAG);
          }
          if (!bitmask || bitmask == WRAP_BIND_FLAG) {
            var result2 = createBind(func, bitmask, thisArg);
          } else if (bitmask == WRAP_CURRY_FLAG || bitmask == WRAP_CURRY_RIGHT_FLAG) {
            result2 = createCurry(func, bitmask, arity);
          } else if ((bitmask == WRAP_PARTIAL_FLAG || bitmask == (WRAP_BIND_FLAG | WRAP_PARTIAL_FLAG)) && !holders.length) {
            result2 = createPartial(func, bitmask, thisArg, partials);
          } else {
            result2 = createHybrid.apply(undefined2, newData);
          }
          var setter = data ? baseSetData : setData;
          return setWrapToString(setter(result2, newData), func, bitmask);
        }
        function customDefaultsAssignIn(objValue, srcValue, key, object) {
          if (objValue === undefined2 || eq(objValue, objectProto[key]) && !hasOwnProperty.call(object, key)) {
            return srcValue;
          }
          return objValue;
        }
        function customDefaultsMerge(objValue, srcValue, key, object, source, stack) {
          if (isObject(objValue) && isObject(srcValue)) {
            stack.set(srcValue, objValue);
            baseMerge(objValue, srcValue, undefined2, customDefaultsMerge, stack);
            stack["delete"](srcValue);
          }
          return objValue;
        }
        function customOmitClone(value) {
          return isPlainObject(value) ? undefined2 : value;
        }
        function equalArrays(array, other, bitmask, customizer, equalFunc, stack) {
          var isPartial = bitmask & COMPARE_PARTIAL_FLAG, arrLength = array.length, othLength = other.length;
          if (arrLength != othLength && !(isPartial && othLength > arrLength)) {
            return false;
          }
          var arrStacked = stack.get(array);
          var othStacked = stack.get(other);
          if (arrStacked && othStacked) {
            return arrStacked == other && othStacked == array;
          }
          var index = -1, result2 = true, seen = bitmask & COMPARE_UNORDERED_FLAG ? new SetCache() : undefined2;
          stack.set(array, other);
          stack.set(other, array);
          while (++index < arrLength) {
            var arrValue = array[index], othValue = other[index];
            if (customizer) {
              var compared = isPartial ? customizer(othValue, arrValue, index, other, array, stack) : customizer(arrValue, othValue, index, array, other, stack);
            }
            if (compared !== undefined2) {
              if (compared) {
                continue;
              }
              result2 = false;
              break;
            }
            if (seen) {
              if (!arraySome(other, function(othValue2, othIndex) {
                if (!cacheHas(seen, othIndex) && (arrValue === othValue2 || equalFunc(arrValue, othValue2, bitmask, customizer, stack))) {
                  return seen.push(othIndex);
                }
              })) {
                result2 = false;
                break;
              }
            } else if (!(arrValue === othValue || equalFunc(arrValue, othValue, bitmask, customizer, stack))) {
              result2 = false;
              break;
            }
          }
          stack["delete"](array);
          stack["delete"](other);
          return result2;
        }
        function equalByTag(object, other, tag, bitmask, customizer, equalFunc, stack) {
          switch (tag) {
            case dataViewTag:
              if (object.byteLength != other.byteLength || object.byteOffset != other.byteOffset) {
                return false;
              }
              object = object.buffer;
              other = other.buffer;
            case arrayBufferTag:
              if (object.byteLength != other.byteLength || !equalFunc(new Uint8Array2(object), new Uint8Array2(other))) {
                return false;
              }
              return true;
            case boolTag:
            case dateTag:
            case numberTag:
              return eq(+object, +other);
            case errorTag:
              return object.name == other.name && object.message == other.message;
            case regexpTag:
            case stringTag:
              return object == other + "";
            case mapTag:
              var convert = mapToArray;
            case setTag:
              var isPartial = bitmask & COMPARE_PARTIAL_FLAG;
              convert || (convert = setToArray);
              if (object.size != other.size && !isPartial) {
                return false;
              }
              var stacked = stack.get(object);
              if (stacked) {
                return stacked == other;
              }
              bitmask |= COMPARE_UNORDERED_FLAG;
              stack.set(object, other);
              var result2 = equalArrays(convert(object), convert(other), bitmask, customizer, equalFunc, stack);
              stack["delete"](object);
              return result2;
            case symbolTag:
              if (symbolValueOf) {
                return symbolValueOf.call(object) == symbolValueOf.call(other);
              }
          }
          return false;
        }
        function equalObjects(object, other, bitmask, customizer, equalFunc, stack) {
          var isPartial = bitmask & COMPARE_PARTIAL_FLAG, objProps = getAllKeys(object), objLength = objProps.length, othProps = getAllKeys(other), othLength = othProps.length;
          if (objLength != othLength && !isPartial) {
            return false;
          }
          var index = objLength;
          while (index--) {
            var key = objProps[index];
            if (!(isPartial ? key in other : hasOwnProperty.call(other, key))) {
              return false;
            }
          }
          var objStacked = stack.get(object);
          var othStacked = stack.get(other);
          if (objStacked && othStacked) {
            return objStacked == other && othStacked == object;
          }
          var result2 = true;
          stack.set(object, other);
          stack.set(other, object);
          var skipCtor = isPartial;
          while (++index < objLength) {
            key = objProps[index];
            var objValue = object[key], othValue = other[key];
            if (customizer) {
              var compared = isPartial ? customizer(othValue, objValue, key, other, object, stack) : customizer(objValue, othValue, key, object, other, stack);
            }
            if (!(compared === undefined2 ? objValue === othValue || equalFunc(objValue, othValue, bitmask, customizer, stack) : compared)) {
              result2 = false;
              break;
            }
            skipCtor || (skipCtor = key == "constructor");
          }
          if (result2 && !skipCtor) {
            var objCtor = object.constructor, othCtor = other.constructor;
            if (objCtor != othCtor && ("constructor" in object && "constructor" in other) && !(typeof objCtor == "function" && objCtor instanceof objCtor && typeof othCtor == "function" && othCtor instanceof othCtor)) {
              result2 = false;
            }
          }
          stack["delete"](object);
          stack["delete"](other);
          return result2;
        }
        function flatRest(func) {
          return setToString(overRest(func, undefined2, flatten), func + "");
        }
        function getAllKeys(object) {
          return baseGetAllKeys(object, keys, getSymbols);
        }
        function getAllKeysIn(object) {
          return baseGetAllKeys(object, keysIn, getSymbolsIn);
        }
        var getData = !metaMap ? noop : function(func) {
          return metaMap.get(func);
        };
        function getFuncName(func) {
          var result2 = func.name + "", array = realNames[result2], length = hasOwnProperty.call(realNames, result2) ? array.length : 0;
          while (length--) {
            var data = array[length], otherFunc = data.func;
            if (otherFunc == null || otherFunc == func) {
              return data.name;
            }
          }
          return result2;
        }
        function getHolder(func) {
          var object = hasOwnProperty.call(lodash, "placeholder") ? lodash : func;
          return object.placeholder;
        }
        function getIteratee() {
          var result2 = lodash.iteratee || iteratee;
          result2 = result2 === iteratee ? baseIteratee : result2;
          return arguments.length ? result2(arguments[0], arguments[1]) : result2;
        }
        function getMapData(map2, key) {
          var data = map2.__data__;
          return isKeyable(key) ? data[typeof key == "string" ? "string" : "hash"] : data.map;
        }
        function getMatchData(object) {
          var result2 = keys(object), length = result2.length;
          while (length--) {
            var key = result2[length], value = object[key];
            result2[length] = [key, value, isStrictComparable(value)];
          }
          return result2;
        }
        function getNative(object, key) {
          var value = getValue(object, key);
          return baseIsNative(value) ? value : undefined2;
        }
        function getRawTag(value) {
          var isOwn = hasOwnProperty.call(value, symToStringTag), tag = value[symToStringTag];
          try {
            value[symToStringTag] = undefined2;
            var unmasked = true;
          } catch (e) {
          }
          var result2 = nativeObjectToString.call(value);
          if (unmasked) {
            if (isOwn) {
              value[symToStringTag] = tag;
            } else {
              delete value[symToStringTag];
            }
          }
          return result2;
        }
        var getSymbols = !nativeGetSymbols ? stubArray : function(object) {
          if (object == null) {
            return [];
          }
          object = Object2(object);
          return arrayFilter(nativeGetSymbols(object), function(symbol) {
            return propertyIsEnumerable.call(object, symbol);
          });
        };
        var getSymbolsIn = !nativeGetSymbols ? stubArray : function(object) {
          var result2 = [];
          while (object) {
            arrayPush(result2, getSymbols(object));
            object = getPrototype(object);
          }
          return result2;
        };
        var getTag = baseGetTag;
        if (DataView && getTag(new DataView(new ArrayBuffer(1))) != dataViewTag || Map2 && getTag(new Map2()) != mapTag || Promise2 && getTag(Promise2.resolve()) != promiseTag || Set2 && getTag(new Set2()) != setTag || WeakMap2 && getTag(new WeakMap2()) != weakMapTag) {
          getTag = function(value) {
            var result2 = baseGetTag(value), Ctor = result2 == objectTag ? value.constructor : undefined2, ctorString = Ctor ? toSource(Ctor) : "";
            if (ctorString) {
              switch (ctorString) {
                case dataViewCtorString:
                  return dataViewTag;
                case mapCtorString:
                  return mapTag;
                case promiseCtorString:
                  return promiseTag;
                case setCtorString:
                  return setTag;
                case weakMapCtorString:
                  return weakMapTag;
              }
            }
            return result2;
          };
        }
        function getView(start, end, transforms) {
          var index = -1, length = transforms.length;
          while (++index < length) {
            var data = transforms[index], size2 = data.size;
            switch (data.type) {
              case "drop":
                start += size2;
                break;
              case "dropRight":
                end -= size2;
                break;
              case "take":
                end = nativeMin(end, start + size2);
                break;
              case "takeRight":
                start = nativeMax(start, end - size2);
                break;
            }
          }
          return { "start": start, "end": end };
        }
        function getWrapDetails(source) {
          var match = source.match(reWrapDetails);
          return match ? match[1].split(reSplitDetails) : [];
        }
        function hasPath(object, path6, hasFunc) {
          path6 = castPath(path6, object);
          var index = -1, length = path6.length, result2 = false;
          while (++index < length) {
            var key = toKey(path6[index]);
            if (!(result2 = object != null && hasFunc(object, key))) {
              break;
            }
            object = object[key];
          }
          if (result2 || ++index != length) {
            return result2;
          }
          length = object == null ? 0 : object.length;
          return !!length && isLength(length) && isIndex(key, length) && (isArray(object) || isArguments(object));
        }
        function initCloneArray(array) {
          var length = array.length, result2 = new array.constructor(length);
          if (length && typeof array[0] == "string" && hasOwnProperty.call(array, "index")) {
            result2.index = array.index;
            result2.input = array.input;
          }
          return result2;
        }
        function initCloneObject(object) {
          return typeof object.constructor == "function" && !isPrototype(object) ? baseCreate(getPrototype(object)) : {};
        }
        function initCloneByTag(object, tag, isDeep) {
          var Ctor = object.constructor;
          switch (tag) {
            case arrayBufferTag:
              return cloneArrayBuffer(object);
            case boolTag:
            case dateTag:
              return new Ctor(+object);
            case dataViewTag:
              return cloneDataView(object, isDeep);
            case float32Tag:
            case float64Tag:
            case int8Tag:
            case int16Tag:
            case int32Tag:
            case uint8Tag:
            case uint8ClampedTag:
            case uint16Tag:
            case uint32Tag:
              return cloneTypedArray(object, isDeep);
            case mapTag:
              return new Ctor();
            case numberTag:
            case stringTag:
              return new Ctor(object);
            case regexpTag:
              return cloneRegExp(object);
            case setTag:
              return new Ctor();
            case symbolTag:
              return cloneSymbol(object);
          }
        }
        function insertWrapDetails(source, details) {
          var length = details.length;
          if (!length) {
            return source;
          }
          var lastIndex = length - 1;
          details[lastIndex] = (length > 1 ? "& " : "") + details[lastIndex];
          details = details.join(length > 2 ? ", " : " ");
          return source.replace(reWrapComment, "{\n/* [wrapped with " + details + "] */\n");
        }
        function isFlattenable(value) {
          return isArray(value) || isArguments(value) || !!(spreadableSymbol && value && value[spreadableSymbol]);
        }
        function isIndex(value, length) {
          var type = typeof value;
          length = length == null ? MAX_SAFE_INTEGER : length;
          return !!length && (type == "number" || type != "symbol" && reIsUint.test(value)) && (value > -1 && value % 1 == 0 && value < length);
        }
        function isIterateeCall(value, index, object) {
          if (!isObject(object)) {
            return false;
          }
          var type = typeof index;
          if (type == "number" ? isArrayLike(object) && isIndex(index, object.length) : type == "string" && index in object) {
            return eq(object[index], value);
          }
          return false;
        }
        function isKey(value, object) {
          if (isArray(value)) {
            return false;
          }
          var type = typeof value;
          if (type == "number" || type == "symbol" || type == "boolean" || value == null || isSymbol(value)) {
            return true;
          }
          return reIsPlainProp.test(value) || !reIsDeepProp.test(value) || object != null && value in Object2(object);
        }
        function isKeyable(value) {
          var type = typeof value;
          return type == "string" || type == "number" || type == "symbol" || type == "boolean" ? value !== "__proto__" : value === null;
        }
        function isLaziable(func) {
          var funcName = getFuncName(func), other = lodash[funcName];
          if (typeof other != "function" || !(funcName in LazyWrapper.prototype)) {
            return false;
          }
          if (func === other) {
            return true;
          }
          var data = getData(other);
          return !!data && func === data[0];
        }
        function isMasked(func) {
          return !!maskSrcKey && maskSrcKey in func;
        }
        var isMaskable = coreJsData ? isFunction : stubFalse;
        function isPrototype(value) {
          var Ctor = value && value.constructor, proto = typeof Ctor == "function" && Ctor.prototype || objectProto;
          return value === proto;
        }
        function isStrictComparable(value) {
          return value === value && !isObject(value);
        }
        function matchesStrictComparable(key, srcValue) {
          return function(object) {
            if (object == null) {
              return false;
            }
            return object[key] === srcValue && (srcValue !== undefined2 || key in Object2(object));
          };
        }
        function memoizeCapped(func) {
          var result2 = memoize(func, function(key) {
            if (cache.size === MAX_MEMOIZE_SIZE) {
              cache.clear();
            }
            return key;
          });
          var cache = result2.cache;
          return result2;
        }
        function mergeData(data, source) {
          var bitmask = data[1], srcBitmask = source[1], newBitmask = bitmask | srcBitmask, isCommon = newBitmask < (WRAP_BIND_FLAG | WRAP_BIND_KEY_FLAG | WRAP_ARY_FLAG);
          var isCombo = srcBitmask == WRAP_ARY_FLAG && bitmask == WRAP_CURRY_FLAG || srcBitmask == WRAP_ARY_FLAG && bitmask == WRAP_REARG_FLAG && data[7].length <= source[8] || srcBitmask == (WRAP_ARY_FLAG | WRAP_REARG_FLAG) && source[7].length <= source[8] && bitmask == WRAP_CURRY_FLAG;
          if (!(isCommon || isCombo)) {
            return data;
          }
          if (srcBitmask & WRAP_BIND_FLAG) {
            data[2] = source[2];
            newBitmask |= bitmask & WRAP_BIND_FLAG ? 0 : WRAP_CURRY_BOUND_FLAG;
          }
          var value = source[3];
          if (value) {
            var partials = data[3];
            data[3] = partials ? composeArgs(partials, value, source[4]) : value;
            data[4] = partials ? replaceHolders(data[3], PLACEHOLDER) : source[4];
          }
          value = source[5];
          if (value) {
            partials = data[5];
            data[5] = partials ? composeArgsRight(partials, value, source[6]) : value;
            data[6] = partials ? replaceHolders(data[5], PLACEHOLDER) : source[6];
          }
          value = source[7];
          if (value) {
            data[7] = value;
          }
          if (srcBitmask & WRAP_ARY_FLAG) {
            data[8] = data[8] == null ? source[8] : nativeMin(data[8], source[8]);
          }
          if (data[9] == null) {
            data[9] = source[9];
          }
          data[0] = source[0];
          data[1] = newBitmask;
          return data;
        }
        function nativeKeysIn(object) {
          var result2 = [];
          if (object != null) {
            for (var key in Object2(object)) {
              result2.push(key);
            }
          }
          return result2;
        }
        function objectToString(value) {
          return nativeObjectToString.call(value);
        }
        function overRest(func, start, transform2) {
          start = nativeMax(start === undefined2 ? func.length - 1 : start, 0);
          return function() {
            var args = arguments, index = -1, length = nativeMax(args.length - start, 0), array = Array2(length);
            while (++index < length) {
              array[index] = args[start + index];
            }
            index = -1;
            var otherArgs = Array2(start + 1);
            while (++index < start) {
              otherArgs[index] = args[index];
            }
            otherArgs[start] = transform2(array);
            return apply(func, this, otherArgs);
          };
        }
        function parent(object, path6) {
          return path6.length < 2 ? object : baseGet(object, baseSlice(path6, 0, -1));
        }
        function reorder(array, indexes) {
          var arrLength = array.length, length = nativeMin(indexes.length, arrLength), oldArray = copyArray(array);
          while (length--) {
            var index = indexes[length];
            array[length] = isIndex(index, arrLength) ? oldArray[index] : undefined2;
          }
          return array;
        }
        function safeGet(object, key) {
          if (key === "constructor" && typeof object[key] === "function") {
            return;
          }
          if (key == "__proto__") {
            return;
          }
          return object[key];
        }
        var setData = shortOut(baseSetData);
        var setTimeout2 = ctxSetTimeout || function(func, wait) {
          return root.setTimeout(func, wait);
        };
        var setToString = shortOut(baseSetToString);
        function setWrapToString(wrapper, reference, bitmask) {
          var source = reference + "";
          return setToString(wrapper, insertWrapDetails(source, updateWrapDetails(getWrapDetails(source), bitmask)));
        }
        function shortOut(func) {
          var count = 0, lastCalled = 0;
          return function() {
            var stamp = nativeNow(), remaining = HOT_SPAN - (stamp - lastCalled);
            lastCalled = stamp;
            if (remaining > 0) {
              if (++count >= HOT_COUNT) {
                return arguments[0];
              }
            } else {
              count = 0;
            }
            return func.apply(undefined2, arguments);
          };
        }
        function shuffleSelf(array, size2) {
          var index = -1, length = array.length, lastIndex = length - 1;
          size2 = size2 === undefined2 ? length : size2;
          while (++index < size2) {
            var rand = baseRandom(index, lastIndex), value = array[rand];
            array[rand] = array[index];
            array[index] = value;
          }
          array.length = size2;
          return array;
        }
        var stringToPath = memoizeCapped(function(string) {
          var result2 = [];
          if (string.charCodeAt(0) === 46) {
            result2.push("");
          }
          string.replace(rePropName, function(match, number, quote, subString) {
            result2.push(quote ? subString.replace(reEscapeChar, "$1") : number || match);
          });
          return result2;
        });
        function toKey(value) {
          if (typeof value == "string" || isSymbol(value)) {
            return value;
          }
          var result2 = value + "";
          return result2 == "0" && 1 / value == -INFINITY ? "-0" : result2;
        }
        function toSource(func) {
          if (func != null) {
            try {
              return funcToString.call(func);
            } catch (e) {
            }
            try {
              return func + "";
            } catch (e) {
            }
          }
          return "";
        }
        function updateWrapDetails(details, bitmask) {
          arrayEach(wrapFlags, function(pair) {
            var value = "_." + pair[0];
            if (bitmask & pair[1] && !arrayIncludes(details, value)) {
              details.push(value);
            }
          });
          return details.sort();
        }
        function wrapperClone(wrapper) {
          if (wrapper instanceof LazyWrapper) {
            return wrapper.clone();
          }
          var result2 = new LodashWrapper(wrapper.__wrapped__, wrapper.__chain__);
          result2.__actions__ = copyArray(wrapper.__actions__);
          result2.__index__ = wrapper.__index__;
          result2.__values__ = wrapper.__values__;
          return result2;
        }
        function chunk(array, size2, guard) {
          if (guard ? isIterateeCall(array, size2, guard) : size2 === undefined2) {
            size2 = 1;
          } else {
            size2 = nativeMax(toInteger(size2), 0);
          }
          var length = array == null ? 0 : array.length;
          if (!length || size2 < 1) {
            return [];
          }
          var index = 0, resIndex = 0, result2 = Array2(nativeCeil(length / size2));
          while (index < length) {
            result2[resIndex++] = baseSlice(array, index, index += size2);
          }
          return result2;
        }
        function compact(array) {
          var index = -1, length = array == null ? 0 : array.length, resIndex = 0, result2 = [];
          while (++index < length) {
            var value = array[index];
            if (value) {
              result2[resIndex++] = value;
            }
          }
          return result2;
        }
        function concat() {
          var length = arguments.length;
          if (!length) {
            return [];
          }
          var args = Array2(length - 1), array = arguments[0], index = length;
          while (index--) {
            args[index - 1] = arguments[index];
          }
          return arrayPush(isArray(array) ? copyArray(array) : [array], baseFlatten(args, 1));
        }
        var difference = baseRest(function(array, values2) {
          return isArrayLikeObject(array) ? baseDifference(array, baseFlatten(values2, 1, isArrayLikeObject, true)) : [];
        });
        var differenceBy = baseRest(function(array, values2) {
          var iteratee2 = last(values2);
          if (isArrayLikeObject(iteratee2)) {
            iteratee2 = undefined2;
          }
          return isArrayLikeObject(array) ? baseDifference(array, baseFlatten(values2, 1, isArrayLikeObject, true), getIteratee(iteratee2, 2)) : [];
        });
        var differenceWith = baseRest(function(array, values2) {
          var comparator = last(values2);
          if (isArrayLikeObject(comparator)) {
            comparator = undefined2;
          }
          return isArrayLikeObject(array) ? baseDifference(array, baseFlatten(values2, 1, isArrayLikeObject, true), undefined2, comparator) : [];
        });
        function drop(array, n, guard) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return [];
          }
          n = guard || n === undefined2 ? 1 : toInteger(n);
          return baseSlice(array, n < 0 ? 0 : n, length);
        }
        function dropRight(array, n, guard) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return [];
          }
          n = guard || n === undefined2 ? 1 : toInteger(n);
          n = length - n;
          return baseSlice(array, 0, n < 0 ? 0 : n);
        }
        function dropRightWhile(array, predicate) {
          return array && array.length ? baseWhile(array, getIteratee(predicate, 3), true, true) : [];
        }
        function dropWhile(array, predicate) {
          return array && array.length ? baseWhile(array, getIteratee(predicate, 3), true) : [];
        }
        function fill(array, value, start, end) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return [];
          }
          if (start && typeof start != "number" && isIterateeCall(array, value, start)) {
            start = 0;
            end = length;
          }
          return baseFill(array, value, start, end);
        }
        function findIndex(array, predicate, fromIndex) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return -1;
          }
          var index = fromIndex == null ? 0 : toInteger(fromIndex);
          if (index < 0) {
            index = nativeMax(length + index, 0);
          }
          return baseFindIndex(array, getIteratee(predicate, 3), index);
        }
        function findLastIndex(array, predicate, fromIndex) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return -1;
          }
          var index = length - 1;
          if (fromIndex !== undefined2) {
            index = toInteger(fromIndex);
            index = fromIndex < 0 ? nativeMax(length + index, 0) : nativeMin(index, length - 1);
          }
          return baseFindIndex(array, getIteratee(predicate, 3), index, true);
        }
        function flatten(array) {
          var length = array == null ? 0 : array.length;
          return length ? baseFlatten(array, 1) : [];
        }
        function flattenDeep(array) {
          var length = array == null ? 0 : array.length;
          return length ? baseFlatten(array, INFINITY) : [];
        }
        function flattenDepth(array, depth) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return [];
          }
          depth = depth === undefined2 ? 1 : toInteger(depth);
          return baseFlatten(array, depth);
        }
        function fromPairs(pairs) {
          var index = -1, length = pairs == null ? 0 : pairs.length, result2 = {};
          while (++index < length) {
            var pair = pairs[index];
            result2[pair[0]] = pair[1];
          }
          return result2;
        }
        function head(array) {
          return array && array.length ? array[0] : undefined2;
        }
        function indexOf(array, value, fromIndex) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return -1;
          }
          var index = fromIndex == null ? 0 : toInteger(fromIndex);
          if (index < 0) {
            index = nativeMax(length + index, 0);
          }
          return baseIndexOf(array, value, index);
        }
        function initial(array) {
          var length = array == null ? 0 : array.length;
          return length ? baseSlice(array, 0, -1) : [];
        }
        var intersection = baseRest(function(arrays) {
          var mapped = arrayMap(arrays, castArrayLikeObject);
          return mapped.length && mapped[0] === arrays[0] ? baseIntersection(mapped) : [];
        });
        var intersectionBy = baseRest(function(arrays) {
          var iteratee2 = last(arrays), mapped = arrayMap(arrays, castArrayLikeObject);
          if (iteratee2 === last(mapped)) {
            iteratee2 = undefined2;
          } else {
            mapped.pop();
          }
          return mapped.length && mapped[0] === arrays[0] ? baseIntersection(mapped, getIteratee(iteratee2, 2)) : [];
        });
        var intersectionWith = baseRest(function(arrays) {
          var comparator = last(arrays), mapped = arrayMap(arrays, castArrayLikeObject);
          comparator = typeof comparator == "function" ? comparator : undefined2;
          if (comparator) {
            mapped.pop();
          }
          return mapped.length && mapped[0] === arrays[0] ? baseIntersection(mapped, undefined2, comparator) : [];
        });
        function join5(array, separator) {
          return array == null ? "" : nativeJoin.call(array, separator);
        }
        function last(array) {
          var length = array == null ? 0 : array.length;
          return length ? array[length - 1] : undefined2;
        }
        function lastIndexOf(array, value, fromIndex) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return -1;
          }
          var index = length;
          if (fromIndex !== undefined2) {
            index = toInteger(fromIndex);
            index = index < 0 ? nativeMax(length + index, 0) : nativeMin(index, length - 1);
          }
          return value === value ? strictLastIndexOf(array, value, index) : baseFindIndex(array, baseIsNaN, index, true);
        }
        function nth(array, n) {
          return array && array.length ? baseNth(array, toInteger(n)) : undefined2;
        }
        var pull = baseRest(pullAll);
        function pullAll(array, values2) {
          return array && array.length && values2 && values2.length ? basePullAll(array, values2) : array;
        }
        function pullAllBy(array, values2, iteratee2) {
          return array && array.length && values2 && values2.length ? basePullAll(array, values2, getIteratee(iteratee2, 2)) : array;
        }
        function pullAllWith(array, values2, comparator) {
          return array && array.length && values2 && values2.length ? basePullAll(array, values2, undefined2, comparator) : array;
        }
        var pullAt = flatRest(function(array, indexes) {
          var length = array == null ? 0 : array.length, result2 = baseAt(array, indexes);
          basePullAt(array, arrayMap(indexes, function(index) {
            return isIndex(index, length) ? +index : index;
          }).sort(compareAscending));
          return result2;
        });
        function remove(array, predicate) {
          var result2 = [];
          if (!(array && array.length)) {
            return result2;
          }
          var index = -1, indexes = [], length = array.length;
          predicate = getIteratee(predicate, 3);
          while (++index < length) {
            var value = array[index];
            if (predicate(value, index, array)) {
              result2.push(value);
              indexes.push(index);
            }
          }
          basePullAt(array, indexes);
          return result2;
        }
        function reverse(array) {
          return array == null ? array : nativeReverse.call(array);
        }
        function slice(array, start, end) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return [];
          }
          if (end && typeof end != "number" && isIterateeCall(array, start, end)) {
            start = 0;
            end = length;
          } else {
            start = start == null ? 0 : toInteger(start);
            end = end === undefined2 ? length : toInteger(end);
          }
          return baseSlice(array, start, end);
        }
        function sortedIndex(array, value) {
          return baseSortedIndex(array, value);
        }
        function sortedIndexBy(array, value, iteratee2) {
          return baseSortedIndexBy(array, value, getIteratee(iteratee2, 2));
        }
        function sortedIndexOf(array, value) {
          var length = array == null ? 0 : array.length;
          if (length) {
            var index = baseSortedIndex(array, value);
            if (index < length && eq(array[index], value)) {
              return index;
            }
          }
          return -1;
        }
        function sortedLastIndex(array, value) {
          return baseSortedIndex(array, value, true);
        }
        function sortedLastIndexBy(array, value, iteratee2) {
          return baseSortedIndexBy(array, value, getIteratee(iteratee2, 2), true);
        }
        function sortedLastIndexOf(array, value) {
          var length = array == null ? 0 : array.length;
          if (length) {
            var index = baseSortedIndex(array, value, true) - 1;
            if (eq(array[index], value)) {
              return index;
            }
          }
          return -1;
        }
        function sortedUniq(array) {
          return array && array.length ? baseSortedUniq(array) : [];
        }
        function sortedUniqBy(array, iteratee2) {
          return array && array.length ? baseSortedUniq(array, getIteratee(iteratee2, 2)) : [];
        }
        function tail(array) {
          var length = array == null ? 0 : array.length;
          return length ? baseSlice(array, 1, length) : [];
        }
        function take(array, n, guard) {
          if (!(array && array.length)) {
            return [];
          }
          n = guard || n === undefined2 ? 1 : toInteger(n);
          return baseSlice(array, 0, n < 0 ? 0 : n);
        }
        function takeRight(array, n, guard) {
          var length = array == null ? 0 : array.length;
          if (!length) {
            return [];
          }
          n = guard || n === undefined2 ? 1 : toInteger(n);
          n = length - n;
          return baseSlice(array, n < 0 ? 0 : n, length);
        }
        function takeRightWhile(array, predicate) {
          return array && array.length ? baseWhile(array, getIteratee(predicate, 3), false, true) : [];
        }
        function takeWhile(array, predicate) {
          return array && array.length ? baseWhile(array, getIteratee(predicate, 3)) : [];
        }
        var union = baseRest(function(arrays) {
          return baseUniq(baseFlatten(arrays, 1, isArrayLikeObject, true));
        });
        var unionBy = baseRest(function(arrays) {
          var iteratee2 = last(arrays);
          if (isArrayLikeObject(iteratee2)) {
            iteratee2 = undefined2;
          }
          return baseUniq(baseFlatten(arrays, 1, isArrayLikeObject, true), getIteratee(iteratee2, 2));
        });
        var unionWith = baseRest(function(arrays) {
          var comparator = last(arrays);
          comparator = typeof comparator == "function" ? comparator : undefined2;
          return baseUniq(baseFlatten(arrays, 1, isArrayLikeObject, true), undefined2, comparator);
        });
        function uniq(array) {
          return array && array.length ? baseUniq(array) : [];
        }
        function uniqBy(array, iteratee2) {
          return array && array.length ? baseUniq(array, getIteratee(iteratee2, 2)) : [];
        }
        function uniqWith(array, comparator) {
          comparator = typeof comparator == "function" ? comparator : undefined2;
          return array && array.length ? baseUniq(array, undefined2, comparator) : [];
        }
        function unzip(array) {
          if (!(array && array.length)) {
            return [];
          }
          var length = 0;
          array = arrayFilter(array, function(group) {
            if (isArrayLikeObject(group)) {
              length = nativeMax(group.length, length);
              return true;
            }
          });
          return baseTimes(length, function(index) {
            return arrayMap(array, baseProperty(index));
          });
        }
        function unzipWith(array, iteratee2) {
          if (!(array && array.length)) {
            return [];
          }
          var result2 = unzip(array);
          if (iteratee2 == null) {
            return result2;
          }
          return arrayMap(result2, function(group) {
            return apply(iteratee2, undefined2, group);
          });
        }
        var without = baseRest(function(array, values2) {
          return isArrayLikeObject(array) ? baseDifference(array, values2) : [];
        });
        var xor = baseRest(function(arrays) {
          return baseXor(arrayFilter(arrays, isArrayLikeObject));
        });
        var xorBy = baseRest(function(arrays) {
          var iteratee2 = last(arrays);
          if (isArrayLikeObject(iteratee2)) {
            iteratee2 = undefined2;
          }
          return baseXor(arrayFilter(arrays, isArrayLikeObject), getIteratee(iteratee2, 2));
        });
        var xorWith = baseRest(function(arrays) {
          var comparator = last(arrays);
          comparator = typeof comparator == "function" ? comparator : undefined2;
          return baseXor(arrayFilter(arrays, isArrayLikeObject), undefined2, comparator);
        });
        var zip = baseRest(unzip);
        function zipObject(props, values2) {
          return baseZipObject(props || [], values2 || [], assignValue);
        }
        function zipObjectDeep(props, values2) {
          return baseZipObject(props || [], values2 || [], baseSet);
        }
        var zipWith = baseRest(function(arrays) {
          var length = arrays.length, iteratee2 = length > 1 ? arrays[length - 1] : undefined2;
          iteratee2 = typeof iteratee2 == "function" ? (arrays.pop(), iteratee2) : undefined2;
          return unzipWith(arrays, iteratee2);
        });
        function chain(value) {
          var result2 = lodash(value);
          result2.__chain__ = true;
          return result2;
        }
        function tap(value, interceptor) {
          interceptor(value);
          return value;
        }
        function thru(value, interceptor) {
          return interceptor(value);
        }
        var wrapperAt = flatRest(function(paths) {
          var length = paths.length, start = length ? paths[0] : 0, value = this.__wrapped__, interceptor = function(object) {
            return baseAt(object, paths);
          };
          if (length > 1 || this.__actions__.length || !(value instanceof LazyWrapper) || !isIndex(start)) {
            return this.thru(interceptor);
          }
          value = value.slice(start, +start + (length ? 1 : 0));
          value.__actions__.push({
            "func": thru,
            "args": [interceptor],
            "thisArg": undefined2
          });
          return new LodashWrapper(value, this.__chain__).thru(function(array) {
            if (length && !array.length) {
              array.push(undefined2);
            }
            return array;
          });
        });
        function wrapperChain() {
          return chain(this);
        }
        function wrapperCommit() {
          return new LodashWrapper(this.value(), this.__chain__);
        }
        function wrapperNext() {
          if (this.__values__ === undefined2) {
            this.__values__ = toArray(this.value());
          }
          var done = this.__index__ >= this.__values__.length, value = done ? undefined2 : this.__values__[this.__index__++];
          return { "done": done, "value": value };
        }
        function wrapperToIterator() {
          return this;
        }
        function wrapperPlant(value) {
          var result2, parent2 = this;
          while (parent2 instanceof baseLodash) {
            var clone2 = wrapperClone(parent2);
            clone2.__index__ = 0;
            clone2.__values__ = undefined2;
            if (result2) {
              previous.__wrapped__ = clone2;
            } else {
              result2 = clone2;
            }
            var previous = clone2;
            parent2 = parent2.__wrapped__;
          }
          previous.__wrapped__ = value;
          return result2;
        }
        function wrapperReverse() {
          var value = this.__wrapped__;
          if (value instanceof LazyWrapper) {
            var wrapped = value;
            if (this.__actions__.length) {
              wrapped = new LazyWrapper(this);
            }
            wrapped = wrapped.reverse();
            wrapped.__actions__.push({
              "func": thru,
              "args": [reverse],
              "thisArg": undefined2
            });
            return new LodashWrapper(wrapped, this.__chain__);
          }
          return this.thru(reverse);
        }
        function wrapperValue() {
          return baseWrapperValue(this.__wrapped__, this.__actions__);
        }
        var countBy = createAggregator(function(result2, value, key) {
          if (hasOwnProperty.call(result2, key)) {
            ++result2[key];
          } else {
            baseAssignValue(result2, key, 1);
          }
        });
        function every(collection, predicate, guard) {
          var func = isArray(collection) ? arrayEvery : baseEvery;
          if (guard && isIterateeCall(collection, predicate, guard)) {
            predicate = undefined2;
          }
          return func(collection, getIteratee(predicate, 3));
        }
        function filter(collection, predicate) {
          var func = isArray(collection) ? arrayFilter : baseFilter;
          return func(collection, getIteratee(predicate, 3));
        }
        var find = createFind(findIndex);
        var findLast = createFind(findLastIndex);
        function flatMap(collection, iteratee2) {
          return baseFlatten(map(collection, iteratee2), 1);
        }
        function flatMapDeep(collection, iteratee2) {
          return baseFlatten(map(collection, iteratee2), INFINITY);
        }
        function flatMapDepth(collection, iteratee2, depth) {
          depth = depth === undefined2 ? 1 : toInteger(depth);
          return baseFlatten(map(collection, iteratee2), depth);
        }
        function forEach(collection, iteratee2) {
          var func = isArray(collection) ? arrayEach : baseEach;
          return func(collection, getIteratee(iteratee2, 3));
        }
        function forEachRight(collection, iteratee2) {
          var func = isArray(collection) ? arrayEachRight : baseEachRight;
          return func(collection, getIteratee(iteratee2, 3));
        }
        var groupBy = createAggregator(function(result2, value, key) {
          if (hasOwnProperty.call(result2, key)) {
            result2[key].push(value);
          } else {
            baseAssignValue(result2, key, [value]);
          }
        });
        function includes(collection, value, fromIndex, guard) {
          collection = isArrayLike(collection) ? collection : values(collection);
          fromIndex = fromIndex && !guard ? toInteger(fromIndex) : 0;
          var length = collection.length;
          if (fromIndex < 0) {
            fromIndex = nativeMax(length + fromIndex, 0);
          }
          return isString(collection) ? fromIndex <= length && collection.indexOf(value, fromIndex) > -1 : !!length && baseIndexOf(collection, value, fromIndex) > -1;
        }
        var invokeMap = baseRest(function(collection, path6, args) {
          var index = -1, isFunc = typeof path6 == "function", result2 = isArrayLike(collection) ? Array2(collection.length) : [];
          baseEach(collection, function(value) {
            result2[++index] = isFunc ? apply(path6, value, args) : baseInvoke(value, path6, args);
          });
          return result2;
        });
        var keyBy = createAggregator(function(result2, value, key) {
          baseAssignValue(result2, key, value);
        });
        function map(collection, iteratee2) {
          var func = isArray(collection) ? arrayMap : baseMap;
          return func(collection, getIteratee(iteratee2, 3));
        }
        function orderBy(collection, iteratees, orders, guard) {
          if (collection == null) {
            return [];
          }
          if (!isArray(iteratees)) {
            iteratees = iteratees == null ? [] : [iteratees];
          }
          orders = guard ? undefined2 : orders;
          if (!isArray(orders)) {
            orders = orders == null ? [] : [orders];
          }
          return baseOrderBy(collection, iteratees, orders);
        }
        var partition = createAggregator(function(result2, value, key) {
          result2[key ? 0 : 1].push(value);
        }, function() {
          return [[], []];
        });
        function reduce(collection, iteratee2, accumulator) {
          var func = isArray(collection) ? arrayReduce : baseReduce, initAccum = arguments.length < 3;
          return func(collection, getIteratee(iteratee2, 4), accumulator, initAccum, baseEach);
        }
        function reduceRight(collection, iteratee2, accumulator) {
          var func = isArray(collection) ? arrayReduceRight : baseReduce, initAccum = arguments.length < 3;
          return func(collection, getIteratee(iteratee2, 4), accumulator, initAccum, baseEachRight);
        }
        function reject(collection, predicate) {
          var func = isArray(collection) ? arrayFilter : baseFilter;
          return func(collection, negate(getIteratee(predicate, 3)));
        }
        function sample(collection) {
          var func = isArray(collection) ? arraySample : baseSample;
          return func(collection);
        }
        function sampleSize(collection, n, guard) {
          if (guard ? isIterateeCall(collection, n, guard) : n === undefined2) {
            n = 1;
          } else {
            n = toInteger(n);
          }
          var func = isArray(collection) ? arraySampleSize : baseSampleSize;
          return func(collection, n);
        }
        function shuffle(collection) {
          var func = isArray(collection) ? arrayShuffle : baseShuffle;
          return func(collection);
        }
        function size(collection) {
          if (collection == null) {
            return 0;
          }
          if (isArrayLike(collection)) {
            return isString(collection) ? stringSize(collection) : collection.length;
          }
          var tag = getTag(collection);
          if (tag == mapTag || tag == setTag) {
            return collection.size;
          }
          return baseKeys(collection).length;
        }
        function some(collection, predicate, guard) {
          var func = isArray(collection) ? arraySome : baseSome;
          if (guard && isIterateeCall(collection, predicate, guard)) {
            predicate = undefined2;
          }
          return func(collection, getIteratee(predicate, 3));
        }
        var sortBy = baseRest(function(collection, iteratees) {
          if (collection == null) {
            return [];
          }
          var length = iteratees.length;
          if (length > 1 && isIterateeCall(collection, iteratees[0], iteratees[1])) {
            iteratees = [];
          } else if (length > 2 && isIterateeCall(iteratees[0], iteratees[1], iteratees[2])) {
            iteratees = [iteratees[0]];
          }
          return baseOrderBy(collection, baseFlatten(iteratees, 1), []);
        });
        var now = ctxNow || function() {
          return root.Date.now();
        };
        function after(n, func) {
          if (typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          n = toInteger(n);
          return function() {
            if (--n < 1) {
              return func.apply(this, arguments);
            }
          };
        }
        function ary(func, n, guard) {
          n = guard ? undefined2 : n;
          n = func && n == null ? func.length : n;
          return createWrap(func, WRAP_ARY_FLAG, undefined2, undefined2, undefined2, undefined2, n);
        }
        function before(n, func) {
          var result2;
          if (typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          n = toInteger(n);
          return function() {
            if (--n > 0) {
              result2 = func.apply(this, arguments);
            }
            if (n <= 1) {
              func = undefined2;
            }
            return result2;
          };
        }
        var bind = baseRest(function(func, thisArg, partials) {
          var bitmask = WRAP_BIND_FLAG;
          if (partials.length) {
            var holders = replaceHolders(partials, getHolder(bind));
            bitmask |= WRAP_PARTIAL_FLAG;
          }
          return createWrap(func, bitmask, thisArg, partials, holders);
        });
        var bindKey = baseRest(function(object, key, partials) {
          var bitmask = WRAP_BIND_FLAG | WRAP_BIND_KEY_FLAG;
          if (partials.length) {
            var holders = replaceHolders(partials, getHolder(bindKey));
            bitmask |= WRAP_PARTIAL_FLAG;
          }
          return createWrap(key, bitmask, object, partials, holders);
        });
        function curry(func, arity, guard) {
          arity = guard ? undefined2 : arity;
          var result2 = createWrap(func, WRAP_CURRY_FLAG, undefined2, undefined2, undefined2, undefined2, undefined2, arity);
          result2.placeholder = curry.placeholder;
          return result2;
        }
        function curryRight(func, arity, guard) {
          arity = guard ? undefined2 : arity;
          var result2 = createWrap(func, WRAP_CURRY_RIGHT_FLAG, undefined2, undefined2, undefined2, undefined2, undefined2, arity);
          result2.placeholder = curryRight.placeholder;
          return result2;
        }
        function debounce2(func, wait, options) {
          var lastArgs, lastThis, maxWait, result2, timerId, lastCallTime, lastInvokeTime = 0, leading = false, maxing = false, trailing = true;
          if (typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          wait = toNumber(wait) || 0;
          if (isObject(options)) {
            leading = !!options.leading;
            maxing = "maxWait" in options;
            maxWait = maxing ? nativeMax(toNumber(options.maxWait) || 0, wait) : maxWait;
            trailing = "trailing" in options ? !!options.trailing : trailing;
          }
          function invokeFunc(time) {
            var args = lastArgs, thisArg = lastThis;
            lastArgs = lastThis = undefined2;
            lastInvokeTime = time;
            result2 = func.apply(thisArg, args);
            return result2;
          }
          function leadingEdge(time) {
            lastInvokeTime = time;
            timerId = setTimeout2(timerExpired, wait);
            return leading ? invokeFunc(time) : result2;
          }
          function remainingWait(time) {
            var timeSinceLastCall = time - lastCallTime, timeSinceLastInvoke = time - lastInvokeTime, timeWaiting = wait - timeSinceLastCall;
            return maxing ? nativeMin(timeWaiting, maxWait - timeSinceLastInvoke) : timeWaiting;
          }
          function shouldInvoke(time) {
            var timeSinceLastCall = time - lastCallTime, timeSinceLastInvoke = time - lastInvokeTime;
            return lastCallTime === undefined2 || timeSinceLastCall >= wait || timeSinceLastCall < 0 || maxing && timeSinceLastInvoke >= maxWait;
          }
          function timerExpired() {
            var time = now();
            if (shouldInvoke(time)) {
              return trailingEdge(time);
            }
            timerId = setTimeout2(timerExpired, remainingWait(time));
          }
          function trailingEdge(time) {
            timerId = undefined2;
            if (trailing && lastArgs) {
              return invokeFunc(time);
            }
            lastArgs = lastThis = undefined2;
            return result2;
          }
          function cancel() {
            if (timerId !== undefined2) {
              clearTimeout2(timerId);
            }
            lastInvokeTime = 0;
            lastArgs = lastCallTime = lastThis = timerId = undefined2;
          }
          function flush() {
            return timerId === undefined2 ? result2 : trailingEdge(now());
          }
          function debounced() {
            var time = now(), isInvoking = shouldInvoke(time);
            lastArgs = arguments;
            lastThis = this;
            lastCallTime = time;
            if (isInvoking) {
              if (timerId === undefined2) {
                return leadingEdge(lastCallTime);
              }
              if (maxing) {
                clearTimeout2(timerId);
                timerId = setTimeout2(timerExpired, wait);
                return invokeFunc(lastCallTime);
              }
            }
            if (timerId === undefined2) {
              timerId = setTimeout2(timerExpired, wait);
            }
            return result2;
          }
          debounced.cancel = cancel;
          debounced.flush = flush;
          return debounced;
        }
        var defer = baseRest(function(func, args) {
          return baseDelay(func, 1, args);
        });
        var delay = baseRest(function(func, wait, args) {
          return baseDelay(func, toNumber(wait) || 0, args);
        });
        function flip(func) {
          return createWrap(func, WRAP_FLIP_FLAG);
        }
        function memoize(func, resolver) {
          if (typeof func != "function" || resolver != null && typeof resolver != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          var memoized = function() {
            var args = arguments, key = resolver ? resolver.apply(this, args) : args[0], cache = memoized.cache;
            if (cache.has(key)) {
              return cache.get(key);
            }
            var result2 = func.apply(this, args);
            memoized.cache = cache.set(key, result2) || cache;
            return result2;
          };
          memoized.cache = new (memoize.Cache || MapCache)();
          return memoized;
        }
        memoize.Cache = MapCache;
        function negate(predicate) {
          if (typeof predicate != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          return function() {
            var args = arguments;
            switch (args.length) {
              case 0:
                return !predicate.call(this);
              case 1:
                return !predicate.call(this, args[0]);
              case 2:
                return !predicate.call(this, args[0], args[1]);
              case 3:
                return !predicate.call(this, args[0], args[1], args[2]);
            }
            return !predicate.apply(this, args);
          };
        }
        function once(func) {
          return before(2, func);
        }
        var overArgs = castRest(function(func, transforms) {
          transforms = transforms.length == 1 && isArray(transforms[0]) ? arrayMap(transforms[0], baseUnary(getIteratee())) : arrayMap(baseFlatten(transforms, 1), baseUnary(getIteratee()));
          var funcsLength = transforms.length;
          return baseRest(function(args) {
            var index = -1, length = nativeMin(args.length, funcsLength);
            while (++index < length) {
              args[index] = transforms[index].call(this, args[index]);
            }
            return apply(func, this, args);
          });
        });
        var partial = baseRest(function(func, partials) {
          var holders = replaceHolders(partials, getHolder(partial));
          return createWrap(func, WRAP_PARTIAL_FLAG, undefined2, partials, holders);
        });
        var partialRight = baseRest(function(func, partials) {
          var holders = replaceHolders(partials, getHolder(partialRight));
          return createWrap(func, WRAP_PARTIAL_RIGHT_FLAG, undefined2, partials, holders);
        });
        var rearg = flatRest(function(func, indexes) {
          return createWrap(func, WRAP_REARG_FLAG, undefined2, undefined2, undefined2, indexes);
        });
        function rest(func, start) {
          if (typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          start = start === undefined2 ? start : toInteger(start);
          return baseRest(func, start);
        }
        function spread(func, start) {
          if (typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          start = start == null ? 0 : nativeMax(toInteger(start), 0);
          return baseRest(function(args) {
            var array = args[start], otherArgs = castSlice(args, 0, start);
            if (array) {
              arrayPush(otherArgs, array);
            }
            return apply(func, this, otherArgs);
          });
        }
        function throttle(func, wait, options) {
          var leading = true, trailing = true;
          if (typeof func != "function") {
            throw new TypeError2(FUNC_ERROR_TEXT);
          }
          if (isObject(options)) {
            leading = "leading" in options ? !!options.leading : leading;
            trailing = "trailing" in options ? !!options.trailing : trailing;
          }
          return debounce2(func, wait, {
            "leading": leading,
            "maxWait": wait,
            "trailing": trailing
          });
        }
        function unary(func) {
          return ary(func, 1);
        }
        function wrap(value, wrapper) {
          return partial(castFunction(wrapper), value);
        }
        function castArray() {
          if (!arguments.length) {
            return [];
          }
          var value = arguments[0];
          return isArray(value) ? value : [value];
        }
        function clone(value) {
          return baseClone(value, CLONE_SYMBOLS_FLAG);
        }
        function cloneWith(value, customizer) {
          customizer = typeof customizer == "function" ? customizer : undefined2;
          return baseClone(value, CLONE_SYMBOLS_FLAG, customizer);
        }
        function cloneDeep(value) {
          return baseClone(value, CLONE_DEEP_FLAG | CLONE_SYMBOLS_FLAG);
        }
        function cloneDeepWith(value, customizer) {
          customizer = typeof customizer == "function" ? customizer : undefined2;
          return baseClone(value, CLONE_DEEP_FLAG | CLONE_SYMBOLS_FLAG, customizer);
        }
        function conformsTo(object, source) {
          return source == null || baseConformsTo(object, source, keys(source));
        }
        function eq(value, other) {
          return value === other || value !== value && other !== other;
        }
        var gt = createRelationalOperation(baseGt);
        var gte = createRelationalOperation(function(value, other) {
          return value >= other;
        });
        var isArguments = baseIsArguments(/* @__PURE__ */ function() {
          return arguments;
        }()) ? baseIsArguments : function(value) {
          return isObjectLike(value) && hasOwnProperty.call(value, "callee") && !propertyIsEnumerable.call(value, "callee");
        };
        var isArray = Array2.isArray;
        var isArrayBuffer = nodeIsArrayBuffer ? baseUnary(nodeIsArrayBuffer) : baseIsArrayBuffer;
        function isArrayLike(value) {
          return value != null && isLength(value.length) && !isFunction(value);
        }
        function isArrayLikeObject(value) {
          return isObjectLike(value) && isArrayLike(value);
        }
        function isBoolean(value) {
          return value === true || value === false || isObjectLike(value) && baseGetTag(value) == boolTag;
        }
        var isBuffer = nativeIsBuffer || stubFalse;
        var isDate = nodeIsDate ? baseUnary(nodeIsDate) : baseIsDate;
        function isElement(value) {
          return isObjectLike(value) && value.nodeType === 1 && !isPlainObject(value);
        }
        function isEmpty(value) {
          if (value == null) {
            return true;
          }
          if (isArrayLike(value) && (isArray(value) || typeof value == "string" || typeof value.splice == "function" || isBuffer(value) || isTypedArray(value) || isArguments(value))) {
            return !value.length;
          }
          var tag = getTag(value);
          if (tag == mapTag || tag == setTag) {
            return !value.size;
          }
          if (isPrototype(value)) {
            return !baseKeys(value).length;
          }
          for (var key in value) {
            if (hasOwnProperty.call(value, key)) {
              return false;
            }
          }
          return true;
        }
        function isEqual2(value, other) {
          return baseIsEqual(value, other);
        }
        function isEqualWith(value, other, customizer) {
          customizer = typeof customizer == "function" ? customizer : undefined2;
          var result2 = customizer ? customizer(value, other) : undefined2;
          return result2 === undefined2 ? baseIsEqual(value, other, undefined2, customizer) : !!result2;
        }
        function isError(value) {
          if (!isObjectLike(value)) {
            return false;
          }
          var tag = baseGetTag(value);
          return tag == errorTag || tag == domExcTag || typeof value.message == "string" && typeof value.name == "string" && !isPlainObject(value);
        }
        function isFinite2(value) {
          return typeof value == "number" && nativeIsFinite(value);
        }
        function isFunction(value) {
          if (!isObject(value)) {
            return false;
          }
          var tag = baseGetTag(value);
          return tag == funcTag || tag == genTag || tag == asyncTag || tag == proxyTag;
        }
        function isInteger(value) {
          return typeof value == "number" && value == toInteger(value);
        }
        function isLength(value) {
          return typeof value == "number" && value > -1 && value % 1 == 0 && value <= MAX_SAFE_INTEGER;
        }
        function isObject(value) {
          var type = typeof value;
          return value != null && (type == "object" || type == "function");
        }
        function isObjectLike(value) {
          return value != null && typeof value == "object";
        }
        var isMap = nodeIsMap ? baseUnary(nodeIsMap) : baseIsMap;
        function isMatch(object, source) {
          return object === source || baseIsMatch(object, source, getMatchData(source));
        }
        function isMatchWith(object, source, customizer) {
          customizer = typeof customizer == "function" ? customizer : undefined2;
          return baseIsMatch(object, source, getMatchData(source), customizer);
        }
        function isNaN(value) {
          return isNumber(value) && value != +value;
        }
        function isNative(value) {
          if (isMaskable(value)) {
            throw new Error2(CORE_ERROR_TEXT);
          }
          return baseIsNative(value);
        }
        function isNull(value) {
          return value === null;
        }
        function isNil(value) {
          return value == null;
        }
        function isNumber(value) {
          return typeof value == "number" || isObjectLike(value) && baseGetTag(value) == numberTag;
        }
        function isPlainObject(value) {
          if (!isObjectLike(value) || baseGetTag(value) != objectTag) {
            return false;
          }
          var proto = getPrototype(value);
          if (proto === null) {
            return true;
          }
          var Ctor = hasOwnProperty.call(proto, "constructor") && proto.constructor;
          return typeof Ctor == "function" && Ctor instanceof Ctor && funcToString.call(Ctor) == objectCtorString;
        }
        var isRegExp = nodeIsRegExp ? baseUnary(nodeIsRegExp) : baseIsRegExp;
        function isSafeInteger(value) {
          return isInteger(value) && value >= -MAX_SAFE_INTEGER && value <= MAX_SAFE_INTEGER;
        }
        var isSet = nodeIsSet ? baseUnary(nodeIsSet) : baseIsSet;
        function isString(value) {
          return typeof value == "string" || !isArray(value) && isObjectLike(value) && baseGetTag(value) == stringTag;
        }
        function isSymbol(value) {
          return typeof value == "symbol" || isObjectLike(value) && baseGetTag(value) == symbolTag;
        }
        var isTypedArray = nodeIsTypedArray ? baseUnary(nodeIsTypedArray) : baseIsTypedArray;
        function isUndefined(value) {
          return value === undefined2;
        }
        function isWeakMap(value) {
          return isObjectLike(value) && getTag(value) == weakMapTag;
        }
        function isWeakSet(value) {
          return isObjectLike(value) && baseGetTag(value) == weakSetTag;
        }
        var lt = createRelationalOperation(baseLt);
        var lte = createRelationalOperation(function(value, other) {
          return value <= other;
        });
        function toArray(value) {
          if (!value) {
            return [];
          }
          if (isArrayLike(value)) {
            return isString(value) ? stringToArray(value) : copyArray(value);
          }
          if (symIterator && value[symIterator]) {
            return iteratorToArray(value[symIterator]());
          }
          var tag = getTag(value), func = tag == mapTag ? mapToArray : tag == setTag ? setToArray : values;
          return func(value);
        }
        function toFinite(value) {
          if (!value) {
            return value === 0 ? value : 0;
          }
          value = toNumber(value);
          if (value === INFINITY || value === -INFINITY) {
            var sign = value < 0 ? -1 : 1;
            return sign * MAX_INTEGER;
          }
          return value === value ? value : 0;
        }
        function toInteger(value) {
          var result2 = toFinite(value), remainder = result2 % 1;
          return result2 === result2 ? remainder ? result2 - remainder : result2 : 0;
        }
        function toLength(value) {
          return value ? baseClamp(toInteger(value), 0, MAX_ARRAY_LENGTH) : 0;
        }
        function toNumber(value) {
          if (typeof value == "number") {
            return value;
          }
          if (isSymbol(value)) {
            return NAN;
          }
          if (isObject(value)) {
            var other = typeof value.valueOf == "function" ? value.valueOf() : value;
            value = isObject(other) ? other + "" : other;
          }
          if (typeof value != "string") {
            return value === 0 ? value : +value;
          }
          value = baseTrim(value);
          var isBinary = reIsBinary.test(value);
          return isBinary || reIsOctal.test(value) ? freeParseInt(value.slice(2), isBinary ? 2 : 8) : reIsBadHex.test(value) ? NAN : +value;
        }
        function toPlainObject(value) {
          return copyObject(value, keysIn(value));
        }
        function toSafeInteger(value) {
          return value ? baseClamp(toInteger(value), -MAX_SAFE_INTEGER, MAX_SAFE_INTEGER) : value === 0 ? value : 0;
        }
        function toString(value) {
          return value == null ? "" : baseToString(value);
        }
        var assign = createAssigner(function(object, source) {
          if (isPrototype(source) || isArrayLike(source)) {
            copyObject(source, keys(source), object);
            return;
          }
          for (var key in source) {
            if (hasOwnProperty.call(source, key)) {
              assignValue(object, key, source[key]);
            }
          }
        });
        var assignIn = createAssigner(function(object, source) {
          copyObject(source, keysIn(source), object);
        });
        var assignInWith = createAssigner(function(object, source, srcIndex, customizer) {
          copyObject(source, keysIn(source), object, customizer);
        });
        var assignWith = createAssigner(function(object, source, srcIndex, customizer) {
          copyObject(source, keys(source), object, customizer);
        });
        var at = flatRest(baseAt);
        function create(prototype, properties) {
          var result2 = baseCreate(prototype);
          return properties == null ? result2 : baseAssign(result2, properties);
        }
        var defaults = baseRest(function(object, sources) {
          object = Object2(object);
          var index = -1;
          var length = sources.length;
          var guard = length > 2 ? sources[2] : undefined2;
          if (guard && isIterateeCall(sources[0], sources[1], guard)) {
            length = 1;
          }
          while (++index < length) {
            var source = sources[index];
            var props = keysIn(source);
            var propsIndex = -1;
            var propsLength = props.length;
            while (++propsIndex < propsLength) {
              var key = props[propsIndex];
              var value = object[key];
              if (value === undefined2 || eq(value, objectProto[key]) && !hasOwnProperty.call(object, key)) {
                object[key] = source[key];
              }
            }
          }
          return object;
        });
        var defaultsDeep = baseRest(function(args) {
          args.push(undefined2, customDefaultsMerge);
          return apply(mergeWith, undefined2, args);
        });
        function findKey(object, predicate) {
          return baseFindKey(object, getIteratee(predicate, 3), baseForOwn);
        }
        function findLastKey(object, predicate) {
          return baseFindKey(object, getIteratee(predicate, 3), baseForOwnRight);
        }
        function forIn(object, iteratee2) {
          return object == null ? object : baseFor(object, getIteratee(iteratee2, 3), keysIn);
        }
        function forInRight(object, iteratee2) {
          return object == null ? object : baseForRight(object, getIteratee(iteratee2, 3), keysIn);
        }
        function forOwn(object, iteratee2) {
          return object && baseForOwn(object, getIteratee(iteratee2, 3));
        }
        function forOwnRight(object, iteratee2) {
          return object && baseForOwnRight(object, getIteratee(iteratee2, 3));
        }
        function functions(object) {
          return object == null ? [] : baseFunctions(object, keys(object));
        }
        function functionsIn(object) {
          return object == null ? [] : baseFunctions(object, keysIn(object));
        }
        function get(object, path6, defaultValue) {
          var result2 = object == null ? undefined2 : baseGet(object, path6);
          return result2 === undefined2 ? defaultValue : result2;
        }
        function has(object, path6) {
          return object != null && hasPath(object, path6, baseHas);
        }
        function hasIn(object, path6) {
          return object != null && hasPath(object, path6, baseHasIn);
        }
        var invert = createInverter(function(result2, value, key) {
          if (value != null && typeof value.toString != "function") {
            value = nativeObjectToString.call(value);
          }
          result2[value] = key;
        }, constant(identity));
        var invertBy = createInverter(function(result2, value, key) {
          if (value != null && typeof value.toString != "function") {
            value = nativeObjectToString.call(value);
          }
          if (hasOwnProperty.call(result2, value)) {
            result2[value].push(key);
          } else {
            result2[value] = [key];
          }
        }, getIteratee);
        var invoke = baseRest(baseInvoke);
        function keys(object) {
          return isArrayLike(object) ? arrayLikeKeys(object) : baseKeys(object);
        }
        function keysIn(object) {
          return isArrayLike(object) ? arrayLikeKeys(object, true) : baseKeysIn(object);
        }
        function mapKeys(object, iteratee2) {
          var result2 = {};
          iteratee2 = getIteratee(iteratee2, 3);
          baseForOwn(object, function(value, key, object2) {
            baseAssignValue(result2, iteratee2(value, key, object2), value);
          });
          return result2;
        }
        function mapValues(object, iteratee2) {
          var result2 = {};
          iteratee2 = getIteratee(iteratee2, 3);
          baseForOwn(object, function(value, key, object2) {
            baseAssignValue(result2, key, iteratee2(value, key, object2));
          });
          return result2;
        }
        var merge = createAssigner(function(object, source, srcIndex) {
          baseMerge(object, source, srcIndex);
        });
        var mergeWith = createAssigner(function(object, source, srcIndex, customizer) {
          baseMerge(object, source, srcIndex, customizer);
        });
        var omit = flatRest(function(object, paths) {
          var result2 = {};
          if (object == null) {
            return result2;
          }
          var isDeep = false;
          paths = arrayMap(paths, function(path6) {
            path6 = castPath(path6, object);
            isDeep || (isDeep = path6.length > 1);
            return path6;
          });
          copyObject(object, getAllKeysIn(object), result2);
          if (isDeep) {
            result2 = baseClone(result2, CLONE_DEEP_FLAG | CLONE_FLAT_FLAG | CLONE_SYMBOLS_FLAG, customOmitClone);
          }
          var length = paths.length;
          while (length--) {
            baseUnset(result2, paths[length]);
          }
          return result2;
        });
        function omitBy(object, predicate) {
          return pickBy(object, negate(getIteratee(predicate)));
        }
        var pick = flatRest(function(object, paths) {
          return object == null ? {} : basePick(object, paths);
        });
        function pickBy(object, predicate) {
          if (object == null) {
            return {};
          }
          var props = arrayMap(getAllKeysIn(object), function(prop) {
            return [prop];
          });
          predicate = getIteratee(predicate);
          return basePickBy(object, props, function(value, path6) {
            return predicate(value, path6[0]);
          });
        }
        function result(object, path6, defaultValue) {
          path6 = castPath(path6, object);
          var index = -1, length = path6.length;
          if (!length) {
            length = 1;
            object = undefined2;
          }
          while (++index < length) {
            var value = object == null ? undefined2 : object[toKey(path6[index])];
            if (value === undefined2) {
              index = length;
              value = defaultValue;
            }
            object = isFunction(value) ? value.call(object) : value;
          }
          return object;
        }
        function set(object, path6, value) {
          return object == null ? object : baseSet(object, path6, value);
        }
        function setWith(object, path6, value, customizer) {
          customizer = typeof customizer == "function" ? customizer : undefined2;
          return object == null ? object : baseSet(object, path6, value, customizer);
        }
        var toPairs = createToPairs(keys);
        var toPairsIn = createToPairs(keysIn);
        function transform(object, iteratee2, accumulator) {
          var isArr = isArray(object), isArrLike = isArr || isBuffer(object) || isTypedArray(object);
          iteratee2 = getIteratee(iteratee2, 4);
          if (accumulator == null) {
            var Ctor = object && object.constructor;
            if (isArrLike) {
              accumulator = isArr ? new Ctor() : [];
            } else if (isObject(object)) {
              accumulator = isFunction(Ctor) ? baseCreate(getPrototype(object)) : {};
            } else {
              accumulator = {};
            }
          }
          (isArrLike ? arrayEach : baseForOwn)(object, function(value, index, object2) {
            return iteratee2(accumulator, value, index, object2);
          });
          return accumulator;
        }
        function unset(object, path6) {
          return object == null ? true : baseUnset(object, path6);
        }
        function update(object, path6, updater) {
          return object == null ? object : baseUpdate(object, path6, castFunction(updater));
        }
        function updateWith(object, path6, updater, customizer) {
          customizer = typeof customizer == "function" ? customizer : undefined2;
          return object == null ? object : baseUpdate(object, path6, castFunction(updater), customizer);
        }
        function values(object) {
          return object == null ? [] : baseValues(object, keys(object));
        }
        function valuesIn(object) {
          return object == null ? [] : baseValues(object, keysIn(object));
        }
        function clamp(number, lower, upper) {
          if (upper === undefined2) {
            upper = lower;
            lower = undefined2;
          }
          if (upper !== undefined2) {
            upper = toNumber(upper);
            upper = upper === upper ? upper : 0;
          }
          if (lower !== undefined2) {
            lower = toNumber(lower);
            lower = lower === lower ? lower : 0;
          }
          return baseClamp(toNumber(number), lower, upper);
        }
        function inRange(number, start, end) {
          start = toFinite(start);
          if (end === undefined2) {
            end = start;
            start = 0;
          } else {
            end = toFinite(end);
          }
          number = toNumber(number);
          return baseInRange(number, start, end);
        }
        function random(lower, upper, floating) {
          if (floating && typeof floating != "boolean" && isIterateeCall(lower, upper, floating)) {
            upper = floating = undefined2;
          }
          if (floating === undefined2) {
            if (typeof upper == "boolean") {
              floating = upper;
              upper = undefined2;
            } else if (typeof lower == "boolean") {
              floating = lower;
              lower = undefined2;
            }
          }
          if (lower === undefined2 && upper === undefined2) {
            lower = 0;
            upper = 1;
          } else {
            lower = toFinite(lower);
            if (upper === undefined2) {
              upper = lower;
              lower = 0;
            } else {
              upper = toFinite(upper);
            }
          }
          if (lower > upper) {
            var temp = lower;
            lower = upper;
            upper = temp;
          }
          if (floating || lower % 1 || upper % 1) {
            var rand = nativeRandom();
            return nativeMin(lower + rand * (upper - lower + freeParseFloat("1e-" + ((rand + "").length - 1))), upper);
          }
          return baseRandom(lower, upper);
        }
        var camelCase = createCompounder(function(result2, word, index) {
          word = word.toLowerCase();
          return result2 + (index ? capitalize(word) : word);
        });
        function capitalize(string) {
          return upperFirst(toString(string).toLowerCase());
        }
        function deburr(string) {
          string = toString(string);
          return string && string.replace(reLatin, deburrLetter).replace(reComboMark, "");
        }
        function endsWith(string, target, position) {
          string = toString(string);
          target = baseToString(target);
          var length = string.length;
          position = position === undefined2 ? length : baseClamp(toInteger(position), 0, length);
          var end = position;
          position -= target.length;
          return position >= 0 && string.slice(position, end) == target;
        }
        function escape(string) {
          string = toString(string);
          return string && reHasUnescapedHtml.test(string) ? string.replace(reUnescapedHtml, escapeHtmlChar) : string;
        }
        function escapeRegExp(string) {
          string = toString(string);
          return string && reHasRegExpChar.test(string) ? string.replace(reRegExpChar, "\\$&") : string;
        }
        var kebabCase = createCompounder(function(result2, word, index) {
          return result2 + (index ? "-" : "") + word.toLowerCase();
        });
        var lowerCase = createCompounder(function(result2, word, index) {
          return result2 + (index ? " " : "") + word.toLowerCase();
        });
        var lowerFirst = createCaseFirst("toLowerCase");
        function pad(string, length, chars) {
          string = toString(string);
          length = toInteger(length);
          var strLength = length ? stringSize(string) : 0;
          if (!length || strLength >= length) {
            return string;
          }
          var mid = (length - strLength) / 2;
          return createPadding(nativeFloor(mid), chars) + string + createPadding(nativeCeil(mid), chars);
        }
        function padEnd(string, length, chars) {
          string = toString(string);
          length = toInteger(length);
          var strLength = length ? stringSize(string) : 0;
          return length && strLength < length ? string + createPadding(length - strLength, chars) : string;
        }
        function padStart(string, length, chars) {
          string = toString(string);
          length = toInteger(length);
          var strLength = length ? stringSize(string) : 0;
          return length && strLength < length ? createPadding(length - strLength, chars) + string : string;
        }
        function parseInt2(string, radix, guard) {
          if (guard || radix == null) {
            radix = 0;
          } else if (radix) {
            radix = +radix;
          }
          return nativeParseInt(toString(string).replace(reTrimStart, ""), radix || 0);
        }
        function repeat(string, n, guard) {
          if (guard ? isIterateeCall(string, n, guard) : n === undefined2) {
            n = 1;
          } else {
            n = toInteger(n);
          }
          return baseRepeat(toString(string), n);
        }
        function replace() {
          var args = arguments, string = toString(args[0]);
          return args.length < 3 ? string : string.replace(args[1], args[2]);
        }
        var snakeCase = createCompounder(function(result2, word, index) {
          return result2 + (index ? "_" : "") + word.toLowerCase();
        });
        function split(string, separator, limit) {
          if (limit && typeof limit != "number" && isIterateeCall(string, separator, limit)) {
            separator = limit = undefined2;
          }
          limit = limit === undefined2 ? MAX_ARRAY_LENGTH : limit >>> 0;
          if (!limit) {
            return [];
          }
          string = toString(string);
          if (string && (typeof separator == "string" || separator != null && !isRegExp(separator))) {
            separator = baseToString(separator);
            if (!separator && hasUnicode(string)) {
              return castSlice(stringToArray(string), 0, limit);
            }
          }
          return string.split(separator, limit);
        }
        var startCase = createCompounder(function(result2, word, index) {
          return result2 + (index ? " " : "") + upperFirst(word);
        });
        function startsWith(string, target, position) {
          string = toString(string);
          position = position == null ? 0 : baseClamp(toInteger(position), 0, string.length);
          target = baseToString(target);
          return string.slice(position, position + target.length) == target;
        }
        function template(string, options, guard) {
          var settings = lodash.templateSettings;
          if (guard && isIterateeCall(string, options, guard)) {
            options = undefined2;
          }
          string = toString(string);
          options = assignInWith({}, options, settings, customDefaultsAssignIn);
          var imports = assignInWith({}, options.imports, settings.imports, customDefaultsAssignIn), importsKeys = keys(imports), importsValues = baseValues(imports, importsKeys);
          var isEscaping, isEvaluating, index = 0, interpolate = options.interpolate || reNoMatch, source = "__p += '";
          var reDelimiters = RegExp2(
            (options.escape || reNoMatch).source + "|" + interpolate.source + "|" + (interpolate === reInterpolate ? reEsTemplate : reNoMatch).source + "|" + (options.evaluate || reNoMatch).source + "|$",
            "g"
          );
          var sourceURL = "//# sourceURL=" + (hasOwnProperty.call(options, "sourceURL") ? (options.sourceURL + "").replace(/\s/g, " ") : "lodash.templateSources[" + ++templateCounter + "]") + "\n";
          string.replace(reDelimiters, function(match, escapeValue, interpolateValue, esTemplateValue, evaluateValue, offset) {
            interpolateValue || (interpolateValue = esTemplateValue);
            source += string.slice(index, offset).replace(reUnescapedString, escapeStringChar);
            if (escapeValue) {
              isEscaping = true;
              source += "' +\n__e(" + escapeValue + ") +\n'";
            }
            if (evaluateValue) {
              isEvaluating = true;
              source += "';\n" + evaluateValue + ";\n__p += '";
            }
            if (interpolateValue) {
              source += "' +\n((__t = (" + interpolateValue + ")) == null ? '' : __t) +\n'";
            }
            index = offset + match.length;
            return match;
          });
          source += "';\n";
          var variable = hasOwnProperty.call(options, "variable") && options.variable;
          if (!variable) {
            source = "with (obj) {\n" + source + "\n}\n";
          } else if (reForbiddenIdentifierChars.test(variable)) {
            throw new Error2(INVALID_TEMPL_VAR_ERROR_TEXT);
          }
          source = (isEvaluating ? source.replace(reEmptyStringLeading, "") : source).replace(reEmptyStringMiddle, "$1").replace(reEmptyStringTrailing, "$1;");
          source = "function(" + (variable || "obj") + ") {\n" + (variable ? "" : "obj || (obj = {});\n") + "var __t, __p = ''" + (isEscaping ? ", __e = _.escape" : "") + (isEvaluating ? ", __j = Array.prototype.join;\nfunction print() { __p += __j.call(arguments, '') }\n" : ";\n") + source + "return __p\n}";
          var result2 = attempt(function() {
            return Function2(importsKeys, sourceURL + "return " + source).apply(undefined2, importsValues);
          });
          result2.source = source;
          if (isError(result2)) {
            throw result2;
          }
          return result2;
        }
        function toLower(value) {
          return toString(value).toLowerCase();
        }
        function toUpper(value) {
          return toString(value).toUpperCase();
        }
        function trim(string, chars, guard) {
          string = toString(string);
          if (string && (guard || chars === undefined2)) {
            return baseTrim(string);
          }
          if (!string || !(chars = baseToString(chars))) {
            return string;
          }
          var strSymbols = stringToArray(string), chrSymbols = stringToArray(chars), start = charsStartIndex(strSymbols, chrSymbols), end = charsEndIndex(strSymbols, chrSymbols) + 1;
          return castSlice(strSymbols, start, end).join("");
        }
        function trimEnd(string, chars, guard) {
          string = toString(string);
          if (string && (guard || chars === undefined2)) {
            return string.slice(0, trimmedEndIndex(string) + 1);
          }
          if (!string || !(chars = baseToString(chars))) {
            return string;
          }
          var strSymbols = stringToArray(string), end = charsEndIndex(strSymbols, stringToArray(chars)) + 1;
          return castSlice(strSymbols, 0, end).join("");
        }
        function trimStart(string, chars, guard) {
          string = toString(string);
          if (string && (guard || chars === undefined2)) {
            return string.replace(reTrimStart, "");
          }
          if (!string || !(chars = baseToString(chars))) {
            return string;
          }
          var strSymbols = stringToArray(string), start = charsStartIndex(strSymbols, stringToArray(chars));
          return castSlice(strSymbols, start).join("");
        }
        function truncate(string, options) {
          var length = DEFAULT_TRUNC_LENGTH, omission = DEFAULT_TRUNC_OMISSION;
          if (isObject(options)) {
            var separator = "separator" in options ? options.separator : separator;
            length = "length" in options ? toInteger(options.length) : length;
            omission = "omission" in options ? baseToString(options.omission) : omission;
          }
          string = toString(string);
          var strLength = string.length;
          if (hasUnicode(string)) {
            var strSymbols = stringToArray(string);
            strLength = strSymbols.length;
          }
          if (length >= strLength) {
            return string;
          }
          var end = length - stringSize(omission);
          if (end < 1) {
            return omission;
          }
          var result2 = strSymbols ? castSlice(strSymbols, 0, end).join("") : string.slice(0, end);
          if (separator === undefined2) {
            return result2 + omission;
          }
          if (strSymbols) {
            end += result2.length - end;
          }
          if (isRegExp(separator)) {
            if (string.slice(end).search(separator)) {
              var match, substring = result2;
              if (!separator.global) {
                separator = RegExp2(separator.source, toString(reFlags.exec(separator)) + "g");
              }
              separator.lastIndex = 0;
              while (match = separator.exec(substring)) {
                var newEnd = match.index;
              }
              result2 = result2.slice(0, newEnd === undefined2 ? end : newEnd);
            }
          } else if (string.indexOf(baseToString(separator), end) != end) {
            var index = result2.lastIndexOf(separator);
            if (index > -1) {
              result2 = result2.slice(0, index);
            }
          }
          return result2 + omission;
        }
        function unescape(string) {
          string = toString(string);
          return string && reHasEscapedHtml.test(string) ? string.replace(reEscapedHtml, unescapeHtmlChar) : string;
        }
        var upperCase = createCompounder(function(result2, word, index) {
          return result2 + (index ? " " : "") + word.toUpperCase();
        });
        var upperFirst = createCaseFirst("toUpperCase");
        function words(string, pattern, guard) {
          string = toString(string);
          pattern = guard ? undefined2 : pattern;
          if (pattern === undefined2) {
            return hasUnicodeWord(string) ? unicodeWords(string) : asciiWords(string);
          }
          return string.match(pattern) || [];
        }
        var attempt = baseRest(function(func, args) {
          try {
            return apply(func, undefined2, args);
          } catch (e) {
            return isError(e) ? e : new Error2(e);
          }
        });
        var bindAll = flatRest(function(object, methodNames) {
          arrayEach(methodNames, function(key) {
            key = toKey(key);
            baseAssignValue(object, key, bind(object[key], object));
          });
          return object;
        });
        function cond(pairs) {
          var length = pairs == null ? 0 : pairs.length, toIteratee = getIteratee();
          pairs = !length ? [] : arrayMap(pairs, function(pair) {
            if (typeof pair[1] != "function") {
              throw new TypeError2(FUNC_ERROR_TEXT);
            }
            return [toIteratee(pair[0]), pair[1]];
          });
          return baseRest(function(args) {
            var index = -1;
            while (++index < length) {
              var pair = pairs[index];
              if (apply(pair[0], this, args)) {
                return apply(pair[1], this, args);
              }
            }
          });
        }
        function conforms(source) {
          return baseConforms(baseClone(source, CLONE_DEEP_FLAG));
        }
        function constant(value) {
          return function() {
            return value;
          };
        }
        function defaultTo(value, defaultValue) {
          return value == null || value !== value ? defaultValue : value;
        }
        var flow = createFlow();
        var flowRight = createFlow(true);
        function identity(value) {
          return value;
        }
        function iteratee(func) {
          return baseIteratee(typeof func == "function" ? func : baseClone(func, CLONE_DEEP_FLAG));
        }
        function matches(source) {
          return baseMatches(baseClone(source, CLONE_DEEP_FLAG));
        }
        function matchesProperty(path6, srcValue) {
          return baseMatchesProperty(path6, baseClone(srcValue, CLONE_DEEP_FLAG));
        }
        var method = baseRest(function(path6, args) {
          return function(object) {
            return baseInvoke(object, path6, args);
          };
        });
        var methodOf = baseRest(function(object, args) {
          return function(path6) {
            return baseInvoke(object, path6, args);
          };
        });
        function mixin(object, source, options) {
          var props = keys(source), methodNames = baseFunctions(source, props);
          if (options == null && !(isObject(source) && (methodNames.length || !props.length))) {
            options = source;
            source = object;
            object = this;
            methodNames = baseFunctions(source, keys(source));
          }
          var chain2 = !(isObject(options) && "chain" in options) || !!options.chain, isFunc = isFunction(object);
          arrayEach(methodNames, function(methodName) {
            var func = source[methodName];
            object[methodName] = func;
            if (isFunc) {
              object.prototype[methodName] = function() {
                var chainAll = this.__chain__;
                if (chain2 || chainAll) {
                  var result2 = object(this.__wrapped__), actions = result2.__actions__ = copyArray(this.__actions__);
                  actions.push({ "func": func, "args": arguments, "thisArg": object });
                  result2.__chain__ = chainAll;
                  return result2;
                }
                return func.apply(object, arrayPush([this.value()], arguments));
              };
            }
          });
          return object;
        }
        function noConflict() {
          if (root._ === this) {
            root._ = oldDash;
          }
          return this;
        }
        function noop() {
        }
        function nthArg(n) {
          n = toInteger(n);
          return baseRest(function(args) {
            return baseNth(args, n);
          });
        }
        var over = createOver(arrayMap);
        var overEvery = createOver(arrayEvery);
        var overSome = createOver(arraySome);
        function property(path6) {
          return isKey(path6) ? baseProperty(toKey(path6)) : basePropertyDeep(path6);
        }
        function propertyOf(object) {
          return function(path6) {
            return object == null ? undefined2 : baseGet(object, path6);
          };
        }
        var range = createRange();
        var rangeRight = createRange(true);
        function stubArray() {
          return [];
        }
        function stubFalse() {
          return false;
        }
        function stubObject() {
          return {};
        }
        function stubString() {
          return "";
        }
        function stubTrue() {
          return true;
        }
        function times(n, iteratee2) {
          n = toInteger(n);
          if (n < 1 || n > MAX_SAFE_INTEGER) {
            return [];
          }
          var index = MAX_ARRAY_LENGTH, length = nativeMin(n, MAX_ARRAY_LENGTH);
          iteratee2 = getIteratee(iteratee2);
          n -= MAX_ARRAY_LENGTH;
          var result2 = baseTimes(length, iteratee2);
          while (++index < n) {
            iteratee2(index);
          }
          return result2;
        }
        function toPath(value) {
          if (isArray(value)) {
            return arrayMap(value, toKey);
          }
          return isSymbol(value) ? [value] : copyArray(stringToPath(toString(value)));
        }
        function uniqueId(prefix) {
          var id = ++idCounter;
          return toString(prefix) + id;
        }
        var add = createMathOperation(function(augend, addend) {
          return augend + addend;
        }, 0);
        var ceil = createRound("ceil");
        var divide = createMathOperation(function(dividend, divisor) {
          return dividend / divisor;
        }, 1);
        var floor = createRound("floor");
        function max(array) {
          return array && array.length ? baseExtremum(array, identity, baseGt) : undefined2;
        }
        function maxBy(array, iteratee2) {
          return array && array.length ? baseExtremum(array, getIteratee(iteratee2, 2), baseGt) : undefined2;
        }
        function mean(array) {
          return baseMean(array, identity);
        }
        function meanBy(array, iteratee2) {
          return baseMean(array, getIteratee(iteratee2, 2));
        }
        function min(array) {
          return array && array.length ? baseExtremum(array, identity, baseLt) : undefined2;
        }
        function minBy(array, iteratee2) {
          return array && array.length ? baseExtremum(array, getIteratee(iteratee2, 2), baseLt) : undefined2;
        }
        var multiply = createMathOperation(function(multiplier, multiplicand) {
          return multiplier * multiplicand;
        }, 1);
        var round = createRound("round");
        var subtract = createMathOperation(function(minuend, subtrahend) {
          return minuend - subtrahend;
        }, 0);
        function sum(array) {
          return array && array.length ? baseSum(array, identity) : 0;
        }
        function sumBy(array, iteratee2) {
          return array && array.length ? baseSum(array, getIteratee(iteratee2, 2)) : 0;
        }
        lodash.after = after;
        lodash.ary = ary;
        lodash.assign = assign;
        lodash.assignIn = assignIn;
        lodash.assignInWith = assignInWith;
        lodash.assignWith = assignWith;
        lodash.at = at;
        lodash.before = before;
        lodash.bind = bind;
        lodash.bindAll = bindAll;
        lodash.bindKey = bindKey;
        lodash.castArray = castArray;
        lodash.chain = chain;
        lodash.chunk = chunk;
        lodash.compact = compact;
        lodash.concat = concat;
        lodash.cond = cond;
        lodash.conforms = conforms;
        lodash.constant = constant;
        lodash.countBy = countBy;
        lodash.create = create;
        lodash.curry = curry;
        lodash.curryRight = curryRight;
        lodash.debounce = debounce2;
        lodash.defaults = defaults;
        lodash.defaultsDeep = defaultsDeep;
        lodash.defer = defer;
        lodash.delay = delay;
        lodash.difference = difference;
        lodash.differenceBy = differenceBy;
        lodash.differenceWith = differenceWith;
        lodash.drop = drop;
        lodash.dropRight = dropRight;
        lodash.dropRightWhile = dropRightWhile;
        lodash.dropWhile = dropWhile;
        lodash.fill = fill;
        lodash.filter = filter;
        lodash.flatMap = flatMap;
        lodash.flatMapDeep = flatMapDeep;
        lodash.flatMapDepth = flatMapDepth;
        lodash.flatten = flatten;
        lodash.flattenDeep = flattenDeep;
        lodash.flattenDepth = flattenDepth;
        lodash.flip = flip;
        lodash.flow = flow;
        lodash.flowRight = flowRight;
        lodash.fromPairs = fromPairs;
        lodash.functions = functions;
        lodash.functionsIn = functionsIn;
        lodash.groupBy = groupBy;
        lodash.initial = initial;
        lodash.intersection = intersection;
        lodash.intersectionBy = intersectionBy;
        lodash.intersectionWith = intersectionWith;
        lodash.invert = invert;
        lodash.invertBy = invertBy;
        lodash.invokeMap = invokeMap;
        lodash.iteratee = iteratee;
        lodash.keyBy = keyBy;
        lodash.keys = keys;
        lodash.keysIn = keysIn;
        lodash.map = map;
        lodash.mapKeys = mapKeys;
        lodash.mapValues = mapValues;
        lodash.matches = matches;
        lodash.matchesProperty = matchesProperty;
        lodash.memoize = memoize;
        lodash.merge = merge;
        lodash.mergeWith = mergeWith;
        lodash.method = method;
        lodash.methodOf = methodOf;
        lodash.mixin = mixin;
        lodash.negate = negate;
        lodash.nthArg = nthArg;
        lodash.omit = omit;
        lodash.omitBy = omitBy;
        lodash.once = once;
        lodash.orderBy = orderBy;
        lodash.over = over;
        lodash.overArgs = overArgs;
        lodash.overEvery = overEvery;
        lodash.overSome = overSome;
        lodash.partial = partial;
        lodash.partialRight = partialRight;
        lodash.partition = partition;
        lodash.pick = pick;
        lodash.pickBy = pickBy;
        lodash.property = property;
        lodash.propertyOf = propertyOf;
        lodash.pull = pull;
        lodash.pullAll = pullAll;
        lodash.pullAllBy = pullAllBy;
        lodash.pullAllWith = pullAllWith;
        lodash.pullAt = pullAt;
        lodash.range = range;
        lodash.rangeRight = rangeRight;
        lodash.rearg = rearg;
        lodash.reject = reject;
        lodash.remove = remove;
        lodash.rest = rest;
        lodash.reverse = reverse;
        lodash.sampleSize = sampleSize;
        lodash.set = set;
        lodash.setWith = setWith;
        lodash.shuffle = shuffle;
        lodash.slice = slice;
        lodash.sortBy = sortBy;
        lodash.sortedUniq = sortedUniq;
        lodash.sortedUniqBy = sortedUniqBy;
        lodash.split = split;
        lodash.spread = spread;
        lodash.tail = tail;
        lodash.take = take;
        lodash.takeRight = takeRight;
        lodash.takeRightWhile = takeRightWhile;
        lodash.takeWhile = takeWhile;
        lodash.tap = tap;
        lodash.throttle = throttle;
        lodash.thru = thru;
        lodash.toArray = toArray;
        lodash.toPairs = toPairs;
        lodash.toPairsIn = toPairsIn;
        lodash.toPath = toPath;
        lodash.toPlainObject = toPlainObject;
        lodash.transform = transform;
        lodash.unary = unary;
        lodash.union = union;
        lodash.unionBy = unionBy;
        lodash.unionWith = unionWith;
        lodash.uniq = uniq;
        lodash.uniqBy = uniqBy;
        lodash.uniqWith = uniqWith;
        lodash.unset = unset;
        lodash.unzip = unzip;
        lodash.unzipWith = unzipWith;
        lodash.update = update;
        lodash.updateWith = updateWith;
        lodash.values = values;
        lodash.valuesIn = valuesIn;
        lodash.without = without;
        lodash.words = words;
        lodash.wrap = wrap;
        lodash.xor = xor;
        lodash.xorBy = xorBy;
        lodash.xorWith = xorWith;
        lodash.zip = zip;
        lodash.zipObject = zipObject;
        lodash.zipObjectDeep = zipObjectDeep;
        lodash.zipWith = zipWith;
        lodash.entries = toPairs;
        lodash.entriesIn = toPairsIn;
        lodash.extend = assignIn;
        lodash.extendWith = assignInWith;
        mixin(lodash, lodash);
        lodash.add = add;
        lodash.attempt = attempt;
        lodash.camelCase = camelCase;
        lodash.capitalize = capitalize;
        lodash.ceil = ceil;
        lodash.clamp = clamp;
        lodash.clone = clone;
        lodash.cloneDeep = cloneDeep;
        lodash.cloneDeepWith = cloneDeepWith;
        lodash.cloneWith = cloneWith;
        lodash.conformsTo = conformsTo;
        lodash.deburr = deburr;
        lodash.defaultTo = defaultTo;
        lodash.divide = divide;
        lodash.endsWith = endsWith;
        lodash.eq = eq;
        lodash.escape = escape;
        lodash.escapeRegExp = escapeRegExp;
        lodash.every = every;
        lodash.find = find;
        lodash.findIndex = findIndex;
        lodash.findKey = findKey;
        lodash.findLast = findLast;
        lodash.findLastIndex = findLastIndex;
        lodash.findLastKey = findLastKey;
        lodash.floor = floor;
        lodash.forEach = forEach;
        lodash.forEachRight = forEachRight;
        lodash.forIn = forIn;
        lodash.forInRight = forInRight;
        lodash.forOwn = forOwn;
        lodash.forOwnRight = forOwnRight;
        lodash.get = get;
        lodash.gt = gt;
        lodash.gte = gte;
        lodash.has = has;
        lodash.hasIn = hasIn;
        lodash.head = head;
        lodash.identity = identity;
        lodash.includes = includes;
        lodash.indexOf = indexOf;
        lodash.inRange = inRange;
        lodash.invoke = invoke;
        lodash.isArguments = isArguments;
        lodash.isArray = isArray;
        lodash.isArrayBuffer = isArrayBuffer;
        lodash.isArrayLike = isArrayLike;
        lodash.isArrayLikeObject = isArrayLikeObject;
        lodash.isBoolean = isBoolean;
        lodash.isBuffer = isBuffer;
        lodash.isDate = isDate;
        lodash.isElement = isElement;
        lodash.isEmpty = isEmpty;
        lodash.isEqual = isEqual2;
        lodash.isEqualWith = isEqualWith;
        lodash.isError = isError;
        lodash.isFinite = isFinite2;
        lodash.isFunction = isFunction;
        lodash.isInteger = isInteger;
        lodash.isLength = isLength;
        lodash.isMap = isMap;
        lodash.isMatch = isMatch;
        lodash.isMatchWith = isMatchWith;
        lodash.isNaN = isNaN;
        lodash.isNative = isNative;
        lodash.isNil = isNil;
        lodash.isNull = isNull;
        lodash.isNumber = isNumber;
        lodash.isObject = isObject;
        lodash.isObjectLike = isObjectLike;
        lodash.isPlainObject = isPlainObject;
        lodash.isRegExp = isRegExp;
        lodash.isSafeInteger = isSafeInteger;
        lodash.isSet = isSet;
        lodash.isString = isString;
        lodash.isSymbol = isSymbol;
        lodash.isTypedArray = isTypedArray;
        lodash.isUndefined = isUndefined;
        lodash.isWeakMap = isWeakMap;
        lodash.isWeakSet = isWeakSet;
        lodash.join = join5;
        lodash.kebabCase = kebabCase;
        lodash.last = last;
        lodash.lastIndexOf = lastIndexOf;
        lodash.lowerCase = lowerCase;
        lodash.lowerFirst = lowerFirst;
        lodash.lt = lt;
        lodash.lte = lte;
        lodash.max = max;
        lodash.maxBy = maxBy;
        lodash.mean = mean;
        lodash.meanBy = meanBy;
        lodash.min = min;
        lodash.minBy = minBy;
        lodash.stubArray = stubArray;
        lodash.stubFalse = stubFalse;
        lodash.stubObject = stubObject;
        lodash.stubString = stubString;
        lodash.stubTrue = stubTrue;
        lodash.multiply = multiply;
        lodash.nth = nth;
        lodash.noConflict = noConflict;
        lodash.noop = noop;
        lodash.now = now;
        lodash.pad = pad;
        lodash.padEnd = padEnd;
        lodash.padStart = padStart;
        lodash.parseInt = parseInt2;
        lodash.random = random;
        lodash.reduce = reduce;
        lodash.reduceRight = reduceRight;
        lodash.repeat = repeat;
        lodash.replace = replace;
        lodash.result = result;
        lodash.round = round;
        lodash.runInContext = runInContext2;
        lodash.sample = sample;
        lodash.size = size;
        lodash.snakeCase = snakeCase;
        lodash.some = some;
        lodash.sortedIndex = sortedIndex;
        lodash.sortedIndexBy = sortedIndexBy;
        lodash.sortedIndexOf = sortedIndexOf;
        lodash.sortedLastIndex = sortedLastIndex;
        lodash.sortedLastIndexBy = sortedLastIndexBy;
        lodash.sortedLastIndexOf = sortedLastIndexOf;
        lodash.startCase = startCase;
        lodash.startsWith = startsWith;
        lodash.subtract = subtract;
        lodash.sum = sum;
        lodash.sumBy = sumBy;
        lodash.template = template;
        lodash.times = times;
        lodash.toFinite = toFinite;
        lodash.toInteger = toInteger;
        lodash.toLength = toLength;
        lodash.toLower = toLower;
        lodash.toNumber = toNumber;
        lodash.toSafeInteger = toSafeInteger;
        lodash.toString = toString;
        lodash.toUpper = toUpper;
        lodash.trim = trim;
        lodash.trimEnd = trimEnd;
        lodash.trimStart = trimStart;
        lodash.truncate = truncate;
        lodash.unescape = unescape;
        lodash.uniqueId = uniqueId;
        lodash.upperCase = upperCase;
        lodash.upperFirst = upperFirst;
        lodash.each = forEach;
        lodash.eachRight = forEachRight;
        lodash.first = head;
        mixin(lodash, function() {
          var source = {};
          baseForOwn(lodash, function(func, methodName) {
            if (!hasOwnProperty.call(lodash.prototype, methodName)) {
              source[methodName] = func;
            }
          });
          return source;
        }(), { "chain": false });
        lodash.VERSION = VERSION;
        arrayEach(["bind", "bindKey", "curry", "curryRight", "partial", "partialRight"], function(methodName) {
          lodash[methodName].placeholder = lodash;
        });
        arrayEach(["drop", "take"], function(methodName, index) {
          LazyWrapper.prototype[methodName] = function(n) {
            n = n === undefined2 ? 1 : nativeMax(toInteger(n), 0);
            var result2 = this.__filtered__ && !index ? new LazyWrapper(this) : this.clone();
            if (result2.__filtered__) {
              result2.__takeCount__ = nativeMin(n, result2.__takeCount__);
            } else {
              result2.__views__.push({
                "size": nativeMin(n, MAX_ARRAY_LENGTH),
                "type": methodName + (result2.__dir__ < 0 ? "Right" : "")
              });
            }
            return result2;
          };
          LazyWrapper.prototype[methodName + "Right"] = function(n) {
            return this.reverse()[methodName](n).reverse();
          };
        });
        arrayEach(["filter", "map", "takeWhile"], function(methodName, index) {
          var type = index + 1, isFilter = type == LAZY_FILTER_FLAG || type == LAZY_WHILE_FLAG;
          LazyWrapper.prototype[methodName] = function(iteratee2) {
            var result2 = this.clone();
            result2.__iteratees__.push({
              "iteratee": getIteratee(iteratee2, 3),
              "type": type
            });
            result2.__filtered__ = result2.__filtered__ || isFilter;
            return result2;
          };
        });
        arrayEach(["head", "last"], function(methodName, index) {
          var takeName = "take" + (index ? "Right" : "");
          LazyWrapper.prototype[methodName] = function() {
            return this[takeName](1).value()[0];
          };
        });
        arrayEach(["initial", "tail"], function(methodName, index) {
          var dropName = "drop" + (index ? "" : "Right");
          LazyWrapper.prototype[methodName] = function() {
            return this.__filtered__ ? new LazyWrapper(this) : this[dropName](1);
          };
        });
        LazyWrapper.prototype.compact = function() {
          return this.filter(identity);
        };
        LazyWrapper.prototype.find = function(predicate) {
          return this.filter(predicate).head();
        };
        LazyWrapper.prototype.findLast = function(predicate) {
          return this.reverse().find(predicate);
        };
        LazyWrapper.prototype.invokeMap = baseRest(function(path6, args) {
          if (typeof path6 == "function") {
            return new LazyWrapper(this);
          }
          return this.map(function(value) {
            return baseInvoke(value, path6, args);
          });
        });
        LazyWrapper.prototype.reject = function(predicate) {
          return this.filter(negate(getIteratee(predicate)));
        };
        LazyWrapper.prototype.slice = function(start, end) {
          start = toInteger(start);
          var result2 = this;
          if (result2.__filtered__ && (start > 0 || end < 0)) {
            return new LazyWrapper(result2);
          }
          if (start < 0) {
            result2 = result2.takeRight(-start);
          } else if (start) {
            result2 = result2.drop(start);
          }
          if (end !== undefined2) {
            end = toInteger(end);
            result2 = end < 0 ? result2.dropRight(-end) : result2.take(end - start);
          }
          return result2;
        };
        LazyWrapper.prototype.takeRightWhile = function(predicate) {
          return this.reverse().takeWhile(predicate).reverse();
        };
        LazyWrapper.prototype.toArray = function() {
          return this.take(MAX_ARRAY_LENGTH);
        };
        baseForOwn(LazyWrapper.prototype, function(func, methodName) {
          var checkIteratee = /^(?:filter|find|map|reject)|While$/.test(methodName), isTaker = /^(?:head|last)$/.test(methodName), lodashFunc = lodash[isTaker ? "take" + (methodName == "last" ? "Right" : "") : methodName], retUnwrapped = isTaker || /^find/.test(methodName);
          if (!lodashFunc) {
            return;
          }
          lodash.prototype[methodName] = function() {
            var value = this.__wrapped__, args = isTaker ? [1] : arguments, isLazy = value instanceof LazyWrapper, iteratee2 = args[0], useLazy = isLazy || isArray(value);
            var interceptor = function(value2) {
              var result3 = lodashFunc.apply(lodash, arrayPush([value2], args));
              return isTaker && chainAll ? result3[0] : result3;
            };
            if (useLazy && checkIteratee && typeof iteratee2 == "function" && iteratee2.length != 1) {
              isLazy = useLazy = false;
            }
            var chainAll = this.__chain__, isHybrid = !!this.__actions__.length, isUnwrapped = retUnwrapped && !chainAll, onlyLazy = isLazy && !isHybrid;
            if (!retUnwrapped && useLazy) {
              value = onlyLazy ? value : new LazyWrapper(this);
              var result2 = func.apply(value, args);
              result2.__actions__.push({ "func": thru, "args": [interceptor], "thisArg": undefined2 });
              return new LodashWrapper(result2, chainAll);
            }
            if (isUnwrapped && onlyLazy) {
              return func.apply(this, args);
            }
            result2 = this.thru(interceptor);
            return isUnwrapped ? isTaker ? result2.value()[0] : result2.value() : result2;
          };
        });
        arrayEach(["pop", "push", "shift", "sort", "splice", "unshift"], function(methodName) {
          var func = arrayProto[methodName], chainName = /^(?:push|sort|unshift)$/.test(methodName) ? "tap" : "thru", retUnwrapped = /^(?:pop|shift)$/.test(methodName);
          lodash.prototype[methodName] = function() {
            var args = arguments;
            if (retUnwrapped && !this.__chain__) {
              var value = this.value();
              return func.apply(isArray(value) ? value : [], args);
            }
            return this[chainName](function(value2) {
              return func.apply(isArray(value2) ? value2 : [], args);
            });
          };
        });
        baseForOwn(LazyWrapper.prototype, function(func, methodName) {
          var lodashFunc = lodash[methodName];
          if (lodashFunc) {
            var key = lodashFunc.name + "";
            if (!hasOwnProperty.call(realNames, key)) {
              realNames[key] = [];
            }
            realNames[key].push({ "name": methodName, "func": lodashFunc });
          }
        });
        realNames[createHybrid(undefined2, WRAP_BIND_KEY_FLAG).name] = [{
          "name": "wrapper",
          "func": undefined2
        }];
        LazyWrapper.prototype.clone = lazyClone;
        LazyWrapper.prototype.reverse = lazyReverse;
        LazyWrapper.prototype.value = lazyValue;
        lodash.prototype.at = wrapperAt;
        lodash.prototype.chain = wrapperChain;
        lodash.prototype.commit = wrapperCommit;
        lodash.prototype.next = wrapperNext;
        lodash.prototype.plant = wrapperPlant;
        lodash.prototype.reverse = wrapperReverse;
        lodash.prototype.toJSON = lodash.prototype.valueOf = lodash.prototype.value = wrapperValue;
        lodash.prototype.first = lodash.prototype.head;
        if (symIterator) {
          lodash.prototype[symIterator] = wrapperToIterator;
        }
        return lodash;
      };
      var _ = runInContext();
      if (typeof define == "function" && typeof define.amd == "object" && define.amd) {
        root._ = _;
        define(function() {
          return _;
        });
      } else if (freeModule) {
        (freeModule.exports = _)._ = _;
        freeExports._ = _;
      } else {
        root._ = _;
      }
    }).call(exports2);
  }
});

// src/extension.ts
var extension_exports = {};
__export(extension_exports, {
  activate: () => activate,
  deactivate: () => deactivate
});
module.exports = __toCommonJS(extension_exports);
var vs15 = __toESM(require("vscode"));

// src/model.ts
var vs3 = __toESM(require("vscode"));

// src/tracer.ts
var vs = __toESM(require("vscode"));
var TraceLevel = /* @__PURE__ */ ((TraceLevel2) => {
  TraceLevel2[TraceLevel2["Silent"] = 0] = "Silent";
  TraceLevel2[TraceLevel2["Error"] = 1] = "Error";
  TraceLevel2[TraceLevel2["Warning"] = 2] = "Warning";
  TraceLevel2[TraceLevel2["Info"] = 3] = "Info";
  TraceLevel2[TraceLevel2["Verbose"] = 4] = "Verbose";
  return TraceLevel2;
})(TraceLevel || {});
function isDebugging() {
  const args = process.execArgv;
  return args && args.some((arg) => arg.startsWith("--inspect"));
}
var Tracer = class {
  static set level(value) {
    if (value === "error") {
      this._level = 1 /* Error */;
    } else if (value === "warning") {
      this._level = 2 /* Warning */;
    } else if (value === "info") {
      this._level = 3 /* Info */;
    } else if (value === "verbose") {
      this._level = 4 /* Verbose */;
    } else {
      this._level = 0 /* Silent */;
    }
  }
  static verbose(message) {
    this._log(message, 4 /* Verbose */);
  }
  static info(message) {
    this._log(message, 3 /* Info */);
  }
  static warning(message) {
    this._log(message, 2 /* Warning */);
  }
  static error(message) {
    this._log(message, 1 /* Error */);
  }
  static get output() {
    if (!this._output) {
      this._output = vs.window.createOutputChannel("GitHD");
    }
    return this._output;
  }
  static get timestamp() {
    return (/* @__PURE__ */ new Date()).toISOString().split("T")[1].replace("Z", "");
  }
  static _log(message, level) {
    if (this._level >= level) {
      message = `[${this.timestamp}][${TraceLevel[level]}] ${message}`;
      if (this._debugging) {
        console.log("[GitHD]", message);
      } else {
        this.output.appendLine(message);
      }
    }
  }
};
Tracer._level = 0 /* Silent */;
Tracer._debugging = isDebugging();

// src/contextTracker.ts
var import_lodash = __toESM(require_lodash());
var vs2 = __toESM(require("vscode"));
var maxTrackedCount = 100;
var ContextTracker = class {
  constructor(_goBackFlag, _goForwardFlag) {
    this._goBackFlag = _goBackFlag;
    this._goForwardFlag = _goForwardFlag;
    this._tracker = [];
    this._nextIndex = 0;
  }
  setContext(context) {
    if (!context || (0, import_lodash.isEqual)(this.current, context)) {
      return;
    }
    vs2.commands.executeCommand("setContext", this._goForwardFlag, false);
    if (this._nextIndex == 1) {
      vs2.commands.executeCommand("setContext", this._goBackFlag, true);
    }
    if (this._nextIndex == maxTrackedCount) {
      this._tracker = this._tracker.slice(1, this._nextIndex);
    } else {
      this._nextIndex++;
      this._tracker = this._tracker.slice(0, this._nextIndex);
    }
    this._tracker[this._nextIndex - 1] = context;
  }
  get current() {
    return this._nextIndex > 0 ? this._tracker[this._nextIndex - 1] : void 0;
  }
  goBack() {
    if (this._nextIndex <= 1) {
      return false;
    }
    if (this._nextIndex == this._tracker.length) {
      vs2.commands.executeCommand("setContext", this._goForwardFlag, true);
    }
    this._nextIndex--;
    if (this._nextIndex <= 1) {
      vs2.commands.executeCommand("setContext", this._goBackFlag, false);
    }
    return true;
  }
  goForward() {
    if (this._nextIndex == this._tracker.length) {
      return false;
    }
    if (this._nextIndex <= 1) {
      vs2.commands.executeCommand("setContext", this._goBackFlag, true);
    }
    this._nextIndex++;
    if (this._nextIndex == this._tracker.length) {
      vs2.commands.executeCommand("setContext", this._goForwardFlag, false);
    }
    return true;
  }
  clear() {
    this._nextIndex = 0;
    this._tracker = [];
    vs2.commands.executeCommand("setContext", this._goBackFlag, false);
    vs2.commands.executeCommand("setContext", this._goForwardFlag, false);
  }
};

// src/model.ts
function getConfiguration() {
  return {
    withFolder: vs3.workspace.getConfiguration("githd.explorerView").get("withFolder"),
    commitsCount: vs3.workspace.getConfiguration("githd.logView").get("commitsCount"),
    expressMode: vs3.workspace.getConfiguration("githd.logView").get("expressMode"),
    displayExpress: vs3.workspace.getConfiguration("githd.logView").get("displayExpressStatus"),
    blameEnabled: vs3.workspace.getConfiguration("githd.blameView").get("enabled"),
    disabledInEditor: vs3.workspace.getConfiguration("githd.editor").get("disabled"),
    traceLevel: vs3.workspace.getConfiguration("githd").get("traceLevel"),
    cacheEnabled: vs3.workspace.getConfiguration("githd").get("cacheEnabled"),
    dataBucketsCount: vs3.workspace.getConfiguration("githd.statsView").get("dataBucketsCount")
  };
}
var Model = class {
  constructor(context, _loader) {
    this._loader = _loader;
    this._historyViewContextTracker = new ContextTracker(
      "githd.canGoBackHistoryView",
      "githd.canGoForwardHistoryView"
    );
    this._filesViewContextTracker = new ContextTracker(
      "githd.canGoBackFilesView",
      "canGoForwardFilesView"
    );
    this._onDidChangeConfiguration = new vs3.EventEmitter();
    this._onDidChangeFilesViewContext = new vs3.EventEmitter();
    this._onDidChangeHistoryViewContext = new vs3.EventEmitter();
    this._config = getConfiguration();
    Tracer.level = this._config.traceLevel;
    vs3.commands.executeCommand("setContext", "githd.disableInEditor", this._config.disabledInEditor);
    this._loader.enableCache(this._config.cacheEnabled);
    vs3.workspace.onDidChangeConfiguration(
      () => {
        let newConfig = getConfiguration();
        if (newConfig.withFolder !== this._config.withFolder || newConfig.commitsCount !== this._config.commitsCount || newConfig.expressMode !== this._config.expressMode || newConfig.displayExpress !== this._config.displayExpress || newConfig.blameEnabled !== this._config.blameEnabled || newConfig.disabledInEditor !== this._config.disabledInEditor || newConfig.traceLevel !== this._config.traceLevel || newConfig.cacheEnabled !== this._config.cacheEnabled || newConfig.dataBucketsCount !== this._config.dataBucketsCount) {
          Tracer.info(`Model: configuration updated ${JSON.stringify(newConfig)}`);
          this._config = newConfig;
          this._onDidChangeConfiguration.fire(newConfig);
          Tracer.level = newConfig.traceLevel;
          vs3.commands.executeCommand("setContext", "githd.disableInEditor", newConfig.disabledInEditor);
          this._loader.enableCache(newConfig.cacheEnabled);
        }
      },
      null,
      context.subscriptions
    );
    context.subscriptions.push(
      this._onDidChangeConfiguration,
      this._onDidChangeFilesViewContext,
      this._onDidChangeHistoryViewContext
    );
  }
  get configuration() {
    return this._config;
  }
  get filesViewContext() {
    const current = this._filesViewContextTracker.current;
    if (!current) {
      return;
    }
    return {
      repo: current.repo,
      focusedLineInfo: current.focusedLineInfo,
      isStash: current.isStash,
      leftRef: current.leftRef,
      rightRef: current.rightRef,
      specifiedPath: current.specifiedPath
    };
  }
  setFilesViewContext(context) {
    var _a3;
    (_a3 = context.specifiedPath) == null ? void 0 : _a3.fsPath;
    Tracer.info(`Model: set filesViewContext - ${JSON.stringify(context)}`);
    const currentContext = this.filesViewContext;
    if (!currentContext || currentContext.leftRef != (context == null ? void 0 : context.leftRef) || currentContext.rightRef != (context == null ? void 0 : context.rightRef) || currentContext.specifiedPath != (context == null ? void 0 : context.specifiedPath) || currentContext.focusedLineInfo != (context == null ? void 0 : context.focusedLineInfo)) {
      this._filesViewContextTracker.setContext({
        repo: context.repo,
        focusedLineInfo: context.focusedLineInfo,
        isStash: context.isStash,
        leftRef: context.leftRef,
        rightRef: context.rightRef,
        specifiedPath: context.specifiedPath
      });
      this._onDidChangeFilesViewContext.fire(context);
    }
    vs3.commands.executeCommand("workbench.view.extension.githd-explorer");
  }
  goBackFilesViewContext() {
    if (this._filesViewContextTracker.goBack()) {
      const context = this._filesViewContextTracker.current;
      Tracer.verbose(`Model: go back files view context ${context == null ? void 0 : context.leftRef}..${context == null ? void 0 : context.rightRef}`);
      this._onDidChangeFilesViewContext.fire(context);
    }
  }
  goForwardFilesViewContext() {
    if (this._filesViewContextTracker.goForward()) {
      const context = this._filesViewContextTracker.current;
      Tracer.verbose(`Model: go forward files view context ${context == null ? void 0 : context.leftRef}..${context == null ? void 0 : context.rightRef}`);
      this._onDidChangeFilesViewContext.fire(context);
    }
  }
  clearFilesViewContexts() {
    this._filesViewContextTracker.clear();
    this._onDidChangeFilesViewContext.fire(void 0);
  }
  get historyViewContext() {
    const current = this._historyViewContextTracker.current;
    if (!current) {
      return;
    }
    return {
      branch: current.branch,
      repo: current.repo,
      author: current.author,
      isStash: current.isStash,
      line: current.line,
      specifiedPath: current.specifiedPath,
      startTime: current.startTime,
      endTime: current.endTime
    };
  }
  async setHistoryViewContext(context) {
    var _a3;
    (_a3 = context.specifiedPath) == null ? void 0 : _a3.fsPath;
    if (context && !context.branch) {
      context.branch = await this._loader.getCurrentBranch(context == null ? void 0 : context.repo);
    }
    Tracer.info(`Model: set historyViewContext - ${JSON.stringify(context)}`);
    this._historyViewContextTracker.setContext({
      branch: context.branch,
      repo: context.repo,
      author: context.author,
      isStash: context.isStash,
      line: context.line,
      specifiedPath: context.specifiedPath,
      startTime: context.startTime,
      endTime: context.endTime
    });
    this._onDidChangeHistoryViewContext.fire(context);
  }
  goBackHistoryViewContext() {
    if (this._historyViewContextTracker.goBack()) {
      const context = this._historyViewContextTracker.current;
      Tracer.verbose(`Model: go back history view context - ${JSON.stringify(context)}`);
      this._onDidChangeHistoryViewContext.fire(context);
    }
  }
  goForwardHistoryViewContext() {
    if (this._historyViewContextTracker.goForward()) {
      const context = this._historyViewContextTracker.current;
      Tracer.verbose(`Model: go forward history view context - ${JSON.stringify(context)}`);
      this._onDidChangeHistoryViewContext.fire(context);
    }
  }
  clearHistoryViewContexts() {
    this._historyViewContextTracker.clear();
  }
  get onDidChangeConfiguration() {
    return this._onDidChangeConfiguration.event;
  }
  get onDidChangeFilesViewContext() {
    return this._onDidChangeFilesViewContext.event;
  }
  get onDidChangeHistoryViewContext() {
    return this._onDidChangeHistoryViewContext.event;
  }
};

// src/gitService.ts
var path = __toESM(require("path"));
var fs = __toESM(require("fs"));
var os = __toESM(require("os"));
var vs5 = __toESM(require("vscode"));
var import_child_process = require("child_process");

// src/utils.ts
var vs4 = __toESM(require("vscode"));
var prHoverMessage = new vs4.MarkdownString("Click to see GitHub PR");
function decorateWithoutWhitespace(ranges, target, line, offset) {
  let start = 0;
  let newWord = true;
  let i = 0;
  for (; i < target.length; ++i) {
    if (target[i] === " " || target[i] === "	" || target[i] === "\n") {
      if (!newWord) {
        newWord = true;
        ranges.push(new vs4.Range(line, offset + start, line, offset + i));
      }
    } else {
      if (newWord) {
        newWord = false;
        start = i;
      }
    }
  }
  if (!newWord) {
    ranges.push(new vs4.Range(line, offset + start, line, offset + i));
  }
}
function getTextEditors(scheme) {
  return vs4.window.visibleTextEditors.filter((editor) => editor.document.uri.scheme === scheme);
}
function getPullRequests(content) {
  const found = content.match(/#[0-9]+/g);
  if (!found) {
    return [];
  }
  return found.map((pr) => [pr, content.indexOf(pr)]);
}
function isEmptyHash(hash) {
  return !hash || hash.startsWith("0000");
}
function debounce(func, wait) {
  let timeout = null;
  return (...args) => {
    if (timeout) {
      clearTimeout(timeout);
    }
    timeout = setTimeout(() => func(...args), wait);
  };
}

// src/gitService.ts
var EntrySeparator = "[githd-es]";
var FormatSeparator = "[githd-fs]";
function normalizeFilePath(fsPath) {
  fsPath = path.normalize(fsPath);
  if (os.platform() == "win32") {
    fsPath = fsPath.toLocaleLowerCase();
  }
  if (!fsPath.endsWith(path.sep)) {
    fsPath = fsPath + path.sep;
  }
  return fsPath;
}
var GitCommittedFileImpl = class {
  constructor(_repo, gitRelativePath, gitRelativeOldPath, status) {
    this._repo = _repo;
    this.gitRelativePath = gitRelativePath;
    this.gitRelativeOldPath = gitRelativeOldPath;
    this.status = status;
  }
  get fileUri() {
    return vs5.Uri.file(path.join(this._repo.root, this.gitRelativePath));
  }
  get oldFileUri() {
    return vs5.Uri.file(path.join(this._repo.root, this.gitRelativeOldPath));
  }
};
function singleLined(value) {
  return value.replace(/\r?\n|\r/g, " ");
}
var GitService = class {
  constructor(context) {
    this._gitRepos = [];
    this._onDidChangeGitRepositories = new vs5.EventEmitter();
    this._onDidChangeCurrentGitRepo = new vs5.EventEmitter();
    var _a3;
    context.subscriptions.push(
      vs5.workspace.onDidChangeWorkspaceFolders((_) => this.updateGitRoots(vs5.workspace.workspaceFolders)),
      this._onDidChangeGitRepositories,
      this._onDidChangeCurrentGitRepo
    );
    let gitPath = (_a3 = vs5.workspace.getConfiguration("git").get("path")) != null ? _a3 : "";
    if (gitPath) {
      try {
        (0, import_child_process.execSync)(gitPath);
      } catch (err) {
        gitPath = "git";
      }
    } else {
      gitPath = "git";
    }
    this._gitPath = gitPath;
  }
  get onDidChangeGitRepositories() {
    return this._onDidChangeGitRepositories.event;
  }
  get onDidChangeCurrentGitRepo() {
    return this._onDidChangeCurrentGitRepo.event;
  }
  async updateGitRoots(wsFolders) {
    this._gitRepos = [];
    vs5.commands.executeCommand("setContext", "githd.hasGitRepo", false);
    this._onDidChangeGitRepositories.fire([]);
    const start = Date.now();
    const promises = wsFolders ? wsFolders.map((wsFolder) => this._scanFolder(wsFolder.uri.fsPath, true)) : [Promise.resolve(0)];
    const count = await Promise.all(promises).then((results) => results.reduce((a, b) => a + b, 0));
    if (count === 1) {
      this.updateCurrentGitRepo(this._gitRepos[0]);
    }
    Tracer.info(`updateGitRoots: ${wsFolders == null ? void 0 : wsFolders.length} wsFolders ${count} subFolders (${Date.now() - start}ms)`);
  }
  getGitRepos() {
    return this._gitRepos;
  }
  get currentGitRepo() {
    return this._currentRepo;
  }
  updateCurrentGitRepo(repo) {
    this._currentRepo = repo;
    this._onDidChangeCurrentGitRepo.fire(repo);
  }
  async getGitRepo(fsPath) {
    if (fs.statSync(fsPath).isFile()) {
      fsPath = path.dirname(fsPath);
    }
    fsPath = normalizeFilePath(fsPath);
    let repo = this._gitRepos.find((r) => fsPath.startsWith(r.root));
    if (repo) {
      return repo;
    }
    let root = (await this._exec(["rev-parse", "--show-toplevel"], fsPath)).trim();
    if (root) {
      root = normalizeFilePath(root);
      if (this._gitRepos.findIndex((value) => {
        return value.root == root;
      }) === -1) {
        const remoteUrl = await this._getRemoteUrl(fsPath);
        repo = { root, remoteUrl };
        this._gitRepos.push(repo);
        vs5.commands.executeCommand("setContext", "githd.hasGitRepo", true);
        this._onDidChangeGitRepositories.fire(this.getGitRepos());
      }
    }
    return repo;
  }
  async getGitRelativePath(file) {
    if (!file) {
      return;
    }
    const repo = await this.getGitRepo(file.fsPath);
    if (!repo) {
      return;
    }
    let relative3 = path.relative(repo.root, file.fsPath).replace(/\\/g, "/");
    return relative3 === "" ? "." : relative3;
  }
  async getCurrentBranch(repo) {
    if (!repo) {
      return;
    }
    return (await this._exec(["rev-parse", "--abbrev-ref", "HEAD"], repo.root)).trim();
  }
  async getCommitsCount(repo, branch, file, author, startTime, endTime) {
    var _a3;
    if (!repo) {
      return 0;
    }
    let args = ["rev-list", "--simplify-merges", "--count", branch];
    if (author) {
      args.push(`--author=${author}`);
    }
    if (startTime) {
      args.push(`--after=${startTime.toISOString()}`);
    }
    if (endTime) {
      args.push(`--before=${endTime.toISOString()}`);
    }
    if (file) {
      const filePath = (_a3 = await this.getGitRelativePath(file)) != null ? _a3 : ".";
      args.push("--", "--follow", filePath);
    } else {
      args.push("--");
    }
    return parseInt(await this._exec(args, repo.root));
  }
  async getRefs(repo) {
    if (!repo) {
      return [];
    }
    const result = await this._exec(["for-each-ref", "--format=%(refname) %(objectname:short)"], repo.root);
    const fn = (line) => {
      let match;
      if (match = /^refs\/heads\/([^ ]+) ([0-9a-f]+)$/.exec(line)) {
        return { name: match[1], commit: match[2], type: 0 /* Head */ };
      } else if (match = /^refs\/remotes\/([^/]+)\/([^ ]+) ([0-9a-f]+)$/.exec(line)) {
        return {
          name: `${match[1]}/${match[2]}`,
          commit: match[3],
          type: 1 /* RemoteHead */
        };
      } else if (match = /^refs\/tags\/([^ ]+) ([0-9a-f]+)$/.exec(line)) {
        return { name: match[1], commit: match[2], type: 2 /* Tag */ };
      }
      return null;
    };
    return result.trim().split("\n").filter((line) => !!line).map(fn).filter((ref) => !!ref);
  }
  // returns [total commits stats, [commits]]
  async getCommittedFiles(repo, rightRef, leftRef, isStash) {
    if (!repo) {
      return ["", []];
    }
    let args = ["show", "--format=%h", "--name-status", rightRef];
    if (leftRef) {
      args = ["diff", "--name-status", `${leftRef}..${rightRef}`];
    } else if (isStash) {
      args.unshift("stash");
    }
    const result = await this._exec(args, repo.root);
    let files = [];
    result.split(/\r?\n/g).forEach((value, index) => {
      if (value) {
        let info = value.split(/\t/g);
        if (info.length < 2) {
          return;
        }
        let gitRelativePath;
        let gitRelativeOldPath;
        const status = info[0][0].toLocaleUpperCase();
        switch (status) {
          case "M":
          case "A":
          case "D":
          case "T":
            gitRelativeOldPath = info[1];
            gitRelativePath = info[1];
            break;
          case "R":
          case "C":
            gitRelativeOldPath = info[1];
            gitRelativePath = info[2];
            break;
          default:
            throw new Error("Cannot parse " + info);
        }
        files.push(new GitCommittedFileImpl(repo, gitRelativePath, gitRelativeOldPath, status));
      }
    });
    const stats = !leftRef && !isStash ? await this._updateCommitsStats(repo, rightRef, files) : "";
    return [stats, files];
  }
  async getLogEntries(repo, express, start, count, branch, isStash, file, line, author, startTime, endTime) {
    var _a3;
    Tracer.info(
      `Get entries. repo: ${repo.root}, express: ${express}, start: ${start}, count: ${count}, branch: ${branch}, isStash: ${isStash}, file: ${file == null ? void 0 : file.fsPath}, line: ${line}, author: ${author}, startTime: ${startTime == null ? void 0 : startTime.toISOString()}, endTime: ${endTime == null ? void 0 : endTime.toISOString()}`
    );
    if (!repo) {
      return [];
    }
    let format = EntrySeparator;
    if (isStash) {
      format += "%gd:";
    }
    let logItem;
    ((logItem2) => {
      logItem2[logItem2["subject"] = 0] = "subject";
      logItem2[logItem2["hash"] = 1] = "hash";
      logItem2[logItem2["ref"] = 2] = "ref";
      logItem2[logItem2["author"] = 3] = "author";
      logItem2[logItem2["email"] = 4] = "email";
      logItem2[logItem2["timestamp"] = 5] = "timestamp";
      logItem2[logItem2["date"] = 6] = "date";
      logItem2[logItem2["relativeDate"] = 7] = "relativeDate";
      logItem2[logItem2["additional"] = 8] = "additional";
      logItem2[logItem2["total"] = 9] = "total";
    })(logItem || (logItem = {}));
    format += `%s${FormatSeparator}%h${FormatSeparator}%d${FormatSeparator}%aN${FormatSeparator}%ae${FormatSeparator}%ct${FormatSeparator}%cd${FormatSeparator}%cr${FormatSeparator}`;
    let args = [`--format=${format}`, "--date=local"];
    if (!express && !line) {
      args.push("--shortstat");
    }
    if (isStash) {
      args.unshift("stash", "list");
    } else {
      args.unshift("log", `--skip=${start}`, `--max-count=${count}`, "--date-order", "--simplify-merges", branch);
      if (author) {
        args.push(`--author=${author}`);
      }
      if (startTime) {
        args.push(`--after=${startTime.toISOString()}`);
      }
      if (endTime) {
        args.push(`--before=${endTime.toISOString()}`);
      }
      if (file) {
        const filePath = (_a3 = await this.getGitRelativePath(file)) != null ? _a3 : ".";
        if (line) {
          args.push(`-L ${line},${line}:${filePath}`, "--");
        } else {
          args.push("--", "--follow", filePath);
        }
      } else {
        args.push("--");
      }
    }
    const result = await this._exec(args, repo.root);
    let entries = [];
    result.split(EntrySeparator).forEach((entry) => {
      if (!entry) {
        return;
      }
      let subject;
      let hash;
      let ref;
      let author2;
      let email;
      let timestamp;
      let date;
      let relativeDate;
      let stat;
      let lineInfo;
      entry.split(FormatSeparator).forEach((value, index) => {
        switch (index % 9 /* total */) {
          case 0 /* subject */:
            subject = singleLined(value);
            break;
          case 1 /* hash */:
            hash = value;
            break;
          case 2 /* ref */:
            ref = value;
            break;
          case 3 /* author */:
            author2 = value;
            break;
          case 4 /* email */:
            email = value;
            break;
          case 5 /* timestamp */:
            timestamp = parseInt(value);
            break;
          case 6 /* date */:
            date = value;
            break;
          case 7 /* relativeDate */:
            relativeDate = value;
            break;
          case 8 /* additional */:
            if (!!line) {
              lineInfo = value.trim();
            } else {
              stat = value.trim();
            }
            entries.push({
              subject,
              hash,
              ref,
              author: author2,
              email,
              timestamp,
              date,
              relativeDate,
              stat,
              lineInfo
            });
            break;
        }
      });
    });
    return entries;
  }
  async getCommitDetails(repo, ref, isStash) {
    if (!repo) {
      return "";
    }
    const format = isStash ? `Stash:         %H %nAuthor:        %aN <%aE> %nAuthorDate:    %ad %n%n%s %n` : "Commit:        %H %nAuthor:        %aN <%aE> %nAuthorDate:    %ad %nCommit:        %cN <%cE> %nCommitDate:    %cd %n%n%s %n";
    let details = await this._exec(
      ["show", `--format=${format}`, "--no-patch", "--date=local", ref],
      repo.root
    );
    const body = (await this._exec(["show", "--format=%b", "--no-patch", ref], repo.root)).trim();
    if (body) {
      details += body + "\r\n\r\n";
    }
    details += "-----------------------------\r\n\r\n";
    details += await this._exec(["show", "--format=", "--stat", "--stat-width=120", ref], repo.root);
    return details;
  }
  async getAuthors(repo) {
    if (!repo) {
      return [];
    }
    const result = (await this._exec(["shortlog", "-se", "HEAD"], repo.root)).trim();
    return result.split(/\r?\n/g).map((item) => {
      item = item.trim();
      let start = item.search(/ |\t/);
      item = item.substring(start + 1).trim();
      start = item.indexOf("<");
      const name = item.substring(0, start);
      const email = item.substring(start + 1, item.length - 1);
      return { name, email };
    });
  }
  async getBlameItem(file, line) {
    var _a3, _b2, _c, _d, _e;
    const repo = await this.getGitRepo(file.fsPath);
    if (!repo) {
      return;
    }
    const filePath = file.fsPath;
    const result = await this._exec(
      ["blame", `${filePath}`, "-L", `${line + 1},${line + 1}`, "--incremental", "--root"],
      repo.root
    );
    let hash = "";
    let subject = "";
    let author = "";
    let date = "";
    let email = "";
    result.split(/\r?\n/g).forEach((line2, index) => {
      if (index == 0) {
        hash = line2.split(" ")[0];
      } else {
        const infoName = line2.split(" ")[0];
        const info = line2.substring(infoName.length).trim();
        if (!info) {
          return;
        }
        switch (infoName) {
          case "author":
            author = info;
            break;
          case "committer-time":
            date = new Date(parseInt(info) * 1e3).toLocaleDateString();
            break;
          case "author-mail":
            email = info;
            break;
          case "summary":
            subject = singleLined(info);
            break;
          default:
            break;
        }
      }
    });
    if ([hash, subject, author, email, date].some((v) => !v)) {
      Tracer.warning(
        `Blame info missed. repo ${repo.root} file ${filePath}:${line} ${hash} author: ${author}, mail: ${email}, date: ${date}, summary: ${subject}`
      );
      return;
    }
    if (isEmptyHash(hash)) {
      Tracer.verbose(`Blame info skipped. repo ${repo.root} file ${filePath}:${line} ${hash}`);
      return { file, line, hash };
    }
    const addition = await this._exec(
      ["show", `--format=%h${FormatSeparator}%cr${FormatSeparator}%b${FormatSeparator}`, "--stat", `${hash}`],
      repo.root
    );
    const items = addition.split(FormatSeparator);
    hash = (_a3 = items[0]) != null ? _a3 : "";
    const relativeDate = (_b2 = items[1]) != null ? _b2 : "";
    const body = (_d = (_c = items[2]) == null ? void 0 : _c.trim()) != null ? _d : "";
    const stat = " " + ((_e = items[3]) == null ? void 0 : _e.trim());
    return {
      file,
      line,
      subject,
      body,
      hash,
      author,
      date,
      email,
      relativeDate,
      stat
    };
  }
  // commits will be updated with stats
  async _updateCommitsStats(repo, ref, commits) {
    const res = await this._exec(["show", "--format=", "--stat", "--stat-width=200", ref], repo.root);
    const stats = /* @__PURE__ */ new Map();
    let total = "";
    res.split(/\r?\n/g).forEach((line) => {
      const items = line.split("|");
      if (items.length == 2) {
        stats.set(items[0].trim(), items[1].trim());
      } else if (line.indexOf("changed") > 0) {
        total = line;
      }
    });
    commits.forEach((commit) => commit.stat = stats.get(commit.gitRelativeOldPath));
    return total;
  }
  async getCommits(repo, branch) {
    if (!branch) {
      return [];
    }
    const result = await this._exec(
      ["log", "--format=%h", "--simplify-merges", "--date-order", branch, "--"],
      repo.root
    );
    return result.split(/\r?\n/g);
  }
  async _scanFolder(folder, includeSubFolders) {
    const children = fs.readdirSync(folder, { withFileTypes: true });
    const promises = children.filter((child) => child.isDirectory()).map(async (child) => {
      if (child.name === ".git") {
        await this.getGitRepo(folder);
        return 1;
      }
      if (includeSubFolders) {
        return await this._scanFolder(path.join(folder, child.name));
      }
      return 0;
    });
    return await Promise.all(promises).then((results) => results.reduce((a, b) => a + b, 0));
  }
  async _getRemoteUrl(fsPath) {
    let remotes = (await this._exec(["remote"], fsPath)).split(/\r?\n/g);
    const remote = remotes.find((r) => r === "upstream") || remotes.find((r) => r === "origin");
    if (!remote) {
      return "";
    }
    let remoteGit = (await this._exec(["remote", "get-url", "--push", remote], fsPath)).trim();
    if (remoteGit.startsWith("git@")) {
      remoteGit = remoteGit.replace(":", "/").replace("git@", "https://");
    }
    let url = remoteGit.replace(/\.git$/g, "");
    if (url.search(/\.(com|org|net|io|cloud)\//g) > 0) {
      return url;
    }
    Tracer.info("Remote URL: " + remoteGit);
    return url.replace(/:\/\/.*?\//g, "://github.com/");
  }
  async _exec(args, cwd) {
    const start = Date.now();
    const cmd = this._gitPath;
    try {
      const result = await new Promise((resolve, reject) => {
        const childProcess = (0, import_child_process.spawn)(cmd, args, { cwd });
        childProcess.stdout.setEncoding("utf8");
        childProcess.stderr.setEncoding("utf8");
        let stdout = "", stderr = "";
        childProcess.stdout.on("data", (chunk) => {
          stdout += chunk;
        });
        childProcess.stderr.on("data", (chunk) => {
          stderr += chunk;
        });
        childProcess.on("error", reject).on("close", (code) => {
          if (code === 0) {
            resolve(stdout);
          } else {
            reject(stderr);
          }
        });
      });
      Tracer.verbose(
        `git command: ${cmd} ${args.join(" ")}. Output size: ${result.length} (${Date.now() - start}ms) ${cwd}`
      );
      return result;
    } catch (err) {
      Tracer.error(`git command failed: ${cmd} ${args.join(" ")} (${Date.now() - start}ms) ${cwd} ${err}`);
      return "";
    }
  }
};

// src/commands.ts
var path2 = __toESM(require("path"));
var vs8 = __toESM(require("vscode"));

// src/infoViewProvider.ts
var vs7 = __toESM(require("vscode"));

// src/clickable.ts
var vs6 = __toESM(require("vscode"));
var ClickableProvider = class {
  constructor(_scheme) {
    this._scheme = _scheme;
    this._clickables = [];
    this._disposables = [];
    this._lastClickedItems = [];
    this._decoration = vs6.window.createTextEditorDecorationType({
      cursor: "pointer",
      textDecoration: "underline"
    });
    this._disposables.push(vs6.languages.registerHoverProvider({ scheme: _scheme }, this));
    this._disposables.push(this._decoration);
    vs6.window.onDidChangeTextEditorSelection(
      (event) => {
        let editor = event.textEditor;
        if (editor && editor.document.uri.scheme === _scheme) {
          if (event.kind === vs6.TextEditorSelectionChangeKind.Mouse) {
            const pos = event.selections[0].anchor;
            const clickable = this._clickables.find((e) => {
              return e.range.contains(pos);
            });
            if (clickable) {
              this._onClicked(clickable, editor);
            }
          }
        }
      },
      null,
      this._disposables
    );
    vs6.window.onDidChangeActiveTextEditor(
      (editor) => {
        if (editor && editor.document.uri.scheme === _scheme) {
          this._setDecorations(editor);
        }
      },
      null,
      this._disposables
    );
    vs6.window.onDidChangeVisibleTextEditors(
      (editors) => {
        editors.forEach((editor) => {
          if (editor && editor.document.uri.scheme === _scheme) {
            this._setDecorations(editor);
          }
        });
      },
      null,
      this._disposables
    );
    vs6.workspace.onDidChangeTextDocument(
      (e) => {
        if (e.document.uri.scheme === _scheme) {
          getTextEditors(_scheme).forEach((editor) => this._setDecorations(editor));
        }
      },
      null,
      this._disposables
    );
  }
  async provideHover(document, position) {
    const clickable = this._clickables.find((e) => {
      return e.range.contains(position);
    });
    if (clickable && clickable.getHoverMessage) {
      const content = await clickable.getHoverMessage();
      return new vs6.Hover(content);
    }
  }
  addClickable(clickable) {
    this._clickables.push(clickable);
  }
  removeClickable(range) {
    if (range) {
      [this._clickables, this._lastClickedItems].forEach((clickables) => {
        const index = clickables.findIndex((e) => {
          return e.range.isEqual(range);
        });
        if (index !== -1) {
          clickables.splice(index, 1);
        }
      });
    }
  }
  clear() {
    this._clickables = [];
    getTextEditors(this._scheme).forEach((editor) => {
      this._lastClickedItems.forEach((clickable) => {
        if (clickable.clickedDecorationType) {
          editor.setDecorations(clickable.clickedDecorationType, []);
        }
      });
    });
    this._lastClickedItems = [];
  }
  dispose() {
    this._disposables.forEach((d) => d.dispose());
  }
  _onClicked(clickable, editor) {
    if (clickable.clickedDecorationType) {
      editor.setDecorations(clickable.clickedDecorationType, [clickable.range]);
      const index = this._lastClickedItems.findIndex((e) => {
        return e.clickedDecorationType === clickable.clickedDecorationType;
      });
      if (index !== -1) {
        this._lastClickedItems.splice(index, 1);
      }
      this._lastClickedItems.push(clickable);
    }
    clickable.callback();
  }
  _setDecorations(editor) {
    if (!editor || editor.document.uri.scheme !== this._scheme) {
      Tracer.warning(`Clickable: try to set decoration to wrong scheme: ${editor ? editor.document.uri.scheme : ""}`);
      return;
    }
    this._lastClickedItems.forEach((clickable) => {
      if (clickable.clickedDecorationType) {
        editor.setDecorations(clickable.clickedDecorationType, [clickable.range]);
      }
    });
    editor.setDecorations(
      this._decoration,
      this._clickables.map((clickable) => {
        return clickable.range;
      })
    );
  }
};

// src/infoViewProvider.ts
var _InfoViewProvider = class _InfoViewProvider {
  constructor(context, model, _gitService) {
    this._gitService = _gitService;
    this._prDecoration = vs7.window.createTextEditorDecorationType({
      color: new vs7.ThemeColor("textLink.foreground")
    });
    this._infoDecoration = vs7.window.createTextEditorDecorationType({
      color: new vs7.ThemeColor("githd.infoView.content")
    });
    this._infoRanges = [];
    this._prRange = [];
    this._clickableProvider = new ClickableProvider(_InfoViewProvider.scheme);
    this._onDidChange = new vs7.EventEmitter();
    context.subscriptions.push(vs7.workspace.registerTextDocumentContentProvider(_InfoViewProvider.scheme, this));
    vs7.window.onDidChangeActiveTextEditor(
      (editor) => {
        if (editor && editor.document.uri.scheme === _InfoViewProvider.scheme) {
          this._decorate(editor);
        }
      },
      null,
      context.subscriptions
    );
    vs7.window.onDidChangeVisibleTextEditors(
      (editors) => {
        editors.forEach((editor) => {
          if (editor && editor.document.uri.scheme === _InfoViewProvider.scheme) {
            this._decorate(editor);
          }
        });
      },
      null,
      context.subscriptions
    );
    vs7.workspace.onDidChangeTextDocument(
      (e) => {
        if (e.document.uri.scheme === _InfoViewProvider.scheme) {
          getTextEditors(_InfoViewProvider.scheme).forEach((editor) => this._decorate(editor));
        }
      },
      null,
      context.subscriptions
    );
    model.onDidChangeFilesViewContext((context2) => this._update(context2), null, context.subscriptions);
    context.subscriptions.push(this._onDidChange, this._infoDecoration, this._clickableProvider, this._prDecoration);
  }
  get onDidChange() {
    return this._onDidChange.event;
  }
  provideTextDocumentContent(uri) {
    var _a3;
    return (_a3 = this._content) != null ? _a3 : "";
  }
  async _update(context) {
    this._infoRanges = [];
    this._prRange = [];
    this._clickableProvider.clear();
    if (!(context == null ? void 0 : context.rightRef)) {
      return;
    }
    this._content = await this._gitService.getCommitDetails(context == null ? void 0 : context.repo, context.rightRef, context == null ? void 0 : context.isStash);
    if (this._content) {
      const remoteUrl = context == null ? void 0 : context.repo.remoteUrl;
      let addPR = false;
      if (remoteUrl) {
        addPR = remoteUrl.indexOf("github.com") > 0;
      }
      let i = 0;
      this._content.split(/\r?\n/g).forEach((line) => {
        if (addPR) {
          getPullRequests(line).forEach(([pr, start]) => {
            const range = new vs7.Range(i, start, i, start + pr.length);
            const url = remoteUrl + "/pull/" + pr.substring(1);
            this._clickableProvider.addClickable({
              range,
              callback: () => vs7.env.openExternal(vs7.Uri.parse(url)),
              getHoverMessage: () => prHoverMessage
            });
            this._prRange.push(range);
          });
        }
        decorateWithoutWhitespace(this._infoRanges, line, i, 0);
        ++i;
      });
    }
    this._onDidChange.fire(_InfoViewProvider.defaultUri);
  }
  _decorate(editor) {
    if (editor && this._content) {
      editor.setDecorations(this._infoDecoration, this._infoRanges);
      editor.setDecorations(this._prDecoration, this._prRange);
    }
  }
};
_InfoViewProvider.scheme = "githd-info";
_InfoViewProvider.defaultUri = vs7.Uri.parse(_InfoViewProvider.scheme + "://authority//Commit Info");
var InfoViewProvider = _InfoViewProvider;

// src/panelViewProvider.ts
var vscode = __toESM(require("vscode"));
var _PanelViewProvider = class _PanelViewProvider {
  constructor(context, _model) {
    this._model = _model;
    this._commits = [];
    this._shadowArea = null;
    var _a3;
    this._extensionUri = context.extensionUri;
    this._webviewUri = vscode.Uri.joinPath(this._extensionUri, "media");
    context.subscriptions.push(
      vscode.window.registerWebviewViewProvider(_PanelViewProvider.viewType, this, {
        webviewOptions: {
          retainContextWhenHidden: true
        }
      })
    );
    this._model.onDidChangeConfiguration(() => {
      const bucketsCount = this._model.configuration.dataBucketsCount;
      if (!!bucketsCount && this._dataBucketsCount !== bucketsCount) {
        this._dataBucketsCount = bucketsCount;
        this.update();
      }
    });
    this._dataBucketsCount = (_a3 = _model.configuration.dataBucketsCount) != null ? _a3 : 91;
  }
  resolveWebviewView(webviewView) {
    Tracer.verbose("PanelViewProvider: resolveWebviewView");
    this._view = webviewView;
    webviewView.webview.options = {
      enableScripts: true,
      localResourceRoots: [this._webviewUri, vscode.Uri.joinPath(this._extensionUri, "dist")]
    };
    webviewView.webview.html = this._getWebviewContent(webviewView.webview);
    setTimeout(() => {
      this.update();
      if (this._shadowArea) {
        this.setShadowArea(this._shadowArea.start, this._shadowArea.end);
      }
    }, 500);
    webviewView.onDidChangeVisibility(() => {
      if (webviewView.visible) {
      }
    });
    webviewView.webview.onDidReceiveMessage((message) => {
      switch (message.type) {
        case "selectionMade":
          this.onSelectionMade(message.start, message.end);
          break;
      }
    });
  }
  onSelectionMade(start, end) {
    const startDate = new Date(start);
    const endDate = new Date(end);
    Tracer.verbose(`PanelViewProvider: selectionMade: ${startDate.toISOString()} - ${endDate.toISOString()}`);
    const context = this._model.historyViewContext;
    if (context) {
      context.startTime = startDate;
      context.endTime = endDate;
      this._model.setHistoryViewContext(context);
    }
  }
  update() {
    Tracer.verbose(`PanelViewProvider: update: commits ${this._commits.length} buckets ${this._dataBucketsCount}`);
    if (this._view) {
      this._view.webview.postMessage({
        type: "updateChart",
        data: this._commits,
        bucketsCount: this._dataBucketsCount
      });
    }
  }
  addLogs(logs) {
    const commits = logs.filter((log) => !!log.stat).map((log) => {
      var _a3;
      return { stats: (_a3 = log.stat) != null ? _a3 : "", date: log.timestamp * 1e3 };
    });
    this._commits.push(...commits);
  }
  clearLogs() {
    this._commits = [];
  }
  setShadowArea(start, end) {
    this._shadowArea = { start, end };
    if (this._view) {
      Tracer.verbose(`PanelViewProvider: setShadowArea: ${start} - ${end}`);
      this._view.webview.postMessage({
        type: "setShadowArea",
        start: start * 1e3,
        end: end * 1e3
      });
    }
  }
  _getWebviewContent(webview) {
    const scriptUri = webview.asWebviewUri(vscode.Uri.joinPath(this._webviewUri, "stats.js"));
    const styleUri = webview.asWebviewUri(vscode.Uri.joinPath(this._webviewUri, "style.css"));
    const chartjsUri = webview.asWebviewUri(vscode.Uri.joinPath(this._extensionUri, "dist", "chart.js"));
    const chartjsAdapterUri = webview.asWebviewUri(
      vscode.Uri.joinPath(this._extensionUri, "dist", "chartjs-adapter-date-fns.bundle.js")
    );
    return `<!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <link href="${styleUri}" rel="stylesheet">
                <script src="${chartjsUri}"></script>
                <script src="${chartjsAdapterUri}"></script>
            </head>
            <body>
                <div id="chart-container">
                    <canvas id="chart"></canvas>
                </div>
                <script src="${scriptUri}"></script>
            </body>
            </html>`;
  }
};
_PanelViewProvider.viewType = "githd.stats";
var PanelViewProvider = _PanelViewProvider;

// src/commands.ts
function toGitUri(uri, ref) {
  return uri.with({
    scheme: "git",
    path: uri.path,
    query: JSON.stringify({
      path: uri.fsPath,
      ref
    })
  });
}
async function selectBranch(gitService, repo, allowEnterSha) {
  const refs = await gitService.getRefs(repo);
  const items = refs.map((ref) => {
    let description;
    if (ref.type === 0 /* Head */) {
      description = ref.commit;
    } else if (ref.type === 2 /* Tag */) {
      description = `Tag at ${ref.commit}`;
    } else if (ref.type === 1 /* RemoteHead */) {
      description = `Remote branch at ${ref.commit}`;
    }
    return { label: ref.name || ref.commit, description };
  });
  if (allowEnterSha) items.unshift(new EnterShaPickItem());
  return items;
}
async function branchCombination(gitService, repo) {
  const refs = (await gitService.getRefs(repo)).filter((ref) => {
    return ref.type != 2 /* Tag */;
  });
  const localRefs = refs.filter((ref) => {
    return ref.type != 1 /* RemoteHead */;
  });
  let items = [];
  localRefs.forEach((source) => {
    refs.forEach((target) => {
      if (source.name != target.name && source.commit != target.commit) {
        items.push({
          label: `${source.name || source.commit} .. ${target.name || target.commit}`
        });
      }
    });
  });
  return items;
}
var EnterShaPickItem = class {
  constructor() {
    this.label = "Enter commit SHA";
    this.description = "";
    this.openShaTextBox = true;
  }
};
async function selectGitRepo(gitService) {
  const repos = gitService.getGitRepos();
  if (repos.length === 0) {
    return;
  }
  if (repos.length === 1) {
    return repos[0];
  }
  const pickItems = repos.map((repo) => {
    return { label: path2.basename(repo.root), description: repo.root, repo };
  });
  const item = await vs8.window.showQuickPick(pickItems, {
    placeHolder: "Select the git repo"
  });
  return item == null ? void 0 : item.repo;
}
async function getRefFromQuickPickItem(item, inputBoxTitle) {
  return item.openShaTextBox ? await vs8.window.showInputBox({ prompt: inputBoxTitle }) : item.label;
}
async function selectAuthor(gitService, repo) {
  let authors = await gitService.getAuthors(repo);
  authors.unshift({ name: "All", email: "" });
  return authors.map((author) => {
    return { label: author.name, description: author.email };
  });
}
var Commands = [];
function command(id) {
  return function(_target, _key, descriptor) {
    if (!(typeof descriptor.value === "function")) {
      throw new Error("not supported");
    }
    Commands.push({ id, method: descriptor.value });
  };
}
var CommandCenter = class {
  constructor(context, _model, _loader, _gitService, _historyView, _explorerView) {
    this._model = _model;
    this._loader = _loader;
    this._gitService = _gitService;
    this._historyView = _historyView;
    this._explorerView = _explorerView;
    context.subscriptions.push(
      ...Commands.map(({ id, method }) => {
        return vs8.commands.registerCommand(
          id,
          (...args) => Promise.resolve(method.apply(this, args)).catch((err) => Tracer.error(`command ${id} failed ${err.message}`))
        );
      })
    );
  }
  async viewHistory() {
    Tracer.verbose("Command: githd.viewHistory");
    const repo = await this._getOrUpdateRepo();
    if (repo) {
      this._viewHistory({ repo, branch: "" });
    }
  }
  async viewFileHistory(specifiedPath = ((_b2) => (_b2 = ((_a3) => (_a3 = vs8.window.activeTextEditor) == null ? void 0 : _a3.document)()) == null ? void 0 : _b2.uri)()) {
    Tracer.verbose("Command: githd.viewFileHistory");
    if (!specifiedPath) {
      return;
    }
    let repo = await this._gitService.getGitRepo(specifiedPath.fsPath);
    if (!repo) {
      return;
    }
    return this._viewHistory({ specifiedPath, repo, branch: "" });
  }
  async viewFolderHistory(specifiedPath) {
    Tracer.verbose("Command: githd.viewFolderHistory");
    return this.viewFileHistory(specifiedPath);
  }
  async viewLineHistory(file = ((_d) => (_d = ((_c) => (_c = vs8.window.activeTextEditor) == null ? void 0 : _c.document)()) == null ? void 0 : _d.uri)()) {
    var _a3, _b2, _c2;
    Tracer.verbose("Command: githd.viewLineHistory");
    if (!file) {
      return;
    }
    let repo = await this._gitService.getGitRepo(file.fsPath);
    if (!repo) {
      return;
    }
    let line = (_c2 = (_b2 = (_a3 = vs8.window.activeTextEditor) == null ? void 0 : _a3.selection) == null ? void 0 : _b2.active) == null ? void 0 : _c2.line;
    if (!line) {
      return;
    }
    line++;
    return this._viewHistory({ specifiedPath: file, line, repo, branch: "" });
  }
  async viewAllHistory() {
    Tracer.verbose("Command: githd.viewAllHistory");
    if (this._model.historyViewContext) {
      return this._viewHistory(this._model.historyViewContext, true);
    }
    const selected = await this._getOrUpdateRepo();
    if (!selected) {
      return;
    }
    this._model.setHistoryViewContext({ repo: selected, branch: "" });
  }
  async viewBranchHistory(context) {
    var _a3;
    Tracer.verbose("Command: githd.viewBranchHistory");
    let placeHolder = `Select a ref to see it's history`;
    let repo;
    if (context) {
      repo = context.repo;
      const specifiedPath = (_a3 = this._model.historyViewContext) == null ? void 0 : _a3.specifiedPath;
      if (specifiedPath) {
        placeHolder += ` of ${path2.basename(specifiedPath.fsPath)}`;
      }
    } else {
      const selected = await this._getOrUpdateRepo();
      if (!selected) {
        return;
      }
      repo = selected;
    }
    placeHolder += ` (${repo.root})`;
    vs8.window.showQuickPick(selectBranch(this._gitService, repo), { placeHolder }).then((item) => {
      if (item) {
        if (context) {
          context.branch = item.label;
          this._viewHistory(context);
        } else {
          this._viewHistory({ branch: item.label, repo });
        }
      }
    });
  }
  viewAuthorHistory() {
    Tracer.verbose("Command: githd.viewAuthorHistory");
    const context = this._model.historyViewContext;
    let placeHolder = `Select a author to see his/her commits`;
    vs8.window.showQuickPick(selectAuthor(this._gitService, context.repo), {
      placeHolder
    }).then((item) => {
      if (item) {
        const email = item.description;
        let context2 = this._model.historyViewContext;
        if (context2) {
          context2.author = email;
          this._viewHistory(context2);
        }
      }
    });
  }
  async viewStashes() {
    Tracer.verbose("Command: githd.viewStashes");
    const repo = await this._getOrUpdateRepo();
    if (repo) {
      this._viewHistory({ repo, isStash: true, branch: "" });
    }
  }
  async diffBranch() {
    Tracer.verbose("Command: githd.diffBranch");
    const repo = await this._getOrUpdateRepo();
    if (!repo) {
      return;
    }
    this._diffSelections(repo);
  }
  // explorer/context will pass two args while editor/context will one
  async diffFile(specifiedPath, _, ref) {
    Tracer.verbose("Command: githd.diffFile");
    return this._diffPath(specifiedPath, ref);
  }
  // explorer/context will pass two args while editor/context will one
  async diffFolder(specifiedPath, _, ref) {
    Tracer.verbose("Command: githd.diffFolder");
    return this._diffPath(specifiedPath, ref);
  }
  async inputRef() {
    Tracer.verbose("Command: githd.inputRef");
    const repo = await this._getOrUpdateRepo();
    if (!repo) {
      return;
    }
    vs8.window.showInputBox({
      placeHolder: `Input a ref(sha1) to see it's committed files`
    }).then((ref) => this._model.setFilesViewContext({ rightRef: ref == null ? void 0 : ref.trim(), repo }));
  }
  openCommit(repo, ref, specifiedPath) {
    Tracer.verbose("Command: githd.openCommit");
    this._model.setFilesViewContext({ rightRef: ref, repo, specifiedPath });
  }
  goBackHistoryView() {
    Tracer.verbose("Command: githd.goBackHistoryView");
    this._model.goBackHistoryViewContext();
  }
  goForwardHistoryView() {
    Tracer.verbose("Command: githd.goForwardHistoryView");
    this._model.goForwardHistoryViewContext();
  }
  goBackFilesViewContext() {
    this._model.goBackFilesViewContext();
  }
  goForwardFilesViewContext() {
    this._model.goForwardFilesViewContext();
  }
  dummyForGoBackIcon() {
  }
  dummyForGoForwardIcon() {
  }
  async previousCommit() {
    Tracer.verbose("Command: githd.previousCommit");
    this._explorerView.showPreviousCommit();
  }
  async nextCommit() {
    Tracer.verbose("Command: githd.nextCommit");
    this._explorerView.showNextCommit();
  }
  dummyForPreviousCommitIcon() {
  }
  dummyForNextCommitIcon() {
  }
  openCommittedFile(file) {
    var _a3, _b2;
    Tracer.verbose("Command: githd.openCommittedFile");
    let rightRef = (_a3 = this._model.filesViewContext) == null ? void 0 : _a3.rightRef;
    let leftRef = rightRef + "~";
    let title = rightRef;
    if ((_b2 = this._model.filesViewContext) == null ? void 0 : _b2.leftRef) {
      leftRef = this._model.filesViewContext.leftRef;
      title = `${leftRef} .. ${rightRef}`;
    }
    vs8.commands.executeCommand(
      "vscode.diff",
      toGitUri(file.oldFileUri, leftRef),
      toGitUri(file.fileUri, rightRef),
      title + " | " + path2.basename(file.gitRelativePath),
      { preview: true }
    );
  }
  openCommitInfo() {
    Tracer.verbose("Command: githd.openCommitInfo");
    vs8.workspace.openTextDocument(InfoViewProvider.defaultUri).then((doc) => {
      vs8.languages.setTextDocumentLanguage(doc, "githd");
      vs8.window.showTextDocument(doc, { preview: true, preserveFocus: true }).then(() => vs8.commands.executeCommand("cursorTop"));
    });
  }
  openLineDiff(content) {
    Tracer.verbose("Command: githd.openLineDiff");
    vs8.workspace.openTextDocument({ content, language: "diff" }).then(
      (doc) => vs8.window.showTextDocument(doc, { preview: true, preserveFocus: true }).then(() => vs8.commands.executeCommand("cursorTop"))
    );
  }
  async diffUncommittedFile(file = ((_f) => (_f = ((_e) => (_e = vs8.window.activeTextEditor) == null ? void 0 : _e.document)()) == null ? void 0 : _f.uri)()) {
    if (!file) {
      return;
    }
    Tracer.verbose("Command: githd.diffUncommittedFile");
    const repo = await this._gitService.getGitRepo(file.fsPath);
    if (!repo) {
      return;
    }
    vs8.window.showQuickPick(selectBranch(this._gitService, repo), {
      placeHolder: `Select a ref to see the diff with local copy of ${path2.basename(file.path)}`
    }).then(async (item) => {
      if (item) {
        return await vs8.commands.executeCommand(
          "vscode.diff",
          toGitUri(file, item.label),
          file,
          `${item.label} .. Uncommitted (${path2.basename(file.path)})`,
          { preview: true }
        );
      }
    });
  }
  async setExpressMode() {
    Tracer.verbose("Command: githd.setExpressMode");
    this._historyView.express = !this._historyView.express;
  }
  async setRepository() {
    Tracer.verbose("Command: githd.setRepository");
    const repo = await selectGitRepo(this._gitService);
    if (repo) {
      this._gitService.updateCurrentGitRepo(repo);
    }
  }
  clearFilesView() {
    Tracer.verbose("Command: githd.clearFilesView");
    this._model.clearFilesViewContexts();
  }
  showStats() {
    Tracer.verbose("Command: githd.showStats");
    vs8.commands.executeCommand(`${PanelViewProvider.viewType}.focus`);
  }
  async _viewHistory(context, all = false) {
    this._historyView.loadAll = all;
    await this._model.setHistoryViewContext(context);
  }
  async _diffPath(specifiedPath, ref) {
    if (specifiedPath) {
      const repo = await this._gitService.getGitRepo(specifiedPath.fsPath);
      if (repo) {
        this._diffSelections(repo, specifiedPath, ref);
      }
    }
  }
  async _diffSelections(repo, specifiedPath, ref) {
    const branches = await selectBranch(this._gitService, repo, true);
    const branchWithCombination = await branchCombination(this._gitService, repo);
    const items = [...branches, ...branchWithCombination];
    ref = ref || await this._loader.getCurrentBranch(repo);
    const placeHolder = `Select a ref to see it's diff with ${ref} or select two refs to see their diffs`;
    vs8.window.showQuickPick(items, { placeHolder }).then(async (item) => {
      if (!item) {
        return;
      }
      let leftRef = await getRefFromQuickPickItem(
        item,
        `Input a ref(sha1) to compare with ${ref} or 'ref(sha1) .. ref(sha2)' to compare with two commits`
      );
      let rightRef = ref;
      if (!leftRef) {
        return;
      }
      if (leftRef.indexOf("..") != -1) {
        const diffBranch = leftRef.split("..");
        leftRef = diffBranch[0].trim();
        rightRef = diffBranch[1].trim();
      }
      this._model.setFilesViewContext({
        repo,
        leftRef,
        rightRef,
        specifiedPath
      });
    });
  }
  async _getOrUpdateRepo() {
    if (!this._gitService.currentGitRepo) {
      const repo = await selectGitRepo(this._gitService);
      if (!repo) {
        return;
      }
      this._gitService.updateCurrentGitRepo(repo);
    }
    return this._gitService.currentGitRepo;
  }
};
__decorateClass([
  command("githd.viewHistory")
], CommandCenter.prototype, "viewHistory", 1);
__decorateClass([
  command("githd.viewFileHistory")
], CommandCenter.prototype, "viewFileHistory", 1);
__decorateClass([
  command("githd.viewFolderHistory")
], CommandCenter.prototype, "viewFolderHistory", 1);
__decorateClass([
  command("githd.viewLineHistory")
], CommandCenter.prototype, "viewLineHistory", 1);
__decorateClass([
  command("githd.viewAllHistory")
], CommandCenter.prototype, "viewAllHistory", 1);
__decorateClass([
  command("githd.viewBranchHistory")
], CommandCenter.prototype, "viewBranchHistory", 1);
__decorateClass([
  command("githd.viewAuthorHistory")
], CommandCenter.prototype, "viewAuthorHistory", 1);
__decorateClass([
  command("githd.viewStashes")
], CommandCenter.prototype, "viewStashes", 1);
__decorateClass([
  command("githd.diffBranch")
], CommandCenter.prototype, "diffBranch", 1);
__decorateClass([
  command("githd.diffFile")
], CommandCenter.prototype, "diffFile", 1);
__decorateClass([
  command("githd.diffFolder")
], CommandCenter.prototype, "diffFolder", 1);
__decorateClass([
  command("githd.inputRef")
], CommandCenter.prototype, "inputRef", 1);
__decorateClass([
  command("githd.openCommit")
], CommandCenter.prototype, "openCommit", 1);
__decorateClass([
  command("githd.goBackHistoryView")
], CommandCenter.prototype, "goBackHistoryView", 1);
__decorateClass([
  command("githd.goForwardHistoryView")
], CommandCenter.prototype, "goForwardHistoryView", 1);
__decorateClass([
  command("githd.goBackFilesView")
], CommandCenter.prototype, "goBackFilesViewContext", 1);
__decorateClass([
  command("githd.goForwardFilesView")
], CommandCenter.prototype, "goForwardFilesViewContext", 1);
__decorateClass([
  command("githd.goBackNoMore")
], CommandCenter.prototype, "dummyForGoBackIcon", 1);
__decorateClass([
  command("githd.goForwardNoMore")
], CommandCenter.prototype, "dummyForGoForwardIcon", 1);
__decorateClass([
  command("githd.previousCommit")
], CommandCenter.prototype, "previousCommit", 1);
__decorateClass([
  command("githd.nextCommit")
], CommandCenter.prototype, "nextCommit", 1);
__decorateClass([
  command("githd.previousCommitNoMore")
], CommandCenter.prototype, "dummyForPreviousCommitIcon", 1);
__decorateClass([
  command("githd.nextCommitNoMore")
], CommandCenter.prototype, "dummyForNextCommitIcon", 1);
__decorateClass([
  command("githd.openCommittedFile")
], CommandCenter.prototype, "openCommittedFile", 1);
__decorateClass([
  command("githd.openCommitInfo")
], CommandCenter.prototype, "openCommitInfo", 1);
__decorateClass([
  command("githd.openLineDiff")
], CommandCenter.prototype, "openLineDiff", 1);
__decorateClass([
  command("githd.diffUncommittedFile")
], CommandCenter.prototype, "diffUncommittedFile", 1);
__decorateClass([
  command("githd.setExpressMode")
], CommandCenter.prototype, "setExpressMode", 1);
__decorateClass([
  command("githd.setRepository")
], CommandCenter.prototype, "setRepository", 1);
__decorateClass([
  command("githd.clearFilesView")
], CommandCenter.prototype, "clearFilesView", 1);
__decorateClass([
  command("githd.showStats")
], CommandCenter.prototype, "showStats", 1);

// src/historyViewProvider.ts
var vs10 = __toESM(require("vscode"));

// src/icons.ts
var path3 = __toESM(require("path"));
var vs9 = __toESM(require("vscode"));
var iconsRootPath;
function initializeIcons(context) {
  iconsRootPath = path3.join(context.extensionPath, "media", "icons");
}
function getIconUri(iconName, theme) {
  return vs9.Uri.file(path3.join(iconsRootPath, theme, `${iconName}.svg`));
}

// src/historyViewProvider.ts
var firstLoadingCount = 30;
var maxSingleLoadingCount = 2e3;
var maxPageSize = 1e4;
var stashTitleLabel = "Git Stashes";
var titleLabel = "Git History";
var moreLabel = "\xB7\xB7\xB7";
var separatorLabel = "--------------------------------------------------------------";
var loadingContent = " ";
var branchHoverMessage = new vs10.MarkdownString("Select a branch to see its history");
var authorHoverMessage = new vs10.MarkdownString("Select an author to see the commits");
var loadMoreHoverMessage = new vs10.MarkdownString("Load more commits");
var statsHoverMessage = new vs10.MarkdownString("Show stats chart");
function createTitleDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.title")
  });
}
function createBranchDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.branch")
  });
}
function createFileDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.filePath")
  });
}
function createSubjectDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.subject")
  });
}
function createHashDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.hash")
  });
}
function createSelectedHashDecoration() {
  return vs10.window.createTextEditorDecorationType({
    backgroundColor: new vs10.ThemeColor("merge.currentContentBackground"),
    isWholeLine: true,
    overviewRulerColor: "darkgreen",
    overviewRulerLane: vs10.OverviewRulerLane.Full
  });
}
function createRefDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.ref")
  });
}
function createAuthorDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.author")
  });
}
function createEmailDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.email")
  });
}
function createMoreDecoration() {
  return vs10.window.createTextEditorDecorationType({
    color: new vs10.ThemeColor("githd.historyView.more")
  });
}
function createLoadingDecoration() {
  return vs10.window.createTextEditorDecorationType({
    light: {
      after: {
        contentIconPath: getIconUri("loading", "light")
      }
    },
    dark: {
      after: {
        contentIconPath: getIconUri("loading", "dark")
      }
    }
  });
}
var _HistoryViewProvider = class _HistoryViewProvider {
  constructor(context, _model, _loader, _gitService, _panelView) {
    this._model = _model;
    this._loader = _loader;
    this._gitService = _gitService;
    this._panelView = _panelView;
    this._clickableProvider = new ClickableProvider(_HistoryViewProvider.scheme);
    this._commitsCount = 300;
    // the first page size
    this._content = "";
    this._loadedCount = 0;
    // for all loaded entries which includes multiple pages
    this._currentLine = 0;
    this._loadAll = false;
    this._leftCount = 0;
    // for a single page which includes multiple loadings
    this._totalCommitsCount = 0;
    this._onDidChange = new vs10.EventEmitter();
    this._logEntries = [];
    this._titleDecoration = createTitleDecoration();
    this._branchDecoration = createBranchDecoration();
    this._fileDecoration = createFileDecoration();
    this._subjectDecoration = createSubjectDecoration();
    this._hashDecoration = createHashDecoration();
    this._selectedHashDecoration = createSelectedHashDecoration();
    this._refDecoration = createRefDecoration();
    this._authorDecoration = createAuthorDecoration();
    this._emailDecoration = createEmailDecoration();
    this._moreDecoration = createMoreDecoration();
    this._loadingDecoration = createLoadingDecoration();
    this._titleDecorationOptions = [];
    this._subjectDecorationOptions = [];
    this._hashDecorationOptions = [];
    this._refDecorationOptions = [];
    this._authorDecorationOptions = [];
    this._emailDecorationOptions = [];
    this._repoStatusBar = vs10.window.createStatusBarItem(void 0, 1);
    this._expressStatusBar = vs10.window.createStatusBarItem(void 0, 2);
    this._express = false;
    this._updating = false;
    this._updatingCanceled = false;
    Tracer.info("Creating history view");
    context.subscriptions.push(vs10.workspace.registerTextDocumentContentProvider(_HistoryViewProvider.scheme, this));
    this._updatingPromise = new Promise((resolve) => this._updatingResolver = resolve);
    this._updatingResolver();
    this._expressStatusBar.command = "githd.setExpressMode";
    this._expressStatusBar.tooltip = "Turn on or off of the history view Express mode";
    this._repoStatusBar.command = "githd.setRepository";
    this._repoStatusBar.tooltip = "Change the git repository";
    this.express = this._model.configuration.expressMode;
    this.commitsCount = this._model.configuration.commitsCount;
    this._model.onDidChangeConfiguration(
      (config) => {
        this.commitsCount = config.commitsCount;
        this._updateExpressStatusBar();
      },
      null,
      context.subscriptions
    );
    this._model.onDidChangeHistoryViewContext(
      async (_) => {
        Tracer.verbose(`HistoryView: onDidChangeHistoryViewContext`);
        await this._cancelUpdating();
        const doc = await vs10.workspace.openTextDocument(_HistoryViewProvider.defaultUri);
        vs10.languages.setTextDocumentLanguage(doc, "githd");
        await vs10.window.showTextDocument(doc, {
          preview: false,
          preserveFocus: true
        });
        this._startLoading();
      },
      null,
      context.subscriptions
    );
    this._gitService.onDidChangeGitRepositories(
      (repos) => {
        this._updateExpressStatusBar();
      },
      null,
      context.subscriptions
    );
    this._gitService.onDidChangeCurrentGitRepo(
      (repo) => {
        var _a3;
        this._currentRepo = repo;
        this._repoStatusBar.text = "githd: Repository " + ((_a3 = this._currentRepo) == null ? void 0 : _a3.root);
      },
      null,
      context.subscriptions
    );
    vs10.window.onDidChangeActiveTextEditor(
      (editor) => {
        if ((editor == null ? void 0 : editor.document.uri.scheme) === _HistoryViewProvider.scheme) {
          Tracer.verbose("HistoryView: onDidChangeActiveTextEditor");
          this._show(editor);
        }
      },
      null,
      context.subscriptions
    );
    vs10.window.onDidChangeVisibleTextEditors(
      (editors) => {
        let visible = false;
        editors.forEach((editor) => {
          if ((editor == null ? void 0 : editor.document.uri.scheme) === _HistoryViewProvider.scheme) {
            Tracer.verbose("HistoryView: onDidChangeVisibleTextEditors");
            this._show(editor);
            visible = true;
          }
        });
        if (!visible) {
          this._repoStatusBar.hide();
        }
      },
      null,
      context.subscriptions
    );
    vs10.window.onDidChangeTextEditorVisibleRanges(
      async (e) => {
        if (e.textEditor.document.uri.scheme === _HistoryViewProvider.scheme) {
          if (!this._updating) {
            this._setShadowArea(e.textEditor);
          }
        }
      },
      this,
      context.subscriptions
    );
    vs10.workspace.onDidChangeTextDocument(
      (e) => {
        var _a3;
        if (e.document.uri.scheme === _HistoryViewProvider.scheme) {
          Tracer.verbose("HistoryView: onDidChangeTextDocument");
          if (this._content === loadingContent) {
            this._resetDecorations();
          }
          const editors = getTextEditors(_HistoryViewProvider.scheme);
          editors.forEach((editor) => this._setDecorations(editor));
          if (this._updating) {
            this._updateContent(false);
          } else {
            this._panelView.update();
            if (((_a3 = vs10.window.activeTextEditor) == null ? void 0 : _a3.document.uri.scheme) === _HistoryViewProvider.scheme) {
              this._setShadowArea(vs10.window.activeTextEditor);
            }
            this._updatingResolver();
          }
        }
      },
      null,
      context.subscriptions
    );
    this._updateExpressStatusBar();
    context.subscriptions.push(
      this._expressStatusBar,
      this._repoStatusBar,
      this._onDidChange,
      this._clickableProvider,
      this._titleDecoration,
      this._fileDecoration,
      this._subjectDecoration,
      this._hashDecoration,
      this._selectedHashDecoration,
      this._refDecoration,
      this._authorDecoration,
      this._emailDecoration,
      this._moreDecoration,
      this._branchDecoration,
      this._loadingDecoration
    );
    Tracer.info("History view created");
  }
  get onDidChange() {
    return this._onDidChange.event;
  }
  set loadAll(value) {
    this._loadAll = value;
  }
  get express() {
    return this._express;
  }
  set express(value) {
    this._express = value;
    this._expressStatusBar.text = "githd: Express " + (value ? "On" : "Off");
    vs10.commands.executeCommand("setContext", "githd.expressMode", value);
  }
  set commitsCount(count) {
    if ([100, 200, 300, 400, 500, 1e3].findIndex((a) => {
      return a === count;
    }) >= 0) {
      this._commitsCount = count;
    }
  }
  provideTextDocumentContent(uri) {
    Tracer.verbose(`HistoryView: provideTextDocumentContent length: ${this._content.length}`);
    return this._content;
  }
  _show(editor) {
    var _a3;
    if (!this._updating) {
      this._setDecorations(editor);
    }
    if ((_a3 = this._currentRepo) == null ? void 0 : _a3.root) {
      this._repoStatusBar.show();
    } else {
      this._repoStatusBar.hide();
    }
  }
  _updateExpressStatusBar() {
    if (this._model.configuration.displayExpress && this._gitService.getGitRepos().length > 0) {
      this._expressStatusBar.show();
    } else {
      this._expressStatusBar.hide();
    }
  }
  _update() {
    Tracer.info(`HistoryView: _update, content size ${this._content.length}, left count ${this._leftCount}`);
    if (this._updatingCanceled) {
      Tracer.info("HistoryView: exsiting updating canceled");
      this._updating = false;
    }
    this._onDidChange.fire(_HistoryViewProvider.defaultUri);
  }
  // When start showing the history view page, we do two phase loading for better
  // user experience. Firstly, it displays the first firstLoadingCount entries.
  // Then, it displays the left ones right after the first displaying.
  async _updateContent(loadMore) {
    var _a3;
    const context = this._model.historyViewContext;
    if (!context) {
      return;
    }
    Tracer.info(`HistoryView: _updateContent. ${JSON.stringify(context)}`);
    const timeRangeSet = context.startTime || context.endTime;
    const isStash = (_a3 = context.isStash) != null ? _a3 : false;
    const firstLoading = this._leftCount == 0;
    let loadingCount = this._express ? 3 * firstLoadingCount : firstLoadingCount;
    if (firstLoading) {
      if (loadMore) {
        this._content = this._content.substring(0, this._content.length - moreLabel.length - 1);
        this._content += separatorLabel + "\n\n";
        this._currentLine += 2;
      } else {
        this._content = "";
      }
      if (isStash || context.line) {
        loadingCount = 1e4;
      } else {
        if (this._totalCommitsCount == 0) {
          this._totalCommitsCount = await this._loader.getCommitsCount(
            context.repo,
            context.branch,
            context.specifiedPath,
            context.author,
            context.startTime,
            context.endTime
          );
        }
        let pageSize = Math.min(this._totalCommitsCount - this._loadedCount, this._commitsCount);
        if (this._loadAll || timeRangeSet) {
          pageSize = this._totalCommitsCount - this._loadedCount;
        } else if (loadMore) {
          pageSize = Math.min(2 * this._loadedCount, maxPageSize);
        }
        this._leftCount = Math.max(0, pageSize - loadingCount);
      }
    } else {
      loadingCount = Math.min(this._leftCount, maxSingleLoadingCount);
      this._leftCount = this._leftCount - loadingCount;
    }
    this._updating = this._leftCount > 0;
    Tracer.verbose(
      `HistoryView: left count ${this._leftCount}, current total count ${this._totalCommitsCount}, loading count ${loadingCount}, load more ${loadMore}`
    );
    const entries = await this._loader.getLogEntries(
      context.repo,
      this._express,
      this._loadedCount,
      loadingCount,
      context.branch,
      context.isStash,
      context.specifiedPath,
      context.line,
      context.author,
      context.startTime,
      context.endTime
    );
    this._panelView.addLogs(entries);
    if (entries.length === 0) {
      this._content = isStash ? "No Stash" : "No History";
      this._update();
      return;
    }
    let content = "";
    if (firstLoading && !loadMore) {
      content = await this._updateTitleInfo(context);
    }
    entries.forEach((entry) => {
      ++this._loadedCount;
      this._logEntries.push({ entry, lineNumber: this._currentLine });
      content += this._updateSubject(entry.subject, context.repo.remoteUrl);
      content += this._updateInfo(context, entry, isStash);
      content += this._updateStat(context, entry);
      content += "\n";
      ++this._currentLine;
    });
    if (!this._updating) {
      if (this._totalCommitsCount > this._loadedCount && !context.startTime && !context.endTime) {
        content += this._createClickableForMore();
      } else {
        this._moreClickableRange = void 0;
      }
    }
    this._content += content;
    this._update();
  }
  async _updateTitleInfo(context) {
    var _a3;
    let content = context.isStash ? stashTitleLabel : titleLabel;
    decorateWithoutWhitespace(this._titleDecorationOptions, content, 0, 0);
    if (context.isStash) {
      this._currentLine += 2;
      return content + " \n\n";
    }
    if (context.specifiedPath) {
      content += " of ";
      let start2 = content.length;
      content += await this._gitService.getGitRelativePath(context.specifiedPath);
      this._fileDecorationRange = new vs10.Range(this._currentLine, start2, this._currentLine, content.length);
      if (context.line) {
        content += " at line " + context.line;
      }
    }
    content += " on ";
    this._branchDecorationRange = new vs10.Range(0, content.length, 0, content.length + context.branch.length);
    this._clickableProvider.addClickable({
      range: this._branchDecorationRange,
      callback: () => vs10.commands.executeCommand("githd.viewBranchHistory", context),
      getHoverMessage: () => branchHoverMessage
    });
    content += context.branch;
    content += " by ";
    let author = context.author;
    if (!author) {
      content += "all ";
      author = "authors";
    }
    let start = content.length;
    content += author;
    let range = new vs10.Range(this._currentLine, start, this._currentLine, content.length);
    this._emailDecorationOptions.push(range);
    this._clickableProvider.addClickable({
      range,
      callback: () => vs10.commands.executeCommand("githd.viewAuthorHistory"),
      getHoverMessage: () => authorHoverMessage
    });
    if (context.repo.root != ((_a3 = this._currentRepo) == null ? void 0 : _a3.root)) {
      content += ` (${context.repo.root})`;
    }
    if (context.startTime && context.endTime) {
      content += ` (${context.startTime.toLocaleString()} - ${context.endTime.toLocaleString()})`;
      console.log(`string ${context.startTime.toString()}`);
      console.log(`localeString ${context.startTime.toLocaleString()}`);
      console.log(`timeString ${context.startTime.toTimeString()}`);
      console.log(`localeTimeString ${context.startTime.toLocaleTimeString()}`);
      console.log(`dateString ${context.startTime.toDateString()}`);
      console.log(`localeDateString ${context.startTime.toLocaleDateString()}`);
      console.log(`ISOString ${context.startTime.toISOString()}`);
    }
    const statsChart = "\u{1F4CA}";
    content += `	`;
    this._clickableProvider.addClickable({
      range: new vs10.Range(this._currentLine, content.length, this._currentLine, content.length + statsChart.length),
      callback: () => this._express ? vs10.window.showInformationMessage("Stats chart is only available when Express mode is disabled.") : vs10.commands.executeCommand("githd.showStats"),
      getHoverMessage: () => statsHoverMessage
    });
    content += statsChart;
    this._currentLine += 2;
    return content + " \n\n";
  }
  _updateSubject(subject, remoteUrl) {
    if (remoteUrl.indexOf("github.com") > 0) {
      getPullRequests(subject).forEach(([pr, start]) => {
        const url = remoteUrl + "/pull/" + pr.substring(1);
        this._clickableProvider.addClickable({
          range: new vs10.Range(this._currentLine, start, this._currentLine, start + pr.length),
          callback: () => vs10.env.openExternal(vs10.Uri.parse(url)),
          getHoverMessage: () => prHoverMessage
        });
      });
    }
    decorateWithoutWhitespace(this._subjectDecorationOptions, subject, this._currentLine, 0);
    ++this._currentLine;
    return subject + " \n";
  }
  _updateInfo(context, entry, isStash) {
    let info = entry.hash;
    let range = new vs10.Range(this._currentLine, 0, this._currentLine, info.length);
    this._hashDecorationOptions.push(range);
    this._clickableProvider.addClickable({
      range,
      callback: () => {
        this._model.setFilesViewContext({
          repo: context.repo,
          isStash,
          leftRef: void 0,
          rightRef: entry.hash,
          specifiedPath: context.specifiedPath,
          focusedLineInfo: entry.lineInfo
        });
      },
      clickedDecorationType: this._selectedHashDecoration,
      getHoverMessage: async () => {
        const markdown = new vs10.MarkdownString();
        const details = await this._gitService.getCommitDetails(context.repo, entry.hash, isStash);
        markdown.appendCodeblock(details, "txt");
        return markdown;
      }
    });
    if (entry.ref) {
      let start = info.length;
      info += entry.ref;
      decorateWithoutWhitespace(this._refDecorationOptions, entry.ref, this._currentLine, start);
    }
    if (entry.author) {
      info += " by ";
      let start = info.length;
      info += entry.author;
      decorateWithoutWhitespace(this._authorDecorationOptions, entry.author, this._currentLine, start);
    }
    if (entry.email) {
      info += " <";
      let start = info.length;
      info += entry.email;
      range = new vs10.Range(this._currentLine, start, this._currentLine, info.length);
      this._emailDecorationOptions.push(range);
      info += ">";
    }
    if (entry.date) {
      info += ", " + entry.date.toLocaleString();
    }
    if (!this._model.configuration.cacheEnabled && entry.relativeDate) {
      info += ` (${entry.relativeDate})`;
    }
    ++this._currentLine;
    return info + "\n";
  }
  _updateStat(context, entry) {
    if (!entry.stat || this._express) {
      return "";
    }
    let stat = entry.stat;
    if (context.specifiedPath) {
      stat = entry.stat.replace("1 file changed, ", "");
    }
    ++this._currentLine;
    return stat + "\n";
  }
  _createClickableForMore() {
    this._moreClickableRange = new vs10.Range(this._currentLine, 0, this._currentLine, moreLabel.length);
    this._clickableProvider.addClickable({
      range: this._moreClickableRange,
      callback: () => {
        if (this._moreClickableRange) {
          this._clickableProvider.removeClickable(this._moreClickableRange);
          this._moreClickableRange = void 0;
        }
        this._updateContent(true);
      },
      getHoverMessage: () => loadMoreHoverMessage
    });
    return moreLabel + " ";
  }
  _moveToTop(editor) {
    Tracer.verbose("HistoryView: _moveToTop");
    editor == null ? void 0 : editor.revealRange(new vs10.Range(0, 0, 0, 0), vs10.TextEditorRevealType.AtTop);
  }
  _setDecorations(editor) {
    if ((editor == null ? void 0 : editor.document.uri.scheme) !== _HistoryViewProvider.scheme) {
      Tracer.warning(`HistoryView: try to set decoration to wrong scheme: ${editor ? editor.document.uri.scheme : ""}`);
      return;
    }
    Tracer.verbose(
      `HistoryView: _setDecorations content length: ${this._content.length}, _subjectDecorationOptions size: ${this._subjectDecorationOptions.length}`
    );
    if (this._content === loadingContent) {
      Tracer.verbose("HistoryView: _loadingDecoration used");
      editor.setDecorations(this._loadingDecoration, [new vs10.Range(0, 0, 0, 1)]);
      return;
    }
    editor.selection = new vs10.Selection(0, 0, 0, 0);
    editor.setDecorations(this._loadingDecoration, []);
    editor.setDecorations(this._titleDecoration, this._titleDecorationOptions);
    editor.setDecorations(this._fileDecoration, this._fileDecorationRange ? [this._fileDecorationRange] : []);
    editor.setDecorations(this._branchDecoration, this._branchDecorationRange ? [this._branchDecorationRange] : []);
    editor.setDecorations(this._subjectDecoration, this._subjectDecorationOptions);
    editor.setDecorations(this._hashDecoration, this._hashDecorationOptions);
    editor.setDecorations(this._refDecoration, this._refDecorationOptions);
    editor.setDecorations(this._authorDecoration, this._authorDecorationOptions);
    editor.setDecorations(this._emailDecoration, this._emailDecorationOptions);
    editor.setDecorations(this._moreDecoration, this._moreClickableRange ? [this._moreClickableRange] : []);
  }
  _resetDecorations() {
    Tracer.verbose("HistoryView: _resetDecorations");
    this._clearDecorations();
    this._createDecorations();
  }
  _createDecorations() {
    this._titleDecoration = createTitleDecoration();
    this._fileDecoration = createFileDecoration();
    this._branchDecoration = createBranchDecoration();
    this._subjectDecoration = createSubjectDecoration();
    this._hashDecoration = createHashDecoration();
    this._selectedHashDecoration = createSelectedHashDecoration();
    this._refDecoration = createRefDecoration();
    this._authorDecoration = createAuthorDecoration();
    this._emailDecoration = createEmailDecoration();
    this._moreDecoration = createMoreDecoration();
    this._loadingDecoration = createLoadingDecoration();
  }
  _clearDecorations() {
    this._titleDecoration.dispose();
    this._fileDecoration.dispose();
    this._branchDecoration.dispose();
    this._subjectDecoration.dispose();
    this._hashDecoration.dispose();
    this._selectedHashDecoration.dispose();
    this._refDecoration.dispose();
    this._authorDecoration.dispose();
    this._emailDecoration.dispose();
    this._moreDecoration.dispose();
    this._loadingDecoration.dispose();
    this._clickableProvider.clear();
    this._moreClickableRange = void 0;
    this._titleDecorationOptions = [];
    this._fileDecorationRange = void 0;
    this._branchDecorationRange = void 0;
    this._subjectDecorationOptions = [];
    this._hashDecorationOptions = [];
    this._refDecorationOptions = [];
    this._authorDecorationOptions = [];
    this._emailDecorationOptions = [];
  }
  _reset() {
    Tracer.verbose("HistoryView: _reset");
    this._content = "";
    this._loadedCount = 0;
    this._leftCount = 0;
    this._currentLine = 0;
    this._totalCommitsCount = 0;
    this._panelView.clearLogs();
    this._logEntries = [];
  }
  _startLoading() {
    Tracer.verbose("HistoryView: _startLoading");
    this._reset();
    this._updating = true;
    this._content = loadingContent;
    this._update();
    this._updatingPromise = new Promise((resolve) => this._updatingResolver = resolve);
    getTextEditors(_HistoryViewProvider.scheme).forEach((editor) => this._moveToTop(editor));
  }
  async _cancelUpdating() {
    this._updatingCanceled = true;
    await this._updatingPromise;
    this._updatingCanceled = false;
    Tracer.verbose("HistoryView: updating canceled");
  }
  _setShadowArea(editor) {
    const visibleRange = editor.visibleRanges[0];
    if (!visibleRange) {
      return;
    }
    if (this._logEntries.length === 0) {
      return;
    }
    const startLine = visibleRange.start.line;
    const endLine = visibleRange.end.line;
    const startEntry = this._findLogEntryForLine(startLine, true);
    const endEntry = this._findLogEntryForLine(endLine, false);
    if (startEntry && endEntry) {
      this._panelView.setShadowArea(endEntry.timestamp, startEntry.timestamp);
    }
  }
  _findLogEntryForLine(line, start) {
    if (line < 3) {
      return this._logEntries[0].entry;
    }
    let low = 0;
    let high = this._logEntries.length - 1;
    while (low <= high) {
      const mid = Math.floor((low + high) / 2);
      const entry = this._logEntries[mid];
      if (entry.lineNumber === line) {
        return entry.entry;
      }
      if (entry.lineNumber < line) {
        low = mid + 1;
      } else {
        high = mid - 1;
      }
    }
    if (low >= this._logEntries.length) {
      low = this._logEntries.length - 1;
    }
    if (high < 0) {
      high = 0;
    }
    return start ? this._logEntries[high].entry : this._logEntries[low].entry;
  }
};
_HistoryViewProvider.scheme = "githd-logs";
_HistoryViewProvider.defaultUri = vs10.Uri.parse(_HistoryViewProvider.scheme + "://history//Git History");
var HistoryViewProvider = _HistoryViewProvider;

// src/explorerViewProvider.ts
var path4 = __toESM(require("path"));
var fs2 = __toESM(require("fs"));
var vs12 = __toESM(require("vscode"));

// src/resource.ts
var vs11 = __toESM(require("vscode"));
var Resource = class {
  static getGitStatusColor(status) {
    switch (status) {
      case "M":
        return;
      // majority of files are modified, we don't need to show color
      case "D":
        return new vs11.ThemeColor("gitDecoration.deletedResourceForeground");
      case "A":
        return new vs11.ThemeColor("gitDecoration.addedResourceForeground");
      case "C":
      case "R":
        return new vs11.ThemeColor("gitDecoration.renamedResourceForeground");
      default:
    }
    return;
  }
};

// src/explorerViewProvider.ts
var InfoItem = class extends vs12.TreeItem {
  constructor(label, description) {
    super(label);
    this.parent = void 0;
    this.contextValue = "infoFile";
    this.command = {
      title: "",
      command: "githd.openCommitInfo"
    };
    this.description = description;
    this.tooltip = description;
    this.iconPath = getIconUri("info", "");
  }
};
var LineDiffItem = class extends vs12.TreeItem {
  constructor(content) {
    super("Line diff");
    this.parent = void 0;
    this.command = {
      title: "",
      command: "githd.openLineDiff",
      arguments: [content]
    };
    this.iconPath = getIconUri("info", "");
  }
};
var _CommittedFileItem = class _CommittedFileItem extends vs12.TreeItem {
  constructor(parent, file, label, _description, _withStats) {
    super(label);
    this.parent = parent;
    this.file = file;
    this._description = _description;
    this._withStats = _withStats;
    this.contextValue = "file";
    this.command = {
      title: "",
      command: "githd.openCommittedFile",
      arguments: [this.file]
    };
    this.resourceUri = vs12.Uri.from({
      scheme: ExplorerViewProvider.scheme,
      authority: _CommittedFileItem.authority,
      path: "/" + file.gitRelativePath,
      query: `status=${file.status}`
    });
    if (file.stat) {
      const [count, changes] = file.stat.split(" ");
      let stat = count != null ? count : "0";
      if (changes) {
        stat += ` <span style="color:var(--vscode-terminal-ansiGreen);">${changes.substring(
          0,
          changes.lastIndexOf("+") + 1
        )}</span><span style="color:var(--vscode-terminal-ansiRed);">${changes.substring(
          changes.indexOf("-"),
          changes.lastIndexOf("-") + 1
        )}</span>`;
      }
      const markdown = new vs12.MarkdownString();
      markdown.isTrusted = true;
      markdown.appendMarkdown(stat);
      this.tooltip = markdown;
      this.withStats = _withStats;
    }
  }
  set withStats(value) {
    this._withStats = value;
    this.description = this._description;
    if (value) {
      this.description += "\xA0\u2022\xA0" + this.file.stat;
    }
  }
  updateDescription(description) {
    this._description = description;
    this.withStats = this._withStats;
  }
};
_CommittedFileItem.authority = "files";
var CommittedFileItem = _CommittedFileItem;
var _FolderItem = class _FolderItem extends vs12.TreeItem {
  constructor(context, _parent, _gitRelativePath, label, description, iconPath) {
    super(label);
    this._parent = _parent;
    this._gitRelativePath = _gitRelativePath;
    this._subFolders = [];
    this._files = [];
    this.parent = this._parent;
    this.gitRelativePath = this._gitRelativePath;
    this.resourceUri = vs12.Uri.from({
      scheme: ExplorerViewProvider.scheme,
      authority: _FolderItem.authority,
      path: "/" + this._gitRelativePath
    });
    this.contextValue = context;
    if (description) {
      this.description = description;
      this.tooltip = description;
    }
    this.iconPath = iconPath;
    this.collapsibleState = vs12.TreeItemCollapsibleState.Expanded;
  }
  get subFolders() {
    return this._subFolders;
  }
  set subFolders(value) {
    this._subFolders = value;
  }
  get files() {
    return this._files;
  }
  set files(value) {
    this._files = value;
  }
  get infoItem() {
    return this._infoItem;
  }
  set infoItem(value) {
    this._infoItem = value;
  }
};
_FolderItem.authority = "folders";
var FolderItem = _FolderItem;
function getFormattedLabelAndDesc(relativePath) {
  const name = path4.basename(relativePath);
  let dir = path4.dirname(relativePath);
  if (dir === ".") {
    dir = "";
  }
  return [name, "\xA0\u2022\xA0" + dir];
}
var _ExplorerViewProvider = class _ExplorerViewProvider {
  constructor(context, _model, _dataloader, _gitService) {
    this._model = _model;
    this._dataloader = _dataloader;
    this._gitService = _gitService;
    this._rootFolderIcon = {
      dark: getIconUri("structure", "dark"),
      light: getIconUri("structure", "light")
    };
    this._onDidChange = new vs12.EventEmitter();
    this._fileWithStats = false;
    this._treeRoot = [];
    this._rootCommitPosition = 0 /* Current */;
    this._building = false;
    this.onDidChangeTreeData = this._onDidChange.event;
    this._view = vs12.window.createTreeView("githd.files", { treeDataProvider: this });
    context.subscriptions.push(
      vs12.window.registerFileDecorationProvider(this),
      vs12.commands.registerCommand("githd.showFilesTreeView", (folder) => this._showFilesTreeView(folder)),
      vs12.commands.registerCommand("githd.showFilesListView", (folder) => this._showFilesListView(folder)),
      vs12.commands.registerCommand(
        "githd.collapseFolder",
        (folder) => this._setCollapsibleStateOnAll(folder, vs12.TreeItemCollapsibleState.Collapsed)
      ),
      vs12.commands.registerCommand(
        "githd.expandFolder",
        (folder) => this._setCollapsibleStateOnAll(folder, vs12.TreeItemCollapsibleState.Expanded)
      ),
      vs12.commands.registerCommand("githd.viewFileHistoryFromTree", (fileItem) => {
        if (!this._context) {
          Tracer.warning("ExplorerViewProvider: viewFileHistoryFromTree has empty context");
          return;
        }
        this._model.setHistoryViewContext({
          repo: this._context.repo,
          specifiedPath: fileItem.file.fileUri,
          branch: ""
        });
      }),
      vs12.commands.registerCommand("githd.viewFolderHistoryFromTree", (folder) => {
        if (!this._context) {
          Tracer.warning("ExplorerViewProvider: viewFolderHistoryFromTree has empty context");
          return;
        }
        this._model.setHistoryViewContext({
          repo: this._context.repo,
          specifiedPath: vs12.Uri.file(path4.join(this._context.repo.root, folder.gitRelativePath)),
          branch: ""
        });
      }),
      vs12.commands.registerCommand("githd.diffFileFromTreeView", (fileItem) => {
        var _a3;
        if (!((_a3 = this._context) == null ? void 0 : _a3.rightRef)) {
          Tracer.warning("ExplorerViewProvider: diffFileFromTreeView has empty commit ref");
          return;
        }
        vs12.commands.executeCommand("githd.diffFile", fileItem.file.fileUri, null, this._context.rightRef);
      }),
      vs12.commands.registerCommand("githd.diffFolderFromTreeView", (folder) => {
        var _a3;
        if (!((_a3 = this._context) == null ? void 0 : _a3.rightRef)) {
          Tracer.warning("ExplorerViewProvider: diffFolderFromTreeView has empty commit ref");
          return;
        }
        vs12.commands.executeCommand(
          "githd.diffFolder",
          vs12.Uri.file(path4.join(this._context.repo.root, folder.gitRelativePath)),
          null,
          this._context.rightRef
        );
      }),
      vs12.commands.registerCommand(
        "githd.diffCommitFromTreeView",
        (folder) => vs12.commands.executeCommand("githd.diffFolderFromTreeView", folder)
      ),
      vs12.commands.registerCommand("githd.openFile", (fileItem) => {
        const editor = vs12.window.activeTextEditor;
        vs12.commands.executeCommand("vscode.open", fileItem.file.fileUri).then(() => {
          var _a3;
          if ((editor == null ? void 0 : editor.document.uri.fsPath) === fileItem.file.fileUri.fsPath && ((_a3 = vs12.window.activeTextEditor) == null ? void 0 : _a3.document.uri.fsPath) === fileItem.file.fileUri.fsPath) {
            vs12.window.activeTextEditor.revealRange(editor.visibleRanges[0], vs12.TextEditorRevealType.InCenter);
            vs12.window.activeTextEditor.selection = editor.selection;
            vs12.window.activeTextEditor.selections = editor.selections;
          }
        });
      }),
      vs12.commands.registerCommand("githd.showFileStats", (folder) => this._setFileWithStats(folder, true)),
      vs12.commands.registerCommand("githd.hideFileStats", (folder) => this._setFileWithStats(folder, false)),
      vs12.commands.registerCommand(
        "githd.copyCommitHash",
        () => {
          var _a3, _b2;
          return vs12.env.clipboard.writeText((_b2 = (_a3 = this._context) == null ? void 0 : _a3.rightRef) != null ? _b2 : "");
        }
      ),
      this._onDidChange
    );
    this._model.onDidChangeFilesViewContext(
      (context2) => {
        if (JSON.stringify(this._context) === JSON.stringify(context2)) {
          return;
        }
        this._context = context2;
        this._update();
      },
      null,
      context.subscriptions
    );
    vs12.window.onDidChangeActiveTextEditor(async (editor) => {
      if (this._view.visible && this._treeRoot.length > 0 && editor) {
        const item = await this.findItemByPath(editor.document.uri);
        if (item) {
          this._view.reveal(item);
        }
      }
    });
    this._context = this._model.filesViewContext;
    this._withFolder = this._model.configuration.withFolder;
    this._update();
  }
  provideFileDecoration(uri) {
    if (uri.scheme !== _ExplorerViewProvider.scheme) {
      return;
    }
    const badge = uri.query.split("=")[1];
    if (uri.authority === CommittedFileItem.authority && badge) {
      return new vs12.FileDecoration(badge, void 0, Resource.getGitStatusColor(badge));
    }
  }
  getTreeItem(element) {
    return element;
  }
  async getChildren(element) {
    if (!element) {
      if (this._treeRoot.length === 0) {
        if (this._building) {
          return [];
        }
        this._building = true;
        await this._build();
        this._building = false;
      }
      return this._treeRoot;
    }
    let items = [];
    let folder = element;
    if (folder) {
      items = [...folder.subFolders, ...folder.files];
      if (folder.infoItem) {
        items.push(folder.infoItem);
      }
    }
    return items;
  }
  getParent(element) {
    return element.parent;
  }
  showPreviousCommit() {
    if (this._rootCommitPosition != 0 /* Current */) {
      Tracer.warning("showPreviousCommit: _rootCommitType should be RootCommitType.Current");
      return;
    }
    this._rootCommitPosition = 1 /* Previous */;
    this._treeRoot = [];
    this._onDidChange.fire(void 0);
  }
  showNextCommit() {
    if (this._rootCommitPosition != 0 /* Current */) {
      Tracer.warning("showNextCommit: _rootCommitType should be RootCommitType.Current");
      return;
    }
    this._rootCommitPosition = 2 /* Next */;
    this._treeRoot = [];
    this._onDidChange.fire(void 0);
  }
  get commitOrStashString() {
    var _a3;
    return ((_a3 = this._context) == null ? void 0 : _a3.isStash) ? "Stash" : "Commit";
  }
  _isCommitsView() {
    var _a3, _b2, _c;
    return !((_a3 = this._context) == null ? void 0 : _a3.isStash) && !!((_b2 = this._context) == null ? void 0 : _b2.rightRef) && !((_c = this._context) == null ? void 0 : _c.leftRef);
  }
  async _build() {
    if (this._rootCommitPosition === 0 /* Current */) {
      return this._buildCurrentCommit();
    }
    return this._buildNeighborCommit();
  }
  async _buildCurrentCommit() {
    if (!this._context) {
      vs12.commands.executeCommand("setContext", "githd.hasPreviousCommit", false);
      vs12.commands.executeCommand("setContext", "githd.hasNextCommit", false);
      return;
    }
    const leftRef = this._context.leftRef;
    const rightRef = this._context.rightRef;
    const specifiedPath = this._context.specifiedPath;
    const lineInfo = this._context.focusedLineInfo;
    if (!rightRef) {
      return;
    }
    Promise.resolve().then(async () => {
      var _a3;
      if (this._isCommitsView()) {
        vs12.commands.executeCommand("setContext", "githd.commitsView", true);
        const [hasPrevious, hasNext] = await this._dataloader.hasNeighborCommits((_a3 = this._context) == null ? void 0 : _a3.repo, rightRef);
        vs12.commands.executeCommand("setContext", "githd.hasPreviousCommit", hasPrevious);
        vs12.commands.executeCommand("setContext", "githd.hasNextCommit", hasNext);
      } else {
        vs12.commands.executeCommand("setContext", "githd.hasPreviousCommit", false);
        vs12.commands.executeCommand("setContext", "githd.hasNextCommit", false);
        vs12.commands.executeCommand("setContext", "githd.commitsView", false);
      }
    });
    const [stats, committedFiles] = await this._gitService.getCommittedFiles(
      this._context.repo,
      rightRef,
      leftRef,
      this._context.isStash
    );
    if (!leftRef) {
      await this._buildCommitInfo(stats);
    }
    if (!leftRef && !specifiedPath) {
      this._buildCommitTree(committedFiles, rightRef);
    } else if (leftRef && !specifiedPath) {
      this._buildDiffBranchTree(committedFiles, leftRef, rightRef);
    } else if (!leftRef && specifiedPath) {
      await this._buildPathSpecifiedCommitTree(committedFiles, specifiedPath, rightRef, lineInfo);
    } else {
      await this._buildPathSpecifiedDiffBranchTree(committedFiles, this._context);
    }
  }
  async _buildNeighborCommit() {
    var _a3;
    const position = this._rootCommitPosition;
    this._rootCommitPosition = 0 /* Current */;
    if (!((_a3 = this._context) == null ? void 0 : _a3.rightRef)) {
      return;
    }
    const repo = this._context.repo;
    const ref = this._context.rightRef;
    const commit = position === 1 /* Previous */ ? await this._dataloader.getPreviousCommit(repo, ref) : await this._dataloader.getNextCommit(repo, ref);
    if (commit) {
      this._context = { rightRef: commit, repo: this._context.repo };
      this._treeRoot = [];
      await this._buildCurrentCommit();
      this._model.setFilesViewContext(this._context);
    }
  }
  async _update() {
    this._treeRoot = [];
    this._rootCommitPosition = 0 /* Current */;
    this._onDidChange.fire(void 0);
  }
  async _buildCommitInfo(stats) {
    this._treeRoot.push(
      new InfoItem(`${this.commitOrStashString} Info`, stats.replace(/(\b(insertions?|deletions?)\b)/g, ""))
    );
  }
  _buildCommitTree(files, ref) {
    this._buildCommitFolder(`${this.commitOrStashString}`, ref, files);
  }
  _buildDiffBranchTree(files, leftRef, rightRef) {
    this._buildCommitFolder(`Diffs between ${leftRef} and ${rightRef}`, `(${files.length} files)`, files);
  }
  async _buildPathSpecifiedCommitTree(files, specifiedPath, ref, lineInfo) {
    await this._buildFocusFolder("Focus", files, specifiedPath, lineInfo);
    this._buildCommitTree(files, ref);
  }
  async _buildPathSpecifiedDiffBranchTree(files, context) {
    if (context.specifiedPath) {
      await this._buildFocusFolder(`${context.leftRef} .. ${context.rightRef}`, files, context.specifiedPath);
    }
  }
  _buildCommitFolder(label, description, committedFiles) {
    let folder = new FolderItem(
      "root-folder-" + (this._withFolder ? "tree" : "list"),
      void 0,
      "",
      label,
      description,
      this._rootFolderIcon
    );
    this._buildFileTree(folder, committedFiles);
    this._treeRoot.push(folder);
  }
  async _buildFocusFolder(label, committedFiles, specifiedPath, lineInfo) {
    var _a3;
    const relativePath = (_a3 = await this._gitService.getGitRelativePath(specifiedPath)) != null ? _a3 : "";
    let folder = new FolderItem(
      "focus-folder-" + (this._withFolder ? "tree" : "list"),
      void 0,
      "",
      label,
      relativePath,
      this._rootFolderIcon
    );
    if (fs2.lstatSync(specifiedPath.fsPath).isFile()) {
      if (lineInfo) {
        folder.infoItem = new LineDiffItem(lineInfo);
      }
      let file = committedFiles.find((value) => {
        return value.gitRelativePath === relativePath;
      });
      if (file) {
        folder.files.push(this._createCommittedFileItem(folder, file));
      }
    } else {
      let focus = [];
      committedFiles.forEach((file) => {
        if (relativePath && file.gitRelativePath.search(relativePath) === 0) {
          focus.push(file);
        }
      });
      this._buildFileTree(folder, focus);
    }
    if (folder.files.length + folder.subFolders.length > 0 || folder.infoItem) {
      this._treeRoot.push(folder);
    }
  }
  _showFilesTreeView(folder) {
    var _a3, _b2;
    folder.contextValue = (_a3 = folder.contextValue) == null ? void 0 : _a3.replace("list", "tree");
    this._buildFilesWithFolder(folder);
    this._onDidChange.fire(folder);
    if ((_b2 = folder.contextValue) == null ? void 0 : _b2.startsWith("root-folder")) {
      this._withFolder = true;
    }
  }
  _showFilesListView(folder) {
    var _a3, _b2;
    folder.contextValue = (_a3 = folder.contextValue) == null ? void 0 : _a3.replace("tree", "list");
    folder.subFolders.forEach((sub) => this._buildFilesWithoutFolder(folder, sub));
    folder.subFolders = [];
    this._onDidChange.fire(folder);
    if ((_b2 = folder.contextValue) == null ? void 0 : _b2.startsWith("root-folder")) {
      this._withFolder = false;
    }
  }
  async findItemByPath(file) {
    let activeFile;
    try {
      activeFile = await this._gitService.getGitRelativePath(file);
    } catch (e) {
      return;
    }
    if (activeFile) {
      let findRecursive2 = function(rootFolder) {
        var _a3, _b2, _c;
        const found = (_a3 = rootFolder == null ? void 0 : rootFolder.files) == null ? void 0 : _a3.find((file2) => file2.file.gitRelativePath == activeFile);
        if (found) {
          return found;
        }
        for (let i = 0; i < ((_c = (_b2 = rootFolder == null ? void 0 : rootFolder.subFolders) == null ? void 0 : _b2.length) != null ? _c : 0); i++) {
          const found2 = findRecursive2(rootFolder == null ? void 0 : rootFolder.subFolders[i]);
          if (found2) {
            return found2;
          }
        }
      };
      var findRecursive = findRecursive2;
      Tracer.verbose(`active file: ${activeFile}`);
      for (let i = 0; i < this._treeRoot.length; i++) {
        const found = findRecursive2(this._treeRoot[i]);
        if (found) {
          return found;
        }
      }
    }
  }
  _setCollapsibleState(folder, state) {
    if (folder) {
      folder.collapsibleState = state;
      folder.subFolders.forEach((sub) => this._setCollapsibleState(sub, state));
    }
  }
  _setCollapsibleStateOnAll(folder, state) {
    var _a3;
    if ((_a3 = folder.label) == null ? void 0 : _a3.toString().endsWith(" ")) {
      folder.label = folder.label.toString().slice(0, -1);
    } else {
      folder.label += " ";
    }
    this._setCollapsibleState(folder, state);
    this._onDidChange.fire(folder.parent);
  }
  _setFileWithStats(folder, value) {
    this._fileWithStats = value;
    vs12.commands.executeCommand("setContext", "githd.fileWithStats", value);
    const travesal = (folder2) => {
      folder2.files.forEach((file) => file.withStats = this._fileWithStats);
      folder2.subFolders.forEach((sub) => travesal(sub));
    };
    travesal(folder);
    this._onDidChange.fire(folder);
  }
  _createCommittedFileItem(rootFolder, file) {
    const [label, desc] = getFormattedLabelAndDesc(file.gitRelativePath);
    return new CommittedFileItem(rootFolder, file, label, desc, this._fileWithStats);
  }
  _buildOneFileWithFolder(rootFolder, file, relativePath = "") {
    const segments = relativePath ? path4.relative(relativePath, file.gitRelativePath).split(/\\|\//) : file.gitRelativePath.split("/");
    let gitRelativePath = relativePath;
    let parent = rootFolder;
    let i = 0;
    for (; i < segments.length - 1; ++i) {
      gitRelativePath += segments[i] + "/";
      let folder = parent.subFolders.find((item) => {
        return item.label === segments[i];
      });
      if (!folder) {
        folder = new FolderItem("folder-tree", parent, gitRelativePath, segments[i], "");
        parent.subFolders.push(folder);
      }
      parent = folder;
    }
    parent.files.push(new CommittedFileItem(parent, file, segments[i], "", this._fileWithStats));
  }
  _buildFileTree(rootFolder, files) {
    if (this._withFolder) {
      files.forEach((file) => this._buildOneFileWithFolder(rootFolder, file));
    } else {
      rootFolder.files.push(
        ...files.map((file) => {
          return this._createCommittedFileItem(rootFolder, file);
        })
      );
    }
  }
  _buildFilesWithoutFolder(rootFolder, folder) {
    rootFolder.files.push(
      ...folder.files.map((item) => {
        const [label, description] = getFormattedLabelAndDesc(
          path4.relative(rootFolder.gitRelativePath, item.file.gitRelativePath).replace(/\\/g, "/")
        );
        item.label = label;
        item.updateDescription(description);
        return item;
      })
    );
    folder.subFolders.forEach((f) => this._buildFilesWithoutFolder(rootFolder, f));
    folder.files = [];
    folder.subFolders = [];
  }
  _buildFilesWithFolder(rootFolder) {
    rootFolder.subFolders.forEach((folder) => this._buildFilesWithFolder(folder));
    const files = rootFolder.files;
    rootFolder.files = [];
    files.forEach((fileItem) => this._buildOneFileWithFolder(rootFolder, fileItem.file, rootFolder.gitRelativePath));
  }
};
_ExplorerViewProvider.scheme = "githd-committed";
var ExplorerViewProvider = _ExplorerViewProvider;

// src/blameViewProvider.ts
var vs13 = __toESM(require("vscode"));
var NotCommitted = `Not committed yet`;
var BlameViewStatProvider = class {
  constructor(_owner) {
    this._owner = _owner;
    this._disposables = [];
    this._disposables.push(vs13.languages.registerHoverProvider({ scheme: "file" }, this));
  }
  dispose() {
    vs13.Disposable.from(...this._disposables).dispose();
  }
  provideHover(document, position) {
    var _a3, _b2;
    if (!this._owner.isAvailable(document, position)) {
      return;
    }
    let markdown = new vs13.MarkdownString(
      `*<span style="color:var(--vscode-githd-infoView-content);">Committed Files</span>*\r
>\r
`
    );
    markdown.appendCodeblock((_b2 = (_a3 = this._owner.blame) == null ? void 0 : _a3.stat) != null ? _b2 : "", "typescript");
    markdown.appendMarkdown(">");
    markdown.isTrusted = true;
    return new vs13.Hover(markdown);
  }
};
var BlameViewProvider = class {
  constructor(context, model, _gitService) {
    this._gitService = _gitService;
    this._enabled = false;
    this._decoration = vs13.window.createTextEditorDecorationType({
      after: {
        color: new vs13.ThemeColor("githd.blameView.info"),
        fontStyle: "italic"
      }
    });
    this.enabled = model.configuration.blameEnabled;
    this._statProvider = new BlameViewStatProvider(this);
    this._debouncedUpdate = debounce((editor) => this._update(editor), 250);
    context.subscriptions.push(
      vs13.languages.registerHoverProvider({ scheme: "file" }, this),
      this._statProvider,
      this._decoration
    );
    vs13.window.onDidChangeTextEditorSelection(
      (e) => {
        this._onDidChangeSelection(e.textEditor);
      },
      null,
      context.subscriptions
    );
    vs13.window.onDidChangeActiveTextEditor(
      (editor) => {
        if (editor) {
          this._onDidChangeActiveTextEditor(editor);
        }
      },
      null,
      context.subscriptions
    );
    vs13.workspace.onDidChangeTextDocument(
      (e) => {
        this._onDidChangeTextDocument(e.document);
      },
      null,
      context.subscriptions
    );
    model.onDidChangeConfiguration(
      (config) => {
        this.enabled = config.blameEnabled;
      },
      null,
      context.subscriptions
    );
  }
  set enabled(value) {
    if (this._enabled !== value) {
      Tracer.info(`Blame view: set enabled ${value}`);
      this._enabled = value;
    }
  }
  get blame() {
    return this._blame;
  }
  provideHover(document, position) {
    if (!this.isAvailable(document, position)) {
      return;
    }
    const blame = this._blame;
    return new Promise(async (resolve) => {
      var _a3, _b2, _c;
      const repo = await this._gitService.getGitRepo(blame.file.fsPath);
      const ref = blame.hash;
      let args = encodeURIComponent(JSON.stringify([repo, ref, blame.file]));
      const commit = `*[${ref}](command:githd.openCommit?${args} "Click to see commit details")*`;
      args = encodeURIComponent(JSON.stringify([blame.file]));
      const file = `[*file*](command:githd.viewFileHistory?${args} "Click to see current file history")`;
      const line = `[*line*](command:githd.viewLineHistory?${args} "Click to see current line history")`;
      let subject = "";
      let lastPREnd = 0;
      getPullRequests((_a3 = blame.subject) != null ? _a3 : "").forEach(([pr, start]) => {
        var _a4;
        subject += ((_a4 = blame.subject) == null ? void 0 : _a4.substring(lastPREnd, start)) + `[*${pr}*](${repo == null ? void 0 : repo.remoteUrl}/pull/${pr.substring(1)})`;
        lastPREnd = start + pr.length;
      });
      subject += (_b2 = blame.subject) == null ? void 0 : _b2.substring(lastPREnd);
      const email = (_c = blame.email) == null ? void 0 : _c.replace("@", "\\@");
      Tracer.verbose(`Blame view: ${commit}`);
      const content = `
${commit}
*</span><span style="color:var(--vscode-githd-historyView-author);">${blame.author}</span>*
*<span style="color:var(--vscode-githd-historyView-email);">${email}</span>*
*(${blame.date})*
&ensp;
*(<span style="color:var(--vscode-githd-historyView-title);">history</span>: ${file} || ${line})*

### ${subject}

${blame.body}
>`;
      let markdown = new vs13.MarkdownString(content);
      markdown.isTrusted = true;
      return resolve(new vs13.Hover(markdown));
    });
  }
  isAvailable(doc, pos) {
    var _a3, _b2, _c;
    if (!this._enabled || isEmptyHash((_a3 = this._blame) == null ? void 0 : _a3.hash) || doc.isDirty || pos.line != ((_b2 = this._blame) == null ? void 0 : _b2.line) || pos.character < doc.lineAt(this._blame.line).range.end.character || doc.uri !== ((_c = this._blame) == null ? void 0 : _c.file)) {
      return false;
    }
    return true;
  }
  async _onDidChangeSelection(editor) {
    if (!editor) {
      Tracer.info("_onDidChangeSelection with null or undefined editor");
      return;
    }
    const file = editor.document.uri;
    if (!this._enabled || file.scheme !== "file" || editor.document.isDirty) {
      return;
    }
    Tracer.verbose("Blame view: onDidChangeSelection");
    const line = editor.selection.active.line;
    if (!this._blame || line != this._blame.line || file !== this._blame.file) {
      this._clear(editor);
      this._debouncedUpdate(editor);
    }
  }
  async _onDidChangeActiveTextEditor(editor) {
    if (!editor) {
      Tracer.info("_onDidChangeActiveTextEditor with null or undefined editor");
      return;
    }
    const file = editor.document.uri;
    if (!this._enabled || file.scheme !== "file" || editor.document.isDirty) {
      return;
    }
    Tracer.verbose("Blame view: onDidChangeActiveTextEditor");
    this._clear(editor);
    this._update(editor);
  }
  async _onDidChangeTextDocument(doc) {
    const editor = vs13.window.activeTextEditor;
    if (!this._enabled || doc.uri.scheme !== "file" || (editor == null ? void 0 : editor.document) !== doc) {
      return;
    }
    Tracer.verbose(`Blame view: onDidChange.TextDocument. isDirty ${doc.isDirty}`);
    this._clear(editor);
    if (!doc.isDirty) {
      this._update(editor);
    }
  }
  async _update(editor) {
    const file = editor.document.uri;
    const line = editor.selection.active.line;
    Tracer.verbose(` Try to update blame. ${file.fsPath}: ${line}`);
    this._blame = await this._gitService.getBlameItem(file, line);
    if (!this._blame) {
      return;
    }
    let contentText = "\xA0\xA0\xA0\xA0";
    if (isEmptyHash(this._blame.hash)) {
      contentText += NotCommitted;
    } else {
      contentText += `${this._blame.author} [${this._blame.relativeDate}]\xA0\u2022\xA0${this._blame.subject}`;
    }
    const options = {
      range: new vs13.Range(line, Number.MAX_SAFE_INTEGER, line, Number.MAX_SAFE_INTEGER),
      renderOptions: { after: { contentText } }
    };
    if (file !== editor.document.uri || line != editor.selection.active.line || editor.document.isDirty) {
      Tracer.info(`This update is outdated. ${file.fsPath}: ${line}, dirty ${editor.document.isDirty}`);
      this._blame = void 0;
      return;
    }
    editor.setDecorations(this._decoration, [options]);
  }
  _clear(editor) {
    this._blame = void 0;
    editor.setDecorations(this._decoration, []);
  }
};

// src/dataloader.ts
var path5 = __toESM(require("path"));
var vs14 = __toESM(require("vscode"));

// node_modules/lru-cache/dist/esm/index.js
var perf = typeof performance === "object" && performance && typeof performance.now === "function" ? performance : Date;
var warned = /* @__PURE__ */ new Set();
var PROCESS = typeof process === "object" && !!process ? process : {};
var emitWarning = (msg, type, code, fn) => {
  typeof PROCESS.emitWarning === "function" ? PROCESS.emitWarning(msg, type, code, fn) : console.error(`[${code}] ${type}: ${msg}`);
};
var AC = globalThis.AbortController;
var AS = globalThis.AbortSignal;
var _a;
if (typeof AC === "undefined") {
  AS = class AbortSignal {
    onabort;
    _onabort = [];
    reason;
    aborted = false;
    addEventListener(_, fn) {
      this._onabort.push(fn);
    }
  };
  AC = class AbortController {
    constructor() {
      warnACPolyfill();
    }
    signal = new AS();
    abort(reason) {
      var _a3, _b2;
      if (this.signal.aborted)
        return;
      this.signal.reason = reason;
      this.signal.aborted = true;
      for (const fn of this.signal._onabort) {
        fn(reason);
      }
      (_b2 = (_a3 = this.signal).onabort) == null ? void 0 : _b2.call(_a3, reason);
    }
  };
  let printACPolyfillWarning = ((_a = PROCESS.env) == null ? void 0 : _a.LRU_CACHE_IGNORE_AC_WARNING) !== "1";
  const warnACPolyfill = () => {
    if (!printACPolyfillWarning)
      return;
    printACPolyfillWarning = false;
    emitWarning("AbortController is not defined. If using lru-cache in node 14, load an AbortController polyfill from the `node-abort-controller` package. A minimal polyfill is provided for use by LRUCache.fetch(), but it should not be relied upon in other contexts (eg, passing it to other APIs that use AbortController/AbortSignal might have undesirable effects). You may disable this with LRU_CACHE_IGNORE_AC_WARNING=1 in the env.", "NO_ABORT_CONTROLLER", "ENOTSUP", warnACPolyfill);
  };
}
var shouldWarn = (code) => !warned.has(code);
var TYPE = Symbol("type");
var isPosInt = (n) => n && n === Math.floor(n) && n > 0 && isFinite(n);
var getUintArray = (max) => !isPosInt(max) ? null : max <= Math.pow(2, 8) ? Uint8Array : max <= Math.pow(2, 16) ? Uint16Array : max <= Math.pow(2, 32) ? Uint32Array : max <= Number.MAX_SAFE_INTEGER ? ZeroArray : null;
var ZeroArray = class extends Array {
  constructor(size) {
    super(size);
    this.fill(0);
  }
};
var Stack = class _Stack {
  heap;
  length;
  // private constructor
  static #constructing = false;
  static create(max) {
    const HeapCls = getUintArray(max);
    if (!HeapCls)
      return [];
    _Stack.#constructing = true;
    const s = new _Stack(max, HeapCls);
    _Stack.#constructing = false;
    return s;
  }
  constructor(max, HeapCls) {
    if (!_Stack.#constructing) {
      throw new TypeError("instantiate Stack using Stack.create(n)");
    }
    this.heap = new HeapCls(max);
    this.length = 0;
  }
  push(n) {
    this.heap[this.length++] = n;
  }
  pop() {
    return this.heap[--this.length];
  }
};
var _a2, _b, _max, _maxSize, _dispose, _disposeAfter, _fetchMethod, _memoMethod, _size, _calculatedSize, _keyMap, _keyList, _valList, _next, _prev, _head, _tail, _free, _disposed, _sizes, _starts, _ttls, _hasDispose, _hasFetchMethod, _hasDisposeAfter, _LRUCache_instances, initializeTTLTracking_fn, _updateItemAge, _statusTTL, _setItemTTL, _isStale, initializeSizeTracking_fn, _removeItemSize, _addItemSize, _requireSize, indexes_fn, rindexes_fn, isValidIndex_fn, evict_fn, backgroundFetch_fn, isBackgroundFetch_fn, connect_fn, moveToTail_fn, delete_fn, clear_fn;
var _LRUCache = class _LRUCache {
  constructor(options) {
    __privateAdd(this, _LRUCache_instances);
    // options that cannot be changed without disaster
    __privateAdd(this, _max);
    __privateAdd(this, _maxSize);
    __privateAdd(this, _dispose);
    __privateAdd(this, _disposeAfter);
    __privateAdd(this, _fetchMethod);
    __privateAdd(this, _memoMethod);
    /**
     * {@link LRUCache.OptionsBase.ttl}
     */
    __publicField(this, "ttl");
    /**
     * {@link LRUCache.OptionsBase.ttlResolution}
     */
    __publicField(this, "ttlResolution");
    /**
     * {@link LRUCache.OptionsBase.ttlAutopurge}
     */
    __publicField(this, "ttlAutopurge");
    /**
     * {@link LRUCache.OptionsBase.updateAgeOnGet}
     */
    __publicField(this, "updateAgeOnGet");
    /**
     * {@link LRUCache.OptionsBase.updateAgeOnHas}
     */
    __publicField(this, "updateAgeOnHas");
    /**
     * {@link LRUCache.OptionsBase.allowStale}
     */
    __publicField(this, "allowStale");
    /**
     * {@link LRUCache.OptionsBase.noDisposeOnSet}
     */
    __publicField(this, "noDisposeOnSet");
    /**
     * {@link LRUCache.OptionsBase.noUpdateTTL}
     */
    __publicField(this, "noUpdateTTL");
    /**
     * {@link LRUCache.OptionsBase.maxEntrySize}
     */
    __publicField(this, "maxEntrySize");
    /**
     * {@link LRUCache.OptionsBase.sizeCalculation}
     */
    __publicField(this, "sizeCalculation");
    /**
     * {@link LRUCache.OptionsBase.noDeleteOnFetchRejection}
     */
    __publicField(this, "noDeleteOnFetchRejection");
    /**
     * {@link LRUCache.OptionsBase.noDeleteOnStaleGet}
     */
    __publicField(this, "noDeleteOnStaleGet");
    /**
     * {@link LRUCache.OptionsBase.allowStaleOnFetchAbort}
     */
    __publicField(this, "allowStaleOnFetchAbort");
    /**
     * {@link LRUCache.OptionsBase.allowStaleOnFetchRejection}
     */
    __publicField(this, "allowStaleOnFetchRejection");
    /**
     * {@link LRUCache.OptionsBase.ignoreFetchAbort}
     */
    __publicField(this, "ignoreFetchAbort");
    // computed properties
    __privateAdd(this, _size);
    __privateAdd(this, _calculatedSize);
    __privateAdd(this, _keyMap);
    __privateAdd(this, _keyList);
    __privateAdd(this, _valList);
    __privateAdd(this, _next);
    __privateAdd(this, _prev);
    __privateAdd(this, _head);
    __privateAdd(this, _tail);
    __privateAdd(this, _free);
    __privateAdd(this, _disposed);
    __privateAdd(this, _sizes);
    __privateAdd(this, _starts);
    __privateAdd(this, _ttls);
    __privateAdd(this, _hasDispose);
    __privateAdd(this, _hasFetchMethod);
    __privateAdd(this, _hasDisposeAfter);
    // conditionally set private methods related to TTL
    __privateAdd(this, _updateItemAge, () => {
    });
    __privateAdd(this, _statusTTL, () => {
    });
    __privateAdd(this, _setItemTTL, () => {
    });
    /* c8 ignore stop */
    __privateAdd(this, _isStale, () => false);
    __privateAdd(this, _removeItemSize, (_i) => {
    });
    __privateAdd(this, _addItemSize, (_i, _s, _st) => {
    });
    __privateAdd(this, _requireSize, (_k, _v, size, sizeCalculation) => {
      if (size || sizeCalculation) {
        throw new TypeError("cannot set size without setting maxSize or maxEntrySize on cache");
      }
      return 0;
    });
    /**
     * A String value that is used in the creation of the default string
     * description of an object. Called by the built-in method
     * `Object.prototype.toString`.
     */
    __publicField(this, _a2, "LRUCache");
    const { max = 0, ttl, ttlResolution = 1, ttlAutopurge, updateAgeOnGet, updateAgeOnHas, allowStale, dispose, disposeAfter, noDisposeOnSet, noUpdateTTL, maxSize = 0, maxEntrySize = 0, sizeCalculation, fetchMethod, memoMethod, noDeleteOnFetchRejection, noDeleteOnStaleGet, allowStaleOnFetchRejection, allowStaleOnFetchAbort, ignoreFetchAbort } = options;
    if (max !== 0 && !isPosInt(max)) {
      throw new TypeError("max option must be a nonnegative integer");
    }
    const UintArray = max ? getUintArray(max) : Array;
    if (!UintArray) {
      throw new Error("invalid max value: " + max);
    }
    __privateSet(this, _max, max);
    __privateSet(this, _maxSize, maxSize);
    this.maxEntrySize = maxEntrySize || __privateGet(this, _maxSize);
    this.sizeCalculation = sizeCalculation;
    if (this.sizeCalculation) {
      if (!__privateGet(this, _maxSize) && !this.maxEntrySize) {
        throw new TypeError("cannot set sizeCalculation without setting maxSize or maxEntrySize");
      }
      if (typeof this.sizeCalculation !== "function") {
        throw new TypeError("sizeCalculation set to non-function");
      }
    }
    if (memoMethod !== void 0 && typeof memoMethod !== "function") {
      throw new TypeError("memoMethod must be a function if defined");
    }
    __privateSet(this, _memoMethod, memoMethod);
    if (fetchMethod !== void 0 && typeof fetchMethod !== "function") {
      throw new TypeError("fetchMethod must be a function if specified");
    }
    __privateSet(this, _fetchMethod, fetchMethod);
    __privateSet(this, _hasFetchMethod, !!fetchMethod);
    __privateSet(this, _keyMap, /* @__PURE__ */ new Map());
    __privateSet(this, _keyList, new Array(max).fill(void 0));
    __privateSet(this, _valList, new Array(max).fill(void 0));
    __privateSet(this, _next, new UintArray(max));
    __privateSet(this, _prev, new UintArray(max));
    __privateSet(this, _head, 0);
    __privateSet(this, _tail, 0);
    __privateSet(this, _free, Stack.create(max));
    __privateSet(this, _size, 0);
    __privateSet(this, _calculatedSize, 0);
    if (typeof dispose === "function") {
      __privateSet(this, _dispose, dispose);
    }
    if (typeof disposeAfter === "function") {
      __privateSet(this, _disposeAfter, disposeAfter);
      __privateSet(this, _disposed, []);
    } else {
      __privateSet(this, _disposeAfter, void 0);
      __privateSet(this, _disposed, void 0);
    }
    __privateSet(this, _hasDispose, !!__privateGet(this, _dispose));
    __privateSet(this, _hasDisposeAfter, !!__privateGet(this, _disposeAfter));
    this.noDisposeOnSet = !!noDisposeOnSet;
    this.noUpdateTTL = !!noUpdateTTL;
    this.noDeleteOnFetchRejection = !!noDeleteOnFetchRejection;
    this.allowStaleOnFetchRejection = !!allowStaleOnFetchRejection;
    this.allowStaleOnFetchAbort = !!allowStaleOnFetchAbort;
    this.ignoreFetchAbort = !!ignoreFetchAbort;
    if (this.maxEntrySize !== 0) {
      if (__privateGet(this, _maxSize) !== 0) {
        if (!isPosInt(__privateGet(this, _maxSize))) {
          throw new TypeError("maxSize must be a positive integer if specified");
        }
      }
      if (!isPosInt(this.maxEntrySize)) {
        throw new TypeError("maxEntrySize must be a positive integer if specified");
      }
      __privateMethod(this, _LRUCache_instances, initializeSizeTracking_fn).call(this);
    }
    this.allowStale = !!allowStale;
    this.noDeleteOnStaleGet = !!noDeleteOnStaleGet;
    this.updateAgeOnGet = !!updateAgeOnGet;
    this.updateAgeOnHas = !!updateAgeOnHas;
    this.ttlResolution = isPosInt(ttlResolution) || ttlResolution === 0 ? ttlResolution : 1;
    this.ttlAutopurge = !!ttlAutopurge;
    this.ttl = ttl || 0;
    if (this.ttl) {
      if (!isPosInt(this.ttl)) {
        throw new TypeError("ttl must be a positive integer if specified");
      }
      __privateMethod(this, _LRUCache_instances, initializeTTLTracking_fn).call(this);
    }
    if (__privateGet(this, _max) === 0 && this.ttl === 0 && __privateGet(this, _maxSize) === 0) {
      throw new TypeError("At least one of max, maxSize, or ttl is required");
    }
    if (!this.ttlAutopurge && !__privateGet(this, _max) && !__privateGet(this, _maxSize)) {
      const code = "LRU_CACHE_UNBOUNDED";
      if (shouldWarn(code)) {
        warned.add(code);
        const msg = "TTL caching without ttlAutopurge, max, or maxSize can result in unbounded memory consumption.";
        emitWarning(msg, "UnboundedCacheWarning", code, _LRUCache);
      }
    }
  }
  /**
   * Do not call this method unless you need to inspect the
   * inner workings of the cache.  If anything returned by this
   * object is modified in any way, strange breakage may occur.
   *
   * These fields are private for a reason!
   *
   * @internal
   */
  static unsafeExposeInternals(c) {
    return {
      // properties
      starts: __privateGet(c, _starts),
      ttls: __privateGet(c, _ttls),
      sizes: __privateGet(c, _sizes),
      keyMap: __privateGet(c, _keyMap),
      keyList: __privateGet(c, _keyList),
      valList: __privateGet(c, _valList),
      next: __privateGet(c, _next),
      prev: __privateGet(c, _prev),
      get head() {
        return __privateGet(c, _head);
      },
      get tail() {
        return __privateGet(c, _tail);
      },
      free: __privateGet(c, _free),
      // methods
      isBackgroundFetch: (p) => {
        var _a3;
        return __privateMethod(_a3 = c, _LRUCache_instances, isBackgroundFetch_fn).call(_a3, p);
      },
      backgroundFetch: (k, index, options, context) => {
        var _a3;
        return __privateMethod(_a3 = c, _LRUCache_instances, backgroundFetch_fn).call(_a3, k, index, options, context);
      },
      moveToTail: (index) => {
        var _a3;
        return __privateMethod(_a3 = c, _LRUCache_instances, moveToTail_fn).call(_a3, index);
      },
      indexes: (options) => {
        var _a3;
        return __privateMethod(_a3 = c, _LRUCache_instances, indexes_fn).call(_a3, options);
      },
      rindexes: (options) => {
        var _a3;
        return __privateMethod(_a3 = c, _LRUCache_instances, rindexes_fn).call(_a3, options);
      },
      isStale: (index) => {
        var _a3;
        return __privateGet(_a3 = c, _isStale).call(_a3, index);
      }
    };
  }
  // Protected read-only members
  /**
   * {@link LRUCache.OptionsBase.max} (read-only)
   */
  get max() {
    return __privateGet(this, _max);
  }
  /**
   * {@link LRUCache.OptionsBase.maxSize} (read-only)
   */
  get maxSize() {
    return __privateGet(this, _maxSize);
  }
  /**
   * The total computed size of items in the cache (read-only)
   */
  get calculatedSize() {
    return __privateGet(this, _calculatedSize);
  }
  /**
   * The number of items stored in the cache (read-only)
   */
  get size() {
    return __privateGet(this, _size);
  }
  /**
   * {@link LRUCache.OptionsBase.fetchMethod} (read-only)
   */
  get fetchMethod() {
    return __privateGet(this, _fetchMethod);
  }
  get memoMethod() {
    return __privateGet(this, _memoMethod);
  }
  /**
   * {@link LRUCache.OptionsBase.dispose} (read-only)
   */
  get dispose() {
    return __privateGet(this, _dispose);
  }
  /**
   * {@link LRUCache.OptionsBase.disposeAfter} (read-only)
   */
  get disposeAfter() {
    return __privateGet(this, _disposeAfter);
  }
  /**
   * Return the number of ms left in the item's TTL. If item is not in cache,
   * returns `0`. Returns `Infinity` if item is in cache without a defined TTL.
   */
  getRemainingTTL(key) {
    return __privateGet(this, _keyMap).has(key) ? Infinity : 0;
  }
  /**
   * Return a generator yielding `[key, value]` pairs,
   * in order from most recently used to least recently used.
   */
  *entries() {
    for (const i of __privateMethod(this, _LRUCache_instances, indexes_fn).call(this)) {
      if (__privateGet(this, _valList)[i] !== void 0 && __privateGet(this, _keyList)[i] !== void 0 && !__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, __privateGet(this, _valList)[i])) {
        yield [__privateGet(this, _keyList)[i], __privateGet(this, _valList)[i]];
      }
    }
  }
  /**
   * Inverse order version of {@link LRUCache.entries}
   *
   * Return a generator yielding `[key, value]` pairs,
   * in order from least recently used to most recently used.
   */
  *rentries() {
    for (const i of __privateMethod(this, _LRUCache_instances, rindexes_fn).call(this)) {
      if (__privateGet(this, _valList)[i] !== void 0 && __privateGet(this, _keyList)[i] !== void 0 && !__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, __privateGet(this, _valList)[i])) {
        yield [__privateGet(this, _keyList)[i], __privateGet(this, _valList)[i]];
      }
    }
  }
  /**
   * Return a generator yielding the keys in the cache,
   * in order from most recently used to least recently used.
   */
  *keys() {
    for (const i of __privateMethod(this, _LRUCache_instances, indexes_fn).call(this)) {
      const k = __privateGet(this, _keyList)[i];
      if (k !== void 0 && !__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, __privateGet(this, _valList)[i])) {
        yield k;
      }
    }
  }
  /**
   * Inverse order version of {@link LRUCache.keys}
   *
   * Return a generator yielding the keys in the cache,
   * in order from least recently used to most recently used.
   */
  *rkeys() {
    for (const i of __privateMethod(this, _LRUCache_instances, rindexes_fn).call(this)) {
      const k = __privateGet(this, _keyList)[i];
      if (k !== void 0 && !__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, __privateGet(this, _valList)[i])) {
        yield k;
      }
    }
  }
  /**
   * Return a generator yielding the values in the cache,
   * in order from most recently used to least recently used.
   */
  *values() {
    for (const i of __privateMethod(this, _LRUCache_instances, indexes_fn).call(this)) {
      const v = __privateGet(this, _valList)[i];
      if (v !== void 0 && !__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, __privateGet(this, _valList)[i])) {
        yield __privateGet(this, _valList)[i];
      }
    }
  }
  /**
   * Inverse order version of {@link LRUCache.values}
   *
   * Return a generator yielding the values in the cache,
   * in order from least recently used to most recently used.
   */
  *rvalues() {
    for (const i of __privateMethod(this, _LRUCache_instances, rindexes_fn).call(this)) {
      const v = __privateGet(this, _valList)[i];
      if (v !== void 0 && !__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, __privateGet(this, _valList)[i])) {
        yield __privateGet(this, _valList)[i];
      }
    }
  }
  /**
   * Iterating over the cache itself yields the same results as
   * {@link LRUCache.entries}
   */
  [(_b = Symbol.iterator, _a2 = Symbol.toStringTag, _b)]() {
    return this.entries();
  }
  /**
   * Find a value for which the supplied fn method returns a truthy value,
   * similar to `Array.find()`. fn is called as `fn(value, key, cache)`.
   */
  find(fn, getOptions = {}) {
    for (const i of __privateMethod(this, _LRUCache_instances, indexes_fn).call(this)) {
      const v = __privateGet(this, _valList)[i];
      const value = __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v) ? v.__staleWhileFetching : v;
      if (value === void 0)
        continue;
      if (fn(value, __privateGet(this, _keyList)[i], this)) {
        return this.get(__privateGet(this, _keyList)[i], getOptions);
      }
    }
  }
  /**
   * Call the supplied function on each item in the cache, in order from most
   * recently used to least recently used.
   *
   * `fn` is called as `fn(value, key, cache)`.
   *
   * If `thisp` is provided, function will be called in the `this`-context of
   * the provided object, or the cache if no `thisp` object is provided.
   *
   * Does not update age or recenty of use, or iterate over stale values.
   */
  forEach(fn, thisp = this) {
    for (const i of __privateMethod(this, _LRUCache_instances, indexes_fn).call(this)) {
      const v = __privateGet(this, _valList)[i];
      const value = __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v) ? v.__staleWhileFetching : v;
      if (value === void 0)
        continue;
      fn.call(thisp, value, __privateGet(this, _keyList)[i], this);
    }
  }
  /**
   * The same as {@link LRUCache.forEach} but items are iterated over in
   * reverse order.  (ie, less recently used items are iterated over first.)
   */
  rforEach(fn, thisp = this) {
    for (const i of __privateMethod(this, _LRUCache_instances, rindexes_fn).call(this)) {
      const v = __privateGet(this, _valList)[i];
      const value = __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v) ? v.__staleWhileFetching : v;
      if (value === void 0)
        continue;
      fn.call(thisp, value, __privateGet(this, _keyList)[i], this);
    }
  }
  /**
   * Delete any stale entries. Returns true if anything was removed,
   * false otherwise.
   */
  purgeStale() {
    let deleted = false;
    for (const i of __privateMethod(this, _LRUCache_instances, rindexes_fn).call(this, { allowStale: true })) {
      if (__privateGet(this, _isStale).call(this, i)) {
        __privateMethod(this, _LRUCache_instances, delete_fn).call(this, __privateGet(this, _keyList)[i], "expire");
        deleted = true;
      }
    }
    return deleted;
  }
  /**
   * Get the extended info about a given entry, to get its value, size, and
   * TTL info simultaneously. Returns `undefined` if the key is not present.
   *
   * Unlike {@link LRUCache#dump}, which is designed to be portable and survive
   * serialization, the `start` value is always the current timestamp, and the
   * `ttl` is a calculated remaining time to live (negative if expired).
   *
   * Always returns stale values, if their info is found in the cache, so be
   * sure to check for expirations (ie, a negative {@link LRUCache.Entry#ttl})
   * if relevant.
   */
  info(key) {
    const i = __privateGet(this, _keyMap).get(key);
    if (i === void 0)
      return void 0;
    const v = __privateGet(this, _valList)[i];
    const value = __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v) ? v.__staleWhileFetching : v;
    if (value === void 0)
      return void 0;
    const entry = { value };
    if (__privateGet(this, _ttls) && __privateGet(this, _starts)) {
      const ttl = __privateGet(this, _ttls)[i];
      const start = __privateGet(this, _starts)[i];
      if (ttl && start) {
        const remain = ttl - (perf.now() - start);
        entry.ttl = remain;
        entry.start = Date.now();
      }
    }
    if (__privateGet(this, _sizes)) {
      entry.size = __privateGet(this, _sizes)[i];
    }
    return entry;
  }
  /**
   * Return an array of [key, {@link LRUCache.Entry}] tuples which can be
   * passed to {@link LRLUCache#load}.
   *
   * The `start` fields are calculated relative to a portable `Date.now()`
   * timestamp, even if `performance.now()` is available.
   *
   * Stale entries are always included in the `dump`, even if
   * {@link LRUCache.OptionsBase.allowStale} is false.
   *
   * Note: this returns an actual array, not a generator, so it can be more
   * easily passed around.
   */
  dump() {
    const arr = [];
    for (const i of __privateMethod(this, _LRUCache_instances, indexes_fn).call(this, { allowStale: true })) {
      const key = __privateGet(this, _keyList)[i];
      const v = __privateGet(this, _valList)[i];
      const value = __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v) ? v.__staleWhileFetching : v;
      if (value === void 0 || key === void 0)
        continue;
      const entry = { value };
      if (__privateGet(this, _ttls) && __privateGet(this, _starts)) {
        entry.ttl = __privateGet(this, _ttls)[i];
        const age = perf.now() - __privateGet(this, _starts)[i];
        entry.start = Math.floor(Date.now() - age);
      }
      if (__privateGet(this, _sizes)) {
        entry.size = __privateGet(this, _sizes)[i];
      }
      arr.unshift([key, entry]);
    }
    return arr;
  }
  /**
   * Reset the cache and load in the items in entries in the order listed.
   *
   * The shape of the resulting cache may be different if the same options are
   * not used in both caches.
   *
   * The `start` fields are assumed to be calculated relative to a portable
   * `Date.now()` timestamp, even if `performance.now()` is available.
   */
  load(arr) {
    this.clear();
    for (const [key, entry] of arr) {
      if (entry.start) {
        const age = Date.now() - entry.start;
        entry.start = perf.now() - age;
      }
      this.set(key, entry.value, entry);
    }
  }
  /**
   * Add a value to the cache.
   *
   * Note: if `undefined` is specified as a value, this is an alias for
   * {@link LRUCache#delete}
   *
   * Fields on the {@link LRUCache.SetOptions} options param will override
   * their corresponding values in the constructor options for the scope
   * of this single `set()` operation.
   *
   * If `start` is provided, then that will set the effective start
   * time for the TTL calculation. Note that this must be a previous
   * value of `performance.now()` if supported, or a previous value of
   * `Date.now()` if not.
   *
   * Options object may also include `size`, which will prevent
   * calling the `sizeCalculation` function and just use the specified
   * number if it is a positive integer, and `noDisposeOnSet` which
   * will prevent calling a `dispose` function in the case of
   * overwrites.
   *
   * If the `size` (or return value of `sizeCalculation`) for a given
   * entry is greater than `maxEntrySize`, then the item will not be
   * added to the cache.
   *
   * Will update the recency of the entry.
   *
   * If the value is `undefined`, then this is an alias for
   * `cache.delete(key)`. `undefined` is never stored in the cache.
   */
  set(k, v, setOptions = {}) {
    var _a3, _b2, _c, _d, _e;
    if (v === void 0) {
      this.delete(k);
      return this;
    }
    const { ttl = this.ttl, start, noDisposeOnSet = this.noDisposeOnSet, sizeCalculation = this.sizeCalculation, status } = setOptions;
    let { noUpdateTTL = this.noUpdateTTL } = setOptions;
    const size = __privateGet(this, _requireSize).call(this, k, v, setOptions.size || 0, sizeCalculation);
    if (this.maxEntrySize && size > this.maxEntrySize) {
      if (status) {
        status.set = "miss";
        status.maxEntrySizeExceeded = true;
      }
      __privateMethod(this, _LRUCache_instances, delete_fn).call(this, k, "set");
      return this;
    }
    let index = __privateGet(this, _size) === 0 ? void 0 : __privateGet(this, _keyMap).get(k);
    if (index === void 0) {
      index = __privateGet(this, _size) === 0 ? __privateGet(this, _tail) : __privateGet(this, _free).length !== 0 ? __privateGet(this, _free).pop() : __privateGet(this, _size) === __privateGet(this, _max) ? __privateMethod(this, _LRUCache_instances, evict_fn).call(this, false) : __privateGet(this, _size);
      __privateGet(this, _keyList)[index] = k;
      __privateGet(this, _valList)[index] = v;
      __privateGet(this, _keyMap).set(k, index);
      __privateGet(this, _next)[__privateGet(this, _tail)] = index;
      __privateGet(this, _prev)[index] = __privateGet(this, _tail);
      __privateSet(this, _tail, index);
      __privateWrapper(this, _size)._++;
      __privateGet(this, _addItemSize).call(this, index, size, status);
      if (status)
        status.set = "add";
      noUpdateTTL = false;
    } else {
      __privateMethod(this, _LRUCache_instances, moveToTail_fn).call(this, index);
      const oldVal = __privateGet(this, _valList)[index];
      if (v !== oldVal) {
        if (__privateGet(this, _hasFetchMethod) && __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, oldVal)) {
          oldVal.__abortController.abort(new Error("replaced"));
          const { __staleWhileFetching: s } = oldVal;
          if (s !== void 0 && !noDisposeOnSet) {
            if (__privateGet(this, _hasDispose)) {
              (_a3 = __privateGet(this, _dispose)) == null ? void 0 : _a3.call(this, s, k, "set");
            }
            if (__privateGet(this, _hasDisposeAfter)) {
              (_b2 = __privateGet(this, _disposed)) == null ? void 0 : _b2.push([s, k, "set"]);
            }
          }
        } else if (!noDisposeOnSet) {
          if (__privateGet(this, _hasDispose)) {
            (_c = __privateGet(this, _dispose)) == null ? void 0 : _c.call(this, oldVal, k, "set");
          }
          if (__privateGet(this, _hasDisposeAfter)) {
            (_d = __privateGet(this, _disposed)) == null ? void 0 : _d.push([oldVal, k, "set"]);
          }
        }
        __privateGet(this, _removeItemSize).call(this, index);
        __privateGet(this, _addItemSize).call(this, index, size, status);
        __privateGet(this, _valList)[index] = v;
        if (status) {
          status.set = "replace";
          const oldValue = oldVal && __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, oldVal) ? oldVal.__staleWhileFetching : oldVal;
          if (oldValue !== void 0)
            status.oldValue = oldValue;
        }
      } else if (status) {
        status.set = "update";
      }
    }
    if (ttl !== 0 && !__privateGet(this, _ttls)) {
      __privateMethod(this, _LRUCache_instances, initializeTTLTracking_fn).call(this);
    }
    if (__privateGet(this, _ttls)) {
      if (!noUpdateTTL) {
        __privateGet(this, _setItemTTL).call(this, index, ttl, start);
      }
      if (status)
        __privateGet(this, _statusTTL).call(this, status, index);
    }
    if (!noDisposeOnSet && __privateGet(this, _hasDisposeAfter) && __privateGet(this, _disposed)) {
      const dt = __privateGet(this, _disposed);
      let task;
      while (task = dt == null ? void 0 : dt.shift()) {
        (_e = __privateGet(this, _disposeAfter)) == null ? void 0 : _e.call(this, ...task);
      }
    }
    return this;
  }
  /**
   * Evict the least recently used item, returning its value or
   * `undefined` if cache is empty.
   */
  pop() {
    var _a3;
    try {
      while (__privateGet(this, _size)) {
        const val = __privateGet(this, _valList)[__privateGet(this, _head)];
        __privateMethod(this, _LRUCache_instances, evict_fn).call(this, true);
        if (__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, val)) {
          if (val.__staleWhileFetching) {
            return val.__staleWhileFetching;
          }
        } else if (val !== void 0) {
          return val;
        }
      }
    } finally {
      if (__privateGet(this, _hasDisposeAfter) && __privateGet(this, _disposed)) {
        const dt = __privateGet(this, _disposed);
        let task;
        while (task = dt == null ? void 0 : dt.shift()) {
          (_a3 = __privateGet(this, _disposeAfter)) == null ? void 0 : _a3.call(this, ...task);
        }
      }
    }
  }
  /**
   * Check if a key is in the cache, without updating the recency of use.
   * Will return false if the item is stale, even though it is technically
   * in the cache.
   *
   * Check if a key is in the cache, without updating the recency of
   * use. Age is updated if {@link LRUCache.OptionsBase.updateAgeOnHas} is set
   * to `true` in either the options or the constructor.
   *
   * Will return `false` if the item is stale, even though it is technically in
   * the cache. The difference can be determined (if it matters) by using a
   * `status` argument, and inspecting the `has` field.
   *
   * Will not update item age unless
   * {@link LRUCache.OptionsBase.updateAgeOnHas} is set.
   */
  has(k, hasOptions = {}) {
    const { updateAgeOnHas = this.updateAgeOnHas, status } = hasOptions;
    const index = __privateGet(this, _keyMap).get(k);
    if (index !== void 0) {
      const v = __privateGet(this, _valList)[index];
      if (__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v) && v.__staleWhileFetching === void 0) {
        return false;
      }
      if (!__privateGet(this, _isStale).call(this, index)) {
        if (updateAgeOnHas) {
          __privateGet(this, _updateItemAge).call(this, index);
        }
        if (status) {
          status.has = "hit";
          __privateGet(this, _statusTTL).call(this, status, index);
        }
        return true;
      } else if (status) {
        status.has = "stale";
        __privateGet(this, _statusTTL).call(this, status, index);
      }
    } else if (status) {
      status.has = "miss";
    }
    return false;
  }
  /**
   * Like {@link LRUCache#get} but doesn't update recency or delete stale
   * items.
   *
   * Returns `undefined` if the item is stale, unless
   * {@link LRUCache.OptionsBase.allowStale} is set.
   */
  peek(k, peekOptions = {}) {
    const { allowStale = this.allowStale } = peekOptions;
    const index = __privateGet(this, _keyMap).get(k);
    if (index === void 0 || !allowStale && __privateGet(this, _isStale).call(this, index)) {
      return;
    }
    const v = __privateGet(this, _valList)[index];
    return __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v) ? v.__staleWhileFetching : v;
  }
  async fetch(k, fetchOptions = {}) {
    const {
      // get options
      allowStale = this.allowStale,
      updateAgeOnGet = this.updateAgeOnGet,
      noDeleteOnStaleGet = this.noDeleteOnStaleGet,
      // set options
      ttl = this.ttl,
      noDisposeOnSet = this.noDisposeOnSet,
      size = 0,
      sizeCalculation = this.sizeCalculation,
      noUpdateTTL = this.noUpdateTTL,
      // fetch exclusive options
      noDeleteOnFetchRejection = this.noDeleteOnFetchRejection,
      allowStaleOnFetchRejection = this.allowStaleOnFetchRejection,
      ignoreFetchAbort = this.ignoreFetchAbort,
      allowStaleOnFetchAbort = this.allowStaleOnFetchAbort,
      context,
      forceRefresh = false,
      status,
      signal
    } = fetchOptions;
    if (!__privateGet(this, _hasFetchMethod)) {
      if (status)
        status.fetch = "get";
      return this.get(k, {
        allowStale,
        updateAgeOnGet,
        noDeleteOnStaleGet,
        status
      });
    }
    const options = {
      allowStale,
      updateAgeOnGet,
      noDeleteOnStaleGet,
      ttl,
      noDisposeOnSet,
      size,
      sizeCalculation,
      noUpdateTTL,
      noDeleteOnFetchRejection,
      allowStaleOnFetchRejection,
      allowStaleOnFetchAbort,
      ignoreFetchAbort,
      status,
      signal
    };
    let index = __privateGet(this, _keyMap).get(k);
    if (index === void 0) {
      if (status)
        status.fetch = "miss";
      const p = __privateMethod(this, _LRUCache_instances, backgroundFetch_fn).call(this, k, index, options, context);
      return p.__returned = p;
    } else {
      const v = __privateGet(this, _valList)[index];
      if (__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v)) {
        const stale = allowStale && v.__staleWhileFetching !== void 0;
        if (status) {
          status.fetch = "inflight";
          if (stale)
            status.returnedStale = true;
        }
        return stale ? v.__staleWhileFetching : v.__returned = v;
      }
      const isStale = __privateGet(this, _isStale).call(this, index);
      if (!forceRefresh && !isStale) {
        if (status)
          status.fetch = "hit";
        __privateMethod(this, _LRUCache_instances, moveToTail_fn).call(this, index);
        if (updateAgeOnGet) {
          __privateGet(this, _updateItemAge).call(this, index);
        }
        if (status)
          __privateGet(this, _statusTTL).call(this, status, index);
        return v;
      }
      const p = __privateMethod(this, _LRUCache_instances, backgroundFetch_fn).call(this, k, index, options, context);
      const hasStale = p.__staleWhileFetching !== void 0;
      const staleVal = hasStale && allowStale;
      if (status) {
        status.fetch = isStale ? "stale" : "refresh";
        if (staleVal && isStale)
          status.returnedStale = true;
      }
      return staleVal ? p.__staleWhileFetching : p.__returned = p;
    }
  }
  async forceFetch(k, fetchOptions = {}) {
    const v = await this.fetch(k, fetchOptions);
    if (v === void 0)
      throw new Error("fetch() returned undefined");
    return v;
  }
  memo(k, memoOptions = {}) {
    const memoMethod = __privateGet(this, _memoMethod);
    if (!memoMethod) {
      throw new Error("no memoMethod provided to constructor");
    }
    const { context, forceRefresh, ...options } = memoOptions;
    const v = this.get(k, options);
    if (!forceRefresh && v !== void 0)
      return v;
    const vv = memoMethod(k, v, {
      options,
      context
    });
    this.set(k, vv, options);
    return vv;
  }
  /**
   * Return a value from the cache. Will update the recency of the cache
   * entry found.
   *
   * If the key is not found, get() will return `undefined`.
   */
  get(k, getOptions = {}) {
    const { allowStale = this.allowStale, updateAgeOnGet = this.updateAgeOnGet, noDeleteOnStaleGet = this.noDeleteOnStaleGet, status } = getOptions;
    const index = __privateGet(this, _keyMap).get(k);
    if (index !== void 0) {
      const value = __privateGet(this, _valList)[index];
      const fetching = __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, value);
      if (status)
        __privateGet(this, _statusTTL).call(this, status, index);
      if (__privateGet(this, _isStale).call(this, index)) {
        if (status)
          status.get = "stale";
        if (!fetching) {
          if (!noDeleteOnStaleGet) {
            __privateMethod(this, _LRUCache_instances, delete_fn).call(this, k, "expire");
          }
          if (status && allowStale)
            status.returnedStale = true;
          return allowStale ? value : void 0;
        } else {
          if (status && allowStale && value.__staleWhileFetching !== void 0) {
            status.returnedStale = true;
          }
          return allowStale ? value.__staleWhileFetching : void 0;
        }
      } else {
        if (status)
          status.get = "hit";
        if (fetching) {
          return value.__staleWhileFetching;
        }
        __privateMethod(this, _LRUCache_instances, moveToTail_fn).call(this, index);
        if (updateAgeOnGet) {
          __privateGet(this, _updateItemAge).call(this, index);
        }
        return value;
      }
    } else if (status) {
      status.get = "miss";
    }
  }
  /**
   * Deletes a key out of the cache.
   *
   * Returns true if the key was deleted, false otherwise.
   */
  delete(k) {
    return __privateMethod(this, _LRUCache_instances, delete_fn).call(this, k, "delete");
  }
  /**
   * Clear the cache entirely, throwing away all values.
   */
  clear() {
    return __privateMethod(this, _LRUCache_instances, clear_fn).call(this, "delete");
  }
};
_max = new WeakMap();
_maxSize = new WeakMap();
_dispose = new WeakMap();
_disposeAfter = new WeakMap();
_fetchMethod = new WeakMap();
_memoMethod = new WeakMap();
_size = new WeakMap();
_calculatedSize = new WeakMap();
_keyMap = new WeakMap();
_keyList = new WeakMap();
_valList = new WeakMap();
_next = new WeakMap();
_prev = new WeakMap();
_head = new WeakMap();
_tail = new WeakMap();
_free = new WeakMap();
_disposed = new WeakMap();
_sizes = new WeakMap();
_starts = new WeakMap();
_ttls = new WeakMap();
_hasDispose = new WeakMap();
_hasFetchMethod = new WeakMap();
_hasDisposeAfter = new WeakMap();
_LRUCache_instances = new WeakSet();
initializeTTLTracking_fn = function() {
  const ttls = new ZeroArray(__privateGet(this, _max));
  const starts = new ZeroArray(__privateGet(this, _max));
  __privateSet(this, _ttls, ttls);
  __privateSet(this, _starts, starts);
  __privateSet(this, _setItemTTL, (index, ttl, start = perf.now()) => {
    starts[index] = ttl !== 0 ? start : 0;
    ttls[index] = ttl;
    if (ttl !== 0 && this.ttlAutopurge) {
      const t = setTimeout(() => {
        if (__privateGet(this, _isStale).call(this, index)) {
          __privateMethod(this, _LRUCache_instances, delete_fn).call(this, __privateGet(this, _keyList)[index], "expire");
        }
      }, ttl + 1);
      if (t.unref) {
        t.unref();
      }
    }
  });
  __privateSet(this, _updateItemAge, (index) => {
    starts[index] = ttls[index] !== 0 ? perf.now() : 0;
  });
  __privateSet(this, _statusTTL, (status, index) => {
    if (ttls[index]) {
      const ttl = ttls[index];
      const start = starts[index];
      if (!ttl || !start)
        return;
      status.ttl = ttl;
      status.start = start;
      status.now = cachedNow || getNow();
      const age = status.now - start;
      status.remainingTTL = ttl - age;
    }
  });
  let cachedNow = 0;
  const getNow = () => {
    const n = perf.now();
    if (this.ttlResolution > 0) {
      cachedNow = n;
      const t = setTimeout(() => cachedNow = 0, this.ttlResolution);
      if (t.unref) {
        t.unref();
      }
    }
    return n;
  };
  this.getRemainingTTL = (key) => {
    const index = __privateGet(this, _keyMap).get(key);
    if (index === void 0) {
      return 0;
    }
    const ttl = ttls[index];
    const start = starts[index];
    if (!ttl || !start) {
      return Infinity;
    }
    const age = (cachedNow || getNow()) - start;
    return ttl - age;
  };
  __privateSet(this, _isStale, (index) => {
    const s = starts[index];
    const t = ttls[index];
    return !!t && !!s && (cachedNow || getNow()) - s > t;
  });
};
_updateItemAge = new WeakMap();
_statusTTL = new WeakMap();
_setItemTTL = new WeakMap();
_isStale = new WeakMap();
initializeSizeTracking_fn = function() {
  const sizes = new ZeroArray(__privateGet(this, _max));
  __privateSet(this, _calculatedSize, 0);
  __privateSet(this, _sizes, sizes);
  __privateSet(this, _removeItemSize, (index) => {
    __privateSet(this, _calculatedSize, __privateGet(this, _calculatedSize) - sizes[index]);
    sizes[index] = 0;
  });
  __privateSet(this, _requireSize, (k, v, size, sizeCalculation) => {
    if (__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v)) {
      return 0;
    }
    if (!isPosInt(size)) {
      if (sizeCalculation) {
        if (typeof sizeCalculation !== "function") {
          throw new TypeError("sizeCalculation must be a function");
        }
        size = sizeCalculation(v, k);
        if (!isPosInt(size)) {
          throw new TypeError("sizeCalculation return invalid (expect positive integer)");
        }
      } else {
        throw new TypeError("invalid size value (must be positive integer). When maxSize or maxEntrySize is used, sizeCalculation or size must be set.");
      }
    }
    return size;
  });
  __privateSet(this, _addItemSize, (index, size, status) => {
    sizes[index] = size;
    if (__privateGet(this, _maxSize)) {
      const maxSize = __privateGet(this, _maxSize) - sizes[index];
      while (__privateGet(this, _calculatedSize) > maxSize) {
        __privateMethod(this, _LRUCache_instances, evict_fn).call(this, true);
      }
    }
    __privateSet(this, _calculatedSize, __privateGet(this, _calculatedSize) + sizes[index]);
    if (status) {
      status.entrySize = size;
      status.totalCalculatedSize = __privateGet(this, _calculatedSize);
    }
  });
};
_removeItemSize = new WeakMap();
_addItemSize = new WeakMap();
_requireSize = new WeakMap();
indexes_fn = function* ({ allowStale = this.allowStale } = {}) {
  if (__privateGet(this, _size)) {
    for (let i = __privateGet(this, _tail); true; ) {
      if (!__privateMethod(this, _LRUCache_instances, isValidIndex_fn).call(this, i)) {
        break;
      }
      if (allowStale || !__privateGet(this, _isStale).call(this, i)) {
        yield i;
      }
      if (i === __privateGet(this, _head)) {
        break;
      } else {
        i = __privateGet(this, _prev)[i];
      }
    }
  }
};
rindexes_fn = function* ({ allowStale = this.allowStale } = {}) {
  if (__privateGet(this, _size)) {
    for (let i = __privateGet(this, _head); true; ) {
      if (!__privateMethod(this, _LRUCache_instances, isValidIndex_fn).call(this, i)) {
        break;
      }
      if (allowStale || !__privateGet(this, _isStale).call(this, i)) {
        yield i;
      }
      if (i === __privateGet(this, _tail)) {
        break;
      } else {
        i = __privateGet(this, _next)[i];
      }
    }
  }
};
isValidIndex_fn = function(index) {
  return index !== void 0 && __privateGet(this, _keyMap).get(__privateGet(this, _keyList)[index]) === index;
};
evict_fn = function(free) {
  var _a3, _b2;
  const head = __privateGet(this, _head);
  const k = __privateGet(this, _keyList)[head];
  const v = __privateGet(this, _valList)[head];
  if (__privateGet(this, _hasFetchMethod) && __privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v)) {
    v.__abortController.abort(new Error("evicted"));
  } else if (__privateGet(this, _hasDispose) || __privateGet(this, _hasDisposeAfter)) {
    if (__privateGet(this, _hasDispose)) {
      (_a3 = __privateGet(this, _dispose)) == null ? void 0 : _a3.call(this, v, k, "evict");
    }
    if (__privateGet(this, _hasDisposeAfter)) {
      (_b2 = __privateGet(this, _disposed)) == null ? void 0 : _b2.push([v, k, "evict"]);
    }
  }
  __privateGet(this, _removeItemSize).call(this, head);
  if (free) {
    __privateGet(this, _keyList)[head] = void 0;
    __privateGet(this, _valList)[head] = void 0;
    __privateGet(this, _free).push(head);
  }
  if (__privateGet(this, _size) === 1) {
    __privateSet(this, _head, __privateSet(this, _tail, 0));
    __privateGet(this, _free).length = 0;
  } else {
    __privateSet(this, _head, __privateGet(this, _next)[head]);
  }
  __privateGet(this, _keyMap).delete(k);
  __privateWrapper(this, _size)._--;
  return head;
};
backgroundFetch_fn = function(k, index, options, context) {
  const v = index === void 0 ? void 0 : __privateGet(this, _valList)[index];
  if (__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v)) {
    return v;
  }
  const ac = new AC();
  const { signal } = options;
  signal == null ? void 0 : signal.addEventListener("abort", () => ac.abort(signal.reason), {
    signal: ac.signal
  });
  const fetchOpts = {
    signal: ac.signal,
    options,
    context
  };
  const cb = (v2, updateCache = false) => {
    const { aborted } = ac.signal;
    const ignoreAbort = options.ignoreFetchAbort && v2 !== void 0;
    if (options.status) {
      if (aborted && !updateCache) {
        options.status.fetchAborted = true;
        options.status.fetchError = ac.signal.reason;
        if (ignoreAbort)
          options.status.fetchAbortIgnored = true;
      } else {
        options.status.fetchResolved = true;
      }
    }
    if (aborted && !ignoreAbort && !updateCache) {
      return fetchFail(ac.signal.reason);
    }
    const bf2 = p;
    if (__privateGet(this, _valList)[index] === p) {
      if (v2 === void 0) {
        if (bf2.__staleWhileFetching) {
          __privateGet(this, _valList)[index] = bf2.__staleWhileFetching;
        } else {
          __privateMethod(this, _LRUCache_instances, delete_fn).call(this, k, "fetch");
        }
      } else {
        if (options.status)
          options.status.fetchUpdated = true;
        this.set(k, v2, fetchOpts.options);
      }
    }
    return v2;
  };
  const eb = (er) => {
    if (options.status) {
      options.status.fetchRejected = true;
      options.status.fetchError = er;
    }
    return fetchFail(er);
  };
  const fetchFail = (er) => {
    const { aborted } = ac.signal;
    const allowStaleAborted = aborted && options.allowStaleOnFetchAbort;
    const allowStale = allowStaleAborted || options.allowStaleOnFetchRejection;
    const noDelete = allowStale || options.noDeleteOnFetchRejection;
    const bf2 = p;
    if (__privateGet(this, _valList)[index] === p) {
      const del = !noDelete || bf2.__staleWhileFetching === void 0;
      if (del) {
        __privateMethod(this, _LRUCache_instances, delete_fn).call(this, k, "fetch");
      } else if (!allowStaleAborted) {
        __privateGet(this, _valList)[index] = bf2.__staleWhileFetching;
      }
    }
    if (allowStale) {
      if (options.status && bf2.__staleWhileFetching !== void 0) {
        options.status.returnedStale = true;
      }
      return bf2.__staleWhileFetching;
    } else if (bf2.__returned === bf2) {
      throw er;
    }
  };
  const pcall = (res, rej) => {
    var _a3;
    const fmp = (_a3 = __privateGet(this, _fetchMethod)) == null ? void 0 : _a3.call(this, k, v, fetchOpts);
    if (fmp && fmp instanceof Promise) {
      fmp.then((v2) => res(v2 === void 0 ? void 0 : v2), rej);
    }
    ac.signal.addEventListener("abort", () => {
      if (!options.ignoreFetchAbort || options.allowStaleOnFetchAbort) {
        res(void 0);
        if (options.allowStaleOnFetchAbort) {
          res = (v2) => cb(v2, true);
        }
      }
    });
  };
  if (options.status)
    options.status.fetchDispatched = true;
  const p = new Promise(pcall).then(cb, eb);
  const bf = Object.assign(p, {
    __abortController: ac,
    __staleWhileFetching: v,
    __returned: void 0
  });
  if (index === void 0) {
    this.set(k, bf, { ...fetchOpts.options, status: void 0 });
    index = __privateGet(this, _keyMap).get(k);
  } else {
    __privateGet(this, _valList)[index] = bf;
  }
  return bf;
};
isBackgroundFetch_fn = function(p) {
  if (!__privateGet(this, _hasFetchMethod))
    return false;
  const b = p;
  return !!b && b instanceof Promise && b.hasOwnProperty("__staleWhileFetching") && b.__abortController instanceof AC;
};
connect_fn = function(p, n) {
  __privateGet(this, _prev)[n] = p;
  __privateGet(this, _next)[p] = n;
};
moveToTail_fn = function(index) {
  if (index !== __privateGet(this, _tail)) {
    if (index === __privateGet(this, _head)) {
      __privateSet(this, _head, __privateGet(this, _next)[index]);
    } else {
      __privateMethod(this, _LRUCache_instances, connect_fn).call(this, __privateGet(this, _prev)[index], __privateGet(this, _next)[index]);
    }
    __privateMethod(this, _LRUCache_instances, connect_fn).call(this, __privateGet(this, _tail), index);
    __privateSet(this, _tail, index);
  }
};
delete_fn = function(k, reason) {
  var _a3, _b2, _c, _d;
  let deleted = false;
  if (__privateGet(this, _size) !== 0) {
    const index = __privateGet(this, _keyMap).get(k);
    if (index !== void 0) {
      deleted = true;
      if (__privateGet(this, _size) === 1) {
        __privateMethod(this, _LRUCache_instances, clear_fn).call(this, reason);
      } else {
        __privateGet(this, _removeItemSize).call(this, index);
        const v = __privateGet(this, _valList)[index];
        if (__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v)) {
          v.__abortController.abort(new Error("deleted"));
        } else if (__privateGet(this, _hasDispose) || __privateGet(this, _hasDisposeAfter)) {
          if (__privateGet(this, _hasDispose)) {
            (_a3 = __privateGet(this, _dispose)) == null ? void 0 : _a3.call(this, v, k, reason);
          }
          if (__privateGet(this, _hasDisposeAfter)) {
            (_b2 = __privateGet(this, _disposed)) == null ? void 0 : _b2.push([v, k, reason]);
          }
        }
        __privateGet(this, _keyMap).delete(k);
        __privateGet(this, _keyList)[index] = void 0;
        __privateGet(this, _valList)[index] = void 0;
        if (index === __privateGet(this, _tail)) {
          __privateSet(this, _tail, __privateGet(this, _prev)[index]);
        } else if (index === __privateGet(this, _head)) {
          __privateSet(this, _head, __privateGet(this, _next)[index]);
        } else {
          const pi = __privateGet(this, _prev)[index];
          __privateGet(this, _next)[pi] = __privateGet(this, _next)[index];
          const ni = __privateGet(this, _next)[index];
          __privateGet(this, _prev)[ni] = __privateGet(this, _prev)[index];
        }
        __privateWrapper(this, _size)._--;
        __privateGet(this, _free).push(index);
      }
    }
  }
  if (__privateGet(this, _hasDisposeAfter) && ((_c = __privateGet(this, _disposed)) == null ? void 0 : _c.length)) {
    const dt = __privateGet(this, _disposed);
    let task;
    while (task = dt == null ? void 0 : dt.shift()) {
      (_d = __privateGet(this, _disposeAfter)) == null ? void 0 : _d.call(this, ...task);
    }
  }
  return deleted;
};
clear_fn = function(reason) {
  var _a3, _b2, _c;
  for (const index of __privateMethod(this, _LRUCache_instances, rindexes_fn).call(this, { allowStale: true })) {
    const v = __privateGet(this, _valList)[index];
    if (__privateMethod(this, _LRUCache_instances, isBackgroundFetch_fn).call(this, v)) {
      v.__abortController.abort(new Error("deleted"));
    } else {
      const k = __privateGet(this, _keyList)[index];
      if (__privateGet(this, _hasDispose)) {
        (_a3 = __privateGet(this, _dispose)) == null ? void 0 : _a3.call(this, v, k, reason);
      }
      if (__privateGet(this, _hasDisposeAfter)) {
        (_b2 = __privateGet(this, _disposed)) == null ? void 0 : _b2.push([v, k, reason]);
      }
    }
  }
  __privateGet(this, _keyMap).clear();
  __privateGet(this, _valList).fill(void 0);
  __privateGet(this, _keyList).fill(void 0);
  if (__privateGet(this, _ttls) && __privateGet(this, _starts)) {
    __privateGet(this, _ttls).fill(0);
    __privateGet(this, _starts).fill(0);
  }
  if (__privateGet(this, _sizes)) {
    __privateGet(this, _sizes).fill(0);
  }
  __privateSet(this, _head, 0);
  __privateSet(this, _tail, 0);
  __privateGet(this, _free).length = 0;
  __privateSet(this, _calculatedSize, 0);
  __privateSet(this, _size, 0);
  if (__privateGet(this, _hasDisposeAfter) && __privateGet(this, _disposed)) {
    const dt = __privateGet(this, _disposed);
    let task;
    while (task = dt == null ? void 0 : dt.shift()) {
      (_c = __privateGet(this, _disposeAfter)) == null ? void 0 : _c.call(this, ...task);
    }
  }
};
var LRUCache = _LRUCache;

// src/dataloader.ts
var Cache = class {
  constructor() {
    // current branch
    this.branch = "";
    // all commits in current branch
    this.commits = [];
    // key: history view context, value: GitLogEntry[]
    this.logEntries = new LRUCache({ max: 5 });
    // key: history view context, value: total commits count
    this.counts = new LRUCache({ max: 100 });
  }
  countKey(branch, file, author, startTime, endTime) {
    var _a3, _b2;
    return `${branch},${file != null ? file : ""},${author != null ? author : ""},${(_a3 = startTime == null ? void 0 : startTime.getTime()) != null ? _a3 : ""},${(_b2 = endTime == null ? void 0 : endTime.getTime()) != null ? _b2 : ""}`;
  }
  logEntryKey(branch, stash, file, line, author, startTime, endTime) {
    var _a3, _b2;
    return `${branch},${stash ? "1" : ""},${file != null ? file : ""},${line != null ? line : ""},${author != null ? author : ""},${(_a3 = startTime == null ? void 0 : startTime.getTime()) != null ? _a3 : ""},${(_b2 = endTime == null ? void 0 : endTime.getTime()) != null ? _b2 : ""}`;
  }
  clear() {
    this.branch = "";
    this.commits = [];
    this.logEntries.clear();
    this.counts.clear();
  }
};
// cached log entries count
Cache.logEntriesCount = 1200;
var Dataloader = class {
  constructor(ctx, _gitService) {
    this._gitService = _gitService;
    this._cacheEnabled = true;
    this._cache = new Cache();
    this._updating = false;
    this._debouncedUpdate = debounce((repo) => this._updateCaches(repo), 1e3);
    this._gitService.onDidChangeCurrentGitRepo((repo) => this._updateRepo(repo), null, ctx.subscriptions);
  }
  enableCache(enable) {
    if (this._cacheEnabled === enable) {
      return;
    }
    this._cacheEnabled = enable;
    enable ? this._enableCache() : this._disableCache();
  }
  async getLogEntries(repo, express, start, count, branch, isStash, file, line, author, startTime, endTime) {
    if (!this._useCache(repo.root)) {
      return this._gitService.getLogEntries(
        repo,
        express,
        start,
        count,
        branch,
        isStash,
        file,
        line,
        author,
        startTime,
        endTime
      );
    }
    const key = this._cache.logEntryKey(branch, isStash != null ? isStash : false, file == null ? void 0 : file.fsPath, line, author, startTime, endTime);
    const cache = this._cache.logEntries.get(key);
    if (cache) {
      if (cache.length < Cache.logEntriesCount) {
        return cache.slice(start, start + count);
      }
      if (start + count < cache.length) {
        return cache.slice(start, start + count);
      }
    }
    const entries = await this._gitService.getLogEntries(
      repo,
      express,
      start,
      count,
      branch,
      isStash,
      file,
      line,
      author,
      startTime,
      endTime
    );
    if (start == 0) {
      if (count >= Cache.logEntriesCount && entries.length < Cache.logEntriesCount) {
        this._cache.logEntries.set(key, entries);
      } else if (count < Cache.logEntriesCount) {
        Promise.resolve().then(async () => {
          const cacheEntries = await this._gitService.getLogEntries(
            repo,
            express,
            0,
            Cache.logEntriesCount,
            branch,
            isStash,
            file,
            line,
            author,
            startTime,
            endTime
          );
          this._cache.logEntries.set(key, cacheEntries);
          Tracer.info(`Dataloader: update log entries cache ${key}, ${cacheEntries.length}`);
        });
      }
    } else {
      Tracer.info(`Dataloader: cache missing for non-first page ${key}, start {${start}}, count ${count}`);
    }
    return entries;
  }
  async getCommitsCount(repo, branch, file, author, startTime, endTime) {
    if (!this._useCache(repo.root)) {
      return this._gitService.getCommitsCount(repo, branch, file, author, startTime, endTime);
    }
    const key = this._cache.countKey(branch, file == null ? void 0 : file.fsPath, author, startTime, endTime);
    const count = this._cache.counts.get(key);
    if (count) {
      return count;
    }
    const result = await this._gitService.getCommitsCount(repo, branch, file, author, startTime, endTime);
    this._cache.counts.set(key, result);
    return result;
  }
  async getCurrentBranch(repo) {
    var _a3;
    if (!repo) {
      return "";
    }
    return this._useCache(repo.root) ? this._cache.branch : (_a3 = await this._gitService.getCurrentBranch(repo)) != null ? _a3 : "";
  }
  async getNextCommit(repo, ref) {
    if (!repo) {
      return "";
    }
    const commits = this._useCache(repo.root) ? this._cache.commits : await this._gitService.getCommits(repo);
    const index = commits.indexOf(ref);
    return index > 0 ? commits[index - 1] : "";
  }
  async getPreviousCommit(repo, ref) {
    if (!repo) {
      return "";
    }
    const commits = this._useCache(repo.root) ? this._cache.commits : await this._gitService.getCommits(repo);
    const index = commits.indexOf(ref);
    return index >= 0 && index + 1 < commits.length ? commits[index + 1] : "";
  }
  // Returns [has previous commit, has next commit]
  async hasNeighborCommits(repo, ref) {
    if (!repo) {
      return [false, false];
    }
    const commits = this._useCache(repo.root) ? this._cache.commits : await this._gitService.getCommits(repo);
    const index = commits.indexOf(ref);
    return [index >= 0 && index + 1 < commits.length, index > 0];
  }
  _enableCache() {
    const repo = this._gitService.currentGitRepo;
    if (!repo) {
      return;
    }
    if (repo.root.startsWith("/mnt")) {
      this._cacheEnabled = false;
      return;
    }
    this._repo = repo;
    const watching = new vs14.RelativePattern(path5.join(repo.root, ".git"), "**");
    this._fsWatcher = vs14.workspace.createFileSystemWatcher(watching);
    this._fsWatcher.onDidChange((uri) => this._handleFileUpdate(repo, uri));
    this._fsWatcher.onDidCreate((uri) => this._handleFileUpdate(repo, uri));
    this._fsWatcher.onDidDelete((uri) => this._handleFileUpdate(repo, uri));
    this._handleFileUpdate(repo);
    Tracer.info(`Dataloader: started watching ${watching.baseUri.fsPath}`);
  }
  _disableCache() {
    this._repo = void 0;
    if (this._fsWatcher) {
      this._fsWatcher.dispose();
    }
    this._cache.clear();
    this._updating = false;
  }
  async _updateRepo(repo) {
    var _a3;
    if (!this._cacheEnabled || ((_a3 = this._repo) == null ? void 0 : _a3.root) === repo.root) {
      return;
    }
    this._disableCache();
    this._enableCache();
  }
  _handleFileUpdate(repo, uri) {
    Tracer.verbose(`Dataloader: _handleFileUpdate: current repo:${repo.root}, uri:${uri == null ? void 0 : uri.fsPath}`);
    this._updating = true;
    this._debouncedUpdate(repo);
  }
  async _updateCaches(repo) {
    var _a3, _b2, _c;
    Tracer.verbose(`Dataloader: _updateCaches: updating cache for ${repo.root}`);
    const branch = (_a3 = await this._gitService.getCurrentBranch(repo)) != null ? _a3 : "";
    const [commits, count, logs] = await Promise.all([
      this._gitService.getCommits(repo, branch),
      this._gitService.getCommitsCount(repo, branch),
      this._gitService.getLogEntries(repo, false, 0, Cache.logEntriesCount, branch)
    ]);
    if (((_b2 = this._repo) == null ? void 0 : _b2.root) !== repo.root) {
      Tracer.warning(`Dataloader: different repo: ${repo.root} ${(_c = this._repo) == null ? void 0 : _c.root}`);
      return;
    }
    this._cache.branch = branch;
    this._cache.commits = commits;
    this._cache.counts.set(this._cache.countKey(branch), count);
    this._cache.logEntries.set(this._cache.logEntryKey(branch), logs);
    this._updating = false;
    Tracer.verbose(`Dataloader: _updateCaches: cache updated for ${repo.root}`);
  }
  _useCache(repo) {
    var _a3, _b2;
    if (!this._cacheEnabled) {
      return false;
    }
    if (repo !== ((_a3 = this._repo) == null ? void 0 : _a3.root)) {
      Tracer.warning(`Dataloader: different repo: ${repo} ${(_b2 = this._repo) == null ? void 0 : _b2.root}`);
      return false;
    }
    if (this._updating) {
      Tracer.info("Dataloader: updating");
      return false;
    }
    return true;
  }
};

// src/extension.ts
function activate(context) {
  initializeIcons(context);
  let gitService = new GitService(context);
  let dataloader = new Dataloader(context, gitService);
  let model = new Model(context, dataloader);
  let panelView = new PanelViewProvider(context, model);
  let historyViewProvider = new HistoryViewProvider(context, model, dataloader, gitService, panelView);
  let explorerViewProvider = new ExplorerViewProvider(context, model, dataloader, gitService);
  new InfoViewProvider(context, model, gitService);
  new BlameViewProvider(context, model, gitService);
  new CommandCenter(context, model, dataloader, gitService, historyViewProvider, explorerViewProvider);
  gitService.updateGitRoots(vs15.workspace.workspaceFolders);
}
function deactivate() {
}
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  activate,
  deactivate
});
/*! Bundled license information:

lodash/lodash.js:
  (**
   * @license
   * Lodash <https://lodash.com/>
   * Copyright OpenJS Foundation and other contributors <https://openjsf.org/>
   * Released under MIT license <https://lodash.com/license>
   * Based on Underscore.js 1.8.3 <http://underscorejs.org/LICENSE>
   * Copyright Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
   *)
*/
//# sourceMappingURL=extension.js.map
