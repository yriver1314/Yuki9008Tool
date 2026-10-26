#pragma once
#include "main_shared.h"

class AppState {
public:
    AppState(App& app, UI& ui);
    ~AppState();

    void Build();
    void Begin();
    void Tick();
    void OnResize(int px, int py);

    bool OnCustomMouseDown(int px, int py);
    void OnCustomMouseMove(int px, int py);
    void OnDrop(HDROP drop);
    void TriggerStart();
    void TriggerScan();
    void TriggerLog();

private:
    struct Confetti {
        float x = 0, y = 0, vx = 0, vy = 0, rot = 0, vr = 0, size = 4;
        DWORD color = 0xFFFFFF;
    };

    enum class UpdateState { Idle, Checking, UpToDate, Available, Failed };
    struct UpdateResult {
        bool ok = false;
        bool newer = false;
        std::wstring error;
        std::wstring tag;
        std::wstring pageUrl;
        std::wstring assetUrl;
        std::wstring notes;
    };

    App& m_app;
    UI&  m_ui;
    LLayout m_lay;
    CFonts m_fonts;
    std::unique_ptr<Gdiplus::Bitmap> m_logo;
    Theme m_theme;
    DWORD m_accent = 0x4D9DFF;
    DWORD m_accent2 = 0x9179FF;
    int m_page = 0;
    bool m_animEnabled = true;

    std::vector<Widget> m_dash, m_fwPage, m_part, m_log, m_set;
    Widget m_heroScan, m_heroBrowse, m_startBtn, m_stopBtn, m_browseBtn, m_changeBtn;
    Widget m_optVerify, m_optReboot, m_optBackup, m_optKeep;
    Widget m_fwBrowse, m_fwTable, m_fwRemove, m_fwRescan;
    Widget m_partSearch, m_partFilter, m_partTable, m_partExport;
    Widget m_logArea, m_logExport, m_logClear, m_logAuto;
    Widget m_setAnim, m_setVerify, m_setReboot, m_setReset, m_setLogs, m_setUpdate;

    FirmwareInfo m_fw;
    DeviceInfo m_dev;
    std::vector<LogItem> m_logs;
    std::wstring m_logText;
    std::wstring m_statusMain = L"正在初始化工作区";
    std::wstring m_statusSub = L"YUKI9008 FLASH ENGINE";
    DWORD m_statusColor = col::amber;

    bool m_scanning = false;
    bool m_scanApplied = false;
    bool m_autoStart = false;
    float m_scanT = 0.f;
    bool m_running = false;
    bool m_done = false;
    bool m_aborted = false;
    bool m_failed = false;
    bool m_hasProgress = false;
    int m_stage = 0;
    float m_target = 0.f;
    float m_display = 0.f;
    int m_currentPart = -1;
    float m_speed = 0.f;
    float m_writtenMB = 0.f;
    float m_remaining = 0.f;
    ULONGLONG m_progressMs = 0;
    std::deque<float> m_speedHist;

    unsigned m_frame = 0;
    float m_time = 0.f;
    float m_dt = 0.016f;
    std::chrono::steady_clock::time_point m_lastTick;
    float m_pageFade = 1.f;
    int m_hoverNav = -1;
    std::array<float, 5> m_navT = { 1.f, 0.f, 0.f, 0.f, 0.f };
    bool m_hoverPill = false;
    bool m_hoverDrop = false;
    bool m_hoverHero = false;
    int m_hoverAccent = -1;
    std::vector<Confetti> m_confetti;
    float m_confettiT = 999.f;

    UpdateState m_updateState = UpdateState::Idle;
    std::wstring m_updateMessage = L"GitHub Release 更新通道";
    std::wstring m_updateTag;
    std::wstring m_updateUrl;
    std::wstring m_updateAssetUrl;
    std::wstring m_updateNotes;
    std::thread m_updateThread;
    std::atomic<bool> m_closing = { false };

    // Real QFIL / fh_loader backend
    std::wstring m_engineDir;
    std::wstring m_engineSahara;
    std::wstring m_engineFhLoader;
    std::wstring m_engineNote;
    bool m_engineReady = false;
    HANDLE m_proc = nullptr;
    HANDLE m_procRead = nullptr;
    std::wstring m_procLogPath;
    std::string m_procPending;
    int m_procPhase = 0;   // 1 Sahara, 2 rawprogram, 3 patch
    bool m_patchPending = false;
    ULONGLONG m_procStartMs = 0;

    // ---- lifecycle / construction ----
    void BuildControls();
    void LoadLogo();
    void DrawBrandLogo(Gdiplus::Graphics& g, int x, int y, int size, bool absoluteY) const;
    void Layout();
    void ApplyAccent(bool announce);
    void AttachWidgets();
    void SetPage(int page);

    // ---- data ----
    void ResetFirmware();
    void DetectEngine();
    void LoadFirmwareDir(const std::wstring& dir);
    bool ScanFirmware(const std::wstring& dir, FirmwareInfo& out);
    void DetectDevice();
    void StartScan();
    void PopulateTables();
    void RefreshPartitionTable();
    void BrowseFirmware();
    void OpenFirmwareFolder();
    void ExportLog();
    void ClearLog();
    void ExportPartitions();
    void RefreshFirmwarePage();
    void ResetSettings();
    void CheckForUpdates();
    UpdateResult FetchLatestRelease();
    void ApplyUpdateResult(const UpdateResult& result);
    void UpdateUpdateButton();
    void OpenUpdatePage();

    // ---- flashing ----
    void StartFlash();
    void StopFlash();
    void PollFlash();
    void FinishFlash();
    void FailFlash(const std::wstring& reason);
    void UpdateStartButton();
    bool LaunchPhase(int phase);
    void ReadProcessOutput();
    void CloseProcessHandles();
    std::wstring PortName() const;
    std::wstring MemoryType() const;
    std::wstring ShellQuote(const std::wstring& s) const;

    // ---- logging ----
    void Log(const std::wstring& text, LogLevel level = LogLevel::Info);
    void SyncLog();
    void SetStatus(const std::wstring& main, const std::wstring& sub, DWORD color);

    // ---- painting utilities ----
    int S(int v) const;
    float SF(float v) const;
    int X(int v) const;
    int Y(int v) const;
    Gdiplus::RectF R(const LR& r) const;
    void FillRound(Gdiplus::Graphics& g, const LR& r, DWORD color, int radius,
                   BYTE alpha = 255) const;
    void StrokeRound(Gdiplus::Graphics& g, const LR& r, DWORD color, int radius,
                     float width = 1.f, BYTE alpha = 255) const;
    void Panel(Gdiplus::Graphics& g, const LR& r, DWORD top, DWORD bottom,
               DWORD border, int radius, BYTE alpha = 255, bool shadow = true) const;
    void Glow(Gdiplus::Graphics& g, int cx, int cy, int radius, DWORD color,
              int strength, int layers = 8) const;
    void Txt(Gdiplus::Graphics& g, const std::wstring& s, const LR& r,
             const Gdiplus::Font* f, DWORD color, BYTE alpha = 255,
             int align = 0) const;   // 0 left, 1 center, 2 right
    void ClipText(Gdiplus::Graphics& g, const std::wstring& s, const LR& r,
                  const Gdiplus::Font* f, DWORD color, BYTE alpha = 255,
                  int align = 0) const;
    void Hairline(Gdiplus::Graphics& g, int x1, int y1, int x2, int y2,
                  DWORD color, BYTE alpha = 160, float width = 1.f) const;
    void IconFlash(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const;
    void IconBox(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const;
    void IconGrid(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const;
    void IconTerminal(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const;
    void IconGear(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const;
    void IconRefresh(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color) const;
    void IconChevron(Gdiplus::Graphics& g, int cx, int cy, int size, DWORD color, int dir) const;

    // ---- painting ----
    void ComputeLayout(int w, int h);
    void PaintBackground(Gdiplus::Graphics& g, int w, int h);
    void PaintForeground(Gdiplus::Graphics& g, int w, int h);
    void PaintTitleMark(Gdiplus::Graphics& g);
    void PaintChrome(Gdiplus::Graphics& g);
    void PaintHeader(Gdiplus::Graphics& g);
    void PaintSidebar(Gdiplus::Graphics& g);
    void PaintDashboardBg(Gdiplus::Graphics& g);
    void PaintDashboardFg(Gdiplus::Graphics& g);
    void PaintHeroArt(Gdiplus::Graphics& g);
    void PaintStage(Gdiplus::Graphics& g);
    void PaintFirmwareCard(Gdiplus::Graphics& g);
    void PaintHealth(Gdiplus::Graphics& g);
    void PaintOptions(Gdiplus::Graphics& g);
    void PaintDeviceCard(Gdiplus::Graphics& g);
    void PaintActionBar(Gdiplus::Graphics& g);
    void PaintFirmwarePage(Gdiplus::Graphics& g, bool bg);
    void PaintPartitionsPage(Gdiplus::Graphics& g, bool bg);
    void PaintLogPage(Gdiplus::Graphics& g, bool bg);
    void PaintSettingsPage(Gdiplus::Graphics& g, bool bg);
    void PaintConfetti(Gdiplus::Graphics& g);
    void PaintFade(Gdiplus::Graphics& g, int w, int h);

    LR NavRect(int i) const;
    LR AccentRect(int i) const;
    std::wstring PageTitle(int page) const;
    std::wstring PageEyebrow(int page) const;
    std::wstring PageSubtitle(int page) const;
    std::wstring SpeedText() const;

public:
    static AppState* s_instance;
};
