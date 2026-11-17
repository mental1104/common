#pragma once

#include <string>

namespace mental1104 {
namespace encryption {

/// 选择 AES 算法；LEGACY_CBC 仅用于历史兼容，AES_GCM 用于新业务。
enum class CipherAlgorithm { LEGACY_CBC, AES_GCM };

/// 加密文本并返回标准 Base64；运行时实现需启用 M1104_ENABLE_ENCRYPTION。
/// @param plaintext UTF-8 明文；函数不会修改输入。
/// @param key 原始密钥字节；CBC 保留历史默认值，GCM 必须显式提供 32 字节。
/// @param salt CBC 必须显式提供 16 字节 IV；GCM 仅允许 nullptr。
/// @param algorithm 默认 LEGACY_CBC；新业务显式选择 AES_GCM。
/// @return 新分配的 Base64 ASCII 字符串，线格式与其他语言一致。
/// @throws std::invalid_argument 算法、密钥、IV 或输入长度不合法。
/// @throws std::runtime_error OpenSSL 加密或安全随机源失败。
std::string encrypt(const std::string &plaintext,
    const std::string &key = "0ePThPnLaJcWFcRc",
    const std::string *salt = nullptr,
    CipherAlgorithm algorithm = CipherAlgorithm::LEGACY_CBC);

/// 自动识别版本化 GCM，否则解密历史 CBC；已识别新格式失败时不降级。
/// @param ciphertext 标准 Base64 字符串。
/// @param key 原始密钥；CBC 保留默认值，GCM 必须显式提供 32 字节。
/// @param salt CBC 的 16 字节 IV，nullptr 使用 Key；GCM 仅允许 nullptr。
/// @return 解密后的 UTF-8 字节字符串；C++ 不负责 Unicode 编码校验。
/// @throws std::invalid_argument 格式、版本、密钥或 IV 无效。
/// @throws std::runtime_error 认证、Padding 校验或 OpenSSL 调用失败。
std::string decrypt(const std::string &ciphertext,
    const std::string &key = "0ePThPnLaJcWFcRc",
    const std::string *salt = nullptr);

/// 按明确算法解密；AES_GCM 拒绝缺失 Magic，LEGACY_CBC 拒绝已识别新格式。
/// @param ciphertext 标准 Base64 字符串。
/// @param key 原始密钥字节；GCM 必须显式提供 32 字节。
/// @param salt CBC 的 16 字节 IV，nullptr 使用 Key；GCM 仅允许 nullptr。
/// @param algorithm 明确的算法选择。
/// @return 解密后的 UTF-8 字节字符串；不修改任何输入。
/// @throws std::invalid_argument 格式、版本、算法或参数无效。
/// @throws std::runtime_error 认证、Padding 校验或 OpenSSL 调用失败。
std::string decrypt(const std::string &ciphertext, const std::string &key,
    const std::string *salt, CipherAlgorithm algorithm);

}  // namespace encryption
}  // namespace mental1104
