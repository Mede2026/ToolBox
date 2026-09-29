#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace net {

// GET HTTPS avec WinHTTP (les redirections sont suivies). `progress(reçu, total)` est optionnel.
bool HttpGet(const std::string& url, std::string& body, std::string& err,
             const std::function<void(size_t, size_t)>& progress = nullptr);

// Empreinte SHA-256 en hexadécimal minuscule.
std::string Sha256Hex(const std::string& data);

}  // namespace net
