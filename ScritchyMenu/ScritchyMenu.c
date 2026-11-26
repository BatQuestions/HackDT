/*
 * ScritchyMenu by Mr.Bat
 * Menu de hacks para Scritchy Scratchy (build de la tabla 1246d).
 * Ventana basada en BalMenu. Agradecimiento especial a OWSEEX.
 *
 * Compilar (zig):
 *   zig cc -target x86_64-windows-gnu -O2 -Wl,--subsystem,windows -s -o ScritchyMenu.exe ScritchyMenu.c -luser32 -lgdi32 -lkernel32
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

#define APP_TITLE   "ScritchyMenu by Mr.Bat"
#define GAME_EXE    "ScritchyScratchy.exe"
#define GA_DLL      "GameAssembly.dll"
#define BASE_OFF    0x38252B0ULL

/* ---------- colores (los mismos de BalMenu) ---------- */
#define C_BG     RGB(0x3A, 0x3A, 0x3A)
#define C_EDITBG RGB(0x1F, 0x1F, 0x1F)
#define C_GREEN  RGB(0x33, 0xFF, 0x33)
#define C_DIM    RGB(0x1F, 0x9F, 0x1F)

typedef enum { T_I32, T_F32, T_F64, T_U8 } VType;

/* ---------- estado del proceso ---------- */
static HANDLE    g_proc;
static DWORD     g_pid;
static uintptr_t g_base;
static size_t    g_size;
static char      g_err[160];
static HWND      g_log;

/* linea de log (abajo): siempre con prefijo "> " */
static void setstatus(const char *fmt, ...)
{
    char b[240] = "> ";
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b + 2, sizeof b - 2, fmt, ap);
    va_end(ap);
    if (g_log) SetWindowTextA(g_log, b);
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
static bool wr_code(uintptr_t a, const void *buf, size_t n)
{
    DWORD old = 0, tmp = 0;
    VirtualProtectEx(g_proc, (LPVOID)a, n, PAGE_EXECUTE_READWRITE, &old);
    bool ok = wr(a, buf, n);
    VirtualProtectEx(g_proc, (LPVOID)a, n, old ? old : PAGE_EXECUTE_READ, &tmp);
    FlushInstructionCache(g_proc, (LPCVOID)a, n);
    return ok;
}

/* ---------- escaneo AOB ---------- */
static uintptr_t aob_scan(const unsigned char *pat, int plen)
{
    const size_t CH = 0x100000;
    unsigned char *buf = (unsigned char *)malloc(CH + (size_t)plen);
    if (!buf) return 0;
    uintptr_t found = 0;
    for (size_t o = 0; o < g_size && !found; o += CH) {
        size_t want = CH + (size_t)plen - 1;
        if (o + want > g_size) want = g_size - o;
        SIZE_T got = 0;
        ReadProcessMemory(g_proc, (LPCVOID)(g_base + o), buf, want, &got);
        if (got < (SIZE_T)plen) continue;
        size_t lim = got - (size_t)plen + 1;
        unsigned char *p = buf;
        size_t left = lim;
        while (left) {
            unsigned char *q = (unsigned char *)memchr(p, pat[0], left);
            if (!q) break;
            if (memcmp(q, pat, (size_t)plen) == 0) { found = g_base + o + (size_t)(q - buf); break; }
            left -= (size_t)(q - p) + 1;
            p = q + 1;
        }
    }
    free(buf);
    return found;
}

static uintptr_t alloc_near(uintptr_t target)
{
    uintptr_t a = target & ~(uintptr_t)0xFFFF;
    for (uintptr_t d = 0x10000; d < 0x70000000; d += 0x10000) {
        if (a > d + 0x10000) {
            void *p = VirtualAllocEx(g_proc, (void *)(a - d), 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (p) return (uintptr_t)p;
        }
        void *p = VirtualAllocEx(g_proc, (void *)(a + d), 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (p) return (uintptr_t)p;
    }
    return 0;
}

/* ---------- parches simples ---------- */
typedef struct {
    const unsigned char *pat; int plen; int ofs;
    const unsigned char *on; const unsigned char *off; int n;
    uintptr_t addr;
} Patch;

#define ARR(x) x, (int)sizeof(x)

static const unsigned char LUCK_P[]   = {0x8B,0x81,0x9C,0x00,0x00,0x00,0x2B,0x81,0xA0,0x00,0x00,0x00,0xB9,0xFD,0xFF,0xFF,0xFF,0x3B,0xC1,0x0F,0x4C,0xC1,0xC3};
static const unsigned char LUCK_ON[]  = {0xB8,0x0F,0x27,0x00,0x00,0xC3};
static const unsigned char LUCK_OFF[] = {0x8B,0x81,0x9C,0x00,0x00,0x00};

static const unsigned char JP_P[]     = {0x0F,0xB6,0x81,0x43,0x01,0x00,0x00,0xC3};
static const unsigned char JP_ON[]    = {0xB0,0x01,0xC3,0x90,0x90,0x90,0x90,0x90};

static const unsigned char AFA_P[]    = {0x48,0x83,0xEC,0x38,0x80,0x3D,0xB0,0x7C,0x55,0x03,0x00,0x0F,0x29,0x74,0x24,0x20,0x0F,0x28,0xF1,0x75,0x13};
static const unsigned char AFA_ON[]   = {0xB0,0x01,0xC3};
static const unsigned char AFA_OFF[]  = {0x48,0x83,0xEC};
static const unsigned char AFB_P[]    = {0x45,0x33,0xC0,0x0F,0x28,0xCE,0xE8,0x18,0x41,0xFB,0xFF,0x84,0xC0,0x74,0x78};
static const unsigned char AFB_ON[]   = {0x90,0x90};
static const unsigned char AFB_OFF[]  = {0x74,0x78};
static const unsigned char AFC_P[]    = {0x33,0xC0,0x66,0x0F,0x2F,0xC6,0x0F,0x92,0xC0,0x85,0xC0,0x0F,0x85,0x43,0x01,0x00,0x00};
static const unsigned char AFC_ON[]   = {0x90,0x90,0x90,0x90,0x90,0x90};
static const unsigned char AFC_OFF[]  = {0x0F,0x85,0x43,0x01,0x00,0x00};

static Patch g_luck[1];
static Patch g_jack[1];
static Patch g_afford[3];

static void init_patches(void)
{
    memset(g_luck, 0, sizeof g_luck);
    memset(g_jack, 0, sizeof g_jack);
    memset(g_afford, 0, sizeof g_afford);
    g_luck[0]   = (Patch){ARR(LUCK_P), 0,   LUCK_ON, LUCK_OFF, (int)sizeof LUCK_ON, 0};
    g_jack[0]   = (Patch){ARR(JP_P),   0,   JP_ON,   JP_P,     (int)sizeof JP_ON,   0};
    g_afford[0] = (Patch){ARR(AFA_P),  0,   AFA_ON,  AFA_OFF,  (int)sizeof AFA_ON,  0};
    g_afford[1] = (Patch){ARR(AFB_P),  0xD, AFB_ON,  AFB_OFF,  (int)sizeof AFB_ON,  0};
    g_afford[2] = (Patch){ARR(AFC_P),  0xB, AFC_ON,  AFC_OFF,  (int)sizeof AFC_ON,  0};
}

static bool apply(Patch *p, int cnt, bool en)
{
    if (en) {
        for (int i = 0; i < cnt; i++) {
            if (p[i].addr) continue;
            uintptr_t a = aob_scan(p[i].pat, p[i].plen);
            if (!a) {
                setstatus("Patron no encontrado (otra version del juego o ya parcheado)");
                return false;
            }
            p[i].addr = a + (uintptr_t)p[i].ofs;
        }
    }
    for (int i = 0; i < cnt; i++) {
        if (!p[i].addr) continue;
        if (!wr_code(p[i].addr, en ? p[i].on : p[i].off, (size_t)p[i].n)) {
            setstatus("No pude escribir en el juego");
            return false;
        }
    }
    return true;
}

/* ---------- parches con code cave ---------- */
static const unsigned char TM_P[]   = {0x0F,0x28,0xC6,0x0F,0x28,0x74,0x24,0x60,0x48,0x83,0xC4,0x70,0x5D,0xC3};
static const unsigned char TM_OFF[] = {0x0F,0x28,0xC6,0x0F,0x28,0x74,0x24,0x60};
static uintptr_t tm_hook, tm_cave;
static float g_tmult = 10.0f;

static bool ticket_enable(void)
{
    if (!tm_hook) {
        tm_hook = aob_scan(TM_P, (int)sizeof TM_P);
        if (!tm_hook) { setstatus("Patron no encontrado (otra version del juego o ya parcheado)"); return false; }
    }
    if (!tm_cave) {
        tm_cave = alloc_near(tm_hook);
        if (!tm_cave) { setstatus("No pude reservar memoria cerca del codigo"); return false; }
    }
    unsigned char c[40];
    memset(c, 0x90, sizeof c);
    c[0] = 0xF3; c[1] = 0x0F; c[2] = 0x59; c[3] = 0x35;          /* mulss xmm6,[rip+d] */
    int32_t d = 32 - 8;  memcpy(c + 4, &d, 4);
    c[8] = 0x0F; c[9] = 0x28; c[10] = 0xC6;                      /* movaps xmm0,xmm6 */
    c[11] = 0x0F; c[12] = 0x28; c[13] = 0x74; c[14] = 0x24; c[15] = 0x60; /* movaps xmm6,[rsp+60] */
    c[16] = 0xE9;                                                /* jmp ret */
    int32_t rel = (int32_t)((int64_t)(tm_hook + 8) - (int64_t)(tm_cave + 21));
    memcpy(c + 17, &rel, 4);
    memcpy(c + 32, &g_tmult, 4);
    if (!wr(tm_cave, c, sizeof c)) { setstatus("No pude escribir la cueva"); return false; }

    unsigned char h[8] = {0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90};
    int32_t hr = (int32_t)((int64_t)tm_cave - (int64_t)(tm_hook + 5));
    memcpy(h + 1, &hr, 4);
    if (!wr_code(tm_hook, h, sizeof h)) { setstatus("No pude escribir el hook"); return false; }
    return true;
}
static bool ticket_disable(void)
{
    if (!tm_hook) return true;
    return wr_code(tm_hook, TM_OFF, sizeof TM_OFF);
}

static const unsigned char MN_P[]   = {0x40,0x53,0x48,0x83,0xEC,0x20,0xF2,0x0F,0x58,0x49,0x10,0x33,0xD2,0x48,0x8B,0xD9,0x0F,0x28,0xC1};
static const unsigned char MN_OFF[] = {0x40,0x53,0x48,0x83,0xEC,0x20};
static uintptr_t mn_hook, mn_cave;
static double g_mmult = 5.0;
static unsigned char g_free = 1;

static bool money_enable(void)
{
    if (!mn_hook) {
        mn_hook = aob_scan(MN_P, (int)sizeof MN_P);
        if (!mn_hook) { setstatus("Patron no encontrado (otra version del juego o ya parcheado)"); return false; }
    }
    if (!mn_cave) {
        mn_cave = alloc_near(mn_hook);
        if (!mn_cave) { setstatus("No pude reservar memoria cerca del codigo"); return false; }
    }
    unsigned char c[64];
    memset(c, 0x90, sizeof c);
    int32_t d;
    /*  0 */ c[0]=0x0F; c[1]=0x57; c[2]=0xC0;                       /* xorps xmm0,xmm0 */
    /*  3 */ c[3]=0x66; c[4]=0x0F; c[5]=0x2F; c[6]=0xC1;            /* comisd xmm0,xmm1 */
    /*  7 */ c[7]=0x76; c[8]=0x0E;                                  /* jbe check_mult (23) */
    /*  9 */ c[9]=0x80; c[10]=0x3D; d = 48 - 16; memcpy(c+11,&d,4); c[15]=0x01; /* cmp byte [rip+d],1 */
    /* 16 */ c[16]=0x75; c[17]=0x13;                                /* jne orig (37) */
    /* 18 */ c[18]=0x0F; c[19]=0x57; c[20]=0xC9;                    /* xorps xmm1,xmm1 */
    /* 21 */ c[21]=0xEB; c[22]=0x0E;                                /* jmp orig (37) */
    /* 23 */ c[23]=0x66; c[24]=0x0F; c[25]=0x2F; c[26]=0xC8;        /* comisd xmm1,xmm0 */
    /* 27 */ c[27]=0x76; c[28]=0x08;                                /* jbe orig (37) */
    /* 29 */ c[29]=0xF2; c[30]=0x0F; c[31]=0x59; c[32]=0x0D; d = 56 - 37; memcpy(c+33,&d,4); /* mulsd xmm1,[rip+d] */
    /* 37 */ c[37]=0x40; c[38]=0x53;                                /* push rbx */
    /* 39 */ c[39]=0x48; c[40]=0x83; c[41]=0xEC; c[42]=0x20;        /* sub rsp,20 */
    /* 43 */ c[43]=0xE9;
    int32_t rel = (int32_t)((int64_t)(mn_hook + 6) - (int64_t)(mn_cave + 48));
    memcpy(c + 44, &rel, 4);
    c[48] = g_free ? 1 : 0;
    memcpy(c + 56, &g_mmult, 8);
    if (!wr(mn_cave, c, sizeof c)) { setstatus("No pude escribir la cueva"); return false; }

    unsigned char h[6] = {0xE9, 0, 0, 0, 0, 0x90};
    int32_t hr = (int32_t)((int64_t)mn_cave - (int64_t)(mn_hook + 5));
    memcpy(h + 1, &hr, 4);
    if (!wr_code(mn_hook, h, sizeof h)) { setstatus("No pude escribir el hook"); return false; }
    return true;
}
static bool money_disable(void)
{
    if (!mn_hook) return true;
    return wr_code(mn_hook, MN_OFF, sizeof MN_OFF);
}

/* ---------- sesion ---------- */
#define NTOG 5
static bool g_tOn[NTOG];

static void reset_session(void)
{
    if (g_proc) CloseHandle(g_proc);
    g_proc = NULL; g_pid = 0; g_base = 0; g_size = 0;
    init_patches();
    tm_hook = tm_cave = mn_hook = mn_cave = 0;
    for (int k = 0; k < NTOG; k++) g_tOn[k] = false;
}

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
        reset_session();
    }
    DWORD pid = find_pid(GAME_EXE);
    if (!pid) { snprintf(g_err, sizeof g_err, "esperando el juego"); return false; }
    HANDLE h = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) { snprintf(g_err, sizeof g_err, "sin acceso (ejecuta como admin)"); return false; }
    uintptr_t base = 0; size_t size = 0;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me; me.dwSize = sizeof me;
        if (Module32First(s, &me)) {
            do {
                if (_stricmp(me.szModule, GA_DLL) == 0) { base = (uintptr_t)me.modBaseAddr; size = me.modBaseSize; break; }
            } while (Module32Next(s, &me));
        }
        CloseHandle(s);
    }
    if (!base) {
        CloseHandle(h);
        snprintf(g_err, sizeof g_err, "cargando %s...", GA_DLL);
        return false;
    }
    g_proc = h; g_pid = pid; g_base = base; g_size = size;
    return true;
}

static void disable_all(void)
{
    if (!g_proc) return;
    DWORD ec = 0;
    if (!GetExitCodeProcess(g_proc, &ec) || ec != STILL_ACTIVE) return;
    if (g_tOn[0]) apply(g_luck, 1, false);
    if (g_tOn[1]) apply(g_jack, 1, false);
    if (g_tOn[2]) apply(g_afford, 3, false);
    if (g_tOn[3]) ticket_disable();
    if (g_tOn[4]) money_disable();
}

/* ---------- filas (7 por pagina, como BalMenu) ---------- */
enum { R_PTR, R_TOG, R_TVAL };
typedef struct { const char *label; int kind; VType type; int n; unsigned off[5]; int k; } Row;

/* offsets en el MISMO orden que la tabla de Cheat Engine (el ultimo se aplica primero) */
#define PR(l, t, n, ...) {l, R_PTR, t, n, {__VA_ARGS__}, 0}
#define TG(l, k)         {l, R_TOG, T_I32, 0, {0}, k}
#define TV(l, k)         {l, R_TVAL, T_F32, 0, {0}, k}

static const Row PG1[] = {
    PR("DINERO RUN ($)",   T_F64, 4, 0x10, 0x30, 0x0, 0xB8),
    PR("ALMAS (SOULS)",    T_I32, 4, 0x9C, 0x30, 0x0, 0xB8),
    PR("BATERIA EGG",      T_F32, 4, 0xB0, 0x30, 0x0, 0xB8),
    PR("CARGA VENTILADOR", T_F32, 4, 0xA8, 0x30, 0x0, 0xB8),
    PR("TIEMPO MAQUINA",   T_F32, 4, 0x98, 0x30, 0x0, 0xB8),
    PR("JP (META)",        T_I32, 3, 0x48, 0x0, 0xB8),
    PR("TICKETS (BONUS)",  T_F64, 3, 0xF0, 0x0, 0xB8),
};
static const Row PG2[] = {
    TG("SUERTE MAX 9999", 0),
    TG("TODO JACKPOT",    1),
    TG("SIEMPRE PAGO",    2),
    TG("MULT. TICKETS",   3),
    TV("FACTOR TICKETS",  0),
    TG("DINERO NO BAJA",  4),
    TV("FACTOR DINERO",   1),
};
static const Row PG3[] = {
    TV("COMPRAS GRATIS",   2),
    PR("TICKETS HECHOS",   T_I32, 4, 0x20, 0x30, 0x0, 0xB8),
    PR("JACKPOTS RUN",     T_I32, 4, 0x48, 0x30, 0x0, 0xB8),
    PR("SUPER JACKPOTS",   T_I32, 4, 0x50, 0x30, 0x0, 0xB8),
    PR("TIEMPO RUN (S)",   T_F32, 4, 0x18, 0x30, 0x0, 0xB8),
    PR("ULTIMA META ($)",  T_F64, 4, 0x58, 0x30, 0x0, 0xB8),
    PR("DINERO META ($)",  T_F64, 4, 0x60, 0x30, 0x0, 0xB8),
};
static const Row PG4[] = {
    PR("FINAL CHANCE 0/1", T_U8,  4, 0x81, 0x30, 0x0, 0xB8),
    PR("MAP JACKPOT 0/1",  T_U8,  4, 0x82, 0x30, 0x0, 0xB8),
    PR("MAP CALL 0/1",     T_U8,  4, 0x83, 0x30, 0x0, 0xB8),
    PR("ACTO (1-5)",       T_I32, 3, 0x4C, 0x0, 0xB8),
    PR("PRESTIGIOS",       T_I32, 3, 0x38, 0x0, 0xB8),
    PR("RUNS COMPLETAS",   T_I32, 3, 0x40, 0x0, 0xB8),
    PR("TIEMPO TOTAL (S)", T_F32, 3, 0x28, 0x0, 0xB8),
};
static const Row PG5[] = {
    PR("MUERTES",          T_I32, 3, 0xE4, 0x0, 0xB8),
    PR("PRESTAMOS",        T_I32, 3, 0xE8, 0x0, 0xB8),
    PR("MUERTE FINAL CH.", T_I32, 3, 0x50, 0x0, 0xB8),
    PR("PRESTIGIO GASTO",  T_I32, 3, 0x78, 0x0, 0xB8),
    PR("VENTILADOR PAUSA", T_U8,  4, 0xAC, 0x30, 0x0, 0xB8),
    PR("NIVEL MAQUINA",    T_I32, 4, 0x90, 0x30, 0x0, 0xB8),
    PR("ALIMENT. MAQUINA", T_I32, 4, 0x94, 0x30, 0x0, 0xB8),
};
static const Row PG6[] = {
    PR("TINTA IMPRESORA",  T_I32, 4, 0xD8, 0x30, 0x0, 0xB8),
    PR("RECARGAS TINTA",   T_I32, 4, 0xDC, 0x30, 0x0, 0xB8),
    PR("COMB. HORNO",      T_F64, 4, 0xE0, 0x30, 0x0, 0xB8),
    PR("MUNDO MUERTO 0/1", T_U8,  4, 0xC0, 0x30, 0x0, 0xB8),
    PR("BASURERO MUERTO",  T_U8,  4, 0xC1, 0x30, 0x0, 0xB8),
};

typedef struct { const char *name; const Row *r; int n; } Page;
#define NPAGES 6
#define NROWS  7
static const Page g_pages[NPAGES] = {
    {"EDITABLES",      PG1, 7},
    {"TOGGLES",        PG2, 7},
    {"COMPRAS Y RUN",  PG3, 7},
    {"RUN Y META",     PG4, 7},
    {"META Y GADGETS", PG5, 7},
    {"GADGETS",        PG6, 5},
};

/* ---------- valores ---------- */
static int vsize(VType t) { return t == T_F64 ? 8 : (t == T_U8 ? 1 : 4); }

static bool resolve(const Row *e, uintptr_t *out)
{
    uintptr_t addr = g_base + (uintptr_t)BASE_OFF, p = 0;
    for (int i = e->n - 1; i >= 0; i--) {
        if (!rd(addr, &p, 8) || p < 0x10000) return false;
        addr = p + e->off[i];
    }
    *out = addr;
    return true;
}

/* numero corto para la columna de 68 px (max ~8 caracteres) */
static void short_num(double v, char *out, size_t n)
{
    double a = v < 0 ? -v : v;
    if (a == (double)(long long)a && a < 1e8) snprintf(out, n, "%lld", (long long)v);
    else if (a < 1e5)                         snprintf(out, n, "%.2f", v);
    else                                      snprintf(out, n, "%.2e", v);
}

static bool read_row(const Row *e, char *out, size_t n)
{
    unsigned char b[8] = {0};
    uintptr_t a;
    if (!g_proc || !resolve(e, &a) || !rd(a, b, (size_t)vsize(e->type))) return false;
    switch (e->type) {
    case T_I32: short_num((double)*(int32_t *)b, out, n); break;
    case T_F32: short_num((double)*(float *)b, out, n); break;
    case T_F64: short_num(*(double *)b, out, n); break;
    default:    snprintf(out, n, "%u", (unsigned)b[0]); break;
    }
    return true;
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

/* ---------- UI (clon del diseño de BalMenu) ---------- */
#define ID_PREV   5001
#define ID_NEXT   5002
#define ID_TITLE  7001
#define ID_CREDIT 7002
#define ID_PTITLE 7003
#define ID_STATUS 7004
#define ID_LOG    7005
#define ID_VAL    4000   /* + slot */
#define ID_EDIT   2000
#define ID_BTNA   3000
#define ID_BTNB   3100
#define ID_LBL    6000

static HWND g_hwnd, g_status, g_ptitle;
static HFONT g_fReg, g_fBold, g_fSmall;
static HBRUSH g_bgBrush, g_editBrush;
static HWND g_lbl[NROWS], g_val[NROWS], g_edit[NROWS], g_btnA[NROWS], g_btnB[NROWS];
static bool g_valOk[NROWS];
static char g_valTxt[NROWS][24];
static bool g_connected;
static int  g_page = 0;
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

static void place(HWND h, int x, int y, int w, int hh)
{
    SetWindowPos(h, NULL, S(x), S(y), S(w), S(hh), SWP_NOZORDER);
}

/* acomoda las 7 filas segun el tipo de fila de la pagina */
static void layout_page(void)
{
    const Page *pg = &g_pages[g_page];
    char t[64];
    snprintf(t, sizeof t, "PAGINA %d/%d: %s", g_page + 1, NPAGES, pg->name);
    SetWindowTextA(g_ptitle, t);
    for (int i = 0; i < NROWS; i++) {
        int y = 76 + 28 * i;
        if (i >= pg->n) {
            ShowWindow(g_lbl[i], SW_HIDE); ShowWindow(g_val[i], SW_HIDE);
            ShowWindow(g_edit[i], SW_HIDE); ShowWindow(g_btnA[i], SW_HIDE); ShowWindow(g_btnB[i], SW_HIDE);
            continue;
        }
        const Row *r = &pg->r[i];
        SetWindowTextA(g_lbl[i], r->label);
        ShowWindow(g_lbl[i], SW_SHOW);
        ShowWindow(g_val[i], SW_SHOW);
        place(g_lbl[i], 10, y + 3, 140, 20);
        place(g_val[i], 152, y + 3, 68, 20);
        g_valTxt[i][0] = '\0';
        SetWindowTextA(g_edit[i], "");
        if (r->kind == R_TOG) {
            ShowWindow(g_edit[i], SW_HIDE);
            SetWindowTextA(g_btnA[i], "ACT");
            SetWindowTextA(g_btnB[i], "DES");
            place(g_btnA[i], 224, y, 72, 22);
            place(g_btnB[i], 304, y, 72, 22);
            ShowWindow(g_btnA[i], SW_SHOW);
            ShowWindow(g_btnB[i], SW_SHOW);
        } else {
            SetWindowTextA(g_btnA[i], "SET");
            place(g_edit[i], 224, y, 94, 22);
            place(g_btnA[i], 324, y, 52, 22);
            ShowWindow(g_edit[i], SW_SHOW);
            ShowWindow(g_btnA[i], SW_SHOW);
            ShowWindow(g_btnB[i], SW_HIDE);
        }
    }
    InvalidateRect(g_hwnd, NULL, TRUE);
}

/* refresca estado de conexion y columna de valores */
static void refresh_ui(void)
{
    char t[200];
    bool ok = attach();
    g_connected = ok;
    if (ok) snprintf(t, sizeof t, "[+] %s  PID %lu  (Conectado)", GAME_EXE, (unsigned long)g_pid);
    else    snprintf(t, sizeof t, "[-] %s: %s", GAME_EXE, g_err);
    char cur[200];
    GetWindowTextA(g_status, cur, sizeof cur);
    if (strcmp(cur, t) != 0) { SetWindowTextA(g_status, t); InvalidateRect(g_status, NULL, TRUE); }

    const Page *pg = &g_pages[g_page];
    for (int i = 0; i < pg->n; i++) {
        const Row *r = &pg->r[i];
        char v[24] = "--";
        bool vok = false;
        if (r->kind == R_TOG) {
            snprintf(v, sizeof v, "%s", g_tOn[r->k] ? "ACTIVO" : "INACTIVO");
            vok = g_tOn[r->k];
        } else if (r->kind == R_TVAL) {
            if (r->k == 0) short_num((double)g_tmult, v, sizeof v);
            else if (r->k == 1) short_num(g_mmult, v, sizeof v);
            else snprintf(v, sizeof v, "%u", (unsigned)g_free);
            vok = true;
        } else if (ok && read_row(r, v, sizeof v)) {
            vok = true;
        } else {
            snprintf(v, sizeof v, "--");
        }
        if (strcmp(v, g_valTxt[i]) != 0 || vok != g_valOk[i]) {
            snprintf(g_valTxt[i], sizeof g_valTxt[i], "%s", v);
            g_valOk[i] = vok;
            SetWindowTextA(g_val[i], v);
            InvalidateRect(g_val[i], NULL, TRUE);
        }
    }
}

static void do_set_ptr(const Row *e, const char *txt)
{
    double v;
    if (!parse_double(txt, &v)) { setstatus("%s: valor invalido", e->label); return; }
    if (!attach()) { setstatus("%s: %s", e->label, g_err); return; }
    uintptr_t a;
    if (!resolve(e, &a)) { setstatus("%s: entra a una partida", e->label); return; }
    bool ok;
    switch (e->type) {
    case T_I32: { int32_t x = (int32_t)(int64_t)v; ok = wr(a, &x, 4); break; }
    case T_F32: { float x = (float)v;             ok = wr(a, &x, 4); break; }
    case T_F64: {                                 ok = wr(a, &v, 8); break; }
    default:    { unsigned char x = (unsigned char)(int)v; ok = wr(a, &x, 1); break; }
    }
    if (ok) setstatus("%s = %s (OK)", e->label, txt);
    else    setstatus("%s: no pude escribir", e->label);
}

static void do_set_tval(const Row *e, const char *txt)
{
    double v;
    if (!parse_double(txt, &v)) { setstatus("%s: valor invalido", e->label); return; }
    if (e->k == 0) {
        g_tmult = (float)v;
        if (tm_cave && attach()) wr(tm_cave + 32, &g_tmult, 4);
    } else if (e->k == 1) {
        g_mmult = v;
        if (mn_cave && attach()) wr(mn_cave + 56, &g_mmult, 8);
    } else {
        g_free = (v != 0.0) ? 1 : 0;
        if (mn_cave && attach()) wr(mn_cave + 48, &g_free, 1);
    }
    setstatus("%s = %s (OK)", e->label, txt);
}

static void do_toggle(const Row *e, bool en)
{
    int k = e->k;
    if (!attach()) { setstatus("%s: %s", e->label, g_err); return; }
    if (!en && !g_tOn[k]) { setstatus("%s: Desactivado", e->label); return; }
    bool ok = false;
    switch (k) {
    case 0: ok = apply(g_luck, 1, en); break;
    case 1: ok = apply(g_jack, 1, en); break;
    case 2: ok = apply(g_afford, 3, en); break;
    case 3: ok = en ? ticket_enable() : ticket_disable(); break;
    case 4: ok = en ? money_enable()  : money_disable();  break;
    }
    if (ok) {
        g_tOn[k] = en;
        setstatus("%s: %s", e->label, en ? "Activado" : "Desactivado");
    }
}

static void on_button(int slot, bool second)
{
    const Page *pg = &g_pages[g_page];
    if (slot >= pg->n) return;
    const Row *r = &pg->r[slot];
    if (r->kind == R_TOG) { do_toggle(r, !second); refresh_ui(); return; }
    if (second) return;
    char txt[64];
    GetWindowTextA(g_edit[slot], txt, sizeof txt);
    if (txt[0] == '\0') { setstatus("%s: ingresa un valor", r->label); return; }
    if (r->kind == R_PTR) do_set_ptr(r, txt); else do_set_tval(r, txt);
    refresh_ui();
}

static void show_page(int p)
{
    g_page = (p % NPAGES + NPAGES) % NPAGES;
    layout_page();
    refresh_ui();
}

static void build_ui(HWND hw)
{
    mk(hw, "STATIC", "> ScritchyMenu by Mr.Bat", SS_NOPREFIX, 10, 6, 366, 20, ID_TITLE, g_fBold);
    mk(hw, "STATIC", "Agradecimiento especial a OWSEEX", SS_NOPREFIX, 10, 27, 366, 16, ID_CREDIT, g_fSmall);
    mk(hw, "BUTTON", "<", BS_OWNERDRAW, 10, 48, 40, 22, ID_PREV, g_fReg);
    g_ptitle = mk(hw, "STATIC", "", SS_CENTER | SS_NOPREFIX, 55, 51, 276, 18, ID_PTITLE, g_fBold);
    mk(hw, "BUTTON", ">", BS_OWNERDRAW, 336, 48, 40, 22, ID_NEXT, g_fReg);
    for (int i = 0; i < NROWS; i++) {
        int y = 76 + 28 * i;
        g_lbl[i]  = mk(hw, "STATIC", "", SS_NOPREFIX, 10, y + 3, 140, 20, ID_LBL + i, g_fReg);
        g_val[i]  = mk(hw, "STATIC", "--", SS_NOPREFIX, 152, y + 3, 68, 20, ID_VAL + i, g_fReg);
        g_edit[i] = mk(hw, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 224, y, 94, 22, ID_EDIT + i, g_fReg);
        if (!g_oldEdit) g_oldEdit = (WNDPROC)GetWindowLongPtr(g_edit[i], GWLP_WNDPROC);
        SetWindowLongPtr(g_edit[i], GWLP_WNDPROC, (LONG_PTR)EditProc);
        g_btnA[i] = mk(hw, "BUTTON", "SET", BS_OWNERDRAW, 324, y, 52, 22, ID_BTNA + i, g_fReg);
        g_btnB[i] = mk(hw, "BUTTON", "DES", BS_OWNERDRAW, 304, y, 72, 22, ID_BTNB + i, g_fReg);
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
        show_page(0);
        SetTimer(hw, 1, 700, NULL);
        return 0;
    case WM_TIMER:
        refresh_ui();
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        HWND c = (HWND)l;
        int id = GetDlgCtrlID(c);
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
        if (id == ID_PREV) show_page(g_page - 1);
        else if (id == ID_NEXT) show_page(g_page + 1);
        else if (id >= ID_BTNA && id < ID_BTNA + NROWS) on_button(id - ID_BTNA, false);
        else if (id >= ID_BTNB && id < ID_BTNB + NROWS) on_button(id - ID_BTNB, true);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hw, 1);
        disable_all();
        if (g_proc) CloseHandle(g_proc);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hw, m, w, l);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    (void)hp; (void)cmd;
    init_patches();

    /* DPI: igual que BalMenu, todo se escala con MulDiv */
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
    wc.lpszClassName = "ScritchyMenuWnd";
    RegisterClassA(&wc);

    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;   /* 0xCA0000, como BalMenu */
    RECT r = {0, 0, S(386), S(326)};
    AdjustWindowRect(&r, style, FALSE);
    HWND hw = CreateWindowExA(0, "ScritchyMenuWnd", APP_TITLE, style,
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
