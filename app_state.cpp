#include "app_state.h"

#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
//  update channel
// ---------------------------------------------------------------------------
static constexpr const wchar_t* kAppVersion = L"1.0.1";
static constexpr const wchar_t* kRepoOwner = L"yriver1314";
static constexpr const wchar_t* kRepoName = L"Yuki9008Tool";

static std::string JsonUnescape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c != '\\' || i + 1 >= s.size()) {
            out.push_back(c);
            continue;
        }
        char n = s[++i];
        switch (n) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
            if (i + 4 >= s.size()) break;
            unsigned cp = 0;
            for (int k = 1; k <= 4; ++k) {
                char h = s[i + (size_t)k];
                cp <<= 4;
                if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
            }
            i += 4;
            if (cp < 0x80) out.push_back((char)cp);
            else if (cp < 0x800) {
                out.push_back((char)(0xC0 | (cp >> 6)));
                out.push_back((char)(0x80 | (cp & 0x3F)));
            } else {
                out.push_back((char)(0xE0 | (cp >> 12)));
                out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back((char)(0x80 | (cp & 0x3F)));
            }
            break;
        }
        default: out.push_back(n); break;
        }
    }
    return out;
}

static std::string JsonStringValue(const std::string& json, const std::string& key,
                                   size_t from = 0)
{
    std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle, from);
    if (p == std::string::npos) return "";
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return "";
    ++p;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' ||
                               json[p] == '\r' || json[p] == '\n')) ++p;
    if (p >= json.size() || json[p] != '"') return "";
    ++p;
    std::string raw;
    bool esc = false;
    for (; p < json.size(); ++p) {
        char c = json[p];
        if (esc) {
            raw.push_back('\\');
            raw.push_back(c);
            esc = false;
        } else if (c == '\\') {
            esc = true;
        } else if (c == '"') {
            break;
        } else {
            raw.push_back(c);
        }
    }
    return JsonUnescape(raw);
}

static size_t JsonObjectEnd(const std::string& s, size_t start)
{
    int depth = 0;
    bool inStr = false, esc = false;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') inStr = true;
        else if (c == '{') ++depth;
        else if (c == '}') {
            if (--depth == 0) return i;
        }
    }
    return std::string::npos;
}

static std::vector<int> ParseVersion(const std::wstring& tag)
{
    std::vector<int> out;
    size_t i = 0;
    while (i < tag.size() && !iswdigit(tag[i])) ++i;
    while (i < tag.size()) {
        int v = 0;
        while (i < tag.size() && iswdigit(tag[i])) v = v * 10 + (tag[i++] - L'0');
        out.push_back(v);
        while (i < tag.size() && tag[i] != L'.') ++i;
        if (i < tag.size()) ++i;
        if (out.size() >= 4) break;
    }
    while (out.size() < 3) out.push_back(0);
    return out;
}

static bool IsNewerVersion(const std::wstring& latest, const std::wstring& current)
{
    std::vector<int> a = ParseVersion(latest);
    std::vector<int> b = ParseVersion(current);
    size_t n = std::max(a.size(), b.size());
    a.resize(n, 0);
    b.resize(n, 0);
    return a > b;
}

static bool HttpGetGithub(const std::wstring& path, std::string& body,
                          std::wstring& error, DWORD& status)
{
    body.clear();
    error.clear();
    status = 0;

    HINTERNET session = WinHttpOpen(L"Yuki9008Tool/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        error = L"无法初始化 WinHTTP";
        return false;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 10000, 15000);
    HINTERNET conn = WinHttpConnect(session, L"api.github.com",
                                    INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!conn) {
        error = L"无法连接 api.github.com";
        WinHttpCloseHandle(session);
        return false;
    }
    HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       WINHTTP_FLAG_SECURE);
    if (!req) {
        error = L"无法创建 GitHub API 请求";
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return false;
    }
    const std::wstring headers =
        L"Accept: application/vnd.github+json\r\n"
        L"User-Agent: Yuki9008Tool/1.0\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";
    BOOL ok = WinHttpSendRequest(req, headers.c_str(), (DWORD)-1L,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(req, nullptr);
    if (ok) {
        DWORD cb = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &cb,
                            WINHTTP_NO_HEADER_INDEX);
        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(req, &avail)) break;
            if (avail == 0) break;
            std::vector<char> buf(avail);
            DWORD got = 0;
            if (!WinHttpReadData(req, buf.data(), avail, &got) || got == 0) break;
            body.append(buf.data(), got);
        }
    } else {
        DWORD err = GetLastError();
        error = L"GitHub 网络请求失败 (" + std::to_wstring(err) + L")";
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    if (!error.empty()) return false;
    if (status != 200) {
        error = L"GitHub API 返回 HTTP " + std::to_wstring(status);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------------
static void StyleSecondary(Widget& w)
{
    w.bg(0x101B2C).fg(col::inkSoft).border(0x23324A).radius(10).font(12);
}

static void StyleGhost(Widget& w)
{
    w.bg(0x0B1524).fg(0xBFD2EB).border(0x1B2B43).radius(10).font(11);
}

static void StyleDanger(Widget& w)
{
    w.bg(0x35131D).fg(0xFF9AAA).border(0x5A2433).radius(10).font(12);
}

static std::wstring LevelTag(LogLevel l)
{
    switch (l) {
    case LogLevel::Ok:    return L"OK   ";
    case LogLevel::Warn:  return L"WARN ";
    case LogLevel::Error: return L"ERR  ";
    case LogLevel::Flash: return L"FLASH";
    default:              return L"INFO ";
    }
}

static const wchar_t* KindOf(const std::wstring& label)
{
    std::wstring l = Lower(label);
    if (l.find(L"xbl") != std::wstring::npos || l.find(L"abl") != std::wstring::npos ||
        l.find(L"tz") == 0 || l.find(L"hyp") == 0 || l.find(L"keymaster") != std::wstring::npos ||
        l.find(L"devinfo") != std::wstring::npos)
        return L"引导";
    if (l.find(L"super") != std::wstring::npos || l.find(L"system") != std::wstring::npos ||
        l.find(L"vendor") != std::wstring::npos || l.find(L"product") != std::wstring::npos ||
        l.find(L"odm") != std::wstring::npos)
        return L"系统";
    if (l.find(L"userdata") != std::wstring::npos || l.find(L"cust") != std::wstring::npos ||
        l.find(L"metadata") != std::wstring::npos)
        return L"用户";
    if (l.find(L"modem") != std::wstring::npos || l.find(L"dsp") != std::wstring::npos ||
        l.find(L"bluetooth") != std::wstring::npos)
        return L"通信";
    if (l.find(L"persist") != std::wstring::npos || l.find(L"frp") != std::wstring::npos)
        return L"安全";
    return L"镜像";
}

// ---------------------------------------------------------------------------
//  ctor / build
// ---------------------------------------------------------------------------
AppState::AppState(App& app, UI& ui)
    : m_app(app), m_ui(ui), m_lastTick(std::chrono::steady_clock::now())
{
    s_instance = this;
    m_theme = m_app.theme();
    m_fonts.Init(m_app.dpi());
    LoadLogo();
    DetectEngine();
    ResetFirmware();
}

void AppState::LoadLogo()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir = exePath;
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir.resize(slash);
    std::wstring path = dir + L"\\logo.ico";

    HICON icon = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, 256, 256,
                                  LR_LOADFROMFILE);
    if (!icon)
        icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                 IMAGE_ICON, 256, 256, 0);
    if (!icon) return;

    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromHICON(icon);
    DestroyIcon(icon);
    if (!bmp) return;
    if (bmp->GetLastStatus() == Gdiplus::Ok) m_logo.reset(bmp);
    else delete bmp;
}

AppState::~AppState()
{
    m_closing = true;
    if (m_updateThread.joinable()) m_updateThread.join();
    if (m_proc) TerminateProcess(m_proc, 1);
    CloseProcessHandles();
    if (s_instance == this) s_instance = nullptr;
}

void AppState::Build()
{
    m_app.setPaintHooks(
        [this](Gdiplus::Graphics& g, int w, int h) { PaintBackground(g, w, h); },
        [this](Gdiplus::Graphics& g, int w, int h) { PaintForeground(g, w, h); });

    m_app.onResize([this](int w, int h) { OnResize(w, h); });

    BuildControls();
    Layout();
    PopulateTables();
    ApplyAccent(false);
    UpdateStartButton();
    UpdateUpdateButton();
    m_pageFade = 1.f;
    m_app.redraw();
    m_app.timer(33, [this] { Tick(); }, true);
}

void AppState::BuildControls()
{
    m_heroScan = m_ui.button(L"扫描 9008 设备", [this] { StartScan(); });
    m_heroScan.primary(true).accent((int)m_accent).radius(10).font(12).bold(true);
    m_dash.push_back(m_heroScan);

    m_heroBrowse = m_ui.button(L"选择固件包", [this] { BrowseFirmware(); });
    StyleSecondary(m_heroBrowse);
    m_dash.push_back(m_heroBrowse);

    m_optVerify = m_ui.checkbox(L"校验固件完整性", 0, 0, 10, 26);
    m_optVerify.checked(true).font(11).fg(col::inkSoft);
    m_dash.push_back(m_optVerify);
    m_optReboot = m_ui.checkbox(L"完成后提示重启", 0, 0, 10, 26);
    m_optReboot.checked(true).font(11).fg(col::inkSoft);
    m_dash.push_back(m_optReboot);
    m_optBackup = m_ui.checkbox(L"刷写前备份分区", 0, 0, 10, 26);
    m_optBackup.checked(false).font(11).fg(col::inkSoft);
    m_dash.push_back(m_optBackup);
    m_optKeep = m_ui.checkbox(L"保留用户数据", 0, 0, 10, 26);
    m_optKeep.checked(true).font(11).fg(col::inkSoft);
    m_dash.push_back(m_optKeep);

    m_changeBtn = m_ui.button(L"更换", [this] { BrowseFirmware(); });
    StyleGhost(m_changeBtn);
    m_changeBtn.font(10).radius(8);
    m_dash.push_back(m_changeBtn);

    m_browseBtn = m_ui.button(L"选择固件", [this] { BrowseFirmware(); });
    StyleSecondary(m_browseBtn);
    m_browseBtn.font(11);
    m_dash.push_back(m_browseBtn);

    m_stopBtn = m_ui.button(L"停止", [this] { StopFlash(); });
    StyleDanger(m_stopBtn);
    m_stopBtn.font(11).visible(false);
    m_dash.push_back(m_stopBtn);

    m_startBtn = m_ui.button(L"开始刷写", [this] { StartFlash(); });
    m_startBtn.primary(true).accent((int)m_accent).radius(10).font(13).bold(true);
    m_dash.push_back(m_startBtn);

    m_fwBrowse = m_ui.button(L"选择固件目录", [this] { BrowseFirmware(); });
    m_fwBrowse.primary(true).accent((int)m_accent).radius(10).font(12).bold(true);
    m_fwPage.push_back(m_fwBrowse);

    m_fwTable = m_ui.table({ L"固件包", L"类型", L"分区", L"大小", L"状态" },
                           {}, 0, 0, 10, 10);
    m_fwTable.bg(0x0A121F).fg(0xD8E5F8).border(0x1D2C44).radius(14).font(11);
    m_fwPage.push_back(m_fwTable);

    m_fwRemove = m_ui.button(L"移除当前包", [this] {
        if (!m_fw.valid) {
            m_app.toast(L"当前没有已加载的固件包");
            return;
        }
        ResetFirmware();
        PopulateTables();
        UpdateStartButton();
        Log(L"已移除当前固件包", LogLevel::Warn);
        m_app.toast(L"固件包已移除");
    });
    StyleGhost(m_fwRemove);
    m_fwPage.push_back(m_fwRemove);

    m_fwRescan = m_ui.button(L"重新扫描", [this] { RefreshFirmwarePage(); });
    StyleSecondary(m_fwRescan);
    m_fwPage.push_back(m_fwRescan);

    m_partSearch = m_ui.search(0, 0, 10, 34);
    m_partSearch.placeholder(L"搜索分区或镜像…").bg(0x0A121F).fg(col::ink)
                .border(0x22314A).radius(10).font(11);
    m_partSearch.onChanged([this] { RefreshPartitionTable(); });
    m_part.push_back(m_partSearch);

    m_partFilter = m_ui.combo({ L"全部分区", L"引导分区", L"系统分区", L"用户分区", L"通信分区" },
                              0, 0, 10, 34);
    m_partFilter.bg(0x0A121F).fg(col::inkSoft).border(0x22314A).radius(10).font(11);
    m_partFilter.onChanged([this] { RefreshPartitionTable(); });
    m_part.push_back(m_partFilter);

    m_partTable = m_ui.table({ L"分区标签", L"镜像文件", L"大小", L"类型", L"写入状态" },
                             {}, 0, 0, 10, 10);
    m_partTable.bg(0x0A121F).fg(0xD8E5F8).border(0x1D2C44).radius(14).font(11);
    m_part.push_back(m_partTable);

    m_partExport = m_ui.button(L"导出分区表", [this] { ExportPartitions(); });
    StyleSecondary(m_partExport);
    m_part.push_back(m_partExport);

    m_logArea = m_ui.textArea(0, 0, 10, 10);
    m_logArea.bg(0x060B13).fg(0xB8C8DE).border(0x1A293F).radius(12).font(11);
    m_log.push_back(m_logArea);

    m_logExport = m_ui.button(L"导出日志", [this] { ExportLog(); });
    StyleSecondary(m_logExport);
    m_log.push_back(m_logExport);

    m_logClear = m_ui.button(L"清空", [this] { ClearLog(); });
    StyleGhost(m_logClear);
    m_log.push_back(m_logClear);

    m_logAuto = m_ui.checkbox(L"自动滚动", 0, 0, 10, 26);
    m_logAuto.checked(true).font(11).fg(col::inkSoft);
    m_log.push_back(m_logAuto);

    m_setAnim = m_ui.checkbox(L"启用界面动效与粒子效果", 0, 0, 10, 26);
    m_setAnim.checked(true).font(11).fg(col::inkSoft);
    m_setAnim.onChanged([this] {
        m_animEnabled = m_setAnim.checked();
        Log(m_animEnabled ? L"界面动效已启用" : L"界面动效已暂停", LogLevel::Info);
        m_app.redraw();
    });
    m_set.push_back(m_setAnim);

    m_setVerify = m_ui.checkbox(L"默认校验固件完整性", 0, 0, 10, 26);
    m_setVerify.checked(true).font(11).fg(col::inkSoft);
    m_setVerify.onChanged([this] {
        m_optVerify.checked(m_setVerify.checked());
        Log(L"默认校验策略已更新", LogLevel::Info);
    });
    m_set.push_back(m_setVerify);

    m_setReboot = m_ui.checkbox(L"刷写完成后提示重启设备", 0, 0, 10, 26);
    m_setReboot.checked(true).font(11).fg(col::inkSoft);
    m_setReboot.onChanged([this] {
        m_optReboot.checked(m_setReboot.checked());
        Log(L"自动重启策略已更新", LogLevel::Info);
    });
    m_set.push_back(m_setReboot);

    m_setReset = m_ui.button(L"恢复默认设置", [this] { ResetSettings(); });
    StyleSecondary(m_setReset);
    m_set.push_back(m_setReset);

    m_setLogs = m_ui.button(L"打开运行日志", [this] { SetPage(3); });
    StyleGhost(m_setLogs);
    m_set.push_back(m_setLogs);

    m_setUpdate = m_ui.button(L"检查更新", [this] {
        if (m_updateState == UpdateState::Available)
            OpenUpdatePage();
        else
            CheckForUpdates();
    });
    StyleSecondary(m_setUpdate);
    m_setUpdate.font(11);
    m_set.push_back(m_setUpdate);

    for (Widget& w : m_fwPage) w.visible(false);
    for (Widget& w : m_part) w.visible(false);
    for (Widget& w : m_log) w.visible(false);
    for (Widget& w : m_set) w.visible(false);
}

void AppState::OnResize(int px, int py)
{
    ComputeLayout(px, py);
    Layout();
}

void AppState::ComputeLayout(int w, int h)
{
    m_lay.top = m_app.titleBarHeight();
    m_lay.W = (int)std::lround(w * 96.0 / std::max(1, m_app.dpi()));
    m_lay.H = (int)std::lround((h - m_lay.top) * 96.0 / std::max(1, m_app.dpi()));
    m_lay.W = std::max(900, m_lay.W);
    m_lay.H = std::max(650, m_lay.H);

    const int W = m_lay.W, H = m_lay.H;
    m_lay.R = W - 24;
    m_lay.L = 260;
    m_lay.railW = 280;
    m_lay.railX = m_lay.R - m_lay.railW;
    m_lay.mainW = m_lay.railX - 28 - m_lay.L;
    m_lay.actionY = H - 58;

    m_lay.sidebar = { 0, 0, 232, H };
    m_lay.brand = { 26, 18, 184, 44 };
    m_lay.hero = { m_lay.L, 90, m_lay.mainW, 226 };
    m_lay.stage = { m_lay.L, 328, m_lay.mainW, 170 };
    m_lay.firmware = { m_lay.L, 510, m_lay.mainW,
                       std::max(150, m_lay.actionY - 12 - 510) };
    m_lay.health = { m_lay.railX, 90, m_lay.railW, 216 };
    m_lay.options = { m_lay.railX, 318, m_lay.railW, 226 };
    m_lay.device = { m_lay.railX, 556, m_lay.railW,
                     std::max(106, m_lay.actionY - 12 - 556) };
    m_lay.action = { m_lay.L, m_lay.actionY, m_lay.R - m_lay.L, 46 };
    m_lay.deviceMini = { 16, H - 214, 200, 132 };
    m_lay.tipMini = { 16, 428, 200, 96 };
    m_lay.pill = { m_lay.R - 208, 24, 208, 36 };

    for (int i = 0; i < 5; ++i)
        m_lay.nav[(size_t)i] = { 14, 122 + i * 54, 204, 46 };
}

void AppState::Layout()
{
    const LLayout& l = m_lay;

    m_heroScan.rect(l.hero.x + 28, l.hero.y + 128, 150, 38);
    m_heroBrowse.rect(l.hero.x + 28 + 162, l.hero.y + 128, 132, 38);
    m_changeBtn.rect(l.firmware.r() - 76, l.firmware.y + 14, 58, 26);

    m_optVerify.rect(l.railX + 18, l.options.y + 48, l.railW - 34, 28);
    m_optReboot.rect(l.railX + 18, l.options.y + 78, l.railW - 34, 28);
    m_optBackup.rect(l.railX + 18, l.options.y + 108, l.railW - 34, 28);
    m_optKeep.rect(l.railX + 18, l.options.y + 138, l.railW - 34, 28);

    int right = l.action.r() - 24;
    int startW = 166, browseW = 126, stopW = 84;
    m_startBtn.rect(right - startW, l.actionY + 4, startW, 38);
    right -= startW + 10;
    m_browseBtn.rect(right - browseW, l.actionY + 4, browseW, 38);
    right -= browseW + 10;
    m_stopBtn.rect(right - stopW, l.actionY + 4, stopW, 38);

    int fullW = l.R - l.L;
    m_fwBrowse.rect(l.L + fullW / 2 - 78, 90 + 152, 156, 40);
    m_fwTable.rect(l.L, 326, fullW, std::max(150, l.actionY - 152 - 326));
    m_fwRemove.rect(l.L, l.actionY - 46, 132, 38);
    m_fwRescan.rect(l.L + 144, l.actionY - 46, 112, 38);

    m_partSearch.rect(l.R - 238, 88, 238, 36);
    m_partFilter.rect(l.R - 238 - 12 - 158, 88, 158, 36);
    m_partTable.rect(l.L, 146, fullW, std::max(180, l.actionY - 58 - 146));
    m_partExport.rect(l.R - 138, l.actionY - 42, 138, 36);

    m_logArea.rect(l.L + 16, 146, fullW - 32, std::max(150, l.H - 42 - 146));
    m_logExport.rect(l.R - 118, 98, 118, 34);
    m_logClear.rect(l.R - 118 - 10 - 84, 98, 84, 34);
    m_logAuto.rect(l.L + 16, 108, 140, 26);

    m_setAnim.rect(l.L + 26, 90 + 198, 330, 28);
    m_setVerify.rect(l.L + 26, 90 + 234, 330, 28);
    m_setReboot.rect(l.L + 26, 90 + 270, 330, 28);
    m_setReset.rect(l.L + 26, 90 + 312, 138, 36);
    int rightCardX = l.L + 506;
    m_setLogs.rect(rightCardX + 26, 90 + 340, 138, 36);
    m_setUpdate.rect(rightCardX + 176, 90 + 340, 150, 36);

    m_app.redraw();
}

void AppState::AttachWidgets() {}

void AppState::SetPage(int page)
{
    m_page = std::max(0, std::min(4, page));
    for (Widget& w : m_dash) w.visible(m_page == 0);
    for (Widget& w : m_fwPage) w.visible(m_page == 1);
    for (Widget& w : m_part) w.visible(m_page == 2);
    for (Widget& w : m_log) w.visible(m_page == 3);
    for (Widget& w : m_set) w.visible(m_page == 4);
    m_pageFade = 1.f;
    if (m_page == 2) RefreshPartitionTable();
    if (m_page == 3) SyncLog();
    if (m_page == 1) PopulateTables();
    m_app.redraw();
}

void AppState::Begin()
{
    if (const char* p = std::getenv("YUKI9008_PAGE")) {
        int n = atoi(p);
        if (n > 0 && n <= 4) SetPage(n);
    }
    m_autoStart = std::getenv("YUKI9008_AUTO_START") != nullptr;
    if (const char* fw = std::getenv("YUKI9008_FIRMWARE"))
        LoadFirmwareDir(FromUtf8(fw));
    Log(L"Yuki9008工具 内核 v1.0.1 已启动", LogLevel::Ok);
    Log(L"加载安全策略 · 校验 / 备份 / 防误擦", LogLevel::Info);
    if (m_engineReady)
        Log(L"写入引擎已就绪 · " + m_engineDir, LogLevel::Ok);
    else
        Log(L"未检测到 QFIL 写入引擎（fh_loader / QSaharaServer）", LogLevel::Warn);
    UpdateStartButton();
    SetStatus(L"正在扫描 EDL 总线", L"查找 Qualcomm QDLoader 9008", m_accent);
    StartScan();
}

void AppState::ApplyAccent(bool announce)
{
    m_theme = m_app.theme();
    m_theme.accent = m_accent;
    m_theme.accentHover = lerpColor(m_accent, 0xFFFFFF, 0.22f);
    m_theme.accentPressed = lerpColor(m_accent, 0x000000, 0.18f);
    m_theme.trackFill = m_accent;
    m_theme.inputFocus = m_accent;
    m_app.setTheme(m_theme);

    m_heroScan.accent((int)m_accent);
    m_fwBrowse.accent((int)m_accent);
    m_startBtn.accent((int)m_accent);

    if (announce) {
        Log(L"强调色已切换", LogLevel::Info);
        m_app.toast(L"强调色已更新");
    }
    m_app.redraw();
}

void AppState::StartScan()
{
    if (m_running) return;
    if (m_scanning) return;
    m_scanning = true;
    m_scanApplied = false;
    m_scanT = 0.f;
    m_done = false;
    m_failed = false;
    m_aborted = false;
    m_hasProgress = false;
    m_target = 0.f;
    m_display = 0.f;
    m_currentPart = -1;
    m_stage = 0;
    SetStatus(L"正在扫描 EDL 总线", L"VID_05C6 · PID_9008 · USB 2.0 / 3.0", m_accent);
    Log(L"扫描 USB 总线 · 查找 QDLoader 9008", LogLevel::Info);
    UpdateStartButton();
    m_app.redraw();
}

void AppState::DetectDevice()
{
    m_dev = DeviceInfo();
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr,
                                        DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return;

    SP_DEVINFO_DATA data = {};
    data.cbSize = sizeof(data);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &data); ++i) {
        wchar_t inst[512] = {};
        if (!SetupDiGetDeviceInstanceIdW(set, &data, inst, 512, nullptr)) continue;
        std::wstring id = inst;
        bool edl = Contains(id, L"VID_05C6&PID_9008") ||
                   Contains(id, L"VID_05C6&PID_900E");
        if (!edl) continue;

        wchar_t friendly[512] = {};
        DWORD type = 0, need = 0;
        if (!SetupDiGetDeviceRegistryPropertyW(set, &data, SPDRP_FRIENDLYNAME,
                                               &type, (PBYTE)friendly, sizeof(friendly), &need)) {
            if (!SetupDiGetDeviceRegistryPropertyW(set, &data, SPDRP_DEVICEDESC,
                                                   &type, (PBYTE)friendly, sizeof(friendly), &need))
                wcscpy_s(friendly, L"Qualcomm HS-USB QDLoader 9008");
        }
        m_dev.present = true;
        m_dev.name = friendly;
        m_dev.serial = id;
        m_dev.pid = Contains(id, L"PID_900E") ? L"900E" : L"9008";

        std::wregex comRe(L"\\(COM([0-9]+)\\)", std::regex::icase);
        std::wsmatch match;
        std::wstring fname = friendly;
        if (std::regex_search(fname, match, comRe) && match.size() > 1)
            m_dev.port = L"COM" + match[1].str();
        else
            m_dev.port = L"USB 3.0";
        break;
    }
    SetupDiDestroyDeviceInfoList(set);
}

void AppState::ResetFirmware()
{
    m_fw = FirmwareInfo();
    m_done = false;
    m_failed = false;
    m_aborted = false;
    m_target = 0.f;
    m_display = 0.f;
    m_hasProgress = false;
    m_currentPart = -1;
}

void AppState::DetectEngine()
{
    m_engineDir.clear();
    m_engineSahara.clear();
    m_engineFhLoader.clear();
    m_engineReady = false;
    m_engineNote.clear();

    auto fileExists = [](const std::wstring& p) {
        DWORD a = GetFileAttributesW(p.c_str());
        return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
    };

    std::vector<std::wstring> dirs;
    if (const wchar_t* env = _wgetenv(L"YUKI9008_ENGINE_DIR"))
        if (*env) dirs.push_back(env);

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t slash = exeDir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        exeDir.resize(slash);
        dirs.push_back(exeDir);
        dirs.push_back(exeDir + L"\\bin");
        dirs.push_back(exeDir + L"\\QFIL");
    }

    const wchar_t* common[] = {
        L"C:\\Program Files (x86)\\Qualcomm\\QPST\\bin",
        L"C:\\Program Files\\Qualcomm\\QPST\\bin",
        L"C:\\Program Files (x86)\\Qualcomm\\QFIL",
        L"C:\\Program Files\\Qualcomm\\QFIL",
        L"C:\\Program Files (x86)\\Qualcomm\\QPST",
        L"C:\\Program Files\\Qualcomm\\QPST",
        L"C:\\Qualcomm\\QPST\\bin",
    };
    for (const wchar_t* d : common) dirs.push_back(d);

    wchar_t pathEnv[32768] = {};
    DWORD pathLen = GetEnvironmentVariableW(L"PATH", pathEnv, 32768);
    if (pathLen > 0 && pathLen < 32768) {
        std::wstring all(pathEnv, pathLen);
        size_t start = 0;
        while (start <= all.size()) {
            size_t semi = all.find(L';', start);
            std::wstring part = all.substr(start, semi == std::wstring::npos
                                                  ? std::wstring::npos : semi - start);
            if (!part.empty()) dirs.push_back(part);
            if (semi == std::wstring::npos) break;
            start = semi + 1;
        }
    }

    for (const std::wstring& raw : dirs) {
        if (raw.empty()) continue;
        std::wstring d = raw;
        while (!d.empty() && (d.back() == L'\\' || d.back() == L'/')) d.pop_back();
        std::wstring sahara = d + L"\\QSaharaServer.exe";
        std::wstring fh = d + L"\\fh_loader.exe";
        if (m_engineSahara.empty() && fileExists(sahara)) m_engineSahara = sahara;
        if (m_engineFhLoader.empty() && fileExists(fh)) m_engineFhLoader = fh;
        if (!m_engineSahara.empty() && !m_engineFhLoader.empty()) {
            m_engineDir = d;
            m_engineReady = true;
            m_engineNote = L"QFIL 写入引擎已就绪";
            return;
        }
    }

    if (!m_engineSahara.empty() && m_engineFhLoader.empty())
        m_engineNote = L"已找到 QSaharaServer，缺少 fh_loader.exe";
    else if (m_engineSahara.empty() && !m_engineFhLoader.empty())
        m_engineNote = L"已找到 fh_loader，缺少 QSaharaServer.exe";
    else if (!m_engineSahara.empty() || !m_engineFhLoader.empty())
        m_engineNote = L"写入引擎不完整：需要 QSaharaServer.exe 与 fh_loader.exe";
    else
        m_engineNote = L"未检测到 QFIL 写入引擎";
}

void AppState::BrowseFirmware()
{
    std::wstring dir = PickFolder(m_app.hwnd(), L"选择 9008 固件目录（包含 rawprogram*.xml）");
    if (dir.empty()) return;
    LoadFirmwareDir(dir);
}

void AppState::LoadFirmwareDir(const std::wstring& dir)
{
    SetCursor(LoadCursorA(nullptr, IDC_WAIT));
    Log(L"扫描固件目录 · " + dir, LogLevel::Info);
    m_app.redraw();

    FirmwareInfo out;
    bool ok = ScanFirmware(dir, out);
    SetCursor(LoadCursorA(nullptr, IDC_ARROW));

    if (!ok) {
        Log(L"固件目录无效：缺少 rawprogram XML 或分区映射", LogLevel::Error);
        m_app.msg(L"没有找到可用的 9008 固件结构。\n\n请选择包含 rawprogram*.xml、patch*.xml "
                  L"与 prog_firehose*.elf / *.mbn 的解压目录。",
                  L"无法读取固件包");
        return;
    }

    m_fw = std::move(out);
    m_done = false;
    m_failed = false;
    m_aborted = false;
    m_target = 0.f;
    m_display = 0.f;
    m_hasProgress = false;
    m_currentPart = -1;
    if (m_fw.programmer.empty())
        Log(L"缺少 Firehose 编程器，无法启动真实写入", LogLevel::Warn);
    Log(L"固件包已加载 · " + m_fw.name + L" · " + std::to_wstring(m_fw.parts.size()) +
        L" 个分区 · " + FormatSize(m_fw.totalBytes), LogLevel::Ok);
    SetStatus(L"固件包已就绪", m_fw.name + L" · " + FormatSize(m_fw.totalBytes), col::green);
    m_stopBtn.visible(false);
    PopulateTables();
    UpdateStartButton();
    m_app.toast((L"已载入 " + m_fw.name).c_str(), 3200);
}

bool AppState::ScanFirmware(const std::wstring& dir, FirmwareInfo& out)
{
    std::error_code ec;
    fs::path root(dir);
    if (!fs::is_directory(root, ec)) return false;

    out = FirmwareInfo();
    out.root = dir;
    out.name = FolderName(dir);

    std::vector<fs::path> xmls;
    std::vector<fs::path> programmers;
    int rawCount = 0;
    int guard = 0;

    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec && guard < 20000; it.increment(ec), ++guard) {
        if (!it->is_regular_file(ec)) continue;
        std::wstring fname = it->path().filename().wstring();
        std::wstring low = Lower(fname);
        if (EndsWith(fname, L".xml")) {
            if (low.find(L"rawprogram") != std::wstring::npos) {
                ++rawCount;
                xmls.push_back(it->path());
            } else if (low.find(L"patch") != std::wstring::npos) {
                if (out.patch.empty()) out.patch = it->path().wstring();
            }
        }
        if (EndsWith(fname, L".elf") || EndsWith(fname, L".mbn") || EndsWith(fname, L".bin")) {
            if (low.find(L"firehose") != std::wstring::npos ||
                low.find(L"prog") != std::wstring::npos ||
                low.find(L"sahara") != std::wstring::npos)
                programmers.push_back(it->path());
        }
    }
    if (rawCount == 0 || xmls.empty()) return false;

    std::sort(xmls.begin(), xmls.end());
    out.rawprogram = xmls.front().wstring();
    out.rawCount = rawCount;
    if (!programmers.empty()) {
        std::sort(programmers.begin(), programmers.end(), [](const fs::path& a, const fs::path& b) {
            return Lower(a.filename().wstring()).find(L"firehose") != std::wstring::npos &&
                   Lower(b.filename().wstring()).find(L"firehose") == std::wstring::npos;
        });
        out.programmer = programmers.front().wstring();
    }

    std::vector<std::wstring> seen;
    std::regex programRe(R"re(<program\b([^>]*)>)re", std::regex::icase);
    std::regex labelRe(R"re(label\s*=\s*"([^"]*)")re", std::regex::icase);
    std::regex fileRe(R"re(filename\s*=\s*"([^"]*)")re", std::regex::icase);
    std::regex sizeRe(R"re(size_in_KB\s*=\s*"([^"]*)")re", std::regex::icase);
    uint64_t total = 0;
    std::vector<std::wstring> uniqueFiles;

    for (const fs::path& xml : xmls) {
        std::ifstream in(xml, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (text.empty()) continue;
        auto begin = std::sregex_iterator(text.begin(), text.end(), programRe);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            std::string attrs = (*it)[1].str();
            std::smatch m;
            std::wstring label, filename, sizeKb;
            if (std::regex_search(attrs, m, labelRe)) label = FromUtf8(m[1].str());
            if (std::regex_search(attrs, m, fileRe)) filename = FromUtf8(m[1].str());
            if (std::regex_search(attrs, m, sizeRe)) sizeKb = FromUtf8(m[1].str());
            if (label.empty()) label = filename.empty() ? L"unnamed" : FileName(filename);
            std::wstring key = Lower(label);
            if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
            seen.push_back(key);

            uint64_t bytes = 0;
            bool counted = false;
            if (!filename.empty()) {
                fs::path fp = root / fs::path(filename);
                bytes = FileSizeSafe(fp);
                if (bytes > 0) {
                    std::wstring fpw = fp.wstring();
                    if (std::find(uniqueFiles.begin(), uniqueFiles.end(), fpw) == uniqueFiles.end()) {
                        uniqueFiles.push_back(fpw);
                        total += bytes;
                    }
                    counted = true;
                }
            }
            if (bytes == 0 && !sizeKb.empty()) {
                try { bytes = (uint64_t)std::stoull(sizeKb) * 1024ull; } catch (...) { bytes = 0; }
            }
            if (!counted && bytes > 0) total += bytes;
            PartInfo p;
            p.label = label;
            p.file = filename.empty() ? L"—" : filename;
            p.kind = KindOf(label);
            p.bytes = bytes;
            out.parts.push_back(std::move(p));
            if (out.parts.size() >= 512) break;
        }
        if (out.parts.size() >= 512) break;
    }

    if (out.parts.empty()) return false;
    if (total == 0) {
        for (const PartInfo& p : out.parts) total += p.bytes;
    }
    out.totalBytes = total;
    out.valid = true;
    out.warning = out.programmer.empty();
    if (out.patch.empty()) out.patch = L"—";

    std::string manifest = "YUKI9008\n";
    for (const PartInfo& p : out.parts) {
        manifest += ToUtf8(p.label);
        manifest += "|";
        manifest += std::to_string(p.bytes);
        manifest += "\n";
    }
    out.hash = Sha256Bytes(manifest);
    return true;
}

void AppState::RefreshFirmwarePage()
{
    if (!m_fw.valid) {
        BrowseFirmware();
        return;
    }
    LoadFirmwareDir(m_fw.root);
}

void AppState::PopulateTables()
{
    std::vector<std::vector<std::wstring>> rows;
    if (m_fw.valid) {
        rows.push_back({ m_fw.name, L"LOCAL",
                         std::to_wstring(m_fw.parts.size()),
                         FormatSize(m_fw.totalBytes), L"就绪" });
    }
    m_fwTable.rows(rows);
    if (Control* c = m_app.control(m_fwTable.id())) c->selectedRow = m_fw.valid ? 0 : -1;
    RefreshPartitionTable();
}

void AppState::RefreshPartitionTable()
{
    std::wstring q = Lower(m_partSearch.text());
    int filter = std::max(0, m_partFilter.selected());
    std::vector<std::vector<std::wstring>> rows;
    for (int i = 0; i < (int)m_fw.parts.size(); ++i) {
        const PartInfo& p = m_fw.parts[(size_t)i];
        if (!q.empty() && Lower(p.label).find(q) == std::wstring::npos &&
            Lower(p.file).find(q) == std::wstring::npos)
            continue;
        if (filter > 0) {
            const wchar_t* want = filter == 1 ? L"引导" : filter == 2 ? L"系统" :
                                  filter == 3 ? L"用户" : L"通信";
            if (p.kind != want) continue;
        }
        std::wstring state = L"待写入";
        if (m_running) {
            if (m_currentPart < 0) state = L"等待";
            else if (i < m_currentPart) state = L"已写入";
            else if (i == m_currentPart) state = L"写入中";
            else state = L"等待";
        } else if (m_done) {
            state = L"完成";
        }
        rows.push_back({ p.label, p.file, FormatSize(p.bytes), p.kind, state });
    }
    m_partTable.rows(rows);
}

void AppState::TriggerStart()
{
    StartFlash();
}

void AppState::TriggerScan()
{
    StartScan();
}

void AppState::TriggerLog()
{
    SetPage(3);
}

void AppState::UpdateStartButton()
{
    if (!m_startBtn.valid()) return;
    const bool hasCom = !m_dev.port.empty() && m_dev.port.rfind(L"COM", 0) == 0;
    const bool ready = m_fw.valid && !m_fw.programmer.empty() &&
                       m_dev.present && hasCom && m_engineReady &&
                       !m_running && !m_scanning;

    m_startBtn.primary(m_running || ready);
    if (m_running) {
        m_startBtn.text(L"正在刷写…").enabled(false);
        m_stopBtn.visible(true).enabled(true);
    } else {
        if (!m_engineReady)
            m_startBtn.text(L"写入引擎未就绪");
        else if (!m_fw.valid)
            m_startBtn.text(L"选择固件后开始");
        else if (m_fw.programmer.empty())
            m_startBtn.text(L"缺少引导程序");
        else if (!m_dev.present)
            m_startBtn.text(L"连接设备后开始");
        else if (!hasCom)
            m_startBtn.text(L"端口不可用");
        else if (m_done)
            m_startBtn.text(L"再次刷写");
        else if (m_failed)
            m_startBtn.text(L"重试写入");
        else
            m_startBtn.text(L"开始刷写");
        m_startBtn.enabled(ready);
        m_stopBtn.visible(false);
    }

    m_browseBtn.enabled(!m_running);
    m_heroScan.enabled(!m_running);
    m_changeBtn.enabled(!m_running);
    m_fwRemove.enabled(!m_running);
    m_fwRescan.enabled(!m_running);
    m_fwBrowse.enabled(!m_running);

    if (!m_fw.valid)
        m_startBtn.tooltip(L"请先选择固件目录");
    else if (m_fw.programmer.empty())
        m_startBtn.tooltip(L"固件中缺少 Firehose 编程器");
    else if (!m_engineReady)
        m_startBtn.tooltip(L"未检测到 QFIL 写入引擎");
    else if (!m_dev.present)
        m_startBtn.tooltip(L"请连接 9008 设备");
    else if (!hasCom)
        m_startBtn.tooltip(L"设备未暴露 COM 端口");
    else
        m_startBtn.tooltip(L"开始真实 Firehose 写入");
}

std::wstring AppState::ShellQuote(const std::wstring& s) const
{
    return L"\"" + s + L"\"";
}

std::wstring AppState::PortName() const
{
    if (m_dev.port.rfind(L"COM", 0) == 0) return m_dev.port;
    return L"";
}

std::wstring AppState::MemoryType() const
{
    std::wstring probe = Lower(m_fw.root + L" " + m_fw.programmer + L" " + m_fw.rawprogram);
    if (probe.find(L"emmc") != std::wstring::npos) return L"emmc";
    return L"ufs";
}

void AppState::CloseProcessHandles()
{
    if (m_proc) {
        CloseHandle(m_proc);
        m_proc = nullptr;
    }
    if (m_procRead) {
        CloseHandle(m_procRead);
        m_procRead = nullptr;
    }
}

bool AppState::LaunchPhase(int phase)
{
    if (!m_engineReady || !m_dev.present) return false;
    std::wstring com = PortName();
    if (com.empty()) return false;

    std::wstring command;
    if (phase == 1) {
        std::wstring comNum = com.substr(3);
        command = ShellQuote(m_engineSahara) + L" -u " + comNum +
                  L" -s 13:" + ShellQuote(m_fw.programmer);
    } else {
        std::wstring xml = phase == 3 ? m_fw.patch : m_fw.rawprogram;
        command = ShellQuote(m_engineFhLoader) + L" --port=\\\\.\\" + com +
                  L" --sendxml=" + ShellQuote(xml) +
                  L" --search_path=" + ShellQuote(m_fw.root) +
                  L" --memoryname=" + MemoryType() +
                  L" --noprompt --showpercentagecomplete --zlpawarehost=1";
    }

    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring logPath = std::wstring(tempDir) + L"yuki9008-flash-" +
                           std::to_wstring(GetCurrentProcessId()) + L"-" +
                           std::to_wstring(GetTickCount64()) + L"-" +
                           std::to_wstring(phase) + L".log";

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE writeHandle = CreateFileW(logPath.c_str(),
                                     GENERIC_WRITE | GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (writeHandle == INVALID_HANDLE_VALUE) {
        FailFlash(L"无法创建写入日志文件");
        return false;
    }
    HANDLE readHandle = CreateFileW(logPath.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE nulHandle = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (readHandle == INVALID_HANDLE_VALUE || nulHandle == INVALID_HANDLE_VALUE) {
        if (readHandle != INVALID_HANDLE_VALUE) CloseHandle(readHandle);
        if (nulHandle != INVALID_HANDLE_VALUE) CloseHandle(nulHandle);
        CloseHandle(writeHandle);
        FailFlash(L"无法准备写入引擎的管道");
        return false;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writeHandle;
    si.hStdError = writeHandle;
    si.hStdInput = nulHandle;

    PROCESS_INFORMATION pi = {};
    std::wstring cmd = command;
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                             nullptr, m_fw.root.c_str(), &si, &pi);
    CloseHandle(writeHandle);
    CloseHandle(nulHandle);
    if (!ok) {
        CloseHandle(readHandle);
        FailFlash(L"无法启动写入引擎：" + command);
        return false;
    }
    CloseHandle(pi.hThread);
    CloseProcessHandles();
    m_proc = pi.hProcess;
    m_procRead = readHandle;
    m_procLogPath = logPath;
    m_procPhase = phase;
    m_procPending.clear();
    m_procStartMs = GetTickCount64();
    m_progressMs = m_procStartMs;

    if (phase == 1) {
        m_stage = 1;
        m_target = 0.f;
        m_display = 0.f;
        m_hasProgress = false;
        SetStatus(L"正在发送引导程序", L"Sahara · Firehose programmer", m_accent);
        Log(L"启动 QSaharaServer · " + com, LogLevel::Flash);
    } else if (phase == 2) {
        m_stage = 2;
        m_target = 0.f;
        m_display = 0.f;
        m_hasProgress = false;
        m_currentPart = -1;
        SetStatus(L"正在写入分区", L"fh_loader · 请保持 USB 连接", m_accent);
        Log(L"启动 fh_loader · " + com + L" · " + MemoryType(), LogLevel::Flash);
    } else {
        m_stage = 3;
        m_target = std::max(m_target, 0.99f);
        m_hasProgress = true;
        SetStatus(L"正在应用分区补丁", L"patch XML · " + FileName(m_fw.patch), col::violet);
        Log(L"应用 patch XML · " + FileName(m_fw.patch), LogLevel::Flash);
    }
    Log(L"> " + command, LogLevel::Info);
    m_app.redraw();
    return true;
}

void AppState::ReadProcessOutput()
{
    if (!m_procRead) return;
    char buf[4096];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(m_procRead, buf, (DWORD)sizeof(buf), &got, nullptr) || got == 0)
            break;
        m_procPending.append(buf, got);
        if (m_procPending.size() > 128 * 1024) {
            m_procPending.erase(0, m_procPending.size() - 64 * 1024);
        }
    }

    for (;;) {
        size_t nl = m_procPending.find_first_of("\r\n");
        if (nl == std::string::npos) break;
        std::string raw = m_procPending.substr(0, nl);
        m_procPending.erase(0, nl + 1);
        while (!m_procPending.empty() &&
               (m_procPending[0] == '\r' || m_procPending[0] == '\n'))
            m_procPending.erase(0, 1);
        while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n' || raw.back() == ' '))
            raw.pop_back();
        size_t first = raw.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        raw.erase(0, first);
        if (raw.size() > 600) raw.resize(600);

        std::wstring line = FromUtf8(raw);
        if (line.empty()) line.assign(raw.begin(), raw.end());
        Log(line, LogLevel::Info);

        size_t pct = line.find(L'%');
        if (pct == std::wstring::npos || pct == 0) continue;
        size_t b = pct;
        while (b > 0 && iswdigit(line[b - 1])) --b;
        if (b == pct) continue;
        int value = _wtoi(line.substr(b, pct - b).c_str());
        if (value < 0 || value > 100) continue;
        float next = value / 100.f;
        if (next <= m_target) continue;

        ULONGLONG now = GetTickCount64();
        float totalMB = std::max(1.f, (float)m_fw.totalBytes / (1024.f * 1024.f));
        if (m_progressMs != 0 && now > m_progressMs) {
            double sec = (now - m_progressMs) / 1000.0;
            float deltaMB = (next - m_target) * totalMB;
            if (sec > 0.08 && deltaMB > 0.f) {
                float sp = (float)(deltaMB / sec);
                if (sp > 1.f && sp < 4000.f) {
                    m_speed = sp;
                    m_speedHist.push_back(sp);
                    if (m_speedHist.size() > 42) m_speedHist.pop_front();
                }
            }
        }
        m_target = next;
        m_hasProgress = true;
        m_progressMs = now;
        m_writtenMB = totalMB * m_target;
        m_remaining = m_speed > 1.f ? std::max(0.f, (totalMB - m_writtenMB) / m_speed) : 0.f;

        int n = (int)m_fw.parts.size();
        if (n > 0) {
            int idx = std::min(n - 1, (int)(m_target * n));
            if (idx != m_currentPart) {
                m_currentPart = idx;
                RefreshPartitionTable();
            }
        }
    }
}

void AppState::StartFlash()
{
    if (m_running) return;
    if (m_scanning) {
        m_app.toast(L"设备扫描尚未完成，请稍候");
        return;
    }
    if (!m_fw.valid) {
        m_app.toast(L"请先选择有效的 9008 固件包");
        return;
    }
    if (m_fw.programmer.empty()) {
        m_app.msg(L"固件包缺少 Firehose 编程器（prog_firehose*.elf / *.mbn）。\n\n"
                  L"真实写入无法启动。", L"缺少写入程序");
        return;
    }
    if (!m_engineReady) {
        m_app.msg(L"未找到 Qualcomm 写入引擎。\n\n"
                  L"请安装 QFIL / QPST，并确保 QSaharaServer.exe 与 fh_loader.exe "
                  L"位于安装目录或 PATH 中。", L"写入引擎未就绪");
        return;
    }
    if (!m_dev.present) {
        m_app.toast(L"未检测到 9008 设备，请连接后重试");
        return;
    }
    if (PortName().empty()) {
        m_app.msg(L"设备已连接，但没有暴露 COM 端口。\n\n"
                  L"当前写入引擎需要 Qualcomm QDLoader 串口（COMx）。",
                  L"设备端口不可用");
        return;
    }

    if (m_page != 0) SetPage(0);
    m_running = true;
    m_done = false;
    m_failed = false;
    m_aborted = false;
    m_stage = 1;
    m_target = 0.f;
    m_display = 0.f;
    m_hasProgress = false;
    m_currentPart = -1;
    m_speed = 0.f;
    m_writtenMB = 0.f;
    m_remaining = 0.f;
    m_speedHist.clear();
    m_confetti.clear();
    m_confettiT = 999.f;
    m_procPhase = 0;
    m_patchPending = false;
    if (m_fw.patch != L"—" && !m_fw.patch.empty()) {
        std::error_code ec;
        m_patchPending = fs::is_regular_file(fs::path(m_fw.patch), ec);
    }
    SetStatus(L"正在建立 EDL 会话", L"请勿断开 USB 连接", m_accent);
    Log(L"开始真实刷写会话 · " + m_fw.name, LogLevel::Flash);
    Log(L"设备 " + m_dev.name + L" · " + m_dev.port, LogLevel::Flash);
    UpdateStartButton();
    if (!LaunchPhase(1)) {
        m_app.redraw();
        return;
    }
    m_app.toast(L"已启动 Firehose 写入", 2200);
    m_app.redraw();
}

void AppState::StopFlash()
{
    if (!m_running) return;
    m_running = false;
    m_aborted = true;
    m_failed = false;
    if (m_proc) TerminateProcess(m_proc, 1);
    CloseProcessHandles();
    m_speed = 0.f;
    m_remaining = 0.f;
    m_target = m_display;
    SetStatus(L"刷写已中止", L"写入进程已终止 · 设备可能仍在 EDL", col::amber);
    Log(L"用户中止刷写会话", LogLevel::Warn);
    UpdateStartButton();
    m_app.toast(L"刷写已中止");
    m_app.redraw();
}

void AppState::PollFlash()
{
    if (!m_running) return;
    ReadProcessOutput();
    if (!m_proc) return;

    DWORD code = 0;
    if (!GetExitCodeProcess(m_proc, &code)) {
        FailFlash(L"无法读取写入进程状态");
        return;
    }
    if (code == STILL_ACTIVE) {
        if (GetTickCount64() - m_procStartMs > 20ull * 60ull * 1000ull)
            FailFlash(L"写入进程超时（20 分钟）");
        return;
    }

    ReadProcessOutput();
    int phase = m_procPhase;
    CloseProcessHandles();
    m_procPhase = 0;

    if (code != 0) {
        wchar_t buf[64];
        swprintf(buf, 64, L"%lu", (unsigned long)code);
        FailFlash(L"写入工具退出码 " + std::wstring(buf));
        return;
    }

    if (phase == 1) {
        m_stage = 2;
        Log(L"Sahara 编程器发送完成 · 启动 fh_loader", LogLevel::Ok);
        if (!LaunchPhase(2)) return;
    } else if (phase == 2) {
        if (m_patchPending) {
            if (!LaunchPhase(3)) return;
        } else {
            FinishFlash();
        }
    } else {
        FinishFlash();
    }
}

void AppState::FinishFlash()
{
    if (!m_running) return;
    m_running = false;
    m_done = true;
    m_failed = false;
    m_aborted = false;
    m_stage = 4;
    m_target = 1.f;
    m_display = 1.f;
    m_hasProgress = true;
    m_currentPart = (int)m_fw.parts.size();
    m_speed = 0.f;
    m_remaining = 0.f;
    CloseProcessHandles();

    m_dev = DeviceInfo();
    SetStatus(L"刷写完成", L"所有分区写入完成 · 请手动重启设备", col::green);
    Log(L"刷写完成 · " + std::to_wstring(m_fw.parts.size()) +
        L" 个分区写入完成 · 请重启设备", LogLevel::Ok);
    if (m_optReboot.checked())
        m_app.toast(L"刷写完成 · 请重启设备", 4200);

    m_confetti.clear();
    m_confettiT = 0.f;
    srand((unsigned)GetTickCount());
    for (int i = 0; i < 64; ++i) {
        Confetti c;
        c.x = (float)(rand() % std::max(1, m_lay.W));
        c.y = (float)(-20 - rand() % 220);
        c.vx = (float)(rand() % 80 - 40);
        c.vy = (float)(rand() % 90 + 40);
        c.rot = (float)(rand() % 360);
        c.vr = (float)(rand() % 280 - 140);
        c.size = (float)(rand() % 5 + 3);
        const DWORD palette[] = { m_accent, m_accent2, col::green, 0xFFFFFF, col::amber };
        c.color = palette[rand() % 5];
        m_confetti.push_back(c);
    }
    RefreshPartitionTable();
    UpdateStartButton();
    m_app.redraw();
}

void AppState::FailFlash(const std::wstring& reason)
{
    m_running = false;
    m_done = false;
    m_failed = true;
    m_aborted = false;
    m_speed = 0.f;
    m_remaining = 0.f;
    CloseProcessHandles();
    SetStatus(L"写入失败", reason, col::red);
    Log(L"写入失败 · " + reason, LogLevel::Error);
    UpdateStartButton();
    m_app.toast(L"写入失败，请查看运行日志", 4200);
    m_app.redraw();
}

void AppState::Log(const std::wstring& text, LogLevel level)
{
    LogItem it;
    it.time = Timestamp();
    it.tag = LevelTag(level);
    it.text = text;
    it.level = level;
    m_logs.push_back(it);
    if (m_logs.size() > 400) m_logs.erase(m_logs.begin());
    SyncLog();
}

void AppState::SyncLog()
{
    std::wstring s;
    s.reserve(m_logs.size() * 80);
    for (const LogItem& l : m_logs) {
        s += l.time;
        s += L"  ";
        s += l.tag;
        s += L"  ";
        s += l.text;
        s += L"\n";
    }
    m_logText = std::move(s);
    if (!m_logArea.valid()) return;
    m_logArea.text(m_logText);
    if (Control* c = m_app.control(m_logArea.id())) {
        int lines = (int)m_logs.size();
        int lh = m_app.dp(15);
        int desired = lines * lh - c->h + m_app.dp(18);
        if (m_logAuto.valid() && !m_logAuto.checked()) {
            if (c->viewY < 0) c->viewY = 0;
        } else {
            c->viewY = std::max(0, desired);
        }
    }
}

void AppState::ClearLog()
{
    m_logs.clear();
    m_logText.clear();
    if (m_logArea.valid()) m_logArea.text(L"");
    Log(L"日志已清空", LogLevel::Info);
    m_app.toast(L"日志已清空");
}

void AppState::ExportLog()
{
    std::wstring path = PickSave(m_app.hwnd(), L"导出 Yuki9008工具 运行日志",
                                 L"yuki9008-log.txt", L"文本日志 (*.txt)", L"*.txt");
    if (path.empty()) return;
    std::string utf8;
    int need = WideCharToMultiByte(CP_UTF8, 0, m_logText.c_str(), (int)m_logText.size(),
                                   nullptr, 0, nullptr, nullptr);
    utf8.resize((size_t)std::max(0, need));
    if (need > 0)
        WideCharToMultiByte(CP_UTF8, 0, m_logText.c_str(), (int)m_logText.size(),
                            utf8.data(), need, nullptr, nullptr);
    std::ofstream out(fs::path(path), std::ios::binary | std::ios::trunc);
    out.write("\xEF\xBB\xBF", 3);
    out.write(utf8.data(), (std::streamsize)utf8.size());
    out.close();
    Log(L"运行日志已导出 · " + path, LogLevel::Ok);
    m_app.toast(L"日志已导出");
}

void AppState::ExportPartitions()
{
    std::wstring path = PickSave(m_app.hwnd(), L"导出分区映射",
                                 L"yuki9008-partitions.csv", L"CSV 表格 (*.csv)", L"*.csv");
    if (path.empty()) return;
    std::ofstream out(fs::path(path), std::ios::binary | std::ios::trunc);
    out << "\xEF\xBB\xBF";
    out << "label,file,size_bytes,kind\n";
    for (const PartInfo& p : m_fw.parts) {
        out << ToUtf8(p.label) << "," << ToUtf8(p.file) << "," << p.bytes << ","
            << ToUtf8(p.kind) << "\n";
    }
    out.close();
    Log(L"分区映射已导出 · " + path, LogLevel::Ok);
    m_app.toast(L"分区映射已导出");
}

void AppState::OpenFirmwareFolder()
{
    if (!m_fw.valid || m_fw.root.empty()) {
        m_app.toast(L"尚未加载固件包");
        return;
    }
    ShellExecuteW(m_app.hwnd(), L"open", m_fw.root.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void AppState::ResetSettings()
{
    m_setAnim.checked(true);
    m_setVerify.checked(true);
    m_setReboot.checked(true);
    m_animEnabled = true;
    m_optVerify.checked(true);
    m_optReboot.checked(true);
    m_optBackup.checked(false);
    m_optKeep.checked(true);
    m_accent = 0x4D9DFF;
    m_accent2 = 0x9179FF;
    ApplyAccent(false);
    Log(L"设置已恢复默认", LogLevel::Ok);
    m_app.toast(L"已恢复默认设置");
}

void AppState::UpdateUpdateButton()
{
    if (!m_setUpdate.valid()) return;
    switch (m_updateState) {
    case UpdateState::Checking:
        m_setUpdate.text(L"检查中…").enabled(false);
        m_setUpdate.tooltip(L"正在连接 GitHub Releases");
        break;
    case UpdateState::Available:
        m_setUpdate.text(m_updateAssetUrl.empty() ? L"查看 Release" : L"前往下载").enabled(true);
        m_setUpdate.tooltip(m_updateAssetUrl.empty() ? L"打开 GitHub Release 页面"
                                                     : L"在浏览器中下载新版本");
        break;
    case UpdateState::UpToDate:
        m_setUpdate.text(L"已是最新").enabled(true);
        m_setUpdate.tooltip(L"重新检查 GitHub Releases");
        break;
    case UpdateState::Failed:
        m_setUpdate.text(L"重试").enabled(true);
        m_setUpdate.tooltip(L"重新检查 GitHub Releases");
        break;
    default:
        m_setUpdate.text(L"检查更新").enabled(true);
        m_setUpdate.tooltip(L"基于 GitHub Releases 检查更新");
        break;
    }
}

void AppState::CheckForUpdates()
{
    if (m_closing || m_updateState == UpdateState::Checking) return;
    if (m_updateThread.joinable()) m_updateThread.join();

    m_updateState = UpdateState::Checking;
    m_updateMessage = L"正在检查 GitHub Releases…";
    UpdateUpdateButton();
    m_app.redraw();

    m_updateThread = std::thread([this] {
        UpdateResult result = FetchLatestRelease();
        if (m_closing) return;
        m_app.post([this, result] {
            if (!m_closing) ApplyUpdateResult(result);
        });
    });
}

AppState::UpdateResult AppState::FetchLatestRelease()
{
    UpdateResult r;
    std::wstring repo = std::wstring(kRepoOwner) + L"/" + kRepoName;
    if (const wchar_t* env = _wgetenv(L"YUKI9008_REPO")) {
        std::wstring v = env;
        if (!v.empty()) repo = v;
    }
    std::wstring path = L"/repos/" + repo + L"/releases/latest";

    std::string body;
    std::wstring error;
    DWORD status = 0;
    if (!HttpGetGithub(path, body, error, status)) {
        if (status == 404)
            r.error = L"仓库或 Release 不存在（更新仓库必须为 public）";
        else
            r.error = error;
        return r;
    }

    std::string tagUtf8 = JsonStringValue(body, "tag_name");
    std::string pageUtf8 = JsonStringValue(body, "html_url");
    std::string notesUtf8 = JsonStringValue(body, "body");
    r.tag = FromUtf8(tagUtf8);
    r.pageUrl = FromUtf8(pageUtf8);
    r.notes = FromUtf8(notesUtf8);
    if (r.tag.empty()) {
        r.error = L"Release JSON 缺少 tag_name";
        return r;
    }

    // assets[]: prefer a Windows executable, then an archive.
    std::wstring fallbackExe, fallbackZip;
    size_t apos = body.find("\"assets\"");
    if (apos != std::string::npos) {
        size_t arr = body.find('[', apos);
        size_t i = arr == std::string::npos ? body.size() : arr + 1;
        while (i < body.size()) {
            while (i < body.size() &&
                   (body[i] == ' ' || body[i] == '\t' || body[i] == '\r' ||
                    body[i] == '\n' || body[i] == ',')) ++i;
            if (i >= body.size() || body[i] == ']') break;
            if (body[i] != '{') { ++i; continue; }
            size_t end = JsonObjectEnd(body, i);
            if (end == std::string::npos) break;
            std::string obj = body.substr(i, end - i + 1);
            std::string name = JsonStringValue(obj, "name");
            std::string url = JsonStringValue(obj, "browser_download_url");
            std::wstring wname = Lower(FromUtf8(name));
            std::wstring wurl = FromUtf8(url);
            if (!wurl.empty()) {
                bool isExe = wname.size() >= 4 && wname.substr(wname.size() - 4) == L".exe";
                bool isZip = wname.size() >= 4 && wname.substr(wname.size() - 4) == L".zip";
                if (isExe && wname.find(L"yuki9008") != std::wstring::npos)
                    r.assetUrl = wurl;
                else if (isExe && fallbackExe.empty())
                    fallbackExe = wurl;
                else if (isZip && fallbackZip.empty())
                    fallbackZip = wurl;
            }
            i = end + 1;
        }
    }
    if (r.assetUrl.empty()) r.assetUrl = !fallbackExe.empty() ? fallbackExe : fallbackZip;
    if (r.assetUrl.empty()) r.assetUrl = r.pageUrl;
    if (r.pageUrl.empty() && r.assetUrl.empty()) {
        r.error = L"Release 中没有可下载资源";
        return r;
    }
    if (r.pageUrl.empty()) r.pageUrl = r.assetUrl;

    r.ok = true;
    r.newer = IsNewerVersion(r.tag, kAppVersion);
    return r;
}

void AppState::ApplyUpdateResult(const UpdateResult& result)
{
    if (!result.ok) {
        m_updateState = UpdateState::Failed;
        std::wstring msg = result.error.empty() ? L"未知错误" : result.error;
        if (msg.size() > 64) msg = msg.substr(0, 64) + L"…";
        m_updateMessage = L"检查失败：" + msg;
        m_updateTag.clear();
        m_updateUrl.clear();
        m_updateAssetUrl.clear();
        Log(L"GitHub 更新检查失败 · " + msg, LogLevel::Warn);
    } else if (result.newer) {
        m_updateState = UpdateState::Available;
        m_updateTag = result.tag;
        m_updateUrl = result.pageUrl;
        m_updateAssetUrl = result.assetUrl;
        m_updateNotes = result.notes;
        m_updateMessage = L"发现新版本 " + result.tag + L" · 当前 v" + kAppVersion;
        Log(L"发现新版本 " + result.tag + L"（当前 v" + kAppVersion + L"）", LogLevel::Ok);
        m_app.toast((L"发现新版本 " + result.tag).c_str(), 3600);
    } else {
        m_updateState = UpdateState::UpToDate;
        m_updateTag = result.tag;
        m_updateUrl = result.pageUrl;
        m_updateAssetUrl = result.assetUrl;
        m_updateNotes = result.notes;
        m_updateMessage = L"已是最新版本 · v" + std::wstring(kAppVersion);
        Log(L"GitHub 更新检查完成 · 已是最新版本", LogLevel::Ok);
    }
    UpdateUpdateButton();
    m_app.redraw();
}

void AppState::OpenUpdatePage()
{
    if (m_updateState != UpdateState::Available) {
        CheckForUpdates();
        return;
    }
    std::wstring url = m_updateAssetUrl.empty() ? m_updateUrl : m_updateAssetUrl;
    if (url.empty()) {
        CheckForUpdates();
        return;
    }
    ShellExecuteW(m_app.hwnd(), L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    m_updateMessage = L"已在浏览器打开 GitHub 下载";
    Log(L"打开更新下载地址 · " + url, LogLevel::Info);
    m_app.redraw();
}

void AppState::SetStatus(const std::wstring& main, const std::wstring& sub, DWORD color)
{
    m_statusMain = main;
    m_statusSub = sub;
    m_statusColor = color;
}

LR AppState::NavRect(int i) const
{
    return m_lay.nav[(size_t)std::max(0, std::min(4, i))];
}

LR AppState::AccentRect(int i) const
{
    const int sw = 48;
    return { m_lay.L + 38 + i * 92, 90 + 102, sw, sw };
}

bool AppState::OnCustomMouseDown(int px, int py)
{
    const int dpi = std::max(1, m_app.dpi());
    int lx = px * 96 / dpi;
    int ly = (py - m_lay.top) * 96 / dpi;

    for (int i = 0; i < 5; ++i) {
        if (NavRect(i).has(lx, ly)) {
            SetPage(i);
            return true;
        }
    }
    if (m_lay.pill.has(lx, ly)) {
        StartScan();
        return true;
    }
    if (m_page == 4) {
        for (int i = 0; i < 4; ++i) {
            LR r = AccentRect(i);
            int dx = lx - r.cx(), dy = ly - r.cy();
            if (dx * dx + dy * dy <= (r.w / 2) * (r.w / 2)) {
                static const DWORD accents[4] = { 0x4D9DFF, 0x9179FF, 0x34D6A0, 0xFFB454 };
                static const DWORD accents2[4] = { 0x77B9FF, 0xC2A6FF, 0x79F1C3, 0xFFD79B };
                m_accent = accents[i];
                m_accent2 = accents2[i];
                ApplyAccent(true);
                return true;
            }
        }
    }
    if (m_page == 1) {
        LR zone = { m_lay.L, 90, m_lay.R - m_lay.L, 240 };
        if (zone.has(lx, ly)) {
            BrowseFirmware();
            return true;
        }
    }
    return false;
}

void AppState::OnCustomMouseMove(int px, int py)
{
    const int dpi = std::max(1, m_app.dpi());
    int lx = px * 96 / dpi;
    int ly = (py - m_lay.top) * 96 / dpi;
    int nav = -1;
    for (int i = 0; i < 5; ++i)
        if (NavRect(i).has(lx, ly)) { nav = i; break; }
    bool pill = m_lay.pill.has(lx, ly);
    bool hero = m_lay.hero.has(lx, ly);
    bool drop = m_page == 1 && LR{ m_lay.L, 90, m_lay.R - m_lay.L, 240 }.has(lx, ly);
    int acc = -1;
    if (m_page == 4) {
        for (int i = 0; i < 4; ++i) {
            LR r = AccentRect(i);
            int dx = lx - r.cx(), dy = ly - r.cy();
            if (dx * dx + dy * dy <= (r.w / 2) * (r.w / 2)) { acc = i; break; }
        }
    }
    if (nav != m_hoverNav || pill != m_hoverPill || hero != m_hoverHero ||
        drop != m_hoverDrop || acc != m_hoverAccent) {
        m_hoverNav = nav;
        m_hoverPill = pill;
        m_hoverHero = hero;
        m_hoverDrop = drop;
        m_hoverAccent = acc;
        if (nav >= 0 || pill || drop || acc >= 0)
            SetCursor(LoadCursorA(nullptr, IDC_HAND));
        m_app.redraw();
    }
}

void AppState::OnDrop(HDROP drop)
{
    wchar_t path[MAX_PATH * 4] = {};
    if (DragQueryFileW(drop, 0, path, (UINT)(sizeof(path) / sizeof(wchar_t)))) {
        std::wstring p = path;
        std::error_code ec;
        if (fs::is_directory(p, ec)) {
            LoadFirmwareDir(p);
        } else if (EndsWith(p, L".xml")) {
            LoadFirmwareDir(fs::path(p).parent_path().wstring());
        } else if (EndsWith(p, L".zip")) {
            m_app.msg(L"请先解压固件 ZIP，然后拖入解压后的目录。\n\n"
                      L"Yuki9008工具 直接读取 rawprogram*.xml 与 Firehose 编程器文件。",
                      L"需要解压固件包");
        } else {
            m_app.toast(L"请拖入包含 rawprogram*.xml 的固件目录");
        }
    }
    DragFinish(drop);
}

void AppState::Tick()
{
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_lastTick).count();
    m_lastTick = now;
    dt = std::max(0.f, std::min(0.06f, dt));
    m_dt = dt;
    if (m_animEnabled) m_time += dt;

    if (m_scanning) {
        m_scanT += dt;
        if (!m_scanApplied && m_scanT >= 1.05f) {
            m_scanApplied = true;
            DetectDevice();
            if (m_dev.present) {
                SetStatus(L"9008 设备已连接", m_dev.port + L" · QDLoader", col::green);
                Log(L"检测到 " + m_dev.name + L" · " + m_dev.port, LogLevel::Ok);
                Log(L"EDL 能力集已读取 · Firehose 通道就绪", LogLevel::Info);
            } else {
                SetStatus(L"设备未连接", L"连接 9008 后自动识别 · 需要 QDLoader 串口", col::amber);
                Log(L"未发现 VID_05C6:PID_9008 设备", LogLevel::Warn);
            }
        }
        if (m_scanT >= 1.45f) {
            m_scanning = false;
            UpdateStartButton();
            if (m_autoStart) {
                m_autoStart = false;
                StartFlash();
            }
        }
    }

    if (m_running) PollFlash();

    float k = 1.f - std::exp(-dt * 7.0f);
    m_display += (m_target - m_display) * k;
    if (std::fabs(m_target - m_display) < 0.0005f) m_display = m_target;

    for (int i = 0; i < 5; ++i) {
        float target = (i == m_page) ? 1.f : (i == m_hoverNav ? 0.55f : 0.f);
        m_navT[(size_t)i] += (target - m_navT[(size_t)i]) * std::min(1.f, dt * 12.f);
    }

    if (m_pageFade > 0.f) {
        m_pageFade -= dt * 5.2f;
        if (m_pageFade < 0.f) m_pageFade = 0.f;
    }

    if (m_confettiT < 5.f) {
        m_confettiT += dt;
        for (Confetti& c : m_confetti) {
            c.vy += 240.f * dt;
            c.x += c.vx * dt;
            c.y += c.vy * dt;
            c.rot += c.vr * dt;
        }
    }

    ++m_frame;
    if (GetForegroundWindow() == m_app.hwnd() || m_running || (m_frame % 3u) == 0u)
        m_app.redraw();
}
