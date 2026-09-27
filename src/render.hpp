#pragma once
#include "vgui.hpp"
#include <d3d11_1.h>
#include <stdexcept>
#include <unordered_map>
#include <wrl/client.h>

namespace vgui::detail {
using Microsoft::WRL::ComPtr;
struct DxError : std::runtime_error {
    HRESULT code;
    explicit DxError(HRESULT value)
        : std::runtime_error("DirectX failed: " +
                             std::to_string(static_cast<unsigned long>(value))),
          code(value) {}
};
struct Vertex {
    float x, y, u, v, r, g, b, a;
    ID3D11ShaderResourceView* texture;
    int layer;
};
class Renderer {
public:
    Renderer(HWND window, Theme* theme, ID3D11Device* device = nullptr,
             ID3D11DeviceContext* context = nullptr);
    void resize(UINT width, UINT height);
    void set_scale(float value);
    void reset(ID3D11Device* device = nullptr, ID3D11DeviceContext* context = nullptr);
    bool ready();
    bool lost() const;
    ID3D11Device* get_device() const {
        return device.Get();
    }
    void present(ID3D11RenderTargetView* destination = nullptr);
    void clear_frame();
    void quad(Rect bounds, Color color, float u0 = 2047.5f / 2048, float v0 = 2047.5f / 2048,
              float u1 = 2047.5f / 2048, float v1 = 2047.5f / 2048,
              ID3D11ShaderResourceView* texture = nullptr);
    void image(Rect bounds, ID3D11ShaderResourceView* texture, Rect uv, Color tint);
    void label(float x, float y, std::string_view text, Color color);
    void bevel(Rect bounds, bool sunken = false);
    void checkmark(Rect bounds);
    void arrow_icon(Rect bounds, bool upward, bool held = false);
    float measure(std::string_view text);
    Rect clip{};
    bool vsync = true, external = false;
    std::vector<Vertex> vertices, overlay;
    float width = 0, height = 0, scale = 1;
    UINT pixelWidth = 0, pixelHeight = 0;
    int layer = 0;
    size_t drawCalls = 0;

private:
    struct Glyph {
        Rect uv;
        float width, height, advance;
        ID3D11ShaderResourceView* texture;
    };
    struct FontPage {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> view;
        int x = 0, y = 0, rowHeight = 0;
    };
    void create_device();
    void create_resources();
    void create_font();
    void add_font_page();
    Glyph& glyph(uint32_t codepoint);
    HWND hwnd;
    Theme* theme;
    bool deviceLost = false;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> deviceContext;
    ComPtr<ID3D11DeviceContext1> context1;
    ComPtr<ID3DDeviceContextState> uiState;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11DepthStencilState> depth;
    std::unordered_map<uint32_t, Glyph> glyphs;
    std::vector<FontPage> pages;
    std::vector<ComPtr<ID3D11ShaderResourceView>> frameTextures;
};
}
