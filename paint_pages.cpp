#include "app_state.h"

// ===========================================================================
//  firmware management page — foreground
// ===========================================================================
void AppState::PaintFirmwarePage(Gdiplus::Graphics& g, bool bg)
{
    const LLayout& l = m_lay;
    const int fullW = l.R - l.L;
    LR drop = { l.L, 90, fullW, 200 };

    if (bg) {
        int tableH = std::max(150, l.actionY - 152 - 326);
        LR tablePanel = { l.L, 326, fullW, tableH };
        int detailY = 326 + tableH + 12;
        LR detail = { l.L, detailY, fullW, std::max(64, l.actionY - 58 - detailY) };
        Panel(g, drop, 0x0C1829, 0x09121E, 0x1C3354, 18, 255, true);
        Glow(g, drop.cx(), drop.y + 90, 220, m_accent, 10, 8);
        Panel(g, tablePanel, col::panelTop, col::panelBot, 0x1B2B43, 16, 255, true);
        Panel(g, detail, 0x0A1422, 0x080F1A, 0x1A2B44, 14, 255, false);
        return;
    }
    DWORD dropLine = m_hoverDrop ? m_accent : 0x244363;
    int icoCx = drop.cx();
    int icoCy = drop.y + 44;

    // upload icon
    Glow(g, icoCx, icoCy, 52, m_accent, 26, 8);
    {
        float s = SF(16.f);
        float x = (float)X(icoCx), y = (float)Y(icoCy);
        Gdiplus::Pen p(rgb(m_hoverDrop ? 0xFFFFFF : 0xA9C5E7), std::max(1.f, SF(1.6f)));
        p.SetStartCap(Gdiplus::LineCapRound);
        p.SetEndCap(Gdiplus::LineCapRound);
        p.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawRectangle(&p, x - s, y - s * 0.3f, s * 2, s * 1.3f);
        g.DrawLine(&p, x, y - s * 1.15f, x, y + s * 0.45f);
        g.DrawLine(&p, x, y - s * 1.15f, x - s * 0.38f, y - s * 0.68f);
        g.DrawLine(&p, x, y - s * 1.15f, x + s * 0.38f, y - s * 0.68f);
    }

    Txt(g, L"拖入固件目录，或点击此区域选择",
        { drop.x, drop.y + 80, drop.w, 24 }, m_fonts.h2.get(), 0xE5F0FF, 255, 1);
    Txt(g, L"包含 rawprogram*.xml · patch*.xml · prog_firehose*.elf / *.mbn",
        { drop.x, drop.y + 103, drop.w, 18 }, m_fonts.small.get(), 0x7D94B2, 255, 1);

    // format chips
    const wchar_t* chips[3] = { L"解压目录", L"QFIL 结构", L"SHA-256 指纹" };
    int chipW[3] = { 92, 96, 104 };
    int totalW = 0;
    for (int i = 0; i < 3; ++i) totalW += chipW[i] + (i ? 8 : 0);
    int chipX = drop.cx() - totalW / 2;
    for (int i = 0; i < 3; ++i) {
        LR c = { chipX, drop.y + 128, chipW[i], 20 };
        FillRound(g, c, 0x0D1B2D, 10, 255);
        StrokeRound(g, c, 0x24405E, 10, 1.f, 230);
        Txt(g, chips[i], { c.x, c.y, c.w, c.h }, m_fonts.tiny.get(), 0x849DBB, 255, 1);
        chipX += chipW[i] + 8;
    }

    // library title above table
    Txt(g, L"固件库", { l.L + 2, 298, 160, 22 }, m_fonts.h2.get(), 0xDDEBFF, 255, 0);
    Txt(g, m_fw.valid ? (L"1 个已加载负载  ·  " + FormatSize(m_fw.totalBytes))
                      : L"未加载固件包",
        { l.R - 300, 300, 298, 18 }, m_fonts.small.get(), 0x6B809D, 255, 2);

    int tableH = std::max(150, l.actionY - 152 - 326);
    {
        int emptyY = 326 + tableH - 58;
        Txt(g, m_fw.valid ? L"选择新的固件目录，可替换当前负载并重新生成映射"
                          : L"选择固件目录后，这里会显示分区映射",
            { l.L + 24, emptyY, fullW - 48, 18 }, m_fonts.small.get(), 0x465D79, 255, 1);
        Txt(g, L"RAWPROGRAM  ·  PATCH  ·  FIREHOSE  ·  GPT",
            { l.L + 24, emptyY + 17, fullW - 48, 12 }, m_fonts.eyebrow.get(),
            0x34465C, 255, 1);
    }
    int detailY = 326 + tableH + 12;
    LR detail = { l.L, detailY, fullW, std::max(64, l.actionY - 58 - detailY) };
    int half = detail.w / 2;
    Hairline(g, detail.x + half, detail.y + 12, detail.x + half, detail.b() - 12, 0x1A2B44, 150);
    Hairline(g, detail.x + 18, detail.cy(), detail.r() - 18, detail.cy(), 0x1A2B44, 120);

    struct Field { const wchar_t* label; std::wstring value; };
    std::wstring sha = m_fw.hash;
    if (sha.size() > 26) sha = sha.substr(0, 26) + L"…";
    Field fields[4] = {
        { L"FIREHOSE 编程器", m_fw.programmer.empty() ? L"—" : FileName(m_fw.programmer) },
        { L"MAPPING XML", std::to_wstring(m_fw.rawCount) + L" 个 rawprogram" },
        { L"SHA-256 指纹", sha.empty() ? L"—" : sha },
        { L"结构统计", std::to_wstring(m_fw.parts.size()) + L" 个分区  ·  " +
                        FormatSize(m_fw.totalBytes) },
    };
    for (int i = 0; i < 4; ++i) {
        int col = i % 2;
        int row = i / 2;
        int x = detail.x + 20 + col * half;
        int y = detail.y + 10 + row * 30;
        Txt(g, fields[i].label, { x, y, half - 40, 12 }, m_fonts.tiny.get(), 0x586F8B, 255, 0);
        Txt(g, fields[i].value, { x, y + 12, half - 40, 16 }, m_fonts.monoS.get(),
            i == 2 ? 0x88A9CE : 0xB2C5DD, 255, 0);
    }

    // bottom meta（放在操作按钮右侧，避免覆盖按钮）
    Txt(g, L"拖拽文件夹到窗口任意位置也可以直接载入  ·  UFS / eMMC",
        { l.L + 430, l.actionY - 33, l.R - l.L - 444, 18 }, m_fonts.tiny.get(),
        0x4E617A, 255, 0);

    // subtle drop border accent
    LR inner = { drop.x + 7, drop.y + 7, drop.w - 14, drop.h - 14 };
    Gdiplus::GraphicsPath path;
    buildRoundPath(path, (Gdiplus::REAL)X(inner.x), (Gdiplus::REAL)Y(inner.y),
                   (Gdiplus::REAL)S(inner.w), (Gdiplus::REAL)S(inner.h), (Gdiplus::REAL)S(14));
    Gdiplus::Pen pen(argb(m_hoverDrop ? 190 : 100, dropLine), std::max(1.f, SF(1.2f)));
    Gdiplus::REAL dash[2] = { 5.f, 4.f };
    pen.SetDashPattern(dash, 2);
    g.DrawPath(&pen, &path);
}

// ===========================================================================
//  partition browser page — foreground
// ===========================================================================
void AppState::PaintPartitionsPage(Gdiplus::Graphics& g, bool bg)
{
    const LLayout& l = m_lay;
    LR cards[3] = {
        { l.L, 86, 156, 46 },
        { l.L + 168, 86, 156, 46 },
        { l.L + 336, 86, 156, 46 },
    };
    if (bg) {
        for (int i = 0; i < 3; ++i)
            Panel(g, cards[i], 0x0C1625, 0x09111C, 0x1A2B44, 12, 255, false);
        LR tablePanel = { l.L, 146, l.R - l.L, std::max(160, l.actionY - 56 - 146) };
        Panel(g, tablePanel, col::panelTop, col::panelBot, 0x1B2B43, 16, 255, true);
        Hairline(g, l.L + 18, l.actionY - 52, l.R - 18, l.actionY - 52, 0x1D2E46, 170);
        return;
    }
    struct Card { const wchar_t* label; std::wstring value; DWORD color; };
    Card c[3] = {
        { L"分区总数", std::to_wstring(m_fw.parts.size()), 0xDCEBFF },
        { L"负载大小", FormatSize(m_fw.totalBytes), 0xDCEBFF },
        { L"写入队列", !m_fw.valid ? L"无负载" :
                       m_failed ? L"写入失败" :
                       m_done ? L"全部完成" : (m_running ? L"写入中" : L"待开始"),
          !m_fw.valid ? 0x8A9AB3 :
          m_failed ? col::red :
          m_done ? col::green : (m_running ? m_accent : col::amber) },
    };
    for (int i = 0; i < 3; ++i) {
        LR r = cards[i];
        Txt(g, c[i].label, { r.x + 14, r.y + 7, 80, 12 }, m_fonts.tiny.get(), 0x5B718D, 255, 0);
        Txt(g, c[i].value, { r.x + 14, r.y + 20, r.w - 28, 20 }, m_fonts.numSm.get(),
            c[i].color, 255, 0);
        if (i == 1) {
            Gdiplus::SolidBrush dot(argb(100, m_accent));
            g.FillEllipse(&dot, (Gdiplus::REAL)X(r.x + r.w - 20),
                          (Gdiplus::REAL)Y(r.y + 22), std::max(1.f, SF(4.f)),
                          std::max(1.f, SF(4.f)));
        }
    }

    Txt(g, m_fw.valid ? (L"共 " + std::to_wstring(m_fw.parts.size()) +
                          L" 个分区  ·  按 Firehose 顺序写入")
                      : L"尚未加载固件包",
        { l.L + 2, l.actionY - 32, 420, 18 }, m_fonts.tiny.get(), 0x536781, 255, 0);
    Txt(g, L"过滤结果实时更新",
        { l.R - 340, l.actionY - 32, 198, 18 }, m_fonts.tiny.get(), 0x536781, 255, 2);
}

// ===========================================================================
//  runtime log page — foreground
// ===========================================================================
void AppState::PaintLogPage(Gdiplus::Graphics& g, bool bg)
{
    const LLayout& l = m_lay;

    if (bg) {
        LR panel = { l.L, 90, l.R - l.L, std::max(220, l.H - 16 - 90) };
        Panel(g, panel, 0x0B1523, 0x080F1A, 0x1B2C45, 16, 255, true);
        Hairline(g, l.L + 18, 136, l.R - 18, 136, 0x1D2E46, 180);
        Gdiplus::LinearGradientBrush accent(
            Gdiplus::PointF((Gdiplus::REAL)X(l.L + 18), 0),
            Gdiplus::PointF((Gdiplus::REAL)X(l.R - 18), 0),
            argb(0, m_accent), argb(100, m_accent));
        g.FillRectangle(&accent, (Gdiplus::REAL)X(l.L + 18), (Gdiplus::REAL)Y(136),
                        (Gdiplus::REAL)S(l.R - l.L - 36), std::max(1.f, SF(1.f)));
        return;
    }

    Txt(g, L"会话输出", { l.L + 170, 99, 170, 22 }, m_fonts.h2.get(), 0xDDEBFF, 255, 0);
    Txt(g, L"实时追加  ·  最多 400 行",
        { l.L + 170, 117, 170, 14 }, m_fonts.tiny.get(), 0x5B708C, 255, 0);

    LR live = { l.L + 350, 103, 44, 16 };
    FillRound(g, live, col::green, 8, 22);
    Txt(g, L"LIVE", { live.x, live.y, live.w, live.h }, m_fonts.monoS.get(), col::green, 255, 1);
}

// ===========================================================================
//  settings page — foreground
// ===========================================================================
void AppState::PaintSettingsPage(Gdiplus::Graphics& g, bool bg)
{
    const LLayout& l = m_lay;
    const int fullW = l.R - l.L;
    LR left = { l.L, 90, 482, 400 };
    LR right = { l.L + 506, 90, fullW - 506, 400 };

    LR extra = { l.L, 506, fullW, std::max(100, l.H - 16 - 506) };
    if (bg) {
        Panel(g, left, 0x0D1727, 0x09111D, 0x1C2E48, 16, 255, true);
        Panel(g, right, 0x0D1727, 0x09111D, 0x1C2E48, 16, 255, true);
        Panel(g, extra, 0x0B1523, 0x08101B, 0x1A2B44, 14, 255, false);
        Glow(g, left.x + 200, 260, 180, m_accent, 10, 8);
        Glow(g, right.x + right.w / 2, 180, 180, m_accent2, 8, 8);
        return;
    }

    Txt(g, L"外观与动效", { left.x + 24, left.y + 20, 260, 24 }, m_fonts.h2.get(),
        0xE1EEFF, 255, 0);
    Txt(g, L"调整工作台的强调色与界面运动节奏",
        { left.x + 24, left.y + 45, 400, 16 }, m_fonts.small.get(), 0x7186A2, 255, 0);

    Txt(g, L"强调色", { left.x + 24, left.y + 76, 100, 14 }, m_fonts.tiny.get(),
        0x607692, 255, 0);

    static const DWORD accents[4] = { 0x4D9DFF, 0x9179FF, 0x34D6A0, 0xFFB454 };
    static const wchar_t* labels[4] = { L"冰蓝", L"紫罗兰", L"翡翠", L"琥珀" };
    for (int i = 0; i < 4; ++i) {
        LR r = AccentRect(i);
        bool sel = (accents[i] == m_accent);
        bool hover = (m_hoverAccent == i);
        if (sel || hover) Glow(g, r.cx(), r.cy(), 32, accents[i], sel ? 34 : 18, 7);

        Gdiplus::SolidBrush fill(argb(sel ? 70 : 34, accents[i]));
        g.FillEllipse(&fill, (Gdiplus::REAL)X(r.x), (Gdiplus::REAL)Y(r.y),
                      (Gdiplus::REAL)S(r.w), (Gdiplus::REAL)S(r.h));
        Gdiplus::Pen ring(rgb(accents[i]), std::max(1.f, SF(sel ? 2.2f : 1.2f)));
        g.DrawEllipse(&ring, (Gdiplus::REAL)X(r.x) + 1, (Gdiplus::REAL)Y(r.y) + 1,
                      (Gdiplus::REAL)S(r.w) - 2, (Gdiplus::REAL)S(r.h) - 2);
        Gdiplus::SolidBrush core(rgb(accents[i]));
        g.FillEllipse(&core, (Gdiplus::REAL)X(r.x + 13), (Gdiplus::REAL)Y(r.y + 13),
                      (Gdiplus::REAL)S(22), (Gdiplus::REAL)S(22));
        if (sel) {
            Gdiplus::Pen check(rgb(0xFFFFFF), std::max(1.4f, SF(1.8f)));
            check.SetStartCap(Gdiplus::LineCapRound);
            check.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&check, (Gdiplus::REAL)X(r.cx() - 5), (Gdiplus::REAL)Y(r.cy()),
                       (Gdiplus::REAL)X(r.cx() - 1), (Gdiplus::REAL)Y(r.cy() + 4));
            g.DrawLine(&check, (Gdiplus::REAL)X(r.cx() - 1), (Gdiplus::REAL)Y(r.cy() + 4),
                       (Gdiplus::REAL)X(r.cx() + 6), (Gdiplus::REAL)Y(r.cy() - 4));
        }
        Txt(g, labels[i], { r.x - 10, r.b() + 4, r.w + 20, 14 }, m_fonts.tiny.get(),
            sel ? 0xC9DCF4 : 0x657A96, 255, 1);
    }

    Hairline(g, left.x + 24, left.y + 174, left.r() - 24, left.y + 174, 0x1A2A41, 170);
    Txt(g, L"运动", { left.x + 24, left.y + 178, 100, 14 }, m_fonts.tiny.get(),
        0x607692, 255, 0);

    Txt(g, L"关于 Yuki9008", { right.x + 26, right.y + 20, 260, 24 }, m_fonts.h2.get(),
        0xE1EEFF, 255, 0);
    Txt(g, L"作者：yriver1314  ·  Qualcomm EDL Studio",
        { right.x + 26, right.y + 45, 380, 16 }, m_fonts.small.get(), 0x7186A2, 255, 0);

    // version block
    LR ver = { right.x + 26, right.y + 76, right.w - 52, 78 };
    Gdiplus::LinearGradientBrush b(
        Gdiplus::PointF((Gdiplus::REAL)X(ver.x), (Gdiplus::REAL)Y(ver.y)),
        Gdiplus::PointF((Gdiplus::REAL)X(ver.r()), (Gdiplus::REAL)Y(ver.b())),
        argb(30, m_accent), argb(10, m_accent2));
    Gdiplus::GraphicsPath vp;
    buildRoundPath(vp, (Gdiplus::REAL)X(ver.x), (Gdiplus::REAL)Y(ver.y),
                   (Gdiplus::REAL)S(ver.w), (Gdiplus::REAL)S(ver.h), (Gdiplus::REAL)S(12));
    g.FillPath(&b, &vp);
    Gdiplus::Pen vedge(argb(80, m_accent), 1.f);
    g.DrawPath(&vedge, &vp);
    Txt(g, L"Yuki9008工具", { ver.x + 16, ver.y + 10, 200, 20 }, m_fonts.bodyBold.get(),
        0xEDF5FF, 255, 0);
    Txt(g, L"v1.0.1  ·  BUILD 2026.10  ·  BY YRIVER1314", { ver.x + 16, ver.y + 31, 300, 16 },
        m_fonts.monoS.get(), 0x8EA6C7, 255, 0);
    Txt(g, L"YRGui 4.0  ·  MIT License", { ver.x + 16, ver.y + 49, 260, 16 },
        m_fonts.tiny.get(), 0x607793, 255, 0);
    LR tag = { ver.r() - 66, ver.y + 12, 48, 18 };
    FillRound(g, tag, m_accent, 9, 26);
    Txt(g, L"EDL", { tag.x, tag.y, tag.w, tag.h }, m_fonts.tiny.get(), m_accent, 255, 1);

    Txt(g, L"特性", { right.x + 26, right.y + 172, 80, 14 }, m_fonts.tiny.get(),
        0x607692, 255, 0);
    const wchar_t* features[4] = {
        L"QDLoader 9008 设备枚举与状态反馈",
        L"rawprogram / patch XML 结构解析",
        L"分区级进度、速度与写入状态",
        L"完整 Firehose 会话日志与导出",
    };
    for (int i = 0; i < 4; ++i) {
        int y = right.y + 194 + i * 21;
        Gdiplus::SolidBrush dot(rgb(m_accent));
        g.FillEllipse(&dot, (Gdiplus::REAL)X(right.x + 28), (Gdiplus::REAL)Y(y + 6),
                      std::max(1.f, SF(4.f)), std::max(1.f, SF(4.f)));
        Txt(g, features[i], { right.x + 42, y, right.w - 62, 16 }, m_fonts.small.get(),
            0x93A9C5, 255, 0);
    }

    // GitHub update channel
    {
        DWORD uc = m_updateState == UpdateState::Checking ? m_accent :
                   m_updateState == UpdateState::Available ? col::amber :
                   m_updateState == UpdateState::UpToDate ? col::green :
                   m_updateState == UpdateState::Failed ? col::red : 0x667C99;
        Gdiplus::SolidBrush dot(rgb(uc));
        g.FillEllipse(&dot, (Gdiplus::REAL)X(right.x + 26),
                      (Gdiplus::REAL)Y(right.y + 304), std::max(1.f, SF(5.f)),
                      std::max(1.f, SF(5.f)));
        Txt(g, m_updateMessage, { right.x + 38, right.y + 296, right.w - 62, 18 },
            m_fonts.small.get(), uc, 255, 0);
        Txt(g, L"github.com/yriver1314/Yuki9008Tool", { right.x + 26, right.y + 318,
                                                   right.w - 52, 14 },
            m_fonts.monoS.get(), 0x4E627E, 255, 0);
    }

    // bottom status / shortcut strip
    Txt(g, L"工作台信息", { extra.x + 22, extra.y + 14, 180, 20 }, m_fonts.h3.get(),
        0xDDEBFF, 255, 0);
    Txt(g, L"WORKSPACE RUNTIME", { extra.x + 22, extra.y + 31, 200, 12 },
        m_fonts.eyebrow.get(), 0x4F6581, 255, 0);

    int colW = (extra.w - 44) / 3;
    Txt(g, L"快捷操作", { extra.x + 22, extra.y + 52, colW, 12 }, m_fonts.tiny.get(),
        0x607692, 255, 0);
    Txt(g, L"F5  ·  重新扫描设备", { extra.x + 22, extra.y + 68, colW, 16 },
        m_fonts.monoS.get(), 0x8EA7C6, 255, 0);

    Txt(g, L"运行环境", { extra.x + 22 + colW, extra.y + 52, colW, 12 },
        m_fonts.tiny.get(), 0x607692, 255, 0);
    Txt(g, L"WINDOWS  ·  " + std::to_wstring(m_app.dpi()) + L" DPI  ·  YRGUI 4.0",
        { extra.x + 22 + colW, extra.y + 68, colW, 16 }, m_fonts.monoS.get(),
        0x8EA7C6, 255, 0);

    Txt(g, L"写入引擎", { extra.x + 22 + colW * 2, extra.y + 52, colW, 12 },
        m_fonts.tiny.get(), 0x607692, 255, 0);
    Gdiplus::SolidBrush ed(rgb(m_engineReady ? col::green : col::red));
    g.FillEllipse(&ed, (Gdiplus::REAL)X(extra.x + 22 + colW * 2),
                  (Gdiplus::REAL)Y(extra.y + 73), std::max(1.f, SF(4.f)),
                  std::max(1.f, SF(4.f)));
    Txt(g, m_engineReady ? L"QFIL READY  ·  FIREHOSE 2.0"
                         : (!m_engineSahara.empty() ? L"ENGINE INCOMPLETE  ·  FH_LOADER MISSING"
                                                    : L"ENGINE NOT FOUND"),
        { extra.x + 32 + colW * 2, extra.y + 64, colW - 12, 16 }, m_fonts.monoS.get(),
        m_engineReady ? 0x8DD9BE : 0xF2A9B4, 255, 0);

    if (extra.h > 175) {
        int y2 = extra.y + 98;
        Txt(g, L"渲染后端", { extra.x + 22, y2, colW, 12 }, m_fonts.tiny.get(),
            0x607692, 255, 0);
        Txt(g, L"GDI+  ·  DWM  ·  DOUBLE BUFFER",
            { extra.x + 22, y2 + 16, colW, 16 }, m_fonts.monoS.get(), 0x8EA7C6, 255, 0);
        Txt(g, L"界面帧率", { extra.x + 22 + colW, y2, colW, 12 }, m_fonts.tiny.get(),
            0x607692, 255, 0);
        Txt(g, L"30 FPS  ·  ADAPTIVE IDLE", { extra.x + 22 + colW, y2 + 16, colW, 16 },
            m_fonts.monoS.get(), 0x8EA7C6, 255, 0);
        Txt(g, L"会话状态", { extra.x + 22 + colW * 2, y2, colW, 12 }, m_fonts.tiny.get(),
            0x607692, 255, 0);
        const wchar_t* session = m_running ? L"WRITING  ·  BUSY" :
                                m_failed ? L"ERROR  ·  CHECK LOG" :
                                m_done ? L"DONE  ·  DEVICE REBOOT" :
                                m_dev.present ? L"READY  ·  DEVICE ATTACHED" :
                                L"READY  ·  NO DEVICE";
        Txt(g, session,
            { extra.x + 22 + colW * 2, y2 + 16, colW - 12, 16 }, m_fonts.monoS.get(),
            0x8EA7C6, 255, 0);
    }

    if (extra.h > 220) {
        Gdiplus::GraphicsPath wave;
        int wy = extra.y + 164;
        float px = 0.f, py = 0.f;
        for (int i = 0; i <= 80; ++i) {
            float t = (float)i / 80.f;
            float xx = (float)X(extra.x + 22) + (float)SF((float)(extra.w - 44)) * t;
            float yy = (float)Y(wy) + std::sin(t * 9.f + m_time * 0.7f) * SF(7.f) +
                       std::sin(t * 21.f - m_time * 0.4f) * SF(2.5f);
            if (i == 0) {
                wave.StartFigure();
                px = xx;
                py = yy;
            } else {
                wave.AddLine((Gdiplus::REAL)px, (Gdiplus::REAL)py,
                             (Gdiplus::REAL)xx, (Gdiplus::REAL)yy);
                px = xx;
                py = yy;
            }
        }
        Gdiplus::Pen wp(argb(70, m_accent), std::max(1.f, SF(1.2f)));
        wp.SetStartCap(Gdiplus::LineCapRound);
        wp.SetEndCap(Gdiplus::LineCapRound);
        g.DrawPath(&wp, &wave);
    }

    if (extra.h > 110) {
        Hairline(g, extra.x + 22, extra.b() - 28, extra.r() - 22, extra.b() - 28,
                 0x1A2B44, 140);
        Txt(g, L"当前构建提供完整可视化流程；真实写入需要接入 Firehose 后端。",
            { extra.x + 22, extra.b() - 22, extra.w - 44, 14 }, m_fonts.tiny.get(),
            0x536984, 255, 0);
    }
}

// ===========================================================================
//  confetti / transition
// ===========================================================================
void AppState::PaintConfetti(Gdiplus::Graphics& g)
{
    if (!m_animEnabled || m_confettiT > 4.6f) return;
    float fade = 1.f;
    if (m_confettiT > 3.2f) fade = std::max(0.f, 1.f - (m_confettiT - 3.2f) / 1.4f);
    for (const Confetti& c : m_confetti) {
        Gdiplus::GraphicsState st = g.Save();
        g.TranslateTransform((Gdiplus::REAL)X((int)c.x), (Gdiplus::REAL)Y((int)c.y));
        g.RotateTransform(c.rot);
        Gdiplus::SolidBrush b(argb((BYTE)(235 * fade), c.color));
        g.FillRectangle(&b, -SF(c.size * 0.5f), -SF(c.size * 0.28f),
                        SF(c.size), SF(c.size * 0.56f));
        g.Restore(st);
    }
}

void AppState::PaintFade(Gdiplus::Graphics& g, int w, int h)
{
    if (m_pageFade <= 0.001f) return;
    BYTE alpha = (BYTE)std::min(255, (int)(m_pageFade * 235.f));
    Gdiplus::SolidBrush veil(argb(alpha, 0x05090F));
    int x = S(232);
    int y = m_lay.top;
    g.FillRectangle(&veil, (Gdiplus::REAL)x, (Gdiplus::REAL)y,
                    (Gdiplus::REAL)(w - x), (Gdiplus::REAL)(h - y));
}

// ===========================================================================
//  main foreground entry
// ===========================================================================
void AppState::PaintForeground(Gdiplus::Graphics& g, int w, int h)
{
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    PaintTitleMark(g);
    PaintChrome(g);
    PaintHeader(g);

    switch (m_page) {
    case 0: PaintDashboardFg(g); break;
    case 1: PaintFirmwarePage(g, false); break;
    case 2: PaintPartitionsPage(g, false); break;
    case 3: PaintLogPage(g, false); break;
    case 4: PaintSettingsPage(g, false); break;
    default: break;
    }

    PaintFade(g, w, h);
    PaintConfetti(g);
}
