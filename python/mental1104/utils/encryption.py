import base64
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad, unpad



def encrypt(plaintext, key="0ePThPnLaJcWFcRc", salt=None):
    """        
    使用 AES CBC 模式加密明文。
    :param plaintext: 明文字符串
    :param key: 密钥，默认为 "0ePThPnLaJcWFcRc"
    :param salt: 盐值，默认为密钥
    :return: 加密后的密文，使用 base64 编码
    """
    if salt is None:
        salt = key

    key = bytes(key, encoding="utf-8")
    salt = bytes(salt, encoding="utf-8")
    aes = AES.new(key, mode=AES.MODE_CBC, IV=salt)

    padded_plaintext = pad(plaintext.encode('utf-8'), AES.block_size)
    encrypted = aes.encrypt(padded_plaintext)

    return base64.b64encode(encrypted)

def decrypt(ciphertext, key="0ePThPnLaJcWFcRc", salt=None):
    """
    使用 AES CBC 模式解密密文。
    :param ciphertext: 密文字符串，使用 base64 编码
    :param key: 密钥，默认为 "0ePThPnLaJcWFcRc"
    :param salt: 盐值，默认为密钥
    :return: 解密后的明文，使用 base64 编码
    """
    if salt is None:
        salt = key

    key = bytes(key, encoding="utf-8")
    salt = bytes(salt, encoding="utf-8")
    aes = AES.new(key, mode=AES.MODE_CBC, IV=salt)

    ciphertext = base64.b64decode(ciphertext)

    decrypted = aes.decrypt(ciphertext)
    unpadded_plaintext = unpad(decrypted, AES.block_size)

    return unpadded_plaintext.decode('utf-8')


def generate_salt(length=16):
    """
    生成指定长度的随机盐值。
    :param length: 盐值长度，默认为 16
    :return: 生成的随机盐值字符串
    例如：
        >>> salt = Encryption.generate_salt(16)
        >>> print(salt)
        'a1b2c3d4e5f6g7h8'
    """
    if length < 0:
        raise ValueError("Length must be non-negative")

    import secrets
    import string
    # 生成一个包含字母和数字的随机字符串
    characters = string.ascii_letters + string.digits
    salt = ''.join(secrets.choice(characters) for _ in range(length))
    return salt
