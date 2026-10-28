// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings/dns_provider.hpp"

#include <algorithm>

namespace csweb::settings {
namespace {
const std::vector<DnsProvider> kProviders = {
    {"adguard", "AdGuard DNS (default, non-filtering)", 4, "https://dns.adguard.com/dns-query",
     "dns.adguard.com:443:94.140.14.140,94.140.14.141"},
    {"cloudflare", "Cloudflare", 2, "https://cloudflare-dns.com/dns-query",
     "cloudflare-dns.com:443:1.1.1.1,1.0.0.1"},
    {"google", "Google Public DNS", 1, "https://dns.google/dns-query", "dns.google:443:8.8.8.8,8.8.4.4"},
    {"quad9", "Quad9", 6, "https://dns.quad9.net/dns-query", "dns.quad9.net:443:9.9.9.9,149.112.112.112"},
    {"dns-watch", "DNS.WATCH", 5, "https://resolver2.dns.watch/dns-query",
     "resolver2.dns.watch:443:84.200.69.80,84.200.70.40"},
    {"dns-sb", "DNS.SB", 7, "https://doh.dns.sb/dns-query", "doh.dns.sb:443:185.222.222.222,45.11.45.11"},
    {"canadian-shield", "Canadian Shield", 8, "https://private.canadianshield.cira.ca/dns-query",
     "private.canadianshield.cira.ca:443:149.112.121.10,149.112.122.10"},
    {"system", "System DNS (no DoH)", 0, "", ""},
};
} // namespace

const std::vector<DnsProvider> &dns_providers() { return kProviders; }

const DnsProvider &dns_provider(const std::string &id) {
    const auto it = std::find_if(kProviders.begin(), kProviders.end(), [&](const DnsProvider &p) { return p.id == id; });
    return it == kProviders.end() ? kProviders.front() : *it;
}

int dns_provider_index(const std::string &id) {
    const auto &providers = dns_providers();
    const auto it = std::find_if(providers.begin(), providers.end(), [&](const DnsProvider &p) { return p.id == id; });
    return it == providers.end() ? 0 : static_cast<int>(it - providers.begin());
}

bool configure_dns_provider(wnk::net::HttpTransport *transport, const std::string &id, std::string *error) {
    if (transport == nullptr) {
        if (error) *error = "HTTP transport is unavailable";
        return false;
    }
    const DnsProvider &provider = dns_provider(id);
    return transport->configure_doh(provider.doh_url, provider.doh_resolve, error);
}

} // namespace csweb::settings
