// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "wnk/net/http.hpp"

#include <string>
#include <vector>

namespace csweb::settings {

struct DnsProvider {
    std::string id;
    std::string label;
    int cloudstream_dns_value; // CloudStream RequestsHelper's dns_key value for the plugin runtime
    std::string doh_url;       // empty for the TV's normal resolver
    std::string doh_resolve;   // CURLOPT_RESOLVE bootstrap mapping
};

inline constexpr const char *kDnsProvider = "csweb_dns_provider";
inline constexpr const char *kDefaultDnsProvider = "adguard";

const std::vector<DnsProvider> &dns_providers();
const DnsProvider &dns_provider(const std::string &id); // unknown IDs use AdGuard
int dns_provider_index(const std::string &id);
bool configure_dns_provider(wnk::net::HttpTransport *transport, const std::string &id,
                            std::string *error = nullptr);

} // namespace csweb::settings
