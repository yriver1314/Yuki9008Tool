#include "app_state.h"

// ===========================================================================
//  firmware preview card
// ===========================================================================
void AppState::PaintFirmwareCard(Gdiplus::Graphics& g)
{
    const LR& r = m_lay.firmware;
    Txt(g, L"固件包", { r.x + 22, r.y + 15, 120, 22 }, m_fonts.h2.get(), 0xDDEBFF, 255, 0);

    LR badge = { r.x + 76, r.y + 18, 56, 17 };
    DWORD bc = m_fw.valid ? col::green : 0x5C6B82;
    FillRound(g, badge, bc, 8, m_fw.valid ? 30 : 20);
    Txt(g, m_fw.valid ? L"LOCAL" : L"EMPTY", { badge.x, badge.y, badge.w, badge.h },
        m_fonts.tiny.get(), bc, 255, 1);

    std::wstring path = m_fw.valid ? m_fw.root : L"尚未载入固件包 · 点击「更换」选择目录";
    Txt(g, path, { r.x + 22, r.y + 39, r.w - 128, 15 }, m_fonts.monoS.get(),
        m_fw.valid ? 0x768CA8 : 0x5D708B, 255, 0);

    // chips
    struct Chip { std::wstring t; int w; };
    Chip chips[3] = {
        { L"XML  " + std::to_wstring(m_fw.rawCount), 76 },
        { std::to_wstring(m_fw.parts.size()) + L" 个分区", 82 },
        { FormatSize(m_fw.totalBytes), 86 },
    };
    int cx = r.x + 22;
    for (int i = 0; i < 3; ++i) {
        LR c = { cx, r.y + 58, chips[i].w, 18 };
        FillRound(g, c, 0x0E1B2C, 9, 255);
        StrokeRound(g, c, 0x203653, 9, 1.f, 220);
        Txt(g, chips[i].t, { c.x + 8, c.y, c.w - 14, c.h }, m_fonts.tiny.get(),
            0x8EA6C4, 255, 0);
        cx += chips[i].w + 7;
    }

    Hairline(g, r.x + 22, r.y + 83, r.r() - 22, r.y + 83, 0x1A2A41, 190);

    // partition preview table
    int y = r.y + 88;
    int rowh = 22;
    Txt(g, L"分区", { r.x + 22, y, 110, 13 }, m_fonts.tiny.get(), 0x536A87, 255, 0);
    Txt(g, L"镜像", { r.x + 136, y, 220, 13 }, m_fonts.tiny.get(), 0x536A87, 255, 0);
    Txt(g, L"大小", { r.r() - 134, y, 112, 13 }, m_fonts.tiny.get(), 0x536A87, 255, 2);
    y += 16;

    int maxRows = std::max(1, std::min(10, (r.h - (y - r.y) - 8) / rowh));
    int start = 0;
    if (m_running && m_currentPart >= 0)
        start = std::max(0, std::min((int)m_fw.parts.size() - maxRows, m_currentPart - 1));
    for (int i = 0; i < maxRows && start + i < (int)m_fw.parts.size(); ++i) {
        int idx = start + i;
        const PartInfo& p = m_fw.parts[(size_t)idx];
        bool cur = (idx == m_currentPart && (m_running || m_done));
        if (cur) FillRound(g, { r.x + 18, y - 1, r.w - 36, rowh - 2 }, m_accent, 5, 24);
        Txt(g, p.label, { r.x + 22, y, 110, rowh }, m_fonts.small.get(),
            cur ? 0xFFFFFF : 0xC3D4E9, 255, 0);
        Txt(g, p.file, { r.x + 136, y, 220, rowh }, m_fonts.monoS.get(),
            cur ? 0x9EC8FF : 0x6F86A3, 255, 0);
        Txt(g, FormatSize(p.bytes), { r.r() - 134, y, 112, rowh }, m_fonts.monoS.get(),
            cur ? 0x9EC8FF : 0x7188A6, 255, 2);
        y += rowh;
    }
    if (m_fw.parts.empty()) {
        Txt(g, L"尚未加载固件包", { r.x + 22, y, r.w - 44, 20 }, m_fonts.small.get(),
            0x6C809C, 255, 0);
    } else if ((int)m_fw.parts.size() > start + maxRows) {
        Txt(g, L"还有 " + std::to_wstring((int)m_fw.parts.size() - start - maxRows) +
                L" 个分区  ·  在「分区浏览」查看全部",
            { r.r() - 300, r.b() - 20, 278, 14 }, m_fonts.tiny.get(), 0x4F6B8A, 255, 2);
    }
}

// ===========================================================================
//  transfer health card
// ===========================================================================
void AppState::PaintHealth(Gdiplus::Graphics& g)
{
    const LR& r = m_lay.health;
    Txt(g, L"实时传输", { r.x + 20, r.y + 15, 130, 22 }, m_fonts.h2.get(), 0xDDEBFF, 255, 0);

    // live / state chip
    DWORD liveCol = m_running ? m_accent : m_done ? col::green :
                    m_failed ? col::red : 0x566A85;
    const wchar_t* liveText = m_running ? L"LIVE" : m_done ? L"DONE" :
                              m_failed ? L"ERR" : L"IDLE";
    LR live = { r.r() - 66, r.y + 18, 46, 17 };
    FillRound(g, live, liveCol, 8, 26);
    float pulse = 0.5f + 0.5f * std::sin(m_time * 3.4f);
    Gdiplus::SolidBrush ld(argb((BYTE)(120 + pulse * 120), liveCol));
    g.FillEllipse(&ld, (Gdiplus::REAL)X(live.x + 7), (Gdiplus::REAL)Y(live.cy() - 1.5f),
                  std::max(1.f, SF(3.f)), std::max(1.f, SF(3.f)));
    Txt(g, liveText, { live.x + 13, live.y, 30, live.h }, m_fonts.monoS.get(),
        m_running ? 0xBFE0FF : m_done ? 0x9FF0D1 : m_failed ? 0xFFB8C2 : 0x9FB2CA,
        255, 0);

    int cx = r.x + r.w / 2;
    int cy = r.y + 103;
    int rad = 49;
    DWORD pc = m_done ? col::green : m_failed ? col::red :
               m_running ? m_accent : 0x5C6B82;

    // tick ring
    for (int i = 0; i < 48; ++i) {
        float a = (float)i * 6.2831853f / 48.f - 1.5707963f;
        float r1 = SF((float)rad + 8);
        float r2 = SF((float)rad + ((i % 6 == 0) ? 13.f : 11.f));
        int alpha = (i < (int)(m_display * 48.f)) ? 130 : 35;
        Gdiplus::Pen pen(argb((BYTE)alpha, i < (int)(m_display * 48.f) ? pc : 0x4D6585),
                         std::max(1.f, SF(1.f)));
        pen.SetStartCap(Gdiplus::LineCapRound);
        g.DrawLine(&pen, (Gdiplus::REAL)cx + std::cos(a) * r1,
                   (Gdiplus::REAL)cy + std::sin(a) * r1,
                   (Gdiplus::REAL)cx + std::cos(a) * r2,
                   (Gdiplus::REAL)cy + std::sin(a) * r2);
    }

    {
        float rr = SF((float)rad);
        Gdiplus::Pen track(rgb(0x14223A), std::max(2.f, SF(8.f)));
        track.SetStartCap(Gdiplus::LineCapRound);
        track.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&track, (Gdiplus::REAL)X(cx) - rr, (Gdiplus::REAL)Y(cy) - rr,
                  rr * 2, rr * 2, 0.f, 360.f);

        Gdiplus::Pen glow(argb(60, pc), std::max(3.f, SF(13.f)));
        glow.SetStartCap(Gdiplus::LineCapRound);
        glow.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&glow, (Gdiplus::REAL)X(cx) - rr, (Gdiplus::REAL)Y(cy) - rr,
                  rr * 2, rr * 2, -90.f, std::min(359.9f, m_display * 360.f));

        Gdiplus::Pen arc(rgb(pc), std::max(2.f, SF(8.f)));
        arc.SetStartCap(Gdiplus::LineCapRound);
        arc.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&arc, (Gdiplus::REAL)X(cx) - rr, (Gdiplus::REAL)Y(cy) - rr,
                  rr * 2, rr * 2, -90.f, std::min(359.9f, m_display * 360.f));

        Gdiplus::SolidBrush inner(rgb(0x07101C));
        g.FillEllipse(&inner, (Gdiplus::REAL)X(cx) - rr + SF(13.f),
                      (Gdiplus::REAL)Y(cy) - rr + SF(13.f),
                      (rr - SF(13.f)) * 2, (rr - SF(13.f)) * 2);
        Gdiplus::Pen innerEdge(argb(100, 0x1D3350), 1.f);
        g.DrawEllipse(&innerEdge, (Gdiplus::REAL)X(cx) - rr + SF(13.f),
                      (Gdiplus::REAL)Y(cy) - rr + SF(13.f),
                      (rr - SF(13.f)) * 2, (rr - SF(13.f)) * 2);

        if ((m_running || m_scanning) && m_animEnabled) {
            float a = m_time * 1.5f - 1.5707963f;
            float nx = (float)X(cx) + std::cos(a) * rr;
            float ny = (float)Y(cy) + std::sin(a) * rr;
            Glow(g, (int)((nx - X(0)) * 96.f / std::max(1, m_app.dpi())),
                 (int)((ny - Y(0)) * 96.f / std::max(1, m_app.dpi())), 18, pc, 80, 5);
            Gdiplus::SolidBrush nb(rgb(0xFFFFFF));
            g.FillEllipse(&nb, nx - SF(2.5f), ny - SF(2.5f), SF(5.f), SF(5.f));
        }
    }

    Txt(g, (m_running && !m_hasProgress) ? L"--" : PercentText(m_display),
        { cx - 62, cy - 26, 124, 38 }, m_fonts.numL.get(),
        m_done ? col::green : m_failed ? col::red : 0xFFFFFF, 255, 1);
    Txt(g, L"总进度", { cx - 50, cy + 12, 100, 12 }, m_fonts.tiny.get(), 0x6E829F, 255, 1);

    // metrics
    int my = r.y + 165;
    struct Met { const wchar_t* label; std::wstring value; };
    Met met[3] = {
        { L"速度", SpeedText() },
        { L"已写入", FormatSize((uint64_t)(m_writtenMB * 1024.0 * 1024.0)) },
        { L"剩余", m_remaining > 0.f ? std::to_wstring((int)m_remaining) + L"s" : L"--" },
    };
    int xs[3] = { r.x + 17, r.x + 99, r.x + 181 };
    for (int i = 0; i < 3; ++i) {
        if (i > 0) {
            Gdiplus::Pen sep(argb(100, 0x1A2A41), 1.f);
            g.DrawLine(&sep, (Gdiplus::REAL)X(xs[i] - 10), (Gdiplus::REAL)Y(my),
                       (Gdiplus::REAL)X(xs[i] - 10), (Gdiplus::REAL)Y(my + 34));
        }
        Txt(g, met[i].label, { xs[i], my, 70, 12 }, m_fonts.tiny.get(), 0x556B87, 255, 0);
        Txt(g, met[i].value, { xs[i], my + 13, 76, 16 }, m_fonts.numSm.get(),
            m_running ? 0xC8DCF5 : 0x8196B2, 255, 0);
    }

    // micro sparkline behind metrics
    if (m_running && m_speedHist.size() > 2) {
        float maxV = 1.f;
        for (float v : m_speedHist) maxV = std::max(maxV, v);
        Gdiplus::GraphicsPath p;
        int n = (int)m_speedHist.size();
        float prevX = 0.f, prevY = 0.f;
        for (int i = 0; i < n; ++i) {
            float xx = (float)X(r.x + 17) + (float)SF(246.f) * i / std::max(1, n - 1);
            float yy = (float)Y(r.y + 202) - SF(16.f) * (m_speedHist[(size_t)i] / maxV);
            if (i == 0) {
                p.StartFigure();
                prevX = xx;
                prevY = yy;
            } else {
                p.AddLine((Gdiplus::REAL)prevX, (Gdiplus::REAL)prevY,
                          (Gdiplus::REAL)xx, (Gdiplus::REAL)yy);
                prevX = xx;
                prevY = yy;
            }
        }
        Gdiplus::Pen sp(argb(55, m_accent), std::max(1.f, SF(1.f)));
        g.DrawPath(&sp, &p);
    }
}

// ===========================================================================
//  options
// ===========================================================================
void AppState::PaintOptions(Gdiplus::Graphics& g)
{
    const LR& r = m_lay.options;
    Txt(g, L"刷写选项", { r.x + 20, r.y + 15, 130, 22 }, m_fonts.h2.get(), 0xDDEBFF, 255, 0);

    LR safe = { r.r() - 58, r.y + 18, 38, 17 };
    FillRound(g, safe, col::green, 8, 22);
    Txt(g, L"SAFE", { safe.x, safe.y, safe.w, safe.h }, m_fonts.tiny.get(), col::green, 255, 1);

    Hairline(g, r.x + 20, r.y + 171, r.r() - 20, r.y + 171, 0x1A2A41, 170);

    const wchar_t* note = L"请选择固件包并连接设备";
    DWORD nc = col::amber;
    DWORD nfg = 0xC6A477;
    if (m_running) {
        note = L"真实写入进行中 · 请保持 USB 连接";
        nc = m_accent;
        nfg = 0x9FC3EA;
    } else if (m_failed) {
        note = L"写入失败 · 请查看运行日志";
        nc = col::red;
        nfg = 0xFFB0BC;
    } else if (!m_engineReady) {
        note = L"未检测到 QFIL / fh_loader 写入引擎";
        nc = col::red;
        nfg = 0xF2A9B4;
    } else if (m_fw.valid && m_fw.programmer.empty()) {
        note = L"固件缺少 Firehose 编程器";
        nc = col::red;
        nfg = 0xF2A9B4;
    } else if (m_dev.present && m_dev.port.rfind(L"COM", 0) != 0) {
        note = L"设备未暴露 QDLoader COM 端口";
        nc = col::red;
        nfg = 0xF2A9B4;
    } else if (m_fw.valid && m_dev.present) {
        note = L"写入引擎与设备均已就绪";
        nc = col::green;
        nfg = 0x8DD9BE;
    } else if (m_fw.valid) {
        note = L"固件已加载 · 连接 9008 设备";
        nc = col::amber;
        nfg = 0xC6A477;
    }
    Gdiplus::SolidBrush dot(rgb(nc));
    g.FillEllipse(&dot, (Gdiplus::REAL)X(r.x + 22), (Gdiplus::REAL)Y(r.y + 188),
                  std::max(1.f, SF(5.f)), std::max(1.f, SF(5.f)));
    Txt(g, note, { r.x + 34, r.y + 180, r.w - 48, 18 }, m_fonts.tiny.get(),
        nfg, 255, 0);

    Txt(g, L"userdata 默认保留，擦除操作不可恢复",
        { r.x + 22, r.y + 200, r.w - 44, 14 }, m_fonts.tiny.get(), 0x50647F, 255, 0);
}

// ===========================================================================
//  device info
// ===========================================================================
void AppState::PaintDeviceCard(Gdiplus::Graphics& g)
{
    const LR& r = m_lay.device;
    Txt(g, L"设备信息", { r.x + 20, r.y + 13, 120, 20 }, m_fonts.h2.get(), 0xDDEBFF, 255, 0);

    LR badge = { r.r() - 66, r.y + 15, 50, 17 };
    DWORD bc = m_dev.present ? col::green : 0x566A85;
    FillRound(g, badge, bc, 8, m_dev.present ? 24 : 18);
    Txt(g, m_dev.present ? L"ONLINE" : L"OFFLINE", { badge.x, badge.y, badge.w, badge.h },
        m_fonts.tiny.get(), bc, 255, 1);

    struct Row { const wchar_t* k; std::wstring v; DWORD c; };
    Row rows[4] = {
        { L"连接状态", m_dev.present ? L"已连接" : L"未连接",
          (DWORD)(m_dev.present ? 0x9EEBD2 : 0x8A9AB3) },
        { L"端口", m_dev.present ? m_dev.port : L"--", 0xB9CCE4 },
        { L"协议", L"Qualcomm QDLoader 9008", 0xB9CCE4 },
        { L"序列号", m_dev.present ? m_dev.serial : L"—", 0x7187A3 },
    };
    int maxRows = r.h < 128 ? 3 : 4;
    int y = r.y + 40;
    for (int i = 0; i < maxRows; ++i) {
        Txt(g, rows[i].k, { r.x + 20, y, 70, 20 }, m_fonts.tiny.get(), 0x5D728E, 255, 0);
        Txt(g, rows[i].v, { r.x + 92, y, r.w - 112, 20 }, m_fonts.small.get(),
            rows[i].c, 255, 2);
        if (i < maxRows - 1)
            Hairline(g, r.x + 20, y + 22, r.r() - 20, y + 22, 0x16263D, 120);
        y += 24;
    }
}

// ===========================================================================
//  action bar
// ===========================================================================
void AppState::PaintActionBar(Gdiplus::Graphics& g)
{
    const LR& r = m_lay.action;
    int dotX = r.x + 24, dotY = r.cy();
    DWORD c = m_statusColor;
    float pulse = 0.5f + 0.5f * std::sin(m_time * 3.1f);
    if (m_running || m_scanning) {
        Gdiplus::SolidBrush halo(argb((BYTE)(35 + pulse * 55), c));
        float rr = SF(7.f + pulse * 4.f);
        g.FillEllipse(&halo, (Gdiplus::REAL)X(dotX) - rr, (Gdiplus::REAL)Y(dotY) - rr,
                      rr * 2, rr * 2);
    }
    Gdiplus::SolidBrush dot(rgb(c));
    g.FillEllipse(&dot, (Gdiplus::REAL)X(dotX) - SF(3.f), (Gdiplus::REAL)Y(dotY) - SF(3.f),
                  SF(6.f), SF(6.f));

    Txt(g, m_statusMain, { r.x + 40, r.y + 6, 430, 18 }, m_fonts.bodyBold.get(),
        0xE4EFFF, 255, 0);
    Txt(g, m_statusSub, { r.x + 40, r.y + 24, 470, 14 }, m_fonts.tiny.get(),
        0x7086A2, 255, 0);

    const bool canStart = m_fw.valid && !m_fw.programmer.empty() &&
                          m_dev.present && m_engineReady &&
                          m_dev.port.rfind(L"COM", 0) == 0;
    if (!m_running && !m_scanning && canStart) {
        Txt(g, L"CTRL + ENTER", { m_lay.action.r() - 470, r.y + 16, 110, 14 },
            m_fonts.monoS.get(), 0x4D617C, 255, 2);
    } else if (m_running) {
        // slim progress at bottom of bar
        LR track = { r.x + 40, r.b() - 8, r.w - 480, 3 };
        if (track.w > 10) {
            FillRound(g, track, 0x15243A, 2, 255);
            FillRound(g, { track.x, track.y, (int)(track.w * m_display), 3 }, m_accent, 2, 255);
        }
    }
}
