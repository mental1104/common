//! 与 Python 历史 CBC 及版本化 AES-256-GCM 互通的加解密工具；需启用 encryption feature。

use aes_gcm::{
    aead::{rand_core::RngCore, AeadInPlace, KeyInit, OsRng},
    aes::{
        cipher::{block_padding::Pkcs7, BlockDecryptMut, BlockEncryptMut, KeyIvInit},
        Aes128, Aes192, Aes256,
    },
    Aes256Gcm, Nonce, Tag,
};
use base64::{engine::general_purpose::STANDARD, Engine};

use crate::error::{MentalError, Result};

const MAGIC: &[u8] = b"mental1104:AES:";
const HEADER: &[u8] = b"mental1104:AES:\x01";
const DEFAULT_KEY: &[u8] = b"0ePThPnLaJcWFcRc";
const NONCE_SIZE: usize = 12;
const TAG_SIZE: usize = 16;

/// 选择 AES 算法；LegacyCbc 仅用于兼容，AesGcm 用于新业务。
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CipherAlgorithm {
    LegacyCbc,
    AesGcm,
}

/// 把 message 转为统一 InvalidInput 错误；返回值用于传播可预期的加密失败。
fn invalid(message: &str) -> MentalError {
    MentalError::InvalidInput(message.to_owned())
}

/// 按 key 和 iv 为 data 添加 PKCS#7 并加密；返回新密文，长度无效时返回错误。
fn encrypt_legacy(data: &[u8], key: &[u8], iv: &[u8]) -> Result<Vec<u8>> {
    let error = || invalid("invalid Legacy CBC key or IV length");
    Ok(match key.len() {
        16 => cbc::Encryptor::<Aes128>::new_from_slices(key, iv)
            .map_err(|_| error())?
            .encrypt_padded_vec_mut::<Pkcs7>(data),
        24 => cbc::Encryptor::<Aes192>::new_from_slices(key, iv)
            .map_err(|_| error())?
            .encrypt_padded_vec_mut::<Pkcs7>(data),
        32 => cbc::Encryptor::<Aes256>::new_from_slices(key, iv)
            .map_err(|_| error())?
            .encrypt_padded_vec_mut::<Pkcs7>(data),
        _ => return Err(error()),
    })
}

/// 按 key 和 iv 解密原始 CBC data；返回去除 Padding 的新明文，长度或 Padding 错误直接拒绝。
fn decrypt_legacy(data: &[u8], key: &[u8], iv: &[u8]) -> Result<Vec<u8>> {
    let error = || invalid("invalid Legacy CBC key, IV, ciphertext or padding");
    let result = match key.len() {
        16 => cbc::Decryptor::<Aes128>::new_from_slices(key, iv)
            .map_err(|_| error())?
            .decrypt_padded_vec_mut::<Pkcs7>(data),
        24 => cbc::Decryptor::<Aes192>::new_from_slices(key, iv)
            .map_err(|_| error())?
            .decrypt_padded_vec_mut::<Pkcs7>(data),
        32 => cbc::Decryptor::<Aes256>::new_from_slices(key, iv)
            .map_err(|_| error())?
            .decrypt_padded_vec_mut::<Pkcs7>(data),
        _ => return Err(error()),
    };
    result.map_err(|_| error())
}

/// 校验 key 恰好 32 字节及 salt 为 None，返回 GCM 实例；参数错误以 Result 报告。
fn gcm_cipher(key: Option<&[u8]>, salt: Option<&[u8]>) -> Result<Aes256Gcm> {
    if salt.is_some() {
        return Err(invalid("AES-GCM does not accept salt"));
    }
    let key = key.ok_or_else(|| invalid("AES-256-GCM requires an explicit 32-byte key"))?;
    Aes256Gcm::new_from_slice(key)
        .map_err(|_| invalid("AES-256-GCM requires an explicit 32-byte key"))
}

/// 用 key 加密 data，salt 必须 None；返回带版本头的原始字节，随机源或参数错误直接传播。
fn encrypt_gcm(data: &[u8], key: Option<&[u8]>, salt: Option<&[u8]>) -> Result<Vec<u8>> {
    let cipher = gcm_cipher(key, salt)?;
    let mut nonce = [0_u8; NONCE_SIZE];
    OsRng
        .try_fill_bytes(&mut nonce)
        .map_err(|_| invalid("secure nonce generation failed"))?;
    let mut encrypted = data.to_vec();
    let tag = cipher
        .encrypt_in_place_detached(Nonce::from_slice(&nonce), HEADER, &mut encrypted)
        .map_err(|_| invalid("AES-GCM encryption failed"))?;
    let mut result = Vec::with_capacity(HEADER.len() + NONCE_SIZE + TAG_SIZE + data.len());
    result.extend_from_slice(HEADER);
    result.extend_from_slice(&nonce);
    result.extend_from_slice(&tag);
    result.extend_from_slice(&encrypted);
    Ok(result)
}

/// 验证 payload 的头、版本与 Tag；key 必须提供、salt 必须 None，失败不返回任何明文。
fn decrypt_gcm(payload: &[u8], key: Option<&[u8]>, salt: Option<&[u8]>) -> Result<Vec<u8>> {
    if !payload.starts_with(MAGIC) || payload.len() < HEADER.len() {
        return Err(invalid("invalid or truncated AES-GCM header"));
    }
    if payload[MAGIC.len()] != 1 {
        return Err(invalid("unsupported AES-GCM ciphertext version"));
    }
    let nonce_end = HEADER.len() + NONCE_SIZE;
    let tag_end = nonce_end + TAG_SIZE;
    if payload.len() < tag_end {
        return Err(invalid("truncated AES-GCM ciphertext"));
    }
    let cipher = gcm_cipher(key, salt)?;
    let mut plain = payload[tag_end..].to_vec();
    // 认证失败直接终止；内部缓冲区不会作为成功结果泄漏。
    cipher
        .decrypt_in_place_detached(
            Nonce::from_slice(&payload[HEADER.len()..nonce_end]),
            &payload[..HEADER.len()],
            &mut plain,
            Tag::from_slice(&payload[nonce_end..tag_end]),
        )
        .map_err(|_| invalid("AES-GCM authentication failed"))?;
    Ok(plain)
}

/// 加密 plaintext，返回标准 Base64 bytes；所有输入均为只读借用。
///
/// key 为原始密钥，只有 LegacyCbc 允许 None 使用历史默认 Key；salt 为 CBC IV，
/// 必须显式提供。algorithm 为明确算法选择，AesGcm 要求显式 32 字节 key、None salt。
/// 参数或加密失败返回 MentalError::InvalidInput；不使用口令派生。
pub fn encrypt(
    plaintext: &str,
    key: Option<&[u8]>,
    salt: Option<&[u8]>,
    algorithm: CipherAlgorithm,
) -> Result<Vec<u8>> {
    let payload = match algorithm {
        CipherAlgorithm::LegacyCbc => {
            let key = key.unwrap_or(DEFAULT_KEY);
            let iv = salt
                .ok_or_else(|| invalid("Legacy CBC encryption requires an explicit salt (IV)"))?;
            encrypt_legacy(plaintext.as_bytes(), key, iv)?
        }
        CipherAlgorithm::AesGcm => encrypt_gcm(plaintext.as_bytes(), key, salt)?,
    };
    Ok(STANDARD.encode(payload).into_bytes())
}

/// 解密 Base64 ciphertext 并返回 UTF-8 String；key 语义同 encrypt，输入不修改。
/// salt 为 CBC IV，None 仅在历史解密时使用 Key；GCM 的 salt 必须为 None。
///
/// algorithm 为 None 时自动识别 Magic，也可以显式选择算法。版本、截断、认证、
/// Padding 或 UTF-8 错误均返回 MentalError::InvalidInput，已识别新格式不降级 CBC。
pub fn decrypt(
    ciphertext: &[u8],
    key: Option<&[u8]>,
    salt: Option<&[u8]>,
    algorithm: Option<CipherAlgorithm>,
) -> Result<String> {
    let payload = STANDARD
        .decode(ciphertext)
        .map_err(|_| invalid("invalid Base64 ciphertext"))?;
    let is_gcm = payload.starts_with(MAGIC);
    let selected = algorithm.unwrap_or(if is_gcm {
        CipherAlgorithm::AesGcm
    } else {
        CipherAlgorithm::LegacyCbc
    });
    let plain = match selected {
        CipherAlgorithm::AesGcm => decrypt_gcm(&payload, key, salt)?,
        CipherAlgorithm::LegacyCbc => {
            if is_gcm {
                return Err(invalid(
                    "AES-GCM ciphertext cannot be decrypted with Legacy CBC",
                ));
            }
            let key = key.unwrap_or(DEFAULT_KEY);
            decrypt_legacy(&payload, key, salt.unwrap_or(key))?
        }
    };
    String::from_utf8(plain).map_err(|_| invalid("plaintext is not valid UTF-8"))
}
