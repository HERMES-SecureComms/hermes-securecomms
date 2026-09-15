#include "hermes_security.h"
#ifdef ARDUINO
#include <bootloader_random.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <Ed25519.h>
#include <Crypto.h>
#else
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif
namespace hermes { namespace security { namespace crypto {
bool random(Bytes& out, size_t length) {
  out.resize(length);
#ifdef ARDUINO
  // This firmware never initializes ADC, Wi-Fi or Bluetooth. Entropy is required
  // during EACH call, because the second-stage bootloader disables it on exit.
  bootloader_random_enable();
  esp_fill_random(out.data(), out.size());
  bootloader_random_disable();
  return true;
#else
  return RAND_bytes(out.data(), static_cast<int>(out.size())) == 1;
#endif
}
bool hmacSha256(const Bytes& key, const Bytes& data, Bytes& mac) {
  mac.resize(32);
#ifdef ARDUINO
  const auto* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  return md && mbedtls_md_hmac(md, key.data(), key.size(), data.data(), data.size(), mac.data()) == 0;
#else
  unsigned int length = 0;
  return HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data.data(), data.size(), mac.data(), &length) && length == 32;
#endif
}
bool ed25519Verify(const Bytes& publicKey, const Bytes& data, const Bytes& signature) {
  if (publicKey.size() != 32 || signature.size() != 64) return false;
#ifdef ARDUINO
  return Ed25519::verify(signature.data(), publicKey.data(), data.data(), data.size());
#else
  EVP_PKEY* key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, publicKey.data(), publicKey.size());
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  bool valid = key && ctx && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1 &&
      EVP_DigestVerify(ctx, signature.data(), signature.size(), data.data(), data.size()) == 1;
  EVP_MD_CTX_free(ctx); EVP_PKEY_free(key);
  return valid;
#endif
}
bool equal(const Bytes& a, const Bytes& b) {
  if (a.size() != b.size()) return false;
  if (a.empty()) return true;
#ifdef ARDUINO
  return secure_compare(a.data(), b.data(), a.size());
#else
  return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
#endif
}
} } }
