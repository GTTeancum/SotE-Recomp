#include "menu_renderer.hpp"
#include "menu_skin.hpp"
#include "san_movies.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "rhi/rt64_render_hooks.h"
#include "plume_render_interface_builders.h"
#include "shaders/FullScreenVS.hlsl.spirv.h"
#include "shaders/TextureCopyPS.hlsl.spirv.h"
#ifdef _WIN32
#include "shaders/FullScreenVS.hlsl.dxil.h"
#include "shaders/TextureCopyPS.hlsl.dxil.h"
#endif

namespace sote::menu_renderer {
namespace {
using namespace plume;
struct TextureSet : RenderDescriptorSetBase {
    uint32_t input = 0;
    explicit TextureSet(RenderDevice* device) {
        builder.begin(); input = builder.addTexture(1); builder.end(); create(device);
    }
};
// Matches shared/rt64_texture_copy.h without introducing HLSL macros into the host.
struct CopyConstants { float scroll_x, scroll_y, scale_x, scale_y; };
static_assert(sizeof(CopyConstants) == 16);
struct Resources {
    RenderDevice* device = nullptr;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::unique_ptr<RenderShader> vertex, pixel;
    std::unique_ptr<RenderPipeline> pipeline;
    std::unique_ptr<TextureSet> descriptor;
    std::unique_ptr<RenderTexture> texture;
    std::unique_ptr<RenderBuffer> upload;
    unsigned width = 0, height = 0, pitch = 0;
    uint64_t serial = 0;
    bool ready = false;
};
std::unique_ptr<Resources> resources;
RT64::RenderHookInit* previous_init = nullptr;
RT64::RenderHookDraw* previous_draw = nullptr;
RT64::RenderHookDeinit* previous_deinit = nullptr;

menu_skin::Image letterbox_movie(
    const san_movies::CachedFrame& frame,
    unsigned width,
    unsigned height) {
    menu_skin::Image image;
    image.width = width;
    image.height = height;
    image.rgba.assign(static_cast<size_t>(width) * height * 4, 255);
    for (size_t index = 0; index < image.rgba.size(); index += 4) {
        image.rgba[index + 0] = 0;
        image.rgba[index + 1] = 0;
        image.rgba[index + 2] = 0;
        image.rgba[index + 3] = 255;
    }
    if (!frame.valid()) {
        return image;
    }
    const double scale = std::min(
        static_cast<double>(width) / frame.width,
        static_cast<double>(height) / frame.height);
    const unsigned draw_width = std::max(
        1U,
        static_cast<unsigned>(std::lround(frame.width * scale)));
    const unsigned draw_height = std::max(
        1U,
        static_cast<unsigned>(std::lround(frame.height * scale)));
    const unsigned left = (width - draw_width) / 2;
    const unsigned top = (height - draw_height) / 2;
    for (unsigned y = 0; y < draw_height; ++y) {
        const unsigned sy = std::min(
            frame.height - 1,
            static_cast<unsigned>(
                static_cast<uint64_t>(y) * frame.height / draw_height));
        for (unsigned x = 0; x < draw_width; ++x) {
            const unsigned sx = std::min(
                frame.width - 1,
                static_cast<unsigned>(
                    static_cast<uint64_t>(x) * frame.width / draw_width));
            const size_t src = (static_cast<size_t>(sy) * frame.width + sx) * 4;
            const size_t dst =
                (static_cast<size_t>(top + y) * width + (left + x)) * 4;
            image.rgba[dst + 0] = frame.rgba[src + 0];
            image.rgba[dst + 1] = frame.rgba[src + 1];
            image.rgba[dst + 2] = frame.rgba[src + 2];
            image.rgba[dst + 3] = 255;
        }
    }
    return image;
}

void disable(const char* reason) {
    menu_skin::set_renderer_available(false);
    if (resources) resources->ready = false;
    std::fprintf(stderr, "[sote][menu] Presentation disabled (%s); native menus remain available.\n", reason);
}
void initialize(RenderInterface* rhi, RenderDevice* device) {
    if (previous_init) previous_init(rhi, device);
    try {
        resources = std::make_unique<Resources>();
        auto& r = *resources; r.device = device;
        r.descriptor = std::make_unique<TextureSet>(device);
        RenderPipelineLayoutBuilder builder;
        builder.begin();
        builder.addPushConstant(0, 0, sizeof(CopyConstants), RenderShaderStageFlag::PIXEL);
        builder.addDescriptorSet(*r.descriptor);
        builder.end();
        r.layout = builder.create(device);
        const auto format = rhi->getCapabilities().shaderFormat;
        if (format == RenderShaderFormat::SPIRV) {
            r.vertex = device->createShader(FullScreenVSBlobSPIRV, sizeof(FullScreenVSBlobSPIRV), "VSMain", format);
            r.pixel = device->createShader(TextureCopyPSBlobSPIRV, sizeof(TextureCopyPSBlobSPIRV), "PSMain", format);
        }
#ifdef _WIN32
        else if (format == RenderShaderFormat::DXIL) {
            r.vertex = device->createShader(FullScreenVSBlobDXIL, sizeof(FullScreenVSBlobDXIL), "VSMain", format);
            r.pixel = device->createShader(TextureCopyPSBlobDXIL, sizeof(TextureCopyPSBlobDXIL), "PSMain", format);
        }
#endif
        else throw std::runtime_error("unsupported shader format");
        if (!r.layout || !r.vertex || !r.pixel || !r.descriptor->get()) throw std::runtime_error("shader/descriptor allocation");
        RenderGraphicsPipelineDesc desc;
        desc.pipelineLayout = r.layout.get();
        desc.vertexShader = r.vertex.get(); desc.pixelShader = r.pixel.get();
        desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        // Application::setup fixes the swap chain to this format in this build.
        desc.renderTargetFormat[0] = RenderFormat::B8G8R8A8_UNORM;
        desc.renderTargetCount = 1;
        r.pipeline = device->createGraphicsPipeline(desc);
        if (!r.pipeline) throw std::runtime_error("menu pipeline allocation");
        r.ready = true; menu_skin::set_renderer_available(true);
        std::printf("[sote][menu] RT64 menu presentation ready.\n");
    } catch (const std::exception& e) { disable(e.what()); }
      catch (...) { disable("initialization failure"); }
}
void draw(RenderCommandList* list, RenderFramebuffer* framebuffer) {
    if (previous_draw) previous_draw(list, framebuffer);
    if (!resources || !resources->ready || !framebuffer) return;
    try {
        const auto movie_frame = san_movies::latest_cached_frame();
        const auto snapshot = menu_skin::latest();
        if (!movie_frame.valid() && !snapshot) return;
        auto& r = *resources;
        const unsigned fw = framebuffer->getWidth(), fh = framebuffer->getHeight();
        if (!fw || !fh) return;
        // Preserve window aspect, but cap CPU work at a 1920x1080 surface.
        const double scale = std::min({1.0, 1920.0 / fw, 1080.0 / fh});
        const unsigned w = std::max(1U, unsigned(std::lround(fw * scale)));
        const unsigned h = std::max(1U, unsigned(std::lround(fh * scale)));
        if (w < 320 || h < 180) return;
        const uint64_t serial = movie_frame.valid()
            ? (UINT64_C(1) << 63) | movie_frame.serial
            : snapshot->serial;
        const bool resized = r.width != w || r.height != h;
        if (resized || r.serial != serial || !r.texture) {
            auto image = movie_frame.valid()
                ? letterbox_movie(movie_frame, w, h)
                : menu_skin::render(*snapshot, w, h);
            if (!image.valid()) return;
            if (resized || !r.texture) {
                // RT64's present queue waits for the previous present worker
                // before its next draw hook. Old resources are no longer in use.
                r.pitch = (w * 4 + 255U) & ~255U;
                auto texture = r.device->createTexture(RenderTextureDesc::Texture2D(w, h, 1, RenderFormat::R8G8B8A8_UNORM));
                auto upload = r.device->createBuffer(RenderBufferDesc::UploadBuffer(uint64_t(r.pitch) * h));
                if (!texture || !upload) throw std::runtime_error("menu surface allocation");
                r.texture = std::move(texture); r.upload = std::move(upload);
                r.width = w; r.height = h;
                r.descriptor->setTexture(r.descriptor->input, r.texture.get(), RenderTextureLayout::SHADER_READ);
            }
            auto* mapped = static_cast<uint8_t*>(r.upload->map());
            if (!mapped) throw std::runtime_error("menu surface mapping");
            for (unsigned y = 0; y < h; ++y) std::memcpy(mapped + size_t(y) * r.pitch, image.rgba.data() + size_t(y) * w * 4, size_t(w) * 4);
            r.upload->unmap();
            list->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(r.texture.get(), RenderTextureLayout::COPY_DEST));
            list->copyTextureRegion(RenderTextureCopyLocation::Subresource(r.texture.get()),
                RenderTextureCopyLocation::PlacedFootprint(r.upload.get(), RenderFormat::R8G8B8A8_UNORM, w, h, 1, r.pitch / 4));
            list->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(r.texture.get(), RenderTextureLayout::SHADER_READ));
            r.serial = serial;
        }
        list->setFramebuffer(framebuffer);
        list->setGraphicsPipelineLayout(r.layout.get());
        list->setPipeline(r.pipeline.get());
        list->setGraphicsDescriptorSet(r.descriptor->get(), 0);
        const CopyConstants cb{0, 0, float(w), float(h)};
        list->setGraphicsPushConstants(0, &cb);
        list->setViewports(RenderViewport(0, 0, float(fw), float(fh)));
        list->setScissors(RenderRect(0, 0, int32_t(fw), int32_t(fh)));
        list->setVertexBuffers(0, nullptr, 0, nullptr);
        list->drawInstanced(3, 1, 0, 0);
    } catch (const std::exception& e) { disable(e.what()); }
      catch (...) { disable("draw failure"); }
}
void deinitialize() {
    menu_skin::set_renderer_available(false);
    resources.reset();
    if (previous_deinit) previous_deinit();
}
}
void install() {
    if (RT64::GetRenderHookInit() == initialize) return;
    previous_init = RT64::GetRenderHookInit();
    previous_draw = RT64::GetRenderHookDraw();
    previous_deinit = RT64::GetRenderHookDeinit();
    RT64::SetRenderHooks(initialize, draw, deinitialize);
}
}
