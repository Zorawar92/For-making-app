// SPDX-License-Identifier: GPL-3.0-or-later
//
// CloudStream app links and Stremio player links arriving as webOS launch parameters. Direct HTTP(S)
// streams can be played by the native webOS media pipeline; Stremio torrent links need its separate
// streaming server and cannot be played by this app.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace csweb::app {

struct DeepLink {
    enum class Kind { None, Repository, Search, Play, Unsupported } kind = Kind::None;
    std::string value;  // Repository: URL; Search: query; Play: HTTP(S) media URL; Unsupported: reason
    std::string title;
    std::map<std::string, std::string> headers;
    std::vector<std::pair<std::string, std::string>> subtitles;  // language, URL
    int64_t position_ms = 0;
};

// `params_json`: the launch parameters object ("{}" or empty when there are none). Recognizes the
// normal Stremio stremio:///player/<encoded-stream>/... link for URL streams as well as a plain
// HTTP(S) target/url supplied by a webOS hand-off.
DeepLink parse_deep_link(const std::string &params_json);

}  // namespace csweb::app
