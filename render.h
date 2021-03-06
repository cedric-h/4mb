#define SAFE_RELEASE(release, obj) if (obj) release##_Release(obj)

// set to 0 to create resizable window
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// do you need depth buffer?
#define WINDOW_DEPTH 1

// do you need stencil buffer?
#define WINDOW_STENCIL 0

// use sRGB or MSAA for color buffer
// set 0 to disable, or 1 to enable sRGB
// typical values for MSAA are 2, 4, 8, 16, ...
// when enabled, D3D11 cannot use more modern lower-latency flip swap effect on Windows 8.1/10
// instead you can use sRGB/MSAA render target and copy it to non-sRGB window
#define WINDOW_SRGB 0
#define WINDOW_MSAA 0

// do you need vsync?
#define WINDOW_VSYNC 1


typedef struct {
    Vec3 pos;
    Vec3 norm;
    // uint8_t r, g, b, a;
} Vertex;

static Vertex cube_vertices[] = {
    {{0.0f, 1.0f, 0.0f}, 0.0f, 0.0f,-1.0f},
    {{1.0f, 1.0f, 0.0f}, 0.0f, 0.0f,-1.0f},
    {{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f,-1.0f},
    {{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f,-1.0f},
    {{1.0f, 1.0f, 0.0f}, 0.0f, 0.0f,-1.0f},
    {{1.0f, 0.0f, 0.0f}, 0.0f, 0.0f,-1.0f},
    {{1.0f, 1.0f, 0.0f}, 1.0f, 0.0f, 0.0f},
    {{1.0f, 1.0f, 1.0f}, 1.0f, 0.0f, 0.0f},
    {{1.0f, 0.0f, 0.0f}, 1.0f, 0.0f, 0.0f},
    {{1.0f, 0.0f, 0.0f}, 1.0f, 0.0f, 0.0f},
    {{1.0f, 1.0f, 1.0f}, 1.0f, 0.0f, 0.0f},
    {{1.0f, 0.0f, 1.0f}, 1.0f, 0.0f, 0.0f},
    {{1.0f, 1.0f, 1.0f}, 0.0f, 0.0f, 1.0f},
    {{0.0f, 1.0f, 1.0f}, 0.0f, 0.0f, 1.0f},
    {{1.0f, 0.0f, 1.0f}, 0.0f, 0.0f, 1.0f},
    {{1.0f, 0.0f, 1.0f}, 0.0f, 0.0f, 1.0f},
    {{0.0f, 1.0f, 1.0f}, 0.0f, 0.0f, 1.0f},
    {{0.0f, 0.0f, 1.0f}, 0.0f, 0.0f, 1.0f},
    {{0.0f, 1.0f, 1.0f},-1.0f, 0.0f, 0.0f},
    {{0.0f, 1.0f, 0.0f},-1.0f, 0.0f, 0.0f},
    {{0.0f, 0.0f, 1.0f},-1.0f, 0.0f, 0.0f},
    {{0.0f, 0.0f, 1.0f},-1.0f, 0.0f, 0.0f},
    {{0.0f, 1.0f, 0.0f},-1.0f, 0.0f, 0.0f},
    {{0.0f, 0.0f, 0.0f},-1.0f, 0.0f, 0.0f},
    {{0.0f, 1.0f, 1.0f}, 0.0f, 1.0f, 0.0f},
    {{1.0f, 1.0f, 1.0f}, 0.0f, 1.0f, 0.0f},
    {{0.0f, 1.0f, 0.0f}, 0.0f, 1.0f, 0.0f},
    {{0.0f, 1.0f, 0.0f}, 0.0f, 1.0f, 0.0f},
    {{1.0f, 1.0f, 1.0f}, 0.0f, 1.0f, 0.0f},
    {{1.0f, 1.0f, 0.0f}, 0.0f, 1.0f, 0.0f},
    {{0.0f, 0.0f, 0.0f}, 0.0f,-1.0f, 0.0f},
    {{1.0f, 0.0f, 0.0f}, 0.0f,-1.0f, 0.0f},
    {{0.0f, 0.0f, 1.0f}, 0.0f,-1.0f, 0.0f},
    {{0.0f, 0.0f, 1.0f}, 0.0f,-1.0f, 0.0f},
    {{1.0f, 0.0f, 0.0f}, 0.0f,-1.0f, 0.0f},
    {{1.0f, 0.0f, 1.0f}, 0.0f,-1.0f, 0.0f},
};

#define VERT_BUF_SIZE  ((2 << 10) * sizeof(cube_vertices))
#define INDEX_BUF_SIZE ((2 << 10) * _countof(cube_vertices) * sizeof(uint32_t))

typedef struct {
    Mat4 view_proj;
} UniformBuffer;

#include "./build/d3d11_vshader.h"
#include "./build/d3d11_pshader.h"

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
    ID3D11Buffer *uniform_buffer;
} rcx;

// called when device & all d3d resources needs to be released
// can happen multiple times (e.g. after device is removed/reset)
static void render_destroy() {
    if (rcx.context) {
        ID3D11DeviceContext_ClearState(rcx.context);
    }

    SAFE_RELEASE(ID3D11Buffer, rcx.uniform_buffer);
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
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                offsetof(Vertex, pos.x),
                D3D11_INPUT_PER_VERTEX_DATA,
                0
            },
            {
                "NORMAL",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                offsetof(Vertex, norm.x),
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

    // uniform buffer
    {
        D3D11_BUFFER_DESC desc = {
            .Usage = D3D11_USAGE_DYNAMIC,
            .ByteWidth = sizeof(UniformBuffer),
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        hr = ID3D11Device_CreateBuffer(rcx.device, &desc, NULL, &rcx.uniform_buffer);
        LOG_AND_RETURN_ERROR(hr, "ID3D11Device::CreateBuffer failed");
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
        if (WINDOW_STENCIL || feature_level < D3D_FEATURE_LEVEL_10_0)
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

static D3D11_MAPPED_SUBRESOURCE map_buffer(ID3D11Buffer *buf) {
    D3D11_MAPPED_SUBRESOURCE mapped_sub_res;
    HRESULT hr = ID3D11DeviceContext_Map(
        rcx.context,
        (ID3D11Resource*)buf,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mapped_sub_res
    );

    return mapped_sub_res;
}

/* returns the number of indices to render */
static uint32_t generate_geometry(void) {
    D3D11_MAPPED_SUBRESOURCE verts_mapped = map_buffer(rcx.vertex_buffer);
    D3D11_MAPPED_SUBRESOURCE indxs_mapped = map_buffer(rcx.index_buffer);

    int vi = 0, ii = 0;
    for (BoxId id = 1; id < MAX_BOXES; id++) if (OCCUPIED(boxes[id])) {
        Vec3 pos = box_pos_to_vec3(boxes[id].pos);

        for (int local_i = 0; local_i < _countof(cube_vertices); local_i++)
            ((uint32_t*)indxs_mapped.pData)[ii++] = vi + local_i;

        for (int start_i = vi; (vi - start_i) < _countof(cube_vertices); vi++) {
            #define BASE_VERT (cube_vertices[vi - start_i])
            Vertex *vertex = (Vertex*)verts_mapped.pData + vi;
            *vertex = (Vertex) {
                .pos = add3(BASE_VERT.pos, pos),
                .norm = BASE_VERT.norm,
            };
        }
    }

    ID3D11DeviceContext_Unmap(rcx.context, (ID3D11Resource*)rcx.vertex_buffer, 0);
    ID3D11DeviceContext_Unmap(rcx.context, (ID3D11Resource*)rcx.index_buffer, 0);
    return ii;
}

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
    FLOAT clear_color[] = { 100.f/255.f, 149.f/255.f, 237.f/255.f, 1.f };
    ID3D11DeviceContext_ClearRenderTargetView(
        rcx.context,
        rcx.window_rtview,
        clear_color
    );

    D3D11_MAPPED_SUBRESOURCE uniform_mapped = map_buffer(rcx.uniform_buffer);

    /* Get a pointer to the data in the constant buffer. */
    UniformBuffer *uniform_ptr = (UniformBuffer*)uniform_mapped.pData;
    /* Copy the data into the constant buffer. */
    Vec2 ss = state.screen_size;
    Mat4 proj = perspective4x4(PI_f * 0.25f, ss.x/ss.y, 0.01f, 100.0f);
    Vec3 eye = player_eye();
    Mat4 view = look_at4x4(eye, add3(eye, cam_facing()), vec3_y);
    Mat4 m = mul4x4(proj, view);
    // m = mul4x4(m, translate4x4((Vec3) { 0.0f, -1.0f, 0.0f }));
    // m = mul4x4(m, rotate4x4(vec3_x, state.rot));
    // m = mul4x4(m, rotate4x4(vec3_y, state.rot));
    // m = mul4x4(m, rotate4x4(vec3_z, state.rot));
    uniform_ptr->view_proj = transpose4x4(m);

    ID3D11DeviceContext_Unmap(rcx.context, (ID3D11Resource*)rcx.uniform_buffer, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(rcx.context, 0, 1, &rcx.uniform_buffer);

    uint32_t index_count = generate_geometry();

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
    ID3D11DeviceContext_DrawIndexed(rcx.context, index_count, 0, 0);
}
