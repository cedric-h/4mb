// set to 0 to create resizable window
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// keep this enabled when debugging
#define USE_DEBUG_MODE 1

float (*sinf)(float);
float (*cosf)(float);
float (*sqrtf)(float);
float (*atan2f)(float, float);

#include <stdint.h>
#include "mem.h"
#include "math.h"

#define COBJMACROS
#define WIN32_EXTRA_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#pragma comment (lib, "user32.lib")
#pragma comment (lib, "kernel32.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxguid.lib")

typedef enum {
    Color_DarkGreen, Color_Green, Color_LightGreen, Color_Beige,
    Color_GreenTan, Color_LightBrown, Color_DarkBrown, Color_Purple,
    Color_Black, Color_DarkPurpleBlue, Color_PurpleBlue, Color_LightPurpleBlue,
    Color_DarkBlue, Color_Blue, Color_LightBlue, Color_WhiteBlue,
} Color;
static Byte4 colors[]  = {
    [Color_DarkGreen      ] = { 33,  59,  37, 255},
    [Color_Green          ] = { 58,  96,  74, 255},
    [Color_LightGreen     ] = { 79, 119,  84, 255},
    [Color_Beige          ] = {161, 159, 124, 255},
    [Color_GreenTan       ] = {119, 116,  79, 255},
    [Color_LightBrown     ] = {119,  92,  79, 255},
    [Color_DarkBrown      ] = { 96,  59,  58, 255},
    [Color_Purple         ] = { 59,  33,  55, 255},
    [Color_Black          ] = { 23,  14,  25, 255},
    [Color_DarkPurpleBlue ] = { 47,  33,  59, 255},
    [Color_PurpleBlue     ] = { 67,  58,  96, 255},
    [Color_LightPurpleBlue] = { 79,  82, 119, 255},
    [Color_DarkBlue       ] = {101, 115, 140, 255},
    [Color_Blue           ] = {124, 148, 161, 255},
    [Color_LightBlue      ] = {160, 185, 186, 255},
    [Color_WhiteBlue      ] = {192, 209, 204, 255},
};
/* EntProps enable codepaths for game entities.

   These aren't bitfields because the layout of bitfields isn't specified,
   making serialization difficult.
   These aren't boolean fields because that makes it more difficult to deal
   with them dynamically; this way you can have a function which gives you all
   Ents with a certain property within a distance of a certain point in space,
   or that are children of a certain other Ent. */
typedef enum {
    /* Prevents the Ent's memory from being reused, enables all other codepaths. */
    EntProp_Active,

    /* Knowing how many EntProps there are facilitates allocating just enough memory */
    EntProp_COUNT,
} EntProp;

/* A game entity. Usually, it is rendered somewhere and
   has some sort of dynamic behavior */
typedef struct Ent Ent;
struct Ent {
    /* enough memory for all the props, rounding up to the nearest uint64_t */
    uint64_t props[(EntProp_COUNT + 63)/64];
    Vec2 pos, scale;
    float rot;
    Byte4 rgba, corner_radii;
};

#define MAX_ENTS (10500)
/* Primary storage of global application state, though the rendering context
   and other isolated components may also mantain their own */
static struct {
    Ent ents[MAX_ENTS];
    Vec2 mouse_pos, screen_size;
} state;

/* bithacks to read/write props */
static int has_ent_prop(Ent *ent, EntProp prop) {
    return !!(ent->props[prop/64] & ((uint64_t)1 << (prop%64)));
}
static int take_ent_prop(Ent *ent, EntProp prop) {
    int before = has_ent_prop(ent, prop);
    ent->props[prop/64] &= ~((uint64_t)1 << (prop%64));
    return before;
}
static int give_ent_prop(Ent *ent, EntProp prop) {
    int before = has_ent_prop(ent, prop);
    ent->props[prop/64] |= (uint64_t)1 << (prop%64);
    return before;
}

/* Calling this function will find some unoccupied memory for an Ent if available, 
   returning NULL otherwise. It will automatically set `EntProp_Active` on the Ent,
   keeping its memory from getting recycled until `EntProp_Active` is removed.
   Otherwise, consider the Ent zero-initialized.  */
Ent *add_ent() {
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *ent = &state.ents[i];
        if (!has_ent_prop(ent, EntProp_Active)) {
            *ent = (Ent) {0}; // clear existing memory
            give_ent_prop(ent, EntProp_Active);
            return ent;
        }
    }
    return NULL;
}

/* Use this function to iterate over all of the Ents in the game.
   ex:
        for (Ent *e = 0; e = ent_all_iter(e); )
            draw_ent(e);
*/
Ent *ent_all_iter(Ent *ent) {
    if (ent == NULL) ent = state.ents - 1;

    for (;;) {
        ent++;
        if (ent < state.ents || ent >= state.ents + MAX_ENTS)
            return NULL;
        if (has_ent_prop(ent, EntProp_Active))
            break;
    }

    if (has_ent_prop(ent, EntProp_Active)) return ent;
    return NULL;
}

static void init_world() {
    #define MAP_SIZE (100)
    #define HALF_MAP ((float) MAP_SIZE / 2.0f)

    float heights[MAP_SIZE];
    for (int x = 0; x < MAP_SIZE; x++) {
        float hor = (float)x - HALF_MAP;
        heights[x] = (0.48f + 0.04f * sinf(hor * 0.2f)) * MAP_SIZE - HALF_MAP;
    }

    for (int x = 0; x < MAP_SIZE; x++)
    for (int y = 0; y < MAP_SIZE; y++) {
        typedef enum { Grass, Dirt } Block; 

        Vec2 pos = {x - HALF_MAP, y - HALF_MAP};

        Block block = Dirt;
        if (pos.y > heights[x])
            if ((pos.y - 1.0f) < heights[x])
                block = Grass, pos.y = heights[x];
            else continue;

        Ent *e = add_ent();

        if (block == Grass) {
            Vec2 tangent = sub2((Vec2){pos.x + 1.0f, heights[x + 1]},
                                (Vec2){pos.x - 1.0f, heights[x - 1]});
            e->rot = rot_vec2(tangent) + PI_f * 3.0f / 4.0f;
        }

        e->pos = mul2_f(pos, 0.3f);
        e->scale = (Vec2){0.375f, 0.375f};
        switch (block) {
        case Grass: e->rgba = colors[Color_Green]; break;
        case  Dirt: e->rgba = colors[Color_DarkBrown]; break;
        }
        e->corner_radii = byte4_u(191);
    }
}

#include "render.h"

static LRESULT CALLBACK window_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
       if (FAILED(render_create(wnd)))
           return -1;
       return 0;

    case WM_DESTROY:
        render_destroy(wnd);
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE)
            PostQuitMessage(0);
        return 0;

    case WM_MOUSEMOVE:
        state.mouse_pos.x = GET_X_LPARAM(lparam);
        state.mouse_pos.y = GET_Y_LPARAM(lparam);
        return 0;

    case WM_SIZE:
        float x = LOWORD(lparam),
              y = HIWORD(lparam);
        state.screen_size = (Vec2){x,y};
        if (FAILED(render_resize(wnd, x, y)))
            DestroyWindow(wnd);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wparam, lparam);
}

int WINAPI WinMainCRTStartup(void) {
    {
        HMODULE crt = GetModuleHandle("msvcrt.dll");
        *(void**) &sinf = GetProcAddress(crt, "sinf");
        *(void**) &cosf = GetProcAddress(crt, "cosf");
        *(void**) &sqrtf = GetProcAddress(crt, "sqrtf");
        *(void**) &atan2f = GetProcAddress(crt, "atan2f");
    }

    HINSTANCE instance = GetModuleHandle(NULL);
    INT cmd_show = TRUE;

    WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .lpfnWndProc = window_proc,
        .hInstance = instance,
        .hIcon = LoadIconA(NULL, IDI_APPLICATION),
        .hCursor = LoadCursorA(NULL, IDC_CROSS),
        .lpszClassName = L"d3d11_window_class",
    };

    if (!RegisterClassExW(&wc)) {
        log_win32_last_err("RegisterClassEx failed");
        ExitProcess(1);
    }

    int width = CW_USEDEFAULT;
    int height = CW_USEDEFAULT;

    DWORD exstyle = WS_EX_APPWINDOW;
    DWORD style = WS_OVERLAPPEDWINDOW;

    if (WINDOW_WIDTH && WINDOW_HEIGHT) {
        style &= ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;

        RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        if (!AdjustWindowRectEx(&rect, style, FALSE, exstyle)) {
            log_win32_last_err("AdjustWindowRectEx failed");
            style = WS_OVERLAPPEDWINDOW;
        } else {
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
        }
    }

    HWND wnd = CreateWindowExW(exstyle, wc.lpszClassName, L"D3D11 Window",
        style | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, wc.hInstance, NULL);
    if (!wnd) {
        log_win32_last_err("CreateWindow failed");
        ExitProcess(1);
    }

    init_world();
    for (;;) {
        MSG msg;
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        render_frame();

        if (FAILED(render_present(wnd)))
            break;
    }

    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    ExitProcess(0);
}
