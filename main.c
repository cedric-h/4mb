// keep this enabled when debugging
#define USE_DEBUG_MODE 1
#define CONTROLLER_SUPPORT 1

#include <stdint.h>
extern __declspec(dllimport) float sinf(float x);
extern __declspec(dllimport) float cosf(float x);
extern __declspec(dllimport) float fmodf(float x, float y);
extern __declspec(dllimport) float atan2f(float x, float y);
extern __declspec(dllimport) float sqrtf(float x);
#include "mem.h"
#include "math.h"

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <stddef.h>

#if CONTROLLER_SUPPORT
    #define DIRECTINPUT_VERSION 0x0800
    #include <Dinput.h>
    #include "controller.h"
    #pragma comment (lib, "dinput8.lib")
#endif

#pragma comment (lib, "user32.lib")
#pragma comment (lib, "ced_crt.lib")
#pragma comment (lib, "kernel32.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxguid.lib")

#include "err.h"
#include "box.h"
#include "tree.h"

/* based on scancodes */
typedef enum {
    Key_W = 17,
    Key_S = 31,
    Key_A = 30,
    Key_D = 32,
    Key_Space = 57,
} Key;
typedef enum { CursorGrab_Free, CursorGrab_Grabbed } CursorGrab;
static struct {
    CursorGrab cursor_grab;
    Vec2 mouse_pos, screen_size;
    uint64_t down_keys[512 / 64];

    struct {
        Vec3 pos, vel;
        int8_t jump_cooldown, ground_cooldown;
    } player;

    struct {
        float yaw, pitch;
        Vec2 turn_vel;
    } cam;

    /* https://docs.microsoft.com/en-us/windows/win32/inputdev/about-raw-input */
    struct {
        void *data;
        uint64_t size;
    } raw_input;
} state;

static void key_set_down(Key key) {
    state.down_keys[key/64] |= (uint64_t)1 << (key%64);
}
static void key_set_up(Key key) {
    state.down_keys[key/64] &= ~((uint64_t)1 << (key%64));
}
static int key_down(Key key) {
    return !!(state.down_keys[key/64] & ((uint64_t)1 << (key%64)));
}

static void cam_turn(Vec2 delta) {
    delta.y *= -1.0f;
    delta = mul2_f(delta, 0.0003f);
    state.cam.turn_vel = add2(state.cam.turn_vel, delta);
}
static void cam_update() {
    state.cam.turn_vel = mul2_f(state.cam.turn_vel, 0.9f);
    float yaw_d   = state.cam.turn_vel.x,
          pitch_d = state.cam.turn_vel.y; 
    state.cam.pitch = clamp(state.cam.pitch + pitch_d, -PI_f * 0.49f, PI_f * 0.49f);
    state.cam.yaw = fmodf(state.cam.yaw + yaw_d, PI_f * 2.0f);
}

/* the flat version of cam_facing like you'd want to use for movement */
static Vec2 cam_going() {
    return (Vec2) { sinf(state.cam.yaw), cosf(state.cam.yaw) };
}

static Vec3 cam_facing() {
    return (Vec3) {
        sinf(state.cam.yaw) * cosf(state.cam.pitch),
        sinf(state.cam.pitch),
        cosf(state.cam.yaw) * cosf(state.cam.pitch),
    };
}

static Vec3 player_eye() {
    return add3(state.player.pos, vec3(0.0f, 1.65f, 0.0f));
}

static void player_move(Vec2 dir) {
    float mag = mag2(dir);
    Vec2 norm = vec2_rot(rot_vec2(dir) + rot_vec2(cam_going()));
    Vec2 move = mul2_f(norm, mag * 0.03f);
    state.player.vel = add3(state.player.vel, vec3(move.x, 0.0f, move.y));
}

static void player_interact() {
    Face face = Face_COUNT;
    BoxId build_onto = box_under_ray(player_eye(), cam_facing(), &face);
    if (face == Face_COUNT || build_onto == BoxId_NULL) return;
    add_box(build_onto, face, BoxKind_Dirt);
}

static void player_hit() {
    BoxId target = box_under_ray(player_eye(), cam_facing(), NULL);
    if (target == BoxId_NULL) return;
    rem_box(target);
}

float sdf_box3(Vec3 p) {
    Vec3 d = sub3(abs3(p), vec3_f(0.5f));
    return mag3(max3_f(d, 0.0f)) + min(max(max(d.x,d.y), d.z), 0.0f);
}

Vec3 sdf_box_normal3(Vec3 p) {
    float h = 0.0002f; /* TODO: make sure this is appropriate */
    Vec2 k = vec2(1,-1);
    Vec3   q = mul3_f(vec3( 1, -1, -1), sdf_box3(add3(p, mul3_f(vec3( 1, -1, -1), h))));
    q = add3(q, mul3_f(vec3(-1, -1,  1), sdf_box3(add3(p, mul3_f(vec3(-1, -1,  1), h)))));
    q = add3(q, mul3_f(vec3(-1,  1, -1), sdf_box3(add3(p, mul3_f(vec3(-1,  1, -1), h)))));
    q = add3(q, mul3_f(vec3( 1,  1,  1), sdf_box3(add3(p, mul3_f(vec3( 1,  1,  1), h)))));
    return norm3(q);
}

/* tests the player's position against all of the boxes, 
   pushing him out if he intersects with any of them. */
static void player_physics() {
    typedef struct { Vec3 pos; BoxId box; float dist; } Nearest;
    Nearest nearest = { .dist = INFINITY };

    #define plyr state.player

    /* center of the player's collider */
    #define PLAYER_COLLIDER_SIZE (0.4f)
    Vec3 plrc = plyr.pos;
    plrc.y += PLAYER_COLLIDER_SIZE;

    for (BoxId id = 1; id < MAX_BOXES; id++)
        if (OCCUPIED(boxes[id])) {
            Vec3 pos = box_pos_to_vec3(boxes[id].pos);
            pos = sub3(plrc, add3_f(pos, 0.5f));
            float this_dist = sdf_box3(pos);
            /* but if the distance is less than 0.25f, we've probably placed a block
               over our head, which probably shouldn't be handled by this code. */
            if (this_dist < nearest.dist && this_dist > 0.25f)
                nearest = (Nearest) { pos, id, this_dist };
        }
    
    /* if the distance is less than 0.5f, they're inside of our collider. */
    int touched_tile = 0;
    if (nearest.dist < PLAYER_COLLIDER_SIZE) {
        float depth = fabsf(nearest.dist - PLAYER_COLLIDER_SIZE);
        Vec3 out = mul3_f(sdf_box_normal3(nearest.pos), depth * 0.65f);
        plyr.vel = add3(plyr.vel, out);

        if (boxes[nearest.box].pos.y < plyr.pos.y) {
            plyr.ground_cooldown = min(0, sat_i8(plyr.ground_cooldown - 1));
            touched_tile = 1;
        } else {
            /* ya done bumped ya head, go down faster! */
            plyr.jump_cooldown = sat_i8(plyr.jump_cooldown + 5);
        }
    }
    if (!touched_tile) 
        plyr.ground_cooldown = max(0, sat_i8(plyr.ground_cooldown + 1));

    float boost = (float) sat_i8(plyr.ground_cooldown - 10) / 100.0f;
    plyr.pos.y -= 0.03f + 0.1f * max(0.0f, min(1.0f, boost));

    plyr.jump_cooldown = sat_i8(plyr.jump_cooldown + 1);
    if (plyr.jump_cooldown < 50)
        plyr.vel.y += 0.0675f * (1.0f - (plyr.jump_cooldown / 50.0f));

    plyr.vel = mul3_f(plyr.vel, 0.65f);
    plyr.pos = add3(plyr.pos, plyr.vel);
}

static void player_try_jump() {
    if (state.player.ground_cooldown < -15)
        state.player.jump_cooldown = 0;
}

static void init_world() {
    /* moved somewhere and pulled these values out of debugger */
    state.player.pos = vec3(0.1f, -0.01f, 0.09f);
    state.cam.yaw = 0.818;
    state.cam.pitch = -0.88;

    #define ORIGIN 1
    boxes[ORIGIN] = (Box) {
        .kind = BoxKind_Dirt,
        .pos.y = -1,
    };
    add_box(ORIGIN, Face_Left, BoxKind_Dirt);
    add_box(ORIGIN, Face_Right, BoxKind_Dirt);

    add_box(3, Face_Right, BoxKind_Dirt);
    add_box(4, Face_Right, BoxKind_Dirt);
    add_box(5, Face_Right, BoxKind_Dirt);
    add_box(6, Face_Right, BoxKind_Dirt);
    add_box(7, Face_Right, BoxKind_Dirt);
    add_box(8, Face_Right, BoxKind_Dirt);

    add_box(9, Face_Back, BoxKind_Dirt);
    add_box(10, Face_Back, BoxKind_Dirt);
    add_box(11, Face_Back, BoxKind_Dirt);
    add_box(12, Face_Back, BoxKind_Dirt);
    add_box(13, Face_Back, BoxKind_Dirt);

    add_box(ORIGIN, Face_Front, BoxKind_Dirt);
    add_box(ORIGIN, Face_Back, BoxKind_Dirt);

    add_tree(-4, -1, -4);
}


#include "render.h"

static void update_clip_rect(HWND wnd) {
    RECT clip_rect;
    GetClientRect(wnd, &clip_rect);
    ClientToScreen(wnd, (POINT*) &clip_rect.left);
    ClientToScreen(wnd, (POINT*) &clip_rect.right);
    ClipCursor(&clip_rect);
}

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
            if (state.cursor_grab) {
                const RAWINPUTDEVICE rid = { 0x01, 0x02, RIDEV_REMOVE, NULL };
                if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
                    log_win32_last_err("Failed to remove mouse raw input");
                    break;
                }
                update_clip_rect(NULL);
                SetCursor(LoadCursorA(NULL, IDC_CROSS));
                ReleaseCapture();
                state.cursor_grab = CursorGrab_Free;
            } else {
                PostQuitMessage(0);
                #if USE_DEBUG_MODE
                #endif
            }

        key_set_down(HIWORD(lparam) & (KF_EXTENDED | 0xff));
        return 0;

    case WM_KEYUP:
        key_set_up(HIWORD(lparam) & (KF_EXTENDED | 0xff));
        return 0;

    case WM_LBUTTONDOWN:
        if (!state.cursor_grab) {
            state.cursor_grab = CursorGrab_Grabbed;

            RAWINPUTDEVICE rid = { 0x01, 0x02, 0, wnd };
            if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
                log_win32_last_err("Mouse grab failed");
                break;
            }
            SetCapture(wnd);
            SetCursor(NULL);
            update_clip_rect(wnd);
        } else {
            player_hit();
        }
        return 0;

    case WM_RBUTTONDOWN:
        player_interact();
        return 0;

    case WM_MOUSEMOVE:
        state.mouse_pos.x = GET_X_LPARAM(lparam);
        state.mouse_pos.y = GET_Y_LPARAM(lparam);
        return 0;

    case WM_INPUT:;
        uint32_t size = 0;
        HRAWINPUT ri_handle = (HRAWINPUT) lparam;

        GetRawInputData(ri_handle, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
        if (size > (uint32_t) state.raw_input.size) {
            state.raw_input.size = size;
            HeapFree(GetProcessHeap(), 0, state.raw_input.data);
            state.raw_input.data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
            if (state.raw_input.data == NULL) {
                log_win32_last_err("Failed to resize raw input data");
                break;
            }
        }

        size = state.raw_input.size;
        uint32_t res = GetRawInputData(
            ri_handle, RID_INPUT,
            state.raw_input.data,
            &size,
            sizeof(RAWINPUTHEADER)
        );
        if (res == -1) {
            log_win32_last_err("Failed to retrieve raw input data");
            break;
        }

        RAWINPUT* data = state.raw_input.data;
        Vec2 delta = {
            data->data.mouse.lLastX,
            data->data.mouse.lLastY,
        };

        if (data->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
            delta = sub2(delta, state.mouse_pos);

        cam_turn(delta);
        state.mouse_pos = add2(state.mouse_pos, delta);
        return 0;

    case WM_SIZE:
        float x = LOWORD(lparam),
              y = HIWORD(lparam);
        state.screen_size = (Vec2){x,y};

        if (state.cursor_grab)
            update_clip_rect(wnd);

        if (FAILED(render_resize(wnd, x, y)))
            DestroyWindow(wnd);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wparam, lparam);
}

int WINAPI WinMainCRTStartup(void) {
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

#if CONTROLLER_SUPPORT
    Controllers controllers = controller_init(wc.hInstance);
#endif

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

#if CONTROLLER_SUPPORT
        for (uint32_t i = 0; i < controllers.device_count; i++) {
            DIJOYSTATE state;
            IDirectInputDevice8* dvi = *controllers.devices + i;
            if (IDirectInputDevice_GetDeviceState(dvi, sizeof(state), &state) == DI_OK) {
                #define AXIS_MAX (float) ((2 << 15) - 1)

                {
                    Vec2 lthumb = {
                        .x = (float)(state.lX - AXIS_MAX/2) / (float) AXIS_MAX,
                        .y = (float)(state.lY - AXIS_MAX/2) / (float) AXIS_MAX,
                    };

                    /* camera inputs */
                    if (magmag2(lthumb) > 0.03f)
                        cam_turn(mul2_f(lthumb, 30.0f));
                }

                {
                    Vec2 rthumb = {
                        .x = (float)(state.lRx - AXIS_MAX/2) / (float) AXIS_MAX,
                        .y = (float)(state.lRy - AXIS_MAX/2) / (float) AXIS_MAX,
                    };
                    rthumb = vec2(-rthumb.y, -rthumb.x);

                    /* movement inputs */
                    if (magmag2(rthumb) > 0.03f)
                        player_move(rthumb);
                }

                {
                    float triggers = (float) (state.lZ - 128)
                                       / (float) ((2 << 15) - 256);

                    static int rtrigger_held = 0;
                    if (triggers < 0.4f) {
                        if (!rtrigger_held) {
                            rtrigger_held = 1;
                            player_interact();
                        }
                    } else rtrigger_held = 0;

                    static int ltrigger_held = 0;
                    if (triggers > 0.6f) {
                        if (!ltrigger_held) {
                            ltrigger_held = 1;
                            player_hit();
                        }
                    } else ltrigger_held = 0;
                }

                if (state.rgbButtons[0])
                    player_try_jump();
            }
        }
#endif

        Vec2 move = {0};
        if (key_down(Key_W)) move.x += 1.0f;
        if (key_down(Key_S)) move.x -= 1.0f;
        if (key_down(Key_A)) move.y += 1.0f;
        if (key_down(Key_D)) move.y -= 1.0f;
        if (magmag2(move) > 0.0f)
            player_move(norm2(move));

        if (key_down(Key_Space))
            player_try_jump();

        cam_update();
        player_physics();

        render_frame();

        if (FAILED(render_present(wnd)))
            break;
    }

    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    ExitProcess(0);
}
