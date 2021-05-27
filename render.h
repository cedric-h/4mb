// do you need depth buffer?
#define WINDOW_DEPTH 0

// do you need stencil buffer?
#define WINDOW_STENCIL 0

// use sRGB or MSAA for color buffer
// set 0 to disable, or 1 to enable sRGB
// typical values for MSAA are 2, 4, 8, 16, ...
// when enabled, D3D11 cannot use more modern lower-latency flip swap effect on Windows 8.1/10
// instead you can use sRGB/MSAA render target and copy it to non-sRGB window
#define WINDOW_SRGB 0
#define WINDOW_MSAA 8

// do you need vsync?
#define WINDOW_VSYNC 1

#include "d3d11_vshader.h"
#include "d3d11_pshader.h"

#define SAFE_RELEASE(release, obj) if (obj) release##_Release(obj)

#define LOG_AND_RETURN_ERROR(hr, msg) do { \
    if (FAILED(hr)) {                      \
        log_win32_err(hr, msg);            \
        return hr;                         \
    }                                      \
} while (0)

typedef struct {
    Vec2 pos;
    Byte4 rgba;
    uint16_t size[2];
    uint8_t radius;
} Vertex;

#define VERT_BUF_COUNT (MAX_ENTS * 4)
#define VERT_BUF_SIZE  (VERT_BUF_COUNT * sizeof(Vertex))
#define INDEX_BUF_SIZE (MAX_ENTS * 6 * sizeof(uint32_t))

static struct {
    /* is window visible?
       if this is 0 you can skip rendering part in your code to save time */
    int occluded;
    IDXGISwapChain *swapchain;
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11DeviceContext1 *context1;
    ID3D11RenderTargetView *window_rtview;
    #if WINDOW_DEPTH || WINDOW_STENCIL
    ID3D11DepthStencilView *window_dpview;
    #endif
    HANDLE frame_latency_wait;

    ID3D11RasterizerState *raster_state;
    ID3D11DepthStencilState *depthstencil_state;
    ID3D11BlendState *blend_state;
    ID3D11PixelShader *pixel_shader;
    ID3D11VertexShader *vertex_shader;
    ID3D11InputLayout *input_layout;
    ID3D11Buffer *vertex_buffer;
    ID3D11Buffer *index_buffer;
} rcx;

static void log_win32_err(DWORD err, const char *msg) {
    #if USE_DEBUG_MODE
    OutputDebugStringA(msg);
    OutputDebugStringA("!\n");

    LPWSTR str;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if (FormatMessageW(flags, NULL, err, lang, (LPWSTR)&str, 0, NULL)) {
        OutputDebugStringW(str);
        LocalFree(str);
    }
    #endif
}
static void log_win32_last_err(const char *msg) {
    #if USE_DEBUG_MODE
    log_win32_err(GetLastError(), msg);
    #endif
}

static HRESULT fatal_device_lost_error() {
    #if USE_DEBUG_MODE
    wchar_t *msg = L"Cannot recreate D3D11 device, it is reset or removed!";
    MessageBoxW(NULL, msg, L"Error", MB_ICONEXCLAMATION);
    #endif
    return E_FAIL;
}

// called when device & all d3d resources needs to be released
// can happen multiple times (e.g. after device is removed/reset)
static void render_destroy() {
    if (rcx.context) {
        ID3D11DeviceContext_ClearState(rcx.context);
    }

    SAFE_RELEASE(ID3D11Buffer, rcx.index_buffer);
    SAFE_RELEASE(ID3D11Buffer, rcx.vertex_buffer);
    SAFE_RELEASE(ID3D11InputLayout, rcx.input_layout);
    SAFE_RELEASE(ID3D11VertexShader, rcx.vertex_shader);
    SAFE_RELEASE(ID3D11PixelShader, rcx.pixel_shader);
    SAFE_RELEASE(ID3D11RasterizerState, rcx.raster_state);
    SAFE_RELEASE(ID3D11DepthStencilState, rcx.depthstencil_state);
    SAFE_RELEASE(ID3D11BlendState, rcx.blend_state);

#if WINDOW_DEPTH || WINDOW_STENCIL
    SAFE_RELEASE(ID3D11DepthStencilView, rcx.window_dpview);
#endif
    SAFE_RELEASE(ID3D11RenderTargetView, rcx.window_rtview);
    SAFE_RELEASE(ID3D11DeviceContext, rcx.context);
    SAFE_RELEASE(ID3D11Device, rcx.device);
    SAFE_RELEASE(IDXGISwapChain, rcx.swapchain);

    rcx.context1 = NULL;
    rcx.frame_latency_wait = NULL;
}

// called any time device needs to be created
// can happen multiple times (e.g. after device is removed/reset)
static HRESULT render_create(HWND wnd) {
    HRESULT hr;
    D3D_FEATURE_LEVEL level;

    // device, context
    {
        UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if USE_DEBUG_MODE
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        if (FAILED(hr = D3D11CreateDevice(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0, D3D11_SDK_VERSION,
            &rcx.device, &level, &rcx.context)))
        {
            if (FAILED(hr = D3D11CreateDevice(
                NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, NULL, 0, D3D11_SDK_VERSION,
                &rcx.device, &level, &rcx.context)))
            {
                LOG_AND_RETURN_ERROR(hr, "D3D11CreateDevice failed");
            }
        }

        hr = ID3D11DeviceContext_QueryInterface(
            rcx.context,
            &IID_ID3D11DeviceContext1,
            &rcx.context1
        );
        if (SUCCEEDED(hr)) {
            // using ID3D11DeviceContext1 to discard render target
            ID3D11DeviceContext_Release(rcx.context);
            rcx.context = (ID3D11DeviceContext*)rcx.context1;
        }
    }

    // swap chain
    {
        IDXGIFactory *factory;
        if (FAILED(hr = CreateDXGIFactory(&IID_IDXGIFactory, &factory)))
            LOG_AND_RETURN_ERROR(hr, "CreateDXGIFactory failed");

        DXGI_SWAP_CHAIN_DESC desc = {
            .BufferDesc = {
#if WINDOW_SRGB
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
#else
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
#endif
                .RefreshRate = {
                    .Numerator = 60,
                    .Denominator = 1,
                },
            },
            .SampleDesc = {
#if WINDOW_MSAA
                .Count = WINDOW_MSAA,
#else
                .Count = 1,
#endif
                .Quality = 0,
            },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .OutputWindow = wnd,
            .Windowed = TRUE,
        };

#if !WINDOW_SRGB && !WINDOW_MSAA
        // Windows 10 and up
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        if (FAILED(IDXGIFactory_CreateSwapChain(
            factory,
            (IUnknown*)rcx.device,
            &desc,
            &rcx.swapchain
        ))) {
            // Windows 8.1
            desc.BufferCount = 2;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            hr = IDXGIFactory_CreateSwapChain(
                factory,
                (IUnknown*)rcx.device,
                &desc,
                &rcx.swapchain
            );
        }
#else
        hr = E_FAIL;
#endif
        if (FAILED(hr)) {
            // older Windows
            desc.BufferCount = 1;
            desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            desc.Flags = 0;
            hr = IDXGIFactory_CreateSwapChain(
                factory,
                (IUnknown*)rcx.device,
                &desc,
                &rcx.swapchain
            );
            LOG_AND_RETURN_ERROR(hr, "IDXGIFactory::CreateSwapChain failed");
        }

        if (desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) {
            IDXGISwapChain2 *swapchain2;
            if (SUCCEEDED(hr = IDXGISwapChain_QueryInterface(
                rcx.swapchain,
                &IID_IDXGISwapChain2,
                &swapchain2
            ))) {
                // using IDXGISwapChain2 for frame latency control
                rcx.frame_latency_wait = IDXGISwapChain2_GetFrameLatencyWaitableObject(
                    swapchain2
                );
                IDXGISwapChain2_Release(swapchain2);
            }
        }

        hr = IDXGIFactory_MakeWindowAssociation(
            factory,
            wnd,
            DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER
        );
        LOG_AND_RETURN_ERROR(hr, "IDXGIFactory::MakeWindowAssociation failed");

        IDXGIFactory_Release(factory);
    }

    // rasterizer state
    {
        D3D11_RASTERIZER_DESC desc = {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_BACK,
            .FrontCounterClockwise = FALSE,
            .DepthBias = 0,
            .DepthBiasClamp = 0,
            .SlopeScaledDepthBias = 0.0f,
            .DepthClipEnable = TRUE,
            .ScissorEnable = FALSE,
            .MultisampleEnable = WINDOW_MSAA > 0,
            .AntialiasedLineEnable = FALSE,
        };

        hr = ID3D11Device_CreateRasterizerState(
            rcx.device,
            &desc,
            &rcx.raster_state
        );
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateRasterizerState failed");
    }

#if WINDOW_DEPTH || WINDOW_STENCIL
    // depth & stencil state
    {
        D3D11_DEPTH_STENCIL_DESC desc = {
            .DepthEnable = WINDOW_DEPTH ? TRUE : FALSE,
            .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D11_COMPARISON_LESS,
            .StencilEnable = FALSE, // if you need stencil, set up it here
            .StencilReadMask = 0,
            .StencilWriteMask = 0,
        };

        hr = ID3D11Device_CreateDepthStencilState(
            rcx.device,
            &desc,
            &rcx.depthstencil_state
        );
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateDepthStencilState failed");
    }
#endif

    // blend state
    {
        D3D11_BLEND_DESC desc = {
            .AlphaToCoverageEnable = FALSE,
            .IndependentBlendEnable = FALSE,
            .RenderTarget = {
                {
                    .BlendEnable = TRUE,
                    .SrcBlend = D3D11_BLEND_ONE,
                    .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
                    .BlendOp = D3D11_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D11_BLEND_ONE,
                    .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
                    .BlendOpAlpha = D3D11_BLEND_OP_ADD,
                    .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
                },
            },
        };

        hr = ID3D11Device_CreateBlendState(rcx.device, &desc, &rcx.blend_state);
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateBlendState failed");
    }

    // vertex shader & input layout
    {
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {
                "POSITION",
                0,
                DXGI_FORMAT_R32G32_FLOAT,
                0,
                offsetof(Vertex, pos.x),
                D3D11_INPUT_PER_VERTEX_DATA,
                0
            },
            {
                "COLOR",
                0,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                0,
                offsetof(Vertex, rgba.r),
                D3D11_INPUT_PER_VERTEX_DATA,
                0
            },
            {
                "SIZE",
                0,
                DXGI_FORMAT_R16G16_UNORM,
                0,
                offsetof(Vertex, size[0]),
                D3D11_INPUT_PER_VERTEX_DATA,
                0
            },
            {
                "RADIUS",
                0,
                DXGI_FORMAT_R8_UNORM,
                0,
                offsetof(Vertex, radius),
                D3D11_INPUT_PER_VERTEX_DATA,
                0
            },
        };

        ID3DBlob *code = NULL;
        const void *vshader;
        size_t vshader_size;

        vshader = d3d11_vshader;
        vshader_size = sizeof(d3d11_vshader);

        hr = ID3D11Device_CreateVertexShader(
            rcx.device,
            vshader,
            vshader_size,
            NULL,
            &rcx.vertex_shader
        );
        if (FAILED(hr)) {
            SAFE_RELEASE(ID3D10Blob, code);
            LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateVertexShader failed");
        }

        hr = ID3D11Device_CreateInputLayout(
            rcx.device,
            layout,
            _countof(layout),
            vshader,
            vshader_size,
            &rcx.input_layout
        );
        SAFE_RELEASE(ID3D10Blob, code);
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateInputLayout failed");
    }

    // pixel shader
    {
        ID3DBlob *code = NULL;
        const void *pshader;
        size_t pshader_size;

        pshader = d3d11_pshader;
        pshader_size = sizeof(d3d11_pshader);

        hr = ID3D11Device_CreatePixelShader(
            rcx.device,
            pshader,
            pshader_size,
            NULL,
            &rcx.pixel_shader
        );
        SAFE_RELEASE(ID3D10Blob, code);
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreatePixelShader failed");
    }

    // vertex buffer
    {
        D3D11_BUFFER_DESC desc = {
            .ByteWidth = VERT_BUF_SIZE,
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        hr = ID3D11Device_CreateBuffer(
            rcx.device,
            &desc,
            NULL,
            &rcx.vertex_buffer
        );
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateBuffer failed (vertex)");
    }

    // index buffer
    {
        D3D11_BUFFER_DESC desc = {
            .ByteWidth = INDEX_BUF_SIZE,
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_INDEX_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        hr = ID3D11Device_CreateBuffer(
            rcx.device,
            &desc,
            NULL,
            &rcx.index_buffer
        );
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateBuffer failed (index)");
    }

    return S_OK;
}

// called when device is reset or removed, recreate it
static HRESULT recreate_device(HWND wnd) {
    render_destroy();
    HRESULT hr = render_create(wnd);
    if (FAILED(hr))
        render_destroy();
    return hr;
}

// called when window is resized
static HRESULT render_resize(HWND wnd, int width, int height) {
    if (width == 0 || height == 0)
        return S_OK;

    if (rcx.window_rtview) {
        ID3D11DeviceContext_OMSetRenderTargets(rcx.context, 0, NULL, NULL);
        ID3D11RenderTargetView_Release(rcx.window_rtview);
        rcx.window_rtview = NULL;
    }

#if WINDOW_DEPTH || WINDOW_STENCIL
    if (rcx.window_dpview) {
        ID3D11DepthStencilView_Release(rcx.window_dpview);
        rcx.window_dpview = NULL;
    }
#endif

    UINT flags = rcx.frame_latency_wait
        ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
        : 0;
    HRESULT hr = IDXGISwapChain_ResizeBuffers(
        rcx.swapchain,
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        flags
    );
    if (hr == DXGI_ERROR_DEVICE_REMOVED
        || hr == DXGI_ERROR_DEVICE_RESET
        || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
    {
        if (FAILED(recreate_device(wnd)))
            return fatal_device_lost_error();
    }
    else
        LOG_AND_RETURN_ERROR(hr, "IDXGISwapChain::ResizeBuffers failed");

    ID3D11Texture2D *window_buffer;
    hr = IDXGISwapChain_GetBuffer(
        rcx.swapchain,
        0,
        &IID_ID3D11Texture2D,
        &window_buffer
    );
    LOG_AND_RETURN_ERROR(hr, "IDXGISwapChain::GetBuffer failed");

    hr = ID3D11Device_CreateRenderTargetView(
        rcx.device,
        (ID3D11Resource*)window_buffer,
        NULL,
        &rcx.window_rtview
    );
    ID3D11Texture2D_Release(window_buffer);
    LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateRenderTargetView failed");

#if WINDOW_DEPTH || WINDOW_STENCIL
    {
        D3D11_TEXTURE2D_DESC desc = {
            .Width = width,
            .Height = height,
            .MipLevels = 1,
            .ArraySize = 1,
            .SampleDesc = {
#if WINDOW_MSAA
                .Count = WINDOW_MSAA,
#else
                .Count = 1,
#endif
                .Quality = 0,
            },
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_DEPTH_STENCIL,
        };

        D3D_FEATURE_LEVEL feature_level = ID3D11Device_GetFeatureLevel(rcx.device);
        if ((WINDOW_STENCIL || feature_level < D3D_FEATURE_LEVEL_10_0)
            desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        else
            desc.Format = DXGI_FORMAT_D32_FLOAT;

        ID3D11Texture2D *depth_stencil;
        hr = ID3D11Device_CreateTexture2D(rcx.device, &desc, NULL, &depth_stencil);
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateTexture2D failed");

        hr = ID3D11Device_CreateDepthStencilView(
            rcx.device,
            (ID3D11Resource*)depth_stencil,
            NULL,
            &rcx.window_dpview
        );
        ID3D11Texture2D_Release(depth_stencil);
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateDepthStencilView failed");
    }
#endif

    D3D11_VIEWPORT viewport =
    {
        .TopLeftX = 0.f,
        .TopLeftY = 0.f,
        .Width = (float)width,
        .Height = (float)height,
        .MinDepth = 0.f,
        .MaxDepth = 1.f,
    };
    ID3D11DeviceContext_RSSetViewports(rcx.context, 1, &viewport);

    return S_OK;
}

// called at end of frame
static HRESULT render_present(HWND wnd) {
    HRESULT hr = S_OK;
    if (rcx.occluded) {
        hr = IDXGISwapChain_Present(rcx.swapchain, 0, DXGI_PRESENT_TEST);

        /* DXGI window is back to normal, resuming rendering */
        if (SUCCEEDED(hr) && hr != DXGI_STATUS_OCCLUDED)
            rcx.occluded = 0;
    }

    if (!rcx.occluded)
        hr = IDXGISwapChain_Present(rcx.swapchain, WINDOW_VSYNC, 0);

    if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
        if (FAILED(recreate_device(wnd)))
            return fatal_device_lost_error();

        RECT rect;
        if (!GetClientRect(wnd, &rect))
            log_win32_last_err("GetClientRect failed");
        else render_resize(wnd, rect.right - rect.left, rect.bottom - rect.top);
    } else if (hr == DXGI_STATUS_OCCLUDED) {
        rcx.occluded = 1; /* DXGI window is occluded, skipping rendering */
    } else LOG_AND_RETURN_ERROR(hr, "IDXGISwapChain::Present failed");

    if (rcx.occluded) {
        Sleep(10);
    } else if (rcx.context1) {
        ID3D11DeviceContext1_DiscardView(
            rcx.context1,
            (ID3D11View*)rcx.window_rtview
        );
    }

    return S_OK;
}

// this is where rendering happens
static void render_frame() {
    if (rcx.occluded) return;

    if (rcx.frame_latency_wait)
        WaitForSingleObjectEx(rcx.frame_latency_wait, INFINITE, TRUE);

#if WINDOW_DEPTH || WINDOW_STENCIL
    ID3D11DeviceContext_OMSetRenderTargets(
        rcx.context,
        1,
        &rcx.window_rtview,
        rcx.window_dpview
    );
    ID3D11DeviceContext_OMSetDepthStencilState(
        rcx.context,
        rcx.depthstencil_state,
        0
    );
    ID3D11DeviceContext_ClearDepthStencilView(
        rcx.context,
        rcx.window_dpview,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.0f, 0
    );
#else
    ID3D11DeviceContext_OMSetRenderTargets(
        rcx.context,
        1,
        &rcx.window_rtview,
        NULL
    );
#endif

    // clear background
    FLOAT clear_color[] = {
        colors[Color_Blue].r / 255.0f,
        colors[Color_Blue].g / 255.0f,
        colors[Color_Blue].b / 255.0f,
        colors[Color_Blue].a / 255.0f
    };
    ID3D11DeviceContext_ClearRenderTargetView(
        rcx.context,
        rcx.window_rtview,
        clear_color
    );

    // fill vertex buffer
    D3D11_MAPPED_SUBRESOURCE verts_mapped;
    D3D11_MAPPED_SUBRESOURCE indices_mapped;
    HRESULT hr = ID3D11DeviceContext_Map(
        rcx.context,
        (ID3D11Resource*)rcx.vertex_buffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &verts_mapped
    );
    hr = ID3D11DeviceContext_Map(
        rcx.context,
        (ID3D11Resource*)rcx.index_buffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &indices_mapped
    );

    #if USE_DEBUG_MODE
    if (FAILED(hr))
        OutputDebugStringA("Couldn't map vertex/index array");
    #endif

    /* indices: 0, 1, 2, 1, 3, 2 */
    const Vertex square[] = {
        { {-0.5f,  0.5f} },
        { { 0.5f,  0.5f} },
        { {-0.5f, -0.5f} },
        { { 0.5f, -0.5f} },
    };

    /* pixels per unit */
    #define WORLD_SCALE (30.0f)
    Vec2 ss = div2_f(state.screen_size, 2.0f * WORLD_SCALE);
    Mat3 cam = ortho3x3(-ss.x, ss.x, -ss.y, ss.y);

    int vi = 0, ii = 0;
    for (Ent *ent = 0; ent = ent_all_iter(ent); ) {
        ((uint32_t*)indices_mapped.pData)[ii++] = vi + 0;
        ((uint32_t*)indices_mapped.pData)[ii++] = vi + 1;
        ((uint32_t*)indices_mapped.pData)[ii++] = vi + 2;
        ((uint32_t*)indices_mapped.pData)[ii++] = vi + 1;
        ((uint32_t*)indices_mapped.pData)[ii++] = vi + 3;
        ((uint32_t*)indices_mapped.pData)[ii++] = vi + 2;

        Mat3 m = cam;
        m = mul3x3(m, translate3x3(ent->pos));
        m = mul3x3(m, rotate3x3(ent->rot));
        m = mul3x3(m, scale3x3(ent->scale));
        for (int start_i = vi; (vi - start_i) < 4; vi++) {
            Vec3 p = {0.0f, 0.0f, 1.0f};
            p.xy = square[vi - start_i].pos;

            Vertex *vertex = &((Vertex*)verts_mapped.pData)[vi];
            *vertex = (Vertex) {
                .pos = mul3x33(m, p).xy,
                .rgba = ent->rgba,
                .radius = ent->corner_radii.nums[vi - start_i],
                .size[0] = (uint16_t) (WORLD_SCALE * ent->scale.x),
                .size[1] = (uint16_t) (WORLD_SCALE * ent->scale.y),
            };
        }
    }

    ID3D11DeviceContext_Unmap(rcx.context, (ID3D11Resource*)rcx.index_buffer, 0);
    ID3D11DeviceContext_Unmap(rcx.context, (ID3D11Resource*)rcx.vertex_buffer, 0);

    // draw a triangle
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    ID3D11DeviceContext_IASetInputLayout(rcx.context, rcx.input_layout);
    ID3D11DeviceContext_IASetVertexBuffers(
        rcx.context,
        0, 1,
        &rcx.vertex_buffer,
        &stride,
        &offset
    );
    ID3D11DeviceContext_IASetIndexBuffer(
        rcx.context,
        rcx.index_buffer,
        DXGI_FORMAT_R32_UINT,
        0
    );
    ID3D11DeviceContext_IASetPrimitiveTopology(
        rcx.context,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );
    ID3D11DeviceContext_VSSetShader(rcx.context, rcx.vertex_shader, NULL, 0);
    ID3D11DeviceContext_PSSetShader(rcx.context, rcx.pixel_shader, NULL, 0);
    ID3D11DeviceContext_RSSetState(rcx.context, rcx.raster_state);
    ID3D11DeviceContext_OMSetBlendState(
        rcx.context,
        rcx.blend_state,
        NULL,
        ~0U
    );
    ID3D11DeviceContext_DrawIndexed(rcx.context, ii, 0, 0);
}

