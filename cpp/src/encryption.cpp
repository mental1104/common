#include "mental1104/encryption.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <climits>
#include <memory>
#include <stdexcept>

namespace mental1104 {
namespace encryption {
namespace {

const std::string kMagic = "mental1104:AES:";
const std::string kHeader = kMagic + '\x01';
const std::size_t kNonceSize = 12;
const std::size_t kTagSize = 16;
using Context = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
enum class Operation { Encrypt, Decrypt };

/// 校验 OpenSSL 整数长度边界；size 为输入字节数，返回安全的 int，否则抛
/// invalid_argument。
int input_size(std::size_t size) {
    if (size > static_cast<std::size_t>(INT_MAX - EVP_MAX_BLOCK_LENGTH)) {
        throw std::invalid_argument("Cipher input is too large");
    }
    return static_cast<int>(size);
}

/// 检查 OpenSSL status；失败时按 message 抛 runtime_error，成功时无副作用。
void require_success(int status, const char *message) {
    if (status != 1) {
        throw std::runtime_error(message);
    }
}

/// 按 cipher、key、iv 和 operation 创建独占上下文，失败抛异常；返回值通过 RAII
/// 释放。
Context make_context(const EVP_CIPHER *cipher, const std::string &key,
    const std::string &iv, Operation operation) {
    Context context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!context) {
        throw std::runtime_error("Cannot allocate cipher context");
    }
    require_success(EVP_CipherInit_ex(context.get(), cipher, nullptr,
                        reinterpret_cast<const unsigned char *>(key.data()),
                        reinterpret_cast<const unsigned char *>(iv.data()),
                        operation == Operation::Encrypt ? 1 : 0),
        "Cannot initialize cipher");
    return context;
}

/// 消费 data 并完成 context；返回原始结果，认证或 Padding
/// 失败时抛异常且不返回明文。
std::string finish(EVP_CIPHER_CTX *context, const std::string &data) {
    const int size = input_size(data.size());
    std::string result(data.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int written = 0;
    require_success(
        EVP_CipherUpdate(context, reinterpret_cast<unsigned char *>(&result[0]),
            &written, reinterpret_cast<const unsigned char *>(data.data()),
            size),
        "Cipher update failed");
    int final_size = 0;
    require_success(EVP_CipherFinal_ex(context,
                        reinterpret_cast<unsigned char *>(&result[0]) + written,
                        &final_size),
        "Cipher authentication or padding verification failed");
    result.resize(static_cast<std::size_t>(written + final_size));
    return result;
}

/// 将 header 作为 context 的附加认证数据；失败抛
/// runtime_error，不产生密文字节。
void authenticate_header(EVP_CIPHER_CTX *context, const std::string &header) {
    int written = 0;
    require_success(EVP_CipherUpdate(context, nullptr, &written,
                        reinterpret_cast<const unsigned char *>(header.data()),
                        input_size(header.size())),
        "Cannot authenticate cipher header");
}

/// 为 CBC key 选择 AES 密钥长度；非法长度抛 invalid_argument，返回借用的
/// OpenSSL 描述符。
const EVP_CIPHER *legacy_cipher(const std::string &key) {
    switch (key.size()) {
        case 16:
            return EVP_aes_128_cbc();
        case 24:
            return EVP_aes_192_cbc();
        case 32:
            return EVP_aes_256_cbc();
        default:
            throw std::invalid_argument("Invalid Legacy CBC key length");
    }
}

/// 按 operation 对 data 执行 CBC；key 为原始字节，salt 必须在加密时显式提供，
/// 解密时的 nullptr 保留历史 Key 作为 IV；返回结果或异常。
std::string legacy(const std::string &data, const std::string &key,
    const std::string *salt, Operation operation) {
    if (operation == Operation::Encrypt && salt == nullptr) {
        throw std::invalid_argument(
            "Legacy CBC encryption requires an explicit salt (IV)");
    }
    const std::string &iv = salt ? *salt : key;
    if (iv.size() != 16) {
        throw std::invalid_argument("Legacy CBC requires a 16-byte IV");
    }
    const Context context =
        make_context(legacy_cipher(key), key, iv, operation);
    return finish(context.get(), data);
}

/// 校验 GCM key 为 32 字节、salt 为 nullptr；参数非法抛
/// invalid_argument，成功时无副作用。
void validate_gcm(const std::string &key, const std::string *salt) {
    if (salt) {
        throw std::invalid_argument("AES-GCM does not accept salt");
    }
    if (key.size() != 32) {
        throw std::invalid_argument(
            "AES-256-GCM requires an explicit 32-byte key");
    }
}

/// 按 key 加密 data；salt 必须 nullptr，返回版本化 GCM 原始字节或异常。
std::string encrypt_gcm(
    const std::string &data, const std::string &key, const std::string *salt) {
    validate_gcm(key, salt);
    std::string nonce(kNonceSize, '\0');
    require_success(RAND_bytes(reinterpret_cast<unsigned char *>(&nonce[0]),
                        input_size(nonce.size())),
        "Secure nonce generation failed");
    // OpenSSL GCM 默认 IV 长度为 12 字节，参数由固定协议控制。
    const Context context =
        make_context(EVP_aes_256_gcm(), key, nonce, Operation::Encrypt);
    authenticate_header(context.get(), kHeader);
    const std::string encrypted = finish(context.get(), data);
    std::string tag(kTagSize, '\0');
    require_success(EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG,
                        static_cast<int>(kTagSize), &tag[0]),
        "Cannot read AES-GCM tag");
    return kHeader + nonce + tag + encrypted;
}

/// 验证并解密 payload；key 为 32 字节、salt 必须
/// nullptr，格式或认证错误直接抛异常。
std::string decrypt_gcm(const std::string &payload, const std::string &key,
    const std::string *salt) {
    if (payload.compare(0, kMagic.size(), kMagic) != 0 ||
        payload.size() < kHeader.size()) {
        throw std::invalid_argument("Invalid or truncated AES-GCM header");
    }
    if (payload[kMagic.size()] != '\x01') {
        throw std::invalid_argument("Unsupported AES-GCM ciphertext version");
    }
    const std::size_t nonce_end = kHeader.size() + kNonceSize;
    const std::size_t tag_end = nonce_end + kTagSize;
    if (payload.size() < tag_end) {
        throw std::invalid_argument("Truncated AES-GCM ciphertext");
    }
    validate_gcm(key, salt);
    const Context context = make_context(EVP_aes_256_gcm(), key,
        payload.substr(kHeader.size(), kNonceSize), Operation::Decrypt);
    authenticate_header(context.get(), payload.substr(0, kHeader.size()));
    // SET_TAG 接口需要可写地址，复制到局部缓冲区，避免移除输入 const。
    std::string tag = payload.substr(nonce_end, kTagSize);
    require_success(EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG,
                        static_cast<int>(kTagSize), &tag[0]),
        "Cannot set AES-GCM tag");
    return finish(context.get(), payload.substr(tag_end));
}

/// 把 data 编码为标准 Base64，返回新字符串；长度越界抛 invalid_argument。
std::string encode(const std::string &data) {
    if (data.size() > static_cast<std::size_t>(INT_MAX / 4 * 3 - 3)) {
        throw std::invalid_argument("Cipher input is too large for Base64");
    }
    const std::size_t size = 4 * ((data.size() + 2) / 3);
    std::string result(size + 1, '\0');
    const int written =
        EVP_EncodeBlock(reinterpret_cast<unsigned char *>(&result[0]),
            reinterpret_cast<const unsigned char *>(data.data()),
            input_size(data.size()));
    result.resize(static_cast<std::size_t>(written));
    return result;
}

/// 解码 Base64 data；允许 CR/LF 换行，返回原始字节，非法字符或 Padding 抛
/// invalid_argument。
std::string decode(const std::string &data) {
    std::string clean;
    clean.reserve(data.size());
    for (const char c : data) {
        if (c != '\r' && c != '\n') {
            clean.push_back(c);
        }
    }
    if (clean.empty()) {
        return {};
    }
    if (clean.size() % 4 != 0) {
        throw std::invalid_argument("Invalid Base64 ciphertext");
    }
    const std::size_t padding =
        clean.back() != '=' ? 0 : (clean[clean.size() - 2] == '=' ? 2 : 1);
    for (std::size_t i = 0; i < clean.size() - padding; ++i) {
        const char c = clean[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '+' || c == '/')) {
            throw std::invalid_argument("Invalid Base64 ciphertext");
        }
    }
    const int size = input_size(clean.size());
    std::string result(clean.size() / 4 * 3, '\0');
    const int written =
        EVP_DecodeBlock(reinterpret_cast<unsigned char *>(&result[0]),
            reinterpret_cast<const unsigned char *>(clean.data()), size);
    if (written < 0) {
        throw std::invalid_argument("Invalid Base64 ciphertext");
    }
    result.resize(static_cast<std::size_t>(written) - padding);
    return result;
}

/// 按 algorithm 解密已去 Base64 的 payload；key/salt
/// 只读，返回明文，失败直接抛异常。
std::string decrypt_payload(const std::string &payload, const std::string &key,
    const std::string *salt, CipherAlgorithm algorithm) {
    if (algorithm == CipherAlgorithm::AES_GCM) {
        return decrypt_gcm(payload, key, salt);
    }
    if (algorithm != CipherAlgorithm::LEGACY_CBC) {
        throw std::invalid_argument("Unsupported cipher algorithm");
    }
    if (payload.compare(0, kMagic.size(), kMagic) == 0) {
        throw std::invalid_argument(
            "AES-GCM ciphertext cannot be decrypted with Legacy CBC");
    }
    return legacy(payload, key, salt, Operation::Decrypt);
}

}  // namespace

std::string encrypt(const std::string &plaintext, const std::string &key,
    const std::string *salt, CipherAlgorithm algorithm) {
    switch (algorithm) {
        case CipherAlgorithm::LEGACY_CBC:
            return encode(legacy(plaintext, key, salt, Operation::Encrypt));
        case CipherAlgorithm::AES_GCM:
            return encode(encrypt_gcm(plaintext, key, salt));
        default:
            throw std::invalid_argument("Unsupported cipher algorithm");
    }
}

std::string decrypt(const std::string &ciphertext, const std::string &key,
    const std::string *salt) {
    const std::string payload = decode(ciphertext);
    const CipherAlgorithm algorithm =
        payload.compare(0, kMagic.size(), kMagic) == 0
            ? CipherAlgorithm::AES_GCM
            : CipherAlgorithm::LEGACY_CBC;
    return decrypt_payload(payload, key, salt, algorithm);
}

std::string decrypt(const std::string &ciphertext, const std::string &key,
    const std::string *salt, CipherAlgorithm algorithm) {
    return decrypt_payload(decode(ciphertext), key, salt, algorithm);
}

}  // namespace encryption
}  // namespace mental1104
