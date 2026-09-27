#include "render.hpp"
#include "text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <d3dcompiler.h>

namespace vgui::detail {
namespace {
void check(HRESULT result) {
    if (FAILED(result))
        throw DxError(result);
}
// Swap the complete pipeline state, including slots the host may use but we don't.
struct StateScope {
    ID3D11DeviceContext1* context;
    ComPtr<ID3DDeviceContextState> saved;
    StateScope(ID3D11DeviceContext1* ctx, ID3DDeviceContextState* state) : context(ctx) {
        context->SwapDeviceContextState(state, &saved);
    }
    ~StateScope() {
        // Don't keep the host's backbuffer alive in our saved state after drawing.
        context->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* empty = nullptr;
        context->PSSetShaderResources(0, 1, &empty);
        context->SwapDeviceContextState(saved.Get(), nullptr);
    }
};
}
Renderer::Renderer(HWND window, Theme* colors, ID3D11Device* supplied, ID3D11DeviceContext* context)
    : external(supplied != nullptr || context != nullptr), hwnd(window), theme(colors) {
    reset(supplied, context);
}
void Renderer::create_device() {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = hwnd;
    desc.Windowed = TRUE;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               levels, 1, D3D11_SDK_VERSION, &desc, &swapChain,
                                               &device, nullptr, &deviceContext);
    if (FAILED(hr))
        check(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 1,
                                            D3D11_SDK_VERSION, &desc, &swapChain, &device, nullptr,
                                            &deviceContext));
}
void Renderer::reset(ID3D11Device* supplied, ID3D11DeviceContext* context) {
    ComPtr<ID3D11Device> keepDevice = supplied;
    ComPtr<ID3D11DeviceContext> keepContext = context;
    if (external) {
        if (!supplied || !context || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
            throw std::invalid_argument(
                "External rendering needs a device and its immediate context");
        ComPtr<ID3D11Device> owner;
        context->GetDevice(&owner);
        if (owner.Get() != supplied)
            throw std::invalid_argument("Device/context mismatch");
    } else if (supplied || context)
        throw std::logic_error("This renderer owns its device");
    clear_frame();
    uiState.Reset();
    context1.Reset();
    target.Reset();
    vertexBuffer.Reset();
    vertexShader.Reset();
    pixelShader.Reset();
    layout.Reset();
    sampler.Reset();
    blend.Reset();
    raster.Reset();
    depth.Reset();
    pages.clear();
    glyphs.clear();
    swapChain.Reset();
    deviceContext.Reset();
    device.Reset();
    pixelWidth = pixelHeight = 0;
    width = height = 0;
    deviceLost = true;
    if (external) {
        device = supplied;
        deviceContext = context;
    } else
        create_device();
    create_resources();
    deviceLost = false;
}
bool Renderer::lost() const {
    return deviceLost || !device || FAILED(device->GetDeviceRemovedReason());
}
bool Renderer::ready() {
    if (!lost())
        return true;
    if (external)
        return false;
    try {
        reset();
        return true;
    } catch (const DxError&) {
        return false;
    }
}
void Renderer::create_resources() {
    check(deviceContext.As(&context1));
    ComPtr<ID3D11Device1> device1;
    check(device.As(&device1));
    D3D_FEATURE_LEVEL feature = device->GetFeatureLevel();
    check(device1->CreateDeviceContextState(0, &feature, 1, D3D11_SDK_VERSION,
                                            __uuidof(ID3D11Device), nullptr, &uiState));
    const char* shader = R"(
struct V { float2 p:POSITION; float2 uv:TEXCOORD; float4 c:COLOR; };
struct P { float4 p:SV_POSITION; float2 uv:TEXCOORD; float4 c:COLOR; };
P vs_main(V v) { P o; o.p=float4(v.p,0,1); o.uv=v.uv; o.c=v.c; return o; }
Texture2D tex:register(t0); SamplerState smp:register(s0);
float4 ps_main(P p):SV_TARGET { return p.c*tex.Sample(smp,p.uv); }
)";
    ComPtr<ID3DBlob> vs, ps, errors;
    check(D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "vs_main", "vs_4_0", 0, 0,
                     &vs, &errors));
    check(D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "ps_main", "ps_4_0", 0, 0,
                     &ps, &errors));
    check(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr,
                                     &vertexShader));
    check(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr,
                                    &pixelShader));
    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    check(device->CreateInputLayout(elements, 3, vs->GetBufferPointer(), vs->GetBufferSize(),
                                    &layout));
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampling.MaxLOD = D3D11_FLOAT32_MAX;
    check(device->CreateSamplerState(&sampling, &sampler));
    D3D11_BLEND_DESC blending{};
    auto& b = blending.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    b.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    b.BlendOp = b.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D11_BLEND_ONE;
    b.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    b.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    check(device->CreateBlendState(&blending, &blend));
    D3D11_RASTERIZER_DESC rasterizing{};
    rasterizing.FillMode = D3D11_FILL_SOLID;
    rasterizing.CullMode = D3D11_CULL_NONE;
    rasterizing.DepthClipEnable = TRUE;
    check(device->CreateRasterizerState(&rasterizing, &raster));
    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    check(device->CreateDepthStencilState(&depthDesc, &depth));
    create_font();
}
void Renderer::add_font_page() {
    FontPage page;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 2048;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    std::vector<uint32_t> pixels(2048 * 2048, 0x00ffffff);
    pixels.back() = 0xffffffff;
    D3D11_SUBRESOURCE_DATA data{pixels.data(), 2048 * 4, 0};
    check(device->CreateTexture2D(&desc, &data, &page.texture));
    check(device->CreateShaderResourceView(page.texture.Get(), nullptr, &page.view));
    pages.push_back(std::move(page));
}
void Renderer::create_font() {
    glyphs.clear();
    pages.clear();
    add_font_page();
}
void Renderer::set_scale(float value) {
    if (scale == value)
        return;
    scale = value;
    create_font();
    width = pixelWidth / scale;
    height = pixelHeight / scale;
}
Renderer::Glyph& Renderer::glyph(uint32_t codepoint) try {
    auto found = glyphs.find(codepoint);
    if (found != glyphs.end())
        return found->second;
    const auto text = utf16(encode(codepoint));
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-static_cast<int>(std::lround(12 * scale)), 0, 0, 0, FW_NORMAL, FALSE,
                             FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
    if (!dc || !font) {
        if (dc)
            DeleteDC(dc);
        if (font)
            DeleteObject(font);
        throw std::runtime_error("Could not create font");
    }
    auto oldFont = SelectObject(dc, font);
    SIZE extent{};
    GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &extent);
    int w = std::max(2L, extent.cx + 2), h = std::max(2, static_cast<int>(std::ceil(20 * scale)));
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) {
        SelectObject(dc, oldFont);
        DeleteObject(font);
        DeleteDC(dc);
        throw std::runtime_error("Could not rasterize glyph");
    }
    auto oldBitmap = SelectObject(dc, bitmap);
    PatBlt(dc, 0, 0, w, h, BLACKNESS);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, 0, 0, text.data(), static_cast<int>(text.size()));
    GdiFlush();
    std::vector<uint32_t> pixels(static_cast<size_t>(w) * h);
    auto source = static_cast<uint32_t*>(bits);
    for (size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = 0x00ffffff | ((source[i] & 255) << 24);
    SelectObject(dc, oldBitmap);
    SelectObject(dc, oldFont);
    DeleteObject(bitmap);
    DeleteObject(font);
    DeleteDC(dc);
    auto* page = &pages.back();
    if (page->x + w >= 2048) {
        page->x = 0;
        page->y += page->rowHeight + 1;
        page->rowHeight = 0;
    }
    if (page->y + h >= 2047) {
        add_font_page();
        page = &pages.back();
    }
    D3D11_BOX box{static_cast<UINT>(page->x),     static_cast<UINT>(page->y),     0,
                  static_cast<UINT>(page->x + w), static_cast<UINT>(page->y + h), 1};
    deviceContext->UpdateSubresource(page->texture.Get(), 0, &box, pixels.data(), w * 4, 0);
    Glyph result{{page->x / 2048.f, page->y / 2048.f, w / 2048.f, h / 2048.f},
                 w / scale,
                 h / scale,
                 extent.cx / scale,
                 page->view.Get()};
    page->x += w + 1;
    page->rowHeight = std::max(page->rowHeight, h);
    return glyphs.emplace(codepoint, result).first->second;
} catch (const DxError&) {
    if (!lost())
        throw;
    // Finish UI submission even if a new atlas page fails during device removal.
    // end_frame discards the failed draw; the next frame can recreate resources.
    deviceLost = true;
    static Glyph missing{{}, 0, 0, 8, nullptr};
    return missing;
}
void Renderer::quad(Rect r, Color c, float u0, float v0, float u1, float v1,
                    ID3D11ShaderResourceView* texture) {
    float left = std::max(r.x, clip.x), top = std::max(r.y, clip.y);
    float right = std::min(r.x + r.w, clip.x + clip.w),
          bottom = std::min(r.y + r.h, clip.y + clip.h);
    if (right <= left || bottom <= top || width <= 0 || height <= 0)
        return;
    float ul = u0 + (u1 - u0) * (left - r.x) / r.w, ur = u0 + (u1 - u0) * (right - r.x) / r.w;
    float vt = v0 + (v1 - v0) * (top - r.y) / r.h, vb = v0 + (v1 - v0) * (bottom - r.y) / r.h;
    if (!texture)
        texture = pages.front().view.Get();
    auto vertex = [&](float x, float y, float u, float v) {
        return Vertex{2 * x / width - 1, 1 - 2 * y / height, u,           v,       c.r / 255.f,
                      c.g / 255.f,       c.b / 255.f,        c.a / 255.f, texture, layer};
    };
    auto a = vertex(left, top, ul, vt), b = vertex(right, top, ur, vt);
    auto d = vertex(left, bottom, ul, vb), e = vertex(right, bottom, ur, vb);
    vertices.insert(vertices.end(), {a, b, e, a, e, d});
}
void Renderer::image(Rect rect, ID3D11ShaderResourceView* texture, Rect uv, Color tint) {
    if (!texture)
        throw std::invalid_argument("Image texture is null");
    ComPtr<ID3D11Device> owner;
    texture->GetDevice(&owner);
    if (owner.Get() != device.Get())
        throw std::invalid_argument("Image belongs to another device");
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    texture->GetDesc(&desc);
    if (desc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D)
        throw std::invalid_argument("Image needs a Texture2D SRV");
    frameTextures.emplace_back(texture);
    quad(rect, tint, uv.x, uv.y, uv.x + uv.w, uv.y + uv.h, texture);
}
void Renderer::label(float x, float y, std::string_view text, Color color) {
    const float start = x;
    for (size_t i = 0; i < text.size();) {
        auto c = decode(text, i);
        i = c.next;
        if (c.value == '\n') {
            x = start;
            y += 17;
            continue;
        }
        if (c.value == '\r')
            continue;
        const auto& g = glyph(c.value);
        quad({x, y, g.width, g.height}, color, g.uv.x, g.uv.y, g.uv.x + g.uv.w, g.uv.y + g.uv.h,
             g.texture);
        x += g.advance;
    }
}
float Renderer::measure(std::string_view text) {
    float line = 0, longest = 0;
    for (size_t i = 0; i < text.size();) {
        auto c = decode(text, i);
        i = c.next;
        if (c.value == '\n') {
            longest = std::max(longest, line);
            line = 0;
        } else if (c.value != '\r')
            line += glyph(c.value).advance;
    }
    return std::max(longest, line);
}
void Renderer::bevel(Rect r, bool sunken) {
    auto a = sunken ? theme->shadow : theme->light, b = sunken ? theme->light : theme->shadow;
    quad({r.x, r.y, r.w, 1}, a);
    quad({r.x, r.y, 1, r.h}, a);
    quad({r.x, r.y + r.h - 1, r.w, 1}, b);
    quad({r.x + r.w - 1, r.y, 1, r.h}, b);
}
void Renderer::checkmark(Rect box) {
    constexpr const char* pixels[] = {"       ##", "      ###", "     ### ", "##  ###  ",
                                      "######   ", " ####    ", "  ##     "};
    float x = std::floor(box.x + (box.w - 9) / 2), y = std::floor(box.y + (box.h - 7) / 2);
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 9; ++col)
            if (pixels[row][col] == '#')
                quad({x + col, y + row, 1, 1}, theme->check);
}
void Renderer::arrow_icon(Rect box, bool upward, bool held) {
    float x = std::floor(box.x + (box.w - 7) / 2) + (held ? 1.f : 0.f);
    float y = std::floor(box.y + (box.h - 4) / 2) + (held ? 1.f : 0.f);
    for (int row = 0; row < 4; ++row) {
        int step = upward ? row : 3 - row;
        quad({x + 3 - step, y + row, static_cast<float>(1 + 2 * step), 1}, theme->text);
    }
}
void Renderer::resize(UINT w, UINT h) {
    if (pixelWidth == w && pixelHeight == h)
        return;
    if (!external) {
        {
            StateScope scope(context1.Get(), uiState.Get());
            deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        }
        deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        target.Reset();
        check(swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0));
        ComPtr<ID3D11Texture2D> back;
        check(swapChain->GetBuffer(0, IID_PPV_ARGS(&back)));
        check(device->CreateRenderTargetView(back.Get(), nullptr, &target));
    }
    pixelWidth = w;
    pixelHeight = h;
    width = w / scale;
    height = h / scale;
}
void Renderer::clear_frame() {
    vertices.clear();
    overlay.clear();
    frameTextures.clear();
    drawCalls = 0;
    layer = 0;
}
void Renderer::present(ID3D11RenderTargetView* destination) {
    if (external && !destination)
        throw std::invalid_argument("External rendering needs a target");
    if (!external && destination)
        throw std::logic_error("Use an external context to draw into a supplied target");
    if (destination) {
        ComPtr<ID3D11Device> owner;
        destination->GetDevice(&owner);
        if (owner.Get() != device.Get())
            throw std::invalid_argument("Target belongs to another device");
    }
    try {
        StateScope scope(context1.Get(), uiState.Get());
        auto output = external ? destination : target.Get();
        if (!external) {
            float clear[] = {theme->background.r / 255.f, theme->background.g / 255.f,
                             theme->background.b / 255.f, 1};
            deviceContext->ClearRenderTargetView(output, clear);
        }
        deviceContext->OMSetRenderTargets(1, &output, nullptr);
        std::stable_sort(vertices.begin(), vertices.end(),
                         [](const Vertex& a, const Vertex& b) { return a.layer < b.layer; });
        if (!vertices.empty()) {
            UINT bytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
            D3D11_BUFFER_DESC desc{};
            if (vertexBuffer)
                vertexBuffer->GetDesc(&desc);
            if (desc.ByteWidth < bytes) {
                vertexBuffer.Reset();
                desc = {};
                desc.ByteWidth = bytes + 65536;
                desc.Usage = D3D11_USAGE_DYNAMIC;
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                check(device->CreateBuffer(&desc, nullptr, &vertexBuffer));
            }
            D3D11_MAPPED_SUBRESOURCE mapped{};
            check(deviceContext->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
            memcpy(mapped.pData, vertices.data(), bytes);
            deviceContext->Unmap(vertexBuffer.Get(), 0);
            D3D11_VIEWPORT viewport{
                0, 0, static_cast<float>(pixelWidth), static_cast<float>(pixelHeight), 0, 1};
            deviceContext->RSSetViewports(1, &viewport);
            deviceContext->RSSetState(raster.Get());
            UINT stride = sizeof(Vertex), offset = 0;
            deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
            deviceContext->IASetInputLayout(layout.Get());
            deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            deviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
            deviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);
            deviceContext->PSSetSamplers(0, 1, sampler.GetAddressOf());
            deviceContext->OMSetBlendState(blend.Get(), nullptr, 0xffffffff);
            deviceContext->OMSetDepthStencilState(depth.Get(), 0);
            for (size_t start = 0; start < vertices.size();) {
                auto texture = vertices[start].texture;
                size_t end = start + 1;
                while (end < vertices.size() && vertices[end].texture == texture)
                    ++end;
                deviceContext->PSSetShaderResources(0, 1, &texture);
                deviceContext->Draw(static_cast<UINT>(end - start), static_cast<UINT>(start));
                ++drawCalls;
                start = end;
            }
        }
        // The saved host state is restored even if Map or Present fails.
        if (!external)
            check(swapChain->Present(vsync ? 1 : 0, 0));
        check(device->GetDeviceRemovedReason());
        frameTextures.clear();
    } catch (const DxError&) {
        frameTextures.clear();
        if (FAILED(device->GetDeviceRemovedReason()))
            deviceLost = true;
        throw;
    }
}
}
