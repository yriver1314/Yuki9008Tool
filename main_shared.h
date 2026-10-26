#pragma once

#include "YRGui.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <setupapi.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace yr;
namespace fs = std::filesystem;

namespace col {
constexpr DWORD bg0      = 0x060A11;
constexpr DWORD bg1      = 0x0A111C;
constexpr DWORD sidebar  = 0x080D16;
constexpr DWORD ink      = 0xEAF3FF;
constexpr DWORD inkSoft  = 0xC7D6EC;
constexpr DWORD dim      = 0x8A9AB3;
constexpr DWORD faint    = 0x5C6B82;
constexpr DWORD line     = 0x1B2A40;
constexpr DWORD lineSoft = 0x142136;
constexpr DWORD panelTop = 0x0D1727;
constexpr DWORD panelBot = 0x0A111D;
constexpr DWORD green    = 0x38D996;
constexpr DWORD amber    = 0xFFB454;
constexpr DWORD red      = 0xFF6076;
constexpr DWORD violet   = 0x9179FF;
} // namespace col

struct LR {
    int x = 0, y = 0, w = 0, h = 0;
    int r() const { return x + w; }
    int b() const { return y + h; }
    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }
    bool has(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    LR inset(int v) const { return { x + v, y + v, w - v * 2, h - v * 2 }; }
};

struct LLayout {
    int W = 1240, H = 776;
    int top = 0;
    int L = 260, R = 1216;
    int railW = 280, railX = 936;
    int mainW = 648;
    int actionY = 718;
    LR hero, stage, firmware, health, options, device, action;
    LR sidebar, brand, deviceMini, tipMini;
    std::array<LR, 5> nav;
    LR pill;
};

static inline std::wstring Lower(std::wstring s)
{
    for (wchar_t& c : s) c = (wchar_t)towlower(c);
    return s;
}

static inline bool EndsWith(const std::wstring& s, const wchar_t* suf)
{
    std::wstring t = Lower(s), u = Lower(suf ? suf : L"");
    return t.size() >= u.size() && t.compare(t.size() - u.size(), u.size(), u) == 0;
}

static inline bool Contains(const std::wstring& s, const wchar_t* part)
{
    return Lower(s).find(Lower(part ? part : L"")) != std::wstring::npos;
}

static inline std::wstring FormatSize(uint64_t bytes)
{
    wchar_t buf[64];
    const double b = (double)bytes;
    if (bytes >= 1024ull * 1024 * 1024)
        swprintf(buf, 64, L"%.2f GB", b / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ull * 1024)
        swprintf(buf, 64, L"%.1f MB", b / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        swprintf(buf, 64, L"%.1f KB", b / 1024.0);
    else
        swprintf(buf, 64, L"%llu B", (unsigned long long)bytes);
    return buf;
}

static inline std::wstring PercentText(float v)
{
    wchar_t buf[32];
    swprintf(buf, 32, L"%d%%", (int)std::lround(std::max(0.f, std::min(1.f, v)) * 100.f));
    return buf;
}

static inline std::wstring HexHash(const unsigned char* data, size_t n)
{
    static const wchar_t* hex = L"0123456789ABCDEF";
    std::wstring s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s.push_back(hex[(data[i] >> 4) & 0xF]);
        s.push_back(hex[data[i] & 0xF]);
    }
    return s;
}

static inline std::wstring Sha256File(const std::wstring& path)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD cb = 0, obj = 0, hashLen = 0;
    std::vector<unsigned char> objBuf, digest;
    std::wstring result;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        return result;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                                          (PUCHAR)&obj, sizeof(obj), &cb, 0))) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return result;
    }
    if (!BCRYPT_SUCCESS(BCryptGetProperty(alg, BCRYPT_HASH_LENGTH,
                                          (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0))) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return result;
    }
    objBuf.resize(obj);
    digest.resize(hashLen);
    if (!BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, objBuf.data(), obj, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return result;
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        std::vector<unsigned char> buf(1024 * 256);
        DWORD got = 0;
        bool ok = true;
        while (ReadFile(file, buf.data(), (DWORD)buf.size(), &got, nullptr) && got > 0) {
            if (!BCRYPT_SUCCESS(BCryptHashData(hash, buf.data(), got, 0))) { ok = false; break; }
        }
        CloseHandle(file);
        if (ok && BCRYPT_SUCCESS(BCryptFinishHash(hash, digest.data(), hashLen, 0)))
            result = HexHash(digest.data(), digest.size());
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

static inline std::wstring Sha256Bytes(const std::string& bytes)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD cb = 0, obj = 0, hashLen = 0;
    std::vector<unsigned char> objBuf, digest;
    std::wstring result;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        return result;
    if (!BCRYPT_SUCCESS(BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                                          (PUCHAR)&obj, sizeof(obj), &cb, 0)) ||
        !BCRYPT_SUCCESS(BCryptGetProperty(alg, BCRYPT_HASH_LENGTH,
                                          (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0))) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return result;
    }
    objBuf.resize(obj);
    digest.resize(hashLen);
    if (BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, objBuf.data(), obj, nullptr, 0, 0))) {
        if (BCRYPT_SUCCESS(BCryptHashData(hash, (PUCHAR)bytes.data(),
                                          (ULONG)bytes.size(), 0)) &&
            BCRYPT_SUCCESS(BCryptFinishHash(hash, digest.data(), hashLen, 0)))
            result = HexHash(digest.data(), digest.size());
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}

static inline std::wstring FromUtf8(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

static inline std::string ToUtf8(const std::wstring& s)
{
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

static inline std::wstring Timestamp()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf(buf, 32, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static inline std::wstring FileName(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

static inline std::wstring FolderName(const std::wstring& path)
{
    std::wstring p = path;
    while (!p.empty() && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? p : p.substr(s + 1);
}

static inline uint64_t FileSizeSafe(const fs::path& p)
{
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) return 0;
    auto sz = fs::file_size(p, ec);
    return ec ? 0 : (uint64_t)sz;
}

static inline std::wstring PickFolder(HWND owner, const wchar_t* title)
{
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return L"";
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    if (title) dlg->SetTitle(title);
    std::wstring result;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                result = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

static inline std::wstring PickSave(HWND owner, const wchar_t* title,
                                    const wchar_t* defaultName,
                                    const wchar_t* typeName, const wchar_t* ext)
{
    IFileSaveDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return L"";
    COMDLG_FILTERSPEC spec = { typeName, ext };
    dlg->SetFileTypes(1, &spec);
    dlg->SetDefaultExtension(ext && ext[0] == L'*' ? ext + 1 : ext);
    if (defaultName) dlg->SetFileName(defaultName);
    if (title) dlg->SetTitle(title);
    dlg->SetOptions(FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT);
    std::wstring result;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                result = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

struct PartInfo {
    std::wstring label;
    std::wstring file;
    std::wstring kind;
    uint64_t bytes = 0;
};

struct FirmwareInfo {
    std::wstring root;
    std::wstring name;
    std::wstring version;
    std::wstring programmer;
    std::wstring rawprogram;
    std::wstring patch;
    std::wstring hash;
    std::vector<PartInfo> parts;
    uint64_t totalBytes = 0;
    int rawCount = 0;
    bool valid = false;
    bool warning = false;
};

struct DeviceInfo {
    bool present = false;
    std::wstring name;
    std::wstring port;
    std::wstring vid = L"05C6";
    std::wstring pid = L"9008";
    std::wstring serial;
};

enum class LogLevel { Info, Ok, Warn, Error, Flash };

struct LogItem {
    std::wstring time;
    std::wstring tag;
    std::wstring text;
    LogLevel level = LogLevel::Info;
};

struct CFonts {
    std::unique_ptr<Gdiplus::Font> eyebrow, title, h2, h3, body, bodyBold;
    std::unique_ptr<Gdiplus::Font> small, tiny, numXL, numL, num, numSm, mono, monoS;

    static std::unique_ptr<Gdiplus::Font> Make(const wchar_t* family, float logical,
                                               int style, int dpi,
                                               const wchar_t* fallback = L"Microsoft YaHei UI")
    {
        float px = logical * (float)dpi / 96.f;
        auto f = std::make_unique<Gdiplus::Font>(family, px, style, Gdiplus::UnitPixel);
        if (f->GetLastStatus() != Gdiplus::Ok)
            f = std::make_unique<Gdiplus::Font>(fallback, px, style, Gdiplus::UnitPixel);
        return f;
    }

    void Init(int dpi)
    {
        eyebrow  = Make(L"Microsoft YaHei UI", 9.0f,  Gdiplus::FontStyleBold, dpi);
        title    = Make(L"Microsoft YaHei UI", 25.0f, Gdiplus::FontStyleBold, dpi);
        h2       = Make(L"Microsoft YaHei UI", 14.0f, Gdiplus::FontStyleBold, dpi);
        h3       = Make(L"Microsoft YaHei UI", 12.0f, Gdiplus::FontStyleBold, dpi);
        body     = Make(L"Microsoft YaHei UI", 11.5f, Gdiplus::FontStyleRegular, dpi);
        bodyBold = Make(L"Microsoft YaHei UI", 11.5f, Gdiplus::FontStyleBold, dpi);
        small    = Make(L"Microsoft YaHei UI", 10.0f, Gdiplus::FontStyleRegular, dpi);
        tiny     = Make(L"Microsoft YaHei UI", 8.5f,  Gdiplus::FontStyleRegular, dpi);
        numXL    = Make(L"Bahnschrift", 34.0f, Gdiplus::FontStyleRegular, dpi, L"Segoe UI");
        numL     = Make(L"Bahnschrift", 23.0f, Gdiplus::FontStyleRegular, dpi, L"Segoe UI");
        num      = Make(L"Bahnschrift", 14.0f, Gdiplus::FontStyleRegular, dpi, L"Segoe UI");
        numSm    = Make(L"Bahnschrift", 10.0f, Gdiplus::FontStyleRegular, dpi, L"Segoe UI");
        mono     = Make(L"Consolas", 10.0f, Gdiplus::FontStyleRegular, dpi, L"Segoe UI");
        monoS    = Make(L"Consolas", 9.0f,  Gdiplus::FontStyleRegular, dpi, L"Segoe UI");
    }
};
