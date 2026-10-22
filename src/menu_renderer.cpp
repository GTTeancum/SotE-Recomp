#include "menu_renderer.hpp"
#include "diagnostic_progress.hpp"
#include "menu_skin.hpp"
#include "san_movies.hpp"
#include "modern_controls.hpp"
#include "control_bindings.hpp"

#include <algorithm>
#include <atomic>
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
    std::unique_ptr<RenderPipeline> alpha_pipeline;
    std::unique_ptr<TextureSet> descriptor;
    std::unique_ptr<RenderTexture> texture;
    std::unique_ptr<RenderBuffer> upload;
    unsigned width = 0, height = 0, pitch = 0;
    uint64_t serial = 0, native_serial = 0, font_serial = 0;
    std::unique_ptr<RenderTexture> scene;
    std::unique_ptr<TextureSet> scene_descriptor;
    unsigned scene_width = 0, scene_height = 0;
    bool scene_valid = false;
    bool ready = false;
};
std::unique_ptr<Resources> resources;
std::atomic<bool> game_ready{false};
RT64::RenderHookInit* previous_init = nullptr;
RT64::RenderHookDraw* previous_draw = nullptr;
RT64::RenderHookDeinit* previous_deinit = nullptr;

menu_skin::Image letterbox_movie(
    const san_movies::MovieFrame& frame,
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

menu_skin::Image centered_aim_marker(unsigned width, unsigned height) {
    menu_skin::Image image{width, height,
        std::vector<uint8_t>(size_t(width) * height * 4)};
    const int cx = int(width / 2), cy = int(height / 2);
    const float scale = std::max(0.75f, float(height) / 720.0f);
    auto line = [&](int x0, int y0, int x1, int y1, uint8_t alpha) {
        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
            if (x < 0 || y < 0 || x >= int(width) || y >= int(height)) continue;
            const size_t p = (size_t(y) * width + unsigned(x)) * 4;
            image.rgba[p] = 111; image.rgba[p + 1] = 233;
            image.rgba[p + 2] = 255; image.rgba[p + 3] = alpha;
        }
    };
    const int inner = std::max(5, int(std::lround(8 * scale)));
    const int outer = std::max(inner + 3, int(std::lround(15 * scale)));
    const int thickness = std::max(1, int(std::lround(scale)));
    line(cx - thickness, cy - thickness, cx + thickness, cy + thickness, 220);
    line(cx - thickness, cy - outer, cx + thickness, cy - inner, 235);
    line(cx - thickness, cy + inner, cx + thickness, cy + outer, 235);
    line(cx - outer, cy - thickness, cx - inner, cy + thickness, 235);
    line(cx + inner, cy - thickness, cx + outer, cy + thickness, 235);
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
        desc.renderTargetBlend[0] = RenderBlendDesc::AlphaBlend();
        r.alpha_pipeline = device->createGraphicsPipeline(desc);
        if (!r.pipeline || !r.alpha_pipeline) throw std::runtime_error("menu pipeline allocation");
        r.ready = true; menu_skin::set_renderer_available(true);
        std::printf("[sote][menu] RT64 menu presentation ready.\n");
    } catch (const std::exception& e) { disable(e.what()); }
      catch (...) { disable("initialization failure"); }
}
void draw(RenderCommandList* list, RenderFramebuffer* framebuffer, RenderTexture* output) {
    sote::diagnostics::Phase phase(sote::diagnostics::present_phase);
    ++sote::diagnostics::presents;
    if (previous_draw) previous_draw(list, framebuffer, output);
    if (!resources || !resources->ready || !framebuffer) return;
    try {
        if (!game_ready.load(std::memory_order_acquire)) {
            list->setFramebuffer(framebuffer);
            list->clearColor(0, RenderColor(0.0f, 0.0f, 0.0f, 1.0f));
            return;
        }
        const auto movie_frame = san_movies::latest_frame();
        phase.set(2);
        const auto snapshot = menu_skin::latest();
        const auto native = movie_frame.valid() ? nullptr : menu_skin::latest_native_text();
        const bool reticle = !movie_frame.valid() && !snapshot &&
            modern_controls::reticle_visible();

        auto& r = *resources;
        const unsigned fw = framebuffer->getWidth(), fh = framebuffer->getHeight();
        if (!fw || !fh) return;
        const bool paused = snapshot && (snapshot->pause_background || snapshot->screen == menu_skin::Screen::Pause);
        bool gameplay = modern_controls::on_foot_active();
        for (int i = 0; i < int(control_bindings::Context::Count); ++i)
            gameplay |= control_bindings::context_active(static_cast<control_bindings::Context>(i));
        // The retail pause renderer alternates clears and partial display lists.
        // Retain the last complete gameplay presentation, before host UI, and
        // blend the translucent pause surface over that stable image.
        if (!movie_frame.valid() && !snapshot && gameplay && output) {
            if (!r.scene || r.scene_width != fw || r.scene_height != fh) {
                r.scene = r.device->createTexture(RenderTextureDesc::Texture2D(fw, fh, 1, RenderFormat::B8G8R8A8_UNORM));
                r.scene_descriptor = std::make_unique<TextureSet>(r.device);
                if (!r.scene) throw std::runtime_error("pause scene allocation");
                r.scene_descriptor->setTexture(r.scene_descriptor->input, r.scene.get(), RenderTextureLayout::SHADER_READ);
                r.scene_width = fw; r.scene_height = fh;
            }
            list->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(output, RenderTextureLayout::COPY_SOURCE));
            list->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(r.scene.get(), RenderTextureLayout::COPY_DEST));
            list->copyTexture(r.scene.get(), output);
            list->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(output, RenderTextureLayout::COLOR_WRITE));
            list->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(r.scene.get(), RenderTextureLayout::SHADER_READ));
            r.scene_valid = true;
        } else if (movie_frame.valid() || (snapshot && (snapshot->screen == menu_skin::Screen::Profiles || snapshot->screen == menu_skin::Screen::Summary))) {
            r.scene_valid = false;
        }
        if (paused && r.scene_valid) {
            list->setFramebuffer(framebuffer);
            list->setGraphicsPipelineLayout(r.layout.get());
            list->setPipeline(r.pipeline.get());
            list->setGraphicsDescriptorSet(r.scene_descriptor->get(), 0);
            const CopyConstants scene_cb{0, 0, float(r.scene_width), float(r.scene_height)};
            list->setGraphicsPushConstants(0, &scene_cb);
            list->setViewports(RenderViewport(0, 0, float(fw), float(fh)));
            list->setScissors(RenderRect(0, 0, int32_t(fw), int32_t(fh)));
            list->setVertexBuffers(0, nullptr, 0, nullptr);
            list->drawInstanced(3, 1, 0, 0);
        }
        if (!movie_frame.valid() && !snapshot && !reticle && !native) return;
        // Preserve window aspect, but cap CPU work at a 1920x1080 surface.
        const double scale = std::min({1.0, 1920.0 / fw, 1080.0 / fh});
        const unsigned w = std::max(1U, unsigned(std::lround(fw * scale)));
        const unsigned h = std::max(1U, unsigned(std::lround(fh * scale)));
        if (w < 320 || h < 180) return;
        const uint64_t serial = movie_frame.valid()
            ? (UINT64_C(1) << 63) | movie_frame.serial
            : reticle ? UINT64_MAX : snapshot ? snapshot->serial : 0;
        const bool resized = r.width != w || r.height != h;
        if (resized || r.serial != serial || r.native_serial != (native ? native->serial : 0) || r.font_serial != menu_skin::font_revision() || !r.texture) {
            phase.set(3);
            auto image = movie_frame.valid()
                ? letterbox_movie(movie_frame, w, h)
                : reticle ? centered_aim_marker(w, h)
                : snapshot ? menu_skin::render(*snapshot, w, h)
                : menu_skin::Image{w,h,std::vector<uint8_t>(size_t(w)*h*4)};
            if (native && (!snapshot || snapshot->screen == menu_skin::Screen::Pause)) menu_skin::composite_native_text(image,*native);
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
            phase.set(4);
            auto* mapped = static_cast<uint8_t*>(r.upload->map());
            if (!mapped) throw std::runtime_error("menu surface mapping");
            for (unsigned y = 0; y < h; ++y) std::memcpy(mapped + size_t(y) * r.pitch, image.rgba.data() + size_t(y) * w * 4, size_t(w) * 4);
            r.upload->unmap();
            list->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(r.texture.get(), RenderTextureLayout::COPY_DEST));
            list->copyTextureRegion(RenderTextureCopyLocation::Subresource(r.texture.get()),
                RenderTextureCopyLocation::PlacedFootprint(r.upload.get(), RenderFormat::R8G8B8A8_UNORM, w, h, 1, r.pitch / 4));
            list->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(r.texture.get(), RenderTextureLayout::SHADER_READ));
            r.serial = serial;
            r.native_serial = native ? native->serial : 0;
            r.font_serial = menu_skin::font_revision();
        }
        list->setFramebuffer(framebuffer);
        list->setGraphicsPipelineLayout(r.layout.get());
        list->setPipeline((reticle || (!snapshot && native) || paused) && !movie_frame.valid()
            ? r.alpha_pipeline.get() : r.pipeline.get());
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
void game_display_list_ready() { game_ready.store(true, std::memory_order_release); }
void install() {
    if (RT64::GetRenderHookInit() == initialize) return;
    previous_init = RT64::GetRenderHookInit();
    previous_draw = RT64::GetRenderHookDraw();
    previous_deinit = RT64::GetRenderHookDeinit();
    RT64::SetRenderHooks(initialize, draw, deinitialize);
}
}
