/* BalMenu by Mr.Bat - Trainer para Balatro.exe (Windows x64)
 * Agradecimiento especial a VoidUzumaki
 * Diseno visual y paleta basados en UnderMenu
 * Soporte para Paginacion y Botones de Activacion / Desactivacion (ACT / DES)
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE        "BalMenu by Mr.Bat"
#define VERSION_TEXT "V.1.0"
#define THANKS_TEXT  "Agradecimiento especial a VoidUzumaki"
#define PROC_NAME    "Balatro.exe"
#define LUA_DLL      "lua51.dll"

/* Colores de diseno guardados */
#define C_BG    RGB(58, 58, 58)
#define C_FIELD RGB(31, 31, 31)
#define C_FG    RGB(51, 255, 51)
#define C_DIM   RGB(31, 159, 31)

/* Identificadores de controles */
enum {
    ID_TITLE = 5002, ID_THANKS = 5003, ID_PAGE_LBL = 5004,
    ID_PREV = 5005, ID_NEXT = 5006,
    ID_STATUS = 5000, ID_MSG = 5001,
    ID_LABEL_BASE = 6000, ID_READ_BASE = 4000,
    ID_EDIT_BASE = 2000, ID_BTN_BASE = 3000, ID_BTNDES_BASE = 3100,
    TIMER_ID = 1
};

#define ROWS_PER_PAGE 7
#define TOTAL_PAGES   6

/* Definicion de tipo de cheat */
typedef enum {
    CHEAT_INPUT_NUM, /* Requiere numero en el edit box */
    CHEAT_ACTION     /* Accion o toggle con botones ACT y DES */
} CheatType;

typedef struct {
    const char *name;
    const char *category;
    CheatType   type;
    int         page;         /* 0 a 5 */
    const char *default_val;  /* Valor inicial para el edit box */
    const char *lua_code;     /* Formato Lua con %s o codigo directo de activacion */
    const char *lua_disable;  /* Codigo Lua de desactivacion */
} Cheat;

/* Lista completa de los 36 cheats con soporte para ACTIVAR y DESACTIVAR */
static const Cheat CHEATS[] = {
    /* --- PAGINA 1: PROBABILIDADES --- */
    { "PROB. ESPECTRAL",  "PROBABILIDADES", CHEAT_INPUT_NUM, 0, "4",  "G.GAME.spectral_rate = %s", NULL },
    { "PROB. PLANETAS",   "PROBABILIDADES", CHEAT_INPUT_NUM, 0, "4",  "G.GAME.planet_rate = %s", NULL },
    { "PROB. TAROT",      "PROBABILIDADES", CHEAT_INPUT_NUM, 0, "4",  "G.GAME.tarot_rate = %s", NULL },
    { "PROB. JOKERS",     "PROBABILIDADES", CHEAT_INPUT_NUM, 0, "20", "G.GAME.joker_rate = %s", NULL },
    { "PROB. CARTAS",     "PROBABILIDADES", CHEAT_INPUT_NUM, 0, "5",  "G.GAME.playing_card_rate = %s", NULL },
    { "PROB. ALQUILER",   "PROBABILIDADES", CHEAT_INPUT_NUM, 0, "3",  "G.GAME.rental_rate = %s", NULL },
    { "PROB. EDICION",    "PROBABILIDADES", CHEAT_INPUT_NUM, 0, "10", "G.GAME.edition_rate = %s", NULL },

    /* --- PAGINA 2: PARTIDA Y ANTE --- */
    { "DINERO / DOLARES", "PARTIDA / ANTE", CHEAT_INPUT_NUM, 1, "50000", "G.GAME.dollars = %s", NULL },
    { "ANTE ACTUAL",      "PARTIDA / ANTE", CHEAT_INPUT_NUM, 1, "1",     "G.GAME.round_resets.ante = %s", NULL },
    { "ANTE VICTORIA",    "PARTIDA / ANTE", CHEAT_INPUT_NUM, 1, "8",     "G.GAME.win_ante = %s", NULL },
    { "DESCARTES",        "PARTIDA / ANTE", CHEAT_INPUT_NUM, 1, "20",    "G.GAME.round_resets.discards = %s\nG.GAME.current_round.discards_left = %s", NULL },
    { "MANOS RESTANTES",  "PARTIDA / ANTE", CHEAT_INPUT_NUM, 1, "20",    "G.GAME.round_resets.hands = %s\nG.GAME.current_round.hands_left = %s", NULL },
    { "RESET DESC. USADOS","PARTIDA / ANTE",CHEAT_ACTION,    1, "-",     "G.GAME.current_round.discards_used = 0", "G.GAME.current_round.discards_used = 1" },
    { "SCORE A TARGET",   "PARTIDA / ANTE", CHEAT_ACTION,    1, "-",     "G.GAME.chips = G.GAME.blind.chips", "G.GAME.chips = 0" },

    /* --- PAGINA 3: TIENDA --- */
    { "TAMANO TIENDA",    "TIENDA",         CHEAT_INPUT_NUM, 2, "6",  "local mod = %s - G.GAME.shop.joker_max\nif G.GAME.shop.joker_max+mod > 0 then change_shop_size(mod) end", NULL },
    { "REROLLS GRATIS",   "TIENDA",         CHEAT_INPUT_NUM, 2, "10", "G.GAME.current_round.free_rerolls = %s", NULL },
    { "REROLLS INFINITOS","TIENDA",         CHEAT_ACTION,    2, "-",
      "bkpTbl = bkpTbl or {}\nlocal name = 'calculate_reroll_cost'\nbkpTbl[name] = _G[name]\n_G[name] = function(...) G.GAME.current_round.reroll_cost = 0; return end",
      "bkpTbl = bkpTbl or {}\nlocal name = 'calculate_reroll_cost'\nif bkpTbl and bkpTbl[name] then _G[name] = bkpTbl[name]; bkpTbl[name] = nil end" },
    { "COSTO REROLL A 0", "TIENDA",         CHEAT_ACTION,    2, "-",
      "G.GAME.current_round.reroll_cost_increase = 0\nG.GAME.current_round.reroll_cost = 0",
      "G.GAME.current_round.reroll_cost_increase = 1\nG.GAME.current_round.reroll_cost = 5" },
    { "MANO TEMPORAL",    "TIENDA",         CHEAT_INPUT_NUM, 2, "12", "G.shop.role.major.config.temp_limit = %s", NULL },
    { "MANO PERMANENTE",  "TIENDA",         CHEAT_INPUT_NUM, 2, "12", "G.shop.role.major.config.card_limit = %s\nG.shop.role.major.config.real_card_limit = %s", NULL },
    { "LIMITE HIGHLIGHT", "TIENDA",         CHEAT_INPUT_NUM, 2, "10", "G.shop.role.major.config.highlighted_limit = %s", NULL },

    /* --- PAGINA 4: PAQUETES --- */
    { "PAQUETES GRATIS",  "PAQUETES",       CHEAT_ACTION,    3, "-",
      "if G.shop_booster and G.shop_booster.cards then for i, card in pairs(G.shop_booster.cards) do card.cost = 0; card.extra_cost = 0 end end",
      "if G.shop_booster and G.shop_booster.cards then for i, card in pairs(G.shop_booster.cards) do card.cost = card.base_cost or 4; card.extra_cost = 0 end end" },
    { "PAQUETE ESPECTRAL","PAQUETES",       CHEAT_ACTION,    3, "-",
      "if G.shop_booster and G.shop_booster.cards then for i, card in pairs(G.shop_booster.cards) do local sc = G.shop_booster.cards[i]; local nc = G.P_CENTERS.p_spectral_mega_1; sc.config.center = nc; sc.config.center_key = nc.key; end end",
      "if G.shop_booster and G.shop_booster.cards then for i, card in pairs(G.shop_booster.cards) do local sc = G.shop_booster.cards[i]; local nc = G.P_CENTERS.p_buffoon_mega_1; if nc then sc.config.center = nc; sc.config.center_key = nc.key end end end" },
    { "CARTAS A ELEGIR",  "PAQUETES",       CHEAT_INPUT_NUM, 3, "3",  "local amt = %s\nif G.shop_booster and G.shop_booster.cards then for i, card in pairs(G.shop_booster.cards) do local sc = G.shop_booster.cards[i]; local nc = {}; for k,v in pairs(sc.config.center.config) do nc[k]=v end; nc.choose = amt; sc.config.center = {config=nc, key=sc.config.center.key}; end end", NULL },
    { "PAQUETES 2/10",    "PAQUETES",       CHEAT_ACTION,    3, "-",
      "if G.shop_booster and G.shop_booster.cards then for i, card in pairs(G.shop_booster.cards) do local sc = G.shop_booster.cards[i]; local nc = {}; for k,v in pairs(G.P_CENTERS.p_spectral_mega_1.config) do nc[k]=v end; nc.choose = 2; nc.extra = 10; sc.config.center = {config=nc, key='p_spectral_mega_1'}; sc.cost = 0; end end",
      "if G.shop_booster and G.shop_booster.cards then for i, card in pairs(G.shop_booster.cards) do card.cost = 4 end end" },
    { "FORZAR THE SOUL",  "PAQUETES",       CHEAT_ACTION,    3, "-",
      "bkpTbl = bkpTbl or {}\nlocal name = 'create_card'\nbkpTbl[name] = _G[name]\nbkpTbl.vars = bkpTbl.vars or {}\nbkpTbl.vars.card = 'c_soul'\nlocal orig = bkpTbl[name]\n_G[name] = function(_t, a, l, _r, sm, s, fk, ka) if _t == 'Spectral' then fk = bkpTbl.vars.card end return orig(_t, a, l, _r, sm, s, fk, ka) end",
      "bkpTbl = bkpTbl or {}; bkpTbl.vars = bkpTbl.vars or {}; bkpTbl.vars.card = nil; local name = 'create_card'; if bkpTbl and bkpTbl[name] then _G[name] = bkpTbl[name]; bkpTbl[name] = nil end" },
    { "FORZAR BLACK HOLE","PAQUETES",       CHEAT_ACTION,    3, "-",
      "bkpTbl = bkpTbl or {}; bkpTbl.vars = bkpTbl.vars or {}; bkpTbl.vars.card = 'c_black_hole'",
      "bkpTbl = bkpTbl or {}; bkpTbl.vars = bkpTbl.vars or {}; bkpTbl.vars.card = 'c_soul'" },
    { "REROLL TIENDA",    "PAQUETES",       CHEAT_ACTION,    3, "-",  "if G.FUNCS.reroll_shop then G.FUNCS.reroll_shop() end", "-- reroll ejecutado" },

    /* --- PAGINA 5: POKER Y JOKERS --- */
    { "MANOS INFINITAS",  "POKER Y JOKERS", CHEAT_ACTION,    4, "-",
      "bkpTbl = bkpTbl or {}\nlocal name = 'ease_hands_played'\nbkpTbl[name] = _G[name]\nlocal orig = bkpTbl[name]\n_G[name] = function(...) local a = {...}; if a[1] > 0 then return orig(...) else return orig(0, select(2, ...)) end end",
      "bkpTbl = bkpTbl or {}\nlocal name = 'ease_hands_played'\nif bkpTbl and bkpTbl[name] then _G[name] = bkpTbl[name]; bkpTbl[name] = nil end" },
    { "DESCARTES INF.",   "POKER Y JOKERS", CHEAT_ACTION,    4, "-",
      "bkpTbl = bkpTbl or {}\nlocal name = 'ease_discard'\nbkpTbl[name] = _G[name]\nlocal orig = bkpTbl[name]\n_G[name] = function(...) local a = {...}; if a[1] > 0 then return orig(...) else return orig(0, select(2, ...)) end end",
      "bkpTbl = bkpTbl or {}\nlocal name = 'ease_discard'\nif bkpTbl and bkpTbl[name] then _G[name] = bkpTbl[name]; bkpTbl[name] = nil end" },
    { "PUNTUACION CHIPS", "POKER Y JOKERS", CHEAT_INPUT_NUM, 4, "1000000", "G.GAME.chips = %s", NULL },
    { "MAXIMO JOKERS",    "POKER Y JOKERS", CHEAT_INPUT_NUM, 4, "10", "G.jokers.config.card_limit = %s\nG.jokers.Mid.config.card_limit = %s", NULL },
    { "JOKER NEGATIVO",   "POKER Y JOKERS", CHEAT_ACTION,    4, "-",
      "bkpTbl = bkpTbl or {}\nlocal name = 'poll_edition'\nbkpTbl[name] = _G[name]\nlocal orig = bkpTbl[name]\n_G[name] = function(...) local a1 = ...; if type(a1) == 'string' and a1:sub(1,3) == 'edi' then return {negative = true} else return orig(...) end end",
      "bkpTbl = bkpTbl or {}\nlocal name = 'poll_edition'\nif bkpTbl and bkpTbl[name] then _G[name] = bkpTbl[name]; bkpTbl[name] = nil end" },
    { "REROLL JEFE",      "POKER Y JOKERS", CHEAT_ACTION,    4, "-",  "if G.FUNCS.reroll_boss then G.FUNCS.reroll_boss() end", "-- reroll jefe ejecutado" },
    { "WIN GAME",         "POKER Y JOKERS", CHEAT_ACTION,    4, "-",  "win_game()", "-- win_game ejecutado" },

    /* --- PAGINA 6: FAVORITOS --- */
    { "SET MIS FAVORITOS","FAVORITOS",      CHEAT_ACTION,    5, "-",
      "G.GAME.dollars = 500\nG.jokers.config.card_limit = 10\nG.jokers.Mid.config.card_limit = 10\nG.GAME.round_resets.hands = 20\nG.GAME.current_round.hands_left = 20\nG.GAME.round_resets.discards = 20\nG.GAME.current_round.discards_left = 20\nG.FUNCS.show_center_message('BalMenu: Favoritos Aplicados!', false, {hold = 5})",
      "G.GAME.dollars = 10\nG.jokers.config.card_limit = 5\nG.jokers.Mid.config.card_limit = 5\nG.GAME.round_resets.hands = 4\nG.GAME.current_round.hands_left = 4\nG.GAME.round_resets.discards = 3\nG.GAME.current_round.discards_left = 3" },
    { "DOLARES RAPIDO",   "FAVORITOS",      CHEAT_INPUT_NUM, 5, "99999", "G.GAME.dollars = %s", NULL },
    { "JOKERS RAPIDO",    "FAVORITOS",      CHEAT_INPUT_NUM, 5, "15",    "G.jokers.config.card_limit = %s\nG.jokers.Mid.config.card_limit = %s", NULL },
    { "CHIPS RAPIDO",     "FAVORITOS",      CHEAT_INPUT_NUM, 5, "50000000", "G.GAME.chips = %s", NULL },
    { "ANTE RAPIDO",      "FAVORITOS",      CHEAT_INPUT_NUM, 5, "1",     "G.GAME.round_resets.ante = %s", NULL },
    { "REROLL BOSS RAPIDO","FAVORITOS",     CHEAT_ACTION,    5, "-",     "if G.FUNCS.reroll_boss then G.FUNCS.reroll_boss() end", "-- reroll boss ejecutado" },
    { "GANAR RONDA",      "FAVORITOS",      CHEAT_ACTION,    5, "-",     "G.GAME.chips = G.GAME.blind.chips", "G.GAME.chips = 0" }
};
#define N_CHEATS ((int)(sizeof(CHEATS) / sizeof(CHEATS[0])))

/* Nombres de categorias de paginas */
static const char *PAGE_TITLES[TOTAL_PAGES] = {
    "PAGINA 1/6: PROBABILIDADES",
    "PAGINA 2/6: PARTIDA Y ANTE",
    "PAGINA 3/6: TIENDA Y REROLLS",
    "PAGINA 4/6: PAQUETES TIENDA",
    "PAGINA 5/6: POKER Y JOKERS",
    "PAGINA 6/6: FAVORITOS Y ACCIONES"
};

/* Referencias a simbolos ensamblados en shellcode.s */
extern char Shellcode_Start[];
extern char Shellcode_End[];
extern uint64_t Offset_HookEntry_Val;
extern uint64_t Offset_HookFn_Val;
extern uint64_t Offset_luaCmdBuf_Val;

/* Estado global de conexion con Balatro.exe */
static struct {
    DWORD     pid;
    HANDLE    hProc;
    uintptr_t baseDll;
    uintptr_t pResume;        /* Direccion de lua_resume */
    uintptr_t pRemoteMem;     /* Memoria remota asignada para el shellcode */
    uint8_t   origBytes[14];  /* 14 bytes originales de lua_resume */
    BOOL      hooked;
    char      err[128];
} G;

/* Estado activo/inactivo por cada cheat (0: inactivo/default, 1: activo) */
static int cheatState[N_CHEATS];
static char cheatVal[N_CHEATS][32];

/* Handles de la interfaz */
static HWND hTitle, hThanks, hPageLbl, hPrevBtn, hNextBtn;
static HWND hLbl[ROWS_PER_PAGE], hRead[ROWS_PER_PAGE], hEdit[ROWS_PER_PAGE];
static HWND hBtn[ROWS_PER_PAGE], hBtnDes[ROWS_PER_PAGE];
static HWND hStatus, hMsg;
static HBRUSH brBg, brField;
static HFONT font, fontBold, fontSmall;
static int dpi = 96;
static int curPage = 0;
static COLORREF statusFg = C_DIM;
static WNDPROC oldEditProc;

#define S(x) MulDiv((x), dpi, 96)

/* Prototipos */
static void update_page_view(void);
static void refresh_status(void);
static BOOL send_lua(const char *code, char *errOut, size_t errSize);

/* ---------- Utilidades de Ventana ---------- */
static void set_text(HWND h, const char *t) {
    char cur[256];
    GetWindowTextA(h, cur, sizeof cur);
    if (strcmp(cur, t) != 0) SetWindowTextA(h, t);
}

/* ---------- Busqueda de Proceso y Modulo ---------- */
static DWORD find_pid(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD pid = 0;
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 e;
    e.dwSize = sizeof e;
    for (BOOL ok = Process32First(snap, &e); ok; ok = Process32Next(snap, &e)) {
        if (_stricmp(e.szExeFile, PROC_NAME) == 0) {
            pid = e.th32ProcessID;
            break;
        }
    }
    CloseHandle(snap);
    return pid;
}

static uintptr_t find_module_base(DWORD pid, const char *modName) {
    HANDLE snap = INVALID_HANDLE_VALUE;
    uintptr_t base = 0;
    for (int i = 0; i < 10; i++) {
        snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_BAD_LENGTH) return 0;
        Sleep(10);
    }
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32 e;
    e.dwSize = sizeof e;
    for (BOOL ok = Module32First(snap, &e); ok; ok = Module32Next(snap, &e)) {
        if (_stricmp(e.szModule, modName) == 0) {
            base = (uintptr_t)e.modBaseAddr;
            break;
        }
    }
    CloseHandle(snap);
    return base;
}

/* Resolucion de funciones exportadas en lua51.dll remoto */
static uintptr_t get_remote_proc_address(HANDLE hProc, uintptr_t modBase, const char *funcName) {
    IMAGE_DOS_HEADER dos;
    if (!ReadProcessMemory(hProc, (LPCVOID)modBase, &dos, sizeof dos, NULL) || dos.e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    IMAGE_NT_HEADERS64 nt;
    if (!ReadProcessMemory(hProc, (LPCVOID)(modBase + dos.e_lfanew), &nt, sizeof nt, NULL) || nt.Signature != IMAGE_NT_SIGNATURE)
        return 0;

    DWORD expRVA = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD expSize = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    if (!expRVA || !expSize) return 0;

    IMAGE_EXPORT_DIRECTORY exp;
    if (!ReadProcessMemory(hProc, (LPCVOID)(modBase + expRVA), &exp, sizeof exp, NULL))
        return 0;

    DWORD *nameRVAs = (DWORD *)malloc(exp.NumberOfNames * sizeof(DWORD));
    WORD *ordinals = (WORD *)malloc(exp.NumberOfNames * sizeof(WORD));
    DWORD *funcRVAs = (DWORD *)malloc(exp.NumberOfFunctions * sizeof(DWORD));
    if (!nameRVAs || !ordinals || !funcRVAs) {
        free(nameRVAs); free(ordinals); free(funcRVAs);
        return 0;
    }

    ReadProcessMemory(hProc, (LPCVOID)(modBase + exp.AddressOfNames), nameRVAs, exp.NumberOfNames * sizeof(DWORD), NULL);
    ReadProcessMemory(hProc, (LPCVOID)(modBase + exp.AddressOfNameOrdinals), ordinals, exp.NumberOfNames * sizeof(WORD), NULL);
    ReadProcessMemory(hProc, (LPCVOID)(modBase + exp.AddressOfFunctions), funcRVAs, exp.NumberOfFunctions * sizeof(DWORD), NULL);

    uintptr_t result = 0;
    char nameBuf[128];
    for (DWORD i = 0; i < exp.NumberOfNames; i++) {
        if (ReadProcessMemory(hProc, (LPCVOID)(modBase + nameRVAs[i]), nameBuf, sizeof nameBuf, NULL)) {
            nameBuf[sizeof(nameBuf) - 1] = 0;
            if (strcmp(nameBuf, funcName) == 0) {
                WORD ord = ordinals[i];
                if (ord < exp.NumberOfFunctions) {
                    result = modBase + funcRVAs[ord];
                }
                break;
            }
        }
    }

    free(nameRVAs); free(ordinals); free(funcRVAs);
    return result;
}

/* Escaneo AOB de lua_resume en lua51.dll */
static uintptr_t scan_pattern(HANDLE hProc, uintptr_t base, size_t size, const uint8_t *pat, size_t patLen) {
    uint8_t buf[4096];
    for (size_t offset = 0; offset < size; offset += sizeof(buf) - patLen) {
        SIZE_T read = 0;
        if (ReadProcessMemory(hProc, (LPCVOID)(base + offset), buf, sizeof buf, &read) && read >= patLen) {
            for (size_t i = 0; i + patLen <= read; i++) {
                if (memcmp(buf + i, pat, patLen) == 0) {
                    return base + offset + i;
                }
            }
        }
    }
    return 0;
}

/* Desconexion y limpieza del hook */
static void detach_game(void) {
    if (G.hProc) {
        if (G.hooked && G.pResume) {
            DWORD oldProt = 0;
            if (VirtualProtectEx(G.hProc, (LPVOID)G.pResume, 14, PAGE_EXECUTE_READWRITE, &oldProt)) {
                WriteProcessMemory(G.hProc, (LPVOID)G.pResume, G.origBytes, 14, NULL);
                VirtualProtectEx(G.hProc, (LPVOID)G.pResume, 14, oldProt, &oldProt);
            }
        }
        if (G.pRemoteMem) {
            VirtualFreeEx(G.hProc, (LPVOID)G.pRemoteMem, 0, MEM_RELEASE);
        }
        CloseHandle(G.hProc);
    }
    memset(&G, 0, sizeof G);
}

/* Inyeccion e instalacion del hook seguro de Lua */
static BOOL attach_and_hook(DWORD pid) {
    G.pid = pid;
    G.hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!G.hProc) {
        snprintf(G.err, sizeof G.err, "Acceso denegado (ejecuta como admin)");
        return FALSE;
    }

    G.baseDll = find_module_base(pid, LUA_DLL);
    if (!G.baseDll) {
        snprintf(G.err, sizeof G.err, "lua51.dll no encontrado en el juego");
        return FALSE;
    }

    /* Patron oficial de CE table: 40 53 48 83 EC 20 48 83 79 30 00 48 8B D9 75 24 80 79 07 01 77 1E 48 63 C2 */
    static const uint8_t PAT[] = {
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x83, 0x79, 0x30, 0x00, 0x48,
        0x8B, 0xD9, 0x75, 0x24, 0x80, 0x79, 0x07, 0x01, 0x77, 0x1E, 0x48, 0x63, 0xC2
    };
    G.pResume = scan_pattern(G.hProc, G.baseDll, 0x200000, PAT, sizeof PAT);
    if (!G.pResume) {
        G.pResume = get_remote_proc_address(G.hProc, G.baseDll, "lua_resume");
    }
    if (!G.pResume) {
        snprintf(G.err, sizeof G.err, "lua_resume no encontrado");
        return FALSE;
    }

    /* Resolucion de funciones Lua necesarias */
    uintptr_t fn_sethook    = get_remote_proc_address(G.hProc, G.baseDll, "lua_sethook");
    uintptr_t fn_gettop     = get_remote_proc_address(G.hProc, G.baseDll, "lua_gettop");
    uintptr_t fn_loadstring = get_remote_proc_address(G.hProc, G.baseDll, "luaL_loadstring");
    uintptr_t fn_pcall      = get_remote_proc_address(G.hProc, G.baseDll, "lua_pcall");
    uintptr_t fn_settop     = get_remote_proc_address(G.hProc, G.baseDll, "lua_settop");
    uintptr_t fn_tolstring  = get_remote_proc_address(G.hProc, G.baseDll, "lua_tolstring");

    if (!fn_sethook || !fn_gettop || !fn_loadstring || !fn_pcall || !fn_settop || !fn_tolstring) {
        snprintf(G.err, sizeof G.err, "Funciones de lua51.dll incompletas");
        return FALSE;
    }

    /* Asignar memoria remota para el shellcode (4KB) */
    size_t scSize = (size_t)(Shellcode_End - Shellcode_Start);
    G.pRemoteMem = (uintptr_t)VirtualAllocEx(G.hProc, NULL, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!G.pRemoteMem) {
        snprintf(G.err, sizeof G.err, "Fallo al asignar memoria remota");
        return FALSE;
    }

    /* Copiar plantilla inicial de shellcode */
    if (!WriteProcessMemory(G.hProc, (LPVOID)G.pRemoteMem, Shellcode_Start, scSize, NULL)) {
        snprintf(G.err, sizeof G.err, "Fallo al escribir shellcode");
        return FALSE;
    }

    /* Escribir punteros de funciones y retorno */
    uint64_t return_addr = (uint64_t)(G.pResume + 14);
    uint64_t pStringPtr  = (uint64_t)(G.pRemoteMem + Offset_luaCmdBuf_Val);

    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x10), &pStringPtr, sizeof pStringPtr, NULL);
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x20), &fn_sethook, sizeof fn_sethook, NULL);
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x28), &fn_gettop, sizeof fn_gettop, NULL);
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x30), &fn_loadstring, sizeof fn_loadstring, NULL);
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x38), &fn_pcall, sizeof fn_pcall, NULL);
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x40), &fn_settop, sizeof fn_settop, NULL);
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x48), &fn_tolstring, sizeof fn_tolstring, NULL);
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x50), &return_addr, sizeof return_addr, NULL);

    /* Guardar bytes originales de lua_resume (14 bytes) */
    ReadProcessMemory(G.hProc, (LPCVOID)G.pResume, G.origBytes, 14, NULL);

    /* Instalar salto de 14 bytes a HookEntry: FF 25 00 00 00 00 [target 64-bit] */
    uint64_t targetEntry = (uint64_t)(G.pRemoteMem + Offset_HookEntry_Val);
    uint8_t jmpCode[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    memcpy(jmpCode + 6, &targetEntry, sizeof targetEntry);

    DWORD oldProt = 0;
    if (!VirtualProtectEx(G.hProc, (LPVOID)G.pResume, 14, PAGE_EXECUTE_READWRITE, &oldProt)) {
        snprintf(G.err, sizeof G.err, "VirtualProtectEx fallo");
        return FALSE;
    }

    if (!WriteProcessMemory(G.hProc, (LPVOID)G.pResume, jmpCode, 14, NULL)) {
        VirtualProtectEx(G.hProc, (LPVOID)G.pResume, 14, oldProt, &oldProt);
        snprintf(G.err, sizeof G.err, "Fallo al escribir jmp en lua_resume");
        return FALSE;
    }

    VirtualProtectEx(G.hProc, (LPVOID)G.pResume, 14, oldProt, &oldProt);
    G.hooked = TRUE;
    return TRUE;
}

/* Sincronizacion de proceso con el temporizador */
static void game_sync(void) {
    /* Fast path: si ya estamos conectados, verificar con GetExitCodeProcess
       (muy rapido, no bloquea) en lugar de hacer un snapshot de procesos */
    if (G.hProc && G.hooked) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(G.hProc, &exitCode) && exitCode == STILL_ACTIVE)
            return; /* Sigue conectado, no hace falta escanear */
    }

    /* Slow path: buscar el proceso (solo cuando no conectado o se cerro el juego) */
    DWORD pid = find_pid();
    if (pid == G.pid && G.hProc && G.hooked) return;

    detach_game();
    if (pid == 0) return;

    if (!attach_and_hook(pid)) {
        detach_game();
    }
}

/* Envio seguro de codigo Lua a traves del hook */
static BOOL send_lua(const char *code, char *errOut, size_t errSize) {
    if (!G.hProc || !G.hooked || !G.pRemoteMem) {
        if (errOut) snprintf(errOut, errSize, "Sin conexion con Balatro");
        return FALSE;
    }

    size_t len = strlen(code);
    if (len >= 4000) {
        if (errOut) snprintf(errOut, errSize, "Comando demasiado largo");
        return FALSE;
    }

    /* Escribir codigo en luaCmdBuf */
    uintptr_t pCmd = G.pRemoteMem + Offset_luaCmdBuf_Val;
    if (!WriteProcessMemory(G.hProc, (LPVOID)pCmd, code, len + 1, NULL)) {
        if (errOut) snprintf(errOut, errSize, "Error al escribir comando Lua");
        return FALSE;
    }

    /* Limpiar banderas */
    uint8_t zero = 0;
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x09), &zero, 1, NULL); /* done = 0 */
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x0A), &zero, 1, NULL); /* errFlag = 0 */

    /* Activar pending = 1 */
    uint8_t one = 1;
    WriteProcessMemory(G.hProc, (LPVOID)(G.pRemoteMem + 0x08), &one, 1, NULL);

    /* Esperar finalizacion hasta 800ms */
    int waited = 0;
    BOOL ok = FALSE;
    while (waited < 800) {
        uint8_t doneVal = 0;
        ReadProcessMemory(G.hProc, (LPCVOID)(G.pRemoteMem + 0x09), &doneVal, 1, NULL);
        if (doneVal == 1) {
            ok = TRUE;
            break;
        }
        Sleep(10);
        waited += 10;
    }

    if (!ok) {
        if (errOut) snprintf(errOut, errSize, "Tiempo de espera agotado (hook no respondio)");
        return FALSE;
    }

    /* Verificar si hubo error en Lua */
    uint8_t errVal = 0;
    ReadProcessMemory(G.hProc, (LPCVOID)(G.pRemoteMem + 0x0A), &errVal, 1, NULL);
    if (errVal != 0) {
        char remoteErr[256];
        remoteErr[0] = 0;
        ReadProcessMemory(G.hProc, (LPCVOID)(G.pRemoteMem + 0x68), remoteErr, sizeof remoteErr - 1, NULL);
        remoteErr[sizeof(remoteErr) - 1] = 0;
        if (errOut) snprintf(errOut, errSize, "Lua: %s", remoteErr[0] ? remoteErr : "Error desconocido");
        return FALSE;
    }

    return TRUE;
}

/* Actualizacion de barra de estado inferior */
static void refresh_status(void) {
    char buf[192];
    game_sync();
    if (G.hProc && G.hooked) {
        snprintf(buf, sizeof buf, "[+] %s  PID %lu  (Conectado)", PROC_NAME, (unsigned long)G.pid);
        statusFg = C_FG;
    } else {
        snprintf(buf, sizeof buf, "[-] %s: %s", PROC_NAME, G.err[0] ? G.err : "esperando el juego");
        statusFg = C_DIM;
    }
    set_text(hStatus, buf);
}

/* Obtener el indice global del cheat para la fila visible dada */
static int get_cheat_index(int slot) {
    int curSlot = 0;
    for (int i = 0; i < N_CHEATS; i++) {
        if (CHEATS[i].page == curPage) {
            if (curSlot == slot) return i;
            curSlot++;
        }
    }
    return -1;
}

/* Aplicar la activacion o desactivacion del cheat */
static void apply_slot(int slot, BOOL enable) {
    int idx = get_cheat_index(slot);
    if (idx < 0) return;

    const Cheat *c = &CHEATS[idx];
    char val[64], luaFinal[2048], msg[256];

    if (c->type == CHEAT_INPUT_NUM) {
        GetWindowTextA(hEdit[slot], val, sizeof val);
        char *p = val;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) {
            snprintf(msg, sizeof msg, "> %s: ingresa un valor", c->name);
            set_text(hMsg, msg);
            return;
        }
        for (char *s = p; *s; s++) if (*s == ',') *s = '.';
        snprintf(luaFinal, sizeof luaFinal, c->lua_code, p, p);
    } else {
        const char *src = enable ? c->lua_code : (c->lua_disable ? c->lua_disable : "--");
        strncpy(luaFinal, src, sizeof luaFinal - 1);
        luaFinal[sizeof(luaFinal) - 1] = 0;
    }

    char errBuf[160];
    if (send_lua(luaFinal, errBuf, sizeof errBuf)) {
        if (c->type == CHEAT_INPUT_NUM) {
            cheatState[idx] = 1;
            strncpy(cheatVal[idx], val, sizeof(cheatVal[idx]) - 1);
            snprintf(msg, sizeof msg, "> %s = %s (OK)", c->name, val);
            set_text(hRead[slot], val);
        } else {
            cheatState[idx] = enable ? 1 : 0;
            if (enable) {
                snprintf(msg, sizeof msg, "> %s: Activado", c->name);
                set_text(hRead[slot], "ACTIVO");
            } else {
                snprintf(msg, sizeof msg, "> %s: Desactivado", c->name);
                set_text(hRead[slot], "INACTIVO");
            }
        }
    } else {
        snprintf(msg, sizeof msg, "> %s: %s", c->name, errBuf);
    }
    set_text(hMsg, msg);
}

/* Actualizar la vista de controles segun la pagina seleccionada */
static void update_page_view(void) {
    set_text(hPageLbl, PAGE_TITLES[curPage]);

    int slot = 0;
    for (int i = 0; i < N_CHEATS; i++) {
        if (CHEATS[i].page == curPage && slot < ROWS_PER_PAGE) {
            const Cheat *c = &CHEATS[i];
            int y = 76 + slot * 28;

            ShowWindow(hLbl[slot], SW_SHOW);
            ShowWindow(hRead[slot], SW_SHOW);
            set_text(hLbl[slot], c->name);

            if (c->type == CHEAT_INPUT_NUM) {
                /* Mostrar Edit box y boton SET */
                SetWindowPos(hEdit[slot], NULL, S(224), S(y), S(94), S(22), SWP_NOZORDER | SWP_SHOWWINDOW);
                SetWindowPos(hBtn[slot], NULL, S(324), S(y), S(52), S(22), SWP_NOZORDER | SWP_SHOWWINDOW);
                ShowWindow(hBtnDes[slot], SW_HIDE);

                set_text(hBtn[slot], "SET");
                set_text(hEdit[slot], c->default_val ? c->default_val : "");
                EnableWindow(hEdit[slot], TRUE);

                if (cheatState[i] == 1 && cheatVal[i][0]) {
                    set_text(hRead[slot], cheatVal[i]);
                } else {
                    set_text(hRead[slot], "--");
                }
            } else {
                /* Ocultar Edit box, mostrar botones ACT y DES */
                ShowWindow(hEdit[slot], SW_HIDE);
                SetWindowPos(hBtn[slot], NULL, S(224), S(y), S(72), S(22), SWP_NOZORDER | SWP_SHOWWINDOW);
                SetWindowPos(hBtnDes[slot], NULL, S(304), S(y), S(72), S(22), SWP_NOZORDER | SWP_SHOWWINDOW);

                set_text(hBtn[slot], "ACT");
                set_text(hBtnDes[slot], "DES");

                if (cheatState[i] == 1) {
                    set_text(hRead[slot], "ACTIVO");
                } else {
                    set_text(hRead[slot], "INACTIVO");
                }
            }
            slot++;
        }
    }

    /* Ocultar filas sobrantes si la pagina tiene menos de 7 elementos */
    for (; slot < ROWS_PER_PAGE; slot++) {
        ShowWindow(hLbl[slot], SW_HIDE);
        ShowWindow(hRead[slot], SW_HIDE);
        ShowWindow(hEdit[slot], SW_HIDE);
        ShowWindow(hBtn[slot], SW_HIDE);
        ShowWindow(hBtnDes[slot], SW_HIDE);
    }
}

/* Interceptar Enter en cuadros de texto */
static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CHAR && w == '\r') {
        int slot = GetDlgCtrlID(h) - ID_EDIT_BASE;
        SendMessageA(GetParent(h), WM_COMMAND, MAKEWPARAM(ID_BTN_BASE + slot, BN_CLICKED), (LPARAM)hBtn[slot]);
        return 0;
    }
    return CallWindowProcA(oldEditProc, h, m, w, l);
}

/* Creacion comoda de controles con DPI */
static HWND mk(const char *cls, const char *text, DWORD style, int x, int y, int w, int h,
               HWND parent, int id, HFONT f) {
    HWND c = CreateWindowA(cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                           parent, (HMENU)(UINT_PTR)id, NULL, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)f, TRUE);
    return c;
}

/* Procedimiento de ventana principal */
static LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        /* Cabecera */
        hTitle  = mk("STATIC", "> " TITLE, SS_LEFT | SS_NOPREFIX, 10, 6, 366, 20, hw, ID_TITLE, fontBold);
        hThanks = mk("STATIC", THANKS_TEXT, SS_LEFT | SS_NOPREFIX, 10, 27, 366, 16, hw, ID_THANKS, fontSmall);

        /* Barra de navegacion de paginas */
        hPrevBtn = mk("BUTTON", "<", BS_OWNERDRAW, 10, 48, 40, 22, hw, ID_PREV, font);
        hPageLbl = mk("STATIC", "", SS_CENTER | SS_NOPREFIX, 55, 51, 276, 18, hw, ID_PAGE_LBL, fontBold);
        hNextBtn = mk("BUTTON", ">", BS_OWNERDRAW, 336, 48, 40, 22, hw, ID_NEXT, font);

        /* 7 Filas de controles reutilizables */
        for (int i = 0; i < ROWS_PER_PAGE; i++) {
            int y = 76 + i * 28;
            hLbl[i]    = mk("STATIC", "", SS_LEFT | SS_NOPREFIX, 10, y + 3, 140, 20, hw, ID_LABEL_BASE + i, font);
            hRead[i]   = mk("STATIC", "--", SS_RIGHT | SS_NOPREFIX, 152, y + 3, 68, 20, hw, ID_READ_BASE + i, font);
            hEdit[i]   = mk("EDIT", "", WS_BORDER | ES_AUTOHSCROLL, 224, y, 94, 22, hw, ID_EDIT_BASE + i, font);
            hBtn[i]    = mk("BUTTON", "SET", BS_OWNERDRAW, 324, y, 52, 22, hw, ID_BTN_BASE + i, font);
            hBtnDes[i] = mk("BUTTON", "DES", BS_OWNERDRAW, 304, y, 72, 22, hw, ID_BTNDES_BASE + i, font);
            oldEditProc = (WNDPROC)SetWindowLongPtrA(hEdit[i], GWLP_WNDPROC, (LONG_PTR)EditProc);
        }

        /* Pie de estado e informacion */
        int yFooter = 76 + ROWS_PER_PAGE * 28 + 6;
        hStatus = mk("STATIC", "", SS_LEFT | SS_NOPREFIX, 10, yFooter, 366, 18, hw, ID_STATUS, font);
        hMsg    = mk("STATIC", "> Listo. Esperando orden...", SS_LEFT | SS_NOPREFIX, 10, yFooter + 20, 366, 18, hw, ID_MSG, font);

        update_page_view();
        refresh_status();
        SetTimer(hw, TIMER_ID, 700, NULL);
        return 0;
    }
    case WM_TIMER:
        refresh_status();
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (HIWORD(wp) == BN_CLICKED) {
            if (id == ID_PREV) {
                curPage = (curPage + TOTAL_PAGES - 1) % TOTAL_PAGES;
                update_page_view();
            } else if (id == ID_NEXT) {
                curPage = (curPage + 1) % TOTAL_PAGES;
                update_page_view();
            } else if (id >= ID_BTN_BASE && id < ID_BTN_BASE + ROWS_PER_PAGE) {
                apply_slot(id - ID_BTN_BASE, TRUE);
            } else if (id >= ID_BTNDES_BASE && id < ID_BTNDES_BASE + ROWS_PER_PAGE) {
                apply_slot(id - ID_BTNDES_BASE, FALSE);
            }
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        int id = GetDlgCtrlID((HWND)lp);
        COLORREF fg = C_FG;
        if (id >= ID_READ_BASE && id < ID_READ_BASE + ROWS_PER_PAGE) {
            int slot = id - ID_READ_BASE;
            int idx = get_cheat_index(slot);
            if (idx >= 0 && cheatState[idx] == 1) {
                fg = C_FG;
            } else {
                fg = C_DIM;
            }
        }
        else if (id >= ID_EDIT_BASE && id < ID_EDIT_BASE + ROWS_PER_PAGE) fg = C_DIM;
        else if (id == ID_STATUS) fg = statusFg;
        else if (id == ID_THANKS) fg = C_DIM;
        else if (id == ID_PAGE_LBL) fg = C_FG;
        SetTextColor(dc, fg);
        SetBkColor(dc, C_BG);
        return (LRESULT)brBg;
    }

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, C_FG);
        SetBkColor((HDC)wp, C_FIELD);
        return (LRESULT)brField;

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)lp;
        BOOL sel = (d->itemState & ODS_SELECTED) != 0;
        BOOL dis = (d->itemState & ODS_DISABLED) != 0;
        HBRUSH fill = CreateSolidBrush(sel && !dis ? C_FG : C_BG);
        HBRUSH edge = CreateSolidBrush(C_DIM);
        FillRect(d->hDC, &d->rcItem, fill);
        FrameRect(d->hDC, &d->rcItem, edge);
        DeleteObject(fill);
        DeleteObject(edge);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, dis ? C_DIM : (sel ? C_BG : C_FG));
        HGDIOBJ old = SelectObject(d->hDC, font);

        char btnTxt[16];
        GetWindowTextA(d->hwndItem, btnTxt, sizeof btnTxt);
        DrawTextA(d->hDC, btnTxt, -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(d->hDC, old);
        return TRUE;
    }

    case WM_DESTROY:
        KillTimer(hw, TIMER_ID);
        detach_game();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show) {
    (void)hp; (void)cmd;
    SetProcessDPIAware();
    HDC sdc = GetDC(NULL);
    dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(NULL, sdc);

    brBg = CreateSolidBrush(C_BG);
    brField = CreateSolidBrush(C_FIELD);
    font = CreateFontA(-S(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    fontBold = CreateFontA(-S(15), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    fontSmall = CreateFontA(-S(11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = brBg;
    wc.lpszClassName = "BalMenuWnd";
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    int clientH = 76 + ROWS_PER_PAGE * 28 + 6 + 18 + 18 + 12;
    RECT r = { 0, 0, S(386), S(clientH) };
    AdjustWindowRect(&r, style, FALSE);

    HWND w = CreateWindowA("BalMenuWnd", TITLE, style, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    if (!w) return 1;

    ShowWindow(w, show);
    UpdateWindow(w);

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return (int)m.wParam;
}
