#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "mental1104/encryption.h"

namespace {
using mental1104::encryption::CipherAlgorithm;
using mental1104::encryption::decrypt;
using mental1104::encryption::encrypt;

/// 通过公开入口串起显式密钥、GCM 加密和自动识别解密，资源由库内 RAII 释放。
TEST(EncryptionTest, MinimalGcmRoundTrip) {
    const std::string key = "0123456789abcdef0123456789abcdef";
    const std::string ciphertext =
        encrypt("secret", key, nullptr, CipherAlgorithm::AES_GCM);
    EXPECT_EQ(decrypt(ciphertext, key), "secret");
}

/// 默认调用、仅传 Key 及显式空指针均不能隐式使用 Key 作为新的 CBC IV。
TEST(EncryptionTest, CbcRequiresExplicitIv) {
    const std::string key = "0123456789abcdef";
    EXPECT_THROW(encrypt("secret"), std::invalid_argument);
    EXPECT_THROW(encrypt("secret", key), std::invalid_argument);
    EXPECT_THROW(encrypt("secret", key, nullptr, CipherAlgorithm::LEGACY_CBC),
        std::invalid_argument);
    const std::string empty_iv;
    EXPECT_THROW(encrypt("secret", key, &empty_iv), std::invalid_argument);
}

/// 显式 IV 保持旧 CBC 字节格式；固定 IV
/// 仅用于向量回归，真实加密应重新随机生成。
TEST(EncryptionTest, ExplicitCbcMatchesHistoricalVector) {
    const std::string iv = "abcdefghijklmnop";
    const std::string ciphertext = encrypt("secret", "0ePThPnLaJcWFcRc", &iv);
    EXPECT_EQ(ciphertext, "vIFLz+7UV9fFbJtsi/P9pw==");
    EXPECT_EQ(decrypt(ciphertext, "0ePThPnLaJcWFcRc", &iv), "secret");
}

/// 固定历史 CBC 与独立 GCM 向量可解密，防止参数收紧损害已有数据和跨语言格式。
TEST(EncryptionTest, DecryptsHistoricalAndGcmVectors) {
    EXPECT_EQ(decrypt("MUSltWQ6CEFtWbeYRymj+A=="), "Default test");
    const std::string key = "0123456789abcdef0123456789abcdef";
    const std::string fixture =
        "bWVudGFsMTEwNDpBRVM6AQABAgMEBQYHCAkKC5vD5fpUDmE02C64madn0BhO"
        "l9QPqjW+zca7BN8WqvF3g2tx4kg=";
    EXPECT_EQ(decrypt(fixture, key), "cross-language secret");
}

/// GCM 继续拒绝旧 salt、默认 Key、错误 Key 和有效 Base64 内的 Tag 篡改。
TEST(EncryptionTest, GcmPreservesAuthenticationAndSaltRules) {
    const std::string key = "0123456789abcdef0123456789abcdef";
    const std::string iv = "abcdefghijklmnop";
    const std::string empty_iv;
    const std::string ciphertext =
        encrypt("secret", key, nullptr, CipherAlgorithm::AES_GCM);
    EXPECT_NE(
        ciphertext, encrypt("secret", key, nullptr, CipherAlgorithm::AES_GCM));
    EXPECT_THROW(encrypt("secret", key, &iv, CipherAlgorithm::AES_GCM),
        std::invalid_argument);
    EXPECT_THROW(encrypt("secret", key, &empty_iv, CipherAlgorithm::AES_GCM),
        std::invalid_argument);
    EXPECT_THROW(encrypt("secret", "0ePThPnLaJcWFcRc", nullptr,
                     CipherAlgorithm::AES_GCM),
        std::invalid_argument);
    EXPECT_THROW(decrypt(ciphertext, std::string(32, 'x')), std::runtime_error);
    EXPECT_THROW(decrypt(ciphertext, key, &iv), std::invalid_argument);
    EXPECT_THROW(decrypt(ciphertext, key, nullptr, CipherAlgorithm::LEGACY_CBC),
        std::invalid_argument);
    std::string damaged = ciphertext;
    damaged[40] = damaged[40] == 'A' ? 'B' : 'A';
    EXPECT_THROW(decrypt(damaged, key), std::runtime_error);
}
}  // namespace
