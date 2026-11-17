using System.Security.Cryptography;
using System.Text;

namespace Mental1104;

/// <summary>选择 AES 实现；LegacyCbc 仅用于兼容，AesGcm 用于新业务。</summary>
public enum CipherAlgorithm
{
    LegacyCbc,
    AesGcm
}

/// <summary>提供兼容 Python 历史 CBC 和版本化 AES-256-GCM 的无状态入口。</summary>
public static class Encryption
{
    private static readonly byte[] Magic = Encoding.ASCII.GetBytes("mental1104:AES:");
    private static readonly byte[] Header = Encoding.ASCII.GetBytes("mental1104:AES:\x01");
    private static readonly byte[] DefaultKey = Encoding.UTF8.GetBytes("0ePThPnLaJcWFcRc");
    private static readonly UTF8Encoding Utf8 = new(false, true);
    private const int NonceSize = 12;
    private const int TagSize = 16;

    /// <summary>校验 GCM 参数，返回只读借用的原始密钥。</summary>
    /// <param name="key">必须由调用方提供的 32 字节密钥。</param>
    /// <param name="salt">必须为 null，不能指定 Nonce。</param>
    /// <returns>通过校验的密钥；输入不会被修改。</returns>
    /// <exception cref="ArgumentException">密钥长度错误或 salt 非 null。</exception>
    private static byte[] GcmKey(byte[]? key, byte[]? salt)
    {
        if (salt is not null)
            throw new ArgumentException("AES-GCM does not accept salt", nameof(salt));
        if (key is null || key.Length != 32)
            throw new ArgumentException("AES-256-GCM requires an explicit 32-byte key", nameof(key));
        return key;
    }

    /// <summary>加密 UTF-8 文本，返回标准 Base64 字节。</summary>
    /// <param name="plaintext">明文字符串。</param>
    /// <param name="key">原始密钥；仅 CBC 允许 null 使用历史默认 Key。</param>
    /// <param name="salt">CBC 必须显式提供 16 字节 IV；GCM 仅允许 null。</param>
    /// <param name="algorithm">默认 LegacyCbc；新业务显式选择 AesGcm。</param>
    /// <returns>新分配的 Base64 ASCII bytes；不修改输入。</returns>
    /// <exception cref="ArgumentException">算法、密钥或 IV 不合法。</exception>
    /// <exception cref="CryptographicException">底层加密失败。</exception>
    public static byte[] Encrypt(string plaintext, byte[]? key = null, byte[]? salt = null,
        CipherAlgorithm algorithm = CipherAlgorithm.LegacyCbc)
    {
        byte[] plain = Utf8.GetBytes(plaintext);
        byte[] payload;
        switch (algorithm)
        {
            case CipherAlgorithm.LegacyCbc:
                if (salt is null)
                    throw new ArgumentException("Legacy CBC encryption requires an explicit salt (IV)", nameof(salt));
                using (Aes aes = Aes.Create())
                {
                    aes.Key = key ?? DefaultKey;
                    payload = aes.EncryptCbc(plain, salt, PaddingMode.PKCS7);
                }
                break;
            case CipherAlgorithm.AesGcm:
                using (AesGcm aes = new(GcmKey(key, salt), TagSize))
                {
                    byte[] nonce = RandomNumberGenerator.GetBytes(NonceSize);
                    payload = new byte[Header.Length + NonceSize + TagSize + plain.Length];
                    Header.CopyTo(payload, 0);
                    nonce.CopyTo(payload, Header.Length);
                    aes.Encrypt(nonce, plain, payload.AsSpan(Header.Length + NonceSize + TagSize),
                        payload.AsSpan(Header.Length + NonceSize, TagSize), Header);
                }
                break;
            default:
                throw new ArgumentException("Unsupported cipher algorithm", nameof(algorithm));
        }
        return Encoding.ASCII.GetBytes(Convert.ToBase64String(payload));
    }

    /// <summary>解密并认证版本化 GCM，或读取历史 CBC；新格式失败不降级。</summary>
    /// <param name="ciphertext">标准 Base64 ASCII bytes。</param>
    /// <param name="key">原始密钥；CBC 的 null 使用历史默认 Key，GCM 必须提供 32 字节。</param>
    /// <param name="salt">CBC 的 IV；null 使用 Key。GCM 仅允许 null。</param>
    /// <param name="algorithm">null 自动识别 Magic，或显式指定算法。</param>
    /// <returns>认证及 UTF-8 解码成功后的明文；不修改输入。</returns>
    /// <exception cref="ArgumentException">算法、密钥或 IV 不合法。</exception>
    /// <exception cref="FormatException">Base64、版本或封装无效。</exception>
    /// <exception cref="CryptographicException">GCM 认证或 CBC Padding 校验失败。</exception>
    /// <exception cref="DecoderFallbackException">明文不是有效 UTF-8。</exception>
    public static string Decrypt(byte[] ciphertext, byte[]? key = null, byte[]? salt = null,
        CipherAlgorithm? algorithm = null)
    {
        byte[] payload = Convert.FromBase64String(Encoding.ASCII.GetString(ciphertext));
        bool isGcm = payload.AsSpan().StartsWith(Magic);
        algorithm ??= isGcm ? CipherAlgorithm.AesGcm : CipherAlgorithm.LegacyCbc;
        byte[] plain;
        switch (algorithm)
        {
            case CipherAlgorithm.LegacyCbc:
                if (isGcm)
                    throw new FormatException("AES-GCM ciphertext cannot be decrypted with Legacy CBC");
                using (Aes aes = Aes.Create())
                {
                    aes.Key = key ?? DefaultKey;
                    plain = aes.DecryptCbc(payload, salt ?? aes.Key, PaddingMode.PKCS7);
                }
                break;
            case CipherAlgorithm.AesGcm:
                if (!isGcm || payload.Length < Header.Length)
                    throw new FormatException("Invalid or truncated AES-GCM header");
                if (payload[Magic.Length] != 1)
                    throw new FormatException("Unsupported AES-GCM ciphertext version");
                int nonceEnd = Header.Length + NonceSize;
                int tagEnd = nonceEnd + TagSize;
                if (payload.Length < tagEnd)
                    throw new FormatException("Truncated AES-GCM ciphertext");
                using (AesGcm aes = new(GcmKey(key, salt), TagSize))
                {
                    plain = new byte[payload.Length - tagEnd];
                    // 验证成功前不解码或返回明文；认证异常直接传播。
                    aes.Decrypt(payload.AsSpan(Header.Length, NonceSize), payload.AsSpan(tagEnd),
                        payload.AsSpan(nonceEnd, TagSize), plain, payload.AsSpan(0, Header.Length));
                }
                break;
            default:
                throw new ArgumentException("Unsupported cipher algorithm", nameof(algorithm));
        }
        return Utf8.GetString(plain);
    }
}
