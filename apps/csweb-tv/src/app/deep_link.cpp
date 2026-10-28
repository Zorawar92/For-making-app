// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/deep_link.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>

namespace csweb::app {
namespace {
using Json = nlohmann::json;

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string percent_decode(const std::string &s, bool plus_as_space) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            const int hi = hex_value(s[i + 1]), lo = hex_value(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += plus_as_space && s[i] == '+' ? ' ' : s[i];
    }
    return out;
}

bool starts_with_ci(const std::string &s, const char *prefix) {
    size_t i = 0;
    for (; prefix[i] && i < s.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    }
    return prefix[i] == '\0';
}

bool http_url(const std::string &url) {
    size_t start = 0;
    if (starts_with_ci(url, "https://")) start = 8;
    else if (starts_with_ci(url, "http://")) start = 7;
    else return false;
    const size_t end = url.find_first_of("/?#", start);
    const std::string authority = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (authority.empty()) return false;
    for (unsigned char c : authority) {
        if (c <= 0x20 || c == 0x7f) return false;
    }
    const size_t at = authority.rfind('@');
    const std::string host_port = authority.substr(at == std::string::npos ? 0 : at + 1);
    return !host_port.empty();
}

std::string first_string(const Json &j, std::initializer_list<const char *> names) {
    for (const char *name : names) {
        auto it = j.find(name);
        if (it != j.end() && it->is_string()) return it->get<std::string>();
    }
    return {};
}

bool valid_header_name(const std::string &s) {
    if (s.empty() || s.size() > 128) return false;
    for (unsigned char c : s) {
        if (!(std::isalnum(c) || c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' ||
              c == '*' || c == '+' || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' ||
              c == '|' || c == '~')) return false;
    }
    return true;
}

void copy_headers(const Json &object, std::map<std::string, std::string> *out) {
    if (!object.is_object() || !out) return;
    for (auto it = object.begin(); it != object.end() && out->size() < 32; ++it) {
        if (!it.value().is_string() || !valid_header_name(it.key())) continue;
        const std::string value = it.value().get<std::string>();
        if (value.size() > 8192 || value.find_first_of("\r\n\0", 0, 3) != std::string::npos) continue;
        out->emplace(it.key(), value);
    }
}

// Stremio Core encodes a Stream as standard base64 of a zlib stream. At the pinned Core revision the
// zlib encoder uses Compression::none(), so DEFLATE consists of stored blocks. Decode only that
// deliberately narrow format: reject any future/other compression rather than guessing.
bool decode_base64(const std::string &encoded, std::vector<uint8_t> *out) {
    if (!out || encoded.empty() || encoded.size() > 131072 || encoded.size() % 4 != 0) return false;
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    out->clear();
    out->reserve((encoded.size() / 4) * 3);
    for (size_t i = 0; i < encoded.size(); i += 4) {
        const int a = value(encoded[i]), b = value(encoded[i + 1]);
        const bool pad2 = encoded[i + 2] == '=', pad3 = encoded[i + 3] == '=';
        const int c = pad2 ? 0 : value(encoded[i + 2]);
        const int d = pad3 ? 0 : value(encoded[i + 3]);
        const bool last = i + 4 == encoded.size();
        if (a < 0 || b < 0 || c < 0 || d < 0 || (!last && (pad2 || pad3)) || (pad2 && !pad3)) return false;
        if (pad2 && (b & 0x0f) != 0) return false;
        if (pad3 && !pad2 && (c & 0x03) != 0) return false;
        const uint32_t bits = (static_cast<uint32_t>(a) << 18) | (static_cast<uint32_t>(b) << 12) |
                              (static_cast<uint32_t>(c) << 6) | static_cast<uint32_t>(d);
        out->push_back(static_cast<uint8_t>(bits >> 16));
        if (!pad2) out->push_back(static_cast<uint8_t>(bits >> 8));
        if (!pad3) out->push_back(static_cast<uint8_t>(bits));
    }
    return true;
}

bool inflate_stored_zlib(const std::vector<uint8_t> &in, std::string *out) {
    if (!out || in.size() < 6) return false;
    const unsigned cmf = in[0], flg = in[1];
    if ((cmf & 0x0f) != 8 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20) != 0) return false;
    size_t pos = 2;
    out->clear();
    out->reserve(std::min<size_t>(in.size() * 4, 65536));
    bool final = false;
    while (!final) {
        if (pos + 5 > in.size() - 4) return false;
        const uint8_t block = in[pos++];
        final = (block & 1) != 0;
        if (((block >> 1) & 3) != 0) return false;  // only DEFLATE stored blocks
        const uint16_t len = static_cast<uint16_t>(in[pos] | (static_cast<uint16_t>(in[pos + 1]) << 8));
        const uint16_t nlen = static_cast<uint16_t>(in[pos + 2] | (static_cast<uint16_t>(in[pos + 3]) << 8));
        pos += 4;
        if (static_cast<uint16_t>(~len) != nlen || pos + len > in.size() - 4 || out->size() + len > 65536) return false;
        out->append(reinterpret_cast<const char *>(in.data() + pos), len);
        pos += len;
    }
    if (pos + 4 != in.size()) return false;
    uint32_t a = 1, b = 0;
    for (unsigned char c : *out) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    const uint32_t expected = (static_cast<uint32_t>(in[pos]) << 24) | (static_cast<uint32_t>(in[pos + 1]) << 16) |
                              (static_cast<uint32_t>(in[pos + 2]) << 8) | in[pos + 3];
    return ((b << 16) | a) == expected;
}

bool decode_stremio_stream(const std::string &segment, std::string *json) {
    std::vector<uint8_t> compressed;
    std::string decoded;
    return decode_base64(segment, &compressed) && inflate_stored_zlib(compressed, &decoded) &&
           (json->assign(std::move(decoded)), true);
}

int64_t stremio_position_ms(const std::string &link) {
    const size_t q = link.find('?');
    if (q == std::string::npos) return 0;
    const std::string query = link.substr(q + 1);
    size_t p = 0;
    while (p <= query.size()) {
        const size_t end = query.find('&', p);
        const std::string item = query.substr(p, end == std::string::npos ? std::string::npos : end - p);
        const size_t eq = item.find('=');
        if (eq != std::string::npos && item.substr(0, eq) == "position") {
            const std::string raw = item.substr(eq + 1);
            if (raw.empty() || raw.size() > 12) return 0;
            int64_t seconds = 0;
            for (unsigned char c : raw) {
                if (!std::isdigit(c) || seconds > (std::numeric_limits<int64_t>::max() - (c - '0')) / 10) return 0;
                seconds = seconds * 10 + (c - '0');
            }
            return seconds <= 86400 * 365 ? seconds * 1000 : 0;
        }
        if (end == std::string::npos) break;
        p = end + 1;
    }
    return 0;
}

DeepLink parse_stremio_player(const std::string &link) {
    constexpr const char *prefix = "stremio:///player/";
    if (link.rfind(prefix, 0) != 0) return {};
    const size_t start = std::char_traits<char>::length(prefix);
    const size_t end = link.find_first_of("/?#", start);
    const std::string segment = percent_decode(link.substr(start, end == std::string::npos ? std::string::npos : end - start), false);
    std::string payload;
    if (!decode_stremio_stream(segment, &payload)) {
        return {DeepLink::Kind::Unsupported, "This Stremio player link uses an unsupported stream encoding."};
    }
    const Json stream = Json::parse(payload, nullptr, false);
    if (!stream.is_object()) return {DeepLink::Kind::Unsupported, "This Stremio player link is malformed."};
    if (stream.contains("infoHash") || (stream.contains("url") && stream["url"].is_string() &&
                                        starts_with_ci(stream["url"].get<std::string>(), "magnet:"))) {
        return {DeepLink::Kind::Unsupported,
                "Stremio torrent and magnet playback needs a Stremio Streaming Server, which this webOS app does not include."};
    }
    std::string url = first_string(stream, {"url", "webosUrl", "externalUrl"});
    if (!http_url(url)) {
        return {DeepLink::Kind::Unsupported,
                "This Stremio stream is not a direct HTTP(S) link. Add-on, torrent, and external-player streams need their own Stremio services."};
    }
    DeepLink result;
    result.kind = DeepLink::Kind::Play;
    result.value = std::move(url);
    result.title = first_string(stream, {"name", "title", "description"});
    result.position_ms = stremio_position_ms(link);
    auto hints = stream.find("behaviorHints");
    if (hints != stream.end() && hints->is_object()) {
        auto proxy = hints->find("proxyHeaders");
        if (proxy != hints->end() && proxy->is_object()) {
            auto request = proxy->find("request");
            if (request != proxy->end()) copy_headers(*request, &result.headers);
        }
    }
    auto subs = stream.find("subtitles");
    if (subs != stream.end() && subs->is_array()) {
        for (const auto &sub : *subs) {
            if (!sub.is_object() || result.subtitles.size() >= 64) continue;
            const std::string sub_url = first_string(sub, {"url"});
            if (http_url(sub_url)) result.subtitles.emplace_back(first_string(sub, {"lang", "language"}), sub_url);
        }
    }
    return result;
}

std::string url_decode(const std::string &s) { return percent_decode(s, true); }

}  // namespace

DeepLink parse_deep_link(const std::string &params_json) {
    const Json j = Json::parse(params_json.empty() ? "{}" : params_json, nullptr, false);
    if (!j.is_object()) return {};

    std::string str;
    for (const char *key : {"target", "url", "streamUrl"}) {
        auto it = j.find(key);
        if (it != j.end() && it->is_string()) {
            str = it->get<std::string>();
            if (!str.empty()) break;
        }
    }
    if (str.empty()) {
        auto stream = j.find("stream");
        if (stream != j.end() && stream->is_object()) {
            str = first_string(*stream, {"url", "webosUrl", "externalUrl"});
            if (http_url(str)) {
                DeepLink result;
                result.kind = DeepLink::Kind::Play;
                result.value = str;
                result.title = first_string(*stream, {"name", "title", "description"});
                if (auto hints = stream->find("behaviorHints"); hints != stream->end() && hints->is_object()) {
                    if (auto proxy = hints->find("proxyHeaders"); proxy != hints->end() && proxy->is_object()) {
                        if (auto request = proxy->find("request"); request != proxy->end()) copy_headers(*request, &result.headers);
                    }
                }
                if (auto subs = stream->find("subtitles"); subs != stream->end() && subs->is_array()) {
                    for (const auto &sub : *subs) {
                        if (!sub.is_object() || result.subtitles.size() >= 64) continue;
                        const std::string sub_url = first_string(sub, {"url"});
                        if (http_url(sub_url)) result.subtitles.emplace_back(first_string(sub, {"lang", "language"}), sub_url);
                    }
                }
                return result;
            }
        }
    }

    if (str.rfind("https://cs.repo", 0) == 0) {
        const size_t q = str.find('?');
        if (q == std::string::npos || q + 1 >= str.size()) return {};
        const std::string repo = "https://" + str.substr(q + 1);
        return http_url(repo) ? DeepLink{DeepLink::Kind::Repository, repo} : DeepLink{};
    }
    if (str.rfind("cloudstreamrepo://", 0) == 0) {
        const std::string repo = "https" + str.substr(std::string("cloudstreamrepo").size());
        return http_url(repo) ? DeepLink{DeepLink::Kind::Repository, repo} : DeepLink{};
    }
    if (str.rfind("cloudstreamsearch://", 0) == 0) {
        const std::string q = url_decode(str.substr(std::string("cloudstreamsearch://").size()));
        if (!q.empty()) return {DeepLink::Kind::Search, q};
        return {};
    }
    if (str.rfind("stremio:///player/", 0) == 0) return parse_stremio_player(str);
    if (starts_with_ci(str, "magnet:")) {
        return {DeepLink::Kind::Unsupported,
                "Torrent and magnet playback requires a Stremio Streaming Server; this app has no torrent server."};
    }
    if (http_url(str)) {
        DeepLink result;
        result.kind = DeepLink::Kind::Play;
        result.value = str;
        result.title = first_string(j, {"title", "name"});
        return result;
    }
    return {};
}

}  // namespace csweb::app
