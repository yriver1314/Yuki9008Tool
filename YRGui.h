//============================================================================
// YRGui.h  —  Windows GUI 框架（手动布局 + 可选自动布局，单头文件）
//============================================================================
//  版本     : 4.0.0
//  语言     : C++11 及以上（仅 Windows）
//  依赖     : user32 / gdi32 / gdiplus / dwmapi（全部为系统自带）
//  许可     : MIT
//
//  v4 的设计要点
//  -------------
//  1. 手动布局：App + UI；每个控件显式指定 x / y / w / h、颜色和事件，
//     不自动铺满，也不需要理解 Element 树。
//  2. 可选自动布局：app.title / app.input / app.button 顺序写，
//     适合简单表单；底层仍是响应式数据。
//  3. 现代无边框窗口：自绘标题栏（拖动 / 最小化 / 最大化 / 关闭 / Win11 贴靠），
//     原生边框缩放、阴影与圆角，Per-Monitor V2 DPI。
//  4. 可选玻璃质感：Effect::Blur / Acrylic / Mica，基于 DWM 系统背景材质，
//     并可通过 WindowOptions.glassOpacity 调整磨砂强度。
//  5. 统一视觉：一套语义化主题（surface / text / accent / border / danger），
//     统一的圆角、间距、字号、悬浮与聚焦反馈。
//
//  快速上手：
//
//      #include "YRGui.h"
//      using namespace yr;
//
//      int main()
//      {
//          App app(L"我的应用", 480, 360);   // 默认深色 + Acrylic
//          UI  ui(app);
//
//          ui.title(L"你好，YRGui", 24, 20, 360, 40);
//          Widget name = ui.input(24, 80, 300, 36).placeholder(L"输入昵称");
//          Widget hello = ui.button(L"问候", 24, 130, 140, 36).primary();
//          hello.onClick([&] {
//              ui.msg((L"欢迎，" + name.text() + L"！").c_str());
//          });
//
//          return ui.run();
//      }
//
//  编译（MinGW）：
//      g++ main.cpp -o app.exe -mwindows -lgdiplus -lgdi32 -luser32 -ldwmapi
//  编译（MSVC）：
//      cl main.cpp /Fe:app.exe /utf-8 /O2 /EHsc
//============================================================================
#ifndef YR_GUI_H
#define YR_GUI_H

#define YR_GUI_VERSION "4.0.0"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include <propidl.h>    // MinGW-w64 的 gdiplus.h 需要 PROPID
#include <gdiplus.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <map>
#include <initializer_list>
#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cwchar>
#include <utility>

// DWM 常量（旧版 SDK / MinGW 头文件可能没有）
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE_ROUND
#define DWMWA_WINDOW_CORNER_PREFERENCE_ROUND 2
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_AUTO 0
#define DWMSBT_NONE 1
#define DWMSBT_MAINWINDOW 2      // Mica
#define DWMSBT_TRANSIENTWINDOW 3 // Acrylic
#define DWMSBT_TABBEDWINDOW 4
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

// MSVC / clang-cl：自动链接系统库，并让 main() 成为 GUI 入口
#if defined(_MSC_VER)
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:mainCRTStartup")
#endif

namespace yr {

using Gdiplus::REAL;

//============================================================================
// 基础工具
//============================================================================

inline Gdiplus::Color rgb(DWORD rrggbb)
{
    return Gdiplus::Color(255,
        (BYTE)((rrggbb >> 16) & 0xFF),
        (BYTE)((rrggbb >> 8) & 0xFF),
        (BYTE)(rrggbb & 0xFF));
}

inline Gdiplus::Color argb(BYTE a, DWORD rrggbb)
{
    return Gdiplus::Color(a,
        (BYTE)((rrggbb >> 16) & 0xFF),
        (BYTE)((rrggbb >> 8) & 0xFF),
        (BYTE)(rrggbb & 0xFF));
}

inline DWORD lerpColor(DWORD a, DWORD b, float t)
{
    if (t <= 0.f) return a;
    if (t >= 1.f) return b;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    return ((DWORD)(ar + (br - ar) * t) << 16)
         | ((DWORD)(ag + (bg - ag) * t) << 8)
         |  (DWORD)(ab + (bb - ab) * t);
}

inline void buildRoundPath(Gdiplus::GraphicsPath& p,
                           float x, float y, float w, float h, float r)
{
    if (r > w / 2.f) r = w / 2.f;
    if (r > h / 2.f) r = h / 2.f;
    if (r < 0.5f) {
        p.AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    float d = r * 2.f;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

// 当前 Windows 内部版本号（Win11 >= 22000；失败返回 0）
inline int yrWindowsBuild()
{
    static int build = [] {
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll) {
            typedef LONG(WINAPI *Fn)(OSVERSIONINFOW*);
            Fn fn = (Fn)(void*)GetProcAddress(ntdll, "RtlGetVersion");
            if (fn) {
                OSVERSIONINFOW vi = {};
                vi.dwOSVersionInfoSize = sizeof(vi);
                if (fn(&vi) == 0) return (int)vi.dwBuildNumber;
            }
        }
        return 0;
    }();
    return build;
}

//============================================================================
// 主题：语义化颜色 + 统一圆角 / 字号
//============================================================================
struct Theme {
    DWORD bgTop    = 0x17181C;   // 窗口背景（上）
    DWORD bgBottom = 0x121317;   // 窗口背景（下）
    DWORD fg       = 0xECEDF0;   // 主文本
    DWORD fgDim    = 0x9AA0A9;   // 次级文本
    DWORD accent       = 0x5B8DFF;
    DWORD accentHover  = 0x7CA5FF;
    DWORD accentPressed= 0x3F74E8;
    DWORD btnBg        = 0x24262D;
    DWORD btnHover     = 0x2D3038;
    DWORD btnPressed   = 0x202229;
    DWORD btnBorder    = 0x343740;
    DWORD inputBg      = 0x1B1D23;
    DWORD inputBorder  = 0x30343D;
    DWORD inputFocus   = 0x5B8DFF;
    DWORD trackBg      = 0x2A2D35;
    DWORD trackFill    = 0x5B8DFF;
    DWORD cardBg       = 0x1B1D22;
    DWORD cardBorder   = 0x2B2E36;
    DWORD divider      = 0x272A31;

    // 标题栏 / 语义色
    DWORD windowBorder = 0x3A3D46;   // 无边框窗口最外圈发丝线
    DWORD titleBarBg   = 0x17181C;
    DWORD titleBarFg   = 0xECEDF0;
    DWORD titleBarDim  = 0x8C929C;
    DWORD titleBarHover= 0x262930;
    DWORD danger       = 0xF05257;
    DWORD success      = 0x3DBE7C;
    DWORD warning      = 0xE5A13A;

    int   radius   = 8;          // 控件圆角（逻辑像素）
    int   radiusLg = 12;         // 卡片 / 面板圆角
    int   fontSize = 14;         // 正文字号（逻辑像素）
    int   titleSize= 24;         // 标题字号
    int   gap      = 12;         // 默认间距
    bool  dark     = true;
};

inline Theme darkTheme()
{
    return Theme();
}

inline Theme lightTheme()
{
    Theme t;
    t.bgTop = 0xF7F8FA; t.bgBottom = 0xEFF1F4;
    t.fg = 0x181A1F; t.fgDim = 0x69707B;
    t.accent = 0x2F6FED; t.accentHover = 0x4C86F5; t.accentPressed = 0x1F58CC;
    t.btnBg = 0xFFFFFF; t.btnHover = 0xF1F4F8; t.btnPressed = 0xE7EBF1; t.btnBorder = 0xDFE3E9;
    t.inputBg = 0xFFFFFF; t.inputBorder = 0xDDE1E7; t.inputFocus = 0x2F6FED;
    t.trackBg = 0xE3E7ED; t.trackFill = 0x2F6FED;
    t.cardBg = 0xFFFFFF; t.cardBorder = 0xE3E7ED;
    t.divider = 0xE6E9EE;
    t.windowBorder = 0xC9CED8;
    t.titleBarBg = 0xFFFFFF; t.titleBarFg = 0x181A1F;
    t.titleBarDim = 0x69707B; t.titleBarHover = 0xEEF1F5;
    t.danger = 0xE5484D; t.success = 0x2E9E63; t.warning = 0xC98A1E;
    t.dark = false;
    return t;
}

//============================================================================
// 窗口选项 / 背景材质
//============================================================================

// None   : 不透明窗口
// Blur   : 系统模糊（Win10 / 旧驱动回退）
// Acrylic: 亚克力磨砂玻璃（推荐）
// Mica   : Windows 11 云母背景（跟随桌面壁纸）
enum class Effect { None, Blur, Acrylic, Mica };

struct WindowOptions {
    int         width  = 900;
    int         height = 640;
    std::wstring title = L"YRGui";
    Theme       theme  = darkTheme();
    Effect      effect = Effect::None;
    int         glassOpacity = 72;   // 玻璃底色不透明度 0~100
    DWORD       glassTint = 0;       // 0 = 使用主题 window 底色
    bool        resizable = true;
    bool        center    = true;
    int         minWidth  = 420;
    int         minHeight = 300;
};

//============================================================================
// 菜单 / 树（控件数据）
//============================================================================
constexpr int kListItemH   = 28;
constexpr int kTableHeadH  = 32;
constexpr int kComboMaxVis = 8;
constexpr int kPopupItemH = 30;

struct MenuItem {
    std::wstring text;
    std::function<void()> onClick;
    std::wstring shortcut;
    bool enabled   = true;
    bool separator = false;

    MenuItem() {}
    MenuItem(const wchar_t* t, std::function<void()> fn = nullptr,
             const wchar_t* sc = L"", bool en = true, bool sep = false)
        : text(t ? t : L""), onClick(std::move(fn)),
          shortcut(sc ? sc : L""), enabled(en), separator(sep) {}
};

inline MenuItem menuSep()
{
    return MenuItem(L"", nullptr, L"", true, true);
}

struct MenuEntry {
    std::wstring title;
    std::vector<MenuItem> items;

    MenuEntry() {}
    MenuEntry(const wchar_t* t, std::vector<MenuItem> it)
        : title(t ? t : L""), items(std::move(it)) {}
};

struct TreeNode {
    std::wstring text;
    bool expanded = true;
    std::vector<TreeNode> children;

    TreeNode() {}
    TreeNode(const wchar_t* t, bool exp = true, std::vector<TreeNode> ch = {})
        : text(t ? t : L""), expanded(exp), children(std::move(ch)) {}
};

enum class Ctl {
    Label, Button, CheckBox, Edit, ProgressBar, Slider, Divider, Card,
    ComboBox, ListBox,
    TextArea, RadioButton, Toggle, GroupBox, Tabs, SpinBox, Image, Link,
    MenuBar, Table, Tree, StatusBar, ProgressRing, Badge, Tag, Rating, Calendar
};

//============================================================================
// 响应式状态 + 双向绑定
//============================================================================
template <class T>
struct StateSlot {
    T value;
    std::function<void()> notify;
};

template <class T>
class State {
public:
    State() {}
    explicit State(std::shared_ptr<StateSlot<T>> slot) : m_slot(std::move(slot)) {}

    bool valid() const { return (bool)m_slot; }
    T get() const { return m_slot ? m_slot->value : T(); }
    void set(T v)
    {
        if (!m_slot || m_slot->value == v) return;
        m_slot->value = std::move(v);
        if (m_slot->notify) m_slot->notify();
    }
    void update(const std::function<void(T&)>& fn)
    {
        if (!m_slot) return;
        fn(m_slot->value);
        if (m_slot->notify) m_slot->notify();
    }

private:
    std::shared_ptr<StateSlot<T>> m_slot;
};

// 显式类型别名：不想写 auto 时使用
using StringState = State<std::wstring>;
using IntState    = State<int>;
using BoolState   = State<bool>;
using FloatState  = State<float>;

// 控件与 State 之间的绑定：kind 决定同步哪个字段
struct Binding {
    int kind = 0;   // 1 文本 / 2 布尔 / 3 浮点 / 4 整数
    std::function<std::wstring()>              getText;
    std::function<void(const std::wstring&)>   setText;
    std::function<bool()>                      getBool;
    std::function<void(bool)>                  setBool;
    std::function<float()>                     getFloat;
    std::function<void(float)>                 setFloat;
    std::function<int()>                       getInt;
    std::function<void(int)>                   setInt;
};

inline std::shared_ptr<Binding> bind(State<std::wstring> s)
{
    std::shared_ptr<Binding> b = std::make_shared<Binding>();
    b->kind = 1;
    b->getText = [s] { return s.get(); };
    b->setText = [s](const std::wstring& v) mutable { s.set(v); };
    return b;
}

inline std::shared_ptr<Binding> bind(State<bool> s)
{
    std::shared_ptr<Binding> b = std::make_shared<Binding>();
    b->kind = 2;
    b->getBool = [s] { return s.get(); };
    b->setBool = [s](bool v) mutable { s.set(v); };
    return b;
}

inline std::shared_ptr<Binding> bind(State<float> s)
{
    std::shared_ptr<Binding> b = std::make_shared<Binding>();
    b->kind = 3;
    b->getFloat = [s] { return s.get(); };
    b->setFloat = [s](float v) mutable { s.set(v); };
    return b;
}

inline std::shared_ptr<Binding> bind(State<int> s)
{
    std::shared_ptr<Binding> b = std::make_shared<Binding>();
    b->kind = 4;
    b->getInt = [s] { return s.get(); };
    b->setInt = [s](int v) mutable { s.set(v); };
    return b;
}

//============================================================================
// 声明式元素树
//============================================================================

// 尺寸语义：kAuto = 由内容决定；kFill = 填满父容器
constexpr int kAuto = -2;
constexpr int kFill = -1;

enum class Align { Start, Center, End, Stretch };

enum class Kind {
    Column, Row, Stack, Scroll, Spacer,
    Text, Title, Caption, Link, Divider,
    Button, Input, Password, Search, TextArea, SpinBox,
    Checkbox, Radio, Switch, Slider, Progress, Spinner,
    Combo, ListBox, Tabs, Table, Tree, Calendar, Rating, Tag, Badge,
    MenuBar, StatusBar, Image, Card
};

struct Element {
    struct Props {
        std::wstring text, placeholder, tooltip, key, path;
        int width  = kAuto;
        int height = kAuto;
        int pad = 0;
        int gap = 0;
        int fontSize = 0;
        bool strong = false, dim = false, center = false, wrap = false;
        bool primary = false, enabled = true, visible = true, checked = false;
        bool expand = false;
        bool fillWidth = false;
        float value = 0.f, min = 0.f, max = 1.f, step = 1.f;
        int decimals = 0;
        int selected = 0;
        int group = 0;
        int radius = -1, bg = -1, fg = -1, border = -1, accent = -1;
        Align align = Align::Stretch;
        std::vector<std::wstring> items;
        std::vector<std::wstring> headers;
        std::vector<std::vector<std::wstring>> rows;
        std::vector<TreeNode> nodes;
        std::vector<MenuEntry> menus;
        std::function<void()> onClick, onChanged, onEnter, onRightClick;
        std::shared_ptr<Binding> binding;
    };

    Kind tag = Kind::Column;
    Props p;
    std::vector<Element> kids;

    Element() {}
    explicit Element(Kind t) : tag(t) {}

    // ---- 通用链式属性 ----
    Element& key(const wchar_t* k)     { p.key = k ? k : L""; return *this; }
    Element& tooltip(const wchar_t* t) { p.tooltip = t ? t : L""; return *this; }
    Element& width(int v)  { p.width = v; return *this; }
    Element& height(int v) { p.height = v; return *this; }
    Element& size(int w, int h) { p.width = w; p.height = h; return *this; }
    Element& padding(int v) { p.pad = std::max(0, v); return *this; }
    Element& gap(int v)     { p.gap = std::max(0, v); return *this; }
    Element& font(int v)    { p.fontSize = v; return *this; }
    Element& strong(bool v = true) { p.strong = v; return *this; }
    Element& dim(bool v = true)    { p.dim = v; return *this; }
    Element& center(bool v = true) { p.center = v; return *this; }
    Element& wrap(bool v = true)   { p.wrap = v; return *this; }
    Element& primary(bool v = true){ p.primary = v; return *this; }
    Element& enabled(bool v)       { p.enabled = v; return *this; }
    Element& visible(bool v)       { p.visible = v; return *this; }
    Element& checked(bool v)       { p.checked = v; return *this; }
    Element& value(float v)        { p.value = v; return *this; }
    Element& range(float a, float b) { p.min = a; p.max = b; return *this; }
    Element& step(float v)         { p.step = v; return *this; }
    Element& decimals(int v)       { p.decimals = v; return *this; }
    Element& selected(int v)       { p.selected = v; return *this; }
    Element& group(int v)          { p.group = v; return *this; }
    Element& align(Align a)        { p.align = a; return *this; }
    Element& radius(int v)         { p.radius = v; return *this; }
    Element& bg(int c)             { p.bg = c; return *this; }
    Element& fg(int c)             { p.fg = c; return *this; }
    Element& border(int c)         { p.border = c; return *this; }
    Element& accent(int c)         { p.accent = c; return *this; }
    Element& placeholder(const wchar_t* t) { p.placeholder = t ? t : L""; return *this; }

    // 填满父容器的主轴剩余空间（配合 Row / Column 使用）
    Element& fill() { p.expand = true; p.align = Align::Stretch; return *this; }

    // 只占满父容器宽度，不改变高度（适合 Column 里的整行按钮）
    Element& fillWidth() { p.fillWidth = true; return *this; }

    // 双向绑定（立即生效，无需手动 rebuild）
    Element& bindText(State<std::wstring> st) { p.binding = bind(st); return *this; }
    Element& bindChecked(State<bool> st)      { p.binding = bind(st); return *this; }
    Element& bindValue(State<float> st)       { p.binding = bind(st); return *this; }
    Element& bindSelected(State<int> st)      { p.binding = bind(st); return *this; }

    Element& onClick(std::function<void()> fn)    { p.onClick = std::move(fn); return *this; }
    Element& onChanged(std::function<void()> fn)  { p.onChanged = std::move(fn); return *this; }
    Element& onEnter(std::function<void()> fn)    { p.onEnter = std::move(fn); return *this; }
    Element& onRightClick(std::function<void()> fn) { p.onRightClick = std::move(fn); return *this; }

    Element& items(std::initializer_list<const wchar_t*> its)
    {
        p.items.clear();
        for (const wchar_t* it : its) p.items.push_back(it ? it : L"");
        return *this;
    }
    Element& headers(std::initializer_list<const wchar_t*> hs)
    {
        p.headers.clear();
        for (const wchar_t* h : hs) p.headers.push_back(h ? h : L"");
        return *this;
    }
    Element& rows(std::vector<std::vector<std::wstring>> rs)
    {
        p.rows = std::move(rs);
        return *this;
    }
    Element& nodes(std::vector<TreeNode> ns)
    {
        p.nodes = std::move(ns);
        return *this;
    }
    Element& menus(std::vector<MenuEntry> ms)
    {
        p.menus = std::move(ms);
        return *this;
    }
};

// ---- 容器 ----
inline Element Column(std::initializer_list<Element> kids = {}, int gap = 0)
{
    Element e(Kind::Column);
    e.kids.assign(kids.begin(), kids.end());
    e.p.gap = gap;
    return e;
}
inline Element Column(std::vector<Element> kids, int gap = 0)
{
    Element e(Kind::Column);
    e.kids = std::move(kids);
    e.p.gap = gap;
    return e;
}
inline Element Row(std::initializer_list<Element> kids = {}, int gap = 0)
{
    Element e(Kind::Row);
    e.kids.assign(kids.begin(), kids.end());
    e.p.gap = gap;
    return e;
}
inline Element Row(std::vector<Element> kids, int gap = 0)
{
    Element e(Kind::Row);
    e.kids = std::move(kids);
    e.p.gap = gap;
    return e;
}
inline Element Stack(std::initializer_list<Element> kids = {})
{
    Element e(Kind::Stack);
    e.kids.assign(kids.begin(), kids.end());
    return e;
}
inline Element Scroll(std::initializer_list<Element> kids = {}, int gap = 0)
{
    Element e(Kind::Scroll);
    e.kids.assign(kids.begin(), kids.end());
    e.p.gap = gap;
    return e;
}
inline Element Card(std::initializer_list<Element> kids = {}, int gap = 12)
{
    Element e(Kind::Card);
    e.kids.assign(kids.begin(), kids.end());
    e.p.pad = 16;
    e.p.gap = gap;
    return e;
}
inline Element Spacer() { Element e(Kind::Spacer); e.p.expand = true; return e; }
inline Element Space(int h) { Element e(Kind::Spacer); e.p.height = h; return e; }

// ---- 文本 ----
inline Element Text(const wchar_t* s, int fontSize = 0)
{
    Element e(Kind::Text);
    e.p.text = s ? s : L"";
    e.p.fontSize = fontSize;
    e.p.strong = true;
    return e;
}
inline Element Text(State<std::wstring> s, int fontSize = 0)
{
    Element e = Text(s.get().c_str(), fontSize);
    e.p.binding = bind(s);
    return e;
}
inline Element Title(const wchar_t* s)
{
    Element e(Kind::Title);
    e.p.text = s ? s : L"";
    e.p.strong = true;
    return e;
}
inline Element Title(const std::wstring& s) { return Title(s.c_str()); }
inline Element Caption(const wchar_t* s)
{
    Element e(Kind::Caption);
    e.p.text = s ? s : L"";
    e.p.dim = true;
    return e;
}
inline Element Caption(const std::wstring& s) { return Caption(s.c_str()); }
inline Element Text(const std::wstring& s, int fontSize = 0) { return Text(s.c_str(), fontSize); }
inline Element Link(const wchar_t* s, std::function<void()> fn = nullptr)
{
    Element e(Kind::Link);
    e.p.text = s ? s : L"";
    e.p.onClick = std::move(fn);
    return e;
}
inline Element Divider() { return Element(Kind::Divider); }

// ---- 基础控件 ----
inline Element Button(const std::wstring& label, std::function<void()> fn = nullptr) { return Button(label.c_str(), std::move(fn)); }
inline Element Button(const wchar_t* label, std::function<void()> fn = nullptr)
{
    Element e(Kind::Button);
    e.p.text = label ? label : L"";
    e.p.onClick = std::move(fn);
    return e;
}
inline Element Input(const wchar_t* text = L"")
{
    Element e(Kind::Input);
    e.p.text = text ? text : L"";
    return e;
}
inline Element Input(State<std::wstring> s)
{
    Element e(Kind::Input);
    e.p.text = s.get();
    e.p.binding = bind(s);
    return e;
}
inline Element Password(const wchar_t* text = L"", const wchar_t* ph = L"")
{
    Element e(Kind::Password);
    e.p.text = text ? text : L"";
    e.p.placeholder = ph ? ph : L"";
    return e;
}
inline Element Search(const wchar_t* text = L"", const wchar_t* ph = L"")
{
    Element e(Kind::Search);
    e.p.text = text ? text : L"";
    e.p.placeholder = ph ? ph : L"";
    return e;
}
inline Element Search(State<std::wstring> st, const wchar_t* ph = L"")
{
    Element e(Kind::Search);
    e.p.text = st.get();
    e.p.placeholder = ph ? ph : L"";
    e.p.binding = bind(st);
    return e;
}
inline Element TextArea(const wchar_t* text = L"")
{
    Element e(Kind::TextArea);
    e.p.text = text ? text : L"";
    return e;
}
inline Element TextArea(State<std::wstring> s)
{
    Element e(Kind::TextArea);
    e.p.text = s.get();
    e.p.binding = bind(s);
    return e;
}
inline Element SpinBox(float value = 0.f, float min = 0.f, float max = 100.f, float step = 1.f)
{
    Element e(Kind::SpinBox);
    e.p.value = value; e.p.min = min; e.p.max = max; e.p.step = step;
    return e;
}
inline Element SpinBox(State<float> value, float min = 0.f, float max = 100.f, float step = 1.f)
{
    Element e(Kind::SpinBox);
    e.p.value = value.get(); e.p.min = min; e.p.max = max; e.p.step = step;
    e.p.binding = bind(value);
    return e;
}
inline Element Checkbox(const wchar_t* label, bool checked = false)
{
    Element e(Kind::Checkbox);
    e.p.text = label ? label : L"";
    e.p.checked = checked;
    return e;
}
inline Element Checkbox(const wchar_t* label, State<bool> s)
{
    Element e(Kind::Checkbox);
    e.p.text = label ? label : L"";
    e.p.checked = s.get();
    e.p.binding = bind(s);
    return e;
}
inline Element Radio(const wchar_t* label, bool checked = false, int group = 0)
{
    Element e(Kind::Radio);
    e.p.text = label ? label : L"";
    e.p.checked = checked;
    e.p.group = group;
    return e;
}
inline Element Switch(bool checked = false)
{
    Element e(Kind::Switch);
    e.p.checked = checked;
    return e;
}
inline Element Switch(State<bool> s)
{
    Element e(Kind::Switch);
    e.p.checked = s.get();
    e.p.binding = bind(s);
    return e;
}
inline Element Slider(float value = 0.f)
{
    Element e(Kind::Slider);
    e.p.value = value;
    return e;
}
inline Element Slider(State<float> s)
{
    Element e(Kind::Slider);
    e.p.value = s.get();
    e.p.binding = bind(s);
    return e;
}
inline Element Progress(float value = 0.f)
{
    Element e(Kind::Progress);
    e.p.value = value;
    return e;
}
inline Element Progress(State<float> st)
{
    Element e(Kind::Progress);
    e.p.value = st.get();
    e.p.binding = bind(st);
    return e;
}
inline Element Spinner()
{
    Element e(Kind::Spinner);
    return e;
}
inline Element Combo(std::initializer_list<const wchar_t*> items, int selected = 0)
{
    Element e(Kind::Combo);
    for (const wchar_t* it : items) e.p.items.push_back(it ? it : L"");
    e.p.selected = selected;
    return e;
}
inline Element Combo(std::initializer_list<const wchar_t*> items, State<int> s)
{
    Element e(Kind::Combo);
    for (const wchar_t* it : items) e.p.items.push_back(it ? it : L"");
    e.p.selected = s.get();
    e.p.binding = bind(s);
    return e;
}
inline Element ListBox(std::initializer_list<const wchar_t*> items, int selected = 0)
{
    Element e(Kind::ListBox);
    for (const wchar_t* it : items) e.p.items.push_back(it ? it : L"");
    e.p.selected = selected;
    return e;
}
inline Element Tabs(std::initializer_list<const wchar_t*> items, int selected = 0)
{
    Element e(Kind::Tabs);
    for (const wchar_t* it : items) e.p.items.push_back(it ? it : L"");
    e.p.selected = selected;
    return e;
}
inline Element Tabs(std::initializer_list<const wchar_t*> items, State<int> s)
{
    Element e(Kind::Tabs);
    for (const wchar_t* it : items) e.p.items.push_back(it ? it : L"");
    e.p.selected = s.get();
    e.p.binding = bind(s);
    return e;
}
inline Element Tabs(std::initializer_list<const wchar_t*> items, State<int> s,
                    std::vector<Element> pages)
{
    Element e = Tabs(items, s);
    e.kids = std::move(pages);
    return e;
}
inline Element Table(std::initializer_list<const wchar_t*> headers,
                     std::vector<std::vector<std::wstring>> rows = {})
{
    Element e(Kind::Table);
    for (const wchar_t* h : headers) e.p.headers.push_back(h ? h : L"");
    e.p.rows = std::move(rows);
    return e;
}
inline Element Tree(std::vector<TreeNode> nodes)
{
    Element e(Kind::Tree);
    e.p.nodes = std::move(nodes);
    return e;
}
inline Element Calendar(int y = 0, int m = 0, int d = 0)
{
    Element e(Kind::Calendar);
    if (y > 0) { e.p.value = (float)(y * 10000 + m * 100 + d); }
    return e;
}
inline Element Rating(int maxStars = 5, int value = 0)
{
    Element e(Kind::Rating);
    e.p.max = (float)maxStars;
    e.p.value = (float)value;
    return e;
}
inline Element Rating(int maxStars, State<int> value)
{
    Element e(Kind::Rating);
    e.p.max = (float)maxStars;
    e.p.value = (float)value.get();
    e.p.binding = bind(value);
    return e;
}
inline Element RadioGroup(std::initializer_list<const wchar_t*> items, State<int> sel)
{
    static int groupSeq = 1;
    const int gid = groupSeq++;
    std::vector<Element> rows;
    int idx = 0;
    for (const wchar_t* t : items) {
        int i = idx++;
        Element r = Radio(t, sel.get() == i, gid);
        r.p.onChanged = [sel, i]() mutable { sel.set(i); };
        rows.push_back(r);
    }
    return Column(rows, 6);
}

inline Element Tag(const wchar_t* label)
{
    Element e(Kind::Tag);
    e.p.text = label ? label : L"";
    return e;
}
inline Element Tag(const wchar_t* label, std::function<void()> onClose)
{
    Element e = Tag(label);
    e.p.onClick = std::move(onClose);
    return e;
}
inline Element Badge(const wchar_t* label)
{
    Element e(Kind::Badge);
    e.p.text = label ? label : L"";
    return e;
}
inline Element MenuBar(std::vector<MenuEntry> menus)
{
    Element e(Kind::MenuBar);
    e.p.menus = std::move(menus);
    return e;
}
inline Element StatusBar(std::initializer_list<const wchar_t*> parts)
{
    Element e(Kind::StatusBar);
    for (const wchar_t* s : parts) e.p.items.push_back(s ? s : L"");
    return e;
}
inline Element Image(const wchar_t* path, int height = 140)
{
    Element e(Kind::Image);
    e.p.path = path ? path : L"";
    e.p.height = height;
    return e;
}

//============================================================================
// 控件（内部渲染模型）：由声明式元素在布局阶段生成
//============================================================================
struct Control {
    Ctl  type      = Ctl::Label;
    int  x = 0, y = 0, w = 0, h = 0;
    std::wstring text;
    std::wstring key;                    // 声明式重建时用于保持瞬时状态
    std::shared_ptr<Binding> binding;    // 双向绑定（声明式）
    bool visible   = true;
    bool enabled   = true;
    bool checked   = false;
    bool primary   = false;
    bool center    = false;
    bool strong    = false;
    bool bold      = false;
    bool wrap      = false;
    int  fontSize  = 0;       // 逻辑字号；0 = 主题默认
    float progress = 0.f;
    float min = 0.f, max = 1.f;

    std::vector<std::wstring> items;
    int  selected  = -1;
    int  hoverItem = -1;
    int  scrollY   = 0;
    bool open      = false;

    int  groupId   = 0;
    float step     = 1.f;
    int  decimals  = 0;
    float toggleT  = 0.f;
    int  viewY     = 0;
    std::shared_ptr<Gdiplus::Image> image;

    std::vector<MenuEntry> menus;
    int  menuIdx = -1;

    std::vector<TreeNode> nodes;
    std::vector<int> selPath;

    std::vector<std::wstring> headers;
    std::vector<std::vector<std::wstring>> rows;
    std::vector<int> colWidths;
    int  selectedRow = -1;
    int  sortCol = -1;
    bool sortAsc = true;

    int  calY = 0, calM = 0, calD = 0;
    bool hasDate = false;

    int  rating = 0;
    int  ratingMax = 5;
    int  hoverRating = 0;

    bool spin = false;
    float rotate = 0.f;

    bool password  = false;
    bool searchBox = false;
    std::wstring placeholder;
    std::wstring tooltip;
    std::function<void()> onRightClick;
    std::function<void()> onClick;
    std::function<void()> onChanged;
    std::function<void()> onEnter;
    std::function<void()> onHover;
    std::function<void()> onLeave;

    // 样式覆盖（-1 = 跟随主题；0xRRGGBB）
    int  cornerRadius = -1;
    int  bgColor   = -1;
    int  fgColor   = -1;
    int  borderColor = -1;
    int  accentColor = -1;

    // 内部状态
    bool hover     = false;
    bool pressed   = false;
    bool dragging  = false;
    bool focused   = false;
    float hoverT   = 0.f;
    int  caret     = 0;
    int  selA = 0, selB = 0;
    int  scrollX   = 0;

    // 滚动容器的裁剪区（声明式）
    bool hasClip = false;
    RECT clipRect = { 0, 0, 0, 0 };

    // 链式样式（便捷 API）
    Control& radius(int r)          { cornerRadius = r; return *this; }
    Control& bg(int c)              { bgColor = c; return *this; }
    Control& fg(int c)              { fgColor = c; return *this; }
    Control& border(int c)          { borderColor = c; return *this; }
    Control& accent(int c)          { accentColor = c; return *this; }
    Control& colors(int bgC = -1, int fgC = -1, int borderC = -1, int accentC = -1)
    {
        bgColor = bgC; fgColor = fgC; borderColor = borderC; accentColor = accentC;
        return *this;
    }
    Control& resetStyle()
    {
        cornerRadius = bgColor = fgColor = borderColor = accentColor = -1;
        return *this;
    }
    Control& setText(const wchar_t* t)  { text = t ? t : L""; return *this; }
    Control& setFontSize(int s)         { fontSize = s; return *this; }
    Control& setPos(int nx, int ny)     { x = nx; y = ny; return *this; }
    Control& setSize(int nw, int nh)    { w = nw; h = nh; return *this; }
    Control& setVisible(bool v)         { visible = v; return *this; }
    Control& setEnabled(bool e)         { enabled = e; return *this; }
    Control& setChecked(bool v)         { checked = v; return *this; }
    Control& setPrimary(bool p)         { primary = p; return *this; }
    Control& setCenter(bool c)          { center = c; return *this; }
    Control& setStrong(bool s)          { strong = s; return *this; }
    Control& setProgress(float v)       { progress = v; return *this; }
    Control& setRange(float mn, float mx) { min = mn; max = mx; return *this; }
    Control& setStep(float s)           { step = s; return *this; }
    Control& setDecimals(int d)         { decimals = d; return *this; }
    Control& setSelected(int idx)       { selected = idx; return *this; }
    Control& setPlaceholder(const wchar_t* s) { placeholder = s ? s : L""; return *this; }
    Control& setTooltip(const wchar_t* s)     { tooltip = s ? s : L""; return *this; }
    Control& setOnClick(std::function<void()> fn)    { onClick = std::move(fn); return *this; }
    Control& setOnChanged(std::function<void()> fn)  { onChanged = std::move(fn); return *this; }
    Control& setOnEnter(std::function<void()> fn)    { onEnter = std::move(fn); return *this; }
    Control& setPassword(bool p)        { password = p; return *this; }
    Control& setSearchBox(bool s)       { searchBox = s; return *this; }
};

// 自绘模态消息框
inline int showMsgBox(HWND parent, const std::wstring& text,
                      const std::wstring& title, const Theme& th,
                      bool glassEnabled = false);

//============================================================================
// 运行时性能指标（构建 / 绘制）
//============================================================================
struct PerfStats {
    double buildMs = 0, paintMs = 0;       // 最近一次
    double avgBuildMs = 0, avgPaintMs = 0; // 指数滑动平均
    double maxBuildMs = 0, maxPaintMs = 0;
    unsigned long long builds = 0, paints = 0;

    void reset()
    {
        buildMs = paintMs = avgBuildMs = avgPaintMs = 0;
        maxBuildMs = maxPaintMs = 0;
        builds = paints = 0;
    }
};

//============================================================================
// App —— 一个窗口即一个 App
//============================================================================
class App {
public:
    explicit App(const WindowOptions& opt = WindowOptions())
        : m_th(opt.theme), m_opt(opt)
    {
        initialize();
    }

    // 兼容旧写法：App(宽, 高, 标题)
    App(int w, int h, const wchar_t* title,
        const Theme& th = darkTheme(), Effect effect = Effect::None)
    {
        m_th = th;
        m_opt.width = w; m_opt.height = h;
        m_opt.title = title ? title : L"";
        m_opt.theme = th;
        m_opt.effect = effect;
        initialize();
    }

    // 新手推荐：App app(L"标题", 宽, 高);
    // 默认 720x520、深色主题、Acrylic 磨砂玻璃。
    App(const wchar_t* title, int w = 720, int h = 520,
        Effect effect = Effect::Acrylic)
    {
        m_th = darkTheme();
        m_opt.width = w; m_opt.height = h;
        m_opt.title = title ? title : L"";
        m_opt.theme = m_th;
        m_opt.effect = effect;
        initialize();
    }

    // 新手推荐 + 主题：App app(L"标题", 宽, 高, lightTheme());
    App(const wchar_t* title, int w, int h, const Theme& th,
        Effect effect = Effect::Acrylic)
    {
        m_th = th;
        m_opt.width = w; m_opt.height = h;
        m_opt.title = title ? title : L"";
        m_opt.theme = th;
        m_opt.effect = effect;
        initialize();
    }

    ~App()
    {
        if (m_alive) *m_alive = false;
        if (m_postLockInit) {
            DeleteCriticalSection(&m_postLock);
            m_postLockInit = false;
        }
        releaseMeasure();
        if (m_hwnd) DestroyWindow(m_hwnd);
        destroyFonts();
        releaseBuffer();
        if (m_iconBig)   DestroyIcon(m_iconBig);
        if (m_iconSmall) DestroyIcon(m_iconSmall);
    }

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // ---- 窗口 ----
    HWND hwnd() const { return m_hwnd; }
    int  width()  const { return m_cx; }
    int  height() const { return m_cy; }
    int  dpi()    const { return m_dpi; }
    int  dp(int logical) const { return MulDiv(logical, m_dpi, 96); }
    int  titleBarHeight() const { return m_titleH; }
    Theme& theme() { return m_th; }
    Effect effect() const { return m_opt.effect; }

    void setTitle(const wchar_t* t)
    {
        if (!t) return;
        m_opt.title = t;
        if (m_hwnd) {
            SetWindowTextW(m_hwnd, t);
            redraw();
        }
    }

    // 窗口内容区尺寸变化回调（手动布局时用于重新排布控件）
    void onResize(std::function<void(int, int)> fn) { m_onResize = std::move(fn); }

    void setTheme(const Theme& th)
    {
        m_th = th;
        m_opt.theme = th;
        createFonts();
        if (m_opt.effect != Effect::None) {
            applyEffect();       // 同步 DWM 深浅色与玻璃底色
        } else {
            applyDarkMode();
        }
        updateWindowRegion();
        markDirty();
        redraw();
    }

    void setEffect(Effect e, int opacity = -1)
    {
        m_opt.effect = e;
        if (opacity >= 0) m_opt.glassOpacity = std::min(100, std::max(0, opacity));
        applyEffect();
        markDirty();
        redraw();
    }

    void setGlassOpacity(int percent)
    {
        m_opt.glassOpacity = std::min(100, std::max(0, percent));
        m_glassAlpha = std::min(255, std::max(0, m_opt.glassOpacity * 255 / 100));
        if (m_glass && m_opt.effect != Effect::Acrylic && m_opt.effect != Effect::Mica)
            applyEffect();   // 旧系统 blur 需要同步 accent 参数
        markDirty();
        redraw();
    }

    void redraw() { if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE); }
    void close()  { if (m_hwnd) PostMessageW(m_hwnd, WM_CLOSE, 0, 0); }
    void minimize() { if (m_hwnd) ShowWindow(m_hwnd, SW_MINIMIZE); }
    void maximize() { if (m_hwnd) ShowWindow(m_hwnd, SW_MAXIMIZE); }
    void restore()  { if (m_hwnd) ShowWindow(m_hwnd, SW_RESTORE); }
    bool isMaximized() const { return m_hwnd && IsZoomed(m_hwnd) != FALSE; }

    // 自绘模态消息框
    void msg(const wchar_t* text, const wchar_t* title = nullptr)
    {
        showMsgBox(m_hwnd, text ? text : L"", title ? title : L"提示", m_th, m_glass);
    }

    // 定时器
    int timer(int ms, std::function<void()> fn, bool repeat = false)
    {
        if (!m_hwnd || ms <= 0) return 0;
        int id = m_timerSeq++;
        m_timers.emplace(id, TimerRec{ std::move(fn), repeat });
        if (!SetTimer(m_hwnd, (UINT_PTR)id, (UINT)ms, nullptr)) {
            m_timers.erase(id);
            return 0;
        }
        return id;
    }

    void killTimer(int id)
    {
        if (!m_hwnd || id <= 0) return;
        KillTimer(m_hwnd, (UINT_PTR)id);
        m_timers.erase(id);
    }

    // 右下角通知（可堆叠）
    void toast(const wchar_t* text, int ms = 3000)
    {
        if (!m_hwnd || !text) return;
        ToastRec t;
        t.text = text;
        t.totalMs = std::max(600, ms);
        t.t = 0.f;
        m_toasts.push_back(std::move(t));
        ensureAnim();
        redraw();
    }

    // 右键 / 上下文菜单（在鼠标位置弹出）
    void showMenu(std::vector<MenuItem> items)
    {
        m_popup.clear();
        for (MenuItem& it : items) {
            PopupItem p;
            p.text = it.text;
            p.shortcut = it.shortcut;
            p.onClick = it.onClick;
            p.enabled = it.enabled;
            p.separator = it.separator;
            m_popup.push_back(std::move(p));
        }
        m_popupOpen = true;
        m_popupOwner = -2;
        m_popupOwnerKey.clear();
        m_popupMenuIdx = -1;
        m_popupHover = -1;
        openPopupAt(m_mx + 8, m_my + 8);
    }

    int mouseX() const { return m_mx; }
    int mouseY() const { return m_my; }

    // ---- 声明式界面 ----

    // 注册界面构建函数；状态变化时自动重新构建
    void setView(std::function<Element()> builder)
    {
        m_view = std::move(builder);
        markDirty();
        redraw();
    }

    // 请求重建（在下一次绘制前执行；不在回调中同步重构，避免引用失效）
    void rebuild()
    {
        markDirty();
        redraw();
    }

    // 请求重绘 + 重建（修改外部数据后调用）
    void invalidate() { markDirty(); }

    // 自定义绘制钩子：background 在控件之前、foreground 在控件之后绘制。
    // 保留 YRGui 的窗口 / 控件体系，同时允许绘制渐变、光效与动画装饰。
    void setPaintHooks(std::function<void(Gdiplus::Graphics&, int, int)> background,
                       std::function<void(Gdiplus::Graphics&, int, int)> foreground = nullptr)
    {
        m_paintBg = std::move(background);
        m_paintFg = std::move(foreground);
        markDirty();
        redraw();
    }

    // 最近构建 / 绘制耗时（可用于性能面板或日志）
    const PerfStats& stats() const { return m_stats; }
    void resetStats() { m_stats.reset(); }

    // 线程安全：把回调投递回 UI 线程执行。
    // 工作线程只负责计算，结果通过 post() 回到 UI 线程更新 State，
    // 避免在后台线程直接碰窗口 / GDI+ / State 通知。
    void post(std::function<void()> fn)
    {
        if (!m_hwnd || !fn) return;
        if (m_postLockInit) EnterCriticalSection(&m_postLock);
        m_postJobs.push_back(std::move(fn));
        if (m_postLockInit) LeaveCriticalSection(&m_postLock);
        PostMessageW(m_hwnd, kPostMessage, 0, 0);
    }

    // 创建响应式状态
    template <class T>
    State<T> state(T initial = T())
    {
        std::shared_ptr<StateSlot<T>> slot = std::make_shared<StateSlot<T>>();
        slot->value = std::move(initial);
        std::weak_ptr<bool> alive = m_alive;
        slot->notify = [this, alive] {
            if (alive.lock()) this->markDirty();
        };
        return State<T>(slot);
    }

    // 创建字符串状态：StringState name = app.state(L"张三");
    State<std::wstring> state(const wchar_t* initial)
    {
        return state<std::wstring>(initial ? initial : L"");
    }

    // =====================================================================
    // 新手简洁 API
    // ---------------------------------------------------------------------
    //   App app(L"我的应用", 480, 360);
    //   app.title(L"你好");
    //   StringState name = app.input(L"昵称");
    //   app.button(L"问候", [&] {
    //       app.toast((L"你好，" + name.get()).c_str());
    //   }, true);
    //   return app.run();
    // =====================================================================
    void title(const wchar_t* s)        { addUi(Title(s)); }
    void title(const std::wstring& s)   { addUi(Title(s)); }
    void text(const wchar_t* s, int size = 0)      { addUi(Text(s, size)); }
    void text(const std::wstring& s, int size = 0) { addUi(Text(s, size)); }
    void caption(const wchar_t* s)      { addUi(Caption(s)); }
    void caption(const std::wstring& s) { addUi(Caption(s)); }
    State<std::wstring> label(const wchar_t* initial = L"", int size = 0)
    {
        State<std::wstring> st = state<std::wstring>(initial ? initial : L"");
        addUi(Text(st, size));
        return st;
    }
    void space(int h = 10)              { addUi(Space(h)); }
    void divider()                      { addUi(Divider()); }
    void spacer()                       { addUi(Spacer()); }

    void input(State<std::wstring> st, const wchar_t* placeholder = nullptr,
               bool fill = false)
    {
        Element e = Input(std::move(st));
        if (placeholder) e.placeholder(placeholder);
        if (fill) e.fillWidth();
        addUi(std::move(e));
    }
    State<std::wstring> input(const wchar_t* placeholder = nullptr, bool fill = false)
    {
        State<std::wstring> st = state<std::wstring>(L"");
        input(st, placeholder, fill);
        return st;
    }
    void password(State<std::wstring> st, const wchar_t* placeholder = nullptr,
                  bool fill = false)
    {
        Element e = Password(L"").bindText(std::move(st));
        if (placeholder) e.placeholder(placeholder);
        if (fill) e.fillWidth();
        addUi(std::move(e));
    }
    State<std::wstring> password(const wchar_t* placeholder = nullptr, bool fill = false)
    {
        State<std::wstring> st = state<std::wstring>(L"");
        password(st, placeholder, fill);
        return st;
    }
    void search(State<std::wstring> st, const wchar_t* placeholder = nullptr,
                bool fill = false)
    {
        Element e = Search(std::move(st));
        if (placeholder) e.placeholder(placeholder);
        if (fill) e.fillWidth();
        addUi(std::move(e));
    }
    State<std::wstring> search(const wchar_t* placeholder = nullptr, bool fill = false)
    {
        State<std::wstring> st = state<std::wstring>(L"");
        search(st, placeholder, fill);
        return st;
    }
    void button(const wchar_t* label, std::function<void()> onClick = nullptr,
                bool primary = false, bool fill = false)
    {
        Element e = Button(label, std::move(onClick));
        if (primary) e.primary();
        if (fill) e.fillWidth();
        addUi(std::move(e));
    }
    void button(const std::wstring& label, std::function<void()> onClick = nullptr,
                bool primary = false, bool fill = false)
    {
        button(label.c_str(), std::move(onClick), primary, fill);
    }
    void check(const wchar_t* label, State<bool> st)
    {
        addUi(Checkbox(label, std::move(st)));
    }
    State<bool> check(const wchar_t* label)
    {
        State<bool> st = state(false);
        check(label, st);
        return st;
    }
    void toggle(State<bool> st) { addUi(Switch(std::move(st))); }
    State<bool> toggle()
    {
        State<bool> st = state(false);
        toggle(st);
        return st;
    }
    void slider(State<float> st) { addUi(Slider(std::move(st))); }
    State<float> slider()
    {
        State<float> st = state(0.5f);
        slider(st);
        return st;
    }
    void progress(float value) { addUi(Progress(value)); }
    void progress(State<float> st) { addUi(Progress(std::move(st))); }
    void combo(std::initializer_list<const wchar_t*> items, State<int> st)
    {
        addUi(Combo(items, std::move(st)));
    }
    State<int> combo(std::initializer_list<const wchar_t*> items, int initial = 0)
    {
        State<int> st = state(initial);
        combo(items, st);
        return st;
    }
    void radioGroup(std::initializer_list<const wchar_t*> items, State<int> st)
    {
        addUi(RadioGroup(items, std::move(st)));
    }
    State<int> radioGroup(std::initializer_list<const wchar_t*> items, int initial = 0)
    {
        State<int> st = state(initial);
        radioGroup(items, st);
        return st;
    }
    void link(const wchar_t* label, std::function<void()> onClick = nullptr)
    {
        addUi(Link(label, std::move(onClick)));
    }
    void image(const wchar_t* path, int height = 140)
    {
        Element e = Image(path, height);
        e.fill();
        addUi(std::move(e));
    }
    void textArea(State<std::wstring> st, const wchar_t* placeholder = nullptr,
                  int height = 110)
    {
        Element e = TextArea(std::move(st));
        if (placeholder) e.placeholder(placeholder);
        e.height(height);
        addUi(std::move(e));
    }
    State<std::wstring> textArea(const wchar_t* placeholder = nullptr, int height = 110)
    {
        State<std::wstring> st = state<std::wstring>(L"");
        textArea(st, placeholder, height);
        return st;
    }
    State<float> spinBox(float value = 0.f, float min = 0.f, float max = 100.f,
                         float step = 1.f)
    {
        State<float> st = state(value);
        Element e = SpinBox(st, min, max, step);
        e.fillWidth();
        addUi(std::move(e));
        return st;
    }
    void spinBox(State<float> st, float min = 0.f, float max = 100.f,
                 float step = 1.f)
    {
        Element e = SpinBox(std::move(st), min, max, step);
        e.fillWidth();
        addUi(std::move(e));
    }
    void stack(std::function<void()> body)
    {
        if (!m_uiActive || !m_uiCurrent) return;
        std::vector<Element>* parent = m_uiCurrent;
        std::vector<Element> kids;
        m_uiCurrent = &kids;
        if (body) body();
        m_uiCurrent = parent;
        Element s = Stack({});
        s.kids = std::move(kids);
        parent->push_back(std::move(s));
    }

    void badge(const wchar_t* label, int color = 0)
    {
        Element e = Badge(label);
        if (color) e.accent(color);
        addUi(std::move(e));
    }
    void tag(const wchar_t* label, std::function<void()> onClose = nullptr)
    {
        if (onClose) addUi(Tag(label, std::move(onClose)));
        else         addUi(Tag(label));
    }
    void spinner() { addUi(Spinner()); }
    void rating(int maxStars, State<int> st)
    {
        addUi(Rating(maxStars, std::move(st)));
    }
    State<int> rating(int maxStars = 5, int value = 0)
    {
        State<int> st = state(value);
        addUi(Rating(maxStars, st));
        return st;
    }
    void calendar(int height = 280, int width = 280)
    {
        Element e = Calendar();
        e.width(width).height(height);
        addUi(std::move(e));
    }
    void list(std::initializer_list<const wchar_t*> items, State<int> selected,
              int height = 180)
    {
        Element e = ListBox(items, selected.get());
        e.height(height).bindSelected(std::move(selected));
        addUi(std::move(e));
    }
    State<int> list(std::initializer_list<const wchar_t*> items, int height = 180)
    {
        State<int> st = state(0);
        Element e = ListBox(items, 0);
        e.height(height).bindSelected(st);
        addUi(std::move(e));
        return st;
    }
    void table(std::initializer_list<const wchar_t*> headers,
               std::vector<std::vector<std::wstring>> rows,
               State<int> selected, int height = 260)
    {
        Element e = Table(headers, std::move(rows));
        e.height(height).bindSelected(std::move(selected));
        addUi(std::move(e));
    }
    State<int> table(std::initializer_list<const wchar_t*> headers,
                     std::vector<std::vector<std::wstring>> rows, int height = 260)
    {
        State<int> st = state(0);
        Element e = Table(headers, std::move(rows));
        e.height(height).bindSelected(st);
        addUi(std::move(e));
        return st;
    }
    void tree(std::vector<TreeNode> nodes, int height = 260)
    {
        Element e = Tree(std::move(nodes));
        e.height(height);
        addUi(std::move(e));
    }
    void menuBar(std::vector<MenuEntry> menus)
    {
        addUi(MenuBar(std::move(menus)));
    }
    void statusBar(std::initializer_list<const wchar_t*> parts)
    {
        addUi(StatusBar(parts));
    }

    // 表单行：左边标签，右边控件组（自动占满剩余宽度）
    void formRow(const wchar_t* label, std::function<void()> body,
                 int labelWidth = 90)
    {
        if (!m_uiActive || !m_uiCurrent) return;
        std::vector<Element>* parent = m_uiCurrent;
        std::vector<Element> kids;
        m_uiCurrent = &kids;
        if (body) body();
        m_uiCurrent = parent;
        Element col = Column(std::move(kids));
        col.p.gap = 8;
        col.p.fillWidth = true;
        Element row = Row({ Text(label ? label : L"").width(labelWidth), col }, 12);
        row.p.align = Align::Center;
        parent->push_back(std::move(row));
    }

    // 带标题的卡片
    void section(const wchar_t* title, std::function<void()> body,
                 int gap = 12, bool fillHeight = false)
    {
        card([&] {
            if (title) text(title, 16);
            if (body) body();
        }, gap, fillHeight);
    }

    // 等宽网格：app.grid(3, { [&]{...}, [&]{...}, [&]{...} });
    void grid(int columns, std::initializer_list<std::function<void()>> cells,
              int gap = 12)
    {
        if (!m_uiActive || !m_uiCurrent || columns <= 0) return;
        std::vector<Element> rows;
        std::vector<Element> rowCells;
        for (const std::function<void()>& body : cells) {
            std::vector<Element> kids;
            std::vector<Element>* prev = m_uiCurrent;
            m_uiCurrent = &kids;
            if (body) body();
            m_uiCurrent = prev;
            Element cell = Column(std::move(kids));
            cell.p.gap = 8;
            cell.p.expand = true;
            cell.p.align = Align::Stretch;
            rowCells.push_back(std::move(cell));
            if ((int)rowCells.size() == columns) {
                rows.push_back(Row(std::move(rowCells), gap));
                rowCells.clear();
            }
        }
        if (!rowCells.empty()) {
            while ((int)rowCells.size() < columns) rowCells.push_back(Spacer());
            rows.push_back(Row(std::move(rowCells), gap));
        }
        Element g = Column(std::move(rows), gap);
        m_uiCurrent->push_back(std::move(g));
    }

    // 可滚动内容：放在页面最后，自动占满剩余高度
    void scroll(std::function<void()> body, int gap = 12)
    {
        if (!m_uiActive || !m_uiCurrent) return;
        std::vector<Element>* parent = m_uiCurrent;
        std::vector<Element> kids;
        m_uiCurrent = &kids;
        if (body) body();
        m_uiCurrent = parent;
        Element content = Column(std::move(kids), gap);
        Element s = Scroll({ std::move(content) });
        s.fill();
        parent->push_back(std::move(s));
    }

    // 简易多页面：app.tabs({ L"首页", L"设置" }, page, {
    //     [&] { app.text(L"首页内容"); },
    //     [&] { app.check(L"启用", enabled); },
    // });
    void tabs(std::initializer_list<const wchar_t*> items, State<int> selected,
              std::initializer_list<std::function<void()>> pages)
    {
        if (!m_uiActive || !m_uiCurrent) return;
        const int current = selected.get();
        const bool buildAll = m_uiSetup;   // 顺序创建时所有页都建好，切换不空白
        std::vector<Element> pageElems;
        int index = 0;
        for (const std::function<void()>& body : pages) {
            if (buildAll || index == current) {
                std::vector<Element> kids;
                std::vector<Element>* prev = m_uiCurrent;
                m_uiCurrent = &kids;
                if (body) body();
                m_uiCurrent = prev;
                pageElems.push_back(Column(kids).gap(12).fill());
            } else {
                pageElems.push_back(Column({}).fill());
            }
            ++index;
        }
        Element t = Tabs(items, std::move(selected), std::move(pageElems));
        t.fill();
        m_uiCurrent->push_back(std::move(t));
    }

    void row(std::function<void()> body, int gap = 10)
    {
        if (!m_uiActive || !m_uiCurrent) return;
        std::vector<Element>* parent = m_uiCurrent;
        std::vector<Element> kids;
        m_uiCurrent = &kids;
        if (body) body();
        m_uiCurrent = parent;
        parent->push_back(Row(kids, gap));
    }

    void column(std::function<void()> body, int gap = 8)
    {
        if (!m_uiActive || !m_uiCurrent) return;
        std::vector<Element>* parent = m_uiCurrent;
        std::vector<Element> kids;
        m_uiCurrent = &kids;
        if (body) body();
        m_uiCurrent = parent;
        parent->push_back(Column(std::move(kids), gap));
    }

    void card(std::function<void()> body, int gap = 12, bool fillHeight = false)
    {
        if (!m_uiActive || !m_uiCurrent) return;
        std::vector<Element>* parent = m_uiCurrent;
        std::vector<Element> kids;
        m_uiCurrent = &kids;
        if (body) body();
        m_uiCurrent = parent;
        Element c(Kind::Card);
        c.kids = std::move(kids);
        c.p.pad = 16;
        c.p.gap = gap;
        c.p.fillWidth = true;   // 在 Row 中自动等宽；在 Column 中本来就是整行
        if (fillHeight) c.p.expand = true;
        parent->push_back(std::move(c));
    }

    void addUi(Element e)
    {
        if (m_uiActive && m_uiCurrent) m_uiCurrent->push_back(std::move(e));
    }

    // ---- 手动布局（绝对定位）----
    // UI 类使用这些方法直接添加 Control；不再经过 Element 树。
    int addControl(Control c)
    {
        m_ctrls.push_back(std::move(c));
        m_dirty = true;
        redraw();
        return (int)m_ctrls.size() - 1;
    }
    Control* control(int id)
    {
        if (id < 0 || id >= (int)m_ctrls.size()) return nullptr;
        return &m_ctrls[(size_t)id];
    }
    void clearControls()
    {
        m_ctrls.clear();
        m_focus = -1;
        m_focusKey.clear();
        m_captureKey.clear();
        m_popupOpen = false;
        m_dirty = true;
        redraw();
    }

    // 按 key 查找控件 / 访问控件列表（高级用法）
    Control* find(const wchar_t* key)
    {
        if (!key) return nullptr;
        for (Control& c : m_ctrls)
            if (c.key == key) return &c;
        return nullptr;
    }
    Control& at(int id) { return m_ctrls[(size_t)id]; }
    int count() const { return (int)m_ctrls.size(); }

    // ---- 主循环 ----
    int run()
    {
        if (!m_hwnd) return 1;
        // 结束“顺序创建”阶段：之后回调里再调用 app.xxx 不会追加控件
        m_uiActive = false;
        m_uiCurrent = nullptr;
        m_uiSetup = false;
        if (!m_view && !m_pendingUi.empty()) {
            setView([this] {
                return Column(m_pendingUi).padding(m_uiPad).gap(m_uiGap);
            });
        }
        if (const char* env = std::getenv("YR_GUI_AUTO_CLOSE_MS")) {
            int ms = atoi(env);
            if (ms > 0) timer(ms, [this] { close(); });
        }
        ensureBuilt();
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        m_hwnd = nullptr;
        return (int)msg.wParam;
    }

private:
    //========================================================================
    // 内部实现
    //========================================================================
    HWND     m_hwnd = nullptr;
    Theme    m_th;
    WindowOptions m_opt;
    std::function<void(int, int)> m_onResize;
    int      m_dpi = 96;
    int      m_winBuild = 0;
    int      m_cx = 0, m_cy = 0;
    int      m_titleH = 44;
    bool     m_active = true;
    int      m_hoverChrome = 0;          // 1 最小化 / 2 最大化 / 3 关闭

    bool     m_glass = false;
    int      m_glassAlpha = 180;
    DWORD    m_glassTint = 0x14161B;

    Gdiplus::Font* m_font = nullptr;
    Gdiplus::Font* m_fontTitle = nullptr;
    std::map<int, Gdiplus::Font*> m_fonts;   // key = 像素字号 * 2 + bold

    HICON    m_iconBig = nullptr;
    HICON    m_iconSmall = nullptr;

    // ---- 自定义绘制 ----
    std::function<void(Gdiplus::Graphics&, int, int)> m_paintBg;
    std::function<void(Gdiplus::Graphics&, int, int)> m_paintFg;

    // ---- 声明式界面 ----
    std::function<Element()> m_view;
    std::vector<Element> m_pendingUi;        // run() 之前顺序创建的控件
    std::vector<Element>* m_uiCurrent = nullptr;
    bool m_uiActive = false;
    bool m_uiSetup = true;
    int  m_uiPad = 20;
    int  m_uiGap = 12;
    bool     m_dirty = true;
    std::vector<Control> m_ctrls;
    std::map<std::wstring, int> m_scroll;
    struct ScrollArea { std::wstring key; RECT rect; int contentH; };
    std::vector<ScrollArea> m_scrollAreas;
    std::wstring m_focusKey;
    std::wstring m_captureKey;
    std::wstring m_popupOwnerKey;

    // ---- 焦点 / 捕获 / 鼠标 ----
    int      m_focus = -1;
    int      m_capturedId = -1;
    int      m_mx = 0, m_my = 0;

    // ---- 动画 / 定时器 ----
    bool     m_animOn = false;
    int      m_timerSeq = 0x1000;
    struct TimerRec { std::function<void()> fn; bool repeat; };
    std::map<int, TimerRec> m_timers;

    // ---- 弹出菜单 ----
    struct PopupItem {
        std::wstring text;
        std::wstring shortcut;
        std::function<void()> onClick;
        bool enabled = true;
        bool separator = false;
    };
    std::vector<PopupItem> m_popup;
    bool m_popupOpen = false;
    int  m_popupX = 0, m_popupY = 0;
    int  m_popupW = 0, m_popupH = 0;
    int  m_popupHover = -1;
    int  m_popupOwner = -1;    // 菜单栏控件 id；-2 = 右键菜单
    int  m_popupMenuIdx = -1;

    // ---- 悬浮提示 ----
    std::wstring m_tipText;
    bool m_tipShow = false;
    int  m_tipX = 0, m_tipY = 0;
    int  m_tipTimerId = 0;
    int  m_tipCtrlId = -1;

    // ---- 右下角通知 ----
    struct ToastRec {
        std::wstring text;
        int totalMs;
        float t;
    };
    std::vector<ToastRec> m_toasts;

    // ---- 后台缓冲（32bpp，支持真透明）----
    HDC     m_memDC = nullptr;
    HBITMAP m_memBmp = nullptr;
    HGDIOBJ m_oldBmp = nullptr;
    void*   m_bits = nullptr;
    int     m_bufW = 0, m_bufH = 0;

    std::shared_ptr<bool> m_alive;

    // ---- 性能指标 ----
    PerfStats m_stats;

    // ---- 文本测量缓存（构建 / 绘制共用）----
    mutable HDC m_measureDC = nullptr;
    mutable std::unique_ptr<Gdiplus::Graphics> m_measureG;
    mutable std::map<std::pair<const Gdiplus::Font*, std::wstring>, int> m_textW;

    // ---- 跨线程投递 ----
    CRITICAL_SECTION m_postLock = {};
    bool m_postLockInit = false;
    std::vector<std::function<void()>> m_postJobs;
    static const UINT kPostMessage = WM_APP + 0x51;

    //========================================================================
    // 初始化 / 资源
    //========================================================================
    void initialize()
    {
        if (!m_postLockInit) {
            InitializeCriticalSection(&m_postLock);
            m_postLockInit = true;
        }
        initGdiplus();
        initDpi();
        m_winBuild = yrWindowsBuild();
        createWindow();
        applyEffect();
        createFonts();
        updateClientSize();
        createIcon();
        m_alive = std::make_shared<bool>(true);
        m_titleH = dp(44);
        // 顺序创建模式：app.title(...) 直接追加到根容器，不要求 run(body)
        m_pendingUi.clear();
        m_uiCurrent = &m_pendingUi;
        m_uiActive = true;
        m_uiSetup = true;
        if (m_hwnd) {
            SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
            m_dirty = true;
        }
    }

    static double nowMs()
    {
        static LARGE_INTEGER freq = [] {
            LARGE_INTEGER f = {};
            QueryPerformanceFrequency(&f);
            return f;
        }();
        LARGE_INTEGER c = {};
        QueryPerformanceCounter(&c);
        return (double)c.QuadPart * 1000.0 / (double)freq.QuadPart;
    }

    static void initGdiplus()
    {
        static ULONG_PTR token = [] {
            Gdiplus::GdiplusStartupInput in;
            ULONG_PTR tok = 0;
            Gdiplus::GdiplusStartup(&tok, &in, nullptr);
            return tok;
        }();
        (void)token;
    }

    void initDpi()
    {
        static bool dpiSet = [] {
            if (HMODULE u = LoadLibraryW(L"user32.dll")) {
                typedef BOOL(WINAPI *Fn)(HANDLE);
                Fn fn = (Fn)(void*)GetProcAddress(u, "SetProcessDpiAwarenessContext");
                if (fn) fn((HANDLE)-4);   // Per-Monitor V2
            }
            return true;
        }();
        (void)dpiSet;

        m_dpi = 96;
        if (HMODULE u = GetModuleHandleW(L"user32.dll")) {
            typedef UINT(WINAPI *Fn)();
            Fn fn = (Fn)(void*)GetProcAddress(u, "GetDpiForSystem");
            if (fn) {
                UINT d = fn();
                if (d >= 72) m_dpi = (int)d;
            }
        }
    }

    void createWindow()
    {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc   = &App::wndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"YRGuiWindow";
        wc.hIcon         = nullptr;
        wc.hIconSm       = nullptr;
        RegisterClassExW(&wc);

        DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
        if (!m_opt.resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

        int w = std::max(1, dp(m_opt.width));
        int h = std::max(1, dp(m_opt.height));
        int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
        if (m_opt.center) {
            RECT work = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
            x = work.left + std::max(0, (int)((work.right - work.left - w) / 2));
            y = work.top + std::max(0, (int)((work.bottom - work.top - h) / 2));
        }

        m_hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"YRGuiWindow", m_opt.title.c_str(), style,
                                 x, y, w, h, nullptr, nullptr,
                                 wc.hInstance, this);
        if (!m_hwnd) return;
        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
        m_titleH = dp(44);
    }

    void applyDarkMode()
    {
        if (!m_hwnd) return;
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        if (!dwm) return;
        typedef HRESULT(WINAPI *Fn)(HWND, DWORD, LPCVOID, DWORD);
        Fn f = (Fn)(void*)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (!f) return;
        int dark = m_th.dark ? 1 : 0;
        f(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        int corner = DWMWA_WINDOW_CORNER_PREFERENCE_ROUND;
        f(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        DWORD borderNone = 0xFFFFFFFE;   // DWMWA_COLOR_NONE
        f(m_hwnd, DWMWA_BORDER_COLOR, &borderNone, sizeof(borderNone));
    }

    void applyEffect()
    {
        m_glass = (m_opt.effect != Effect::None);
        m_glassTint = m_opt.glassTint ? m_opt.glassTint
                                      : (m_th.dark ? 0x14161B : 0xF2F4F7);
        m_glassAlpha = std::min(255, std::max(0, m_opt.glassOpacity * 255 / 100));
        if (!m_hwnd) return;
        applyDarkMode();

        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        typedef HRESULT(WINAPI *DwmAttr)(HWND, DWORD, LPCVOID, DWORD);
        DwmAttr attr = dwm ? (DwmAttr)(void*)GetProcAddress(dwm, "DwmSetWindowAttribute") : nullptr;
        typedef HRESULT(WINAPI *Extend)(HWND, const MARGINS*);
        Extend extend = dwm ? (Extend)(void*)GetProcAddress(dwm, "DwmExtendFrameIntoClientArea") : nullptr;

        if (!m_glass) {
            if (attr) {
                int none = DWMSBT_NONE;
                attr(m_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &none, sizeof(none));
            }
            if (extend) {
                MARGINS m = { 0, 0, 0, 0 };
                extend(m_hwnd, &m);
            }
            disableAccent();
            return;
        }

        if (extend) {
            MARGINS m = { -1, -1, -1, -1 };
            extend(m_hwnd, &m);
        }

        bool applied = false;
        if (attr) {
            int type = (m_opt.effect == Effect::Mica) ? DWMSBT_MAINWINDOW
                     : (m_opt.effect == Effect::Acrylic) ? DWMSBT_TRANSIENTWINDOW
                     : DWMSBT_NONE;
            if (m_opt.effect != Effect::Blur) {
                applied = SUCCEEDED(attr(m_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type)));
            }
        }
        if (!applied) {
            // Windows 10 / 不支持 DWM 背景材质时的回退：亚克力模糊
            applyAccent(m_opt.effect == Effect::Blur ? 3 : 4);
        }
    }

    void applyAccent(int state)
    {
        if (!m_hwnd) return;
        struct AccentPolicy {
            int   AccentState;
            DWORD AccentFlags;
            DWORD GradientColor;
            DWORD AnimationId;
        };
        struct WCAData {
            int   Attribute;
            PVOID Data;
            SIZE_T Size;
        };
        typedef BOOL(WINAPI *Fn)(HWND, WCAData*);
        HMODULE u = GetModuleHandleW(L"user32.dll");
        Fn fn = u ? (Fn)(void*)GetProcAddress(u, "SetWindowCompositionAttribute") : nullptr;
        if (!fn) return;
        AccentPolicy ap = {};
        ap.AccentState = state;   // 4 = ACCENT_ENABLE_ACRYLICBLURBEHIND
        ap.AccentFlags = 2;
        DWORD tint = m_glassTint;
        ap.GradientColor = ((DWORD)m_glassAlpha << 24)
                         | (((tint >> 0) & 0xFF) << 16)
                         | (((tint >> 8) & 0xFF) << 8)
                         |  ((tint >> 16) & 0xFF);
        WCAData data = { 19 /*WCA_ACCENT_POLICY*/, &ap, sizeof(ap) };
        fn(m_hwnd, &data);
    }

    void disableAccent()
    {
        if (!m_hwnd) return;
        struct AccentPolicy {
            int   AccentState;
            DWORD AccentFlags;
            DWORD GradientColor;
            DWORD AnimationId;
        };
        struct WCAData {
            int   Attribute;
            PVOID Data;
            SIZE_T Size;
        };
        typedef BOOL(WINAPI *Fn)(HWND, WCAData*);
        HMODULE u = GetModuleHandleW(L"user32.dll");
        Fn fn = u ? (Fn)(void*)GetProcAddress(u, "SetWindowCompositionAttribute") : nullptr;
        if (!fn) return;
        AccentPolicy ap = {};
        ap.AccentState = 0;   // ACCENT_DISABLED
        WCAData data = { 19, &ap, sizeof(ap) };
        fn(m_hwnd, &data);
    }

    Gdiplus::Font* makeFont(int logicalSize, bool bold)
    {
        int px = std::max(8, dp(logicalSize));
        int key = px * 2 + (bold ? 1 : 0);
        std::map<int, Gdiplus::Font*>::iterator it = m_fonts.find(key);
        if (it != m_fonts.end()) return it->second;
        Gdiplus::Font* f = new Gdiplus::Font(
            L"Segoe UI", (REAL)px,
            bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
        if (f->GetLastStatus() != Gdiplus::Ok) {
            delete f;
            f = new Gdiplus::Font(L"Microsoft YaHei UI", (REAL)px,
                                  bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular,
                                  Gdiplus::UnitPixel);
        }
        m_fonts[key] = f;
        return f;
    }

    void createFonts()
    {
        destroyFonts();
        m_textW.clear();
        m_font = makeFont(m_th.fontSize, false);
        m_fontTitle = makeFont(m_th.titleSize, true);
    }

    void destroyFonts()
    {
        for (std::pair<const int, Gdiplus::Font*>& kv : m_fonts) delete kv.second;
        m_fonts.clear();
        m_font = nullptr;
        m_fontTitle = nullptr;
    }

    const Gdiplus::Font* fontFor(const Control& c) const
    {
        int logical = c.fontSize > 0 ? c.fontSize : m_th.fontSize;
        return const_cast<App*>(this)->makeFont(logical, c.bold);
    }

    const Gdiplus::Font* fontFor(const Element& e) const
    {
        int logical = e.p.fontSize > 0 ? e.p.fontSize
                    : (e.tag == Kind::Title ? m_th.titleSize : m_th.fontSize);
        return const_cast<App*>(this)->makeFont(logical, e.tag == Kind::Title);
    }

    void updateClientSize()
    {
        if (!m_hwnd) return;
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        m_cx = rc.right - rc.left;
        m_cy = rc.bottom - rc.top;
        m_titleH = dp(44);
        updateWindowRegion();
        if (m_onResize) m_onResize(m_cx, m_cy);
    }

    // 无边框窗口的圆角：Win11 的 DWM 圆角对自定义边框并不总是生效，
    // 这里直接用窗口区域保证所有系统上都有一致的小圆角。
    void updateWindowRegion()
    {
        // Windows 11 的 DWM 自带抗锯齿圆角；再叠加窗口区域会在四角
        // 形成一层“双击圆角”，留下锯齿和零散像素。Win11 交给 DWM，
        // 仅 Win10 使用区域裁切作为回退。
        if (!m_hwnd || m_winBuild >= 22000) return;
        if (IsZoomed(m_hwnd) || IsIconic(m_hwnd) || m_cx <= 0 || m_cy <= 0) {
            SetWindowRgn(m_hwnd, nullptr, FALSE);
            return;
        }
        int r = dp(m_th.radiusLg);
        HRGN rgn = CreateRoundRectRgn(0, 0, m_cx + 1, m_cy + 1, r * 2, r * 2);
        if (rgn) SetWindowRgn(m_hwnd, rgn, TRUE);
    }

    void createIcon()
    {
        if (!m_hwnd) return;
        const int S = 32;
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = S;
        bi.bmiHeader.biHeight = -S;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC dc = CreateCompatibleDC(nullptr);
        HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!color) { DeleteDC(dc); return; }
        HGDIOBJ old = SelectObject(dc, color);
        memset(bits, 0, S * S * 4);
        {
            Gdiplus::Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath p;
            buildRoundPath(p, 1.f, 1.f, (REAL)(S - 2), (REAL)(S - 2), 8.f);
            Gdiplus::LinearGradientBrush br(Gdiplus::PointF(0, 0), Gdiplus::PointF((REAL)S, (REAL)S),
                                           rgb(m_th.accentHover), rgb(m_th.accentPressed));
            g.FillPath(&br, &p);
            Gdiplus::Font f(L"Segoe UI", 18.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush tb(rgb(0xFFFFFF));
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(L"Y", -1, &f, Gdiplus::RectF(0, 0, (REAL)S, (REAL)S), &fmt, &tb);
        }
        SelectObject(dc, old);
        DeleteDC(dc);
        HBITMAP mask = CreateBitmap(S, S, 1, 1, nullptr);
        ICONINFO ii = {};
        ii.fIcon = TRUE;
        ii.hbmColor = color;
        ii.hbmMask = mask;
        m_iconBig = CreateIconIndirect(&ii);
        m_iconSmall = (HICON)CopyIcon(m_iconBig);
        DeleteObject(color);
        DeleteObject(mask);
        if (m_iconBig)  SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)m_iconBig);
        if (m_iconSmall) SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)m_iconSmall);
    }

    bool ensureBuffer(int w, int h)
    {
        if (m_memDC && w == m_bufW && h == m_bufH) return true;
        releaseBuffer();
        HDC screen = GetDC(nullptr);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        m_memDC = CreateCompatibleDC(screen);
        m_memBmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
        ReleaseDC(nullptr, screen);
        if (!m_memDC || !m_memBmp || !m_bits) { releaseBuffer(); return false; }
        m_oldBmp = SelectObject(m_memDC, m_memBmp);
        m_bufW = w;
        m_bufH = h;
        return true;
    }

    void releaseBuffer()
    {
        if (m_memDC) {
            if (m_oldBmp) SelectObject(m_memDC, m_oldBmp);
            DeleteDC(m_memDC);
        }
        if (m_memBmp) DeleteObject(m_memBmp);
        m_memDC = nullptr;
        m_memBmp = nullptr;
        m_bits = nullptr;
        m_oldBmp = nullptr;
        m_bufW = m_bufH = 0;
    }

    void markDirty()
    {
        m_dirty = true;
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    void syncBinding(Control& c)
    {
        if (!c.binding) return;
        if (c.binding->kind == 1 && c.binding->setText) c.binding->setText(c.text);
        else if (c.binding->kind == 2 && c.binding->setBool) c.binding->setBool(c.checked);
        else if (c.binding->kind == 3 && c.binding->setFloat) c.binding->setFloat(c.progress);
        else if (c.binding->kind == 4 && c.binding->setInt) {
            if (c.type == Ctl::Table) c.binding->setInt(c.selectedRow);
            else if (c.type == Ctl::Rating) c.binding->setInt(c.rating);
            else c.binding->setInt(c.selected);
        }
    }

    void fireChanged(Control& c)
    {
        syncBinding(c);
        if (c.onChanged) c.onChanged();
        markDirty();
    }

    int indexOfKey(const std::wstring& key) const
    {
        if (key.empty()) return -1;
        for (int i = 0; i < (int)m_ctrls.size(); ++i)
            if (m_ctrls[i].key == key) return i;
        return -1;
    }

    void setFocus(int id)
    {
        if (m_focus >= 0 && m_focus < (int)m_ctrls.size())
            m_ctrls[m_focus].focused = false;
        m_focus = id;
        if (id >= 0 && id < (int)m_ctrls.size()) {
            m_ctrls[id].focused = true;
            m_focusKey = m_ctrls[id].key;
        } else {
            m_focusKey.clear();
        }
        redraw();
    }

    void setCapture(int id)
    {
        m_capturedId = id;
        m_captureKey = (id >= 0 && id < (int)m_ctrls.size()) ? m_ctrls[id].key : L"";
    }

    //========================================================================
    // 测量 / 布局 / 重建
    //========================================================================
    struct LSize {
        int w, h;
        LSize(int w_ = 0, int h_ = 0) : w(w_), h(h_) {}
    };
    struct LBox {
        int x, y, w, h;
        LBox(int x_ = 0, int y_ = 0, int w_ = 0, int h_ = 0)
            : x(x_), y(y_), w(w_), h(h_) {}
    };

    static RECT intersectRect(const RECT& a, const RECT& b)
    {
        RECT r;
        r.left   = std::max(a.left, b.left);
        r.top    = std::max(a.top, b.top);
        r.right  = std::min(a.right, b.right);
        r.bottom = std::min(a.bottom, b.bottom);
        if (r.right < r.left) r.right = r.left;
        if (r.bottom < r.top) r.bottom = r.top;
        return r;
    }

    bool ensureMeasure() const
    {
        if (m_measureG) return true;
        m_measureDC = GetDC(nullptr);
        if (!m_measureDC) return false;
        m_measureG.reset(new Gdiplus::Graphics(m_measureDC));
        m_measureG->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        m_measureG->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        return true;
    }

    void releaseMeasure()
    {
        m_measureG.reset();
        if (m_measureDC) {
            ReleaseDC(nullptr, m_measureDC);
            m_measureDC = nullptr;
        }
        m_textW.clear();
    }

    // 带缓存的文本宽度：构建与绘制共用，避免每次 GetDC + MeasureString
    int cachedTextWidth(const std::wstring& s, const Gdiplus::Font* f) const
    {
        if (s.empty() || !f) return 0;
        std::pair<const Gdiplus::Font*, std::wstring> key(f, s);
        std::map<std::pair<const Gdiplus::Font*, std::wstring>, int>::iterator it = m_textW.find(key);
        if (it != m_textW.end()) return it->second;
        if (!ensureMeasure()) return (int)s.size() * 8;
        Gdiplus::StringFormat fmt;
        Gdiplus::RectF rc;
        m_measureG->MeasureString(s.c_str(), (int)s.size(), f,
                                  Gdiplus::PointF(0, 0), &fmt, &rc);
        int w = (int)std::ceil(rc.Width);
        if (m_textW.size() > 4096) m_textW.clear();
        m_textW[key] = w;
        return w;
    }

    int measureTextWidth(const std::wstring& s, const Gdiplus::Font* f)
    {
        return cachedTextWidth(s, f);
    }

    int lineHeight(const Gdiplus::Font* f)
    {
        if (!f) return dp(m_th.fontSize) + 6;
        if (!ensureMeasure()) return dp(m_th.fontSize) + 6;
        return (int)std::ceil(f->GetHeight(m_measureG.get()));
    }

    int measureWrappedHeight(const std::wstring& s, const Gdiplus::Font* f, int maxW)
    {
        if (s.empty() || !f || maxW <= 0) return lineHeight(f);
        if (!ensureMeasure()) return lineHeight(f);
        Gdiplus::StringFormat fmt;
        fmt.SetTrimming(Gdiplus::StringTrimmingNone);
        Gdiplus::RectF rc;
        m_measureG->MeasureString(s.c_str(), (int)s.size(), f,
                                  Gdiplus::RectF(0, 0, (REAL)maxW, 100000.f), &fmt, &rc);
        return (int)std::ceil(rc.Height);
    }

    int textLogicalSize(const Element& e) const
    {
        if (e.p.fontSize > 0) return e.p.fontSize;
        return e.tag == Kind::Title ? m_th.titleSize : m_th.fontSize;
    }

    bool intrinsicWidthInColumn(Kind k) const
    {
        return k == Kind::Button || k == Kind::Badge || k == Kind::Tag ||
               k == Kind::Rating || k == Kind::Switch || k == Kind::Checkbox ||
               k == Kind::Radio || k == Kind::Spinner || k == Kind::Link ||
               k == Kind::Image;
    }

    // Row 内只有这些“容器型”元素默认纵向拉伸；按钮 / 输入框等居中，
    // 避免进度条被拉成和文字一样高。
    bool containerKind(Kind k) const
    {
        return k == Kind::Column || k == Kind::Row || k == Kind::Stack ||
               k == Kind::Scroll || k == Kind::Card || k == Kind::Tabs ||
               k == Kind::MenuBar || k == Kind::StatusBar;
    }

    bool stretchHeightInRow(Kind k) const
    {
        return k == Kind::Column || k == Kind::Row || k == Kind::Stack ||
               k == Kind::Scroll || k == Kind::Card || k == Kind::Image ||
               k == Kind::Divider || k == Kind::TextArea || k == Kind::ListBox ||
               k == Kind::Table || k == Kind::Tree || k == Kind::Calendar;
    }

    int buttonTextWidth(const Element& e)
    {
        return measureTextWidth(e.p.text, fontFor(e)) + dp(30);
    }

    int elementSelected(const Element& e) const
    {
        int sel = e.p.selected;
        if (e.p.binding && e.p.binding->kind == 4 && e.p.binding->getInt)
            sel = e.p.binding->getInt();
        return sel;
    }

    LSize measure(const Element& e, int maxW, int maxH)
    {
        if (!e.p.visible) return LSize{ 0, 0 };
        const int pad = dp(e.p.pad);
        const int gap = dp(e.p.gap);
        const int innerMaxW = std::max(0, maxW - pad * 2);

        switch (e.tag) {
        case Kind::Column:
        case Kind::Card: {
            int wmax = 0, hsum = 0, n = 0;
            for (const Element& k : e.kids) {
                if (!k.p.visible) continue;
                LSize s = measure(k, innerMaxW, maxH);
                wmax = std::max(wmax, s.w);
                hsum += s.h;
                ++n;
            }
            int w = e.p.width >= 0 ? dp(e.p.width) : wmax + pad * 2;
            int h = e.p.height >= 0 ? dp(e.p.height)
                                    : hsum + (n > 0 ? gap * (n - 1) : 0) + pad * 2;
            return LSize{ w, h };
        }
        case Kind::Row: {
            int sum = 0, hmax = 0, n = 0;
            for (const Element& k : e.kids) {
                if (!k.p.visible) continue;
                LSize s = measure(k, maxW, maxH);
                sum += s.w;
                hmax = std::max(hmax, s.h);
                ++n;
            }
            int w = e.p.width >= 0 ? dp(e.p.width)
                                   : sum + (n > 0 ? gap * (n - 1) : 0) + pad * 2;
            int h = e.p.height >= 0 ? dp(e.p.height) : hmax + pad * 2;
            return LSize{ w, h };
        }
        case Kind::Stack: {
            int wmax = 0, hmax = 0;
            for (const Element& k : e.kids) {
                if (!k.p.visible) continue;
                LSize s = measure(k, innerMaxW, maxH);
                wmax = std::max(wmax, s.w);
                hmax = std::max(hmax, s.h);
            }
            int w = e.p.width >= 0 ? dp(e.p.width) : wmax + pad * 2;
            int h = e.p.height >= 0 ? dp(e.p.height) : hmax + pad * 2;
            return LSize{ w, h };
        }
        case Kind::Scroll: {
            int w = e.p.width >= 0 ? dp(e.p.width) : std::max(0, maxW);
            int h = e.p.height >= 0 ? dp(e.p.height) : std::max(0, maxH);
            if (h <= 0) h = dp(240);
            return LSize{ w, h };
        }
        case Kind::Tabs: {
            int barW = 0;
            for (const std::wstring& item : e.p.items)
                barW += measureTextWidth(item, makeFont(m_th.fontSize, false)) + dp(28);
            const int sel = elementSelected(e);
            if (!e.kids.empty() && sel >= 0 && sel < (int)e.kids.size()) {
                LSize page = measure(e.kids[(size_t)sel], maxW, maxH);
                return LSize{ std::max(barW, page.w), dp(40) + dp(10) + page.h };
            }
            return LSize{ barW, dp(40) };
        }
        case Kind::Spacer:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : 0,
                          e.p.height >= 0 ? dp(e.p.height) : 0 };
        case Kind::Text:
        case Kind::Title:
        case Kind::Caption: {
            const Gdiplus::Font* f = fontFor(e);
            int w = measureTextWidth(e.p.text, f);
            int h = lineHeight(f);
            if (e.p.wrap && innerMaxW > 0 && w > innerMaxW)
                h = measureWrappedHeight(e.p.text, f, innerMaxW);
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : w,
                          e.p.height >= 0 ? dp(e.p.height) : h };
        }
        case Kind::Button:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : buttonTextWidth(e),
                          e.p.height >= 0 ? dp(e.p.height) : dp(36) };
        case Kind::Link:
            return LSize{ e.p.width >= 0 ? dp(e.p.width)
                                         : measureTextWidth(e.p.text, fontFor(e)),
                          e.p.height >= 0 ? dp(e.p.height) : dp(24) };
        case Kind::Divider:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : std::max(0, maxW),
                          e.p.height >= 0 ? dp(e.p.height) : std::max(1, dp(1)) };
        case Kind::Input:
        case Kind::Password:
        case Kind::Search:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(240),
                          e.p.height >= 0 ? dp(e.p.height) : dp(36) };
        case Kind::TextArea:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(280),
                          e.p.height >= 0 ? dp(e.p.height) : dp(140) };
        case Kind::SpinBox:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(160),
                          e.p.height >= 0 ? dp(e.p.height) : dp(36) };
        case Kind::Checkbox:
        case Kind::Radio:
            return LSize{ e.p.width >= 0 ? dp(e.p.width)
                                         : measureTextWidth(e.p.text, fontFor(e)) + dp(30),
                          e.p.height >= 0 ? dp(e.p.height) : dp(26) };
        case Kind::Switch:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(46),
                          e.p.height >= 0 ? dp(e.p.height) : dp(26) };
        case Kind::Slider:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(220),
                          e.p.height >= 0 ? dp(e.p.height) : dp(24) };
        case Kind::Progress:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(220),
                          e.p.height >= 0 ? dp(e.p.height) : dp(8) };
        case Kind::Spinner:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(24),
                          e.p.height >= 0 ? dp(e.p.height) : dp(24) };
        case Kind::Combo:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(200),
                          e.p.height >= 0 ? dp(e.p.height) : dp(36) };
        case Kind::ListBox:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(220),
                          e.p.height >= 0 ? dp(e.p.height) : dp(160) };
        case Kind::Table:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(420),
                          e.p.height >= 0 ? dp(e.p.height) : dp(260) };
        case Kind::Tree:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(260),
                          e.p.height >= 0 ? dp(e.p.height) : dp(240) };
        case Kind::Calendar:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(280),
                          e.p.height >= 0 ? dp(e.p.height) : dp(280) };
        case Kind::Rating:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp((int)e.p.max * 24),
                          e.p.height >= 0 ? dp(e.p.height) : dp(26) };
        case Kind::Tag:
            return LSize{ e.p.width >= 0 ? dp(e.p.width)
                                         : measureTextWidth(e.p.text, fontFor(e)) +
                                               (e.p.onClick ? dp(36) : dp(28)),
                          e.p.height >= 0 ? dp(e.p.height) : dp(26) };
        case Kind::Badge:
            return LSize{ e.p.width >= 0 ? dp(e.p.width)
                                         : measureTextWidth(e.p.text, fontFor(e)) + dp(20),
                          e.p.height >= 0 ? dp(e.p.height) : dp(20) };
        case Kind::MenuBar: {
            int w = dp(12);
            for (const MenuEntry& m : e.p.menus)
                w += (int)measureTextWidth(m.title, fontFor(e)) + dp(24);
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : w,
                          e.p.height >= 0 ? dp(e.p.height) : dp(36) };
        }
        case Kind::StatusBar:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : std::max(0, maxW),
                          e.p.height >= 0 ? dp(e.p.height) : dp(28) };
        case Kind::Image:
            return LSize{ e.p.width >= 0 ? dp(e.p.width) : dp(320),
                          e.p.height >= 0 ? dp(e.p.height) : dp(140) };
        }
        return LSize{ dp(100), dp(30) };
    }

    // 从旧控件恢复瞬时状态（悬浮、光标、滚动、展开等）
    void restoreTransient(Control& c, const Control& o)
    {
        c.hover = o.hover;
        c.hoverT = o.hoverT;
        c.pressed = o.pressed;
        c.dragging = o.dragging;
        c.focused = o.focused;
        c.caret = o.caret;
        c.selA = o.selA;
        c.selB = o.selB;
        c.scrollX = o.scrollX;
        c.scrollY = o.scrollY;
        c.viewY = o.viewY;
        c.toggleT = o.toggleT;
        c.open = o.open;
        c.hoverItem = o.hoverItem;
        c.menuIdx = o.menuIdx;
        c.hoverRating = o.hoverRating;
        c.sortCol = o.sortCol;
        c.sortAsc = o.sortAsc;
        c.selectedRow = o.selectedRow;
        c.selPath = o.selPath;
        c.calY = o.calY;
        c.calM = o.calM;
        c.calD = o.calD;
        c.hasDate = o.hasDate;
        c.rating = o.rating;

        const bool hasTextBind  = c.binding && c.binding->kind == 1;
        const bool hasBoolBind  = c.binding && c.binding->kind == 2;
        const bool hasFloatBind = c.binding && c.binding->kind == 3;
        const bool hasIntBind   = c.binding && c.binding->kind == 4;

        if ((c.type == Ctl::Edit || c.type == Ctl::TextArea) && !hasTextBind) {
            c.text = o.text;
            int n = (int)c.text.size();
            c.caret = std::min(std::max(0, c.caret), n);
            c.selA = std::min(std::max(0, c.selA), n);
            c.selB = std::min(std::max(0, c.selB), n);
        }
        if ((c.type == Ctl::CheckBox || c.type == Ctl::Toggle) && !hasBoolBind)
            c.checked = o.checked;
        if (c.type == Ctl::RadioButton && !hasBoolBind && !o.onChanged)
            c.checked = o.checked;
        if ((c.type == Ctl::Slider || c.type == Ctl::SpinBox) && !hasFloatBind)
            c.progress = o.progress;
        if ((c.type == Ctl::ComboBox || c.type == Ctl::ListBox || c.type == Ctl::Tabs)
            && !hasIntBind)
            c.selected = o.selected;
        if (c.type == Ctl::Image && c.text == o.text && o.image)
            c.image = o.image;
        if (c.type == Ctl::Tree && c.nodes.size() == o.nodes.size())
            copyTreeExpansion(c.nodes, o.nodes);
    }

    void copyTreeExpansion(std::vector<TreeNode>& dst, const std::vector<TreeNode>& src)
    {
        for (size_t i = 0; i < dst.size() && i < src.size(); ++i) {
            if (dst[i].text != src[i].text) continue;
            dst[i].expanded = src[i].expanded;
            copyTreeExpansion(dst[i].children, src[i].children);
        }
    }

    Control makeControl(const Element& e, const LBox& box, const std::wstring& key,
                        const Control* old)
    {
        Control c;
        c.key = key;
        c.x = box.x; c.y = box.y; c.w = box.w; c.h = box.h;
        c.visible = e.p.visible;
        c.enabled = e.p.enabled;
        c.center = e.p.center;
        c.strong = e.p.strong;
        c.wrap = e.p.wrap;
        c.tooltip = e.p.tooltip;
        c.onClick = e.p.onClick;
        c.onChanged = e.p.onChanged;
        c.onEnter = e.p.onEnter;
        c.onRightClick = e.p.onRightClick;
        c.binding = e.p.binding;
        if (e.p.radius >= 0)    c.cornerRadius = e.p.radius;
        if (e.p.bg >= 0)        c.bgColor = e.p.bg;
        if (e.p.fg >= 0)        c.fgColor = e.p.fg;
        if (e.p.border >= 0)    c.borderColor = e.p.border;
        if (e.p.accent >= 0)    c.accentColor = e.p.accent;
        c.checked = e.p.checked;
        c.primary = e.p.primary;
        c.progress = e.p.value;
        c.min = e.p.min;
        c.max = e.p.max;
        c.step = e.p.step;
        c.decimals = e.p.decimals;
        c.items = e.p.items;
        c.headers = e.p.headers;
        c.rows = e.p.rows;
        c.nodes = e.p.nodes;
        c.menus = e.p.menus;
        c.selected = e.p.selected;
        c.groupId = e.p.group;
        c.text = e.p.text;
        c.placeholder = e.p.placeholder;
        c.fontSize = e.p.fontSize;
        c.ratingMax = std::max(1, (int)e.p.max);
        c.rating = std::min(std::max(0, (int)e.p.value), c.ratingMax);

        switch (e.tag) {
        case Kind::Text:
            c.type = Ctl::Label;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::Title:
            c.type = Ctl::Label;
            c.fontSize = m_th.titleSize;
            c.strong = true;
            c.bold = true;
            break;
        case Kind::Caption:
            c.type = Ctl::Label;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : 12;
            c.strong = false;
            break;
        case Kind::Button:
            c.type = Ctl::Button;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : 14;
            break;
        case Kind::Link:
            c.type = Ctl::Link;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::Divider:
            c.type = Ctl::Divider;
            break;
        case Kind::Input:
            c.type = Ctl::Edit;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            c.caret = (int)c.text.size();
            c.selA = c.selB = c.caret;
            break;
        case Kind::Password:
            c.type = Ctl::Edit;
            c.password = true;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            c.caret = (int)c.text.size();
            c.selA = c.selB = c.caret;
            break;
        case Kind::Search:
            c.type = Ctl::Edit;
            c.searchBox = true;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            c.caret = (int)c.text.size();
            c.selA = c.selB = c.caret;
            break;
        case Kind::TextArea:
            c.type = Ctl::TextArea;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            c.caret = (int)c.text.size();
            c.selA = c.selB = c.caret;
            break;
        case Kind::SpinBox:
            c.type = Ctl::SpinBox;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::Checkbox:
            c.type = Ctl::CheckBox;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            c.toggleT = c.checked ? 1.f : 0.f;
            break;
        case Kind::Radio:
            c.type = Ctl::RadioButton;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::Switch:
            c.type = Ctl::Toggle;
            c.toggleT = c.checked ? 1.f : 0.f;
            break;
        case Kind::Slider:
            c.type = Ctl::Slider;
            break;
        case Kind::Progress:
            c.type = Ctl::ProgressBar;
            break;
        case Kind::Spinner:
            c.type = Ctl::ProgressRing;
            c.spin = true;
            break;
        case Kind::Combo:
            c.type = Ctl::ComboBox;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::ListBox:
            c.type = Ctl::ListBox;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::Tabs:
            c.type = Ctl::Tabs;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::Table:
            c.type = Ctl::Table;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            c.selectedRow = -1;
            break;
        case Kind::Tree:
            c.type = Ctl::Tree;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::Calendar:
            c.type = Ctl::Calendar;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            if (e.p.value > 0.f) {
                int v = (int)e.p.value;
                c.calY = v / 10000;
                c.calM = (v / 100) % 100;
                c.calD = v % 100;
                c.hasDate = c.calY > 0 && c.calM >= 1 && c.calM <= 12;
            } else {
                SYSTEMTIME st;
                GetLocalTime(&st);
                c.calY = st.wYear; c.calM = st.wMonth; c.calD = st.wDay;
                c.hasDate = true;
            }
            break;
        case Kind::Rating:
            c.type = Ctl::Rating;
            break;
        case Kind::Tag:
            c.type = Ctl::Tag;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : 12;
            break;
        case Kind::Badge:
            c.type = Ctl::Badge;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : 11;
            break;
        case Kind::MenuBar:
            c.type = Ctl::MenuBar;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : m_th.fontSize;
            break;
        case Kind::StatusBar:
            c.type = Ctl::StatusBar;
            c.fontSize = e.p.fontSize > 0 ? e.p.fontSize : 12;
            break;
        case Kind::Image:
            c.type = Ctl::Image;
            c.text = e.p.path;
            if (old && old->type == Ctl::Image && old->text == c.text && old->image) {
                c.image = old->image;           // 同一路径复用解码后的图片
            } else if (!e.p.path.empty()) {
                c.image = std::make_shared<Gdiplus::Image>(e.p.path.c_str());
                if (c.image->GetLastStatus() != Gdiplus::Ok) c.image.reset();
            }
            break;
        case Kind::Card:
            c.type = Ctl::Card;
            c.cornerRadius = e.p.radius >= 0 ? e.p.radius : m_th.radiusLg;
            break;
        default:
            c.type = Ctl::Label;
            break;
        }

        if (old) restoreTransient(c, *old);

        // 绑定数据覆盖（受控组件）
        if (c.binding) {
            if (c.binding->kind == 1 && c.binding->getText)
                c.text = c.binding->getText();
            else if (c.binding->kind == 2 && c.binding->getBool)
                c.checked = c.binding->getBool();
            else if (c.binding->kind == 3 && c.binding->getFloat)
                c.progress = c.binding->getFloat();
            else if (c.binding->kind == 4 && c.binding->getInt) {
                if (c.type == Ctl::Table) c.selectedRow = c.binding->getInt();
                else if (c.type == Ctl::Rating) {
                    c.rating = std::min(std::max(0, c.binding->getInt()), c.ratingMax);
                }
                else c.selected = c.binding->getInt();
            }
        }
        if ((c.type == Ctl::Edit || c.type == Ctl::TextArea) && c.binding &&
            c.binding->kind == 1) {
            int n = (int)c.text.size();
            c.caret = std::min(c.caret, n);
            c.selA = std::min(c.selA, n);
            c.selB = std::min(c.selB, n);
        }
        if (c.type == Ctl::Table && c.sortCol >= 0) applyTableSort(c);
        return c;
    }

    void emitControl(const Element& e, const LBox& box, const std::wstring& key,
                     const RECT& clip, std::vector<Control>& out,
                     std::map<std::wstring, const Control*>& old)
    {
        const Control* prev = nullptr;
        std::map<std::wstring, const Control*>::iterator it = old.find(key);
        if (it != old.end()) prev = it->second;
        Control c = makeControl(e, box, key, prev);
        c.hasClip = true;
        c.clipRect = clip;
        out.push_back(std::move(c));
    }

    std::wstring childPath(const std::wstring& path, int index) const
    {
        wchar_t buf[32];
        swprintf(buf, 32, L".%d", index);
        return path + buf;
    }

    std::wstring elementKey(const Element& e, const std::wstring& path)
    {
        if (!e.p.key.empty()) return L"#" + e.p.key;
        return path;
    }

    // 垂直排列子元素（Column / Card / Scroll 共用）
    void layoutColumnKids(const Element& e, const LBox& inner, const RECT& clip,
                          const std::wstring& path, std::vector<Control>& out,
                          std::map<std::wstring, const Control*>& old, bool allowExpand,
                          int yOffset)
    {
        const int gap = dp(e.p.gap);
        const int n = (int)e.kids.size();
        std::vector<LSize> sizes((size_t)n);
        int fixedH = 0, visible = 0, expandCount = 0;
        for (int i = 0; i < n; ++i) {
            if (!e.kids[i].p.visible) continue;
            sizes[i] = measure(e.kids[i], inner.w, inner.h);
            ++visible;
            if (allowExpand && e.kids[i].p.expand && e.kids[i].p.height < 0)
                ++expandCount;
            else
                fixedH += sizes[i].h;
        }
        if (visible > 1) fixedH += gap * (visible - 1);
        int availExpand = allowExpand ? std::max(0, inner.h - fixedH) : 0;
        int share = expandCount > 0 ? availExpand / expandCount : 0;
        int rem = expandCount > 0 ? availExpand % expandCount : 0;

        int y = inner.y + yOffset;
        for (int i = 0; i < n; ++i) {
            const Element& k = e.kids[i];
            if (!k.p.visible) continue;
            LSize s = sizes[i];
            int w;
            if (k.p.width >= 0) {
                w = dp(k.p.width);
            } else if (k.p.expand || k.p.fillWidth || containerKind(k.tag)) {
                w = inner.w;
            } else if (intrinsicWidthInColumn(k.tag) ||
                       k.p.align == Align::Center || k.p.align == Align::End) {
                w = s.w;
            } else {
                w = inner.w;
            }
            int h = k.p.height >= 0 ? dp(k.p.height) : s.h;
            if (allowExpand && k.p.expand && k.p.height < 0) {
                h = share + (rem > 0 ? 1 : 0);
                if (rem > 0) --rem;
            }
            int x = inner.x;
            if (!k.p.expand && w < inner.w) {
                if (k.p.align == Align::Center) x = inner.x + (inner.w - w) / 2;
                else if (k.p.align == Align::End) x = inner.x + inner.w - w;
            }
            LBox b = { x, y, w, h };
            layoutElement(k, b, clip, childPath(path, i), out, old);
            y += h + gap;
        }
    }

    void layoutRowKids(const Element& e, const LBox& inner, const RECT& clip,
                       const std::wstring& path, std::vector<Control>& out,
                       std::map<std::wstring, const Control*>& old)
    {
        const int gap = dp(e.p.gap);
        const int n = (int)e.kids.size();
        std::vector<LSize> sizes((size_t)n);
        int fixedW = 0, visible = 0, expandCount = 0;
        for (int i = 0; i < n; ++i) {
            if (!e.kids[i].p.visible) continue;
            sizes[i] = measure(e.kids[i], inner.w, inner.h);
            ++visible;
            if ((e.kids[i].p.expand || e.kids[i].p.fillWidth) && e.kids[i].p.width < 0)
                ++expandCount;
            else
                fixedW += e.kids[i].p.width >= 0 ? dp(e.kids[i].p.width) : sizes[i].w;
        }
        if (visible > 1) fixedW += gap * (visible - 1);
        int availExpand = std::max(0, inner.w - fixedW);
        int share = expandCount > 0 ? availExpand / expandCount : 0;
        int rem = expandCount > 0 ? availExpand % expandCount : 0;

        int x = inner.x;
        for (int i = 0; i < n; ++i) {
            const Element& k = e.kids[i];
            if (!k.p.visible) continue;
            LSize s = sizes[i];
            int w = k.p.width >= 0 ? dp(k.p.width) : s.w;
            if ((k.p.expand || k.p.fillWidth) && k.p.width < 0) {
                w = share + (rem > 0 ? 1 : 0);
                if (rem > 0) --rem;
            }
            bool stretchH = (k.p.align == Align::Stretch && stretchHeightInRow(k.tag));
            int h = k.p.height >= 0 ? dp(k.p.height) : (stretchH ? inner.h : s.h);
            int y = inner.y;
            if (stretchH) y = inner.y;
            else if (k.p.align == Align::End) y = inner.y + inner.h - h;
            else y = inner.y + (inner.h - h) / 2;
            LBox b = { x, y, w, h };
            layoutElement(k, b, clip, childPath(path, i), out, old);
            x += w + gap;
        }
    }

    void layoutElement(const Element& e, const LBox& box, const RECT& parentClip,
                       const std::wstring& path, std::vector<Control>& out,
                       std::map<std::wstring, const Control*>& old)
    {
        if (!e.p.visible || box.w <= 0 || box.h <= 0) return;
        const int pad = dp(e.p.pad);
        const std::wstring key = elementKey(e, path);
        RECT clip = intersectRect(parentClip, RECT{ box.x, box.y, box.x + box.w, box.y + box.h });

        switch (e.tag) {
        case Kind::Column:
            layoutColumnKids(e, LBox{ box.x + pad, box.y + pad,
                                      std::max(0, box.w - pad * 2),
                                      std::max(0, box.h - pad * 2) },
                             clip, path, out, old, true, 0);
            return;
        case Kind::Card:
            emitControl(e, box, key, clip, out, old);
            layoutColumnKids(e, LBox{ box.x + pad, box.y + pad,
                                      std::max(0, box.w - pad * 2),
                                      std::max(0, box.h - pad * 2) },
                             clip, path, out, old, false, 0);
            return;
        case Kind::Row:
            layoutRowKids(e, LBox{ box.x + pad, box.y + pad,
                                   std::max(0, box.w - pad * 2),
                                   std::max(0, box.h - pad * 2) },
                          clip, path, out, old);
            return;
        case Kind::Stack:
            for (int i = 0; i < (int)e.kids.size(); ++i) {
                const Element& k = e.kids[i];
                if (!k.p.visible) continue;
                LSize s = measure(k, box.w - pad * 2, box.h - pad * 2);
                int w = k.p.width >= 0 ? dp(k.p.width) : (k.p.expand ? box.w - pad * 2 : s.w);
                int h = k.p.height >= 0 ? dp(k.p.height) : (k.p.expand ? box.h - pad * 2 : s.h);
                int x = box.x + pad + (box.w - pad * 2 - w) / 2;
                int y = box.y + pad + (box.h - pad * 2 - h) / 2;
                if (!k.p.expand) {
                    if (k.p.align == Align::Start) {
                        x = box.x + pad;
                        y = box.y + pad;
                    } else if (k.p.align == Align::End) {
                        x = box.x + box.w - pad - w;
                        y = box.y + box.h - pad - h;
                    }
                }
                layoutElement(k, LBox{ x, y, w, h }, clip, childPath(path, i), out, old);
            }
            return;
        case Kind::Scroll: {
            const int innerW = std::max(0, box.w - pad * 2);
            int contentH = pad * 2;
            int visible = 0;
            for (int i = 0; i < (int)e.kids.size(); ++i) {
                if (!e.kids[i].p.visible) continue;
                contentH += measure(e.kids[i], innerW, 1000000).h;
                ++visible;
            }
            if (visible > 1) contentH += dp(e.p.gap) * (visible - 1);
            int maxScroll = std::max(0, contentH - box.h);
            int& off = m_scroll[key];
            off = std::min(maxScroll, std::max(0, off));
            ScrollArea area;
            area.key = key;
            area.rect = RECT{ box.x, box.y, box.x + box.w, box.y + box.h };
            area.contentH = contentH;
            m_scrollAreas.push_back(area);
            layoutColumnKids(e, LBox{ box.x + pad, box.y + pad, innerW,
                                      std::max(0, contentH - pad * 2) },
                             clip, path, out, old, false, -off);
            return;
        }
        case Kind::Tabs: {
            int barH = dp(40);
            LBox bar = { box.x, box.y, box.w, std::min(barH, box.h) };
            emitControl(e, bar, key, clip, out, old);
            const int sel = elementSelected(e);
            if (!e.kids.empty() && sel >= 0 &&
                sel < (int)e.kids.size() && box.h > barH) {
                const Element& page = e.kids[(size_t)sel];
                if (!page.p.visible) return;
                LBox pb = { box.x, box.y + barH + dp(10), box.w,
                            std::max(0, box.h - barH - dp(10)) };
                RECT pageClip = intersectRect(clip, RECT{ pb.x, pb.y,
                                                         pb.x + pb.w, pb.y + pb.h });
                layoutElement(page, pb, pageClip, childPath(path, sel), out, old);
            }
            return;
        }
        default:
            emitControl(e, box, key, clip, out, old);
            return;
        }
    }

    void ensureBuilt()
    {
        if (!m_dirty) return;
        m_dirty = false;
        buildView();
    }

    void buildView()
    {
        if (!m_view || !m_hwnd) return;
        const double t0 = nowMs();
        Element root = m_view();

        std::map<std::wstring, const Control*> old;
        for (Control& c : m_ctrls)
            if (!c.key.empty()) old[c.key] = &c;

        std::vector<Control> out;
        m_scrollAreas.clear();

        RECT content = { 0, m_titleH, m_cx, m_cy };
        LBox box = { 0, m_titleH, m_cx, std::max(0, m_cy - m_titleH) };
        layoutElement(root, box, content, L"r", out, old);

        m_ctrls.swap(out);

        // 恢复焦点 / 捕获
        m_focus = -1;
        bool focusFound = false;
        if (!m_focusKey.empty()) {
            for (int i = 0; i < (int)m_ctrls.size(); ++i) {
                if (m_ctrls[i].key == m_focusKey) {
                    m_focus = i; m_ctrls[i].focused = true; focusFound = true; break;
                }
            }
        }
        if (!focusFound) m_focusKey.clear();
        m_capturedId = indexOfKey(m_captureKey);
        if (m_capturedId < 0) m_captureKey.clear();

        // 弹出菜单的归属控件
        if (m_popupOpen && m_popupOwner >= 0)
            m_popupOwner = indexOfKey(m_popupOwnerKey);

        // 重建后按当前鼠标位置刷新 hover
        int hit = hitTest(m_mx, m_my);
        for (int i = 0; i < (int)m_ctrls.size(); ++i)
            m_ctrls[i].hover = (i == hit && m_ctrls[i].enabled);

        const double ms = nowMs() - t0;
        m_stats.buildMs = ms;
        ++m_stats.builds;
        m_stats.avgBuildMs = (m_stats.builds == 1) ? ms : m_stats.avgBuildMs * 0.9 + ms * 0.1;
        if (ms > m_stats.maxBuildMs) m_stats.maxBuildMs = ms;
    }

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        App* self = nullptr;
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
            self = (App*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        } else {
            self = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        }
        if (self) return self->handleMessage(hwnd, msg, wp, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    // ---- 弹出菜单（菜单栏下拉 / 右键菜单）----

    // 计算弹出菜单尺寸并限界到窗口内（m_popup 需已填充）
    void openPopupAt(int x, int y)
    {
        // 测量最长文本与快捷键，确定宽度
        float maxT = 0.f, maxS = 0.f;
        bool anyShortcut = false;
        if (m_font) {
            for (const PopupItem& it : m_popup) {
                if (it.separator) continue;
                maxT = std::max(maxT, (float)cachedTextWidth(it.text, m_font));
                if (!it.shortcut.empty()) {
                    anyShortcut = true;
                    maxS = std::max(maxS, (float)cachedTextWidth(it.shortcut, m_font));
                }
            }
        }
        m_popupW = (int)(maxT + (anyShortcut ? maxS + dp(100) : dp(36)));
        m_popupW = std::max(m_popupW, dp(180));
        m_popupH = dp(8);
        for (const PopupItem& it : m_popup)
            m_popupH += it.separator ? dp(9) : popupItemH();
        x = std::max(dp(4), std::min(x, m_cx - m_popupW - dp(4)));
        y = std::max(dp(4), std::min(y, m_cy - m_popupH - dp(4)));
        m_popupX = x; m_popupY = y;
        m_popupHover = -1;
        redraw();
    }

    // 命中弹出菜单，返回菜单项索引（未命中返回 -1）
    int popupHit(int mx, int my) const
    {
        if (!m_popupOpen) return -1;
        if (mx < m_popupX || mx >= m_popupX + m_popupW ||
            my < m_popupY || my >= m_popupY + m_popupH) return -1;
        int y = m_popupY + dp(4);
        for (int i = 0; i < (int)m_popup.size(); ++i) {
            int ih = m_popup[i].separator ? dp(9) : popupItemH();
            if (my >= y && my < y + ih) return i;
            y += ih;
        }
        return -1;
    }

    void closePopup()
    {
        m_popupOpen = false;
        m_popupOwner = -1;
        m_popupMenuIdx = -1;
        m_popupHover = -1;
        m_popup.clear();
        redraw();
    }

    // 打开菜单栏某菜单（ownerId 为 MenuBar 控件 id，mi 为菜单项索引）
    void openMenu(Control& c, int ownerId, int mi)
    {
        m_popup.clear();
        for (const MenuItem& it : c.menus[mi].items) {
            PopupItem p;
            p.text = it.text;
            p.shortcut = it.shortcut;
            p.onClick = it.onClick;
            p.enabled = it.enabled;
            p.separator = it.separator;
            m_popup.push_back(std::move(p));
        }
        m_popupOpen = true;
        m_popupOwner = ownerId;
        m_popupOwnerKey = c.key;
        m_popupMenuIdx = mi;
        int bx = c.x + dp(6);
        for (int i = 0; i < mi; ++i)
            bx += (int)textWidth(c.menus[i].title, fontFor(c)) + dp(24);
        openPopupAt(bx, c.y + c.h + dp(2));
    }

    // 菜单栏：命中标题索引（布局与绘制保持一致：文本宽 + 24 padding）
    int menuBarHit(const Control& c, int mx) const
    {
        if (c.menus.empty()) return -1;
        int x = c.x + dp(6);
        for (int i = 0; i < (int)c.menus.size(); ++i) {
            int w = (int)textWidth(c.menus[i].title, fontFor(c)) + dp(24);
            if (mx >= x && mx < x + w) return i;
            x += w;
        }
        return -1;
    }

    // ---- 焦点 / 命中 ----

    // ComboBox 弹出层矩形（超出窗口底部时自动向上展开）
    RECT comboPopupRect(const Control& c) const
    {
        int n = (int)c.items.size();
        int vis = std::min(n, kComboMaxVis);
        int ph = vis * listRowH() + dp(8);
        int py = c.y + c.h + dp(2);
        if (py + ph > m_cy - dp(4)) py = std::max(dp(4), c.y - ph - dp(2));
        RECT r = { c.x, py, c.x + c.w, py + ph };
        return r;
    }

    // 关闭所有下拉框的弹出层
    void closeCombos()
    {
        for (Control& c : m_ctrls)
            if (c.type == Ctl::ComboBox) c.open = false;
    }

    int hitTest(int mx, int my) const
    {
        // 弹出菜单（菜单栏下拉 / 右键菜单）优先命中，返回 -2 特殊值
        if (m_popupOpen && popupHit(mx, my) >= 0) return -2;
        // 打开的 ComboBox 弹出层优先命中（可能覆盖其它控件）
        for (int i = (int)m_ctrls.size() - 1; i >= 0; --i) {
            const Control& c = m_ctrls[i];
            if (!c.visible || c.type != Ctl::ComboBox || !c.open) continue;
            RECT r = comboPopupRect(c);
            if (mx >= r.left && mx < r.right && my >= r.top && my < r.bottom)
                return i;
        }
        for (int i = (int)m_ctrls.size() - 1; i >= 0; --i) {
            const Control& c = m_ctrls[i];
            if (!c.visible) continue;
            if (c.hasClip && (mx < c.clipRect.left || mx >= c.clipRect.right ||
                              my < c.clipRect.top || my >= c.clipRect.bottom)) continue;
            // 无交互的纯展示控件不参与命中
            if (c.type == Ctl::Label || c.type == Ctl::Divider ||
                c.type == Ctl::Card || c.type == Ctl::ProgressBar ||
                c.type == Ctl::GroupBox || c.type == Ctl::Image ||
                c.type == Ctl::StatusBar || c.type == Ctl::Badge ||
                c.type == Ctl::ProgressRing) continue;
            if (mx >= c.x && mx < c.x + c.w && my >= c.y && my < c.y + c.h)
                return i;
        }
        return -1;
    }

    bool isFocusable(const Control& c) const
    {
        return c.type == Ctl::Edit || c.type == Ctl::Button ||
               c.type == Ctl::CheckBox || c.type == Ctl::Slider ||
               c.type == Ctl::TextArea || c.type == Ctl::RadioButton ||
               c.type == Ctl::Toggle || c.type == Ctl::SpinBox ||
               c.type == Ctl::Tabs;
    }

    // ---- 悬浮状态与动画 ----
    void updateHover(int mx, int my)
    {
        int hit = hitTest(mx, my);
        bool changed = false;
        std::vector<std::function<void()>> enterFns, leaveFns;
        for (size_t i = 0; i < m_ctrls.size(); ++i) {
            bool h = (int)i == hit && m_ctrls[i].enabled;
            if (m_ctrls[i].hover != h) {
                m_ctrls[i].hover = h;
                changed = true;
                if (h && m_ctrls[i].onHover)  enterFns.push_back(m_ctrls[i].onHover);
                if (!h && m_ctrls[i].onLeave) leaveFns.push_back(m_ctrls[i].onLeave);
            }
        }
        for (std::function<void()>& fn : leaveFns) if (fn) fn();
        for (std::function<void()>& fn : enterFns) if (fn) fn();
        if (changed) {
            ensureAnim();
            redraw();
            // 悬浮在可交互控件上时切换光标
            if (hit >= 0 && hit < (int)m_ctrls.size() && m_ctrls[hit].enabled) {
                if (m_ctrls[hit].type == Ctl::Edit ||
                    m_ctrls[hit].type == Ctl::TextArea)
                    SetCursor(LoadCursorA(nullptr, IDC_IBEAM));
                else if (m_ctrls[hit].type != Ctl::Label &&
                         m_ctrls[hit].type != Ctl::Divider &&
                         m_ctrls[hit].type != Ctl::Card &&
                         m_ctrls[hit].type != Ctl::ProgressBar &&
                         m_ctrls[hit].type != Ctl::GroupBox &&
                         m_ctrls[hit].type != Ctl::Image &&
                         m_ctrls[hit].type != Ctl::StatusBar &&
                         m_ctrls[hit].type != Ctl::Badge &&
                         m_ctrls[hit].type != Ctl::ProgressRing)
                    SetCursor(LoadCursorA(nullptr, IDC_HAND));
            } else {
                SetCursor(LoadCursorA(nullptr, IDC_ARROW));
            }
        }
    }

    // ---- 悬浮提示（Tooltip）----

    void showTooltip()
    {
        if (m_tipCtrlId < 0 || m_tipCtrlId >= (int)m_ctrls.size()) return;
        const Control& c = m_ctrls[m_tipCtrlId];
        if (c.tooltip.empty()) return;
        m_tipText = c.tooltip;
        m_tipShow = true;
        m_tipX = m_mx; m_tipY = m_my;
        redraw();
    }

    void hideTooltip()
    {
        if (m_tipTimerId) {
            killTimer(m_tipTimerId);
            m_tipTimerId = 0;
        }
        m_tipShow = false;
        m_tipText.clear();
        redraw();
    }

    void clearHover()
    {
        bool any = false;
        for (Control& c : m_ctrls)
            if (c.hover) { c.hover = false; any = true; }
        if (any) { ensureAnim(); redraw(); }
    }

    void ensureAnim()
    {
        if (m_animOn || !m_hwnd) return;
        m_animOn = true;
        SetTimer(m_hwnd, 0x51A1, 16, nullptr);
    }

    void tickAnim()
    {
        bool any = false;
        for (Control& c : m_ctrls) {
            float target = (c.hover || c.pressed) ? 1.f : 0.f;
            if (c.hoverT != target) {
                c.hoverT += (target - c.hoverT) * 0.35f;
                if (std::fabs(c.hoverT - target) < 0.02f) c.hoverT = target;
                else any = true;
            }
            // Toggle 开关滑块动画
            float ttarget = c.checked ? 1.f : 0.f;
            if (c.toggleT != ttarget) {
                c.toggleT += (ttarget - c.toggleT) * 0.3f;
                if (std::fabs(c.toggleT - ttarget) < 0.02f) c.toggleT = ttarget;
                else any = true;
            }
            // ProgressRing 不定量旋转
            if (c.type == Ctl::ProgressRing && c.spin) {
                c.rotate += 0.045f;
                any = true;
            }
        }
        // Toast 生命周期推进
        for (ToastRec& t : m_toasts) t.t += 16.f / (float)t.totalMs;
        m_toasts.erase(std::remove_if(m_toasts.begin(), m_toasts.end(),
            [](const ToastRec& t) { return t.t >= 1.f; }), m_toasts.end());
        if (!m_toasts.empty()) any = true;
        redraw();
        if (!any && m_animOn) {
            m_animOn = false;
            KillTimer(m_hwnd, 0x51A1);
        }
    }

    // ---- 消息分发 ----
    RECT chromeButtonRect(int index) const   // 0 最小化 / 1 最大化 / 2 关闭
    {
        int bw = dp(46);
        int rightEdge = m_cx - (2 - index) * bw;
        RECT r = { rightEdge - bw, 0, rightEdge, m_titleH };
        return r;
    }

    static bool inRect(const RECT& r, int x, int y)
    {
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }

    void toggleMaximize()
    {
        if (!m_hwnd) return;
        ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    }

    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) {
                if (IsZoomed(hwnd)) {
                    NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lp;
                    MONITORINFO mi = {};
                    mi.cbSize = sizeof(mi);
                    if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
                        p->rgrc[0] = mi.rcWork;
                }
                return 0;
            }
            break;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = dp(m_opt.minWidth);
            mmi->ptMinTrackSize.y = dp(m_opt.minHeight);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            const bool maxed = IsZoomed(hwnd) != FALSE;
            if (!maxed) {
                int b = dp(6);
                int c = dp(12);
                bool left   = pt.x >= 0 && pt.x < b;
                bool right  = pt.x < m_cx && pt.x >= m_cx - b;
                bool top    = pt.y >= 0 && pt.y < b;
                bool bottom = pt.y < m_cy && pt.y >= m_cy - b;
                bool cleft  = pt.x >= 0 && pt.x < c;
                bool cright = pt.x < m_cx && pt.x >= m_cx - c;
                bool ctop   = pt.y >= 0 && pt.y < c;
                bool cbottom= pt.y < m_cy && pt.y >= m_cy - c;
                if (m_opt.resizable) {
                    if (ctop && cleft) return HTTOPLEFT;
                    if (ctop && cright) return HTTOPRIGHT;
                    if (cbottom && cleft) return HTBOTTOMLEFT;
                    if (cbottom && cright) return HTBOTTOMRIGHT;
                    if (left) return HTLEFT;
                    if (right) return HTRIGHT;
                    if (top) return HTTOP;
                    if (bottom) return HTBOTTOM;
                }
            }
            if (pt.y >= 0 && pt.y < m_titleH && pt.x >= 0 && pt.x < m_cx) {
                if (inRect(chromeButtonRect(2), pt.x, pt.y)) return HTCLOSE;
                if (inRect(chromeButtonRect(1), pt.x, pt.y)) return HTMAXBUTTON;
                if (inRect(chromeButtonRect(0), pt.x, pt.y)) return HTMINBUTTON;
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_NCMOUSEMOVE: {
            int hover = (wp == HTMINBUTTON) ? 1 : (wp == HTMAXBUTTON) ? 2
                      : (wp == HTCLOSE) ? 3 : 0;
            if (hover != m_hoverChrome) {
                m_hoverChrome = hover;
                redraw();
            }
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            return 0;
        }
        case WM_NCMOUSELEAVE:
            if (m_hoverChrome) { m_hoverChrome = 0; redraw(); }
            return 0;
        case WM_NCLBUTTONDOWN:
            if (wp == HTCLOSE) { close(); return 0; }
            if (wp == HTMINBUTTON) { minimize(); return 0; }
            if (wp == HTMAXBUTTON) { toggleMaximize(); return 0; }
            break;
        case WM_ACTIVATE:
            m_active = (LOWORD(wp) != WA_INACTIVE);
            redraw();
            return 0;
        case WM_DPICHANGED: {
            m_dpi = (int)HIWORD(wp);
            if (m_dpi < 72) m_dpi = 96;
            createFonts();
            RECT* target = (RECT*)lp;
            if (target)
                SetWindowPos(hwnd, nullptr, target->left, target->top,
                             target->right - target->left, target->bottom - target->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            updateClientSize();
            m_titleH = dp(44);
            markDirty();
            redraw();
            return 0;
        }
        case WM_CREATE:
            updateClientSize();
            return 0;
        case WM_SIZE:
            updateClientSize();
            m_titleH = dp(44);
            markDirty();
            redraw();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            ensureBuilt();
            onPaint(hwnd);
            return 0;
        case WM_MOUSEMOVE: {
            ensureBuilt();
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            m_mx = mx; m_my = my;
            if (m_hoverChrome) { m_hoverChrome = 0; redraw(); }

            if (m_popupOpen) {
                int pi = popupHit(mx, my);
                if (pi != m_popupHover) { m_popupHover = pi; redraw(); }
            }

            for (int i = 0; i < (int)m_ctrls.size(); ++i) {
                Control& c = m_ctrls[i];
                if (c.type != Ctl::MenuBar) continue;
                int mi = (mx >= c.x && mx < c.x + c.w && my >= c.y && my < c.y + c.h)
                             ? menuBarHit(c, mx) : -1;
                if (c.menuIdx != mi) { c.menuIdx = mi; redraw(); }
                if (m_popupOpen && m_popupOwner == i && mi >= 0 && mi != m_popupMenuIdx)
                    openMenu(c, i, mi);
            }

            {
                int hit = hitTest(mx, my);
                int tipId = -1;
                if (hit >= 0 && hit < (int)m_ctrls.size() &&
                    m_ctrls[hit].enabled && !m_ctrls[hit].tooltip.empty() &&
                    !m_popupOpen)
                    tipId = hit;
                if (tipId != m_tipCtrlId) {
                    m_tipCtrlId = tipId;
                    hideTooltip();
                    if (tipId >= 0)
                        m_tipTimerId = timer(600, [this] { showTooltip(); }, false);
                } else if (tipId >= 0 && m_tipShow) {
                    m_tipX = mx; m_tipY = my;
                    redraw();
                }
            }

            for (Control& c : m_ctrls) {
                if (c.type != Ctl::Rating || !c.visible) continue;
                int nv = 0;
                if (mx >= c.x && mx < c.x + c.w && my >= c.y && my < c.y + c.h)
                    nv = std::min(c.ratingMax, std::max(0, (mx - c.x) / std::max(1, dp(24)) + 1));
                if (c.hoverRating != nv) { c.hoverRating = nv; redraw(); }
            }

            if (m_capturedId >= 0 && m_capturedId < (int)m_ctrls.size()) {
                Control& c = m_ctrls[m_capturedId];
                if (c.dragging && c.type == Ctl::Slider) {
                    float ratio = (float)(mx - c.x) / (float)std::max(1, c.w);
                    float old = c.progress;
                    c.progress = std::min(1.f, std::max(0.f, ratio));
                    if (c.progress != old) fireChanged(c);
                }
            }

            for (Control& c : m_ctrls) {
                if (c.type != Ctl::ListBox && c.type != Ctl::ComboBox &&
                    c.type != Ctl::Tabs) continue;
                int ni = -1;
                if (c.type == Ctl::ListBox) {
                    if (mx >= c.x && mx < c.x + c.w && my >= c.y && my < c.y + c.h)
                        ni = (my - c.y + c.scrollY) / listRowH();
                } else if (c.type == Ctl::ComboBox && c.open) {
                    RECT r = comboPopupRect(c);
                    if (mx >= r.left && mx < r.right && my >= r.top && my < r.bottom)
                        ni = (my - r.top) / listRowH();
                } else if (c.type == Ctl::Tabs) {
                    if (mx >= c.x && mx < c.x + c.w && my >= c.y && my < c.y + c.h)
                        ni = tabHitTest(c, mx);
                }
                if (ni < 0 || ni >= (int)c.items.size()) ni = -1;
                if (c.hoverItem != ni) { c.hoverItem = ni; redraw(); }
            }
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            updateHover(mx, my);
            return 0;
        }
        case WM_MOUSELEAVE:
            m_tipCtrlId = -1;
            hideTooltip();
            clearHover();
            return 0;
        case WM_LBUTTONDOWN: {
            ensureBuilt();
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            m_mx = mx; m_my = my;
            hideTooltip();
            int id = hitTest(mx, my);
            SetCapture(hwnd);
            setCapture(id);

            if (id == -2) {
                int pi = popupHit(mx, my);
                if (pi >= 0 && pi < (int)m_popup.size() &&
                    m_popup[pi].enabled && !m_popup[pi].separator &&
                    m_popup[pi].onClick)
                    m_popup[pi].onClick();
                closePopup();
                closeCombos();
                ReleaseCapture();
                setCapture(-1);
                return 0;
            }
            if (m_popupOpen) {
                bool onBar = (m_popupOwner >= 0 && id == m_popupOwner);
                if (!onBar) closePopup();
            }

            // 点击其它控件时收起已展开的下拉框（点击它自己则保留，用于切换）
            for (int ci = 0; ci < (int)m_ctrls.size(); ++ci) {
                Control& cc = m_ctrls[ci];
                if (cc.type == Ctl::ComboBox && cc.open && ci != id) cc.open = false;
            }

            if (id >= 0 && m_ctrls[id].type == Ctl::ComboBox && m_ctrls[id].open) {
                RECT r = comboPopupRect(m_ctrls[id]);
                if (my >= r.top && my < r.bottom) {
                    int idx = (my - r.top) / listRowH();
                    if (idx >= 0 && idx < (int)m_ctrls[id].items.size()) {
                        m_ctrls[id].selected = idx;
                        m_ctrls[id].open = false;
                        fireChanged(m_ctrls[id]);
                    } else {
                        m_ctrls[id].open = false;
                        redraw();
                    }
                    ReleaseCapture();
                    setCapture(-1);
                    return 0;
                }
            }

            if (id >= 0) {
                Control& c = m_ctrls[id];
                if (!c.enabled) {
                    ReleaseCapture();
                    setCapture(-1);
                    setFocus(-1);
                    return 0;
                }
                c.pressed = true;
                if (c.type == Ctl::ComboBox) {
                    bool wasOpen = c.open;
                    closeCombos();
                    c.open = !wasOpen;
                    if (c.open) setFocus(id); else setFocus(-1);
                } else if (c.type == Ctl::ListBox) {
                    int idx = (my - c.y + c.scrollY) / listRowH();
                    if (idx >= 0 && idx < (int)c.items.size()) {
                        c.selected = idx;
                        fireChanged(c);
                    }
                    setFocus(id);
                } else if (c.type == Ctl::MenuBar) {
                    int mi = menuBarHit(c, mx);
                    if (mi >= 0) {
                        if (m_popupOpen && m_popupOwner == id && m_popupMenuIdx == mi)
                            closePopup();
                        else
                            openMenu(c, id, mi);
                        setFocus(-1);
                    }
                } else if (c.type == Ctl::Table) {
                    handleTableClick(c, mx, my);
                    setFocus(-1);
                } else if (c.type == Ctl::Tree) {
                    handleTreeClick(c, mx, my);
                    setFocus(-1);
                } else if (c.type == Ctl::Calendar) {
                    handleCalendarClick(c, mx, my);
                    setFocus(-1);
                } else if (c.type == Ctl::Rating) {
                    int star = std::min(c.ratingMax, std::max(1, (mx - c.x) / std::max(1, dp(24)) + 1));
                    if (star != c.rating) {
                        c.rating = star;
                        fireChanged(c);
                    }
                    setFocus(-1);
                } else if (c.type == Ctl::Tag) {
                    if (c.onClick && mx >= c.x + c.w - dp(26)) c.onClick();
                } else {
                    if (c.type == Ctl::Slider) {
                        float ratio = (float)(mx - c.x) / (float)std::max(1, c.w);
                        float old = c.progress;
                        c.progress = std::min(1.f, std::max(0.f, ratio));
                        c.dragging = true;
                        if (c.progress != old) fireChanged(c);
                    } else if (c.type == Ctl::RadioButton) {
                        for (Control& o : m_ctrls)
                            if (o.type == Ctl::RadioButton && o.groupId == c.groupId)
                                o.checked = false;
                        c.checked = true;
                        fireChanged(c);
                    } else if (c.type == Ctl::Toggle) {
                        c.checked = !c.checked;
                        fireChanged(c);
                    } else if (c.type == Ctl::SpinBox) {
                        int ax = c.x + c.w - dp(24);
                        if (mx >= ax) {
                            if (my < c.y + c.h / 2)
                                c.progress = std::min(c.max, c.progress + c.step);
                            else
                                c.progress = std::max(c.min, c.progress - c.step);
                            fireChanged(c);
                        }
                    } else if (c.type == Ctl::Tabs) {
                        int idx = tabHitTest(c, mx);
                        if (idx >= 0 && idx != c.selected) {
                            c.selected = idx;
                            fireChanged(c);
                        }
                    } else if (c.type == Ctl::Edit) {
                        if (c.searchBox && mx >= c.x + c.w - dp(26) && !c.text.empty()) {
                            c.text.clear();
                            c.caret = 0;
                            c.selA = c.selB = 0;
                            updateEditScroll();
                            fireChanged(c);
                        } else {
                            placeEditCaret(c, mx);
                        }
                    } else if (c.type == Ctl::TextArea) {
                        placeCaretAt(c, mx, my);
                    }
                    setFocus(id);
                }
            } else {
                closeCombos();
                setFocus(-1);
            }
            ensureAnim();
            redraw();
            return 0;
        }
        case WM_LBUTTONUP: {
            ensureBuilt();
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            int upId = hitTest(mx, my);
            if (m_capturedId >= 0 && m_capturedId < (int)m_ctrls.size()) {
                Control& c = m_ctrls[m_capturedId];
                c.pressed = false;
                c.dragging = false;
                if (c.type == Ctl::CheckBox && upId == m_capturedId && c.enabled) {
                    c.checked = !c.checked;
                    fireChanged(c);
                } else if (upId == m_capturedId && c.enabled && c.onClick) {
                    c.onClick();
                    markDirty();
                }
                ensureAnim();
                redraw();
            }
            ReleaseCapture();
            setCapture(-1);
            return 0;
        }
        case WM_RBUTTONUP: {
            ensureBuilt();
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            m_mx = mx; m_my = my;
            int id = hitTest(mx, my);
            if (id >= 0 && id < (int)m_ctrls.size()) {
                Control& c = m_ctrls[id];
                if (c.enabled && c.onRightClick) {
                    hideTooltip();
                    c.onRightClick();
                }
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            m_capturedId = -1;
            m_captureKey.clear();
            return 0;
        case WM_MOUSEWHEEL: {
            ensureBuilt();
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            int step = (GET_WHEEL_DELTA_WPARAM(wp) > 0) ? -3 : 3;

            int hitId = hitTest(pt.x, pt.y);
            bool innerScroll = false;
            if (hitId >= 0 && hitId < (int)m_ctrls.size()) {
                const Control& hc = m_ctrls[hitId];
                innerScroll = (hc.type == Ctl::ListBox || hc.type == Ctl::Table ||
                               hc.type == Ctl::Tree || hc.type == Ctl::TextArea ||
                               (hc.type == Ctl::ComboBox && hc.open));
            }
            if (!innerScroll) {
                for (int i = (int)m_scrollAreas.size() - 1; i >= 0; --i) {
                    const ScrollArea& a = m_scrollAreas[(size_t)i];
                    if (!inRect(a.rect, pt.x, pt.y)) continue;
                    int& off = m_scroll[a.key];
                    int maxScroll = std::max(0, a.contentH - (int)(a.rect.bottom - a.rect.top));
                    int delta = step * dp(18);
                    int ns = std::min(maxScroll, std::max(0, off + delta));
                    if (ns != off) { off = ns; markDirty(); redraw(); }
                    return 0;
                }
            }

            for (Control& c : m_ctrls) {
                if (!c.visible) continue;
                bool over = pt.x >= c.x && pt.x < c.x + c.w &&
                            pt.y >= c.y && pt.y < c.y + c.h;
                bool overPopup = (c.type == Ctl::ComboBox && c.open);
                RECT pr = {};
                if (overPopup) {
                    pr = comboPopupRect(c);
                    overPopup = pt.x >= pr.left && pt.x < pr.right &&
                                pt.y >= pr.top && pt.y < pr.bottom;
                }
                if (c.type == Ctl::ListBox) {
                    if (!over) continue;
                    int maxScroll = std::max(0, (int)c.items.size() * listRowH() - c.h + 4);
                    int ns = std::min(maxScroll, std::max(0, c.scrollY + step));
                    if (ns != c.scrollY) { c.scrollY = ns; redraw(); }
                    break;
                } else if (c.type == Ctl::Table) {
                    if (!over) continue;
                    int maxScroll = std::max(0,
                        (int)c.rows.size() * listRowH() - (c.h - tableHeadH()));
                    int ns = std::min(maxScroll, std::max(0, c.scrollY + step));
                    if (ns != c.scrollY) { c.scrollY = ns; redraw(); }
                    break;
                } else if (c.type == Ctl::Tree) {
                    if (!over) continue;
                    int maxScroll = std::max(0, treeContentH(c) - c.h + 4);
                    int ns = std::min(maxScroll, std::max(0, c.scrollY + step));
                    if (ns != c.scrollY) { c.scrollY = ns; redraw(); }
                    break;
                } else if (c.type == Ctl::ComboBox) {
                    if (!overPopup) continue;
                    int maxScroll = std::max(0, (int)c.items.size() * listRowH() -
                                                (int)(pr.bottom - pr.top) + 4);
                    int ns = std::min(maxScroll, std::max(0, c.scrollY + step));
                    if (ns != c.scrollY) { c.scrollY = ns; redraw(); }
                    break;
                }
            }
            return 0;
        }
        case WM_KEYDOWN:
            ensureBuilt();
            onKeyDown(wp);
            return 0;
        case WM_CHAR:
        case WM_IME_CHAR:
            ensureBuilt();
            onChar((wchar_t)wp);
            return 0;
        case kPostMessage: {
            std::vector<std::function<void()>> jobs;
            if (m_postLockInit) EnterCriticalSection(&m_postLock);
            jobs.swap(m_postJobs);
            if (m_postLockInit) LeaveCriticalSection(&m_postLock);
            for (std::function<void()>& fn : jobs)
                if (fn) fn();
            return 0;
        }
        case WM_TIMER:
            if (wp == 0x51A1) {
                tickAnim();
                return 0;
            }
            {
                std::map<int, TimerRec>::iterator it = m_timers.find((int)wp);
                if (it != m_timers.end()) {
                    if (it->second.fn) it->second.fn();
                    if (it->second.repeat) return 0;
                    KillTimer(hwnd, (UINT_PTR)wp);
                    m_timers.erase(it);
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            releaseMeasure();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    // ---- 表格 / 树 / 日历 交互 ----

    // Table：点击列头排序（再次点击切换升降序），点击行选中
    void handleTableClick(Control& c, int mx, int my)
    {
        if (c.headers.empty()) return;
        if (my < c.y + tableHeadH()) {
            // 表头区：计算列索引（与绘制列宽逻辑保持一致）
            int ncol = (int)c.headers.size();
            int used = 0;
            if ((int)c.colWidths.size() == ncol)
                for (int i = 0; i < ncol; ++i) used += c.colWidths[i];
            int autoW = ncol > 0 ? (c.w - used) / ncol : 0;
            int x = c.x;
            for (int i = 0; i < ncol; ++i) {
                int cw = ((int)c.colWidths.size() == ncol)
                             ? c.colWidths[i] : (i < ncol - 1 ? autoW : c.w - (x - c.x) - autoW);
                if (i == ncol - 1 && (int)c.colWidths.size() != ncol)
                    cw = c.w - (x - c.x);
                if (mx >= x && mx < x + cw) {
                    sortTable(c, i);
                    return;
                }
                x += cw;
            }
            return;
        }
        int row = (my - c.y - tableHeadH() + c.scrollY) / listRowH();
        if (row >= 0 && row < (int)c.rows.size()) {
            c.selectedRow = row;
            fireChanged(c);
            redraw();
        }
    }

    void applyTableSort(Control& c)
    {
        if (c.sortCol < 0) return;
        const int col = c.sortCol;
        std::stable_sort(c.rows.begin(), c.rows.end(),
            [col](const std::vector<std::wstring>& a,
                  const std::vector<std::wstring>& b) {
                std::wstring av = col < (int)a.size() ? a[col] : L"";
                std::wstring bv = col < (int)b.size() ? b[col] : L"";
                return av < bv;
            });
        if (!c.sortAsc) std::reverse(c.rows.begin(), c.rows.end());
    }

    void sortTable(Control& c, int col)
    {
        if (c.sortCol == col) c.sortAsc = !c.sortAsc;
        else { c.sortCol = col; c.sortAsc = true; }
        applyTableSort(c);
        redraw();
    }

    // Tree：统计内容总高度
    int treeContentH(const Control& c) const
    {
        int cnt = 0;
        countTreeNodes(c.nodes, cnt);
        return cnt * listRowH() + dp(4);
    }

    void countTreeNodes(const std::vector<TreeNode>& nodes, int& cnt) const
    {
        for (const TreeNode& n : nodes) {
            ++cnt;
            if (n.expanded) countTreeNodes(n.children, cnt);
        }
    }

    // Tree：按可见顺序命中节点，返回根到节点的路径（y 为遍历游标，从可视起点开始）
    bool treeHitRec(const std::vector<TreeNode>& nodes, int mx, int my,
                    int& y, std::vector<int>& path) const
    {
        for (int i = 0; i < (int)nodes.size(); ++i) {
            const TreeNode& n = nodes[i];
            if (my >= y && my < y + listRowH()) { path.push_back(i); return true; }
            y += listRowH();
            if (n.expanded) {
                path.push_back(i);
                if (treeHitRec(n.children, mx, my, y, path)) return true;
                path.pop_back();
            }
        }
        return false;
    }

    TreeNode* treeNodeAtRec(std::vector<TreeNode>& nodes,
                            const std::vector<int>& path, size_t idx)
    {
        if (idx >= path.size()) return nullptr;
        if (idx + 1 == path.size()) return &nodes[path[idx]];
        return treeNodeAtRec(nodes[path[idx]].children, path, idx + 1);
    }

    // Tree：点击三角展开/折叠；点击文本选中
    void handleTreeClick(Control& c, int mx, int my)
    {
        std::vector<int> path;
        int y = c.y + dp(2) - c.scrollY;
        if (!treeHitRec(c.nodes, mx, my, y, path)) return;
        TreeNode* node = treeNodeAtRec(c.nodes, path, 0);
        if (!node) return;
        int indent = c.x + dp(8) + (int)path.size() * dp(16) - dp(16);
        bool onTwist = (mx >= indent - dp(4) && mx < indent + dp(16));
        if (onTwist && !node->children.empty()) {
            node->expanded = !node->expanded;
            fireChanged(c);
        } else {
            c.selPath = path;
            fireChanged(c);
        }
        redraw();
    }

    // ---- 日历工具 ----
    static bool isLeapYear(int y)
    {
        return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    }

    static int daysInMonth(int y, int m)
    {
        static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        return (m == 2 && isLeapYear(y)) ? 29 : d[m - 1];
    }

    // 星期几（0 = 周日，1 = 周一 ...）
    static int weekdayOf(int y, int m, int d)
    {
        tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon  = m - 1;
        t.tm_mday = d;
        t.tm_hour = 12;
        mktime(&t);
        return t.tm_wday;
    }

    // Calendar：点击 < > 切换月份、点标题回今天、点日期选中
    void handleCalendarClick(Control& c, int mx, int my)
    {
        int headH = dp(36), weekH = dp(22);
        int cellW = c.w / 7;
        int cellH = (c.h - headH - weekH) / 6;
        int gridY = c.y + headH + weekH;
        // 标题区按钮
        if (my < c.y + headH) {
            if (mx >= c.x && mx < c.x + dp(34)) {           // < 上月
                if (--c.calM < 1) { c.calM = 12; --c.calY; }
                redraw(); return;
            }
            if (mx >= c.x + c.w - dp(34) && mx < c.x + c.w) { // > 下月
                if (++c.calM > 12) { c.calM = 1; ++c.calY; }
                redraw(); return;
            }
            // 点标题回今天
            SYSTEMTIME st;
            GetLocalTime(&st);
            c.calY = st.wYear; c.calM = st.wMonth; c.calD = st.wDay;
            c.hasDate = true;
            fireChanged(c);
            redraw(); return;
        }
        if (my < gridY) return;  // 星期行
        int row = (my - gridY) / cellH;
        int col = (mx - c.x) / cellW;
        if (row < 0 || row >= 6 || col < 0 || col >= 7) return;
        int firstW = weekdayOf(c.calY, c.calM, 1);   // 0=周日
        int offset = (firstW + 6) % 7;               // 周一开头的偏移
        int day = row * 7 + col - offset + 1;
        int dm = daysInMonth(c.calY, c.calM);
        if (day >= 1 && day <= dm) {
            c.calD = day;
            c.hasDate = true;
            fireChanged(c);
            redraw();
        } else {
            // 点到相邻月日期：切换月份并选中
            int ty = c.calY, tm = c.calM;
            if (day < 1) { if (--tm < 1) { tm = 12; --ty; } day += daysInMonth(ty, tm); }
            else         { if (++tm > 12) { tm = 1; ++ty; } day -= dm; }
            c.calY = ty; c.calM = tm; c.calD = day;
            c.hasDate = true;
            fireChanged(c);
            redraw();
        }
    }

    // ---- 键盘：编辑框输入 ----
    void onKeyDown(WPARAM vk)
    {
        if (vk == VK_ESCAPE) {
            if (m_popupOpen) closePopup();
            for (Control& cc : m_ctrls)
                if (cc.type == Ctl::ComboBox) cc.open = false;
            setFocus(-1);
            redraw();
            return;
        }
        if (m_focus < 0 || m_focus >= (int)m_ctrls.size()) {
            // Tab 在可聚焦控件间切换
            if (vk == VK_TAB) {
                std::vector<int> order;
                for (int i = 0; i < (int)m_ctrls.size(); ++i)
                    if (isFocusable(m_ctrls[i]) && m_ctrls[i].visible)
                        order.push_back(i);
                if (!order.empty()) {
                    int cur = m_focus;
                    int idx = 0;
                    for (size_t k = 0; k < order.size(); ++k)
                        if (order[k] == cur) { idx = (int)k; break; }
                    idx = (idx + 1) % (int)order.size();
                    setFocus(order[idx]);
                }
            }
            return;
        }
        Control& c = m_ctrls[m_focus];

        // Tab 切换焦点（任何可聚焦控件）
        if (vk == VK_TAB) {
            std::vector<int> order;
            for (int i = 0; i < (int)m_ctrls.size(); ++i)
                if (isFocusable(m_ctrls[i]) && m_ctrls[i].visible)
                    order.push_back(i);
            if (!order.empty()) {
                int idx = 0;
                for (size_t k = 0; k < order.size(); ++k)
                    if (order[k] == m_focus) { idx = (int)k; break; }
                idx = (idx + 1) % (int)order.size();
                setFocus(order[idx]);
            }
            return;
        }

        if (c.type == Ctl::Button && (vk == VK_RETURN || vk == VK_SPACE)) {
            if (c.onClick) c.onClick();
            markDirty();
            return;
        }

        // 回车：Edit 触发 onEnter 并失焦；TextArea 插入换行
        if (vk == VK_RETURN) {
            if (c.type == Ctl::Edit) {
                if (c.onEnter) c.onEnter();
                setFocus(-1);
                return;
            }
            if (c.type == Ctl::TextArea) {
                int a = std::min(c.selA, c.selB), b = std::max(c.selA, c.selB);
                if (a != b) c.text.erase(a, b - a);
                c.text.insert(a, 1, L'\n');
                c.caret = a + 1;
                c.selA = c.selB = c.caret;
                updateTextAreaScroll(c);
                fireChanged(c);
                redraw();
                return;
            }
        }

        // SpinBox：聚焦时上下键调整数值
        if (c.type == Ctl::SpinBox) {
            if (vk == VK_UP) {
                c.progress = std::min(c.max, c.progress + c.step);
                fireChanged(c);
                redraw();
            } else if (vk == VK_DOWN) {
                c.progress = std::max(c.min, c.progress - c.step);
                fireChanged(c);
                redraw();
            }
            return;
        }

        if (c.type != Ctl::Edit && c.type != Ctl::TextArea) return;

        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        std::wstring& t = c.text;
        int len = (int)t.size();

        std::function<void()> eraseSelection = [&] {
            int a = std::min(c.selA, c.selB), b = std::max(c.selA, c.selB);
            if (a != b) {
                t.erase(a, b - a);
                c.caret = a;
                c.selA = c.selB = a;
            }
        };

        switch (vk) {
        case VK_BACK:
            eraseSelection();
            if (c.caret > 0) {
                t.erase(t.begin() + (c.caret - 1));
                --c.caret;
            }
            c.selA = c.selB = c.caret;
            break;
        case VK_DELETE:
            eraseSelection();
            if (c.caret < len) t.erase(t.begin() + c.caret);
            c.selA = c.selB = c.caret;
            break;
        case VK_LEFT:
            if (shift) c.selA = std::max(0, c.caret - 1);
            else { c.selA = c.selB = std::max(0, c.caret - 1); }
            c.caret = std::max(0, c.caret - 1);
            break;
        case VK_RIGHT:
            if (shift) c.selB = std::min(len, c.caret + 1);
            else { c.selA = c.selB = std::min(len, c.caret + 1); }
            c.caret = std::min(len, c.caret + 1);
            break;
        case VK_HOME:
            if (c.type == Ctl::TextArea) {
                size_t nl = c.text.rfind(L'\n', c.caret > 0 ? c.caret - 1 : 0);
                c.caret = (nl == std::wstring::npos) ? 0 : (int)nl + 1;
            } else {
                c.caret = 0;
            }
            c.selA = c.selB = c.caret;
            break;
        case VK_END:
            if (c.type == Ctl::TextArea) {
                size_t nl = c.text.find(L'\n', c.caret);
                c.caret = (nl == std::wstring::npos) ? len : (int)nl;
            } else {
                c.caret = len;
            }
            c.selA = c.selB = c.caret;
            break;
        case VK_UP:
            if (c.type == Ctl::TextArea) {
                moveCaretLine(c, -1);
                updateTextAreaScroll(c);
                fireChanged(c);
                redraw();
            }
            break;
        case VK_DOWN:
            if (c.type == Ctl::TextArea) {
                moveCaretLine(c, 1);
                updateTextAreaScroll(c);
                fireChanged(c);
                redraw();
            }
            break;
        default:
            if (ctrl && vk == 'A') {
                c.selA = 0; c.selB = len; c.caret = len;
            } else if (ctrl && vk == 'C') {
                int a = std::min(c.selA, c.selB), b = std::max(c.selA, c.selB);
                if (a != b) {
                    std::wstring sel = t.substr(a, b - a);
                    if (OpenClipboard(m_hwnd)) {
                        EmptyClipboard();
                        size_t bytes = (sel.size() + 1) * sizeof(wchar_t);
                        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
                        if (hg) {
                            memcpy(GlobalLock(hg), sel.c_str(), bytes);
                            GlobalUnlock(hg);
                            SetClipboardData(CF_UNICODETEXT, hg);
                        }
                        CloseClipboard();
                    }
                }
            } else if (ctrl && vk == 'V') {
                eraseSelection();
                if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(m_hwnd)) {
                    HANDLE h = GetClipboardData(CF_UNICODETEXT);
                    if (h) {
                        const wchar_t* p = (const wchar_t*)GlobalLock(h);
                        if (p) {
                            std::wstring paste(p);
                            GlobalUnlock(h);
                            // 过滤换行与控制字符
                            paste.erase(std::remove_if(paste.begin(), paste.end(),
                                [](wchar_t ch) { return ch == L'\r' || ch == L'\n' || ch < 32; }),
                                paste.end());
                            t.insert(c.caret, paste);
                            c.caret += (int)paste.size();
                            c.selA = c.selB = c.caret;
                        }
                    }
                    CloseClipboard();
                }
            } else if (ctrl && vk == 'X') {
                int a = std::min(c.selA, c.selB), b = std::max(c.selA, c.selB);
                if (a != b) {
                    std::wstring sel = t.substr(a, b - a);
                    if (OpenClipboard(m_hwnd)) {
                        EmptyClipboard();
                        size_t bytes = (sel.size() + 1) * sizeof(wchar_t);
                        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
                        if (hg) {
                            memcpy(GlobalLock(hg), sel.c_str(), bytes);
                            GlobalUnlock(hg);
                            SetClipboardData(CF_UNICODETEXT, hg);
                        }
                        CloseClipboard();
                    }
                    t.erase(a, b - a);
                    c.caret = a;
                    c.selA = c.selB = a;
                }
            }
            break;
        }
        if (c.type == Ctl::Edit) updateEditScroll();
        else updateTextAreaScroll(c);
        fireChanged(c);
        redraw();
    }

    void onChar(wchar_t ch)
    {
        if (m_focus < 0 || m_focus >= (int)m_ctrls.size()) return;
        Control& c = m_ctrls[m_focus];
        if (c.type != Ctl::Edit && c.type != Ctl::TextArea) return;
        if (ch < 32) return;  // 控制字符已在 WM_KEYDOWN 处理

        // 删除选区后插入字符
        int a = std::min(c.selA, c.selB), b = std::max(c.selA, c.selB);
        if (a != b) c.text.erase(a, b - a);
        c.text.insert(a, 1, ch);
        c.caret = a + 1;
        c.selA = c.selB = c.caret;
        if (c.type == Ctl::Edit) updateEditScroll();
        else updateTextAreaScroll(c);
        fireChanged(c);
        redraw();
    }

    void placeEditCaret(Control& c, int mx)
    {
        const Gdiplus::Font* f = fontFor(c);
        const int textOff = c.searchBox ? dp(32) : dp(12);
        std::wstring disp = dispText(c);
        float rel = (float)(mx - (c.x + textOff) + c.scrollX);
        int pos = 0;
        float acc = 0.f;
        HDC hdc = GetDC(m_hwnd);
        if (hdc && f) {
            Gdiplus::Graphics g(hdc);
            Gdiplus::StringFormat fmt;
            Gdiplus::RectF rc;
            for (int i = 0; i < (int)disp.size(); ++i) {
                wchar_t ch[2] = { disp[i], 0 };
                g.MeasureString(ch, 1, f, Gdiplus::PointF(0, 0), &fmt, &rc);
                if (acc + rc.Width * 0.5f > rel) { pos = i; break; }
                acc += rc.Width;
                pos = i + 1;
            }
            float avail = (float)(c.w - dp(22) - (c.searchBox ? dp(32) : 0));
            if (acc > avail) c.scrollX = (int)(acc - avail);
            else if (acc - c.scrollX < 0.f) c.scrollX = std::max(0, (int)acc);
            ReleaseDC(m_hwnd, hdc);
        }
        c.caret = pos;
        c.selA = c.selB = pos;
        redraw();
    }

    void updateEditScroll()
    {
        if (m_focus < 0 || m_focus >= (int)m_ctrls.size()) return;
        Control& c = m_ctrls[m_focus];
        if (c.type != Ctl::Edit) return;
        const Gdiplus::Font* f = fontFor(c);

        HDC hdc = GetDC(m_hwnd);
        if (!hdc) return;
        {
            // 测量光标前的显示文本宽度（密码框为掩码串），据此调整水平滚动
            Gdiplus::Graphics g(hdc);
            Gdiplus::StringFormat fmt;
            std::wstring disp = dispText(c);
            std::wstring before = disp.substr(0, std::min(c.caret, (int)disp.size()));
            Gdiplus::RectF rc;
            g.MeasureString(before.c_str(), (int)before.size(), f,
                            Gdiplus::PointF(0, 0), &fmt, &rc);
            float caretX = rc.Width;
            float avail = (float)(c.w - dp(22) - (c.searchBox ? dp(32) : 0));
            if (caretX > avail) c.scrollX = (int)(caretX - avail);
            else if (caretX - c.scrollX < 0) c.scrollX = std::max(0, (int)caretX);
        }
        ReleaseDC(m_hwnd, hdc);
    }

    // 测量文本宽度（Tabs 页签布局、TextArea 定位用）
    float textWidth(const std::wstring& s, const Gdiplus::Font* f) const
    {
        return (float)cachedTextWidth(s, f);
    }

    // Tabs：命中页签索引（布局与绘制保持一致：文本宽 + 24 padding）
    int tabHitTest(const Control& c, int mx) const
    {
        if (c.items.empty()) return -1;
        int x = c.x + dp(4);
        for (int i = 0; i < (int)c.items.size(); ++i) {
            int w = (int)textWidth(c.items[i], fontFor(c)) + dp(28);
            if (mx >= x && mx < x + w) return i;
            x += w;
        }
        return -1;
    }

    // TextArea：点击定位光标到最近字符
    void placeCaretAt(Control& c, int mx, int my)
    {
        const Gdiplus::Font* f = fontFor(c);
        int lh = lineHeight(f);
        int n = (int)c.text.size();
        int row = std::max(0, (my - c.y + c.viewY) / lh);
        // 定位到第 row 行的起始
        int lineStart = 0, rowIdx = 0;
        while (rowIdx < row && lineStart < n) {
            size_t nl = c.text.find(L'\n', lineStart);
            if (nl == std::wstring::npos) { lineStart = n; break; }
            lineStart = (int)nl + 1;
            ++rowIdx;
        }
        if (lineStart > n) lineStart = n;
        size_t nl = c.text.find(L'\n', lineStart);
        int lineEnd = (nl == std::wstring::npos) ? n : (int)nl;
        std::wstring line = c.text.substr(lineStart, lineEnd - lineStart);
        // 逐字符定位列
        float relX = (float)(mx - c.x - dp(10) + c.scrollX);
        int pos = (int)line.size();
        HDC hdc = GetDC(m_hwnd);
        if (hdc) {
            Gdiplus::Graphics g(hdc);
            Gdiplus::StringFormat fmt;
            Gdiplus::RectF rc;
            float acc = 0.f;
            for (int i = 0; i < (int)line.size(); ++i) {
                wchar_t ch[2] = { line[i], 0 };
                g.MeasureString(ch, 1, f, Gdiplus::PointF(0, 0), &fmt, &rc);
                if (acc + rc.Width * 0.5f > relX) { pos = i; break; }
                acc += rc.Width;
                pos = i + 1;
            }
            ReleaseDC(m_hwnd, hdc);
        }
        c.caret = lineStart + pos;
        c.selA = c.selB = c.caret;
        updateTextAreaScroll(c);
        redraw();
    }

    // TextArea：滚动到光标可见（垂直 + 水平）
    void updateTextAreaScroll(Control& c)
    {
        const Gdiplus::Font* f = fontFor(c);
        int lh = lineHeight(f);
        int lineStart = 0, row = 0;
        for (int i = 0; i < c.caret && i < (int)c.text.size(); ++i)
            if (c.text[i] == L'\n') { ++row; lineStart = i + 1; }
        int caretY = row * lh;
        int visH = c.h - dp(8);
        if (caretY < c.viewY) c.viewY = caretY;
        else if (caretY + lh > c.viewY + visH) c.viewY = caretY + lh - visH;
        if (c.viewY < 0) c.viewY = 0;
        // 水平：光标列可见
        if (m_hwnd && m_font) {
            HDC hdc = GetDC(m_hwnd);
            if (hdc) {
                Gdiplus::Graphics g(hdc);
                Gdiplus::StringFormat fmt;
                Gdiplus::RectF rc;
                std::wstring before = c.text.substr(lineStart, c.caret - lineStart);
                g.MeasureString(before.c_str(), (int)before.size(), f,
                                Gdiplus::PointF(0, 0), &fmt, &rc);
                float caretX = rc.Width;
                float avail = (float)(c.w - dp(34));
                if (caretX > avail) c.scrollX = (int)(caretX - avail);
                else if (caretX - c.scrollX < 0) c.scrollX = std::max(0, (int)caretX);
                ReleaseDC(m_hwnd, hdc);
            }
        }
    }

    // TextArea：上下键移动光标到相邻行（保持列位置）
    void moveCaretLine(Control& c, int dir)
    {
        int n = (int)c.text.size();
        int lineStart = 0, row = 0;
        for (int i = 0; i < c.caret && i < n; ++i)
            if (c.text[i] == L'\n') { ++row; lineStart = i + 1; }
        size_t nl = c.text.find(L'\n', lineStart);
        (void)nl;
        int col = c.caret - lineStart;
        int trow = row + dir;
        if (trow < 0) { c.caret = 0; c.selA = c.selB = 0; return; }
        int tstart = lineStart, r = row;
        while (r < trow) {
            size_t nnl = c.text.find(L'\n', tstart);
            if (nnl == std::wstring::npos) {
                c.caret = n; c.selA = c.selB = n; return;
            }
            tstart = (int)nnl + 1;
            ++r;
        }
        size_t tnl = c.text.find(L'\n', tstart);
        int tend = (tnl == std::wstring::npos) ? n : (int)tnl;
        c.caret = tstart + std::min(col, tend - tstart);
        c.selA = c.selB = c.caret;
    }

    // ---- 绘制 ----
    void onPaint(HWND hwnd)
    {
        const double t0 = nowMs();
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) { EndPaint(hwnd, &ps); return; }
        if (!ensureBuffer(w, h)) { EndPaint(hwnd, &ps); return; }

        memset(m_bits, 0, (size_t)w * (size_t)h * 4);

        Gdiplus::Graphics g(m_memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(m_glass ? Gdiplus::TextRenderingHintAntiAliasGridFit
                                       : Gdiplus::TextRenderingHintClearTypeGridFit);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        // 背景：玻璃模式只铺一层带透明度主题色，其余交给 DWM 背景材质
        if (m_glass) {
            Gdiplus::SolidBrush tint(argb((BYTE)m_glassAlpha, m_glassTint));
            g.FillRectangle(&tint, (REAL)0, (REAL)0, (REAL)w, (REAL)h);
        } else {
            drawBackground(g, w, h);
        }

        if (m_paintBg) m_paintBg(g, w, h);

        drawTitleBar(g, w, h);

        for (const Control& c : m_ctrls)
            if (c.visible) drawControl(g, c);

        if (m_paintFg) m_paintFg(g, w, h);

        // 滚动容器的滚动条
        drawScrollIndicators(g);

        // 第二遍：打开的 ComboBox 弹出层
        for (const Control& c : m_ctrls)
            if (c.visible && c.type == Ctl::ComboBox && c.open)
                drawComboPopup(g, c);
        if (m_popupOpen) drawPopup(g);
        if (m_tipShow && !m_tipText.empty()) drawTooltip(g);
        for (size_t i = 0; i < m_toasts.size(); ++i)
            drawToast(g, m_toasts[i], (int)i);

        // 最外圈 1px 发丝线：让 DWM 圆角裁切的边缘更干净、更完整
        if (!IsZoomed(hwnd)) {
            // Win11 使用系统 8px 圆角；Win10 回退到自己设置的主题圆角
            int r = dp(m_winBuild >= 22000 ? 8 : m_th.radiusLg);
            Gdiplus::GraphicsPath frame;
            buildRoundPath(frame, 0.5f, 0.5f, (REAL)w - 1.f, (REAL)h - 1.f, (REAL)r);
            Gdiplus::Pen pen(argb(m_glass ? 120 : 255, m_th.windowBorder), 1.f);
            g.DrawPath(&pen, &frame);
        }

        BitBlt(hdc, 0, 0, w, h, m_memDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);

        const double ms = nowMs() - t0;
        m_stats.paintMs = ms;
        ++m_stats.paints;
        m_stats.avgPaintMs = (m_stats.paints == 1) ? ms : m_stats.avgPaintMs * 0.9 + ms * 0.1;
        if (ms > m_stats.maxPaintMs) m_stats.maxPaintMs = ms;
    }

    void drawBackground(Gdiplus::Graphics& g, int w, int h)
    {
        Gdiplus::LinearGradientBrush br(
            Gdiplus::PointF(0, 0), Gdiplus::PointF(0, (REAL)h),
            rgb(m_th.bgTop), rgb(m_th.bgBottom));
        g.FillRectangle(&br, (REAL)0, (REAL)0, (REAL)w, (REAL)h);

        // 顶部一条极淡的强调色光晕，避免大面积纯色显得平板
        Gdiplus::LinearGradientBrush glow(
            Gdiplus::PointF(0, 0), Gdiplus::PointF(0, (REAL)(h / 2)),
            argb(26, m_th.accent), argb(0, m_th.accent));
        g.FillRectangle(&glow, (REAL)0, (REAL)0, (REAL)w, (REAL)(h / 2));
    }

    void drawTitleBar(Gdiplus::Graphics& g, int w, int h)
    {
        (void)h;
        const int th = m_titleH;
        if (th <= 0) return;

        if (!m_glass) {
            Gdiplus::SolidBrush bg(rgb(m_th.titleBarBg));
            g.FillRectangle(&bg, (REAL)0, (REAL)0, (REAL)w, (REAL)th);
            Gdiplus::LinearGradientBrush sheen(
                Gdiplus::PointF(0, 0), Gdiplus::PointF(0, (REAL)th),
                argb(10, 0xFFFFFF), argb(0, 0xFFFFFF));
            g.FillRectangle(&sheen, (REAL)0, (REAL)0, (REAL)w, (REAL)th);
        } else {
            Gdiplus::SolidBrush veil(argb(28, 0x000000));
            g.FillRectangle(&veil, (REAL)0, (REAL)0, (REAL)w, (REAL)th);
        }

        // 窗口控制按钮
        struct Glyph { int kind; RECT r; };
        Glyph glyphs[3] = {
            { 0, chromeButtonRect(0) },
            { 1, chromeButtonRect(1) },
            { 2, chromeButtonRect(2) }
        };
        for (int i = 0; i < 3; ++i) {
            RECT r = glyphs[i].r;
            bool hot = (m_hoverChrome == i + 1);
            DWORD glyphColor = m_active ? m_th.titleBarDim : 0x6A6F78;
            if (hot) {
                DWORD hb = (i == 2) ? m_th.danger : (m_th.dark ? 0x2C3038 : 0xE7EBF1);
                Gdiplus::SolidBrush hbr(rgb(hb));
                g.FillRectangle(&hbr, (REAL)r.left, (REAL)r.top,
                                (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
                if (i == 2) glyphColor = 0xFFFFFF;
                else glyphColor = m_active ? m_th.titleBarFg : m_th.titleBarDim;
            }
            float cx = (r.left + r.right) / 2.f;
            float cy = (r.top + r.bottom) / 2.f;
            float a = (float)dp(5);
            Gdiplus::Pen pen(rgb(glyphColor), 1.15f);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            if (i == 0) {
                g.DrawLine(&pen, cx - a, cy, cx + a, cy);
            } else if (i == 1) {
                if (!IsZoomed(m_hwnd)) {
                    g.DrawRectangle(&pen, cx - a + 0.5f, cy - a + 0.5f, a * 2.f - 1.f, a * 2.f - 1.f);
                } else {
                    g.DrawRectangle(&pen, cx - a, cy - a + 2.f, a * 2.f - 3.f, a * 2.f - 3.f);
                    g.DrawLine(&pen, cx - a + 3.f, cy - a, cx + a, cy - a);
                    g.DrawLine(&pen, cx + a, cy - a, cx + a, cy - a + 3.f);
                }
            } else {
                g.DrawLine(&pen, cx - a, cy - a, cx + a, cy + a);
                g.DrawLine(&pen, cx - a, cy + a, cx + a, cy - a);
            }
        }

        // 底部发丝线
        Gdiplus::Pen line(argb(m_glass ? 60 : 255, m_th.divider), 1.f);
        g.DrawLine(&line, 0.f, (float)th - 0.5f, (float)w, (float)th - 0.5f);

        // 应用标识
        const int logo = dp(22);
        const int lx = dp(16);
        const int ly = (th - logo) / 2;
        Gdiplus::GraphicsPath lp;
        buildRoundPath(lp, (REAL)lx, (REAL)ly, (REAL)logo, (REAL)logo, (REAL)dp(6));
        Gdiplus::LinearGradientBrush lb(
            Gdiplus::PointF((REAL)lx, (REAL)ly),
            Gdiplus::PointF((REAL)(lx + logo), (REAL)(ly + logo)),
            rgb(m_th.accentHover), rgb(m_th.accentPressed));
        g.FillPath(&lb, &lp);
        {
            const Gdiplus::Font* lf = makeFont(12, true);
            Gdiplus::SolidBrush tb(rgb(0xFFFFFF));
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(L"Y", -1, lf,
                         Gdiplus::RectF((REAL)lx, (REAL)ly, (REAL)logo, (REAL)logo),
                         &fmt, &tb);
        }

        // 标题
        if (!m_opt.title.empty()) {
            const Gdiplus::Font* tf = makeFont(13, false);
            DWORD tc = m_active ? m_th.titleBarFg : m_th.titleBarDim;
            int tx = lx + logo + dp(10);
            int tw = m_cx - tx - dp(46) * 3 - dp(12);
            Gdiplus::SolidBrush tb(rgb(tc));
            Gdiplus::StringFormat fmt;
            fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            g.DrawString(m_opt.title.c_str(), -1, tf,
                         Gdiplus::RectF((REAL)tx, (REAL)0, (REAL)std::max(0, tw), (REAL)th),
                         &fmt, &tb);
        }
    }

    void drawScrollIndicators(Gdiplus::Graphics& g)
    {
        for (const ScrollArea& a : m_scrollAreas) {
            int viewH = a.rect.bottom - a.rect.top;
            if (a.contentH <= viewH + 1 || viewH <= 0) continue;
            std::map<std::wstring, int>::iterator it = m_scroll.find(a.key);
            int off = (it == m_scroll.end()) ? 0 : it->second;
            int maxOff = std::max(1, a.contentH - viewH);
            int trackH = viewH - dp(8);
            int thumbH = std::max(dp(24), trackH * viewH / std::max(1, a.contentH));
            int y = a.rect.top + dp(4) + (trackH - thumbH) * off / maxOff;
            int x = a.rect.right - dp(7);
            Gdiplus::GraphicsPath p;
            buildRoundPath(p, (REAL)x, (REAL)y, (REAL)dp(3), (REAL)thumbH, (REAL)dp(1.5f));
            Gdiplus::SolidBrush br(argb(m_glass ? 150 : 200,
                m_th.dark ? 0x8A9099 : 0x8A9099));
            g.FillPath(&br, &p);
        }
    }

    void drawText(Gdiplus::Graphics& g, const std::wstring& s,
                  const Gdiplus::RectF& rc, const Gdiplus::Font* f,
                  DWORD color, bool centerX = false)
    {
        if (s.empty()) return;
        Gdiplus::SolidBrush br(rgb(color));
        Gdiplus::StringFormat fmt;
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetAlignment(centerX ? Gdiplus::StringAlignmentCenter
                                 : Gdiplus::StringAlignmentNear);
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        g.DrawString(s.c_str(), -1, f, rc, &fmt, &br);
    }

    // 多行文本（保留换行、自动折行）
    void drawTextBlock(Gdiplus::Graphics& g, const std::wstring& s,
                       const Gdiplus::RectF& rc, const Gdiplus::Font* f,
                       DWORD color, bool centerX = false)
    {
        if (s.empty()) return;
        Gdiplus::SolidBrush br(rgb(color));
        Gdiplus::StringFormat fmt;
        fmt.SetLineAlignment(Gdiplus::StringAlignmentNear);
        fmt.SetAlignment(centerX ? Gdiplus::StringAlignmentCenter
                                 : Gdiplus::StringAlignmentNear);
        fmt.SetTrimming(Gdiplus::StringTrimmingNone);
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);
        g.DrawString(s.c_str(), -1, f, rc, &fmt, &br);
    }
    // =====================================================================
    // 控件样式解析：每个控件可独立覆盖主题的圆角与颜色。
    // 规则：控件自定义值（>= 0）优先，否则回落到主题默认值。
    // =====================================================================
    int listRowH() const { return dp(kListItemH); }
    int tableHeadH() const { return dp(kTableHeadH); }
    int popupItemH() const { return dp(kPopupItemH); }

    int cRadius(const Control& c) const
    {
        int logical = c.cornerRadius >= 0 ? c.cornerRadius
                                          : (c.type == Ctl::Card ? m_th.radiusLg : m_th.radius);
        return dp(logical);
    }
    DWORD cBg(const Control& c, DWORD fb) const
    {
        return c.bgColor >= 0 ? (DWORD)c.bgColor : fb;
    }
    DWORD cFg(const Control& c, DWORD fb) const
    {
        return c.fgColor >= 0 ? (DWORD)c.fgColor : fb;
    }
    DWORD cBorder(const Control& c, DWORD fb) const
    {
        return c.borderColor >= 0 ? (DWORD)c.borderColor : fb;
    }
    DWORD cAccent(const Control& c, DWORD fb) const
    {
        return c.accentColor >= 0 ? (DWORD)c.accentColor : fb;
    }
    // 选中行 / 选中项的高亮底色：自定义 accent 时取其暗化版
    DWORD cSelHighlight(const Control& c) const
    {
        DWORD a = cAccent(c, m_th.accent);
        return lerpColor(a, m_th.dark ? 0x0F1218 : 0xFFFFFF, m_th.dark ? 0.76f : 0.86f);
    }

    // 自定义颜色时的状态自动派生：hover 向白色靠 13%，pressed 向黑色靠 13%，
    // 使"调过颜色的控件"依然保留悬浮 / 按下的反馈动画。
    static DWORD hoverize(DWORD color, float t)
    {
        if (t <= 0.f) return color;
        return lerpColor(color, 0xFFFFFF, 0.13f * t);
    }
    static DWORD pressize(DWORD color)
    {
        return lerpColor(color, 0x000000, 0.13f);
    }

    void drawControl(Gdiplus::Graphics& g, const Control& c)
    {
        Gdiplus::GraphicsState st = 0;
        if (c.hasClip) {
            int cw = c.clipRect.right - c.clipRect.left;
            int ch = c.clipRect.bottom - c.clipRect.top;
            if (cw <= 0 || ch <= 0) return;
            st = g.Save();
            g.SetClip(Gdiplus::RectF((REAL)c.clipRect.left, (REAL)c.clipRect.top,
                                     (REAL)cw, (REAL)ch), Gdiplus::CombineModeIntersect);
        }
        switch (c.type) {
        case Ctl::Card:       drawCard(g, c); break;
        case Ctl::Label:      drawLabel(g, c); break;
        case Ctl::Button:     drawButton(g, c); break;
        case Ctl::CheckBox:   drawCheckBox(g, c); break;
        case Ctl::Edit:       drawEdit(g, c); break;
        case Ctl::ProgressBar:drawProgress(g, c); break;
        case Ctl::Slider:     drawSlider(g, c); break;
        case Ctl::Divider:    drawDivider(g, c); break;
        case Ctl::ComboBox:   drawComboBox(g, c); break;
        case Ctl::ListBox:    drawListBox(g, c); break;
        case Ctl::TextArea:   drawTextArea(g, c); break;
        case Ctl::RadioButton:drawRadio(g, c); break;
        case Ctl::Toggle:     drawToggle(g, c); break;
        case Ctl::GroupBox:   drawGroupBox(g, c); break;
        case Ctl::Tabs:       drawTabs(g, c); break;
        case Ctl::SpinBox:    drawSpinBox(g, c); break;
        case Ctl::Image:      drawImage(g, c); break;
        case Ctl::Link:       drawLink(g, c); break;
        case Ctl::MenuBar:    drawMenuBar(g, c); break;
        case Ctl::Table:      drawTable(g, c); break;
        case Ctl::Tree:       drawTree(g, c); break;
        case Ctl::StatusBar:  drawStatusBar(g, c); break;
        case Ctl::ProgressRing: drawProgressRing(g, c); break;
        case Ctl::Badge:      drawBadge(g, c); break;
        case Ctl::Tag:        drawTag(g, c); break;
        case Ctl::Rating:     drawRating(g, c); break;
        case Ctl::Calendar:   drawCalendar(g, c); break;
        }
        if (c.hasClip) g.Restore(st);
    }

    // 输入框显示文本（密码框以 ● 掩码显示）
    std::wstring dispText(const Control& c) const
    {
        if (c.password && !c.text.empty())
            return std::wstring(c.text.size(), L'●');
        return c.text;
    }

    // ---- 绘制辅助 ----
    void fillRound(Gdiplus::Graphics& g, int x, int y, int w, int h, int r, DWORD color)
    {
        if (w <= 0 || h <= 0) return;
        Gdiplus::GraphicsPath p;
        buildRoundPath(p, (REAL)x + 0.5f, (REAL)y + 0.5f,
                       (REAL)w - 1.f, (REAL)h - 1.f, (REAL)r);
        Gdiplus::SolidBrush br(rgb(color));
        g.FillPath(&br, &p);
    }

    void fillRoundA(Gdiplus::Graphics& g, int x, int y, int w, int h, int r,
                    BYTE a, DWORD color)
    {
        if (w <= 0 || h <= 0) return;
        Gdiplus::GraphicsPath p;
        buildRoundPath(p, (REAL)x + 0.5f, (REAL)y + 0.5f,
                       (REAL)w - 1.f, (REAL)h - 1.f, (REAL)r);
        Gdiplus::SolidBrush br(argb(a, color));
        g.FillPath(&br, &p);
    }

    void strokeRound(Gdiplus::Graphics& g, int x, int y, int w, int h, int r,
                     DWORD color, float width = 1.f)
    {
        if (w <= 0 || h <= 0) return;
        Gdiplus::GraphicsPath p;
        buildRoundPath(p, (REAL)x + 0.5f, (REAL)y + 0.5f,
                       (REAL)w - 1.f, (REAL)h - 1.f, (REAL)r);
        Gdiplus::Pen pen(rgb(color), width);
        g.DrawPath(&pen, &p);
    }

    void drawSoftShadow(Gdiplus::Graphics& g, int x, int y, int w, int h, int r)
    {
        for (int i = dp(10); i >= dp(2); i -= dp(3)) {
            Gdiplus::GraphicsPath p;
            buildRoundPath(p, (REAL)(x - i), (REAL)(y - i + dp(3)),
                           (REAL)(w + i * 2), (REAL)(h + i * 2), (REAL)(r + i));
            Gdiplus::SolidBrush br(argb(7, 0x000000));
            g.FillPath(&br, &p);
        }
    }

    void drawCard(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        drawSoftShadow(g, c.x, c.y, c.w, c.h, r);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.cardBg));
        strokeRound(g, c.x, c.y, c.w, c.h, r, cBorder(c, m_th.cardBorder));
    }

    void drawLabel(Gdiplus::Graphics& g, const Control& c)
    {
        DWORD fg = cFg(c, c.strong ? m_th.fg : m_th.fgDim);
        Gdiplus::RectF rc((REAL)c.x, (REAL)c.y, (REAL)c.w, (REAL)c.h);
        if (c.wrap) drawTextBlock(g, c.text, rc, fontFor(c), fg, c.center);
        else drawText(g, c.text, rc, fontFor(c), fg, c.center);
    }

    void drawButton(Gdiplus::Graphics& g, const Control& c)
    {
        bool customBg = (c.bgColor >= 0);
        DWORD bg = 0, border = 0, fg = 0;
        if (c.primary && !customBg) {
            DWORD base = cAccent(c, m_th.accent);
            DWORD hover = (c.accentColor >= 0) ? hoverize(base, 1.f) : m_th.accentHover;
            bg = lerpColor(base, hover, c.hoverT);
            if (c.pressed) bg = (c.accentColor >= 0) ? pressize(bg) : m_th.accentPressed;
            border = lerpColor(bg, 0xFFFFFF, 0.10f);
            fg = 0xFFFFFF;
        } else if (customBg) {
            bg = hoverize((DWORD)c.bgColor, c.hoverT);
            if (c.pressed) bg = pressize(bg);
            border = cBorder(c, lerpColor(bg, 0xFFFFFF, 0.10f));
            fg = cFg(c, 0xFFFFFF);
        } else {
            bg = lerpColor(m_th.btnBg, m_th.btnHover, c.hoverT);
            if (c.pressed) bg = m_th.btnPressed;
            border = cBorder(c, m_th.btnBorder);
            fg = cFg(c, m_th.fg);
        }
        if (!c.enabled) {
            bg = lerpColor(bg, m_th.bgBottom, 0.55f);
            border = lerpColor(border, m_th.bgBottom, 0.55f);
            fg = lerpColor(fg, m_th.bgBottom, 0.45f);
        }

        int r = cRadius(c);
        fillRound(g, c.x, c.y, c.w, c.h, r, bg);
        strokeRound(g, c.x, c.y, c.w, c.h, r, border);
        if (c.primary) {
            // 主按钮底部 1px 暗边，增加一点“可按”的厚度感
            fillRoundA(g, c.x + dp(6), c.y + c.h - dp(2),
                       c.w - dp(12), dp(2), dp(1), 30, 0x000000);
        }
        if (c.focused) {
            Gdiplus::Pen fp(argb(110, cAccent(c, m_th.accent)), 1.5f);
            Gdiplus::GraphicsPath p;
            buildRoundPath(p, (REAL)(c.x - dp(2)) + 0.5f, (REAL)(c.y - dp(2)) + 0.5f,
                           (REAL)(c.w + dp(4)) - 1.f, (REAL)(c.h + dp(4)) - 1.f,
                           (REAL)(r + dp(2)));
            g.DrawPath(&fp, &p);
        }

        const Gdiplus::Font* f = fontFor(c);
        drawText(g, c.text,
                 Gdiplus::RectF((REAL)c.x, (REAL)c.y, (REAL)c.w, (REAL)c.h),
                 f, fg, true);
    }

    void drawCheckBox(Gdiplus::Graphics& g, const Control& c)
    {
        int box = dp(18);
        int bx = c.x + dp(1);
        int by = c.y + (c.h - box) / 2;
        int r = dp(5);
        DWORD fill = c.checked ? cAccent(c, m_th.accent)
                               : cBg(c, m_th.dark ? 0x1D1F25 : 0xFFFFFF);
        DWORD border = c.checked ? fill : cBorder(c, m_th.btnBorder);
        if (!c.checked && c.hover) border = lerpColor(border, m_th.accent, 0.55f);
        if (!c.enabled) { fill = lerpColor(fill, m_th.bgBottom, 0.5f); border = fill; }

        fillRound(g, bx, by, box, box, r, fill);
        strokeRound(g, bx, by, box, box, r, border, c.checked ? 1.f : 1.2f);
        if (c.checked) {
            Gdiplus::Pen tick(rgb(0xFFFFFF), 2.f);
            tick.SetLineJoin(Gdiplus::LineJoinRound);
            tick.SetStartCap(Gdiplus::LineCapRound);
            tick.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&tick, (REAL)(bx + dp(4)), (REAL)(by + box / 2),
                       (REAL)(bx + box / 2 - dp(1)), (REAL)(by + box - dp(5)));
            g.DrawLine(&tick, (REAL)(bx + box / 2), (REAL)(by + box - dp(5)),
                       (REAL)(bx + box - dp(4)), (REAL)(by + dp(4)));
        }
        drawText(g, c.text,
                 Gdiplus::RectF((REAL)(bx + box + dp(10)), (REAL)c.y,
                                (REAL)(c.w - box - dp(12)), (REAL)c.h),
                 fontFor(c), cFg(c, c.enabled ? m_th.fg : m_th.fgDim), false);
    }

    void drawEdit(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        DWORD bg = cBg(c, m_th.inputBg);
        DWORD border = cBorder(c, c.focused ? m_th.inputFocus : m_th.inputBorder);
        fillRound(g, c.x, c.y, c.w, c.h, r, bg);
        strokeRound(g, c.x, c.y, c.w, c.h, r, border, c.focused ? 1.6f : 1.f);
        if (c.focused) {
            Gdiplus::Pen glow(argb(70, cAccent(c, m_th.accent)), dp(3));
            Gdiplus::GraphicsPath gp;
            buildRoundPath(gp, (REAL)c.x + 0.5f, (REAL)c.y + 0.5f,
                           (REAL)c.w - 1.f, (REAL)c.h - 1.f, (REAL)(r + 1));
            g.DrawPath(&glow, &gp);
        }

        const int textOff = c.searchBox ? dp(32) : dp(12);
        if (c.searchBox) {
            Gdiplus::Pen lp(rgb(cFg(c, m_th.fgDim)), 1.6f);
            float lx = (float)(c.x + dp(16)), ly = (float)(c.y + c.h / 2);
            g.DrawEllipse(&lp, (REAL)(lx - dp(4)), (REAL)(ly - dp(4)), (REAL)dp(7), (REAL)dp(7));
            g.DrawLine(&lp, lx + dp(2), ly + dp(2), lx + dp(5.5f), ly + dp(5.5f));
        }

        int rightPad = c.searchBox ? dp(30) : dp(12);
        Gdiplus::RectF clip((REAL)(c.x + textOff), (REAL)(c.y + dp(3)),
                            (REAL)(c.w - textOff - rightPad), (REAL)(c.h - dp(6)));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(clip, Gdiplus::CombineModeIntersect);

        const Gdiplus::Font* f = fontFor(c);
        std::wstring disp = dispText(c);
        if (c.selA != c.selB) {
            int a = std::min(c.selA, c.selB), b = std::max(c.selA, c.selB);
            std::wstring pre = disp.substr(0, a);
            std::wstring sel = disp.substr(a, b - a);
            Gdiplus::RectF r1, r2;
            Gdiplus::StringFormat fmt;
            Gdiplus::PointF pt((REAL)(c.x + textOff - c.scrollX), (REAL)(c.y + dp(4)));
            g.MeasureString(pre.c_str(), (int)pre.size(), f, pt, &fmt, &r1);
            g.MeasureString(sel.c_str(), (int)sel.size(), f,
                            Gdiplus::PointF(pt.X + r1.Width, pt.Y), &fmt, &r2);
            Gdiplus::SolidBrush selBr(Gdiplus::Color(90,
                (BYTE)((cAccent(c, m_th.accent) >> 16) & 0xFF),
                (BYTE)((cAccent(c, m_th.accent) >> 8) & 0xFF),
                (BYTE)(cAccent(c, m_th.accent) & 0xFF)));
            g.FillRectangle(&selBr, r1.Width + pt.X, pt.Y, r2.Width,
                            (REAL)(c.h - dp(8)));
        }

        if (disp.empty() && !c.placeholder.empty()) {
            drawText(g, c.placeholder,
                     Gdiplus::RectF((REAL)(c.x + textOff - c.scrollX), (REAL)(c.y + dp(3)),
                                    (REAL)(c.w - textOff - rightPad + c.scrollX),
                                    (REAL)(c.h - dp(6))),
                     f, cFg(c, m_th.fgDim), false);
        } else {
            drawText(g, disp,
                     Gdiplus::RectF((REAL)(c.x + textOff - c.scrollX), (REAL)(c.y + dp(3)),
                                    (REAL)(c.w - textOff - rightPad + c.scrollX),
                                    (REAL)(c.h - dp(6))),
                     f, cFg(c, disp.empty() ? m_th.fgDim : m_th.fg), false);
        }

        if (c.focused) {
            std::wstring before = disp.substr(0, std::min(c.caret, (int)disp.size()));
            Gdiplus::RectF mrc;
            Gdiplus::StringFormat fmt;
            Gdiplus::PointF pt((REAL)(c.x + textOff - c.scrollX), (REAL)(c.y + dp(4)));
            g.MeasureString(before.c_str(), (int)before.size(), f, pt, &fmt, &mrc);
            Gdiplus::SolidBrush caretBr(rgb(cFg(c, m_th.fg)));
            g.FillRectangle(&caretBr, pt.X + mrc.Width, (REAL)(c.y + dp(7)),
                            std::max(1.f, (REAL)dp(1.5f)), (REAL)(c.h - dp(14)));
        }
        g.Restore(st);

        if (c.searchBox && !c.text.empty()) {
            float cx = (float)(c.x + c.w - dp(17));
            float cy = (float)(c.y + c.h / 2);
            Gdiplus::SolidBrush cb(argb(210, cFg(c, m_th.fgDim)));
            g.FillEllipse(&cb, (REAL)(cx - dp(6)), (REAL)(cy - dp(6)), (REAL)dp(12), (REAL)dp(12));
            Gdiplus::Pen xp(rgb(cBg(c, m_th.inputBg)), 1.4f);
            g.DrawLine(&xp, cx - dp(3), cy - dp(3), cx + dp(3), cy + dp(3));
            g.DrawLine(&xp, cx - dp(3), cy + dp(3), cx + dp(3), cy - dp(3));
        }
    }

    void drawProgress(Gdiplus::Graphics& g, const Control& c)
    {
        int r = std::min(cRadius(c), c.h / 2);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.trackBg));
        float pw = (float)c.w * std::min(1.f, std::max(0.f, c.progress));
        if (pw > 1.f) {
            DWORD base = cAccent(c, m_th.trackFill);
            Gdiplus::GraphicsPath fp;
            buildRoundPath(fp, (REAL)c.x + 0.5f, (REAL)c.y + 0.5f,
                           std::max(1.f, pw - 1.f), (REAL)c.h - 1.f, (REAL)r);
            Gdiplus::LinearGradientBrush fill(
                Gdiplus::PointF((REAL)c.x, 0), Gdiplus::PointF((REAL)(c.x + c.w), 0),
                rgb(base), rgb(lerpColor(base, m_th.dark ? 0x9FC2FF : 0xFFFFFF, 0.45f)));
            g.FillPath(&fill, &fp);
        }
    }

    void drawSlider(Gdiplus::Graphics& g, const Control& c)
    {
        int trackH = dp(4);
        int ty = c.y + c.h / 2 - trackH / 2;
        int knob = dp(16);
        int ky = c.y + (c.h - knob) / 2;
        DWORD accent = cAccent(c, m_th.trackFill);

        fillRound(g, c.x + dp(2), ty, c.w - dp(4), trackH,
                  trackH / 2, cBg(c, m_th.trackBg));
        float kx = (float)(c.x + knob / 2) + (float)(c.w - knob) * c.progress;
        if (c.progress > 0.01f) {
            int fw = std::max(trackH, (int)(kx - c.x - dp(2)));
            fillRound(g, c.x + dp(2), ty, fw, trackH, trackH / 2, accent);
        }

        // 滑块阴影 + 白色旋钮 + accent 描边
        Gdiplus::SolidBrush sh(argb(45, 0x000000));
        g.FillEllipse(&sh, kx - knob / 2.f, (REAL)ky + dp(1), (REAL)knob, (REAL)knob);
        DWORD knobCol = cFg(c, 0xFFFFFF);
        Gdiplus::SolidBrush kb(rgb(knobCol));
        g.FillEllipse(&kb, kx - knob / 2.f, (REAL)ky, (REAL)knob, (REAL)knob);
        Gdiplus::Pen ring(rgb(hoverize(accent, 0.2f + c.hoverT * 0.3f)), 1.4f);
        g.DrawEllipse(&ring, kx - knob / 2.f + 0.5f, (REAL)ky + 0.5f,
                      (REAL)knob - 1.f, (REAL)knob - 1.f);
    }

    void drawDivider(Gdiplus::Graphics& g, const Control& c)
    {
        Gdiplus::SolidBrush br(rgb(cBorder(c, m_th.divider)));
        g.FillRectangle(&br, (REAL)c.x, (REAL)(c.y + c.h / 2 - 1) + 0.5f,
                        (REAL)c.w, 1.f);
    }

    void drawComboBox(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        DWORD border = cBorder(c, c.focused ? m_th.inputFocus : m_th.inputBorder);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.inputBg));
        strokeRound(g, c.x, c.y, c.w, c.h, r, border, c.focused ? 1.6f : 1.f);

        std::wstring cur = (c.selected >= 0 && c.selected < (int)c.items.size())
                               ? c.items[c.selected] : L"";
        drawText(g, cur,
                 Gdiplus::RectF((REAL)(c.x + dp(12)), (REAL)c.y,
                                (REAL)(c.w - dp(40)), (REAL)c.h),
                 fontFor(c), cFg(c, cur.empty() ? m_th.fgDim : m_th.fg), false);

        float ax = (float)(c.x + c.w - dp(17));
        float ay = (float)(c.y + c.h / 2);
        Gdiplus::Pen arrow(rgb(cFg(c, m_th.fgDim)), 1.4f);
        arrow.SetStartCap(Gdiplus::LineCapRound);
        arrow.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&arrow, ax - dp(4), ay - dp(1.5f), ax, ay + dp(2.5f));
        g.DrawLine(&arrow, ax, ay + dp(2.5f), ax + dp(4), ay - dp(1.5f));
    }

    void drawComboPopup(Gdiplus::Graphics& g, const Control& c)
    {
        RECT r = comboPopupRect(c);
        int x = r.left, y = r.top, w = r.right - r.left, h = r.bottom - r.top;
        int rad = dp(10);
        drawSoftShadow(g, x, y, w, h, rad);
        fillRound(g, x, y, w, h, rad, cBg(c, m_th.cardBg));
        strokeRound(g, x, y, w, h, rad, cBorder(c, m_th.cardBorder));

        Gdiplus::RectF clip((REAL)(x + dp(3)), (REAL)(y + dp(3)),
                            (REAL)(w - dp(6)), (REAL)(h - dp(6)));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(clip, Gdiplus::CombineModeIntersect);

        const int rowH = listRowH();
        for (int i = 0; i < (int)c.items.size(); ++i) {
            int iy = y + dp(4) + i * rowH - c.scrollY;
            if (iy + rowH < y) continue;
            if (iy > y + h) break;
            if (i == c.hoverItem || i == c.selected) {
                fillRound(g, x + dp(5), iy + dp(1), w - dp(10), rowH - dp(2),
                          dp(6), i == c.selected ? cSelHighlight(c)
                                                 : cBg(c, m_th.btnHover));
            }
            drawText(g, c.items[i],
                     Gdiplus::RectF((REAL)(x + dp(14)), (REAL)iy,
                                    (REAL)(w - dp(22)), (REAL)rowH),
                     fontFor(c), cFg(c, i == c.selected ? m_th.fg : m_th.fg), false);
        }
        g.Restore(st);
    }

    void drawListBox(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.inputBg));
        strokeRound(g, c.x, c.y, c.w, c.h, r,
                    cBorder(c, c.focused ? m_th.inputFocus : m_th.inputBorder));

        Gdiplus::RectF clip((REAL)(c.x + dp(3)), (REAL)(c.y + dp(3)),
                            (REAL)(c.w - dp(6)), (REAL)(c.h - dp(6)));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(clip, Gdiplus::CombineModeIntersect);

        const int rowH = listRowH();
        for (int i = 0; i < (int)c.items.size(); ++i) {
            int iy = c.y + dp(4) + i * rowH - c.scrollY;
            if (iy + rowH < c.y) continue;
            if (iy > c.y + c.h) break;
            if (i == c.hoverItem || i == c.selected) {
                fillRound(g, c.x + dp(4), iy + dp(1), c.w - dp(8), rowH - dp(2),
                          dp(6), i == c.selected ? cSelHighlight(c)
                                                 : cBg(c, m_th.btnHover));
            }
            drawText(g, c.items[i],
                     Gdiplus::RectF((REAL)(c.x + dp(12)), (REAL)iy,
                                    (REAL)(c.w - dp(20)), (REAL)rowH),
                     fontFor(c), cFg(c, m_th.fg), false);
        }
        g.Restore(st);
        drawScrollbar(g, c.x, c.y, c.w, c.h, c.scrollY,
                      (int)c.items.size() * rowH + dp(4));
    }

    void drawTextArea(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.inputBg));
        strokeRound(g, c.x, c.y, c.w, c.h, r,
                    cBorder(c, c.focused ? m_th.inputFocus : m_th.inputBorder),
                    c.focused ? 1.6f : 1.f);

        const Gdiplus::Font* f = fontFor(c);
        int lh = lineHeight(f);
        Gdiplus::RectF clip((REAL)(c.x + dp(3)), (REAL)(c.y + dp(3)),
                            (REAL)(c.w - dp(6)), (REAL)(c.h - dp(6)));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(clip, Gdiplus::CombineModeIntersect);

        int y = c.y + dp(5) - c.viewY;
        if (c.text.empty() && !c.placeholder.empty()) {
            drawText(g, c.placeholder,
                     Gdiplus::RectF((REAL)(c.x + dp(10) - c.scrollX), (REAL)y,
                                    (REAL)(c.w - dp(20) + c.scrollX), (REAL)lh),
                     f, cFg(c, m_th.fgDim), false);
        } else {
            size_t start = 0;
            while (start <= c.text.size()) {
                size_t nl = c.text.find(L'\n', start);
                size_t end = (nl == std::wstring::npos) ? c.text.size() : nl;
                drawText(g, c.text.substr(start, end - start),
                         Gdiplus::RectF((REAL)(c.x + dp(10) - c.scrollX), (REAL)y,
                                        (REAL)(c.w - dp(20) + c.scrollX), (REAL)lh),
                         f, cFg(c, m_th.fg), false);
                y += lh;
                if (nl == std::wstring::npos) break;
                start = nl + 1;
            }
        }

        if (c.focused) {
            int lineStart = 0, row = 0;
            for (int i = 0; i < c.caret && i < (int)c.text.size(); ++i)
                if (c.text[i] == L'\n') { ++row; lineStart = i + 1; }
            std::wstring before = c.text.substr(lineStart, c.caret - lineStart);
            Gdiplus::RectF mrc;
            Gdiplus::StringFormat fmt;
            Gdiplus::PointF pt((REAL)(c.x + dp(10) - c.scrollX),
                               (REAL)(c.y + dp(5) - c.viewY + row * lh));
            g.MeasureString(before.c_str(), (int)before.size(), f, pt, &fmt, &mrc);
            Gdiplus::SolidBrush caretBr(rgb(cFg(c, m_th.fg)));
            g.FillRectangle(&caretBr, pt.X + mrc.Width, pt.Y,
                            std::max(1.f, (REAL)dp(1.5f)), (REAL)(lh - dp(6)));
        }
        g.Restore(st);

        int lines = 1;
        for (wchar_t ch : c.text) if (ch == L'\n') ++lines;
        drawScrollbar(g, c.x, c.y, c.w, c.h, c.viewY, lines * lh + dp(6));
    }

    void drawRadio(Gdiplus::Graphics& g, const Control& c)
    {
        int d = dp(18);
        int cx = c.x + dp(1) + d / 2, cy = c.y + c.h / 2;
        DWORD ring = c.checked ? cAccent(c, m_th.accent)
                    : (c.hover ? lerpColor(m_th.btnBorder, m_th.accent, 0.55f)
                               : cBorder(c, m_th.btnBorder));
        Gdiplus::SolidBrush ringBr(rgb(ring));
        g.FillEllipse(&ringBr, (REAL)(cx - d / 2), (REAL)(cy - d / 2), (REAL)d, (REAL)d);
        Gdiplus::SolidBrush inner(rgb(cBg(c, m_th.inputBg)));
        g.FillEllipse(&inner, (REAL)(cx - d / 2 + 2), (REAL)(cy - d / 2 + 2),
                      (REAL)(d - 4), (REAL)(d - 4));
        if (c.checked) {
            int dot = d - dp(10);
            Gdiplus::SolidBrush db(rgb(cAccent(c, m_th.accent)));
            g.FillEllipse(&db, (REAL)(cx - dot / 2), (REAL)(cy - dot / 2),
                          (REAL)dot, (REAL)dot);
        }
        drawText(g, c.text,
                 Gdiplus::RectF((REAL)(c.x + d + dp(10)), (REAL)c.y,
                                (REAL)(c.w - d - dp(12)), (REAL)c.h),
                 fontFor(c), cFg(c, c.enabled ? m_th.fg : m_th.fgDim), false);
    }

    void drawToggle(Gdiplus::Graphics& g, const Control& c)
    {
        int th = std::min(c.h, dp(26));
        int tw = std::min(c.w, dp(46));
        int tx = c.x;
        int ty = c.y + (c.h - th) / 2;
        int r = th / 2;

        Gdiplus::GraphicsPath tp;
        buildRoundPath(tp, (REAL)tx + 0.5f, (REAL)ty + 0.5f,
                       (REAL)tw - 1.f, (REAL)th - 1.f, (REAL)r);
        DWORD tcol = lerpColor(cBg(c, m_th.trackBg), cAccent(c, m_th.accent), c.toggleT);
        Gdiplus::SolidBrush tb(rgb(tcol));
        g.FillPath(&tb, &tp);
        Gdiplus::Pen edge(rgb(lerpColor(tcol, 0x000000, 0.10f)), 1.f);
        g.DrawPath(&edge, &tp);

        int knob = th - dp(6);
        float kx = (float)(tx + dp(3)) + (float)(tw - knob - dp(6)) * c.toggleT;
        float ky = (float)(ty + dp(3));
        Gdiplus::SolidBrush sh(argb(40, 0x000000));
        g.FillEllipse(&sh, kx, ky + dp(1), (REAL)knob, (REAL)knob);
        DWORD knobCol = cFg(c, 0xFFFFFF);
        Gdiplus::SolidBrush kb(rgb(knobCol));
        g.FillEllipse(&kb, kx, ky, (REAL)knob, (REAL)knob);
    }

    void drawGroupBox(Gdiplus::Graphics& g, const Control& c)
    {
        float tw = (float)textWidth(c.text, fontFor(c));
        fillRound(g, c.x + dp(12), c.y, (int)tw + dp(12), dp(18), 0,
                  cBg(c, m_th.cardBg));
        drawText(g, c.text,
                 Gdiplus::RectF((REAL)(c.x + dp(12)), (REAL)c.y,
                                tw + dp(12), (REAL)dp(18)),
                 fontFor(c), cFg(c, m_th.fgDim), false);
        strokeRound(g, c.x, c.y + dp(9), c.w, c.h - dp(9), cRadius(c),
                    cBorder(c, m_th.cardBorder));
    }

    int tabWidth(const Control& c, int i) const
    {
        return (int)textWidth(c.items[(size_t)i], fontFor(c)) + dp(28);
    }

    void drawTabs(Gdiplus::Graphics& g, const Control& c)
    {
        int x = c.x + dp(4);
        for (int i = 0; i < (int)c.items.size(); ++i) {
            int w = tabWidth(c, i);
            bool sel = (i == c.selected);
            bool hov = (i == c.hoverItem);
            if (hov && !sel) {
                fillRound(g, x, c.y + dp(3), w, c.h - dp(6), dp(7),
                          cBg(c, m_th.btnHover));
            }
            drawText(g, c.items[i],
                     Gdiplus::RectF((REAL)x, (REAL)c.y, (REAL)w, (REAL)(c.h - dp(2))),
                     fontFor(c),
                     sel ? cAccent(c, m_th.accent)
                         : cFg(c, c.enabled ? m_th.fg : m_th.fgDim),
                     true);
            if (sel) {
                int uw = w - dp(16);
                fillRound(g, x + dp(8), c.y + c.h - dp(4), uw, dp(3), dp(1.5f),
                          cAccent(c, m_th.accent));
            }
            x += w;
        }
    }

    void drawSpinBox(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.inputBg));
        strokeRound(g, c.x, c.y, c.w, c.h, r,
                    cBorder(c, c.focused ? m_th.inputFocus : m_th.inputBorder),
                    c.focused ? 1.6f : 1.f);

        std::wstring val;
        if (c.decimals > 0) {
            wchar_t buf[64];
            swprintf(buf, 64, L"%.*f", c.decimals, (double)c.progress);
            val = buf;
        } else {
            val = std::to_wstring((long long)c.progress);
        }
        drawText(g, val,
                 Gdiplus::RectF((REAL)(c.x + dp(12)), (REAL)c.y,
                                (REAL)(c.w - dp(40)), (REAL)c.h),
                 fontFor(c), cFg(c, m_th.fg), false);

        int ax = c.x + c.w - dp(26);
        Gdiplus::Pen sep(rgb(cBorder(c, m_th.inputBorder)), 1.f);
        g.DrawLine(&sep, (REAL)ax, (REAL)(c.y + dp(5)),
                   (REAL)ax, (REAL)(c.y + c.h - dp(5)));
        Gdiplus::SolidBrush arrow(rgb(cFg(c, m_th.fgDim)));
        float cx = (float)(ax + dp(13));
        float cy1 = (float)(c.y + c.h / 4);
        float cy2 = (float)(c.y + c.h * 3 / 4);
        Gdiplus::PointF tri1[3] = { { cx - dp(3.5f), cy1 + dp(2) },
                                    { cx + dp(3.5f), cy1 + dp(2) },
                                    { cx, cy1 - dp(2) } };
        g.FillPolygon(&arrow, tri1, 3);
        Gdiplus::PointF tri2[3] = { { cx - dp(3.5f), cy2 - dp(2) },
                                    { cx + dp(3.5f), cy2 - dp(2) },
                                    { cx, cy2 + dp(2) } };
        g.FillPolygon(&arrow, tri2, 3);
    }

    void drawImage(Gdiplus::Graphics& g, const Control& c)
    {
        if (!c.image) return;
        float iw = (float)c.image->GetWidth();
        float ih = (float)c.image->GetHeight();
        if (iw <= 0.f || ih <= 0.f) return;
        float scale = std::min((float)c.w / iw, (float)c.h / ih);
        float dw = iw * scale, dh = ih * scale;
        float dx = c.x + (c.w - dw) / 2.f;
        float dy = c.y + (c.h - dh) / 2.f;
        Gdiplus::GraphicsState st = g.Save();
        int r = cRadius(c);
        if (r > 0) {
            Gdiplus::GraphicsPath clip;
            buildRoundPath(clip, (REAL)c.x, (REAL)c.y, (REAL)c.w, (REAL)c.h, (REAL)r);
            g.SetClip(&clip, Gdiplus::CombineModeIntersect);
        }
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(c.image.get(), dx, dy, dw, dh);
        g.Restore(st);
    }

    void drawLink(Gdiplus::Graphics& g, const Control& c)
    {
        DWORD base = cAccent(c, m_th.accent);
        DWORD col = c.hover
            ? (c.accentColor >= 0 ? hoverize(base, 1.f)
                                  : lerpColor(base, 0xFFFFFF, 0.3f))
            : base;
        const Gdiplus::Font* f = fontFor(c);
        Gdiplus::SolidBrush br(rgb(col));
        Gdiplus::StringFormat fmt;
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(c.text.c_str(), -1, f,
                     Gdiplus::RectF((REAL)c.x, (REAL)c.y, (REAL)c.w, (REAL)c.h),
                     &fmt, &br);
        float tw = (float)textWidth(c.text, f);
        float ty = (float)(c.y + c.h / 2 + lineHeight(f) / 2 - dp(3));
        Gdiplus::Pen pen(argb(150, col), 1.f);
        g.DrawLine(&pen, (REAL)c.x, ty, (REAL)(c.x + tw), ty);
    }

    void drawMenuBar(Gdiplus::Graphics& g, const Control& c)
    {
        int x = c.x + dp(6);
        for (int i = 0; i < (int)c.menus.size(); ++i) {
            int w = (int)textWidth(c.menus[i].title, fontFor(c)) + dp(24);
            bool open = (m_popupOpen && m_popupMenuIdx == i);
            bool hov = (i == c.menuIdx) && !open;
            if (hov || open) {
                fillRound(g, x, c.y + dp(3), w, c.h - dp(6), dp(7),
                          cBg(c, m_th.btnHover));
            }
            drawText(g, c.menus[i].title,
                     Gdiplus::RectF((REAL)x, (REAL)c.y, (REAL)w, (REAL)c.h),
                     fontFor(c), open ? cAccent(c, m_th.accent)
                                      : cFg(c, m_th.fg), true);
            x += w;
        }
    }

    void drawTable(Gdiplus::Graphics& g, const Control& c)
    {
        int ncol = (int)c.headers.size();
        int r = cRadius(c);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.inputBg));

        std::vector<int> cw((size_t)ncol);
        if ((int)c.colWidths.size() == ncol) {
            int used = 0;
            for (int i = 0; i < ncol; ++i) used += c.colWidths[i];
            for (int i = 0; i < ncol; ++i) cw[i] = c.colWidths[i];
            if (ncol > 0) cw[ncol - 1] += std::max(0, c.w - used);
        } else if (ncol > 0) {
            int base = c.w / ncol;
            for (int i = 0; i < ncol; ++i) cw[i] = base;
            cw[ncol - 1] += c.w - base * ncol;
        }

        const int headH = tableHeadH();
        fillRoundA(g, c.x + 1, c.y + 1, c.w - 2, headH - 1, r,
                   255, cBg(c, m_th.dark ? 0x202228 : 0xF6F8FA));

        const Gdiplus::Font* f = fontFor(c);
        int x = c.x;
        for (int i = 0; i < ncol; ++i) {
            bool sorted = (i == c.sortCol);
            drawText(g, c.headers[i],
                     Gdiplus::RectF((REAL)(x + dp(12)), (REAL)(c.y + 1),
                                    (REAL)(cw[i] - dp(26)), (REAL)(headH - 2)),
                     f, sorted ? cAccent(c, m_th.accent) : cFg(c, m_th.fgDim), false);
            if (sorted) {
                Gdiplus::SolidBrush arr(rgb(cAccent(c, m_th.accent)));
                float ax = (float)(x + cw[i] - dp(16));
                float ay = (float)(c.y + headH / 2);
                if (c.sortAsc) {
                    Gdiplus::PointF tri[3] = { { ax - dp(4), ay + dp(2) },
                                               { ax + dp(4), ay + dp(2) },
                                               { ax, ay - dp(3) } };
                    g.FillPolygon(&arr, tri, 3);
                } else {
                    Gdiplus::PointF tri[3] = { { ax - dp(4), ay - dp(2) },
                                               { ax + dp(4), ay - dp(2) },
                                               { ax, ay + dp(3) } };
                    g.FillPolygon(&arr, tri, 3);
                }
            }
            x += cw[i];
        }

        Gdiplus::Pen hline(rgb(cBorder(c, m_th.divider)), 1.f);
        g.DrawLine(&hline, (REAL)(c.x + 1), (REAL)(c.y + headH - 1),
                   (REAL)(c.x + c.w - 1), (REAL)(c.y + headH - 1));

        const int rowH = listRowH();
        Gdiplus::RectF clip((REAL)(c.x + 1), (REAL)(c.y + headH),
                            (REAL)(c.w - dp(9)), (REAL)(c.h - headH - 1));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(clip, Gdiplus::CombineModeIntersect);
        for (int i = 0; i < (int)c.rows.size(); ++i) {
            int iy = c.y + headH + i * rowH - c.scrollY;
            if (iy + rowH < c.y + headH) continue;
            if (iy > c.y + c.h) break;
            bool sel = (i == c.selectedRow);
            if (sel) {
                fillRoundA(g, c.x + dp(3), iy + dp(1), c.w - dp(8), rowH - dp(2),
                           dp(5), 255, cSelHighlight(c));
            }
            int cx = c.x;
            for (int j = 0; j < ncol; ++j) {
                std::wstring cell = (j < (int)c.rows[i].size()) ? c.rows[i][j] : L"";
                drawText(g, cell,
                         Gdiplus::RectF((REAL)(cx + dp(12)), (REAL)iy,
                                        (REAL)(cw[j] - dp(20)), (REAL)rowH),
                         f, sel ? cFg(c, m_th.fg) : cFg(c, m_th.fg), false);
                cx += cw[j];
            }
        }
        for (int i = 0; i <= (int)c.rows.size(); ++i) {
            int iy = c.y + headH + i * rowH - c.scrollY;
            if (iy < c.y + headH) continue;
            if (iy > c.y + c.h) break;
            Gdiplus::Pen line(argb(120, cBorder(c, m_th.divider)), 1.f);
            g.DrawLine(&line, (REAL)(c.x + dp(8)), (REAL)iy,
                       (REAL)(c.x + c.w - dp(8)), (REAL)iy);
        }
        g.Restore(st);

        strokeRound(g, c.x, c.y, c.w, c.h, r, cBorder(c, m_th.inputBorder));
        drawScrollbar(g, c.x, c.y + headH, c.w, c.h - headH, c.scrollY,
                      (int)c.rows.size() * rowH + dp(4));
    }

    void drawTree(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.inputBg));

        Gdiplus::RectF clip((REAL)(c.x + 1), (REAL)(c.y + 1),
                            (REAL)(c.w - dp(9)), (REAL)(c.h - 2));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(clip, Gdiplus::CombineModeIntersect);
        int y = c.y + dp(2) - c.scrollY;
        std::vector<int> path;
        drawTreeRec(g, c, c.nodes, y, 0, path);
        g.Restore(st);

        strokeRound(g, c.x, c.y, c.w, c.h, r, cBorder(c, m_th.inputBorder));
        drawScrollbar(g, c.x, c.y, c.w, c.h, c.scrollY, treeContentH(c));
    }

    void drawTreeRec(Gdiplus::Graphics& g, const Control& c,
                     const std::vector<TreeNode>& nodes, int& y, int depth,
                     std::vector<int>& path)
    {
        const int rowH = listRowH();
        const Gdiplus::Font* f = fontFor(c);
        for (int i = 0; i < (int)nodes.size(); ++i) {
            const TreeNode& n = nodes[i];
            if (y > c.y + c.h) break;
            int indent = c.x + dp(8) + depth * dp(16);
            bool hasKid = !n.children.empty();
            bool sel = (path.size() + 1 == c.selPath.size());
            if (sel) {
                for (size_t k = 0; k < path.size(); ++k)
                    if (path[k] != c.selPath[k]) { sel = false; break; }
                if (sel && c.selPath.back() != i) sel = false;
            }
            if (sel) {
                fillRoundA(g, c.x + dp(4), y + dp(1), c.w - dp(9), rowH - dp(2),
                           dp(5), 255, cSelHighlight(c));
            }
            float tx = (float)(indent + dp(6));
            float tyc = (float)(y + rowH / 2);
            Gdiplus::Pen twist(rgb(sel ? 0xFFFFFF : cFg(c, m_th.fgDim)), 1.2f);
            twist.SetStartCap(Gdiplus::LineCapRound);
            twist.SetEndCap(Gdiplus::LineCapRound);
            if (hasKid) {
                if (n.expanded) {
                    g.DrawLine(&twist, tx - dp(3), tyc - dp(2), tx + dp(3), tyc - dp(2));
                    g.DrawLine(&twist, tx, tyc - dp(2), tx, tyc + dp(3));
                } else {
                    g.DrawLine(&twist, tx - dp(2), tyc - dp(3), tx + dp(3), tyc);
                    g.DrawLine(&twist, tx + dp(3), tyc, tx - dp(2), tyc + dp(3));
                }
            } else {
                Gdiplus::SolidBrush dot(rgb(cFg(c, m_th.fgDim)));
                g.FillEllipse(&dot, (REAL)(tx - dp(2)), (REAL)(tyc - dp(2)), (REAL)dp(4), (REAL)dp(4));
            }
            drawText(g, n.text,
                     Gdiplus::RectF((REAL)(indent + dp(18)), (REAL)y,
                                    (REAL)(c.w - (indent - c.x) - dp(28)), (REAL)rowH),
                     f, sel ? cFg(c, m_th.fg) : cFg(c, m_th.fg), false);
            y += rowH;
            if (n.expanded) {
                path.push_back(i);
                drawTreeRec(g, c, n.children, y, depth + 1, path);
                path.pop_back();
            }
        }
    }

    void drawStatusBar(Gdiplus::Graphics& g, const Control& c)
    {
        Gdiplus::SolidBrush line(rgb(cBorder(c, m_th.divider)));
        g.FillRectangle(&line, (REAL)c.x, (REAL)(c.y + 0.5f), (REAL)c.w, 1.f);
        int n = (int)c.items.size();
        if (n == 0) return;
        int w = c.w / n;
        for (int i = 0; i < n; ++i) {
            int x = c.x + i * w;
            drawText(g, c.items[i],
                     Gdiplus::RectF((REAL)(x + dp(14)), (REAL)(c.y + 1),
                                    (REAL)(w - dp(20)), (REAL)(c.h - 2)),
                     fontFor(c), cFg(c, m_th.fgDim), false);
            if (i < n - 1) {
                Gdiplus::Pen v(rgb(cBorder(c, m_th.divider)), 1.f);
                g.DrawLine(&v, (REAL)(x + w), (REAL)(c.y + c.h / 2 - dp(5)),
                           (REAL)(x + w), (REAL)(c.y + c.h / 2 + dp(5)));
            }
        }
    }

    void drawProgressRing(Gdiplus::Graphics& g, const Control& c)
    {
        int d = std::min(c.w, c.h);
        if (d < dp(12)) return;
        int pad = dp(3);
        int rr = d / 2 - pad;
        int cx = c.x + c.w / 2, cy = c.y + c.h / 2;
        float penW = std::max(2.f, (float)dp(4));
        Gdiplus::Pen track(rgb(cBg(c, m_th.trackBg)), penW);
        track.SetStartCap(Gdiplus::LineCapRound);
        track.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&track, (REAL)(cx - rr), (REAL)(cy - rr),
                  (REAL)(rr * 2), (REAL)(rr * 2), 0.f, 360.f);
        Gdiplus::Pen fill(rgb(cAccent(c, m_th.trackFill)), penW);
        fill.SetStartCap(Gdiplus::LineCapRound);
        fill.SetEndCap(Gdiplus::LineCapRound);
        if (c.spin) {
            float deg = c.rotate * 180.f / 3.14159265f;
            g.DrawArc(&fill, (REAL)(cx - rr), (REAL)(cy - rr),
                      (REAL)(rr * 2), (REAL)(rr * 2), deg - 90.f, 270.f);
        } else {
            g.DrawArc(&fill, (REAL)(cx - rr), (REAL)(cy - rr),
                      (REAL)(rr * 2), (REAL)(rr * 2), -90.f,
                      std::min(360.f, c.progress * 360.f));
            if (d >= dp(40)) {
                wchar_t buf[16];
                swprintf(buf, 16, L"%d%%", (int)(c.progress * 100.f));
                drawText(g, buf,
                         Gdiplus::RectF((REAL)(cx - rr), (REAL)(cy - rr),
                                        (REAL)(rr * 2), (REAL)(rr * 2)),
                         makeFont(11, true), cFg(c, m_th.fg), true);
            }
        }
    }

    void drawBadge(Gdiplus::Graphics& g, const Control& c)
    {
        if (c.text.empty()) return;
        int bh = std::min(c.h, dp(20));
        int by = c.y + (c.h - bh) / 2;
        float bw = std::max((float)c.w, textWidth(c.text, makeFont(11, false)) + dp(18.f));
        DWORD badgeBg = c.accentColor >= 0 ? (DWORD)c.accentColor : m_th.danger;
        fillRound(g, c.x, by, (int)bw, bh, bh / 2, badgeBg);
        drawText(g, c.text,
                 Gdiplus::RectF((REAL)c.x, (REAL)by, bw, (REAL)bh),
                 makeFont(11, true), cFg(c, 0xFFFFFF), true);
    }

    void drawTag(Gdiplus::Graphics& g, const Control& c)
    {
        int th = std::min(c.h, dp(26));
        int ty = c.y + (c.h - th) / 2;
        bool closable = (c.onClick != nullptr);
        float tw = textWidth(c.text, fontFor(c)) + (closable ? dp(32.f) : dp(22.f));
        float bw = std::max((float)c.w, tw);
        DWORD tagAccent = cAccent(c, m_th.accent);
        DWORD fill = lerpColor(tagAccent, m_th.dark ? 0x14161B : 0xFFFFFF, 0.78f);
        fillRound(g, c.x, ty, (int)bw, th, th / 2, fill);
        drawText(g, c.text,
                 Gdiplus::RectF((REAL)(c.x + dp(11)), (REAL)ty,
                                tw - (closable ? dp(36.f) : dp(16.f)), (REAL)th),
                 fontFor(c), tagAccent, false);
        if (closable) {
            float bx = (float)(c.x + bw - dp(16));
            float byc = (float)(ty + th / 2);
            Gdiplus::Pen xp(rgb(tagAccent), 1.4f);
            g.DrawLine(&xp, bx - dp(3.5f), byc - dp(3.5f), bx + dp(3.5f), byc + dp(3.5f));
            g.DrawLine(&xp, bx - dp(3.5f), byc + dp(3.5f), bx + dp(3.5f), byc - dp(3.5f));
        }
    }

    void starPoints(float cx, float cy, float r, Gdiplus::PointF* out) const
    {
        for (int k = 0; k < 10; ++k) {
            float ang = -3.14159265f / 2.f + k * 3.14159265f / 5.f;
            float rr = (k % 2 == 0) ? r : r * 0.45f;
            out[k].X = cx + std::cos(ang) * rr;
            out[k].Y = cy + std::sin(ang) * rr;
        }
    }

    void drawRating(Gdiplus::Graphics& g, const Control& c)
    {
        int starW = dp(24);
        int r = dp(8);
        for (int i = 0; i < c.ratingMax; ++i) {
            float sx = (float)(c.x + i * starW + starW / 2);
            float sy = (float)(c.y + c.h / 2);
            bool filled = (i < (c.hoverRating > 0 ? c.hoverRating : c.rating));
            Gdiplus::PointF pts[10];
            starPoints(sx, sy, (float)r, pts);
            if (filled) {
                Gdiplus::SolidBrush sb(rgb(cAccent(c, m_th.trackFill)));
                g.FillPolygon(&sb, pts, 10);
            } else {
                Gdiplus::SolidBrush eb(rgb(cBg(c, m_th.trackBg)));
                g.FillPolygon(&eb, pts, 10);
                Gdiplus::Pen op(rgb(cBorder(c, m_th.btnBorder)), 1.f);
                g.DrawPolygon(&op, pts, 10);
            }
        }
    }

    void drawCalendar(Gdiplus::Graphics& g, const Control& c)
    {
        int r = cRadius(c);
        fillRound(g, c.x, c.y, c.w, c.h, r, cBg(c, m_th.inputBg));
        strokeRound(g, c.x, c.y, c.w, c.h, r, cBorder(c, m_th.inputBorder));

        int headH = dp(36), weekH = dp(22);
        int cellW = c.w / 7;
        int cellH = (c.h - headH - weekH) / 6;
        int gridY = c.y + headH + weekH;

        wchar_t buf[64];
        swprintf(buf, 64, L"%d年%d月", c.calY, c.calM);
        drawText(g, buf,
                 Gdiplus::RectF((REAL)(c.x + dp(30)), (REAL)c.y,
                                (REAL)(c.w - dp(60)), (REAL)headH),
                 makeFont(13, true), cFg(c, m_th.fg), true);
        Gdiplus::SolidBrush arr(rgb(cFg(c, m_th.fgDim)));
        float ay = (float)(c.y + headH / 2);
        float ax = (float)(c.x + dp(18));
        Gdiplus::PointF lt[3] = { { ax + dp(2), ay - dp(4) }, { ax + dp(2), ay + dp(4) }, { ax - dp(4), ay } };
        g.FillPolygon(&arr, lt, 3);
        float bx = (float)(c.x + c.w - dp(18));
        Gdiplus::PointF rt[3] = { { bx - dp(2), ay - dp(4) }, { bx - dp(2), ay + dp(4) }, { bx + dp(4), ay } };
        g.FillPolygon(&arr, rt, 3);

        static const wchar_t* wd[] = { L"一", L"二", L"三", L"四", L"五", L"六", L"日" };
        Gdiplus::Pen sep(rgb(cBorder(c, m_th.divider)), 1.f);
        g.DrawLine(&sep, (REAL)(c.x + dp(4)), (REAL)(c.y + headH - 1),
                   (REAL)(c.x + c.w - dp(4)), (REAL)(c.y + headH - 1));
        const Gdiplus::Font* wf = makeFont(11, false);
        for (int i = 0; i < 7; ++i) {
            drawText(g, wd[i],
                     Gdiplus::RectF((REAL)(c.x + i * cellW), (REAL)(c.y + headH),
                                    (REAL)cellW, (REAL)weekH),
                     wf, (i >= 5) ? cAccent(c, m_th.accent) : cFg(c, m_th.fgDim), true);
        }
        g.DrawLine(&sep, (REAL)(c.x + dp(4)), (REAL)(gridY - 1),
                   (REAL)(c.x + c.w - dp(4)), (REAL)(gridY - 1));

        int firstW = weekdayOf(c.calY, c.calM, 1);
        int offset = (firstW + 6) % 7;
        int dm = daysInMonth(c.calY, c.calM);
        int prevM = (c.calM == 1) ? 12 : c.calM - 1;
        int prevY = (c.calM == 1) ? c.calY - 1 : c.calY;
        int prevDm = daysInMonth(prevY, prevM);
        SYSTEMTIME now;
        GetLocalTime(&now);
        for (int i = 0; i < 42; ++i) {
            int dx = c.x + (i % 7) * cellW;
            int dy = gridY + (i / 7) * cellH;
            int day = i - offset + 1;
            int yy = c.calY, mm = c.calM, dd = day;
            bool dim = false;
            if (day < 1) { dd = prevDm + day; mm = prevM; yy = prevY; dim = true; }
            else if (day > dm) {
                dd = day - dm;
                mm = (c.calM == 12) ? 1 : c.calM + 1;
                yy = (c.calM == 12) ? c.calY + 1 : c.calY;
                dim = true;
            }
            bool inMonth = (day >= 1 && day <= dm);
            bool today = (yy == now.wYear && mm == now.wMonth && dd == now.wDay);
            bool picked = c.hasDate && inMonth && (yy == c.calY && mm == c.calM && dd == c.calD);
            float rr = (float)std::min(cellW, cellH) / 2.f - dp(4.f);
            float ccx = (float)(dx + cellW / 2 - rr);
            float ccy = (float)(dy + cellH / 2 - rr);
            if (picked) {
                Gdiplus::SolidBrush pb(rgb(cAccent(c, m_th.accent)));
                g.FillEllipse(&pb, ccx, ccy, rr * 2.f, rr * 2.f);
            } else if (today) {
                Gdiplus::Pen tp(rgb(cAccent(c, m_th.accent)), 1.4f);
                g.DrawEllipse(&tp, ccx, ccy, rr * 2.f, rr * 2.f);
            }
            wchar_t dbuf[8];
            swprintf(dbuf, 8, L"%d", dd);
            drawText(g, dbuf,
                     Gdiplus::RectF((REAL)dx, (REAL)dy, (REAL)cellW, (REAL)cellH),
                     makeFont(12, false),
                     picked ? 0xFFFFFF : (dim ? cFg(c, m_th.fgDim) : cFg(c, m_th.fg)),
                     true);
        }
    }

    // 垂直滚动条
    void drawScrollbar(Gdiplus::Graphics& g, int x, int y, int w, int h,
                       int scrollY, int contentH) const
    {
        if (contentH <= h + 2) return;
        int bw = dp(5);
        int bx = x + w - bw - dp(3);
        float ratio = (float)h / contentH;
        int th = std::max(dp(20), (int)(h * ratio));
        float maxOff = (float)(contentH - h);
        int ty = y + (maxOff > 0 ? (int)((float)scrollY / maxOff * (h - th)) : 0);
        Gdiplus::SolidBrush thumb(rgb(lerpColor(m_th.btnBorder, m_th.btnHover, 0.4f)));
        Gdiplus::GraphicsPath tp;
        buildRoundPath(tp, (REAL)bx, (REAL)ty + 0.5f, (REAL)bw, (REAL)(th - 1), (REAL)(bw / 2));
        g.FillPath(&thumb, &tp);
    }

    // ---- 弹出菜单 / 悬浮提示 / 通知 ----
    void drawPopup(Gdiplus::Graphics& g)
    {
        int rad = dp(10);
        drawSoftShadow(g, m_popupX, m_popupY, m_popupW, m_popupH, rad);
        fillRound(g, m_popupX, m_popupY, m_popupW, m_popupH, rad, m_th.cardBg);
        strokeRound(g, m_popupX, m_popupY, m_popupW, m_popupH, rad, m_th.cardBorder);

        int y = m_popupY + dp(4);
        const int itemH = popupItemH();
        for (int i = 0; i < (int)m_popup.size(); ++i) {
            const PopupItem& it = m_popup[i];
            if (it.separator) {
                Gdiplus::SolidBrush sb(rgb(m_th.divider));
                g.FillRectangle(&sb, (REAL)(m_popupX + dp(12)), (REAL)(y + dp(4)),
                                (REAL)(m_popupW - dp(24)), 1.f);
                y += dp(9);
                continue;
            }
            if (i == m_popupHover && it.enabled) {
                fillRound(g, m_popupX + dp(5), y + dp(1), m_popupW - dp(10),
                          itemH - dp(2), dp(6), m_th.btnHover);
            }
            int textW = m_popupW - (it.shortcut.empty() ? dp(28) : dp(100));
            drawText(g, it.text,
                     Gdiplus::RectF((REAL)(m_popupX + dp(14)), (REAL)(y + 1),
                                    (REAL)textW, (REAL)(itemH - 2)),
                     m_font, it.enabled ? m_th.fg : m_th.fgDim, false);
            if (!it.shortcut.empty()) {
                drawText(g, it.shortcut,
                         Gdiplus::RectF((REAL)(m_popupX + m_popupW - dp(90)), (REAL)(y + 1),
                                        (REAL)(m_popupW - dp(24)), (REAL)(itemH - 2)),
                         m_font, m_th.fgDim, false);
            }
            y += itemH;
        }
    }

    void drawTooltip(Gdiplus::Graphics& g)
    {
        const Gdiplus::Font* f = makeFont(12, false);
        float tw = (float)textWidth(m_tipText, f);
        int w = (int)tw + dp(22);
        int h = dp(30);
        int x = m_tipX + dp(14), y = m_tipY + dp(18);
        if (x + w > m_cx - dp(4)) x = m_tipX - w - dp(14);
        if (y + h > m_cy - dp(4)) y = m_tipY - h - dp(18);
        x = std::max(dp(4), x);
        y = std::max(dp(4), y);
        fillRound(g, x, y, w, h, dp(7), m_th.dark ? 0x2A2D34 : 0xFFFFFF);
        strokeRound(g, x, y, w, h, dp(7), m_th.cardBorder);
        drawText(g, m_tipText,
                 Gdiplus::RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h),
                 f, m_th.dark ? 0xF2F3F5 : 0x1A1C20, true);
    }

    void drawToast(Gdiplus::Graphics& g, const ToastRec& t, int index)
    {
        float a = 1.f;
        if (t.t < 0.12f) a = t.t / 0.12f;
        else if (t.t > 0.85f) a = (1.f - t.t) / 0.15f;
        if (a <= 0.f) return;
        a = std::min(1.f, std::max(0.f, a));

        const Gdiplus::Font* f = makeFont(13, false);
        int w = (int)textWidth(t.text, f) + dp(60);
        int h = dp(44);
        int x = m_cx - w - dp(18);
        // 避开底部操作栏：通知向上浮动，避免遮挡主操作按钮
        int y = m_cy - h - dp(86) - index * (h + dp(10));
        if (y < 0) return;
        int slide = (int)((1.f - std::min(1.f, t.t / 0.15f)) * dp(50));
        x += slide;
        BYTE aa = (BYTE)(255 * a);

        Gdiplus::GraphicsPath p;
        buildRoundPath(p, (REAL)x + 0.5f, (REAL)y + 0.5f,
                       (REAL)(w - 1), (REAL)(h - 1), (REAL)dp(10));
        Gdiplus::SolidBrush bg(argb(aa, m_th.cardBg));
        g.FillPath(&bg, &p);
        Gdiplus::Pen pen(argb((BYTE)(200 * a), m_th.cardBorder), 1.f);
        g.DrawPath(&pen, &p);
        Gdiplus::SolidBrush bar(argb(aa, m_th.accent));
        Gdiplus::GraphicsPath bp;
        buildRoundPath(bp, (REAL)(x + dp(4)), (REAL)(y + dp(10)),
                       (REAL)dp(3), (REAL)(h - dp(20)), (REAL)dp(1.5f));
        g.FillPath(&bar, &bp);
        Gdiplus::SolidBrush tb(argb(aa, m_th.fg));
        Gdiplus::StringFormat fmt;
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(t.text.c_str(), -1, f,
                     Gdiplus::RectF((REAL)(x + dp(18)), (REAL)y,
                                    (REAL)(w - dp(28)), (REAL)h),
                     &fmt, &tb);
    }
};

//============================================================================
// 自绘模态消息框
//============================================================================
namespace msgimpl {

struct BoxState {
    std::wstring text;
    std::wstring title;
    Theme  th;
    float  scale = 1.f;
    int    winW = 430, winH = 300;
    int    textW = 370;
    int    lineH = 24;
    RECT   btn = { 0, 0, 0, 0 };
    bool   hoverBtn = false;
    bool   pressBtn = false;
    bool   hoverClose = false;
    int    result = 0;
    int    lines = 1;
};

inline BoxState& st()
{
    static BoxState s;
    return s;
}

// 贪心折行（逻辑像素）
inline void wrapText(const std::wstring& s, int maxW, const Gdiplus::Font& f,
                     std::vector<std::wstring>& out)
{
    HDC hdc = GetDC(nullptr);
    Gdiplus::Graphics g(hdc);
    Gdiplus::StringFormat fmt;
    Gdiplus::RectF rc;
    const int n = (int)s.size();
    int i = 0;
    while (i < n) {
        float w = 0.f;
        int lastSpace = -1;
        int j = i;
        for (; j < n; ++j) {
            float cw = 8.f;
            if (s[j] != L' ') {
                wchar_t tmp[2] = { s[j], 0 };
                g.MeasureString(tmp, 1, &f, Gdiplus::PointF(0, 0), &fmt, &rc);
                cw = rc.Width;
            }
            if (w + cw > (float)maxW) break;
            w += cw;
            if (s[j] == L' ') lastSpace = j;
        }
        if (j == i) j = i + 1;
        else if (lastSpace > i && j < n) j = lastSpace + 1;
        out.push_back(s.substr(i, j - i));
        i = j;
    }
    ReleaseDC(nullptr, hdc);
}

inline LRESULT CALLBACK msgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    BoxState& s = st();
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        int pw = (int)(s.winW * s.scale);
        int ph = (int)(s.winH * s.scale);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, pw, ph);
        HBITMAP old = (HBITMAP)SelectObject(mem, bmp);

        Gdiplus::Graphics g(mem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.ScaleTransform(s.scale, s.scale);

        int w = s.winW, h = s.winH;

        // 背景
        Gdiplus::LinearGradientBrush bgb(
            Gdiplus::PointF(0, 0), Gdiplus::PointF(0, (REAL)h),
            rgb(s.th.bgTop), rgb(s.th.bgBottom));
        g.FillRectangle(&bgb, (REAL)0, (REAL)0, (REAL)w, (REAL)h);

        // 卡片
        Gdiplus::GraphicsPath card;
        buildRoundPath(card, 10.f, 10.f, (REAL)(w - 20), (REAL)(h - 20), 12.f);
        Gdiplus::SolidBrush cardBr(rgb(s.th.cardBg));
        g.FillPath(&cardBr, &card);
        Gdiplus::Pen cardPen(rgb(s.th.cardBorder), 1.f);
        g.DrawPath(&cardPen, &card);

        // 信息图标（accent 圆角方块 + i）
        Gdiplus::GraphicsPath icoPath;
        buildRoundPath(icoPath, 30.f, 30.f, 44.f, 44.f, 12.f);
        Gdiplus::LinearGradientBrush icoBg(
            Gdiplus::PointF(30, 30), Gdiplus::PointF(74, 74),
            rgb(s.th.accentHover), rgb(s.th.accentPressed));
        g.FillPath(&icoBg, &icoPath);
        Gdiplus::Font icoFont(L"Segoe UI", 22.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush icoTxt(rgb(0xFFFFFF));
        Gdiplus::StringFormat icoFmt;
        icoFmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        icoFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"i", -1, &icoFont, Gdiplus::RectF(30.f, 30.f, 44.f, 44.f),
                     &icoFmt, &icoTxt);

        // 标题
        Gdiplus::Font titleFont(L"Segoe UI", 16.f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush titleBr(rgb(s.th.fg));
        Gdiplus::StringFormat tfmt;
        tfmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(s.title.c_str(), -1, &titleFont,
                     Gdiplus::RectF(90.f, 30.f, (REAL)(w - 130), 44.f), &tfmt, &titleBr);

        // 正文
        Gdiplus::Font bodyFont(L"Segoe UI", 14.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush bodyBr(rgb(s.th.fg));
        Gdiplus::StringFormat bfmt;
        bfmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        int y = 94;
        std::vector<std::wstring> raw;
        {
            std::wstring cur;
            for (size_t k = 0; k <= s.text.size(); ++k) {
                if (k == s.text.size() || s.text[k] == L'\n') {
                    raw.push_back(cur);
                    cur.clear();
                } else cur += s.text[k];
            }
        }
        std::vector<std::wstring> all;
        for (const std::wstring& line : raw) wrapText(line, s.textW, bodyFont, all);
        for (const std::wstring& line : all) {
            g.DrawString(line.c_str(), -1, &bodyFont,
                         Gdiplus::RectF(30.f, (REAL)y, (REAL)s.textW, (REAL)s.lineH),
                         &bfmt, &bodyBr);
            y += s.lineH;
        }

        // 主按钮
        Gdiplus::GraphicsPath bp;
        buildRoundPath(bp, (REAL)s.btn.left + 0.5f, (REAL)s.btn.top + 0.5f,
                       (REAL)(s.btn.right - s.btn.left - 1),
                       (REAL)(s.btn.bottom - s.btn.top - 1), 8.f);
        DWORD bc = s.pressBtn ? s.th.accentPressed
                 : (s.hoverBtn ? s.th.accentHover : s.th.accent);
        Gdiplus::SolidBrush bbr(rgb(bc));
        g.FillPath(&bbr, &bp);
        Gdiplus::Font btnFont(L"Segoe UI", 14.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush btxt(rgb(0xFFFFFF));
        Gdiplus::StringFormat bfmt2;
        bfmt2.SetAlignment(Gdiplus::StringAlignmentCenter);
        bfmt2.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"确定", -1, &btnFont,
                     Gdiplus::RectF((REAL)s.btn.left, (REAL)s.btn.top,
                                    (REAL)(s.btn.right - s.btn.left),
                                    (REAL)(s.btn.bottom - s.btn.top)),
                     &bfmt2, &btxt);

        // 与主窗口一致的最外圈发丝线
        {
            int rad = yrWindowsBuild() >= 22000 ? 8 : 12;
            Gdiplus::GraphicsPath frame;
            buildRoundPath(frame, 0.5f, 0.5f, (REAL)w - 1.f, (REAL)h - 1.f, (REAL)rad);
            Gdiplus::Pen pen(rgb(s.th.windowBorder), 1.f);
            g.DrawPath(&pen, &frame);
        }

        BitBlt(hdc, 0, 0, pw, ph, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { (int)(GET_X_LPARAM(lp) / s.scale),
                     (int)(GET_Y_LPARAM(lp) / s.scale) };
        bool h = PtInRect(&s.btn, pt) != FALSE;
        if (h != s.hoverBtn) {
            s.hoverBtn = h;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (s.hoverBtn) { s.hoverBtn = false; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt = { (int)(GET_X_LPARAM(lp) / s.scale),
                     (int)(GET_Y_LPARAM(lp) / s.scale) };
        if (PtInRect(&s.btn, pt)) {
            s.pressBtn = true;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt = { (int)(GET_X_LPARAM(lp) / s.scale),
                     (int)(GET_Y_LPARAM(lp) / s.scale) };
        if (s.pressBtn && PtInRect(&s.btn, pt)) {
            s.result = 1;
            DestroyWindow(hwnd);
        }
        s.pressBtn = false;
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_SPACE) {
            s.result = 1;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        s.result = 1;
        DestroyWindow(hwnd);
        return 0;
    case WM_TIMER:
        s.result = 1;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace msgimpl

inline int showMsgBox(HWND parent, const std::wstring& text,
                      const std::wstring& title, const Theme& th, bool glassEnabled)
{
    (void)glassEnabled;
    using namespace msgimpl;
    BoxState& s = st();
    s.text  = text;
    s.title = title;
    s.th    = th;
    s.result = 0;
    s.hoverBtn = s.pressBtn = false;

    int dpi = 96;
    if (HMODULE u = GetModuleHandleW(L"user32.dll")) {
        typedef UINT(WINAPI *Fn)(HWND);
        Fn fn = (Fn)(void*)GetProcAddress(u, "GetDpiForWindow");
        if (fn && parent) dpi = (int)fn(parent);
    }
    s.scale = (float)dpi / 96.f;
    s.winW = 430;
    s.textW = s.winW - 60;
    s.lineH = 24;

    Gdiplus::Font mf(L"Segoe UI", 14.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    std::vector<std::wstring> linesV;
    std::wstring flat;
    for (size_t k = 0; k <= text.size(); ++k) {
        if (k == text.size() || text[k] == L'\n') {
            std::vector<std::wstring> sub;
            wrapText(flat, s.textW, mf, sub);
            for (std::wstring& ln : sub) linesV.push_back(ln);
            flat.clear();
        } else flat += text[k];
    }
    s.lines = (int)linesV.size();
    if (s.lines < 1) s.lines = 1;

    s.winH = 94 + s.lines * s.lineH + 14 + 36 + 22;
    s.btn = { s.winW - 30 - 104, s.winH - 22 - 36,
              s.winW - 30, s.winH - 22 };

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc   = &msgProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
        wc.lpszClassName = L"YRGuiMsg";
        RegisterClassExW(&wc);
        reg = true;
    }

    int pw = (int)(s.winW * s.scale);
    int ph = (int)(s.winH * s.scale);
    HWND msg = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                               L"YRGuiMsg", title.c_str(), WS_POPUP,
                               0, 0, pw, ph,
                               parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!msg) return 0;

    // DWM 圆角 + 阴影
    if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll")) {
        typedef HRESULT(WINAPI *Fn)(HWND, DWORD, LPCVOID, DWORD);
        Fn f = (Fn)(void*)GetProcAddress(dwm, "DwmSetWindowAttribute");
        if (f) {
            int corner = 2;
            f(msg, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
            int dark = th.dark ? 1 : 0;
            f(msg, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        }
        typedef HRESULT(WINAPI *Ext)(HWND, const MARGINS*);
        Ext ext = (Ext)(void*)GetProcAddress(dwm, "DwmExtendFrameIntoClientArea");
        if (ext) {
            MARGINS m = { 1, 1, 1, 1 };
            ext(msg, &m);
        }
    }
    if (yrWindowsBuild() < 22000) {
        int rad = (int)(12 * s.scale);
        HRGN rgn = CreateRoundRectRgn(0, 0, pw + 1, ph + 1, rad * 2, rad * 2);
        if (rgn) SetWindowRgn(msg, rgn, TRUE);
    }

    RECT pr;
    if (parent && IsWindow(parent)) GetWindowRect(parent, &pr);
    else {
        pr.left = pr.top = 0;
        pr.right  = GetSystemMetrics(SM_CXSCREEN);
        pr.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int cx = pr.left + (pr.right - pr.left - pw) / 2;
    int cy = pr.top + (pr.bottom - pr.top - ph) / 2;
    SetWindowPos(msg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    if (const char* env = std::getenv("YR_GUI_AUTO_CLOSE_MS")) {
        int ms = atoi(env);
        if (ms > 0) SetTimer(msg, 0x51B1, (UINT)ms, nullptr);
    }

    ShowWindow(msg, SW_SHOW);
    if (parent && IsWindow(parent)) EnableWindow(parent, FALSE);
    SetForegroundWindow(msg);
    SetActiveWindow(msg);
    SetFocus(msg);
    UpdateWindow(msg);

    for (;;) {
        MSG m;
        BOOL r = GetMessageW(&m, nullptr, 0, 0);
        if (r <= 0) {
            PostQuitMessage((int)m.wParam);
            break;
        }
        if (m.hwnd == msg) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        } else if ((m.message == WM_KEYDOWN || m.message == WM_SYSKEYDOWN) &&
                   (m.wParam == VK_ESCAPE || m.wParam == VK_RETURN || m.wParam == VK_SPACE)) {
            s.result = 1;
            DestroyWindow(msg);
        }
        if (s.result) break;
    }
    if (parent && IsWindow(parent)) {
        EnableWindow(parent, TRUE);
        SetActiveWindow(parent);
    }
    return s.result;
}

//============================================================================
// 手动布局：Widget / UI
// ---------------------------------------------------------------------------
//   UI 不使用 Element、不自动排版。每个控件都显式指定 x / y / w / h。
//   坐标以窗口标题栏下方的内容区左上角为 (0, 0)，单位为逻辑像素。
//============================================================================
class Widget {
public:
    Widget() : m_app(nullptr), m_id(-1), m_top(0) {}
    Widget(App* app, int id, int top) : m_app(app), m_id(id), m_top(top) {}

    bool valid() const { return c() != nullptr; }
    int  id() const { return m_id; }

    Widget& rect(int x, int y, int w, int h)
    {
        Control* p = c();
        if (!p) return *this;
        p->x = m_app->dp(x);
        p->y = m_top + m_app->dp(y);
        p->w = m_app->dp(w);
        p->h = m_app->dp(h);
        m_app->redraw();
        return *this;
    }
    Widget& move(int x, int y)
    {
        Control* p = c();
        if (!p) return *this;
        p->x = m_app->dp(x);
        p->y = m_top + m_app->dp(y);
        m_app->redraw();
        return *this;
    }
    Widget& size(int w, int h)
    {
        Control* p = c();
        if (!p) return *this;
        p->w = m_app->dp(w);
        p->h = m_app->dp(h);
        m_app->redraw();
        return *this;
    }
    Widget& x(int v) { Control* p = c(); if (p) { p->x = m_app->dp(v); m_app->redraw(); } return *this; }
    Widget& y(int v) { Control* p = c(); if (p) { p->y = m_top + m_app->dp(v); m_app->redraw(); } return *this; }
    Widget& w(int v) { Control* p = c(); if (p) { p->w = m_app->dp(v); m_app->redraw(); } return *this; }
    Widget& h(int v) { Control* p = c(); if (p) { p->h = m_app->dp(v); m_app->redraw(); } return *this; }

    Widget& bg(int color)     { Control* p = c(); if (p) { p->bgColor = color; m_app->redraw(); } return *this; }
    Widget& fg(int color)     { Control* p = c(); if (p) { p->fgColor = color; m_app->redraw(); } return *this; }
    Widget& border(int color) { Control* p = c(); if (p) { p->borderColor = color; m_app->redraw(); } return *this; }
    Widget& accent(int color) { Control* p = c(); if (p) { p->accentColor = color; m_app->redraw(); } return *this; }
    Widget& radius(int v)     { Control* p = c(); if (p) { p->cornerRadius = v; m_app->redraw(); } return *this; }
    Widget& font(int v)       { Control* p = c(); if (p) { p->fontSize = v; m_app->redraw(); } return *this; }
    Widget& bold(bool v = true)   { Control* p = c(); if (p) { p->bold = v; p->strong = v; m_app->redraw(); } return *this; }
    Widget& center(bool v = true) { Control* p = c(); if (p) { p->center = v; m_app->redraw(); } return *this; }
    Widget& wrap(bool v = true)   { Control* p = c(); if (p) { p->wrap = v; m_app->redraw(); } return *this; }
    Widget& primary(bool v = true){ Control* p = c(); if (p) { p->primary = v; m_app->redraw(); } return *this; }
    Widget& visible(bool v)       { Control* p = c(); if (p) { p->visible = v; m_app->redraw(); } return *this; }
    Widget& enabled(bool v)       { Control* p = c(); if (p) { p->enabled = v; m_app->redraw(); } return *this; }
    Widget& tooltip(const wchar_t* t) { Control* p = c(); if (p) { p->tooltip = t ? t : L""; m_app->redraw(); } return *this; }

    Widget& text(const wchar_t* t)
    {
        Control* p = c();
        if (!p) return *this;
        p->text = t ? t : L"";
        if (p->type == Ctl::Edit || p->type == Ctl::TextArea) {
            p->caret = (int)p->text.size();
            p->selA = p->selB = p->caret;
        }
        m_app->redraw();
        return *this;
    }
    Widget& text(const std::wstring& t) { return text(t.c_str()); }
    std::wstring text() const { Control* p = c(); return p ? p->text : std::wstring(); }

    Widget& placeholder(const wchar_t* t) { Control* p = c(); if (p) { p->placeholder = t ? t : L""; m_app->redraw(); } return *this; }
    Widget& checked(bool v) { Control* p = c(); if (p) { p->checked = v; p->toggleT = v ? 1.f : 0.f; m_app->redraw(); } return *this; }
    bool checked() const { Control* p = c(); return p ? p->checked : false; }
    Widget& value(float v) { Control* p = c(); if (p) { p->progress = v; m_app->redraw(); } return *this; }
    float value() const { Control* p = c(); return p ? p->progress : 0.f; }
    Widget& range(float a, float b) { Control* p = c(); if (p) { p->min = a; p->max = b; m_app->redraw(); } return *this; }
    Widget& step(float v) { Control* p = c(); if (p) { p->step = v; m_app->redraw(); } return *this; }
    Widget& selected(int v) { Control* p = c(); if (p) { p->selected = v; m_app->redraw(); } return *this; }
    int selected() const { Control* p = c(); return p ? p->selected : -1; }
    Widget& items(std::initializer_list<const wchar_t*> its)
    {
        Control* p = c();
        if (!p) return *this;
        p->items.clear();
        for (const wchar_t* it : its) p->items.push_back(it ? it : L"");
        m_app->redraw();
        return *this;
    }
    Widget& headers(std::initializer_list<const wchar_t*> hs)
    {
        Control* p = c();
        if (!p) return *this;
        p->headers.clear();
        for (const wchar_t* h : hs) p->headers.push_back(h ? h : L"");
        m_app->redraw();
        return *this;
    }
    Widget& rows(std::vector<std::vector<std::wstring>> rs)
    {
        Control* p = c();
        if (p) { p->rows = std::move(rs); m_app->redraw(); }
        return *this;
    }
    Widget& nodes(std::vector<TreeNode> ns)
    {
        Control* p = c();
        if (p) { p->nodes = std::move(ns); m_app->redraw(); }
        return *this;
    }
    Widget& menus(std::vector<MenuEntry> ms)
    {
        Control* p = c();
        if (p) { p->menus = std::move(ms); m_app->redraw(); }
        return *this;
    }
    Widget& image(const wchar_t* path)
    {
        Control* p = c();
        if (!p) return *this;
        p->text = path ? path : L"";
        p->image.reset();
        if (p->text.size()) {
            std::shared_ptr<Gdiplus::Image> img = std::make_shared<Gdiplus::Image>(p->text.c_str());
            if (img->GetLastStatus() == Gdiplus::Ok) p->image = img;
        }
        m_app->redraw();
        return *this;
    }

    Widget& onClick(std::function<void()> fn)   { Control* p = c(); if (p) p->onClick = std::move(fn); return *this; }
    Widget& onChanged(std::function<void()> fn) { Control* p = c(); if (p) p->onChanged = std::move(fn); return *this; }
    Widget& onEnter(std::function<void()> fn)   { Control* p = c(); if (p) p->onEnter = std::move(fn); return *this; }
    Widget& onHover(std::function<void()> fn)   { Control* p = c(); if (p) p->onHover = std::move(fn); return *this; }
    Widget& onLeave(std::function<void()> fn)   { Control* p = c(); if (p) p->onLeave = std::move(fn); return *this; }

    Widget& redraw() { if (m_app) m_app->redraw(); return *this; }

private:
    Control* c() const { return (m_app && m_id >= 0) ? m_app->control(m_id) : nullptr; }

    App* m_app;
    int  m_id;
    int  m_top;
};

// 手动布局入口：显式坐标 + 显式样式 + 事件
class UI {
public:
    explicit UI(App& app) : m_app(app), m_top(app.titleBarHeight()) {}

    int contentTop() const { return m_top; }

    Widget label(const wchar_t* text, int x, int y, int w, int h, int fontSize = 0)
    {
        Control c;
        c.type = Ctl::Label;
        c.text = text ? text : L"";
        c.fontSize = fontSize;
        c.strong = true;
        return place(std::move(c), x, y, w, h);
    }
    Widget title(const wchar_t* text, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Label;
        c.text = text ? text : L"";
        c.fontSize = m_app.theme().titleSize;
        c.bold = true;
        c.strong = true;
        return place(std::move(c), x, y, w, h);
    }
    Widget caption(const wchar_t* text, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Label;
        c.text = text ? text : L"";
        c.fontSize = 12;
        c.fgColor = m_app.theme().fgDim;
        return place(std::move(c), x, y, w, h);
    }
    Widget button(const wchar_t* text, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Button;
        c.text = text ? text : L"";
        c.fontSize = 14;
        return place(std::move(c), x, y, w, h);
    }
    // 便捷重载：先创建按钮，再挂点击回调（适合固定布局）
    Widget button(const wchar_t* text, std::function<void()> onClick)
    {
        Widget w = button(text, 0, 0, 10, 10);
        w.onClick(std::move(onClick));
        return w;
    }
    Widget input(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Edit;
        c.caret = 0;
        return place(std::move(c), x, y, w, h);
    }
    Widget password(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Edit;
        c.password = true;
        return place(std::move(c), x, y, w, h);
    }
    Widget search(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Edit;
        c.searchBox = true;
        return place(std::move(c), x, y, w, h);
    }
    Widget textArea(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::TextArea;
        return place(std::move(c), x, y, w, h);
    }
    Widget checkbox(const wchar_t* text, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::CheckBox;
        c.text = text ? text : L"";
        return place(std::move(c), x, y, w, h);
    }
    Widget radio(const wchar_t* text, int x, int y, int w, int h, int group = 0)
    {
        Control c;
        c.type = Ctl::RadioButton;
        c.text = text ? text : L"";
        c.groupId = group;
        return place(std::move(c), x, y, w, h);
    }
    Widget toggle(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Toggle;
        return place(std::move(c), x, y, w, h);
    }
    Widget slider(int x, int y, int w, int h, float value = 0.5f,
                  float min = 0.f, float max = 1.f, float step = 0.01f)
    {
        Control c;
        c.type = Ctl::Slider;
        c.progress = value;
        c.min = min;
        c.max = max;
        c.step = step;
        return place(std::move(c), x, y, w, h);
    }
    Widget progress(int x, int y, int w, int h, float value = 0.f,
                    float min = 0.f, float max = 1.f)
    {
        Control c;
        c.type = Ctl::ProgressBar;
        c.progress = value;
        c.min = min;
        c.max = max;
        return place(std::move(c), x, y, w, h);
    }
    Widget spinBox(int x, int y, int w, int h, float value = 0.f,
                   float min = 0.f, float max = 100.f, float step = 1.f)
    {
        Control c;
        c.type = Ctl::SpinBox;
        c.progress = value;
        c.min = min;
        c.max = max;
        c.step = step;
        return place(std::move(c), x, y, w, h);
    }
    Widget combo(std::initializer_list<const wchar_t*> items, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::ComboBox;
        for (const wchar_t* it : items) c.items.push_back(it ? it : L"");
        c.selected = 0;
        return place(std::move(c), x, y, w, h);
    }
    Widget list(std::initializer_list<const wchar_t*> items, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::ListBox;
        for (const wchar_t* it : items) c.items.push_back(it ? it : L"");
        c.selected = 0;
        return place(std::move(c), x, y, w, h);
    }
    Widget table(std::initializer_list<const wchar_t*> headers,
                 std::vector<std::vector<std::wstring>> rows,
                 int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Table;
        for (const wchar_t* hd : headers) c.headers.push_back(hd ? hd : L"");
        c.rows = std::move(rows);
        c.selectedRow = -1;
        return place(std::move(c), x, y, w, h);
    }
    Widget tree(std::vector<TreeNode> nodes, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Tree;
        c.nodes = std::move(nodes);
        return place(std::move(c), x, y, w, h);
    }
    Widget calendar(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Calendar;
        SYSTEMTIME st;
        GetLocalTime(&st);
        c.calY = st.wYear; c.calM = st.wMonth; c.calD = st.wDay;
        c.hasDate = true;
        return place(std::move(c), x, y, w, h);
    }
    Widget rating(int maxStars, int value, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Rating;
        c.ratingMax = maxStars > 0 ? maxStars : 5;
        c.rating = value;
        return place(std::move(c), x, y, w, h);
    }
    Widget panel(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Card;
        c.cornerRadius = m_app.theme().radiusLg;
        return place(std::move(c), x, y, w, h);
    }
    Widget image(const wchar_t* path, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Image;
        c.text = path ? path : L"";
        if (c.text.size()) {
            std::shared_ptr<Gdiplus::Image> img = std::make_shared<Gdiplus::Image>(c.text.c_str());
            if (img->GetLastStatus() == Gdiplus::Ok) c.image = img;
        }
        return place(std::move(c), x, y, w, h);
    }
    Widget tag(const wchar_t* text, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Tag;
        c.text = text ? text : L"";
        c.fontSize = 12;
        return place(std::move(c), x, y, w, h);
    }
    Widget badge(const wchar_t* text, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::Badge;
        c.text = text ? text : L"";
        c.fontSize = 11;
        return place(std::move(c), x, y, w, h);
    }
    Widget link(const wchar_t* text, int x, int y, int w, int h,
                std::function<void()> onClick = nullptr)
    {
        Control c;
        c.type = Ctl::Link;
        c.text = text ? text : L"";
        c.onClick = std::move(onClick);
        return place(std::move(c), x, y, w, h);
    }
    Widget divider(int x, int y, int w)
    {
        Control c;
        c.type = Ctl::Divider;
        return place(std::move(c), x, y, w, 1);
    }
    Widget spinner(int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::ProgressRing;
        c.spin = true;
        return place(std::move(c), x, y, w, h);
    }
    Widget menuBar(std::vector<MenuEntry> menus, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::MenuBar;
        c.menus = std::move(menus);
        return place(std::move(c), x, y, w, h);
    }
    Widget statusBar(std::initializer_list<const wchar_t*> items, int x, int y, int w, int h)
    {
        Control c;
        c.type = Ctl::StatusBar;
        for (const wchar_t* it : items) c.items.push_back(it ? it : L"");
        c.fontSize = 12;
        return place(std::move(c), x, y, w, h);
    }

    void onResize(std::function<void(int, int)> fn)
    {
        App* app = &m_app;
        m_app.onResize([app, fn](int w, int h) {
            if (!fn) return;
            const int dpi = app->dpi();
            fn(w * 96 / (dpi > 0 ? dpi : 96), h * 96 / (dpi > 0 ? dpi : 96));
        });
    }
    void clear() { m_app.clearControls(); }
    void toast(const wchar_t* text, int ms = 1800) { m_app.toast(text, ms); }
    void msg(const wchar_t* text, const wchar_t* title = nullptr) { m_app.msg(text, title); }
    void close() { m_app.close(); }
    int  run()  { return m_app.run(); }

private:
    Widget place(Control c, int x, int y, int w, int h)
    {
        c.x = m_app.dp(x);
        c.y = m_top + m_app.dp(y);
        c.w = m_app.dp(w);
        c.h = m_app.dp(h);
        int id = m_app.addControl(std::move(c));
        return Widget(&m_app, id, m_top);
    }

    App& m_app;
    int  m_top;
};

} // namespace yr

#endif // YR_GUI_H
