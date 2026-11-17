package com.mental1104.common;

import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.Base64;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** 无状态 AES 工具，兼容 Python 历史 CBC 和统一的版本化 AES-256-GCM 格式。 */
public final class Encryption {
  private static final byte[] MAGIC = "mental1104:AES:".getBytes(StandardCharsets.US_ASCII);
  private static final byte[] HEADER = "mental1104:AES:\u0001".getBytes(StandardCharsets.US_ASCII);
  private static final byte[] DEFAULT_KEY = "0ePThPnLaJcWFcRc".getBytes(StandardCharsets.UTF_8);
  private static final SecureRandom RANDOM = new SecureRandom();
  private static final int NONCE_SIZE = 12;
  private static final int TAG_SIZE = 16;

  /** 静态工具不需要实例或资源生命周期。 */
  private Encryption() { }

  /**
   * 初始化选定算法；CBC 加密必须显式提供 IV，解密保留历史默认 IV。
   * @param key 原始密钥，CBC 的 null 使用默认 Key。
   * @param salt CBC 加密必须提供的 IV，解密 null 使用 Key；GCM 必须为 null。
   * @param algorithm 明确选择的算法，不允许 null。
   * @param operation Cipher.ENCRYPT_MODE 或 Cipher.DECRYPT_MODE。
   * @param nonce GCM 的 12 字节 Nonce；CBC 时忽略。
   * @return 已配置且未消费数据的 Cipher。
   * @throws GeneralSecurityException 密钥、IV 或底层加密配置无效。
   */
  private static Cipher cipher(byte[] key, byte[] salt, CipherAlgorithm algorithm,
      int operation, byte[] nonce) throws GeneralSecurityException {
    if (algorithm == CipherAlgorithm.AES_GCM) {
      if (salt != null || key == null || key.length != 32) {
        throw new GeneralSecurityException("AES-256-GCM requires an explicit 32-byte key and null salt");
      }
      Cipher result = Cipher.getInstance("AES/GCM/NoPadding");
      result.init(operation, new SecretKeySpec(key, "AES"), new GCMParameterSpec(TAG_SIZE * 8, nonce));
      result.updateAAD(HEADER);
      return result;
    }
    if (algorithm != CipherAlgorithm.LEGACY_CBC) {
      throw new GeneralSecurityException("Unsupported cipher algorithm");
    }
    if (operation == Cipher.ENCRYPT_MODE && salt == null) {
      throw new GeneralSecurityException("Legacy CBC encryption requires an explicit salt (IV)");
    }
    byte[] actualKey = key == null ? DEFAULT_KEY : key;
    Cipher result = Cipher.getInstance("AES/CBC/PKCS5Padding");
    // AES 的 PKCS5Padding 名称在 JCE 中采用 16 字节块，线格式等同 PKCS#7。
    result.init(operation, new SecretKeySpec(actualKey, "AES"),
        new IvParameterSpec(salt == null ? actualKey : salt));
    return result;
  }

  /**
   * 拒绝未提供 IV 的旧式加密调用；调用方需改用带显式 salt 或选择 GCM 的重载。
   * @param plaintext UTF-8 明文字符串。
   * @return 不返回结果，此重载始终抛出异常。
   * @throws GeneralSecurityException 缺少显式 CBC IV。
   */
  public static byte[] encrypt(String plaintext) throws GeneralSecurityException {
    return encrypt(plaintext, null, null, CipherAlgorithm.LEGACY_CBC);
  }

  /**
   * 加密文本；GCM 使用随机 Nonce 和经过认证的版本头，输入数组不会被修改。
   * @param plaintext UTF-8 明文字符串。
   * @param key 原始密钥；CBC 的 null 使用默认 Key，GCM 必须显式提供 32 字节。
   * @param salt CBC 必须显式提供 16 字节 IV；GCM 必须为 null。
   * @param algorithm 明确的算法选择。
   * @return 标准 Base64 bytes；GCM 格式与其他语言实现一致。
   * @throws GeneralSecurityException 参数无效、随机源或加密失败。
   */
  public static byte[] encrypt(String plaintext, byte[] key, byte[] salt,
      CipherAlgorithm algorithm) throws GeneralSecurityException {
    byte[] nonce = new byte[NONCE_SIZE];
    if (algorithm == CipherAlgorithm.AES_GCM) {
      RANDOM.nextBytes(nonce);
    }
    byte[] encrypted = cipher(key, salt, algorithm, Cipher.ENCRYPT_MODE, nonce)
        .doFinal(plaintext.getBytes(StandardCharsets.UTF_8));
    if (algorithm == CipherAlgorithm.LEGACY_CBC) {
      return Base64.getEncoder().encode(encrypted);
    }
    int size = encrypted.length - TAG_SIZE;
    ByteBuffer payload = ByteBuffer.allocate(HEADER.length + NONCE_SIZE + encrypted.length);
    payload.put(HEADER).put(nonce).put(encrypted, size, TAG_SIZE).put(encrypted, 0, size);
    return Base64.getEncoder().encode(payload.array());
  }

  /**
   * 使用默认 Key 自动识别密文；带 GCM 头的数据仍要求显式密钥，因此会拒绝。
   * @param ciphertext 标准 Base64 bytes。
   * @return UTF-8 明文字符串。
   * @throws GeneralSecurityException 参数、格式、认证或解密失败。
   */
  public static String decrypt(byte[] ciphertext) throws GeneralSecurityException {
    return decrypt(ciphertext, null, null, null);
  }

  /**
   * 自动识别新旧密文或显式选择算法；新格式的版本与认证失败均不会降级 CBC。
   * @param ciphertext 标准 Base64 bytes。
   * @param key 原始密钥；CBC 的 null 使用默认 Key，GCM 必须显式提供 32 字节。
   * @param salt CBC 的 IV；null 使用 Key。GCM 必须为 null。
   * @param algorithm null 表示自动识别，或明确选择一种算法。
   * @return 完成认证并按 UTF-8 解码的明文；输入数组不会被修改。
   * @throws GeneralSecurityException 参数、封装、版本、认证或 UTF-8 解码失败。
   * @throws IllegalArgumentException Base64 编码无效。
   */
  public static String decrypt(byte[] ciphertext, byte[] key, byte[] salt,
      CipherAlgorithm algorithm) throws GeneralSecurityException {
    byte[] payload = Base64.getDecoder().decode(ciphertext);
    boolean isGcm = payload.length >= MAGIC.length
        && Arrays.equals(Arrays.copyOf(payload, MAGIC.length), MAGIC);
    CipherAlgorithm selected = algorithm == null
        ? (isGcm ? CipherAlgorithm.AES_GCM : CipherAlgorithm.LEGACY_CBC) : algorithm;
    byte[] plain;
    if (selected == CipherAlgorithm.LEGACY_CBC) {
      if (isGcm) {
        throw new GeneralSecurityException("AES-GCM ciphertext cannot be decrypted with Legacy CBC");
      }
      plain = cipher(key, salt, selected, Cipher.DECRYPT_MODE, null).doFinal(payload);
    } else if (selected == CipherAlgorithm.AES_GCM) {
      if (!isGcm || payload.length < HEADER.length) {
        throw new GeneralSecurityException("Invalid or truncated AES-GCM header");
      }
      if (payload[MAGIC.length] != 1) {
        throw new GeneralSecurityException("Unsupported AES-GCM ciphertext version");
      }
      int nonceEnd = HEADER.length + NONCE_SIZE;
      int tagEnd = nonceEnd + TAG_SIZE;
      if (payload.length < tagEnd) {
        throw new GeneralSecurityException("Truncated AES-GCM ciphertext");
      }
      byte[] nonce = Arrays.copyOfRange(payload, HEADER.length, nonceEnd);
      ByteBuffer sealed = ByteBuffer.allocate(payload.length - nonceEnd);
      sealed.put(payload, tagEnd, payload.length - tagEnd).put(payload, nonceEnd, TAG_SIZE);
      // doFinal 会先验证 Tag；认证异常直接传播，不能返回或降级处理明文。
      plain = cipher(key, salt, selected, Cipher.DECRYPT_MODE, nonce).doFinal(sealed.array());
    } else {
      throw new GeneralSecurityException("Unsupported cipher algorithm");
    }
    try {
      return StandardCharsets.UTF_8.newDecoder().decode(ByteBuffer.wrap(plain)).toString();
    } catch (CharacterCodingException error) {
      throw new GeneralSecurityException("Plaintext is not valid UTF-8", error);
    }
  }
}
