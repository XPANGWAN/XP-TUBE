#include "xp/net/curl_transport.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include "xp/net/certificate.hpp"

namespace xp::net {
namespace {

std::once_flag gCurlInit;
std::mutex gShareMutex;
CURLSH* gCurlShare = nullptr;
std::mutex gPoolMutex;

void lockShare(CURL*, curl_lock_data, curl_lock_access, void*) { gShareMutex.lock(); }
void unlockShare(CURL*, curl_lock_data, void*) { gShareMutex.unlock(); }

struct HandlePool {
    std::vector<CURL*> handles;
    ~HandlePool() {
        for (CURL* handle : handles) curl_easy_cleanup(handle);
    }
};
HandlePool gHandles;

class HandleLease {
public:
    HandleLease() {
        {
            std::lock_guard<std::mutex> lock(gPoolMutex);
            if (!gHandles.handles.empty()) {
                handle_ = gHandles.handles.back();
                gHandles.handles.pop_back();
            }
        }
        if (handle_ == nullptr) {
            handle_ = curl_easy_init();
        } else {
            curl_easy_reset(handle_);
        }
    }
    ~HandleLease() {
        if (handle_ != nullptr) {
            std::lock_guard<std::mutex> lock(gPoolMutex);
            gHandles.handles.push_back(handle_);
        }
    }
    HandleLease(const HandleLease&) = delete;
    HandleLease& operator=(const HandleLease&) = delete;
    CURL* get() const noexcept { return handle_; }

private:
    CURL* handle_{nullptr};
};

void initializeCurl() {
    std::call_once(gCurlInit, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        // Share DNS only. Sharing TLS sessions or live connections caused
        // intermittent handshake stalls on the PS4 in the previous backend.
        gCurlShare = curl_share_init();
        if (gCurlShare != nullptr) {
            curl_share_setopt(gCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
            curl_share_setopt(gCurlShare, CURLSHOPT_LOCKFUNC, lockShare);
            curl_share_setopt(gCurlShare, CURLSHOPT_UNLOCKFUNC, unlockShare);
        }
    });
}

struct Transfer {
    std::string* body{nullptr};
    std::size_t body_limit{0};
    const CancelToken* cancel{nullptr};
    const HttpSink* sink{nullptr};
    const HttpProgress* progress{nullptr};
    const HttpStarted* started{nullptr};
    CURL* handle{nullptr};
    bool body_too_large{false};
    bool sink_refused{false};
};

std::size_t onWrite(char* data, std::size_t size, std::size_t count, void* opaque) {
    auto* transfer = static_cast<Transfer*>(opaque);
    if (size != 0 && count > static_cast<std::size_t>(-1) / size) return 0;
    const std::size_t bytes = size * count;
    if (transfer->sink != nullptr) {
        if (transfer->started != nullptr && *transfer->started) {
            long status = 0;
            curl_easy_getinfo(transfer->handle, CURLINFO_RESPONSE_CODE, &status);
            (*transfer->started)(status);
            transfer->started = nullptr;
        }
        if (!(*transfer->sink)(data, bytes)) {
            transfer->sink_refused = true;
            return 0;
        }
        return bytes;
    }
    if (transfer->body == nullptr || bytes > transfer->body_limit -
            std::min(transfer->body_limit, transfer->body->size())) {
        transfer->body_too_large = true;
        return 0;
    }
    transfer->body->append(data, bytes);
    return bytes;
}

int onProgress(void* opaque, curl_off_t total, curl_off_t received, curl_off_t, curl_off_t) {
    auto* transfer = static_cast<Transfer*>(opaque);
    if (transfer->progress != nullptr && *transfer->progress) {
        (*transfer->progress)({static_cast<long long>(received), static_cast<long long>(total)});
    }
    return transfer->cancel != nullptr && transfer->cancel->cancelled() ? 1 : 0;
}

void applyCommon(CURL* handle, const HttpConfig& config, const HttpRequest& request,
                 Transfer& transfer) {
    curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, request.timeout_ms);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, request.connect_timeout_ms);
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT,
                     std::max(1L, config.low_speed_limit_bytes_per_second));
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, std::max(1L, config.low_speed_seconds));
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(handle, CURLOPT_SHARE, gCurlShare);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
#ifdef PLAYTUBE_PS4
    // PS4 networking: avoid stalled IPv6 attempts and Linux TCP keepalive option values.
    curl_easy_setopt(handle, CURLOPT_IPRESOLVE, static_cast<long>(CURL_IPRESOLVE_V4));
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 0L);
#endif
    const std::string& ca = request.tls.ca_file.empty() ? config.ca_file : request.tls.ca_file;
    if (!ca.empty()) curl_easy_setopt(handle, CURLOPT_CAINFO, ca.c_str());

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, onWrite);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, onProgress);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, std::max(0L, config.max_redirects));
    const bool secure = request.url.rfind("https://", 0) == 0;
    const char* redirectProtocols =
        secure && config.reject_https_downgrade ? "https" : "http,https";
    curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, redirectProtocols);
    curl_easy_setopt(handle, CURLOPT_BUFFERSIZE,
                     transfer.sink != nullptr ? 512L * 1024L : static_cast<long>(CURL_MAX_WRITE_SIZE));
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
}

std::string trimAscii(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
    return std::string(value);
}

core::Error curlError(CURLcode code, const Transfer& transfer) {
    if (code == CURLE_ABORTED_BY_CALLBACK) {
        return core::Error::make(core::ErrorCode::cancelled, "request cancelled");
    }
    if (code == CURLE_WRITE_ERROR) {
        if (transfer.sink_refused) {
            return core::Error::make(core::ErrorCode::storage, "the download sink refused data");
        }
        if (transfer.body_too_large) {
            return core::Error::make(core::ErrorCode::bad_response, "the response exceeded its size limit");
        }
    }
    if (code == CURLE_OPERATION_TIMEDOUT) {
        return core::Error::make(core::ErrorCode::timeout, "the server did not answer in time");
    }
    if (code == CURLE_PEER_FAILED_VERIFICATION) {
        return core::Error::make(core::ErrorCode::certificate_untrusted, curl_easy_strerror(code));
    }
    if (code == CURLE_SSL_PINNEDPUBKEYNOTMATCH || code == CURLE_SSL_CONNECT_ERROR ||
        code == CURLE_SSL_CERTPROBLEM || code == CURLE_SSL_CIPHER ||
        code == CURLE_SSL_CACERT_BADFILE) {
        return core::Error::make(core::ErrorCode::tls, curl_easy_strerror(code));
    }
    return core::Error::make(core::ErrorCode::network, curl_easy_strerror(code));
}

} // namespace

CurlHttpTransport::CurlHttpTransport(HttpConfig config, core::Logger* logger)
    : config_(std::move(config)), logger_(logger) {
    initializeCurl();
}

core::Result<HttpResponse> CurlHttpTransport::send(const HttpRequest& request,
                                                    const CancelToken& cancel) const {
    return perform(request, cancel, nullptr, nullptr, nullptr);
}

core::Result<HttpResponse> CurlHttpTransport::download(const HttpRequest& request, HttpSink sink,
                                                        HttpProgress progress, const CancelToken& cancel,
                                                        HttpStarted started) const {
    if (!sink) {
        return core::Result<HttpResponse>::failure(
            core::Error::make(core::ErrorCode::invalid_argument, "download sink is required"));
    }
    return perform(request, cancel, &sink, &progress, &started);
}

core::Result<HttpResponse> CurlHttpTransport::perform(const HttpRequest& request, const CancelToken& cancel,
                                                       const HttpSink* sink, const HttpProgress* progress,
                                                       const HttpStarted* started) const {
    if (cancel.cancelled()) {
        return core::Result<HttpResponse>::failure(
            core::Error::make(core::ErrorCode::cancelled, "request cancelled"));
    }
    if (request.url.empty() ||
        (request.url.rfind("https://", 0) != 0 && request.url.rfind("http://", 0) != 0)) {
        return core::Result<HttpResponse>::failure(
            core::Error::make(core::ErrorCode::invalid_argument, "only absolute HTTP(S) URLs are supported"));
    }
    if (request.tls.mode == TlsMode::inspect_peer_certificate) {
        return core::Result<HttpResponse>::failure(
            core::Error::make(core::ErrorCode::invalid_argument,
                              "use fetch_peer_certificate for informational certificate inspection"));
    }
    if (request.tls.mode == TlsMode::verify_pinned_public_key && request.tls.public_key_pin.empty()) {
        return core::Result<HttpResponse>::failure(
            core::Error::make(core::ErrorCode::invalid_argument, "public-key pin is required"));
    }
    if (request.tls.mode != TlsMode::verify_pinned_public_key && !request.tls.public_key_pin.empty()) {
        return core::Result<HttpResponse>::failure(
            core::Error::make(core::ErrorCode::invalid_argument,
                              "public-key pin requires verify_pinned_public_key mode"));
    }

    HandleLease lease;
    CURL* handle = lease.get();
    if (handle == nullptr) {
        return core::Result<HttpResponse>::failure(
            core::Error::make(core::ErrorCode::unavailable, "libcurl could not create a request handle"));
    }

    HttpResponse response;
    Transfer transfer;
    transfer.body = &response.body;
    transfer.body_limit = request.max_body_bytes == 0 ? config_.max_body_bytes : request.max_body_bytes;
    transfer.cancel = &cancel;
    transfer.sink = sink;
    transfer.progress = progress;
    transfer.started = started;
    transfer.handle = handle;
    applyCommon(handle, config_, request, transfer);

    if (request.tls.mode == TlsMode::verify_pinned_public_key) {
        curl_easy_setopt(handle, CURLOPT_PINNEDPUBLICKEY, request.tls.public_key_pin.c_str());
    }

    if (request.method == "POST") {
        curl_easy_setopt(handle, CURLOPT_POST, 1L);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    } else if (request.method != "GET") {
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request.method.c_str());
        if (!request.body.empty()) {
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        }
    }

    curl_slist* headers = nullptr;
    for (const auto& header : request.headers) {
        const std::string line = header.first + ": " + header.second;
        headers = curl_slist_append(headers, line.c_str());
    }
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    const CURLcode code = curl_easy_perform(handle);
    curl_slist_free_all(headers);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status);
    char* contentType = nullptr;
    curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &contentType);
    if (contentType != nullptr) response.content_type = contentType;

    if (code != CURLE_OK) {
        core::Error error = curlError(code, transfer);
        if (logger_ != nullptr) logger_->write(core::LogLevel::warn, request.method + " " + request.url + ": " + error.message);
        return core::Result<HttpResponse>::failure(std::move(error));
    }
    return core::Result<HttpResponse>::success(std::move(response));
}

core::Result<PeerCertificate> CurlHttpTransport::fetch_peer_certificate(
    const std::string& https_url, const CancelToken& cancel) const {
    if (https_url.rfind("https://", 0) != 0) {
        return core::Result<PeerCertificate>::failure(
            core::Error::make(core::ErrorCode::invalid_argument,
                              "certificate inspection requires an HTTPS URL"));
    }
    if (cancel.cancelled()) {
        return core::Result<PeerCertificate>::failure(
            core::Error::make(core::ErrorCode::cancelled, "request cancelled"));
    }

    HandleLease lease;
    CURL* handle = lease.get();
    if (handle == nullptr) {
        return core::Result<PeerCertificate>::failure(
            core::Error::make(core::ErrorCode::unavailable, "libcurl could not create a request handle"));
    }

    std::string discarded;
    Transfer transfer;
    transfer.body = &discarded;
    transfer.body_limit = 64 * 1024;
    transfer.cancel = &cancel;
    transfer.handle = handle;

    HttpRequest request;
    request.url = https_url;
    request.timeout_ms = 10000;
    request.connect_timeout_ms = 5000;
    applyCommon(handle, config_, request, transfer);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(handle, CURLOPT_CERTINFO, 1L);

    const CURLcode code = curl_easy_perform(handle);
    if (code == CURLE_ABORTED_BY_CALLBACK) {
        return core::Result<PeerCertificate>::failure(
            core::Error::make(core::ErrorCode::cancelled, "request cancelled"));
    }

    curl_certinfo* info = nullptr;
    curl_easy_getinfo(handle, CURLINFO_CERTINFO, &info);
    if (info == nullptr || info->num_of_certs < 1) {
        return core::Result<PeerCertificate>::failure(
            core::Error::make(core::ErrorCode::tls,
                              code == CURLE_OK ? "no peer certificate received" : curl_easy_strerror(code)));
    }

    PeerCertificate certificate;
    std::string pem;
    for (curl_slist* entry = info->certinfo[0]; entry != nullptr; entry = entry->next) {
        const std::string_view line(entry->data);
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) continue;
        const std::string_view name = line.substr(0, colon);
        const std::string_view value = line.substr(colon + 1);
        if (name == "Cert") {
            pem = std::string(value);
        } else if (name == "Subject") {
            certificate.subject = trimAscii(value);
        } else if (name == "Issuer") {
            certificate.issuer = trimAscii(value);
        } else if (name == "Expire date") {
            certificate.not_after = trimAscii(value);
        }
    }

    const auto digest = spki_sha256_hex_from_pem(pem);
    if (!digest) {
        return core::Result<PeerCertificate>::failure(
            core::Error::make(core::ErrorCode::tls, "cannot read the peer certificate public key"));
    }
    certificate.spki_sha256_hex = *digest;
    return core::Result<PeerCertificate>::success(std::move(certificate));
}

} // namespace xp::net
