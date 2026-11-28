#include "xp/net/certificate.hpp"

#include <array>
#include <cstdint>
#include <string>

#ifdef XP_TUBE_PLATFORM_PS4
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#else
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <climits>
#endif

namespace xp::net {
namespace {

std::string hexLower(const std::array<unsigned char, 32>& digest) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (unsigned char byte : digest) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

} // namespace

std::optional<std::string> spki_sha256_hex_from_pem(std::string_view pem) {
    if (pem.empty() || pem.size() > 64 * 1024) return std::nullopt;

    std::array<unsigned char, 32> digest{};
#ifdef XP_TUBE_PLATFORM_PS4
    const std::string text(pem);
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt_init(&certificate);
    const int parsed = mbedtls_x509_crt_parse(
        &certificate, reinterpret_cast<const unsigned char*>(text.c_str()), text.size() + 1);
    if (parsed != 0) {
        mbedtls_x509_crt_free(&certificate);
        return std::nullopt;
    }
    std::array<unsigned char, 4096> der{};
    const int length = mbedtls_pk_write_pubkey_der(&certificate.pk, der.data(), der.size());
    mbedtls_x509_crt_free(&certificate);
    if (length <= 0) return std::nullopt;
    const std::size_t size = static_cast<std::size_t>(length);
    if (mbedtls_sha256(der.data() + der.size() - size, size, digest.data(), 0) != 0) {
        return std::nullopt;
    }
#else
    if (pem.size() > static_cast<std::size_t>(INT_MAX)) return std::nullopt;
    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    if (bio == nullptr) return std::nullopt;
    X509* certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (certificate == nullptr) return std::nullopt;

    unsigned char* der = nullptr;
    const int length = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(certificate), &der);
    X509_free(certificate);
    if (length <= 0 || der == nullptr) return std::nullopt;
    SHA256(der, static_cast<std::size_t>(length), digest.data());
    OPENSSL_free(der);
#endif
    return hexLower(digest);
}

} // namespace xp::net
