// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugins/plugin_manager.hpp"

#include "core/http_url.hpp"
#include "core/sha256.hpp"
#include "core/zip_check.hpp"

#include "wnk/log.hpp"

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>

namespace csweb::plugins {

namespace {

constexpr const char *kCat = "csweb";
constexpr size_t kMaxRepoBytes = 1u << 20;     // repo.json, plugins.json
constexpr size_t kMaxPluginBytes = 32u << 20;  // a .cs3

struct StoreRead {
    std::vector<RepositoryData> repos;
    std::vector<InstalledPlugin> installed;
    std::vector<std::string> seed_errors;
};

struct InstallResult {
    bool ok = false;
    std::string error;
    InstalledPlugin record;
};

struct BundledPlugin {
    InstalledPlugin record;
    std::string bytes;
};

struct BundledSet {
    RepositoryData repository;
    std::vector<BundledPlugin> plugins;
    std::vector<std::string> errors;
};

bool read_bundled_set(const std::string &dir, BundledSet *out) {
    const std::string index_path = dir + "/manifest.json";
    if (::access(index_path.c_str(), F_OK) != 0) return true;  // regular packages have no local bundle
    std::ifstream file(index_path, std::ios::binary);
    if (!file) {
        out->errors.push_back("could not open bundled-plugins/manifest.json");
        return false;
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (text.size() > kMaxRepoBytes) {
        out->errors.push_back("bundled plugin manifest exceeds the size limit");
        return false;
    }
    const auto j = nlohmann::json::parse(text, nullptr, false);
    if (!j.is_object() || j.value("schemaVersion", 0) != 1 || !j.contains("repository") || !j["repository"].is_object() ||
        !j.contains("plugins") || !j["plugins"].is_array()) {
        out->errors.push_back("bundled plugin manifest has an unsupported format");
        return false;
    }
    const auto &repo = j["repository"];
    if (!repo.contains("name") || !repo["name"].is_string() || !repo.contains("url") || !repo["url"].is_string()) {
        out->errors.push_back("bundled plugin manifest has no repository name or URL");
        return false;
    }
    out->repository = {repo["name"].get<std::string>(), repo["url"].get<std::string>(), repo.value("iconUrl", "")};
    for (const auto &entry : j["plugins"]) {
        if (!entry.is_object() || !entry.contains("file") || !entry["file"].is_string() ||
            !entry.contains("internalName") || !entry["internalName"].is_string() ||
            !entry.contains("sha256") || !entry["sha256"].is_string() ||
            !entry.contains("version") || !entry["version"].is_number_integer()) {
            out->errors.push_back("skipped a bundled plugin with incomplete metadata");
            continue;
        }
        const std::string filename = entry["file"].get<std::string>();
        const std::string internal = entry["internalName"].get<std::string>();
        const bool safe_name = !filename.empty() && filename.size() <= 128 &&
                               std::all_of(filename.begin(), filename.end(), [](unsigned char c) {
                                   return std::isalnum(c) || c == '.' || c == '_' || c == '-';
                               }) && filename.find("..") == std::string::npos && filename.size() > 4 &&
                               filename.substr(filename.size() - 4) == ".cs3";
        if (!safe_name || internal.empty()) {
            out->errors.push_back("skipped a bundled plugin with an unsafe file name");
            continue;
        }
        const std::string path = dir + "/" + filename;
        std::ifstream plugin_file(path, std::ios::binary);
        if (!plugin_file) {
            out->errors.push_back("missing bundled plugin file " + filename);
            continue;
        }
        std::string bytes{std::istreambuf_iterator<char>(plugin_file), std::istreambuf_iterator<char>()};
        if (bytes.empty() || bytes.size() > kMaxPluginBytes) {
            out->errors.push_back(filename + " is empty or exceeds the plugin size limit");
            continue;
        }
        std::string expected = entry["sha256"].get<std::string>();
        std::transform(expected.begin(), expected.end(), expected.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!core::is_sha256_hex(expected) || core::Sha256::hex(bytes) != expected) {
            out->errors.push_back(filename + " failed SHA-256 verification");
            continue;
        }
        const auto archive = core::check_plugin_archive(bytes);
        if (!archive.ok) {
            out->errors.push_back(filename + " is not a valid .cs3 archive: " + archive.error);
            continue;
        }
        BundledPlugin bundled;
        auto &record = bundled.record;
        record.id = plugin_id(out->repository.url, internal);
        record.internal_name = internal;
        record.name = entry.value("name", internal);
        record.repository_url = out->repository.url;
        record.url = entry.value("url", "");
        record.version = entry["version"].get<int>();
        record.sha256 = expected;
        record.size = static_cast<long long>(bytes.size());
        record.language = entry.value("language", "");
        record.icon_url = entry.value("iconUrl", "");
        if (entry.contains("tvTypes") && entry["tvTypes"].is_array()) {
            for (const auto &type : entry["tvTypes"]) if (type.is_string()) record.tv_types.push_back(type.get<std::string>());
        }
        record.enabled = true;
        bundled.bytes = std::move(bytes);
        out->plugins.push_back(std::move(bundled));
    }
    return true;
}

StoreRead read_store_with_bundled_plugins(PluginStore *store, const std::string &dir) {
    StoreRead read{store->repositories(), store->installed(), {}};
    BundledSet bundled;
    if (!read_bundled_set(dir, &bundled)) {
        read.seed_errors = std::move(bundled.errors);
        return read;
    }
    read.seed_errors = std::move(bundled.errors);
    if (bundled.plugins.empty()) return read;

    const bool repo_exists = std::any_of(read.repos.begin(), read.repos.end(), [&](const RepositoryData &r) {
        return r.url == bundled.repository.url;
    });
    if (!repo_exists) {
        read.repos.push_back(bundled.repository);
        std::string error;
        if (!store->save_repositories(read.repos, &error)) {
            read.seed_errors.push_back("could not save the bundled repository: " + error);
            read.repos.pop_back();
            return read;
        }
    }
    for (auto &plugin : bundled.plugins) {
        const bool installed = std::any_of(read.installed.begin(), read.installed.end(), [&](const InstalledPlugin &p) {
            return p.id == plugin.record.id;
        });
        if (installed) continue;  // do not overwrite a user's installed or updated version
        std::string error;
        if (!store->install(plugin.record, plugin.bytes, &error)) {
            read.seed_errors.push_back("could not install bundled " + plugin.record.name + ": " + error);
            continue;
        }
        read.installed.push_back(std::move(plugin.record));
    }
    return read;
}

std::string net_error(const wnk::net::HttpResponse &r) {
    if (r.error != wnk::net::NetError::None) return std::string("network: ") + wnk::net::error_name(r.error);
    return "HTTP " + std::to_string(r.status);
}

std::string file_id(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.rfind(".cs3");
    return dot == std::string::npos ? name : name.substr(0, dot);
}

}  // namespace

PluginManager::PluginManager(Deps deps) : deps_(std::move(deps)) {
    if (deps_.kv) store_ = std::make_unique<PluginStore>(*deps_.kv);
    if (deps_.http) http_ = std::make_unique<wnk::net::HttpClient>(deps_.http, *deps_.pool, *deps_.main);
}

PluginManager::~PluginManager() = default;

void PluginManager::changed() {
    if (on_changed) on_changed();
}

const runtime::Provider *PluginManager::provider(const std::string &name) const {
    for (const auto &p : providers_) if (p.name == name) return &p;
    return nullptr;
}

const InstalledPlugin *PluginManager::installed_plugin(const std::string &repository_url, const std::string &internal_name) const {
    const std::string id = plugin_id(repository_url, internal_name);
    for (const auto &p : installed_) if (p.id == id) return &p;
    return nullptr;
}

void PluginManager::start() {
    if (!store_) {
        WNK_LOGW(kCat, "plugins: no key/value store; repositories and plugins cannot be kept");
        started_ = true;
        changed();
        return;
    }
    PluginStore *store = store_.get();
    const std::string bundled_dir = deps_.bundled_plugins_dir;
    wnk::async::run_then_post<StoreRead>(
        *deps_.pool, *deps_.main, life_.weak(), {},
        [store, bundled_dir](const wnk::async::CancellationToken &) {
            return read_store_with_bundled_plugins(store, bundled_dir);
        },
        [this](StoreRead r) {
            repos_ = std::move(r.repos);
            installed_ = std::move(r.installed);
            for (const auto &error : r.seed_errors) WNK_LOGW(kCat, "bundled plugins: %s", error.c_str());
            started_ = true;
            WNK_LOGI(kCat, "plugins: %zu repositories, %zu installed", repos_.size(), installed_.size());
            if (runtime_ready_) {
                for (const auto &p : installed_) if (p.enabled) load_plugin(p);
            }
            changed();
        });
}

void PluginManager::on_runtime_ready() {
    runtime_ready_ = true;
    providers_.clear();
    load_errors_.clear();
    pending_loads_ = 0;
    if (started_) {
        for (const auto &p : installed_) if (p.enabled) load_plugin(p);
    }
    changed();
}

void PluginManager::load_plugin(const InstalledPlugin &p) {
    if (!store_ || !deps_.runtime) return;
    pending_loads_++;
    PluginStore *store = store_.get();
    const std::string dir = deps_.plugin_dir;
    struct Out {
        std::string path, error;
    };
    wnk::async::run_then_post<Out>(
        *deps_.pool, *deps_.main, life_.weak(), {},
        [store, p, dir](const wnk::async::CancellationToken &) {
            Out o;
            o.path = store->materialize(p, dir, &o.error);
            return o;
        },
        [this, p](Out o) {
            if (o.path.empty()) {
                pending_loads_--;
                load_errors_[p.id] = o.error;
                WNK_LOGW(kCat, "plugins: %s not written: %s", p.internal_name.c_str(), o.error.c_str());
                changed();
                return;
            }
            deps_.runtime->request(
                {{"op", "load"}, {"plugin", o.path}, {"sha256", p.sha256}},
                [this](const nlohmann::json &e) {
                    if (e.value("event", "") == "provider") providers_.push_back(runtime::parse_provider(e));
                },
                [this, p](bool ok, const std::string &error) {
                    pending_loads_--;
                    load_errors_[p.id] = ok ? std::string() : error;
                    if (!ok) WNK_LOGW(kCat, "plugins: %s did not load: %s", p.internal_name.c_str(), error.c_str());
                    changed();
                });
        });
}

bool PluginManager::loading() const {
    if (!started_ || pending_loads_ > 0) return true;
    if (runtime_ready_) return false;
    // Plugins wait for the runtime, unless it has failed for good.
    if (deps_.runtime && deps_.runtime->state() == runtime::RuntimeClient::State::Failed) return false;
    return std::any_of(installed_.begin(), installed_.end(), [](const InstalledPlugin &p) { return p.enabled; });
}

void PluginManager::reload(const std::string &id) {
    for (const auto &p : installed_) {
        if (p.id != id || !p.enabled) continue;
        const InstalledPlugin copy = p;
        unload_plugin(id, [this, copy] { load_plugin(copy); });
        return;
    }
}

void PluginManager::unload_plugin(const std::string &id, std::function<void()> then) {
    const std::string path = plugin_path(id);
    auto drop = [this, id, path, then] {
        providers_.erase(std::remove_if(providers_.begin(), providers_.end(),
                                        [&](const runtime::Provider &p) { return file_id(p.plugin) == id; }),
                         providers_.end());
        load_errors_.erase(id);
        if (then) then();
    };
    const bool loaded = std::any_of(providers_.begin(), providers_.end(),
                                    [&](const runtime::Provider &p) { return file_id(p.plugin) == id; }) ||
                        (load_errors_.count(id) && load_errors_[id].empty());
    if (!loaded || !deps_.runtime || deps_.runtime->state() != runtime::RuntimeClient::State::Ready) {
        drop();
        return;
    }
    deps_.runtime->request({{"op", "unload"}, {"plugin", path}}, {}, [drop](bool, const std::string &) { drop(); });
}

void PluginManager::fetch(const std::string &url, size_t max_bytes, int max_redirects,
                          std::function<void(wnk::net::HttpResponse)> done) {
    if (!http_) {
        wnk::net::HttpResponse r;
        r.error = wnk::net::NetError::Unsupported;
        done(std::move(r));
        return;
    }
    wnk::net::HttpRequest req;
    req.url = core::canonical_http_url(url);  // as OkHttp sends it: "My Plugin.cs3" -> "My%20Plugin.cs3"
    req.max_response_bytes = max_bytes;
    req.max_redirects = max_redirects;
    req.timeout_ms = 60000;
    // send() does not call back for a request it refuses (a URL it cannot use); report that here so
    // an install or a repository read never waits for ever.
    auto shared = std::make_shared<std::function<void(wnk::net::HttpResponse)>>(std::move(done));
    if (!http_->send(req, {}, life_.weak(), [shared](wnk::net::HttpResponse r) { (*shared)(std::move(r)); })) {
        wnk::net::HttpResponse r;
        r.error = wnk::net::NetError::InvalidRequest;
        (*shared)(std::move(r));
    }
}

void PluginManager::add_repository(const std::string &typed, Done done) {
    const RepoInput in = parse_repo_input(typed);
    if (in.kind == RepoInput::Kind::Invalid) {
        done(false, "Not a repository URL or shortcode");
        return;
    }
    auto read_repo = [this, done](const std::string &url) {
        for (const auto &r : repos_) {
            if (r.url == url) {
                done(false, "This repository is already added");
                return;
            }
        }
        fetch(url, kMaxRepoBytes, 5, [this, url, done](wnk::net::HttpResponse res) {
            if (!res.ok()) {
                done(false, "Could not read the repository (" + net_error(res) + ")");
                return;
            }
            std::string error;
            auto repo = parse_repository(res.body, &error);
            if (!repo) {
                done(false, error);
                return;
            }
            auto repos = repos_;
            repos.push_back({repo->name, url, repo->icon_url});
            PluginStore *store = store_.get();
            if (!store) {
                done(false, "Storage is not available on this TV");
                return;
            }
            wnk::async::run_then_post<std::string>(
                *deps_.pool, *deps_.main, life_.weak(), {},
                [store, repos](const wnk::async::CancellationToken &) {
                    std::string e;
                    return store->save_repositories(repos, &e) ? std::string() : (e.empty() ? "store" : e);
                },
                [this, repos, done](std::string e) {
                    if (!e.empty()) {
                        done(false, "Could not save the repository (" + e + ")");
                        return;
                    }
                    repos_ = repos;
                    changed();
                    done(true, {});
                });
        });
    };
    if (in.kind == RepoInput::Kind::Url) {
        read_repo(in.url);
        return;
    }
    // A shortcode: the service answers with a redirect whose Location is the repository.
    fetch(in.url, 64u << 10, 0, [in, read_repo, done](wnk::net::HttpResponse res) {
        const std::string location = accept_shortcode_redirect(in.url, res.header("location"));
        if (location.empty()) {
            done(false, "Unknown shortcode");
            return;
        }
        const RepoInput again = parse_repo_input(location);
        if (again.kind != RepoInput::Kind::Url) {
            done(false, "The shortcode does not lead to a repository");
            return;
        }
        read_repo(again.url);
    });
}

void PluginManager::remove_repository(const std::string &url, Done done) {
    // Like RepositoryManager.removeRepository: the repository's plugins go with it.
    std::vector<std::string> ids;
    for (const auto &p : installed_) if (p.repository_url == url) ids.push_back(p.id);
    auto remaining = std::make_shared<size_t>(ids.size());
    auto finish = [this, url, done] {
        auto repos = repos_;
        repos.erase(std::remove_if(repos.begin(), repos.end(), [&](const RepositoryData &r) { return r.url == url; }),
                    repos.end());
        PluginStore *store = store_.get();
        if (!store) {
            done(false, "Storage is not available on this TV");
            return;
        }
        wnk::async::run_then_post<bool>(
            *deps_.pool, *deps_.main, life_.weak(), {},
            [store, repos](const wnk::async::CancellationToken &) { return store->save_repositories(repos, nullptr); },
            [this, repos, done](bool ok) {
                if (ok) repos_ = repos;
                changed();
                done(ok, ok ? std::string() : "Could not save repositories");
            });
    };
    if (ids.empty()) {
        finish();
        return;
    }
    for (const auto &id : ids) {
        uninstall(id, [remaining, finish](bool, const std::string &) {
            if (--*remaining == 0) finish();
        });
    }
}

void PluginManager::fetch_plugins(const RepositoryData &repo, PluginsDone done) {
    fetch(repo.url, kMaxRepoBytes, 5, [this, done](wnk::net::HttpResponse res) {
        if (!res.ok()) {
            done(false, "Could not read the repository (" + net_error(res) + ")", {});
            return;
        }
        std::string error;
        auto parsed = parse_repository(res.body, &error);
        if (!parsed) {
            done(false, error, {});
            return;
        }
        if (parsed->plugin_lists.empty()) {
            done(true, {}, {});
            return;
        }
        struct Acc {
            size_t left;
            std::vector<SitePlugin> all;
            std::string error;
        };
        auto acc = std::make_shared<Acc>(Acc{parsed->plugin_lists.size(), {}, {}});
        for (const auto &list_url : parsed->plugin_lists) {
            fetch(list_url, kMaxRepoBytes, 5, [acc, done](wnk::net::HttpResponse r) {
                if (r.ok()) {
                    std::string e;
                    auto list = parse_plugin_list(r.body, &e);
                    acc->all.insert(acc->all.end(), list.begin(), list.end());
                    if (!e.empty()) acc->error = e;
                } else {
                    acc->error = "A plugin list could not be read (" + net_error(r) + ")";
                }
                if (--acc->left == 0) done(!acc->all.empty() || acc->error.empty(), acc->error, std::move(acc->all));
            });
        }
    });
}

void PluginManager::install(const RepositoryData &repo, const SitePlugin &plugin, Done done) {
    if (plugin.disabled()) {
        done(false, "The repository marks this plugin as not working");
        return;
    }
    if (!store_) {
        done(false, "Storage is not available on this TV");
        return;
    }
    fetch(plugin.url, kMaxPluginBytes, 5, [this, repo, plugin, done](wnk::net::HttpResponse res) {
        if (!res.ok()) {
            done(false, "Download failed (" + net_error(res) + ")");
            return;
        }
        PluginStore *store = store_.get();
        const std::string dir = deps_.plugin_dir;
        auto body = std::make_shared<std::string>(std::move(res.body));
        wnk::async::run_then_post<InstallResult>(
            *deps_.pool, *deps_.main, life_.weak(), {},
            [store, repo, plugin, body, dir](const wnk::async::CancellationToken &) {
                InstallResult r;
                const std::string hash = core::Sha256::hex(*body);
                const std::string expected = expected_sha256(plugin);
                if (!expected.empty() && expected != hash) {
                    r.error = "The download does not match the repository's hash";
                    return r;
                }
                const auto zip = core::check_plugin_archive(*body);
                if (!zip.ok) {
                    r.error = "Not a valid plugin: " + zip.error;
                    return r;
                }
                InstalledPlugin rec;
                rec.id = plugin_id(repo.url, plugin.internal_name);
                rec.internal_name = plugin.internal_name;
                rec.name = plugin.name;
                rec.repository_url = repo.url;
                rec.url = plugin.url;
                rec.version = plugin.version;
                rec.sha256 = hash;
                rec.size = static_cast<long long>(body->size());
                rec.tv_types = plugin.tv_types;
                rec.language = plugin.language;
                rec.icon_url = plugin.icon_url;
                if (!store->install(rec, *body, &r.error)) return r;
                r.ok = true;
                r.record = rec;
                return r;
            },
            [this, done](InstallResult r) {
                if (!r.ok) {
                    done(false, r.error);
                    return;
                }
                WNK_LOGI(kCat, "plugins: installed %s v%d", r.record.internal_name.c_str(), r.record.version);
                bool replaced = false;
                for (auto &p : installed_) {
                    if (p.id == r.record.id) {
                        p = r.record;
                        replaced = true;
                    }
                }
                if (!replaced) installed_.push_back(r.record);
                const InstalledPlugin rec = r.record;
                unload_plugin(rec.id, [this, rec] {
                    if (runtime_ready_) load_plugin(rec);
                    changed();
                });
                done(true, {});
            });
    });
}

void PluginManager::uninstall(const std::string &id, Done done) {
    unload_plugin(id, [this, id, done] {
        PluginStore *store = store_.get();
        if (!store) {
            done(false, "Storage is not available on this TV");
            return;
        }
        const std::string path = plugin_path(id);
        wnk::async::run_then_post<bool>(
            *deps_.pool, *deps_.main, life_.weak(), {},
            [store, id, path](const wnk::async::CancellationToken &) {
                ::unlink(path.c_str());
                return store->remove(id, nullptr);
            },
            [this, id, done](bool ok) {
                if (ok) {
                    installed_.erase(std::remove_if(installed_.begin(), installed_.end(),
                                                    [&](const InstalledPlugin &p) { return p.id == id; }),
                                     installed_.end());
                }
                changed();
                done(ok, ok ? std::string() : "Could not remove the plugin");
            });
    });
}

void PluginManager::set_enabled(const std::string &id, bool enabled, Done done) {
    PluginStore *store = store_.get();
    if (!store) {
        done(false, "Storage is not available on this TV");
        return;
    }
    wnk::async::run_then_post<bool>(
        *deps_.pool, *deps_.main, life_.weak(), {},
        [store, id, enabled](const wnk::async::CancellationToken &) { return store->set_enabled(id, enabled, nullptr); },
        [this, id, enabled, done](bool ok) {
            if (!ok) {
                done(false, "Could not save");
                return;
            }
            for (auto &p : installed_) {
                if (p.id != id) continue;
                p.enabled = enabled;
                const InstalledPlugin rec = p;
                if (enabled && runtime_ready_) load_plugin(rec);
                if (!enabled) unload_plugin(id, [this] { changed(); });
            }
            changed();
            done(true, {});
        });
}

}  // namespace csweb::plugins
