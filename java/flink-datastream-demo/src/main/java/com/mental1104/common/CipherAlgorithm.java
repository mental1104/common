package com.mental1104.common;

/** 选择 AES 实现；LEGACY_CBC 用于历史兼容，AES_GCM 用于新业务的认证加密。 */
public enum CipherAlgorithm {
  LEGACY_CBC,
  AES_GCM
}
