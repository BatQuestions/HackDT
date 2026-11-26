/*
 * UnderMenu by Mr.Bat
 * Menu de hacks para Undertale.exe. Ventana basada en BalMenu.
 * Agradecimiento especial a TheInsaneHacker por los punteros.
 *
 * UnderMenu.ini (junto al .exe, valores en hexadecimal):
 *   [UnderMenu]
 *   opp_base=XXXXXX       ; base (desde el modulo) para VIDA ENEMIGO 1 y 2
 *   nivel_offset=XXX      ; ultimo offset de NIVEL
 *
 * Compilar (zig):
 *   zig cc -target x86_64-windows-gnu -O2 -Wl,--subsystem,windows -s -o UnderMenu.exe UnderMenu.c -luser32 -lgdi32 -lkernel32
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#define APP_TITLE   "UnderMenu by Mr.Bat"
#define GAME_EXE    "Undertale.exe"

/* ---------- colores (los mismos de BalMenu) ---------- */
#define C_BG     RGB(0x3A, 0x3A, 0x3A)
#define C_EDITBG RGB(0x1F, 0x1F, 0x1F)
#define C_GREEN  RGB(0x33, 0xFF, 0x33)
#define C_DIM    RGB(0x1F, 0x9F, 0x1F)

/* ---------- estado del proceso ---------- */
static HANDLE    g_proc;
static DWORD     g_pid;
static uintptr_t g_base;     /* base del modulo principal */
static char      g_err[160];
static HWND      g_log;

static void setstatus(const char *fmt, ...)
{
    char b[240] = "> ";
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b + 2, sizeof b - 2, fmt, ap);
    va_end(ap);
    if (g_log) SetWindowTextA(g_log, b);
}

/* ---------- filas ---------- */
/* Undertale es de 32 bits: punteros de 4 bytes, valores double (8 bytes).
 * Camino: addr = base_modulo + base; por cada offset: addr = [addr] + offset; valor = double en addr. */
typedef struct { const char *label; unsigned base; bool hasBase; int n; unsigned off[5]; } Row;

static Row g_rows[] = {
    {"VIDA",           0x408950, true,  4, {0x44, 0x10, 0x13C, 0x4B0}},
    {"VIDA MAX",       0x408950, true,  4, {0x44, 0x10, 0x13C, 0x4A0}},
    {"DINERO",         0x3F9F44, true,  5, {0x0, 0x44, 0x10, 0x364, 0x400}},
    {"EXPERIENCIA",    0x3F9F44, true,  5, {0x0, 0x44, 0x10, 0x364, 0x3F0}},
    {"NIVEL",          0x3F9F44, true,  5, {0x0, 0x44, 0x10, 0x364, 0x3F0}},
    {"VIDA ENEMIGO 1", 0x0,      false, 5, {0x48, 0x8, 0x50, 0x14, 0x5C0}},
    {"VIDA ENEMIGO 2", 0x0,      false, 5, {0x48, 0x8, 0x50, 0x14, 0x5D0}},
};
#define NROWS 7

static void load_ini(void)
{
    char path[MAX_PATH];
    if (!GetModuleFileNameA(NULL, path, sizeof path)) return;
    char *s = strrchr(path, '\\');
    if (!s || (size_t)(s - path) + 16 >= sizeof path) return;
    strcpy(s + 1, "UnderMenu.ini");
    char buf[64];
    char *end;
    GetPrivateProfileStringA("UnderMenu", "opp_base", "", buf, sizeof buf, path);
    if (buf[0]) {
        unsigned long v = strtoul(buf, &end, 16);
        while (*end == ' ' || *end == '\t') end++;
        if (end != buf && *end == '\0') {
            g_rows[5].base = g_rows[6].base = (unsigned)v;
            g_rows[5].hasBase = g_rows[6].hasBase = true;
        }
    }
    GetPrivateProfileStringA("UnderMenu", "nivel_offset", "", buf, sizeof buf, path);
    if (buf[0]) {
        unsigned long v = strtoul(buf, &end, 16);
        while (*end == ' ' || *end == '\t') end++;
        if (end != buf && *end == '\0') g_rows[4].off[4] = (unsigned)v;
    }
}

/* ---------- memoria ---------- */
static bool rd(uintptr_t a, void *buf, size_t n)
{
    SIZE_T got = 0;
    return ReadProcessMemory(g_proc, (LPCVOID)a, buf, n, &got) && got == n;
}
static bool wr(uintptr_t a, const void *buf, size_t n)
{
    SIZE_T got = 0;
    return WriteProcessMemory(g_proc, (LPVOID)a, buf, n, &got) && got == n;
}

/* 0 = ok, 1 = sin base, 2 = no disponible */
static int resolve(const Row *e, uintptr_t *out)
{
    if (!e->hasBase) return 1;
    uintptr_t addr = g_base + e->base;
    for (int i = 0; i < e->n; i++) {
        uint32_t p = 0;
        if (!rd(addr, &p, 4) || p == 0) return 2;
        addr = (uintptr_t)p + e->off[i];
    }
    *out = addr;
    return 0;
}

static void short_num(double v, char *out, size_t n)
{
    double a = v < 0 ? -v : v;
    if (a == (double)(long long)a && a < 1e8) snprintf(out, n, "%lld", (long long)v);
    else if (a < 1e5)                         snprintf(out, n, "%.2f", v);
    else                                      snprintf(out, n, "%.2e", v);
}

/* 0 ok / 1 sin base / 2 no disponible; escribe el texto a mostrar en out */
static int read_row(const Row *e, char *out, size_t n)
{
    uintptr_t a;
    int r = resolve(e, &a);
    if (r == 1) { snprintf(out, n, "sin base"); return 1; }
    double v;
    if (r != 0 || !rd(a, &v, 8) || v != v) { snprintf(out, n, "--"); return 2; }
    short_num(v, out, n);
    return 0;
}

static bool parse_double(const char *txt, double *v)
{
    char b[64];
    snprintf(b, sizeof b, "%s", txt);
    for (char *c = b; *c; c++) if (*c == ',') *c = '.';
    char *end = NULL;
    *v = strtod(b, &end);
    if (end == b) return false;
    while (*end == ' ') end++;
    return *end == '\0';
}

/* ---------- sesion ---------- */
static DWORD find_pid(const char *name)
{
    DWORD pid = 0;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe; pe.dwSize = sizeof pe;
    if (Process32First(s, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32Next(s, &pe));
    }
    CloseHandle(s);
    return pid;
}

static bool attach(void)
{
    if (g_proc) {
        DWORD ec = 0;
        if (GetExitCodeProcess(g_proc, &ec) && ec == STILL_ACTIVE) return true;
        CloseHandle(g_proc);
        g_proc = NULL; g_pid = 0; g_base = 0;
    }
    DWORD pid = find_pid(GAME_EXE);
    if (!pid) { snprintf(g_err, sizeof g_err, "esperando el juego"); return false; }
    HANDLE h = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) { snprintf(g_err, sizeof g_err, "acceso denegado (ejecuta como admin)"); return false; }
    uintptr_t base = 0;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me; me.dwSize = sizeof me;
        if (Module32First(s, &me)) base = (uintptr_t)me.modBaseAddr;   /* el primero es el .exe */
        CloseHandle(s);
    }
    if (!base) { CloseHandle(h); snprintf(g_err, sizeof g_err, "no se encontro el modulo"); return false; }
    g_proc = h; g_pid = pid; g_base = base;
    return true;
}

/* ---------- UI (clon del diseño de BalMenu) ---------- */
#define ID_TITLE  7001
#define ID_CREDIT 7002
#define ID_PTITLE 7003
#define ID_STATUS 7004
#define ID_LOG    7005
#define ID_VAL    4000
#define ID_EDIT   2000
#define ID_BTNA   3000
#define ID_LBL    6000

static HWND g_hwnd, g_status, g_ptitle;
static HFONT g_fReg, g_fBold, g_fSmall;
static HBRUSH g_bgBrush, g_editBrush;
static HWND g_lbl[NROWS], g_val[NROWS], g_edit[NROWS], g_btnA[NROWS];
static bool g_valOk[NROWS];
static char g_valTxt[NROWS][24];
static bool g_connected;
static int  g_dpi = 96;
static WNDPROC g_oldEdit;

#define S(x) MulDiv((x), g_dpi, 96)

static HWND mk(HWND parent, const char *cls, const char *txt, DWORD style,
               int x, int y, int w, int h, int id, HFONT f)
{
    HWND c = CreateWindowExA(0, cls, txt, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                             parent, (HMENU)(INT_PTR)id, GetModuleHandle(NULL), NULL);
    SendMessage(c, WM_SETFONT, (WPARAM)f, TRUE);
    return c;
}

static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CHAR && w == VK_RETURN) return 0;
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        int id = GetDlgCtrlID(h) - ID_EDIT + ID_BTNA;
        PostMessage(GetParent(h), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)GetDlgItem(GetParent(h), id));
        return 0;
    }
    return CallWindowProc(g_oldEdit, h, m, w, l);
}

static void refresh_ui(void)
{
    char t[200], cur[200];
    bool ok = attach();
    g_connected = ok;
    if (ok) snprintf(t, sizeof t, "[+] %s  PID %lu  (Conectado)", GAME_EXE, (unsigned long)g_pid);
    else    snprintf(t, sizeof t, "[-] %s: %s", GAME_EXE, g_err);
    GetWindowTextA(g_status, cur, sizeof cur);
    if (strcmp(cur, t) != 0) { SetWindowTextA(g_status, t); InvalidateRect(g_status, NULL, TRUE); }

    for (int i = 0; i < NROWS; i++) {
        char v[24] = "--";
        bool vok = false;
        if (ok) vok = (read_row(&g_rows[i], v, sizeof v) == 0);
        if (strcmp(v, g_valTxt[i]) != 0 || vok != g_valOk[i]) {
            snprintf(g_valTxt[i], sizeof g_valTxt[i], "%s", v);
            g_valOk[i] = vok;
            SetWindowTextA(g_val[i], v);
            InvalidateRect(g_val[i], NULL, TRUE);
        }
    }
}

static void on_set(int slot)
{
    const Row *e = &g_rows[slot];
    char txt[64];
    GetWindowTextA(g_edit[slot], txt, sizeof txt);
    if (txt[0] == '\0') { setstatus("%s: ingresa un valor", e->label); return; }
    double v;
    if (!parse_double(txt, &v)) { setstatus("%s: valor invalido", e->label); return; }
    if (!attach()) { setstatus("sin conexion con el juego"); return; }
    uintptr_t a;
    int r = resolve(e, &a);
    if (r == 1) { setstatus("%s: sin base (ver UnderMenu.ini)", e->label); return; }
    if (r != 0) { setstatus("%s: no disponible (entra al juego)", e->label); return; }
    if (wr(a, &v, 8)) setstatus("%s = %s (OK)", e->label, txt);
    else              setstatus("%s: no se pudo escribir", e->label);
    refresh_ui();
}

static void build_ui(HWND hw)
{
    mk(hw, "STATIC", "> UnderMenu by Mr.Bat", SS_NOPREFIX, 10, 6, 366, 20, ID_TITLE, g_fBold);
    mk(hw, "STATIC", "Agradecimiento especial a TheInsaneHacker por los punteros", SS_NOPREFIX, 10, 27, 366, 16, ID_CREDIT, g_fSmall);
    g_ptitle = mk(hw, "STATIC", "PAGINA 1/1: UNDERTALE", SS_CENTER | SS_NOPREFIX, 10, 51, 366, 18, ID_PTITLE, g_fBold);
    for (int i = 0; i < NROWS; i++) {
        int y = 76 + 28 * i;
        g_lbl[i]  = mk(hw, "STATIC", g_rows[i].label, SS_NOPREFIX, 10, y + 3, 140, 20, ID_LBL + i, g_fReg);
        g_val[i]  = mk(hw, "STATIC", "--", SS_NOPREFIX, 152, y + 3, 68, 20, ID_VAL + i, g_fReg);
        g_edit[i] = mk(hw, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 224, y, 94, 22, ID_EDIT + i, g_fReg);
        if (!g_oldEdit) g_oldEdit = (WNDPROC)GetWindowLongPtr(g_edit[i], GWLP_WNDPROC);
        SetWindowLongPtr(g_edit[i], GWLP_WNDPROC, (LONG_PTR)EditProc);
        g_btnA[i] = mk(hw, "BUTTON", "SET", BS_OWNERDRAW, 324, y, 52, 22, ID_BTNA + i, g_fReg);
    }
    g_status = mk(hw, "STATIC", "", SS_NOPREFIX, 10, 278, 366, 18, ID_STATUS, g_fReg);
    g_log    = mk(hw, "STATIC", "> Listo. Esperando orden...", SS_NOPREFIX, 10, 298, 366, 18, ID_LOG, g_fReg);
}

static void draw_button(const DRAWITEMSTRUCT *d)
{
    bool down = (d->itemState & ODS_SELECTED) != 0;
    bool dis  = (d->itemState & ODS_DISABLED) != 0;
    HBRUSH bg = CreateSolidBrush(down && !dis ? C_GREEN : C_BG);
    HBRUSH fr = CreateSolidBrush(C_DIM);
    FillRect(d->hDC, &d->rcItem, bg);
    FrameRect(d->hDC, &d->rcItem, fr);
    DeleteObject(bg);
    DeleteObject(fr);
    char t[16];
    GetWindowTextA(d->hwndItem, t, sizeof t);
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, dis ? C_DIM : (down ? C_BG : C_GREEN));
    HGDIOBJ old = SelectObject(d->hDC, g_fReg);
    RECT r = d->rcItem;
    DrawTextA(d->hDC, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(d->hDC, old);
}

static LRESULT CALLBACK WndProc(HWND hw, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        g_hwnd = hw;
        build_ui(hw);
        refresh_ui();
        SetTimer(hw, 1, 700, NULL);
        return 0;
    case WM_TIMER:
        refresh_ui();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        int id = GetDlgCtrlID((HWND)l);
        COLORREF col = C_GREEN;
        if (id == ID_CREDIT) col = C_DIM;
        else if (id == ID_STATUS) col = g_connected ? C_GREEN : C_DIM;
        else if (id >= ID_VAL && id < ID_VAL + NROWS) col = g_valOk[id - ID_VAL] ? C_GREEN : C_DIM;
        SetTextColor(dc, col);
        SetBkColor(dc, C_BG);
        return (LRESULT)g_bgBrush;
    }
    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)w, C_GREEN);
        SetBkColor((HDC)w, C_EDITBG);
        return (LRESULT)g_editBrush;
    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)l);
        return TRUE;
    case WM_COMMAND: {
        if (HIWORD(w) != BN_CLICKED) break;
        int id = LOWORD(w);
        if (id >= ID_BTNA && id < ID_BTNA + NROWS) on_set(id - ID_BTNA);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hw, 1);
        if (g_proc) CloseHandle(g_proc);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hw, m, w, l);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    (void)hp; (void)cmd;
    load_ini();

    HMODULE u = GetModuleHandleA("user32.dll");
    BOOL (WINAPI *setaware)(void) = u ? (BOOL (WINAPI *)(void))GetProcAddress(u, "SetProcessDPIAware") : NULL;
    if (setaware) setaware();
    HDC sdc = GetDC(NULL);
    g_dpi = GetDeviceCaps(sdc, LOGPIXELSY);
    ReleaseDC(NULL, sdc);
    if (g_dpi < 96) g_dpi = 96;

    g_fReg   = CreateFontA(-S(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    g_fBold  = CreateFontA(-S(15), 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    g_fSmall = CreateFontA(-S(11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    g_bgBrush = CreateSolidBrush(C_BG);
    g_editBrush = CreateSolidBrush(C_EDITBG);

    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_bgBrush;
    wc.lpszClassName = "UnderMenuWnd";
    RegisterClassA(&wc);

    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = {0, 0, S(386), S(326)};
    AdjustWindowRect(&r, style, FALSE);
    HWND hw = CreateWindowExA(0, "UnderMenuWnd", APP_TITLE, style,
                              CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                              NULL, NULL, hi, NULL);
    if (!hw) return 1;
    ShowWindow(hw, show);
    UpdateWindow(hw);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
