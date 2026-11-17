package mental1104

import "testing"

// TestEncryption_MinimalGcmRoundTrip 展示 t 所属场景的完整公开 API 往返，错误直接使测试失败。
func TestEncryption_MinimalGcmRoundTrip(t *testing.T) {
	key := []byte("0123456789abcdef0123456789abcdef")
	// GCM 不传 salt，由库生成并封装 Nonce。
	ciphertext, err := Encrypt("secret", key, nil, AESGCM)
	if err != nil {
		t.Fatal(err)
	}
	plaintext, err := Decrypt(ciphertext, key, nil)
	if err != nil {
		t.Fatal(err)
	}
	if plaintext != "secret" {
		t.Fatal("unexpected plaintext")
	}
}

// TestEncryption_CbcRequiresExplicitIv 用 t 验证 nil 和空 IV 不会被默认 Key 隐式替代。
func TestEncryption_CbcRequiresExplicitIv(t *testing.T) {
	for _, key := range [][]byte{nil, []byte("0123456789abcdef")} {
		for _, salt := range [][]byte{nil, {}} {
			if _, err := Encrypt("secret", key, salt); err == nil {
				t.Fatal("missing or empty CBC IV accepted")
			}
		}
		if _, err := Encrypt("secret", key, nil, LegacyCBC); err == nil {
			t.Fatal("explicit CBC accepted nil IV")
		}
	}
}

// TestEncryption_ExplicitCbcMatchesVector 用 t 对照历史字节向量；固定 IV 只用于回归。
func TestEncryption_ExplicitCbcMatchesVector(t *testing.T) {
	iv := []byte("abcdefghijklmnop")
	ciphertext, err := Encrypt("secret", nil, iv)
	if err != nil {
		t.Fatal(err)
	}
	if string(ciphertext) != "vIFLz+7UV9fFbJtsi/P9pw==" {
		t.Fatal("CBC wire format changed")
	}
	plaintext, err := Decrypt(ciphertext, nil, iv)
	if err != nil || plaintext != "secret" {
		t.Fatalf("CBC round trip failed: %v", err)
	}
}

// TestEncryption_HistoricalAndGcmVectors 用 t 验证旧默认 IV 解密及跨语言 GCM 固定向量。
func TestEncryption_HistoricalAndGcmVectors(t *testing.T) {
	plaintext, err := Decrypt([]byte("MUSltWQ6CEFtWbeYRymj+A=="), nil, nil)
	if err != nil || plaintext != "Default test" {
		t.Fatalf("historical decryption failed: %v", err)
	}
	fixture := []byte("bWVudGFsMTEwNDpBRVM6AQABAgMEBQYHCAkKC5vD5fpUDmE02C64madn0BhOl9QPqjW+zca7BN8WqvF3g2tx4kg=")
	plaintext, err = Decrypt(fixture, []byte("0123456789abcdef0123456789abcdef"), nil)
	if err != nil || plaintext != "cross-language secret" {
		t.Fatalf("GCM vector failed: %v", err)
	}
}

// TestEncryption_GcmRules 用 t 验证随机性、拒绝 salt/默认 Key、认证失败和禁止 CBC 降级。
func TestEncryption_GcmRules(t *testing.T) {
	key := []byte("0123456789abcdef0123456789abcdef")
	ciphertext, err := Encrypt("secret", key, nil, AESGCM)
	if err != nil {
		t.Fatal(err)
	}
	second, err := Encrypt("secret", key, nil, AESGCM)
	if err != nil || string(second) == string(ciphertext) {
		t.Fatal("GCM nonce was reused")
	}
	for _, iv := range [][]byte{{}, []byte("abcdefghijklmnop")} {
		if _, err := Encrypt("secret", key, iv, AESGCM); err == nil {
			t.Fatal("GCM accepted salt")
		}
		if _, err := Decrypt(ciphertext, key, iv); err == nil {
			t.Fatal("GCM decrypt accepted salt")
		}
	}
	if _, err := Encrypt("secret", nil, nil, AESGCM); err == nil {
		t.Fatal("GCM accepted default key")
	}
	if _, err := Decrypt(ciphertext, []byte("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"), nil); err == nil {
		t.Fatal("wrong key accepted")
	}
	if _, err := Decrypt(ciphertext, key, nil, LegacyCBC); err == nil {
		t.Fatal("CBC downgrade accepted")
	}
	if ciphertext[40] == 'A' {
		ciphertext[40] = 'B'
	} else {
		ciphertext[40] = 'A'
	}
	if _, err := Decrypt(ciphertext, key, nil); err == nil {
		t.Fatal("tampered tag accepted")
	}
}
