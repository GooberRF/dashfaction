#include <cassert>
#include <cstring>
#include "gr_d3d11.h"
#include "gr_d3d11_context.h"
#include "gr_d3d11_texture.h"
#include "gr_d3d11_state.h"
#include "gr_d3d11_shader.h"

using namespace rf;

namespace df::gr::d3d11
{
    RenderContext::RenderContext(
        ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context,
        StateManager& state_manager, ShaderManager& shader_manager,
        TextureManager& texture_manager
    ) :
        device_{std::move(device)}, context_{std::move(context)},
        state_manager_{state_manager}, shader_manager_{shader_manager}, texture_manager_{texture_manager}
    {
        context_->RSSetState(state_manager_.get_rasterizer_state());
        init_ps_cbuffer();
        init_vs_cbuffer();

        white_bm_ = rf::bm::create(rf::bm::FORMAT_888_RGB, 1, 1);
        assert(white_bm_ != -1);
        rf::gr::LockInfo lock;
        if (rf::gr::lock(white_bm_, 0, &lock, rf::gr::LOCK_WRITE_ONLY)) {
            std::memset(lock.data, 0xFF, lock.stride_in_bytes * lock.h);
            rf::gr::unlock(&lock);
        }
    }

    void RenderContext::init_ps_cbuffer()
    {
        D3D11_BUFFER_DESC buffer_desc;
        ZeroMemory(&buffer_desc, sizeof(buffer_desc));
        buffer_desc.Usage            = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth        = sizeof(PixelShaderUniforms);
        buffer_desc.BindFlags        = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags        = 0;
        buffer_desc.StructureByteStride = 0;

        PixelShaderUniforms data;
        std::memset(&data, 0, sizeof(data));
        D3D11_SUBRESOURCE_DATA subres_data;
        subres_data.pSysMem = &data;
        subres_data.SysMemPitch = 0;
        subres_data.SysMemSlicePitch = 0;

        HRESULT hr = device_->CreateBuffer(&buffer_desc, &subres_data, &ps_cbuffer_);
        check_hr(hr, "CreateBuffer");

        ID3D11Buffer* ps_cbuffers[] = { ps_cbuffer_ };
        context_->PSSetConstantBuffers(0, std::size(ps_cbuffers), ps_cbuffers);
    }

    void RenderContext::init_vs_cbuffer()
    {
        D3D11_BUFFER_DESC buffer_desc;
        ZeroMemory(&buffer_desc, sizeof(buffer_desc));
        buffer_desc.Usage            = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth        = sizeof(CameraUniforms);
        buffer_desc.BindFlags        = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags        = 0;
        buffer_desc.StructureByteStride = 0;

        CameraUniforms data;
        std::memset(&data, 0, sizeof(data));
        data.model_mat = build_identity_matrix();
        data.view_mat = build_identity_matrix();
        data.proj_mat = build_identity_matrix();

        D3D11_SUBRESOURCE_DATA subres_data;
        subres_data.pSysMem = &data;
        subres_data.SysMemPitch = 0;
        subres_data.SysMemSlicePitch = 0;

        HRESULT hr = device_->CreateBuffer(&buffer_desc, &subres_data, &vs_cbuffer_);
        check_hr(hr, "CreateBuffer");

        ZeroMemory(&buffer_desc, sizeof(buffer_desc));
        buffer_desc.Usage            = D3D11_USAGE_DYNAMIC;
        buffer_desc.ByteWidth        = sizeof(TexCoordTransformUniform);
        buffer_desc.BindFlags        = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;
        buffer_desc.MiscFlags        = 0;
        buffer_desc.StructureByteStride = 0;

        TexCoordTransformUniform texture_transform_uniform;
        current_texture_transform_ = build_identity_matrix3();
        texture_transform_uniform.mat = convert_to_4x3_matrix(current_texture_transform_);
        subres_data.pSysMem = &texture_transform_uniform;
        hr = device_->CreateBuffer(&buffer_desc, &subres_data, &texture_transform_cbuffer_);
        check_hr(hr, "CreateBuffer");

        ID3D11Buffer* vs_cbuffers[] = { vs_cbuffer_, texture_transform_cbuffer_ };
        context_->VSSetConstantBuffers(0, std::size(vs_cbuffers), vs_cbuffers);
    }

    void RenderContext::set_mode_and_textures(rf::gr::Mode mode, int tex_handle0, int tex_handle1)
    {
        float vcolor_mul_rgb = 0.0f;
        float vcolor_mul_a = 0.0f;
        float tex0_mul_rgb = 0.0f;
        float tex0_mul_a = 0.0f;
        float tex0_add_rgb = 0.0f;
        float tex1_mul_rgb = 0.0f;
        float tex1_add_rgb = 0.0f;
        float output_add_rgb = 0.0f;

        gr::ColorSource cs = mode.get_color_source();
        gr::AlphaSource as = mode.get_alpha_source();

        switch (mode.get_texture_source()) {
            case gr::TEXTURE_SOURCE_NONE: // no texture, used for rects, etc
                tex_handle0 = -1;
                tex_handle1 = -1;
                vcolor_mul_rgb = 1.0f;
                vcolor_mul_a = 1.0f;
                // gr_d3d_set_texture(0, -1, 0, 0, 0, 0);
                // gr_d3d_set_texture(1, -1, 0, 0, 0, 0);
                // SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                // SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                break;
            case gr::TEXTURE_SOURCE_WRAP: // used by 3D graphics without lightmaps, e.g. skybox, weapon, reticle
                tex_handle1 = -1;
                // if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
                // }
                // else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE_2X) {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                // }
                // else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                // }
                // else {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // }
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // }
                // else if (as == gr::ALPHA_SOURCE_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_TEXTURE
                // }
                // else if (as == gr::ALPHA_SOURCE_VERTEX) {
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2); // D3DTA_DIFFUSE
                // }
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE) {
                    vcolor_mul_rgb = 1.0f;
                    tex0_add_rgb = 1.0f;
                }
                else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE_2X) {
                    vcolor_mul_rgb = 1.0f;
                    tex0_mul_rgb = 2.0f;
                }
                else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_rgb = 1.0f;
                    tex0_mul_rgb = 1.0f;
                }
                else {
                    tex0_mul_rgb = 1.0f;
                }
                if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_a = 1.0f;
                    tex0_mul_a = 1.0f;
                }
                else if (as == gr::ALPHA_SOURCE_TEXTURE) {
                    tex0_mul_a = 1.0f;
                }
                else if (as == gr::ALPHA_SOURCE_VERTEX) {vcolor_mul_a = 1.0f;
                    vcolor_mul_a = 1.0f;
                }
                break;

            case gr::TEXTURE_SOURCE_CLAMP: // decal? used mostly by UI
                tex_handle1 = -1;
                // if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
                // }
                // else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE_2X) {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                // }
                // else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                // }
                // else {
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // }
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_TEXTURE
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE) {
                    vcolor_mul_rgb = 1.0f;
                    tex0_add_rgb = 1.0f;
                }
                else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE_2X) {
                    vcolor_mul_rgb = 1.0f;
                    tex0_mul_rgb = 2.0f;
                }
                else if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_rgb = 1.0f;
                    tex0_mul_rgb = 1.0f;
                }
                else {
                    tex0_mul_rgb = 1.0f;
                }
                if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_a = 1.0f;
                }
                tex0_mul_a = 1.0f;
                break;

            case gr::TEXTURE_SOURCE_CLAMP_NO_FILTERING: // used by text in UI
                tex_handle1 = -1;
                // if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
                // else
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE); //cs = gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE;
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                vcolor_mul_rgb = 1.0f;
                vcolor_mul_a = 1.0f;
                tex0_mul_a = 1.0f;
                if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE) {
                    tex0_add_rgb = 1.0f;
                }
                else {
                    tex0_mul_rgb = 1.0f;
                }
                break;

            case gr::TEXTURE_SOURCE_CLAMP_1_WRAP_0:
                // RF PC handles it as TEXTURE_SOURCE_CLAMP_1_WRAP_0_MOD2X if mod2x is supported (assume it is
                // supported in D3D11)
            case gr::TEXTURE_SOURCE_CLAMP_1_WRAP_0_MOD2X: // used by static geometry
                // if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_TEXTURE
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                // SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                // SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2); // D3DTA_CURRENT
                // SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                // if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                //     vcolor_mul_rgb = 1.0f;
                // }
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                //     vcolor_mul_a = 1.0f;
                // }
                tex0_mul_rgb = 1.0f;
                tex0_mul_a = 1.0f;
                if (tex_handle1 != -1) {
                    tex1_mul_rgb = 2.0f;
                }
                break;

            case gr::TEXTURE_SOURCE_CLAMP_1_CLAMP_0: // used by static geometry
                // SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // }
                // else if (as == gr::ALPHA_SOURCE_VERTEX) {
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2); // D3DTA_DIFFUSE
                // }
                // else if (as == gr::ALPHA_SOURCE_TEXTURE) {
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_TEXTURE
                // }
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE)
                //     SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADD);
                // else
                //     //cs = gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE_2X;???
                //     SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                // SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                // SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2); // D3DTA_CURRENT
                // SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                vcolor_mul_rgb = 0.0f;
                tex0_mul_rgb = 1.0f;
                if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_a = tex0_mul_a = 1.0f;
                }
                else if (as == gr::ALPHA_SOURCE_VERTEX) {
                    vcolor_mul_a = 1.0f;
                }
                else if (as == gr::ALPHA_SOURCE_TEXTURE) {
                    tex0_mul_a = 1.0f;
                }
                if (cs == gr::COLOR_SOURCE_VERTEX_PLUS_TEXTURE) {
                    tex1_add_rgb = 1.0f;
                }
                else {
                    tex1_mul_rgb = 2.0f;
                }
                break;

            case gr::TEXTURE_SOURCE_MT_U_WRAP_V_CLAMP: // used by static geometry, e.g. decals with U tiling
                // if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_TEXTURE
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                // SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                // SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2); // D3DTA_CURRENT
                // SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_rgb = 1.0f;
                }
                if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_a = 1.0f;
                }
                tex0_mul_rgb = 1.0f;
                tex0_mul_a = 1.0f;
                tex1_mul_rgb = 2.0f;
                break;

            case gr::TEXTURE_SOURCE_MT_U_CLAMP_V_WRAP: // used by static geometry, e.g. decals with V tiling
                // if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_TEXTURE
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                // SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                // SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2); // D3DTA_CURRENT
                // SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_rgb = 1.0f;
                }
                if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_a = 1.0f;
                }
                tex0_mul_rgb = 1.0f;
                tex0_mul_a = 1.0f;
                tex1_mul_rgb = 2.0f;
                break;

            case gr::TEXTURE_SOURCE_MT_WRAP_TRILIN:
                // if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE)
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // else
                //     SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_TEXTURE
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADDSIGNED); // D3DTOP_ADDSIGNED?
                // SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                // SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_CURRENT
                // SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                if (cs == gr::COLOR_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_rgb = 1.0f;
                }
                if (as == gr::ALPHA_SOURCE_VERTEX_TIMES_TEXTURE) {
                    vcolor_mul_a = 1.0f;
                }
                tex0_mul_rgb = 1.0f;
                tex0_mul_a = 1.0f;
                tex1_add_rgb = 1.0f;
                output_add_rgb = -0.5f;
                break;

            case gr::TEXTURE_SOURCE_MT_CLAMP_TRILIN:
                // SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                // SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                // SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                // SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                // SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                // SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                // SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1); // D3DTA_CURRENT
                // SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                vcolor_mul_rgb = 1.0f;
                vcolor_mul_a = 1.0f;
                tex0_mul_rgb = 1.0f;
                tex0_mul_a = 1.0f;
                tex1_mul_rgb = 2.0f;
                break;

            default:
                break;
        }

        bool alpha_test = mode.get_zbuffer_type() == gr::ZBUFFER_TYPE_FULL_ALPHA_TEST;

        PixelShaderUniforms ps_data;
        ps_data.vcolor_mul = {vcolor_mul_rgb, vcolor_mul_a};
        ps_data.vcolor_mul_inv = {vcolor_mul_rgb ? 0.0f : 1.0f, vcolor_mul_a ? 0.0f : 1.0f};
        ps_data.tex0_mul = {tex0_mul_rgb, tex0_mul_a};
        ps_data.tex0_mul_inv = {tex0_mul_rgb ? 0.0f : 1.0f, tex0_mul_a ? 0.0f : 1.0f};
        ps_data.tex0_add_rgb = tex0_add_rgb;
        ps_data.tex1_mul_rgb = tex1_mul_rgb;
        ps_data.tex1_mul_rgb_inv = tex1_mul_rgb ? 0.0f : 1.0f;
        ps_data.tex1_add_rgb = tex1_add_rgb;
        ps_data.output_add_rgb = output_add_rgb;
        ps_data.alpha_test = alpha_test ? 0.1f : 0.0f;
        if (mode.get_fog_type() == gr::FOG_NOT_ALLOWED || !gr::screen.fog_mode) {
            ps_data.fog_far = std::numeric_limits<float>::infinity();
            ps_data.fog_color = {0.0f, 0.0f, 0.0f};
        }
        else {
            ps_data.fog_far = gr::screen.fog_far;
            ps_data.fog_color = {
                static_cast<float>(gr::screen.fog_color.red) / 255.0f,
                static_cast<float>(gr::screen.fog_color.green) / 255.0f,
                static_cast<float>(gr::screen.fog_color.blue) / 255.0f,
            };
        }

        D3D11_MAPPED_SUBRESOURCE mapped_cbuffer;
        HRESULT hr = context_->Map(ps_cbuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cbuffer);
        check_hr(hr, "Map");
        std::memcpy(mapped_cbuffer.pData, &ps_data, sizeof(ps_data));
        context_->Unmap(ps_cbuffer_, 0);

        ID3D11SamplerState* sampler_states[] = {
            state_manager_.lookup_sampler_state(mode, 0),
            state_manager_.lookup_sampler_state(mode, 1),
        };
        context_->PSSetSamplers(0, std::size(sampler_states), sampler_states);

        ID3D11BlendState* blend_state = state_manager_.lookup_blend_state(mode);
        context_->OMSetBlendState(blend_state, nullptr, 0xffffffff);

        ID3D11DepthStencilState* depth_stencil_state = state_manager_.lookup_depth_stencil_state(mode);
        context_->OMSetDepthStencilState(depth_stencil_state, 0);

        set_textures(tex_handle0, tex_handle1);
    }

    void RenderContext::update_camera_uniforms(const CameraUniforms& uniforms)
    {
        camera_uniforms_ = uniforms;
        D3D11_MAPPED_SUBRESOURCE mapped_cbuffer;
        HRESULT hr = context_->Map(vs_cbuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cbuffer);
        check_hr(hr, "Map");
        std::memcpy(mapped_cbuffer.pData, &camera_uniforms_, sizeof(camera_uniforms_));
        context_->Unmap(vs_cbuffer_, 0);
    }

    void RenderContext::set_textures(int tex_handle0, int tex_handle1)
    {
        if (tex_handle0 == -1) {
            tex_handle0 = white_bm_;
        }
        if (tex_handle1 == -1) {
            tex_handle1 = white_bm_;
        }
        ID3D11ShaderResourceView* shader_resources[] = {
            texture_manager_.lookup_texture(tex_handle0),
            texture_manager_.lookup_texture(tex_handle1),
        };
        context_->PSSetShaderResources(0, std::size(shader_resources), shader_resources);
    }

    void RenderContext::set_render_target(
        const ComPtr<ID3D11RenderTargetView>& back_buffer_view,
        const ComPtr<ID3D11DepthStencilView>& depth_stencil_buffer_view
    )
    {
        ID3D11RenderTargetView* render_targets[] = { back_buffer_view };
        context_->OMSetRenderTargets(std::size(render_targets), render_targets, depth_stencil_buffer_view);
    }

    void RenderContext::set_texture_transform(const GrMatrix3x3& transform)
    {
        if (current_texture_transform_ == transform) {
            return;
        }
        current_texture_transform_ = transform;
        TexCoordTransformUniform uniforms;
        uniforms.mat = convert_to_4x3_matrix(transform);
        D3D11_MAPPED_SUBRESOURCE mapped_cbuffer;
        HRESULT hr = context_->Map(texture_transform_cbuffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cbuffer);
        check_hr(hr, "Map");
        std::memcpy(mapped_cbuffer.pData, &uniforms, sizeof(uniforms));
        context_->Unmap(texture_transform_cbuffer_, 0);
    }

    void RenderContext::bind_vs_cbuffer(int index, ID3D11Buffer* cbuffer)
    {
        ID3D11Buffer* vs_cbuffers[] = { cbuffer };
        context_->VSSetConstantBuffers(index, std::size(vs_cbuffers), vs_cbuffers);
    }

    void RenderContext::bind_default_shaders()
    {
        shader_manager_.bind_default_shaders(*this);
    }

    void RenderContext::bind_character_shaders()
    {
        shader_manager_.bind_character_shaders(*this);
    }
}
