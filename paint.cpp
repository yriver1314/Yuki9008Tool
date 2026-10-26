#include "app_state.h"

// ===========================================================================
//  geometry / primitive helpers
// ===========================================================================
int AppState::S(int v) const { return MulDiv(v, m_app.dpi(), 96); }
float AppState::SF(float v) const { return v * (float)m_app.dpi() / 96.f; }
int AppState::X(int v) const { return S(v); }
int AppState::Y(int v) const { return m_lay.top + S(v); }

Gdiplus::RectF AppState::R(const LR& r) const
{
    return Gdiplus::RectF((Gdiplus::REAL)X(r.x), (Gdiplus::REAL)Y(r.y),
                          (Gdiplus::REAL)S(r.w), (Gdiplus::REAL)S(r.h));
}

void AppState::FillRound(Gdiplus::Graphics& g, const LR& r, DWORD color, int radius,
                         BYTE alpha) const
{
    if (r.w <= 0 || r.h <= 0) return;
    Gdiplus::GraphicsPath p;
    buildRoundPath(p, (Gdiplus::REAL)X(r.x) + 0.5f, (Gdiplus::REAL)Y(r.y) + 0.5f,
                   (Gdiplus::REAL)S(r.w) - 1.f, (Gdiplus::REAL)S(r.h) - 1.f,
                   (Gdiplus::REAL)S(radius));
    Gdiplus::SolidBrush b(argb(alpha, color));
    g.FillPath(&b, &p);
}

void AppState::StrokeRound(Gdiplus::Graphics& g, const LR& r, DWORD color, int radius,
                           float width, BYTE alpha) const
{
    if (r.w <= 0 || r.h <= 0) return;
    Gdiplus::GraphicsPath p;
    buildRoundPath(p, (Gdiplus::REAL)X(r.x) + 0.5f, (Gdiplus::REAL)Y(r.y) + 0.5f,
                   (Gdiplus::REAL)S(r.w) - 1.f, (Gdiplus::REAL)S(r.h) - 1.f,
                   (Gdiplus::REAL)S(radius));
    Gdiplus::Pen pen(argb(alpha, color), std::max(1.f, SF(width)));
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    g.DrawPath(&pen, &p);
}

void AppState::Panel(Gdiplus::Graphics& g, const LR& r, DWORD top, DWORD bottom,
                     DWORD border, int radius, BYTE alpha, bool shadow) const
{
    if (r.w <= 0 || r.h <= 0) return;
    const int rad = S(radius);
    if (shadow) {
        for (int i = S(9); i >= S(2); i -= std::max(1, S(3))) {
            Gdiplus::GraphicsPath sp;
            buildRoundPath(sp, (Gdiplus::REAL)X(r.x) - i,
                           (Gdiplus::REAL)Y(r.y) - i + S(3),
                           (Gdiplus::REAL)S(r.w) + i * 2.f,
                           (Gdiplus::REAL)S(r.h) + i * 2.f,
                           (Gdiplus::REAL)(rad + i));
            Gdiplus::SolidBrush sb(argb(8, 0x000000));
            g.FillPath(&sb, &sp);
        }
    }

    Gdiplus::GraphicsPath p;
    buildRoundPath(p, (Gdiplus::REAL)X(r.x) + 0.5f, (Gdiplus::REAL)Y(r.y) + 0.5f,
                   (Gdiplus::REAL)S(r.w) - 1.f, (Gdiplus::REAL)S(r.h) - 1.f,
                   (Gdiplus::REAL)rad);
    Gdiplus::LinearGradientBrush b(
        Gdiplus::PointF((Gdiplus::REAL)X(r.x), (Gdiplus::REAL)Y(r.y)),
        Gdiplus::PointF((Gdiplus::REAL)X(r.x), (Gdiplus::REAL)Y(r.y + r.h)),
        argb(alpha, top), argb(alpha, bottom));
    g.FillPath(&b, &p);

    // inner top highlight
    if (r.w > rad * 2) {
        Gdiplus::Pen hl(argb(26, 0xFFFFFF), 1.f);
        g.DrawLine(&hl, (Gdiplus::REAL)(X(r.x) + rad),
                   (Gdiplus::REAL)Y(r.y) + 1.f,
                   (Gdiplus::REAL)(X(r.x + r.w) - rad),
                   (Gdiplus::REAL)Y(r.y) + 1.f);
    }

    Gdiplus::Pen pen(argb((BYTE)std::min(255, (int)alpha), border), 1.f);
    g.DrawPath(&pen, &p);
}

void AppState::Glow(Gdiplus::Graphics& g, int cx, int cy, int radius, DWORD color,
                    int strength, int layers) const
{
    if (!m_animEnabled && strength > 20) strength = std::min(strength, 20);
    for (int i = 0; i < layers; ++i) {
        float t = (float)i / (float)std::max(1, layers - 1);
        float rr = SF((float)radius * (0.32f + 0.68f * t));
        int a = (int)((float)strength * (1.f - t) * (1.f - t) * 2.6f / layers);
        if (a <= 0) continue;
        Gdiplus::SolidBrush b(argb((BYTE)std::min(255, a), color));
        g.FillEllipse(&b, (Gdiplus::REAL)X(cx) - rr, (Gdiplus::REAL)Y(cy) - rr,
                      rr * 2.f, rr * 2.f);
    }
}

void AppState::Txt(Gdiplus::Graphics& g, const std::wstring& s, const LR& r,
                   const Gdiplus::Font* f, DWORD color, BYTE alpha, int align) const
{
    if (!f || s.empty() || r.w <= 0 || r.h <= 0) return;
    Gdiplus::SolidBrush br(argb(alpha, color));
    Gdiplus::StringFormat fmt;
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetAlignment(align == 1 ? Gdiplus::StringAlignmentCenter
                               : align == 2 ? Gdiplus::StringAlignmentFar
                                            : Gdiplus::StringAlignmentNear);
    fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    g.DrawString(s.c_str(), -1, f, R(r), &fmt, &br);
}

void AppState::ClipText(Gdiplus::Graphics& g, const std::wstring& s, const LR& r,
                        const Gdiplus::Font* f, DWORD color, BYTE alpha, int align) const
{
    Txt(g, s, r, f, color, alpha, align);
}

void AppState::Hairline(Gdiplus::Graphics& g, int x1, int y1, int x2, int y2,
                        DWORD color, BYTE alpha, float width) const
{
    Gdiplus::Pen p(argb(alpha, color), std::max(1.f, SF(width)));
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&p, (Gdiplus::REAL)X(x1), (Gdiplus::REAL)Y(y1),
               (Gdiplus::REAL)X(x2), (Gdiplus::REAL)Y(y2));
}

// ===========================================================================
//  icons
// ===========================================================================
void AppState::IconFlash(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const
{
    float s = SF((float)size);
    float x = (float)X(cx), y = (float)Y(cy);
    Gdiplus::PointF pts[6] = {
        { x + s * 0.10f, y - s * 0.52f }, { x - s * 0.34f, y + s * 0.06f },
        { x - s * 0.04f, y + s * 0.06f }, { x - s * 0.13f, y + s * 0.52f },
        { x + s * 0.38f, y - s * 0.12f }, { x + s * 0.04f, y - s * 0.12f }
    };
    Gdiplus::SolidBrush b(rgb(color));
    g.FillPolygon(&b, pts, 6);
}

void AppState::IconBox(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const
{
    float s = SF((float)size), x = (float)X(cx), y = (float)Y(cy);
    Gdiplus::Pen p(rgb(color), std::max(1.f, SF(1.35f)));
    p.SetLineJoin(Gdiplus::LineJoinRound);
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);

    Gdiplus::GraphicsPath body;
    buildRoundPath(body, x - s * 0.44f, y - s * 0.26f,
                   s * 0.88f, s * 0.66f, s * 0.10f);
    g.DrawPath(&p, &body);
    Gdiplus::PointF tab[5] = {
        { x - s * 0.44f, y - s * 0.26f },
        { x - s * 0.18f, y - s * 0.26f },
        { x - s * 0.08f, y - s * 0.40f },
        { x + s * 0.14f, y - s * 0.40f },
        { x + s * 0.22f, y - s * 0.26f }
    };
    g.DrawLines(&p, tab, 5);
}

void AppState::IconGrid(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const
{
    float s = SF((float)size), x = (float)X(cx), y = (float)Y(cy);
    Gdiplus::Pen p(rgb(color), std::max(1.f, SF(1.35f)));
    float q = s * 0.17f, gap = s * 0.11f;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j) {
            Gdiplus::GraphicsPath path;
            buildRoundPath(path, x - q - gap + (q + gap) * 2 * i,
                           y - q - gap + (q + gap) * 2 * j,
                           q * 2, q * 2, s * 0.12f);
            g.DrawPath(&p, &path);
        }
}

void AppState::IconTerminal(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const
{
    float s = SF((float)size), x = (float)X(cx), y = (float)Y(cy);
    Gdiplus::Pen p(rgb(color), std::max(1.f, SF(1.35f)));
    p.SetLineJoin(Gdiplus::LineJoinRound);
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    Gdiplus::GraphicsPath path;
    buildRoundPath(path, x - s * 0.48f, y - s * 0.38f, s * 0.96f, s * 0.76f, s * 0.14f);
    g.DrawPath(&p, &path);
    g.DrawLine(&p, x - s * 0.25f, y - s * 0.11f, x - s * 0.06f, y + s * 0.06f);
    g.DrawLine(&p, x - s * 0.25f, y + s * 0.23f, x - s * 0.06f, y + s * 0.06f);
    g.DrawLine(&p, x + s * 0.04f, y + s * 0.23f, x + s * 0.26f, y + s * 0.23f);
}

void AppState::IconGear(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const
{
    float s = SF((float)size), x = (float)X(cx), y = (float)Y(cy);
    Gdiplus::Pen p(rgb(color), std::max(1.f, SF(1.35f)));
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    g.DrawEllipse(&p, x - s * 0.22f, y - s * 0.22f, s * 0.44f, s * 0.44f);
    g.DrawEllipse(&p, x - s * 0.07f, y - s * 0.07f, s * 0.14f, s * 0.14f);
    for (int i = 0; i < 8; ++i) {
        float a = (float)(i * 3.14159265 / 4.0);
        g.DrawLine(&p, x + std::cos(a) * s * 0.33f, y + std::sin(a) * s * 0.33f,
                   x + std::cos(a) * s * 0.46f, y + std::sin(a) * s * 0.46f);
    }
}

void AppState::IconRefresh(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const
{
    float s = SF((float)size), x = (float)X(cx), y = (float)Y(cy);
    Gdiplus::Pen p(rgb(color), std::max(1.f, SF(1.4f)));
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    g.DrawArc(&p, x - s * 0.42f, y - s * 0.42f, s * 0.84f, s * 0.84f, -45.f, 285.f);
    Gdiplus::PointF tri[3] = { { x + s * 0.43f, y - s * 0.40f },
                               { x + s * 0.16f, y - s * 0.48f },
                               { x + s * 0.30f, y - s * 0.12f } };
    Gdiplus::SolidBrush b(rgb(color));
    g.FillPolygon(&b, tri, 3);
}

void AppState::IconChevron(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color,
                           int dir) const
{
    float s = SF((float)size), x = (float)X(cx), y = (float)Y(cy);
    Gdiplus::Pen p(rgb(color), std::max(1.f, SF(1.4f)));
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    if (dir == 0) {
        g.DrawLine(&p, x - s * 0.22f, y - s * 0.10f, x, y + s * 0.14f);
        g.DrawLine(&p, x, y + s * 0.14f, x + s * 0.22f, y - s * 0.10f);
    } else {
        g.DrawLine(&p, x - s * 0.10f, y - s * 0.22f, x + s * 0.14f, y);
        g.DrawLine(&p, x + s * 0.14f, y, x - s * 0.10f, y + s * 0.22f);
    }
}

// ===========================================================================
//  page labels
// ===========================================================================
std::wstring AppState::PageTitle(int page) const
{
    switch (page) {
    case 1: return L"固件管理";
    case 2: return L"分区浏览";
    case 3: return L"运行日志";
    case 4: return L"设置";
    default: return L"刷写中心";
    }
}
std::wstring AppState::PageEyebrow(int page) const
{
    switch (page) {
    case 1: return L"YUKI9008  /  FIRMWARE";
    case 2: return L"YUKI9008  /  PARTITIONS";
    case 3: return L"YUKI9008  /  RUNTIME";
    case 4: return L"YUKI9008  /  PREFERENCES";
    default: return L"YUKI9008  /  FLASH CENTER";
    }
}
std::wstring AppState::PageSubtitle(int page) const
{
    switch (page) {
    case 1: return L"导入 rawprogram XML 与 Firehose 编程器，校验完整固件结构";
    case 2: return L"查看镜像映射、目标分区与写入顺序";
    case 3: return L"Firehose 会话与刷写事件的完整记录";
    case 4: return L"外观、动效与默认刷写策略";
    default: return L"设备检测  ·  固件校验  ·  分区写入  ·  安全重启";
    }
}

std::wstring AppState::SpeedText() const
{
    if (!m_running || m_speed < 1.f) return L"0 MB/s";
    wchar_t buf[32];
    swprintf(buf, 32, L"%.0f MB/s", m_speed);
    return buf;
}

// ===========================================================================
//  background
// ===========================================================================
void AppState::PaintBackground(Gdiplus::Graphics& g, int w, int h)
{
    ComputeLayout(w, h);
    const LLayout& l = m_lay;

    Gdiplus::LinearGradientBrush base(
        Gdiplus::PointF(0.f, 0.f), Gdiplus::PointF(0.f, (Gdiplus::REAL)h),
        rgb(col::bg0), rgb(0x04070C));
    g.FillRectangle(&base, (Gdiplus::REAL)0, (Gdiplus::REAL)0, (Gdiplus::REAL)w, (Gdiplus::REAL)h);

    // ambient light
    Glow(g, l.W / 2 - 40, 40, 480, m_accent, 12, 11);
    Glow(g, l.R - 40, 260, 350, m_accent2, 9, 9);
    Glow(g, l.L + 20, l.H - 20, 320, 0x2A6BC4, 7, 8);

    // soft grid of dots in the work area
    for (int y = 70; y < l.H - 10; y += 46) {
        for (int x = l.L; x < l.R; x += 46) {
            Gdiplus::SolidBrush dot(argb(16, 0x93B4DE));
            g.FillEllipse(&dot, (Gdiplus::REAL)X(x), (Gdiplus::REAL)Y(y),
                          std::max(1.f, SF(1.1f)), std::max(1.f, SF(1.1f)));
        }
    }

    // slow floating motes (subtle depth, only visible in the gutters)
    if (m_animEnabled) {
        for (int i = 0; i < 26; ++i) {
            float sx = std::fmod(i * 97.31f + m_time * (2.4f + (i % 3) * 0.9f),
                                 (float)std::max(1, l.W));
            float sy = std::fmod(i * 53.77f + m_time * (3.1f + (i % 4) * 0.7f),
                                 (float)std::max(1, l.H));
            if (sx < 220.f) sx += 220.f;
            int a = 14 + (i % 4) * 8;
            Gdiplus::SolidBrush mote(argb((BYTE)a, i % 3 == 0 ? m_accent : 0xBFD8FF));
            float rr = SF((i % 5 == 0) ? 1.6f : 1.0f);
            g.FillEllipse(&mote, (Gdiplus::REAL)X((int)sx) - rr, (Gdiplus::REAL)Y((int)sy) - rr,
                          rr * 2, rr * 2);
        }
    }

    // drifting light band
    if (m_animEnabled) {
        Gdiplus::GraphicsState st = g.Save();
        Gdiplus::GraphicsPath clip;
        buildRoundPath(clip, 0.f, (Gdiplus::REAL)Y(0), (Gdiplus::REAL)w,
                       (Gdiplus::REAL)S(l.H), (Gdiplus::REAL)S(0));
        g.SetClip(&clip, Gdiplus::CombineModeIntersect);
        float phase = std::sin(m_time * 0.23f) * 0.5f + 0.5f;
        Gdiplus::LinearGradientBrush band(
            Gdiplus::PointF((Gdiplus::REAL)X(l.L), 0),
            Gdiplus::PointF((Gdiplus::REAL)X(l.R), 0),
            argb(0, m_accent), argb(0, m_accent2));
        Gdiplus::Color colors[3] = { argb(0, m_accent), argb(18, m_accent2),
                                     argb(0, 0x5AA7FF) };
        Gdiplus::REAL positions[3] = { 0.f, 0.35f + phase * 0.3f, 1.f };
        band.SetInterpolationColors(colors, positions, 3);
        g.FillRectangle(&band, (Gdiplus::REAL)X(l.L), (Gdiplus::REAL)Y(0),
                        (Gdiplus::REAL)S(l.R - l.L), (Gdiplus::REAL)S(4));
        g.Restore(st);
    }

    // faint diagonal light
    {
        Gdiplus::PointF beam[4] = {
            { (Gdiplus::REAL)X(l.W * 2 / 3), (Gdiplus::REAL)Y(-40) },
            { (Gdiplus::REAL)X(l.W + 120), (Gdiplus::REAL)Y(-40) },
            { (Gdiplus::REAL)X(l.W + 260), (Gdiplus::REAL)Y(l.H / 2) },
            { (Gdiplus::REAL)X(l.W * 2 / 3 + 180), (Gdiplus::REAL)Y(l.H / 2) }
        };
        Gdiplus::SolidBrush b(argb(5, m_accent));
        g.FillPolygon(&b, beam, 4);
    }

    // sidebar
    {
        Gdiplus::LinearGradientBrush sb(
            Gdiplus::PointF(0, (Gdiplus::REAL)Y(0)),
            Gdiplus::PointF(0, (Gdiplus::REAL)Y(l.H)),
            rgb(0x0A101A), rgb(0x060A11));
        g.FillRectangle(&sb, 0.f, (Gdiplus::REAL)Y(0), (Gdiplus::REAL)S(232),
                        (Gdiplus::REAL)S(l.H));
        Gdiplus::Pen edge(argb(190, 0x142137), 1.f);
        g.DrawLine(&edge, (Gdiplus::REAL)S(232) - 0.5f, (Gdiplus::REAL)Y(0),
                   (Gdiplus::REAL)S(232) - 0.5f, (Gdiplus::REAL)Y(l.H));
        Gdiplus::LinearGradientBrush topGlow(
            Gdiplus::PointF(0, (Gdiplus::REAL)Y(0)),
            Gdiplus::PointF(0, (Gdiplus::REAL)Y(160)),
            argb(26, m_accent), argb(0, m_accent));
        g.FillRectangle(&topGlow, 0.f, (Gdiplus::REAL)Y(0), (Gdiplus::REAL)S(232),
                        (Gdiplus::REAL)S(160));
    }

    switch (m_page) {
    case 0: PaintDashboardBg(g); break;
    case 1: PaintFirmwarePage(g, true); break;
    case 2: PaintPartitionsPage(g, true); break;
    case 3: PaintLogPage(g, true); break;
    case 4: PaintSettingsPage(g, true); break;
    default: break;
    }
}

void AppState::PaintDashboardBg(Gdiplus::Graphics& g)
{
    const LLayout& l = m_lay;

    // hero
    Panel(g, l.hero, 0x0D1B30, 0x08101C, 0x1C3352, 18, 255, true);
    {
        Gdiplus::GraphicsPath clip;
        buildRoundPath(clip, (Gdiplus::REAL)X(l.hero.x), (Gdiplus::REAL)Y(l.hero.y),
                       (Gdiplus::REAL)S(l.hero.w), (Gdiplus::REAL)S(l.hero.h), (Gdiplus::REAL)S(18));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(&clip, Gdiplus::CombineModeIntersect);
        Gdiplus::LinearGradientBrush tint(
            Gdiplus::PointF((Gdiplus::REAL)X(l.hero.x), 0),
            Gdiplus::PointF((Gdiplus::REAL)X(l.hero.r()), 0),
            argb(34, m_accent), argb(0, m_accent));
        g.FillRectangle(&tint, R(l.hero));
        Gdiplus::LinearGradientBrush tint2(
            Gdiplus::PointF(0, (Gdiplus::REAL)Y(l.hero.y)),
            Gdiplus::PointF(0, (Gdiplus::REAL)Y(l.hero.b())),
            argb(0, m_accent2), argb(12, m_accent2));
        g.FillRectangle(&tint2, R(l.hero));
        g.Restore(st);
    }

    Panel(g, l.stage, col::panelTop, col::panelBot, 0x1B2B43, 16, 255, true);
    Panel(g, l.firmware, col::panelTop, col::panelBot, 0x1B2B43, 16, 255, true);
    Panel(g, l.health, col::panelTop, col::panelBot, 0x1B2B43, 16, 255, true);
    Panel(g, l.options, col::panelTop, col::panelBot, 0x1B2B43, 16, 255, true);
    Panel(g, l.device, col::panelTop, col::panelBot, 0x1B2B43, 16, 255, true);
    Panel(g, l.action, 0x0C1726, 0x09111B, 0x1C2E49, 14, 255, true);

    // accent line on action bar
    {
        Gdiplus::LinearGradientBrush line(
            Gdiplus::PointF((Gdiplus::REAL)X(l.action.x + 18), 0),
            Gdiplus::PointF((Gdiplus::REAL)X(l.action.r() - 18), 0),
            argb(0, m_accent), argb(120, m_accent));
        g.FillRectangle(&line, (Gdiplus::REAL)X(l.action.x + 18),
                        (Gdiplus::REAL)Y(l.action.y),
                        (Gdiplus::REAL)S(l.action.w - 36), std::max(1.f, SF(1.2f)));
    }

    Glow(g, l.hero.r() - 150, l.hero.y + 112, 135, m_accent, 20, 12);
    if (m_running)
        Glow(g, l.health.cx(), l.health.y + 112, 90, m_accent, 14, 8);
    else if (m_done)
        Glow(g, l.health.cx(), l.health.y + 112, 90, col::green, 16, 8);

    // connector ticks between cards
    Gdiplus::Pen tick(argb(30, m_accent), 1.f);
    for (int i = 0; i < 3; ++i) {
        int y = l.hero.b() + 4 + i * 3;
        g.DrawLine(&tick, (Gdiplus::REAL)X(l.hero.cx() - 18 + i * 18),
                   (Gdiplus::REAL)Y(y), (Gdiplus::REAL)X(l.hero.cx() - 7 + i * 18),
                   (Gdiplus::REAL)Y(y));
    }
}


