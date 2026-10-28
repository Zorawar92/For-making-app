// SPDX-License-Identifier: GPL-3.0-or-later
// app/deep_link against CloudStream URLs and Stremio Core's player deep-link format.
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "app/deep_link.hpp"
#include <nlohmann/json.hpp>

using namespace csweb::app;

TEST("repository_links") {
    auto a = parse_deep_link(R"({"target":"cloudstreamrepo://example.org/repo.json"})");
    CHECK(a.kind == DeepLink::Kind::Repository);
    CHECK_EQ(a.value, std::string("https://example.org/repo.json"));
    auto b = parse_deep_link(R"({"url":"https://cs.repo/?example.org/r.json"})");
    CHECK(b.kind == DeepLink::Kind::Repository);
    CHECK_EQ(b.value, std::string("https://example.org/r.json"));
    CHECK(parse_deep_link(R"({"target":"https://cs.repo"})").kind == DeepLink::Kind::None);
}

TEST("search_link") {
    auto s = parse_deep_link(R"({"target":"cloudstreamsearch://pride%20and+prejudice"})");
    CHECK(s.kind == DeepLink::Kind::Search);
    CHECK_EQ(s.value, std::string("pride and prejudice"));
}

TEST("stremio_core_http_player_link") {
    // Stream JSON compressed with zlib Compression::none and standard-base64 encoded, as in
    // Stremio Core's Stream::encode. The player route carries encoded stream, transport URLs,
    // metadata IDs and an optional resume position.
    const std::string url =
        "stremio:///player/eAEBEQHu%2FnsidXJsIjoiaHR0cHM6Ly9jZG4uZXhhbXBsZS50ZXN0L21vdmllLm0zdTg%2FdG9rZW49YWJjIiwibmFtZSI6IlN0cmVtaW8gVGVzdCBNb3ZpZSIsInN1YnRpdGxlcyI6W3sibGFuZyI6Ikl0YWxpYW4iLCJ1cmwiOiJodHRwczovL2Nkbi5leGFtcGxlLnRlc3QvaXQudnR0In1dLCJiZWhhdmlvckhpbnRzIjp7InByb3h5SGVhZGVycyI6eyJyZXF1ZXN0Ijp7IlJlZmVyZXIiOiJodHRwczovL2NhdGFsb2cuZXhhbXBsZS50ZXN0LyIsIlVzZXItQWdlbnQiOiJTdHJlbWlvV2ViT1MifX19fepdX9Y%3D/http%3A%2F%2F127.0.0.1%3A11470%2F/https%3A%2F%2Fmeta.example/movie/tt123/tt123%3A1?position=120";
    const auto link = parse_deep_link(nlohmann::json({{"target", url}}).dump());
    CHECK(link.kind == DeepLink::Kind::Play);
    CHECK_EQ(link.value, std::string("https://cdn.example.test/movie.m3u8?token=abc"));
    CHECK_EQ(link.title, std::string("Stremio Test Movie"));
    CHECK_EQ(link.position_ms, int64_t(120000));
    CHECK_EQ(link.headers.at("Referer"), std::string("https://catalog.example.test/"));
    CHECK_EQ(link.headers.at("User-Agent"), std::string("StremioWebOS"));
    REQUIRE(link.subtitles.size() == 1);
    CHECK_EQ(link.subtitles[0].first, std::string("Italian"));
    CHECK_EQ(link.subtitles[0].second, std::string("https://cdn.example.test/it.vtt"));
}

TEST("direct_http_launch") {
    auto link = parse_deep_link(R"({"target":"https://cdn.example/movie.mp4?sig=abc","title":"Movie"})");
    CHECK(link.kind == DeepLink::Kind::Play);
    CHECK_EQ(link.value, std::string("https://cdn.example/movie.mp4?sig=abc"));
    CHECK_EQ(link.title, std::string("Movie"));
    CHECK(parse_deep_link(R"({"url":"http://media.example/live.m3u8"})").kind == DeepLink::Kind::Play);
    CHECK(parse_deep_link(R"({"target":"https:///missing-host"})").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link(R"({"target":"ftp://media.example/movie.mp4"})").kind == DeepLink::Kind::None);
}

TEST("unsupported_torrent_handoff") {
    CHECK(parse_deep_link(R"({"target":"magnet:?xt=urn:btih:0123456789012345678901234567890123456789"})").kind ==
          DeepLink::Kind::Unsupported);
}

TEST("ignored") {
    CHECK(parse_deep_link("").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link("{}").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link("not json").kind == DeepLink::Kind::None);
    CHECK(parse_deep_link(R"({"target":42})").kind == DeepLink::Kind::None);
}
