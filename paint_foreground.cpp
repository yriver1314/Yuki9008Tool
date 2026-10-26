#include "app_state.h"

// ===========================================================================
//  title mark / chrome
// ===========================================================================
void AppState::DrawBrandLogo(Gdiplus::Graphics& g, int x, int y, int size,
                             bool absoluteY) const
{
    if (!m_logo) return;
    const Gdiplus::REAL px = (Gdiplus::REAL)S(x);
    const Gdiplus::REAL py = (Gdiplus::REAL)(absoluteY ? S(y) : Y(y));
    const Gdiplus::REAL ps = (Gdiplus::REAL)S(size);
    const Gdiplus::REAL iw = (Gdiplus::REAL)m_logo->GetWidth();
    const Gdiplus::REAL ih = (Gdiplus::REAL)m_logo->GetHeight();
    if (iw <= 0 || ih <= 0) return;

    // Use the full logo.ico artwork; only soften the square silhouette a
    // little so it sits naturally in the title bar and sidebar.
    Gdiplus::GraphicsPath clip;
    buildRoundPath(clip, px, py, ps, ps, (Gdiplus::REAL)S(5));
    Gdiplus::GraphicsState st = g.Save();
    g.SetClip(&clip, Gdiplus::CombineModeIntersect);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.DrawImage(m_logo.get(), Gdiplus::RectF(px, py, ps, ps),
                0.f, 0.f, iw, ih, Gdiplus::UnitPixel);
    g.Restore(st);

    Gdiplus::Pen edge(argb(85, 0xFFFFFF), 1.f);
    g.DrawPath(&edge, &clip);
}

void AppState::PaintTitleMark(Gdiplus::Graphics& g)
{
    const int x = S(16), y = S(11), s = S(22);
    if (m_logo) {
        DrawBrandLogo(g, 16, 11, 22, true);
        return;
    }
    Gdiplus::GraphicsPath p;
    buildRoundPath(p, (Gdiplus::REAL)x, (Gdiplus::REAL)y, (Gdiplus::REAL)s, (Gdiplus::REAL)s,
                   (Gdiplus::REAL)S(6));
    Gdiplus::LinearGradientBrush b(Gdiplus::PointF((Gdiplus::REAL)x, (Gdiplus::REAL)y),
                                   Gdiplus::PointF((Gdiplus::REAL)(x + s), (Gdiplus::REAL)(y + s)),
                                   rgb(m_accent), rgb(m_accent2));
    g.FillPath(&b, &p);
    Gdiplus::Pen pen(argb(75, 0xFFFFFF), 1.f);
    g.DrawPath(&pen, &p);

    float cx = (float)x + s * 0.5f, cy = (float)y + s * 0.5f, q = s * 0.26f;
    Gdiplus::PointF bolt[6] = {
        { cx + q * 0.10f, cy - q }, { cx - q * 0.62f, cy + q * 0.12f },
        { cx - q * 0.08f, cy + q * 0.12f }, { cx - q * 0.16f, cy + q },
        { cx + q * 0.64f, cy - q * 0.18f }, { cx + q * 0.10f, cy - q * 0.18f }
    };
    Gdiplus::SolidBrush wb(rgb(0xFFFFFF));
    g.FillPolygon(&wb, bolt, 6);
}

void AppState::PaintChrome(Gdiplus::Graphics& g)
{
    const LLayout& l = m_lay;

    // brand mark (the same logo.ico used by the window / executable)
    {
        LR mark = { 26, 18, 36, 36 };
        if (m_logo) {
            Glow(g, mark.cx(), mark.cy(), 31, m_accent, 20, 8);
            DrawBrandLogo(g, mark.x, mark.y, mark.w, false);
        } else {
            Gdiplus::GraphicsPath p;
            buildRoundPath(p, (Gdiplus::REAL)X(mark.x), (Gdiplus::REAL)Y(mark.y),
                           (Gdiplus::REAL)S(mark.w), (Gdiplus::REAL)S(mark.h),
                           (Gdiplus::REAL)S(11));
            Gdiplus::LinearGradientBrush b(
                Gdiplus::PointF((Gdiplus::REAL)X(mark.x), (Gdiplus::REAL)Y(mark.y)),
                Gdiplus::PointF((Gdiplus::REAL)X(mark.r()), (Gdiplus::REAL)Y(mark.b())),
                rgb(m_accent), rgb(m_accent2));
            g.FillPath(&b, &p);
            Gdiplus::Pen edge(argb(70, 0xFFFFFF), 1.f);
            g.DrawPath(&edge, &p);
            float cx = (float)X(mark.cx()), cy = (float)Y(mark.cy()), s = SF(16.f);
            Gdiplus::PointF bolt[6] = {
                { cx + s * 0.10f, cy - s * 0.48f }, { cx - s * 0.40f, cy + s * 0.05f },
                { cx - s * 0.04f, cy + s * 0.05f }, { cx - s * 0.12f, cy + s * 0.48f },
                { cx + s * 0.42f, cy - s * 0.08f }, { cx + s * 0.06f, cy - s * 0.08f }
            };
            Gdiplus::SolidBrush wb(rgb(0xFFFFFF));
            g.FillPolygon(&wb, bolt, 6);
        }
    }

    Txt(g, L"Yuki9008", { 74, 19, 138, 22 }, m_fonts.h2.get(), col::ink, 255, 0);
    Txt(g, L"9008 TOOL  ·  FLASH STUDIO", { 75, 41, 150, 12 }, m_fonts.tiny.get(), 0x91A4BE, 255, 0);
    Txt(g, L"v1.0.1", { 170, 24, 50, 12 }, m_fonts.tiny.get(), 0x51637C, 255, 2);

    Hairline(g, 26, 76, 206, 76, 0x16243A, 220);
    Txt(g, L"工作区", { 28, 94, 90, 14 }, m_fonts.tiny.get(), 0x5C718F, 255, 0);

    static const wchar_t* names[5] = { L"刷写中心", L"固件管理", L"分区浏览", L"运行日志", L"设置" };
    for (int i = 0; i < 5; ++i) {
        LR r = l.nav[(size_t)i];
        float t = std::max(0.f, std::min(1.f, m_navT[(size_t)i]));
        bool active = (m_page == i);

        if (t > 0.02f) {
            int a = (int)(t * (active ? 46 : 26));
            FillRound(g, r, m_accent, 12, (BYTE)std::max(0, std::min(255, a)));
            if (active) {
                StrokeRound(g, r, m_accent, 12, 1.f, 60);
                Gdiplus::LinearGradientBrush bar(
                    Gdiplus::PointF((Gdiplus::REAL)X(r.x + 1), (Gdiplus::REAL)Y(r.y + 10)),
                    Gdiplus::PointF((Gdiplus::REAL)X(r.x + 1), (Gdiplus::REAL)Y(r.b() - 10)),
                    rgb(m_accent), rgb(m_accent2));
                FillRound(g, { r.x, r.y + 11, 3, r.h - 22 }, m_accent, 2, 255);
                Gdiplus::SolidBrush fb(rgb(m_accent));
                g.FillRectangle(&fb, (Gdiplus::REAL)X(r.x + 1), (Gdiplus::REAL)Y(r.y + 12),
                                std::max(1.f, SF(2.4f)), (Gdiplus::REAL)S(r.h - 24));
            }
        }

        DWORD iconCol = lerpColor(0x6E809A, m_accent, active ? 1.f : t * 0.8f);
        int cx = r.x + 24, cy = r.cy();
        switch (i) {
        case 0: IconFlash(g, cx, cy, 17, iconCol); break;
        case 1: IconBox(g, cx, cy, 16, iconCol); break;
        case 2: IconGrid(g, cx, cy, 15, iconCol); break;
        case 3: IconTerminal(g, cx, cy, 16, iconCol); break;
        default: IconGear(g, cx, cy, 16, iconCol); break;
        }

        DWORD tc = active ? col::ink : lerpColor(0x8A9AB3, 0xD5E3F8, t * 0.55f);
        Txt(g, names[i], { r.x + 44, r.y, 140, r.h }, active ? m_fonts.h3.get() : m_fonts.body.get(),
            tc, 255, 0);
        if (active) {
            Gdiplus::SolidBrush db(rgb(m_accent));
            g.FillEllipse(&db, (Gdiplus::REAL)X(r.r() - 18), (Gdiplus::REAL)Y(r.cy() - 2),
                          std::max(1.f, SF(3.f)), std::max(1.f, SF(3.f)));
        }
    }

    // system label
    Txt(g, L"系统", { 28, 406, 90, 14 }, m_fonts.tiny.get(), 0x5C718F, 255, 0);

    // tip card
    {
        Panel(g, l.tipMini, 0x0B1421, 0x08101A, 0x182840, 12, 235, false);
        Txt(g, L"刷机前检查", { l.tipMini.x + 14, l.tipMini.y + 11, 150, 16 },
            m_fonts.h3.get(), 0xC4D6ED, 255, 0);
        struct Tip { const wchar_t* text; DWORD color; };
        Tip tips[3] = {
            { L"电量高于 30%", col::green },
            { L"使用原装数据线", 0x8EA3BF },
            { L"备份 userdata", 0x8EA3BF },
        };
        for (int i = 0; i < 3; ++i) {
            int y = l.tipMini.y + 35 + i * 17;
            Gdiplus::SolidBrush dot(argb(180, tips[i].color));
            g.FillEllipse(&dot, (Gdiplus::REAL)X(l.tipMini.x + 15),
                          (Gdiplus::REAL)Y(y + 5), std::max(1.f, SF(2.6f)),
                          std::max(1.f, SF(2.6f)));
            Txt(g, tips[i].text, { l.tipMini.x + 25, y, 160, 14 }, m_fonts.tiny.get(),
                0x8498B4, 255, 0);
        }
    }

    // device mini
    {
        LR r = l.deviceMini;
        Panel(g, r, 0x0B1625, 0x08101B, m_dev.present ? 0x1E4C42 : 0x2A3344, 13, 245, false);
        Txt(g, L"设备状态", { r.x + 14, r.y + 11, 100, 14 }, m_fonts.tiny.get(), 0x6F849F, 255, 0);

        bool scanning = m_scanning;
        bool live = m_dev.present;
        DWORD statusCol = m_failed ? col::red :
                          m_done ? col::green :
                          m_running ? m_accent :
                          live ? col::green : (scanning ? m_accent : col::amber);
        const wchar_t* status = m_running ? L"EDL 写入中" :
                               m_failed ? L"写入失败" :
                               m_done ? L"刷写完成" :
                               scanning ? L"正在扫描" :
                               live ? L"已连接" : L"未连接";
        float pulse = 0.5f + 0.5f * std::sin(m_time * 3.0f);
        if (scanning || live || m_running) {
            Gdiplus::SolidBrush halo(argb((BYTE)(40 + pulse * 50), statusCol));
            float rr = SF(6.f + pulse * 4.f);
            g.FillEllipse(&halo, (Gdiplus::REAL)X(r.x + 19) - rr,
                          (Gdiplus::REAL)Y(r.y + 39) - rr, rr * 2, rr * 2);
        }
        Gdiplus::SolidBrush dot(rgb(statusCol));
        g.FillEllipse(&dot, (Gdiplus::REAL)X(r.x + 15), (Gdiplus::REAL)Y(r.y + 35),
                      std::max(1.f, SF(7.f)), std::max(1.f, SF(7.f)));
        Txt(g, status, { r.x + 29, r.y + 30, 155, 18 }, m_fonts.bodyBold.get(),
            m_failed ? 0xFFB3BE : (live ? 0xDDF7EA : 0xE9D9BD), 255, 0);
        Txt(g, L"QDLoader 9008", { r.x + 14, r.y + 53, 160, 14 }, m_fonts.tiny.get(),
            0x7E93AF, 255, 0);
        Txt(g, L"VID_05C6  ·  PID_" + m_dev.pid, { r.x + 14, r.y + 69, 175, 13 },
            m_fonts.monoS.get(), 0x637993, 255, 0);

        // mini link bars
        for (int i = 0; i < 4; ++i) {
            int bh = 4 + i * 3;
            int a = live ? 130 : (scanning ? 40 + (int)(pulse * 50) : 30);
            FillRound(g, { r.x + 14 + i * 8, r.y + 105 - bh, 4, bh }, statusCol, 2, (BYTE)a);
        }
        Txt(g, scanning ? L"ENUMERATING" :
                (live ? L"USB LINK ACTIVE"
                      : (m_engineReady ? L"ENGINE READY"
                                       : (!m_engineSahara.empty() ? L"ENGINE INCOMPLETE"
                                                                  : L"ENGINE NOT FOUND"))),
            { r.x + 56, r.y + 94, 140, 13 }, m_fonts.monoS.get(),
            live ? 0x6FAF99 : (m_engineReady ? 0x6F8FB5 : 0x7D6C52), 255, 0);
    }

    Txt(g, L"BUILD 2026.10  ·  BY YRIVER1314", { 26, l.H - 54, 190, 14 },
        m_fonts.tiny.get(), 0x4C5D73, 255, 0);
    Txt(g, L"YRGUI 4.0  ·  MIT", { 26, l.H - 36, 160, 12 }, m_fonts.tiny.get(), 0x3D4C60, 255, 0);
}

// ===========================================================================
//  header
// ===========================================================================
void AppState::PaintHeader(Gdiplus::Graphics& g)
{
    const LLayout& l = m_lay;
    int maxW = l.pill.x - l.L - 16;
    Txt(g, PageEyebrow(m_page), { l.L, 15, std::max(120, maxW), 14 },
        m_fonts.eyebrow.get(), 0x6F86A5, 255, 0);
    Txt(g, PageTitle(m_page), { l.L, 29, std::max(160, maxW), 38 },
        m_fonts.title.get(), col::ink, 255, 0);
    Txt(g, PageSubtitle(m_page), { l.L, 69, std::max(180, maxW), 16 },
        m_fonts.small.get(), 0x8297B3, 255, 0);

    // status pill
    LR r = l.pill;
    bool scanning = m_scanning;
    bool live = m_dev.present;
    DWORD statusCol = m_running ? m_accent :
                      m_done ? col::green :
                      m_failed ? col::red :
                      m_aborted ? col::amber :
                      scanning ? m_accent :
                      live ? col::green : col::amber;
    DWORD bgTop = lerpColor(0x0E1928, statusCol, 0.08f);
    DWORD bgBot = lerpColor(0x09111C, statusCol, 0.04f);
    Panel(g, r, bgTop, bgBot, lerpColor(0x263650, statusCol, 0.36f), 18, 245, false);
    if (m_hoverPill) StrokeRound(g, r, statusCol, 18, 1.f, 95);

    int dotX = r.x + 20, dotY = r.cy();
    float pulse = 0.5f + 0.5f * std::sin(m_time * 3.2f);
    if (scanning || live || m_running) {
        Gdiplus::SolidBrush halo(argb((BYTE)(35 + pulse * 55), statusCol));
        float rr = SF(7.f + pulse * 3.f);
        g.FillEllipse(&halo, (Gdiplus::REAL)X(dotX) - rr, (Gdiplus::REAL)Y(dotY) - rr,
                      rr * 2, rr * 2);
    }
    Gdiplus::SolidBrush dot(rgb(statusCol));
    g.FillEllipse(&dot, (Gdiplus::REAL)X(dotX) - SF(3.f), (Gdiplus::REAL)Y(dotY) - SF(3.f),
                  SF(6.f), SF(6.f));

    std::wstring main, sub;
    if (m_running) {
        main = m_hasProgress ? (L"正在写入  " + PercentText(m_display)) : L"正在写入";
        sub = m_speed > 1.f ? (SpeedText() + L"  ·  Firehose")
                            : L"Firehose  ·  请保持 USB 连接";
    } else if (m_done) {
        main = L"刷写完成";
        sub = L"全部 " + std::to_wstring(m_fw.parts.size()) + L" 个分区写入完成";
    } else if (m_failed) {
        main = L"写入失败";
        sub = L"请查看运行日志并重新连接设备";
    } else if (m_aborted) {
        main = L"刷写已中止";
        sub = L"写入进程已终止";
    } else if (scanning) {
        int dots = (int)(m_time * 3.f) % 4;
        main = L"正在扫描 9008" + std::wstring((size_t)dots, L'.');
        sub = L"USB  ·  VID_05C6";
    } else if (live) {
        main = L"9008 设备已连接";
        sub = m_dev.port + (m_engineReady ? L"  ·  QDLoader" : L"  ·  引擎未就绪");
    } else {
        main = L"设备未连接";
        sub = L"连接 9008 后开始";
    }
    Txt(g, main, { r.x + 32, r.y + 4, r.w - 72, 18 }, m_fonts.bodyBold.get(), 0xEAF3FF, 255, 0);
    Txt(g, sub, { r.x + 32, r.y + 20, r.w - 72, 13 }, m_fonts.tiny.get(), 0x8398B4, 255, 0);
    IconRefresh(g, r.r() - 20, r.cy(), 14, m_hoverPill ? statusCol : 0x7C90AB);

    // little scan sweep inside pill
    if (scanning) {
        float p = std::fmod(m_time * 1.6f, 1.f);
        Gdiplus::GraphicsPath clip;
        buildRoundPath(clip, (Gdiplus::REAL)X(r.x), (Gdiplus::REAL)Y(r.y),
                       (Gdiplus::REAL)S(r.w), (Gdiplus::REAL)S(r.h), (Gdiplus::REAL)S(18));
        Gdiplus::GraphicsState st = g.Save();
        g.SetClip(&clip, Gdiplus::CombineModeIntersect);
        Gdiplus::LinearGradientBrush sweep(
            Gdiplus::PointF((Gdiplus::REAL)X(r.x + (int)(p * r.w)) - 40, 0),
            Gdiplus::PointF((Gdiplus::REAL)X(r.x + (int)(p * r.w)) + 40, 0),
            argb(0, statusCol), argb(52, statusCol));
        g.FillRectangle(&sweep, R(r));
        g.Restore(st);
    }
}

// ===========================================================================
//  hero
// ===========================================================================
void AppState::PaintHeroArt(Gdiplus::Graphics& g)
{
    const LLayout& l = m_lay;
    const LR& hero = l.hero;
    Gdiplus::GraphicsState st = g.Save();
    Gdiplus::GraphicsPath clip;
    buildRoundPath(clip, (Gdiplus::REAL)X(hero.x), (Gdiplus::REAL)Y(hero.y),
                   (Gdiplus::REAL)S(hero.w), (Gdiplus::REAL)S(hero.h), (Gdiplus::REAL)S(18));
    g.SetClip(&clip, Gdiplus::CombineModeIntersect);

    int cx = hero.r() - 150;
    int cy = hero.y + 112;
    bool hot = m_running || m_scanning;
    DWORD art = m_running ? m_accent : (m_done ? col::green : m_accent);
    DWORD art2 = m_accent2;

    Glow(g, cx, cy, 116, art, hot ? 38 : 24, 13);

    // orbital arcs
    for (int i = 0; i < 3; ++i) {
        float rr = SF((float)(88 - i * 13));
        float start = m_time * (14.f + i * 9.f) + i * 47.f;
        Gdiplus::Pen pen(argb((BYTE)(72 - i * 16), i == 1 ? art2 : art),
                         std::max(1.f, SF(1.15f)));
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&pen, (Gdiplus::REAL)X(cx) - rr, (Gdiplus::REAL)Y(cy) - rr,
                  rr * 2, rr * 2, start, 120.f + i * 25.f);
        Gdiplus::Pen dash(argb(34, 0x7AA4D6), 1.f);
        Gdiplus::REAL d[2] = { 2.f, 7.f };
        dash.SetDashPattern(d, 2);
        g.DrawEllipse(&dash, (Gdiplus::REAL)X(cx) - rr - SF(9),
                      (Gdiplus::REAL)Y(cy) - rr - SF(9),
                      (rr + SF(9)) * 2, (rr + SF(9)) * 2);
    }

    // orbit nodes
    for (int i = 0; i < 3; ++i) {
        float a = m_time * 0.55f + (float)i * 2.094f;
        float rr = SF(76.f);
        float nx = (float)X(cx) + std::cos(a) * rr;
        float ny = (float)Y(cy) + std::sin(a) * rr;
        Gdiplus::SolidBrush nb(argb(210, i == 1 ? art2 : art));
        g.FillEllipse(&nb, nx - SF(2.4f), ny - SF(2.4f), SF(4.8f), SF(4.8f));
    }

    // phone
    LR phone = { cx - 108, cy - 66, 76, 132 };
    {
        Gdiplus::GraphicsPath p;
        buildRoundPath(p, (Gdiplus::REAL)X(phone.x), (Gdiplus::REAL)Y(phone.y),
                       (Gdiplus::REAL)S(phone.w), (Gdiplus::REAL)S(phone.h),
                       (Gdiplus::REAL)S(13));
        Gdiplus::LinearGradientBrush b(
            Gdiplus::PointF((Gdiplus::REAL)X(phone.x), (Gdiplus::REAL)Y(phone.y)),
            Gdiplus::PointF((Gdiplus::REAL)X(phone.r()), (Gdiplus::REAL)Y(phone.b())),
            rgb(0x0C1B2D), rgb(0x060D17));
        g.FillPath(&b, &p);
        Gdiplus::Pen edge(argb(180, 0x2B4C70), 1.f);
        g.DrawPath(&edge, &p);
    }
    LR screen = { phone.x + 5, phone.y + 6, phone.w - 10, phone.h - 12 };
    {
        Gdiplus::GraphicsPath p;
        buildRoundPath(p, (Gdiplus::REAL)X(screen.x), (Gdiplus::REAL)Y(screen.y),
                       (Gdiplus::REAL)S(screen.w), (Gdiplus::REAL)S(screen.h),
                       (Gdiplus::REAL)S(9));
        Gdiplus::LinearGradientBrush b(
            Gdiplus::PointF((Gdiplus::REAL)X(screen.x), (Gdiplus::REAL)Y(screen.y)),
            Gdiplus::PointF((Gdiplus::REAL)X(screen.x), (Gdiplus::REAL)Y(screen.b())),
            rgb(0x08131F), rgb(0x040911));
        g.FillPath(&b, &p);
        Gdiplus::GraphicsState s2 = g.Save();
        g.SetClip(&p, Gdiplus::CombineModeIntersect);

        // scanline
        if (m_animEnabled) {
            float sp = std::fmod(m_time * 0.38f, 1.f);
            int sy = screen.y + 5 + (int)(sp * (screen.h - 10));
            Gdiplus::LinearGradientBrush scan(
                Gdiplus::PointF(0, (Gdiplus::REAL)Y(sy - 18)),
                Gdiplus::PointF(0, (Gdiplus::REAL)Y(sy + 8)),
                argb(0, art), argb(52, art));
            g.FillRectangle(&scan, R(screen));
        }

        // notch
        Gdiplus::SolidBrush notch(rgb(0x03070C));
        g.FillRectangle(&notch, (Gdiplus::REAL)X(screen.cx() - 12),
                        (Gdiplus::REAL)Y(screen.y), (Gdiplus::REAL)S(24),
                        std::max(2.f, SF(3.f)));

        // little EDL UI
        Txt(g, L"EDL", { screen.x + 8, screen.y + 16, 54, 12 }, m_fonts.monoS.get(),
            art, 255, 0);
        Txt(g, L"9008", { screen.x + 8, screen.y + 32, 54, 18 }, m_fonts.num.get(),
            col::ink, 255, 0);
        for (int i = 0; i < 3; ++i) {
            int ww = 18 + i * 11;
            FillRound(g, { screen.x + 8, screen.y + 58 + i * 9, ww, 3 }, 0x24425F, 2, 210);
        }
        // battery
        Gdiplus::Pen bp(argb(190, 0x4C7CA8), std::max(1.f, SF(1.f)));
        g.DrawRectangle(&bp, (Gdiplus::REAL)X(screen.x + 8),
                        (Gdiplus::REAL)Y(screen.b() - 18), (Gdiplus::REAL)S(24),
                        (Gdiplus::REAL)S(9));
        FillRound(g, { screen.x + 10, screen.b() - 16, 16, 5 }, art, 1, 210);
        g.Restore(s2);
    }

    // cable + chip
    int chipX = cx + 22, chipY = cy - 14;
    {
        Gdiplus::GraphicsPath cable;
        cable.AddBezier((Gdiplus::REAL)X(phone.r()),
                        (Gdiplus::REAL)Y(cy + 25),
                        (Gdiplus::REAL)X(phone.r() + 24),
                        (Gdiplus::REAL)Y(cy + 25),
                        (Gdiplus::REAL)X(chipX - 20),
                        (Gdiplus::REAL)Y(chipY + 30),
                        (Gdiplus::REAL)X(chipX),
                        (Gdiplus::REAL)Y(chipY + 30));
        Gdiplus::Pen cablePen(argb(180, 0x315A83), std::max(1.f, SF(1.5f)));
        cablePen.SetStartCap(Gdiplus::LineCapRound);
        cablePen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawPath(&cablePen, &cable);

        for (int i = 0; i < 3; ++i) {
            float t = std::fmod(m_time * 0.42f + i * 0.33f, 1.f);
            float mt = 1.f - t;
            float px = mt * mt * mt * (float)X(phone.r()) +
                       3 * mt * mt * t * (float)X(phone.r() + 24) +
                       3 * mt * t * t * (float)X(chipX - 20) +
                       t * t * t * (float)X(chipX);
            float py = mt * mt * mt * (float)Y(cy + 25) +
                       3 * mt * mt * t * (float)Y(cy + 25) +
                       3 * mt * t * t * (float)Y(chipY + 30) +
                       t * t * t * (float)Y(chipY + 30);
            Gdiplus::SolidBrush pb(argb((BYTE)(120 + 120 * (1.f - t)), art));
            g.FillEllipse(&pb, px - SF(2.2f), py - SF(2.2f), SF(4.4f), SF(4.4f));
        }

        // chip body
        Gdiplus::GraphicsPath chip;
        buildRoundPath(chip, (Gdiplus::REAL)X(chipX), (Gdiplus::REAL)Y(chipY),
                       (Gdiplus::REAL)S(54), (Gdiplus::REAL)S(54), (Gdiplus::REAL)S(10));
        Gdiplus::LinearGradientBrush cb(
            Gdiplus::PointF((Gdiplus::REAL)X(chipX), (Gdiplus::REAL)Y(chipY)),
            Gdiplus::PointF((Gdiplus::REAL)X(chipX + 54), (Gdiplus::REAL)Y(chipY + 54)),
            lerpColor(0x0A1726, art, 0.16f), rgb(0x07101B));
        g.FillPath(&cb, &chip);
        Gdiplus::Pen cedge(argb(210, art), 1.2f);
        g.DrawPath(&cedge, &chip);

        // pins
        for (int i = 0; i < 4; ++i) {
            Hairline(g, chipX - 6, chipY + 10 + i * 9, chipX, chipY + 10 + i * 9, art, 150);
            Hairline(g, chipX + 54, chipY + 10 + i * 9, chipX + 60,
                     chipY + 10 + i * 9, art, 150);
        }
        Txt(g, L"9008", { chipX, chipY + 13, 54, 16 }, m_fonts.num.get(), 0xFFFFFF, 255, 1);
        Txt(g, L"EDL", { chipX, chipY + 30, 54, 12 }, m_fonts.monoS.get(), art, 255, 1);

        // circuit traces to right edge
        Gdiplus::Pen trace(argb(100, 0x2B527A), 1.f);
        trace.SetLineJoin(Gdiplus::LineJoinRound);
        int baseY = chipY + 27;
        g.DrawLine(&trace, (Gdiplus::REAL)X(chipX + 60), (Gdiplus::REAL)Y(baseY),
                   (Gdiplus::REAL)X(chipX + 86), (Gdiplus::REAL)Y(baseY));
        g.DrawLine(&trace, (Gdiplus::REAL)X(chipX + 86), (Gdiplus::REAL)Y(baseY),
                   (Gdiplus::REAL)X(chipX + 86), (Gdiplus::REAL)Y(baseY - 30));
        g.DrawLine(&trace, (Gdiplus::REAL)X(chipX + 86), (Gdiplus::REAL)Y(baseY - 30),
                   (Gdiplus::REAL)X(chipX + 122), (Gdiplus::REAL)Y(baseY - 30));
        g.DrawLine(&trace, (Gdiplus::REAL)X(chipX + 86), (Gdiplus::REAL)Y(baseY),
                   (Gdiplus::REAL)X(chipX + 86), (Gdiplus::REAL)Y(baseY + 26));
        g.DrawLine(&trace, (Gdiplus::REAL)X(chipX + 86), (Gdiplus::REAL)Y(baseY + 26),
                   (Gdiplus::REAL)X(chipX + 112), (Gdiplus::REAL)Y(baseY + 26));
        for (int i = 0; i < 3; ++i) {
            float a = m_time * 1.4f + i * 2.1f;
            int nodeX = chipX + 86, nodeY = baseY + (i == 1 ? -30 : (i == 2 ? 26 : 0));
            Gdiplus::SolidBrush n(argb((BYTE)(90 + 80 * (0.5f + 0.5f * std::sin(a))), art));
            g.FillEllipse(&n, (Gdiplus::REAL)X(nodeX) - SF(2.f),
                          (Gdiplus::REAL)Y(nodeY) - SF(2.f), SF(4.f), SF(4.f));
        }
    }

    Txt(g, L"SAHARA / FIREHOSE", { hero.r() - 268, hero.y + 25, 220, 14 },
        m_fonts.monoS.get(), 0x55779B, 255, 2);
    Txt(g, L"9008", { hero.r() - 152, hero.b() - 56, 140, 46 },
        m_fonts.numXL.get(), art, 20, 2);

    if ((m_scanning || m_running) && m_animEnabled) {
        Gdiplus::GraphicsState ss = g.Save();
        Gdiplus::RectF ac((Gdiplus::REAL)X(hero.r() - 300), (Gdiplus::REAL)Y(hero.y),
                          (Gdiplus::REAL)S(300), (Gdiplus::REAL)S(hero.h));
        g.SetClip(ac, Gdiplus::CombineModeIntersect);
        float p = std::fmod(m_time * (m_scanning ? 0.55f : 0.28f), 1.f);
        int bx = hero.r() - 300 + (int)(p * 300.f);
        Gdiplus::LinearGradientBrush sweep(
            Gdiplus::PointF((Gdiplus::REAL)X(bx - 34), 0),
            Gdiplus::PointF((Gdiplus::REAL)X(bx + 34), 0),
            argb(0, 0xFFFFFF), argb(18, 0xFFFFFF));
        g.FillRectangle(&sweep, (Gdiplus::REAL)X(bx - 34), (Gdiplus::REAL)Y(hero.y),
                        (Gdiplus::REAL)S(68), (Gdiplus::REAL)S(hero.h));
        g.Restore(ss);
    }

    g.Restore(st);
}

void AppState::PaintDashboardFg(Gdiplus::Graphics& g)
{
    const LLayout& l = m_lay;
    const LR& h = l.hero;

    // badge
    {
        LR badge = { h.x + 28, h.y + 22, 176, 22 };
        FillRound(g, badge, m_accent, 11, 30);
        StrokeRound(g, badge, m_accent, 11, 1.f, 60);
        Gdiplus::SolidBrush dot(rgb(m_statusColor));
        g.FillEllipse(&dot, (Gdiplus::REAL)X(badge.x + 12), (Gdiplus::REAL)Y(badge.cy() - 2),
                      std::max(1.f, SF(4.f)), std::max(1.f, SF(4.f)));
        Txt(g, m_dev.present ? L"EDL  /  设备已连接" : L"EDL  /  等待 9008 设备",
            { badge.x + 22, badge.y, badge.w - 28, badge.h }, m_fonts.tiny.get(),
            m_dev.present ? 0x9FF0D1 : 0xA8C8F0, 255, 0);
    }

    std::wstring title;
    if (m_running) title = L"正在向设备写入固件";
    else if (m_done) title = L"刷写完成，设备可重启";
    else if (m_failed) title = L"写入失败，请检查连接";
    else if (m_scanning) title = L"正在扫描 EDL 设备";
    else if (m_dev.present && m_fw.valid) title = L"设备已连接，准备写入";
    else if (m_dev.present) title = L"设备已连接，请载入固件";
    else title = L"连接设备，开始刷机";
    Txt(g, title, { h.x + 28, h.y + 51, std::max(250, l.mainW - 300), 38 },
        m_fonts.title.get(), col::ink, 255, 0);

    std::wstring sub;
    if (m_running) {
        std::wstring part = (m_currentPart >= 0 && m_currentPart < (int)m_fw.parts.size())
                            ? m_fw.parts[(size_t)m_currentPart].label
                            : std::wstring(L"Firehose");
        sub = L"保持 USB 连接 · 正在写入 " + part;
    } else if (m_done) sub = L"所有目标分区已写入完成";
    else if (m_failed) sub = L"写入进程异常，请查看运行日志";
    else if (m_scanning) sub = L"枚举 USB 总线，匹配 VID_05C6 / PID_9008";
    else if (m_dev.present && m_fw.valid) sub = m_dev.name + L"  ·  " + m_dev.port;
    else if (m_dev.present) sub = L"设备已连接 · 请选择固件目录";
    else sub = L"连接 9008 设备并载入固件包";
    Txt(g, sub, { h.x + 28, h.y + 92, std::max(240, l.mainW - 300), 18 },
        m_fonts.small.get(), 0x8EA4C2, 255, 0);

    // metrics
    {
        int y = h.y + 183;
        struct Metric { const wchar_t* label; std::wstring value; DWORD color; };
        Metric m[3];
        m[0] = { L"设备", m_dev.present ? L"QDLoader 9008" : L"未连接", m_dev.present ? col::green : 0x7F91AC };
        m[1] = { L"端口", m_dev.present ? m_dev.port : L"--", 0xC7D8EE };
        m[2] = { L"负载", std::to_wstring(m_fw.parts.size()) + L" 分区 · " + FormatSize(m_fw.totalBytes), 0xB9CCE6 };
        for (int i = 0; i < 3; ++i) {
            int x = h.x + 28 + i * 138;
            Txt(g, m[i].label, { x, y, 120, 12 }, m_fonts.tiny.get(), 0x5E7390, 255, 0);
            Txt(g, m[i].value, { x, y + 13, 132, 15 }, m_fonts.small.get(), m[i].color, 255, 0);
        }
    }

    PaintHeroArt(g);
    PaintStage(g);
    PaintFirmwareCard(g);
    PaintHealth(g);
    PaintOptions(g);
    PaintDeviceCard(g);
    PaintActionBar(g);
}

// ===========================================================================
//  stage
// ===========================================================================
void AppState::PaintStage(Gdiplus::Graphics& g)
{
    const LR& r = m_lay.stage;
    Txt(g, L"刷写进度", { r.x + 22, r.y + 17, 160, 22 }, m_fonts.h2.get(), 0xDDEBFF, 255, 0);
    Txt(g, L"FIRMWARE PIPELINE", { r.x + 22, r.y + 37, 220, 12 },
        m_fonts.eyebrow.get(), 0x506783, 255, 0);

    DWORD pcol = m_failed ? col::red : (m_done ? col::green : m_accent);
    Txt(g, (m_running && !m_hasProgress) ? L"--" : PercentText(m_display),
        { r.r() - 130, r.y + 12, 108, 32 },
        m_fonts.numL.get(), m_failed ? col::red : pcol, 255, 2);

    int active = -1;
    if (m_done) active = 4;
    else if (m_running) {
        active = m_stage == 0 ? 0 : m_stage == 1 ? 1 : m_stage == 2 ? 2 : 3;
    } else if (m_scanning) {
        active = 0;
    }

    static const wchar_t* names[4] = { L"连接设备", L"引导握手", L"写入分区", L"校验重启" };
    static const wchar_t* desc[4] = { L"QDLoader 9008", L"Sahara / Firehose", L"UFS 顺序写入", L"GPT / SHA-256" };
    int s0 = r.x + 82;
    int s1 = r.r() - 82;
    int sy = r.y + 66;

    // connecting line
    Gdiplus::Pen base(argb(235, 0x28405F), std::max(1.f, SF(2.4f)));
    base.SetStartCap(Gdiplus::LineCapRound);
    base.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&base, (Gdiplus::REAL)X(s0), (Gdiplus::REAL)Y(sy),
               (Gdiplus::REAL)X(s1), (Gdiplus::REAL)Y(sy));

    float stepFill = 0.f;
    if (active > 0) stepFill = std::min(1.f, (float)active / 3.f);
    if (active >= 4) stepFill = 1.f;
    if (stepFill > 0.001f) {
        Gdiplus::LinearGradientBrush line(
            Gdiplus::PointF((Gdiplus::REAL)X(s0), 0),
            Gdiplus::PointF((Gdiplus::REAL)X(s1), 0),
            rgb(m_accent), rgb(m_done ? col::green : m_accent2));
        Gdiplus::Pen lp(&line, std::max(1.f, SF(2.2f)));
        lp.SetStartCap(Gdiplus::LineCapRound);
        lp.SetEndCap(Gdiplus::LineCapRound);
        float ex = (float)X(s0) + (float)(X(s1) - X(s0)) * stepFill;
        g.DrawLine(&lp, (Gdiplus::REAL)X(s0), (Gdiplus::REAL)Y(sy), ex, (Gdiplus::REAL)Y(sy));
        // travelling pulse
        if (active >= 0 && active < 4 && m_animEnabled) {
            float t = std::fmod(m_time * 0.55f, 1.f);
            float px = (float)X(s0) + (float)(X(s1) - X(s0)) * (stepFill * t + (1.f - t) * stepFill * 0.15f);
            Gdiplus::SolidBrush pb(argb(160, pcol));
            g.FillEllipse(&pb, px - SF(2.f), (Gdiplus::REAL)Y(sy) - SF(2.f), SF(4.f), SF(4.f));
        }
    }

    for (int i = 0; i < 4; ++i) {
        int cx = s0 + (s1 - s0) * i / 3;
        bool done = (active > i) || (m_done);
        bool now = (active == i) && !m_done;
        DWORD c = done ? col::green : now ? pcol : 0x53657F;

        if (now && m_animEnabled) {
            float pulse = 0.5f + 0.5f * std::sin(m_time * 4.2f);
            Glow(g, cx, sy, 32, c, 22 + (int)(pulse * 14), 7);
        }
        Gdiplus::SolidBrush fill(argb(done ? 52 : now ? 42 : 18, c));
        g.FillEllipse(&fill, (Gdiplus::REAL)X(cx) - SF(12.f), (Gdiplus::REAL)Y(sy) - SF(12.f),
                      SF(24.f), SF(24.f));
        Gdiplus::Pen ring(rgb(c), std::max(1.f, SF(done || now ? 1.8f : 1.1f)));
        g.DrawEllipse(&ring, (Gdiplus::REAL)X(cx) - SF(12.f), (Gdiplus::REAL)Y(sy) - SF(12.f),
                      SF(24.f), SF(24.f));
        if (done) {
            Gdiplus::Pen check(rgb(col::green), std::max(1.f, SF(2.f)));
            check.SetStartCap(Gdiplus::LineCapRound);
            check.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&check, (Gdiplus::REAL)X(cx - 5), (Gdiplus::REAL)Y(sy),
                       (Gdiplus::REAL)X(cx - 1), (Gdiplus::REAL)Y(sy + 4));
            g.DrawLine(&check, (Gdiplus::REAL)X(cx - 1), (Gdiplus::REAL)Y(sy + 4),
                       (Gdiplus::REAL)X(cx + 6), (Gdiplus::REAL)Y(sy - 4));
        } else if (now) {
            Gdiplus::SolidBrush db(rgb(c));
            g.FillEllipse(&db, (Gdiplus::REAL)X(cx) - SF(4.f), (Gdiplus::REAL)Y(sy) - SF(4.f),
                          SF(8.f), SF(8.f));
        } else {
            Txt(g, std::to_wstring(i + 1), { cx - 12, sy - 12, 24, 24 },
                m_fonts.numSm.get(), 0x748AA6, 255, 1);
        }
        Txt(g, names[i], { cx - 62, sy + 15, 124, 16 }, m_fonts.small.get(),
            done ? 0xBDEBD7 : now ? 0xE0EDFF : 0x657A96, 255, 1);
        Txt(g, desc[i], { cx - 72, sy + 31, 144, 12 }, m_fonts.tiny.get(),
            now ? 0x7EA9D7 : 0x4D5F76, 255, 1);
    }

    // progress bar
    LR track = { r.x + 24, r.y + 127, r.w - 48, 8 };
    FillRound(g, track, 0x142138, 4, 255);
    int fillW = (int)((float)track.w * m_display);
    if (fillW > 1) {
        LR fill = { track.x, track.y, fillW, track.h };
        Gdiplus::GraphicsState st = g.Save();
        Gdiplus::GraphicsPath clip;
        buildRoundPath(clip, (Gdiplus::REAL)X(track.x), (Gdiplus::REAL)Y(track.y),
                       (Gdiplus::REAL)S(track.w), (Gdiplus::REAL)S(track.h), (Gdiplus::REAL)S(4));
        g.SetClip(&clip, Gdiplus::CombineModeIntersect);
        Gdiplus::LinearGradientBrush b(
            Gdiplus::PointF((Gdiplus::REAL)X(track.x), 0),
            Gdiplus::PointF((Gdiplus::REAL)X(track.r()), 0),
            rgb(m_done ? col::green : m_accent),
            rgb(m_done ? 0x8AF3C8 : m_accent2));
        g.FillRectangle(&b, R(fill));
        if (m_animEnabled && (m_running || m_scanning)) {
            float p = std::fmod(m_time * 0.8f, 1.f);
            int px = track.x + (int)(p * track.w);
            Gdiplus::LinearGradientBrush shine(
                Gdiplus::PointF((Gdiplus::REAL)X(px - 26), 0),
                Gdiplus::PointF((Gdiplus::REAL)X(px + 26), 0),
                argb(0, 0xFFFFFF), argb(42, 0xFFFFFF));
            g.FillRectangle(&shine, (Gdiplus::REAL)X(track.x), (Gdiplus::REAL)Y(track.y),
                            (Gdiplus::REAL)S(fillW), (Gdiplus::REAL)S(track.h));
        }
        g.Restore(st);
    }

    std::wstring left = m_statusMain;
    if (m_running && m_currentPart >= 0 && m_currentPart < (int)m_fw.parts.size())
        left = L"正在写入  " + m_fw.parts[(size_t)m_currentPart].label;
    Txt(g, left, { r.x + 24, r.y + 142, r.w - 190, 16 }, m_fonts.small.get(),
        m_running || m_scanning ? 0xC6D9F2 : 0x7D91AD, 255, 0);
    std::wstring right = std::to_wstring(std::max(0, m_currentPart + (m_running ? 1 : 0))) +
                         L" / " + std::to_wstring(m_fw.parts.size()) + L"  分区";
    Txt(g, right, { r.r() - 180, r.y + 142, 156, 16 }, m_fonts.tiny.get(),
        0x6E839F, 255, 2);
}
