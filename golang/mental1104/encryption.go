package mental1104

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"errors"
	"unicode/utf8"
)

// CipherAlgorithm 选择 AES 实现；LegacyCBC 仅用于兼容，AESGCM 用于新业务。
type CipherAlgorithm uint8

const (
	LegacyCBC CipherAlgorithm = iota // LegacyCBC 保留历史 CBC、PKCS#7 和默认 Key/IV。
	AESGCM                           // AESGCM 使用显式 32 字节密钥和随机 12 字节 Nonce。
)

const cipherMagic = "mental1104:AES:"
const cipherHeader = cipherMagic + "\x01"
const legacyCipherKey = "0ePThPnLaJcWFcRc"
const cipherNonceSize = 12
const cipherTagSize = 16

// selectCipher 解析至多一个算法参数；algorithms 为空时返回 fallback，数量无效时返回错误。
func selectCipher(algorithms []CipherAlgorithm, fallback CipherAlgorithm) (CipherAlgorithm, error) {
	if len(algorithms) > 1 {
		return 0, errors.New("at most one cipher algorithm is allowed")
	}
	if len(algorithms) == 0 {
		return fallback, nil
	}
	if algorithms[0] != LegacyCBC && algorithms[0] != AESGCM {
		return 0, errors.New("unsupported cipher algorithm")
	}
	return algorithms[0], nil
}

// legacyBlock 读取 key 和 salt；nil 分别表示历史默认 Key 和使用 Key 作为 IV。
// 返回 AES 块与只读 IV；长度错误返回 error，不修改调用方切片。
func legacyBlock(key, salt []byte) (cipher.Block, []byte, error) {
	if key == nil {
		key = []byte(legacyCipherKey)
	}
	if salt == nil {
		salt = key
	}
	if len(salt) != aes.BlockSize {
		return nil, nil, errors.New("Legacy CBC requires a 16-byte IV")
	}
	block, err := aes.NewCipher(key)
	return block, salt, err
}

// gcmCipher 校验 key 必须恰好 32 字节、salt 必须为 nil；返回固定 16 字节 Tag 的 AEAD。
func gcmCipher(key, salt []byte) (cipher.AEAD, error) {
	if salt != nil {
		return nil, errors.New("AES-GCM does not accept salt")
	}
	if len(key) != 32 {
		return nil, errors.New("AES-256-GCM requires an explicit 32-byte key")
	}
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}
	return cipher.NewGCM(block)
}

// encryptLegacy 将 UTF-8 data 按 key/显式 salt 加密，返回原始 CBC 密文；salt 缺失或参数错误时拒绝。
func encryptLegacy(data, key, salt []byte) ([]byte, error) {
	if salt == nil {
		return nil, errors.New("Legacy CBC encryption requires an explicit salt (IV)")
	}
	block, iv, err := legacyBlock(key, salt)
	if err != nil {
		return nil, err
	}
	padding := aes.BlockSize - len(data)%aes.BlockSize
	padded := append(append([]byte(nil), data...), bytes.Repeat([]byte{byte(padding)}, padding)...)
	cipher.NewCBCEncrypter(block, iv).CryptBlocks(padded, padded)
	return padded, nil
}

// decryptLegacy 读取原始 CBC data、key、salt；长度或 Padding 错误返回 error，否则返回新明文字节。
func decryptLegacy(data, key, salt []byte) ([]byte, error) {
	block, iv, err := legacyBlock(key, salt)
	if err != nil {
		return nil, err
	}
	if len(data) == 0 || len(data)%aes.BlockSize != 0 {
		return nil, errors.New("invalid Legacy CBC ciphertext length")
	}
	plain := make([]byte, len(data))
	cipher.NewCBCDecrypter(block, iv).CryptBlocks(plain, data)
	padding := int(plain[len(plain)-1])
	if padding < 1 || padding > aes.BlockSize || !bytes.Equal(plain[len(plain)-padding:], bytes.Repeat([]byte{byte(padding)}, padding)) {
		return nil, errors.New("invalid Legacy CBC padding")
	}
	return plain[:len(plain)-padding], nil
}

// encryptGCM 使用 key 加密 data；salt 仅允许 nil，返回版本化原始密文或参数/随机源错误。
func encryptGCM(data, key, salt []byte) ([]byte, error) {
	aead, err := gcmCipher(key, salt)
	if err != nil {
		return nil, err
	}
	nonce := make([]byte, cipherNonceSize)
	if _, err = rand.Read(nonce); err != nil {
		return nil, err
	}
	sealed := aead.Seal(nil, nonce, data, []byte(cipherHeader))
	// Go 输出 Ciphertext + Tag；封装统一为 Header + Nonce + Tag + Ciphertext。
	result := append([]byte(cipherHeader), nonce...)
	result = append(result, sealed[len(sealed)-cipherTagSize:]...)
	return append(result, sealed[:len(sealed)-cipherTagSize]...), nil
}

// decryptGCM 验证 data 的版本与认证；key 必须 32 字节、salt 必须 nil，失败直接返回 error。
func decryptGCM(data, key, salt []byte) ([]byte, error) {
	if !bytes.HasPrefix(data, []byte(cipherMagic)) || len(data) < len(cipherHeader) {
		return nil, errors.New("invalid or truncated AES-GCM header")
	}
	if data[len(cipherMagic)] != 1 {
		return nil, errors.New("unsupported AES-GCM ciphertext version")
	}
	nonceEnd := len(cipherHeader) + cipherNonceSize
	tagEnd := nonceEnd + cipherTagSize
	if len(data) < tagEnd {
		return nil, errors.New("truncated AES-GCM ciphertext")
	}
	aead, err := gcmCipher(key, salt)
	if err != nil {
		return nil, err
	}
	sealed := append(append([]byte(nil), data[tagEnd:]...), data[nonceEnd:tagEnd]...)
	return aead.Open(nil, data[len(cipherHeader):nonceEnd], sealed, data[:len(cipherHeader)])
}

// Encrypt 加密 plaintext 并返回标准 Base64 bytes；key/salt 为只读字节切片。
// algorithms 可省略（LegacyCBC）或提供一个算法。CBC 的 nil key 使用历史默认 Key，salt 必须显式提供；
// AESGCM 要求显式 32 字节 key、nil salt。参数、UTF-8、随机源或加密失败返回 error。
func Encrypt(plaintext string, key, salt []byte, algorithms ...CipherAlgorithm) ([]byte, error) {
	algorithm, err := selectCipher(algorithms, LegacyCBC)
	if err != nil {
		return nil, err
	}
	if !utf8.ValidString(plaintext) {
		return nil, errors.New("plaintext must be valid UTF-8")
	}
	var payload []byte
	if algorithm == AESGCM {
		payload, err = encryptGCM([]byte(plaintext), key, salt)
	} else {
		payload, err = encryptLegacy([]byte(plaintext), key, salt)
	}
	if err != nil {
		return nil, err
	}
	return []byte(base64.StdEncoding.EncodeToString(payload)), nil
}

// Decrypt 解密 Base64 ciphertext 并返回 UTF-8 字符串；输入切片均不修改。
// key 规则同 Encrypt；CBC 的 nil salt 仅在历史解密时使用 Key 作为 IV，GCM 的 salt 必须 nil。
// algorithms 省略时按 Magic 自动识别，或显式提供一个算法。
// 版本、截断、认证或 UTF-8 错误直接返回 error；已识别的新格式绝不降级 CBC。
func Decrypt(ciphertext, key, salt []byte, algorithms ...CipherAlgorithm) (string, error) {
	payload, err := base64.StdEncoding.DecodeString(string(ciphertext))
	if err != nil {
		return "", err
	}
	isGCM := bytes.HasPrefix(payload, []byte(cipherMagic))
	fallback := LegacyCBC
	if isGCM {
		fallback = AESGCM
	}
	algorithm, err := selectCipher(algorithms, fallback)
	if err != nil {
		return "", err
	}
	var plain []byte
	if algorithm == AESGCM {
		plain, err = decryptGCM(payload, key, salt)
	} else if isGCM {
		return "", errors.New("AES-GCM ciphertext cannot be decrypted with Legacy CBC")
	} else {
		plain, err = decryptLegacy(payload, key, salt)
	}
	if err != nil {
		return "", err
	}
	if !utf8.Valid(plain) {
		return "", errors.New("plaintext is not valid UTF-8")
	}
	return string(plain), nil
}
