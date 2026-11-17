using System;
using System.Security.Cryptography;
using System.Text;
using Xunit;

namespace Mental1104.Tests;

/// <summary>验证公开加密入口、显式 CBC IV、历史解密和 GCM 认证边界。</summary>
public class EncryptionTests
{
    /// <summary>最小完整用法：准备密钥，GCM 加密后自动识别解密；库内资源自动释放。</summary>
    [Fact]
    public void MinimalGcmRoundTrip()
    {
        byte[] key = Encoding.ASCII.GetBytes("0123456789abcdef0123456789abcdef");
        byte[] ciphertext = Encryption.Encrypt("secret", key, algorithm: CipherAlgorithm.AesGcm);
        Assert.Equal("secret", Encryption.Decrypt(ciphertext, key));
    }

    /// <summary>默认调用、仅提供 Key、显式 null 和空 IV 均不能隐式复用 Key。</summary>
    [Fact]
    public void CbcRequiresExplicitIv()
    {
        byte[] key = Encoding.ASCII.GetBytes("0123456789abcdef");
        Assert.Throws<ArgumentException>(() => Encryption.Encrypt("secret"));
        Assert.Throws<ArgumentException>(() => Encryption.Encrypt("secret", key));
        Assert.Throws<ArgumentException>(() => Encryption.Encrypt("secret", key, null, CipherAlgorithm.LegacyCbc));
        Assert.Throws<ArgumentException>(() => Encryption.Encrypt("secret", salt: Array.Empty<byte>()));
    }

    /// <summary>显式 IV 保持历史 CBC 字节格式；固定 IV 仅用于回归向量。</summary>
    [Fact]
    public void ExplicitCbcMatchesHistoricalVector()
    {
        byte[] iv = Encoding.ASCII.GetBytes("abcdefghijklmnop");
        byte[] ciphertext = Encryption.Encrypt("secret", salt: iv);
        Assert.Equal("vIFLz+7UV9fFbJtsi/P9pw==", Encoding.ASCII.GetString(ciphertext));
        Assert.Equal("secret", Encryption.Decrypt(ciphertext, salt: iv));
    }

    /// <summary>默认 IV 的历史数据和独立 GCM 向量仍可通过公开入口解密。</summary>
    [Fact]
    public void DecryptsHistoricalAndGcmVectors()
    {
        Assert.Equal("Default test", Encryption.Decrypt(Encoding.ASCII.GetBytes("MUSltWQ6CEFtWbeYRymj+A==")));
        byte[] key = Encoding.ASCII.GetBytes("0123456789abcdef0123456789abcdef");
        byte[] fixture = Encoding.ASCII.GetBytes(
            "bWVudGFsMTEwNDpBRVM6AQABAgMEBQYHCAkKC5vD5fpUDmE02C64madn0BhOl9QPqjW+zca7BN8WqvF3g2tx4kg=");
        Assert.Equal("cross-language secret", Encryption.Decrypt(fixture, key));
    }

    /// <summary>GCM 保持随机 Nonce、拒绝 salt/默认 Key，以及认证失败不返回明文的契约。</summary>
    [Fact]
    public void GcmPreservesAuthenticationAndSaltRules()
    {
        byte[] key = Encoding.ASCII.GetBytes("0123456789abcdef0123456789abcdef");
        byte[] ciphertext = Encryption.Encrypt("secret", key, algorithm: CipherAlgorithm.AesGcm);
        Assert.NotEqual(Encoding.ASCII.GetString(ciphertext), Encoding.ASCII.GetString(
            Encryption.Encrypt("secret", key, algorithm: CipherAlgorithm.AesGcm)));
        foreach (byte[] iv in new[] { Array.Empty<byte>(), Encoding.ASCII.GetBytes("abcdefghijklmnop") })
        {
            Assert.Throws<ArgumentException>(() => Encryption.Encrypt("secret", key, iv, CipherAlgorithm.AesGcm));
            Assert.Throws<ArgumentException>(() => Encryption.Decrypt(ciphertext, key, iv));
        }
        Assert.Throws<ArgumentException>(() => Encryption.Encrypt("secret", algorithm: CipherAlgorithm.AesGcm));
        byte[] wrongKey = Encoding.ASCII.GetBytes("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
        Assert.ThrowsAny<CryptographicException>(() => Encryption.Decrypt(ciphertext, wrongKey));
        Assert.Throws<FormatException>(() => Encryption.Decrypt(ciphertext, key, algorithm: CipherAlgorithm.LegacyCbc));
        ciphertext[40] = ciphertext[40] == (byte)'A' ? (byte)'B' : (byte)'A';
        Assert.ThrowsAny<CryptographicException>(() => Encryption.Decrypt(ciphertext, key));
    }
}
