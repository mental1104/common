package com.mental1104.common;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import org.junit.jupiter.api.Test;

/** 验证显式 CBC IV、历史数据与版本化 GCM 的公开契约。 */
class EncryptionTest {
  /** 通过公开 API 展示最小 GCM 往返：准备密钥、加密、自动识别并验证明文。 */
  @Test
  void minimalGcmRoundTrip() throws GeneralSecurityException {
    byte[] key = "0123456789abcdef0123456789abcdef".getBytes(StandardCharsets.US_ASCII);
    byte[] ciphertext = Encryption.encrypt("secret", key, null, CipherAlgorithm.AES_GCM);
    assertEquals("secret", Encryption.decrypt(ciphertext, key, null, null));
  }

  /** 默认重载、显式 null 与空 IV 都不能再为新 CBC 加密隐式选择 Key。 */
  @Test
  void cbcRequiresExplicitIv() {
    byte[] key = "0123456789abcdef".getBytes(StandardCharsets.US_ASCII);
    assertThrows(GeneralSecurityException.class, () -> Encryption.encrypt("secret"));
    assertThrows(GeneralSecurityException.class,
        () -> Encryption.encrypt("secret", key, null, CipherAlgorithm.LEGACY_CBC));
    assertThrows(GeneralSecurityException.class,
        () -> Encryption.encrypt("secret", null, null, CipherAlgorithm.LEGACY_CBC));
    assertThrows(GeneralSecurityException.class,
        () -> Encryption.encrypt("secret", null, new byte[0], CipherAlgorithm.LEGACY_CBC));
  }

  /** 显式 IV 对照旧实现的固定向量；真实业务应每次生成新的随机 IV。 */
  @Test
  void explicitCbcMatchesHistoricalVector() throws GeneralSecurityException {
    byte[] iv = "abcdefghijklmnop".getBytes(StandardCharsets.US_ASCII);
    byte[] ciphertext = Encryption.encrypt("secret", null, iv, CipherAlgorithm.LEGACY_CBC);
    assertEquals("vIFLz+7UV9fFbJtsi/P9pw==", new String(ciphertext, StandardCharsets.US_ASCII));
    assertEquals("secret", Encryption.decrypt(ciphertext, null, iv, null));
  }

  /** 默认 IV 历史数据与独立 GCM 固定向量均应继续解密成功。 */
  @Test
  void decryptsHistoricalAndGcmVectors() throws GeneralSecurityException {
    assertEquals("Default test",
        Encryption.decrypt("MUSltWQ6CEFtWbeYRymj+A==".getBytes(StandardCharsets.US_ASCII)));
    byte[] key = "0123456789abcdef0123456789abcdef".getBytes(StandardCharsets.US_ASCII);
    byte[] fixture = ("bWVudGFsMTEwNDpBRVM6AQABAgMEBQYHCAkKC5vD5fpUDmE02C64madn0BhO"
        + "l9QPqjW+zca7BN8WqvF3g2tx4kg=").getBytes(StandardCharsets.US_ASCII);
    assertEquals("cross-language secret", Encryption.decrypt(fixture, key, null, null));
  }

  /** GCM 的随机性、拒绝旧 salt/默认 Key、错误 Key 与篡改检测不受 CBC 收紧影响。 */
  @Test
  void gcmPreservesAuthenticationAndSaltRules() throws GeneralSecurityException {
    byte[] key = "0123456789abcdef0123456789abcdef".getBytes(StandardCharsets.US_ASCII);
    byte[] ciphertext = Encryption.encrypt("secret", key, null, CipherAlgorithm.AES_GCM);
    assertNotEquals(new String(ciphertext, StandardCharsets.US_ASCII), new String(
        Encryption.encrypt("secret", key, null, CipherAlgorithm.AES_GCM), StandardCharsets.US_ASCII));
    for (byte[] iv : new byte[][] {new byte[0], "abcdefghijklmnop".getBytes(StandardCharsets.US_ASCII)}) {
      assertThrows(GeneralSecurityException.class,
          () -> Encryption.encrypt("secret", key, iv, CipherAlgorithm.AES_GCM));
      assertThrows(GeneralSecurityException.class, () -> Encryption.decrypt(ciphertext, key, iv, null));
    }
    assertThrows(GeneralSecurityException.class,
        () -> Encryption.encrypt("secret", null, null, CipherAlgorithm.AES_GCM));
    byte[] wrongKey = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx".getBytes(StandardCharsets.US_ASCII);
    assertThrows(GeneralSecurityException.class, () -> Encryption.decrypt(ciphertext, wrongKey, null, null));
    assertThrows(GeneralSecurityException.class,
        () -> Encryption.decrypt(ciphertext, key, null, CipherAlgorithm.LEGACY_CBC));
    ciphertext[40] = ciphertext[40] == 'A' ? (byte) 'B' : (byte) 'A';
    assertThrows(GeneralSecurityException.class, () -> Encryption.decrypt(ciphertext, key, null, null));
  }
}
