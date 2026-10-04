#ifndef JKSHA256_H
#define JKSHA256_H

// Platform-neutral SHA-256 + CSPRNG (docs/68 W2a): absorbs the three BCrypt
// SHA-256 call sites (jkctl/jktriggers/JKDesktopShell command fingerprints)
// and jkbridge's BCryptGenRandom, so the fingerprint format ("sha256:" +
// 64 hex chars) survives the Linux port byte-for-byte. The digest function
// is pure C++; only RandomBytes has a platform impl. The hand-rolled SHA-1
// in jkbridge (WS accept key) stays as-is — protocol-specific, not shared.
#include <cstddef>
#include <string>

namespace jk {
namespace crypto {

// 64-char lowercase hex of SHA-256(data,len) — NO prefix; callers keep
// their own "sha256:" (byte-identical digests with the old BCrypt sites).
std::string Sha256Hex(const void* data, size_t len);

// Cryptographic random bytes. Windows: BCryptGenRandom (system preferred
// RNG). Other platforms: stage 2 — returns false for now (fail closed).
bool RandomBytes(void* buf, size_t len);

} // namespace crypto
} // namespace jk

#endif // JKSHA256_H