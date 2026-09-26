#pragma once
#include "vgui.hpp"
#include <array>
#include <d3d11.h>
#include <wrl/client.h>

namespace vgui::detail {
using Microsoft::WRL::ComPtr;
struct Vertex {
    float x, y, u, v, r, g, b, a;
};
// Own device and swap chain for now.
class Renderer {
public:
    Renderer(HWND window, Theme* theme);
    void resize(UINT newWidth, UINT newHeight);
    void present();
    void quad(Rect bounds, Color color, float u0 = 255.5f / 256, float v0 = 127.5f / 128,
              float u1 = 255.5f / 256, float v1 = 127.5f / 128);
    void label(float x, float y, std::string_view text, Color color);
    void bevel(Rect bounds, bool sunken = false);
    void checkmark(Rect bounds);
    void arrow_icon(Rect bounds, bool upward, bool held = false);
    float measure(std::string_view text);
    Rect clip{};
    bool vsync = true;
    std::vector<Vertex> vertices, overlay;
    UINT width = 0, height = 0;

private:
    void create_font();
    Theme* theme;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> deviceContext;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11ShaderResourceView> fontAtlas;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11RasterizerState> raster;
    std::array<int, 95> glyphWidths{};
};
}
