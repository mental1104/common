#![cfg(feature = "encryption")]
//! 验证公开加密入口、显式 CBC IV 和历史兼容；通过既有 cargo test 发现。

use mental1104::encryption::{decrypt, encrypt, CipherAlgorithm};

/// 公开 API 的最小完整用法：准备显式密钥，GCM 加密后默认识别解密。
#[test]
fn minimal_gcm_round_trip() {
    let key = b"0123456789abcdef0123456789abcdef";
    let ciphertext = encrypt("secret", Some(key), None, CipherAlgorithm::AesGcm).unwrap();
    assert_eq!(
        decrypt(&ciphertext, Some(key), None, None).unwrap(),
        "secret"
    );
}

/// None 和空 IV 均不能在新 CBC 加密中被 Key 隐式替代。
#[test]
fn cbc_requires_explicit_iv() {
    assert!(encrypt("secret", None, None, CipherAlgorithm::LegacyCbc).is_err());
    assert!(encrypt(
        "secret",
        Some(b"0123456789abcdef"),
        None,
        CipherAlgorithm::LegacyCbc
    )
    .is_err());
    assert!(encrypt("secret", None, Some(b""), CipherAlgorithm::LegacyCbc).is_err());
}

/// 固定显式 IV 保持历史线格式；真实业务每次加密应重新生成随机 IV。
#[test]
fn explicit_cbc_matches_historical_vector() {
    let iv = b"abcdefghijklmnop";
    let ciphertext = encrypt("secret", None, Some(iv), CipherAlgorithm::LegacyCbc).unwrap();
    assert_eq!(ciphertext, b"vIFLz+7UV9fFbJtsi/P9pw==");
    assert_eq!(
        decrypt(&ciphertext, None, Some(iv), None).unwrap(),
        "secret"
    );
}

/// 参数收紧不影响默认 IV 历史数据，也不改变其他语言使用的 GCM 封装。
#[test]
fn decrypts_historical_and_gcm_vectors() {
    assert_eq!(
        decrypt(b"MUSltWQ6CEFtWbeYRymj+A==", None, None, None).unwrap(),
        "Default test"
    );
    let key = b"0123456789abcdef0123456789abcdef";
    let fixture =
        b"bWVudGFsMTEwNDpBRVM6AQABAgMEBQYHCAkKC5vD5fpUDmE02C64madn0BhOl9QPqjW+zca7BN8WqvF3g2tx4kg=";
    assert_eq!(
        decrypt(fixture, Some(key), None, None).unwrap(),
        "cross-language secret"
    );
}

/// GCM 保持随机 Nonce、拒绝旧 salt、显式密钥和认证失败不降级的契约。
#[test]
fn gcm_preserves_authentication_and_salt_rules() {
    let key = b"0123456789abcdef0123456789abcdef";
    let mut ciphertext = encrypt("secret", Some(key), None, CipherAlgorithm::AesGcm).unwrap();
    assert_ne!(
        ciphertext,
        encrypt("secret", Some(key), None, CipherAlgorithm::AesGcm).unwrap()
    );
    for iv in [b"".as_slice(), b"abcdefghijklmnop".as_slice()] {
        assert!(encrypt("secret", Some(key), Some(iv), CipherAlgorithm::AesGcm).is_err());
        assert!(decrypt(&ciphertext, Some(key), Some(iv), None).is_err());
    }
    assert!(encrypt("secret", None, None, CipherAlgorithm::AesGcm).is_err());
    assert!(decrypt(
        &ciphertext,
        Some(b"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"),
        None,
        None
    )
    .is_err());
    assert!(decrypt(
        &ciphertext,
        Some(key),
        None,
        Some(CipherAlgorithm::LegacyCbc)
    )
    .is_err());
    ciphertext[40] = if ciphertext[40] == b'A' { b'B' } else { b'A' };
    assert!(decrypt(&ciphertext, Some(key), None, None).is_err());
}
