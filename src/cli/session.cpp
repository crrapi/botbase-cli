#include "session.hpp"

#include <format>
#include <iostream>

#include "botbase/discovery.hpp"
#include "botbase/errors.hpp"
#include "botbase/signals.hpp"
#include "host_cache.hpp"

namespace botbase::cli {

Client& Session::client() {
    if (!client_) {
        ConnectOptions connect{
            .host = resolve_host(),
            .port = options_.port,
            .interface = options_.interface,
            .timeout = options_.timeout,
        };
        client_ = std::make_unique<Client>(std::move(connect));
        if (options_.fast) client_->fast_mode();
    }
    return *client_;
}

void Session::note(std::string_view message) const {
    if (!options_.quiet) std::cerr << "botbase: " << message << '\n';
}

std::string Session::resolve_host() {
    if (!options_.host.empty()) return options_.host;

    if (options_.use_cache) {
        if (auto cached = read_cached_host()) {
            if (probe(*cached, options_.port, options_.interface, 1.0)) return *cached;
            note(std::format("{} no longer answers, scanning", *cached));
        }
    }

    std::string subnet = options_.subnet.empty() ? default_subnet(options_.interface) : options_.subnet;
    note(std::format("no --host given, scanning {}", subnet));
    auto found = discover(subnet, options_.port, options_.interface);

    if (found.empty()) {
        throw ConnectionError(std::format(
            "no sys-botbase on {}. Is the console awake and on this network? "
            "With a VPN that routes this subnet, pass --interface.", subnet));
    }
    if (found.size() > 1) {
        std::string hosts;
        for (const auto& console : found) hosts += std::format("{}{}", hosts.empty() ? "" : ", ", console.host);
        throw ConnectionError(std::format("found {} consoles ({}); pick one with --host", found.size(), hosts));
    }

    note(std::format("using {} (sys-botbase {})", found[0].host, found[0].version));
    if (options_.use_cache) write_cached_host(found[0].host);
    return found[0].host;
}

void Session::resync() noexcept {
    if (!client_) return;
    try {
        client_->resync();
    } catch (const std::exception&) {
        try {
            client_->reconnect();
        } catch (const std::exception& e) {
            note(std::format("could not reconnect: {}", e.what()));
        }
    }
}

void Session::recover() noexcept {
    if (!client_) return;
    clear_interrupt();
    resync();
    try {
        if (client_->holding_inputs()) {
            client_->release_inputs();
            note("released held inputs");
        }
    } catch (const std::exception& e) {
        note(std::format("could not release held inputs: {}", e.what()));
    }
}

void Session::finish(bool failed) noexcept {
    begin_cleanup();
    if (!client_) return;
    if (failed || failed_) recover();

    try {
        if (options_.fast) client_->fast_mode(false);
        if (options_.detach) client_->detach_controller();
    } catch (const std::exception& e) {
        note(std::format("cleanup failed: {}", e.what()));
    }
}

}  // namespace botbase::cli
