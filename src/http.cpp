#include "http.h"

#include <bcrypt.h>
#include <winhttp.h>

#include "util.h"
#include "version.h"

namespace net {

// GET HTTPS avec WinHTTP (les redirections vers le stockage GitHub sont suivies).
bool HttpGet(const std::string& url, std::string& body, std::string& err,
             const std::function<void(size_t, size_t)>& progress) {
  std::wstring wurl = util::FromUtf8(url);
  URL_COMPONENTS uc{};
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256] = {}, path[2048] = {};
  uc.lpszHostName = host;
  uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path;
  uc.dwUrlPathLength = 2048;
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
    err = "URL invalide";
    return false;
  }

  bool ok = false;
  HINTERNET session = WinHttpOpen(L"ToolBox/" TOOLBOX_VERSION_STR, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  HINTERNET conn = session ? WinHttpConnect(session, host, uc.nPort, 0) : nullptr;
  HINTERNET req = conn ? WinHttpOpenRequest(conn, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                       : nullptr;
  if (req) {
    const wchar_t* headers = L"Accept: application/vnd.github+json, application/octet-stream\r\n";
    if (WinHttpSendRequest(req, headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
      DWORD status = 0, size = sizeof(status);
      WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                          &status, &size, WINHTTP_NO_HEADER_INDEX);
      DWORD total = 0;
      size = sizeof(total);
      WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                          &total, &size, WINHTTP_NO_HEADER_INDEX);
      if (status != 200) {
        err = "Réponse HTTP " + std::to_string(status);
      } else {
        body.clear();
        ok = true;
        for (;;) {
          DWORD avail = 0;
          if (!WinHttpQueryDataAvailable(req, &avail)) {
            ok = false;
            break;
          }
          if (avail == 0) break;
          size_t old = body.size();
          body.resize(old + avail);
          DWORD read = 0;
          if (!WinHttpReadData(req, body.data() + old, avail, &read)) {
            ok = false;
            break;
          }
          body.resize(old + read);
          if (progress) progress(body.size(), total);
        }
        if (!ok) err = "Téléchargement interrompu";
      }
    } else {
      err = "Pas de connexion Internet";
    }
  } else {
    err = "Impossible d'ouvrir la connexion";
  }
  if (req) WinHttpCloseHandle(req);
  if (conn) WinHttpCloseHandle(conn);
  if (session) WinHttpCloseHandle(session);
  return ok;
}

std::string Sha256Hex(const std::string& data) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  unsigned char digest[32] = {};
  std::string hex;
  if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) &&
      BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0)) &&
      BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                                    static_cast<ULONG>(data.size()), 0)) &&
      BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0))) {
    static const char* kHex = "0123456789abcdef";
    for (unsigned char b : digest) {
      hex += kHex[b >> 4];
      hex += kHex[b & 0xF];
    }
  }
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  return hex;
}

}  // namespace net
