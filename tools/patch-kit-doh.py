#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply CSWeb's runtime DoH extension to the pinned, ignored Native App Kit checkout."""
from __future__ import annotations

import sys
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"patch-kit-doh: expected source anchor not found in {path}")
    path.write_text(text.replace(old, new, 1))


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} KIT_DIR", file=sys.stderr)
        return 2
    kit = Path(sys.argv[1]).resolve()
    header = kit / "net/include/wnk/net/http.hpp"
    source = kit / "net/src/curl_transport.cpp"
    if not header.is_file() or not source.is_file():
        raise SystemExit(f"patch-kit-doh: not a Native App Kit checkout: {kit}")

    replace_once(header,
        """    bool cookies = false;
};

// nullptr with *error set if libcurl.so.4 cannot be loaded.
""",
        """    bool cookies = false;
    // CSWEB DoH support: optional DNS-over-HTTPS endpoint and bootstrap CURLOPT_RESOLVE entry.
    std::string doh_url;
    std::string doh_resolve;
};

// nullptr with *error set if libcurl.so.4 cannot be loaded.
""")
    replace_once(header,
        """    virtual HttpResponse perform(const HttpRequest &req, const async::CancellationToken &token) = 0;
""",
        """    virtual HttpResponse perform(const HttpRequest &req, const async::CancellationToken &token) = 0;
    // CSWEB DoH support. Bootstrap entry uses CURLOPT_RESOLVE syntax, e.g. dns.example:443:192.0.2.1.
    virtual bool configure_doh(const std::string &url, const std::string &resolve_entry,
                               std::string *error = nullptr) {
        (void)url;
        (void)resolve_entry;
        if (error) *error = \"transport does not support DNS-over-HTTPS configuration\";
        return false;
    }
""")
    replace_once(source,
        """    ~CurlTransport() override {
        for (CURLM *m : idle_) api().multi_cleanup(m);
        if (share_ != nullptr && api().share_cleanup != nullptr) api().share_cleanup(share_);
    }

    std::string describe() const override { return api().version; }
""",
        """    ~CurlTransport() override {
        for (CURLM *m : idle_) api().multi_cleanup(m);
        if (share_ != nullptr && api().share_cleanup != nullptr) api().share_cleanup(share_);
    }

    // Probe runtime libcurl before changing the active setting: firmware can load older system curl.
    bool configure_doh(const std::string &url, const std::string &resolve_entry,
                       std::string *error) override {
        const CurlApi &c = api();
        if (c.easy_init == nullptr) {
            if (error) *error = c.error;
            return false;
        }
        if (!url.empty()) {
            CURL *probe = c.easy_init();
            if (probe == nullptr) {
                if (error) *error = \"libcurl easy handle allocation failed\";
                return false;
            }
            CURLcode code = c.easy_setopt(probe, CURLOPT_DOH_URL, url.c_str());
            curl_slist *bootstrap = nullptr;
            if (code == CURLE_OK && !resolve_entry.empty()) {
                bootstrap = c.slist_append(nullptr, resolve_entry.c_str());
                if (bootstrap == nullptr) code = CURLE_OUT_OF_MEMORY;
                else code = c.easy_setopt(probe, CURLOPT_RESOLVE, bootstrap);
            }
            if (bootstrap != nullptr) c.slist_free_all(bootstrap);
            c.easy_cleanup(probe);
            if (code != CURLE_OK) {
                if (error) *error = std::string(\"libcurl cannot configure DoH: \") + c.easy_strerror(code);
                return false;
            }
        }
        std::vector<CURLM *> stale;
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            if (options_.doh_url == url && options_.doh_resolve == resolve_entry) return true;
            options_.doh_url = url;
            options_.doh_resolve = resolve_entry;
            ++doh_generation_;
            stale.swap(idle_); // discard DNS caches from the previous resolver
        }
        for (CURLM *m : stale) c.multi_cleanup(m);
        if (error) error->clear();
        return true;
    }

    std::string describe() const override { return api().version; }
""")
    replace_once(source,
        """        CURL *h = c.easy_init();
        CURLM *m = take_multi();
""",
        """        CURL *h = c.easy_init();
        std::string doh_url, doh_resolve;
        uint64_t doh_generation = 0;
        CURLM *m = take_multi(&doh_url, &doh_resolve, &doh_generation);
""")
    replace_once(source,
        """        curl_slist *headers = nullptr;
        for (const auto &[k, v] : req.headers) headers = c.slist_append(headers, (k + \": \" + v).c_str());
        headers = c.slist_append(headers, \"Expect:\");  // no 100-continue round trip for larger bodies

        const long protocols = CURLPROTO_HTTP | CURLPROTO_HTTPS;
""",
        """        curl_slist *headers = nullptr;
        for (const auto &[k, v] : req.headers) headers = c.slist_append(headers, (k + \": \" + v).c_str());
        headers = c.slist_append(headers, \"Expect:\");  // no 100-continue round trip for larger bodies
        curl_slist *doh_bootstrap = nullptr;
        if (!doh_url.empty()) {
            CURLcode code = c.easy_setopt(h, CURLOPT_DOH_URL, doh_url.c_str());
            if (code == CURLE_OK && !doh_resolve.empty()) {
                doh_bootstrap = c.slist_append(nullptr, doh_resolve.c_str());
                if (doh_bootstrap == nullptr) code = CURLE_OUT_OF_MEMORY;
                else code = c.easy_setopt(h, CURLOPT_RESOLVE, doh_bootstrap);
            }
            if (code != CURLE_OK) {
                if (doh_bootstrap != nullptr) c.slist_free_all(doh_bootstrap);
                c.slist_free_all(headers);
                c.easy_cleanup(h);
                c.multi_cleanup(m);
                r.error = NetError::Unsupported;
                r.detail = std::string(\"libcurl cannot configure DoH: \") + c.easy_strerror(code);
                return r;
            }
        }

        const long protocols = CURLPROTO_HTTP | CURLPROTO_HTTPS;
""")
    replace_once(source,
        """        c.multi_remove_handle(m, h);
        give_multi(m);
        c.easy_cleanup(h);
        c.slist_free_all(headers);
""",
        """        c.multi_remove_handle(m, h);
        give_multi(m, doh_generation);
        c.easy_cleanup(h);
        c.slist_free_all(headers);
        if (doh_bootstrap != nullptr) c.slist_free_all(doh_bootstrap);
""")
    replace_once(source,
        """    CURLM *take_multi() {
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            if (!idle_.empty()) {
                CURLM *m = idle_.back();
                idle_.pop_back();
                return m;
            }
        }
        return api().multi_init();
    }

    void give_multi(CURLM *m) {
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            if (idle_.size() < kMaxIdle) {
                idle_.push_back(m);
                return;
            }
        }
        api().multi_cleanup(m);
    }
""",
        """    CURLM *take_multi(std::string *doh_url, std::string *doh_resolve, uint64_t *generation) {
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            *doh_url = options_.doh_url;
            *doh_resolve = options_.doh_resolve;
            *generation = doh_generation_;
            if (!idle_.empty()) {
                CURLM *m = idle_.back();
                idle_.pop_back();
                return m;
            }
        }
        return api().multi_init();
    }

    void give_multi(CURLM *m, uint64_t generation) {
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            if (generation == doh_generation_ && idle_.size() < kMaxIdle) {
                idle_.push_back(m);
                return;
            }
        }
        api().multi_cleanup(m);
    }
""")
    replace_once(source,
        """    std::mutex pool_mutex_;
    std::vector<CURLM *> idle_;  // most recently used last
    CurlOptions options_;
""",
        """    std::mutex pool_mutex_;
    std::vector<CURLM *> idle_;  // most recently used last
    uint64_t doh_generation_ = 0;
    CurlOptions options_;
""")
    print(f"patch-kit-doh: applied/verified in {kit}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
