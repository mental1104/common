# Locale Fallback

- Normalize incoming locale to base language; treat hyphen/underscore equally.
- If normalized locale not supported, fall back to configured default.
- ChainResolver provides ordered lookup; last resort is default locale.
