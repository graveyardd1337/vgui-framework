#include "render.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <d3dcompiler.h>
#include <stdexcept>

namespace vgui::detail {
namespace {
void check(HRESULT result) {
    if (FAILED(result))
        throw std::runtime_error("DirectX operation failed (HRESULT " +
                                 std::to_string(static_cast<unsigned long>(result)) + ")");
}
}
Renderer::Renderer(HWND window, Theme* colors) : theme(colors) {
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.OutputWindow = window;
    swapChainDesc.Windowed = TRUE;
    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               levels, 1, D3D11_SDK_VERSION, &swapChainDesc,
                                               &swapChain, &device, &level, &deviceContext);
    if (FAILED(hr))
        check(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 1,
                                            D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &device,
                                            &level, &deviceContext));
    const char* shader = R"(
struct V { float2 p:POSITION; float2 uv:TEXCOORD; float4 c:COLOR; };
struct P { float4 p:SV_POSITION; float2 uv:TEXCOORD; float4 c:COLOR; };
P vs_main(V v) { P o; o.p=float4(v.p,0,1); o.uv=v.uv; o.c=v.c; return o; }
Texture2D tex:register(t0); SamplerState smp:register(s0);
float4 ps_main(P p):SV_TARGET { return p.c*tex.Sample(smp,p.uv); }
)";
    ComPtr<ID3DBlob> vertexBytecode, pixelBytecode, errors;
    check(D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "vs_main", "vs_4_0", 0, 0,
                     &vertexBytecode, &errors));
    check(D3DCompile(shader, strlen(shader), nullptr, nullptr, nullptr, "ps_main", "ps_4_0", 0, 0,
                     &pixelBytecode, &errors));
    check(device->CreateVertexShader(vertexBytecode->GetBufferPointer(),
                                     vertexBytecode->GetBufferSize(), nullptr, &vertexShader));
    check(device->CreatePixelShader(pixelBytecode->GetBufferPointer(),
                                    pixelBytecode->GetBufferSize(), nullptr, &pixelShader));
    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    check(device->CreateInputLayout(elements, 3, vertexBytecode->GetBufferPointer(),
                                    vertexBytecode->GetBufferSize(), &layout));
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW =
        D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    check(device->CreateSamplerState(&samplerDesc, &sampler));
    D3D11_BLEND_DESC blendDesc{};
    auto& blendTarget = blendDesc.RenderTarget[0];
    blendTarget.BlendEnable = TRUE;
    blendTarget.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendTarget.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendTarget.BlendOp = D3D11_BLEND_OP_ADD;
    blendTarget.SrcBlendAlpha = D3D11_BLEND_ONE;
    blendTarget.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendTarget.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    check(device->CreateBlendState(&blendDesc, &blend));
    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    check(device->CreateRasterizerState(&rasterizerDesc, &raster));
    create_font();
}

void Renderer::create_font() {
    constexpr int atlasWidth = 256, atlasHeight = 128;
    HDC fontDC = CreateCompatibleDC(nullptr);
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = atlasWidth;
    bitmapInfo.bmiHeader.biHeight = -atlasHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(fontDC, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HFONT font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                             DEFAULT_PITCH, L"Tahoma");
    if (!fontDC || !bitmap || !font) {
        if (font)
            DeleteObject(font);
        if (bitmap)
            DeleteObject(bitmap);
        if (fontDC)
            DeleteDC(fontDC);
        throw std::runtime_error("Could not create font atlas");
    }
    auto oldBitmap = SelectObject(fontDC, bitmap);
    auto oldFont = SelectObject(fontDC, font);
    PatBlt(fontDC, 0, 0, atlasWidth, atlasHeight, BLACKNESS);
    SetTextColor(fontDC, RGB(255, 255, 255));
    SetBkMode(fontDC, TRANSPARENT);
    for (int i = 0; i < 95; ++i) {
        wchar_t c = static_cast<wchar_t>(32 + i);
        SIZE s{};
        GetTextExtentPoint32W(fontDC, &c, 1, &s);
        glyphWidths[i] = s.cx;
        TextOutW(fontDC, (i % 16) * 16, (i / 16) * 20, &c, 1);
    }
    GdiFlush();
    std::vector<unsigned char> rgba(atlasWidth * atlasHeight * 4);
    auto* sourcePixels = static_cast<unsigned char*>(pixels);
    for (int i = 0; i < atlasWidth * atlasHeight; ++i) {
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = sourcePixels[i * 4];
    }
    // Reuse a white texel for rectangles, so text and shapes can share one draw call.
    rgba[(atlasWidth * atlasHeight - 1) * 4 + 3] = 255;
    SelectObject(fontDC, oldFont);
    SelectObject(fontDC, oldBitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(fontDC);
    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = atlasWidth;
    textureDesc.Height = atlasHeight;
    textureDesc.MipLevels = textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{rgba.data(), atlasWidth * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    check(device->CreateTexture2D(&textureDesc, &data, &texture));
    check(device->CreateShaderResourceView(texture.Get(), nullptr, &fontAtlas));
}

void Renderer::quad(Rect r, Color c, float u0, float v0, float u1, float v1) {
    float left = std::max(r.x, clip.x), top = std::max(r.y, clip.y);
    float right = std::min(r.x + r.w, clip.x + clip.w),
          bottom = std::min(r.y + r.h, clip.y + clip.h);
    if (right <= left || bottom <= top)
        return;
    float uLeft = u0 + (u1 - u0) * (left - r.x) / r.w;
    float uRight = u0 + (u1 - u0) * (right - r.x) / r.w;
    float vTop = v0 + (v1 - v0) * (top - r.y) / r.h;
    float vBottom = v0 + (v1 - v0) * (bottom - r.y) / r.h;
    auto v = [&](float x, float y, float u, float vv) {
        return Vertex{2 * x / width - 1, 1 - 2 * y / height, u,           vv,
                      c.r / 255.f,       c.g / 255.f,        c.b / 255.f, c.a / 255.f};
    };
    Vertex a = v(left, top, uLeft, vTop), b = v(right, top, uRight, vTop),
           d = v(left, bottom, uLeft, vBottom), e = v(right, bottom, uRight, vBottom);
    vertices.insert(vertices.end(), {a, b, e, a, e, d});
}

void Renderer::label(float x, float y, std::string_view s, Color color) {
    for (unsigned char ch : s) {
        int i = (ch >= 32 && ch <= 126 ? ch : '?') - 32;
        float u = static_cast<float>((i % 16) * 16), v = static_cast<float>((i / 16) * 20);
        float glyphWidth = static_cast<float>(std::min(15, glyphWidths[i] + 1));
        quad({x, y, glyphWidth, 20}, color, u / 256, v / 128, (u + glyphWidth) / 256,
             (v + 20) / 128);
        x += glyphWidths[i];
    }
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
    // Font arrows never lined up right. Just draw the pixels.
    float x = std::floor(box.x + (box.w - 7) / 2) + (held ? 1.f : 0.f);
    float y = std::floor(box.y + (box.h - 4) / 2) + (held ? 1.f : 0.f);
    for (int row = 0; row < 4; ++row) {
        int step = upward ? row : 3 - row;
        quad({x + 3 - step, y + row, static_cast<float>(1 + 2 * step), 1}, theme->text);
    }
}

float Renderer::measure(std::string_view s) {
    float result = 0;
    for (unsigned char c : s)
        result += glyphWidths[(c >= 32 && c <= 126 ? c : '?') - 32];
    return result;
}
void Renderer::resize(UINT newWidth, UINT newHeight) {
    if (width != static_cast<UINT>(newWidth) || height != static_cast<UINT>(newHeight)) {
        deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        target.Reset();
        check(swapChain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0));
        ComPtr<ID3D11Texture2D> back;
        check(swapChain->GetBuffer(0, IID_PPV_ARGS(&back)));
        check(device->CreateRenderTargetView(back.Get(), nullptr, &target));
        width = newWidth;
        height = newHeight;
    }
}
void Renderer::present() {
    float clear[] = {theme->background.r / 255.f, theme->background.g / 255.f,
                     theme->background.b / 255.f, 1};
    deviceContext->ClearRenderTargetView(target.Get(), clear);
    deviceContext->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
    if (!vertices.empty()) {
        UINT bytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        D3D11_BUFFER_DESC bufferInfo{};
        if (vertexBuffer)
            vertexBuffer->GetDesc(&bufferInfo);
        if (bufferInfo.ByteWidth < bytes) {
            vertexBuffer.Reset();
            D3D11_BUFFER_DESC bufferDesc{};
            bufferDesc.ByteWidth = bytes + 65536;
            bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
            bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            check(device->CreateBuffer(&bufferDesc, nullptr, &vertexBuffer));
        }
        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        check(
            deviceContext->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
        memcpy(mappedResource.pData, vertices.data(), bytes);
        deviceContext->Unmap(vertexBuffer.Get(), 0);
        D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        deviceContext->RSSetViewports(1, &viewport);
        deviceContext->RSSetState(raster.Get());
        UINT stride = sizeof(Vertex), offset = 0;
        deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
        deviceContext->IASetInputLayout(layout.Get());
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        deviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
        deviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);
        deviceContext->PSSetShaderResources(0, 1, fontAtlas.GetAddressOf());
        deviceContext->PSSetSamplers(0, 1, sampler.GetAddressOf());
        deviceContext->OMSetBlendState(blend.Get(), nullptr, 0xffffffff);
        deviceContext->Draw(static_cast<UINT>(vertices.size()), 0);
    }
    check(swapChain->Present(vsync ? 1 : 0, 0));
}
}
