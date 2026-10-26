// ============================================================================
//  Yuki9008工具 — Qualcomm Emergency Download Studio
//  A visual EDL / 9008 flashing tool built on YRGui. Author: river.
// ============================================================================
#include "app_state.h"

#include <cstdlib>

AppState* AppState::s_instance = nullptr;

static LRESULT CALLBACK YukiSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                       UINT_PTR, DWORD_PTR ref)
{
    AppState* st = reinterpret_cast<AppState*>(ref);
    switch (msg) {
    case WM_DROPFILES:
        if (st) st->OnDrop((HDROP)wp);
        return 0;
    case WM_MOUSEMOVE: {
        // Let App update standard-control hover/cursor first, then let the
        // custom chrome override the cursor only when it owns the hit area.
        LRESULT r = DefSubclassProc(hwnd, msg, wp, lp);
        if (st) st->OnCustomMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return r;
    }
    case WM_MOUSELEAVE:
        if (st) st->OnCustomMouseMove(-10000, -10000);
        break;
    case WM_LBUTTONDOWN:
        if (st && st->OnCustomMouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)))
            return 0;
        break;
    case WM_KEYDOWN:
        if (st) {
            if (wp == VK_F5) {
                st->TriggerScan();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == VK_RETURN) {
                st->TriggerStart();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (wp == 'L' || wp == 'l')) {
                st->TriggerLog();
                return 0;
            }
        }
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static Theme YukiTheme()
{
    Theme t = darkTheme();
    t.bgTop = col::bg0;
    t.bgBottom = 0x04070D;
    t.fg = col::ink;
    t.fgDim = col::dim;
    t.accent = 0x4D9DFF;
    t.accentHover = 0x72B7FF;
    t.accentPressed = 0x2F7FE6;
    t.btnBg = 0x101B2C;
    t.btnHover = 0x17263C;
    t.btnPressed = 0x0C1523;
    t.btnBorder = 0x23324A;
    t.inputBg = 0x0A121F;
    t.inputBorder = 0x22314A;
    t.inputFocus = 0x4D9DFF;
    t.trackBg = 0x17243A;
    t.trackFill = 0x4D9DFF;
    t.cardBg = 0x0B1321;
    t.cardBorder = 0x1A2940;
    t.divider = 0x17253A;
    t.windowBorder = 0x1D2A41;
    t.titleBarBg = 0x080D16;
    t.titleBarFg = col::ink;
    t.titleBarDim = 0x7D8CA4;
    t.titleBarHover = 0x121D2E;
    t.danger = col::red;
    t.success = col::green;
    t.warning = col::amber;
    t.radius = 10;
    t.radiusLg = 16;
    t.fontSize = 12;
    t.titleSize = 24;
    t.dark = true;
    return t;
}

int main()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    WindowOptions opt;
    opt.width = 1240;
    opt.height = 820;
    opt.title = L"Yuki9008工具  ·  高通 EDL 刷写工作站";
    opt.theme = YukiTheme();
    opt.effect = Effect::None;
    opt.resizable = true;
    opt.center = true;
    opt.minWidth = 1220;
    opt.minHeight = 790;

    App app(opt);
    UI ui(app);
    AppState state(app, ui);
    state.Build();

    // Brand icon: prefer logo.ico next to the exe, fall back to the embedded
    // resource, then to YRGui's generated icon.
    HICON appBig = nullptr, appSmall = nullptr;
    {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring dir = exePath;
        size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) dir.resize(slash);
        std::wstring ico = dir + L"\\logo.ico";
        appBig = (HICON)LoadImageW(nullptr, ico.c_str(), IMAGE_ICON, 0, 0,
                                   LR_LOADFROMFILE | LR_DEFAULTSIZE);
        appSmall = (HICON)LoadImageW(nullptr, ico.c_str(), IMAGE_ICON,
                                     GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
        if (!appBig)
            appBig = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                       IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
        if (!appSmall)
            appSmall = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                         IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                         GetSystemMetrics(SM_CYSMICON), 0);
        if (appBig)
            SendMessageW(app.hwnd(), WM_SETICON, ICON_BIG, (LPARAM)appBig);
        if (appSmall)
            SendMessageW(app.hwnd(), WM_SETICON, ICON_SMALL, (LPARAM)appSmall);
    }

    SetWindowSubclass(app.hwnd(), YukiSubclass, 0x9008, (DWORD_PTR)&state);
    DragAcceptFiles(app.hwnd(), TRUE);

    state.Begin();

    if (const char* env = std::getenv("YUKI9008_AUTO_CLOSE_MS")) {
        int ms = atoi(env);
        if (ms > 0) app.timer(ms, [&app] { app.close(); });
    }

    int rc = app.run();
    if (appBig) DestroyIcon(appBig);
    if (appSmall) DestroyIcon(appSmall);
    DragAcceptFiles(app.hwnd() ? app.hwnd() : nullptr, FALSE);
    CoUninitialize();
    return rc;
}
