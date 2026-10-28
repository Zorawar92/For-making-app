// SPDX-License-Identifier: GPL-3.0-or-later
#define WNK_TEST_MAIN
#include "wnk_test.hpp"

#include "settings/dns_provider.hpp"

using namespace csweb::settings;

namespace {
class FakeTransport final : public wnk::net::HttpTransport {
 public:
    wnk::net::HttpResponse perform(const wnk::net::HttpRequest &, const wnk::async::CancellationToken &) override {
        return {};
    }
    std::string describe() const override { return "fake"; }
    bool configure_doh(const std::string &u, const std::string &r, std::string *error) override {
        url = u;
        resolve = r;
        if (error) error->clear();
        return true;
    }
    std::string url, resolve;
};
} // namespace

TEST("adguard_is_the_default_dns_provider") {
    CHECK_EQ(std::string(kDefaultDnsProvider), std::string("adguard"));
    CHECK_EQ(dns_provider("unknown").id, std::string(kDefaultDnsProvider));
    CHECK_EQ(dns_provider_index(kDefaultDnsProvider), 0);
    CHECK_EQ(dns_provider(kDefaultDnsProvider).cloudstream_dns_value, 4);
    CHECK(dns_provider(kDefaultDnsProvider).doh_url == "https://dns.adguard.com/dns-query");
    CHECK_EQ(dns_provider("system").cloudstream_dns_value, 0);
}

TEST("dns_choices_have_https_and_bootstrap_for_named_resolvers") {
    CHECK(dns_providers().size() >= size_t(5));
    for (const auto &provider : dns_providers()) {
        if (provider.id == "system") {
            CHECK(provider.doh_url.empty());
            CHECK(provider.doh_resolve.empty());
        } else {
            CHECK(provider.doh_url.rfind("https://", 0) == 0);
            CHECK(provider.doh_resolve.find(":443:") != std::string::npos);
        }
    }
}

TEST("selected_dns_provider_is_applied_to_transport") {
    FakeTransport transport;
    std::string error = "stale";
    CHECK(configure_dns_provider(&transport, "quad9", &error));
    CHECK(transport.url == "https://dns.quad9.net/dns-query");
    CHECK(transport.resolve == "dns.quad9.net:443:9.9.9.9,149.112.112.112");
    CHECK(error.empty());

    CHECK(configure_dns_provider(&transport, "system", &error));
    CHECK(transport.url.empty());
    CHECK(transport.resolve.empty());
}
