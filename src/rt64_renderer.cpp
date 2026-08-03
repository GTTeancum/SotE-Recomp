#define HLSL_CPU

#include "rt64_renderer.hpp"
#include "graphics_menu.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>

#include <unknwn.h>
#include <objidl.h>
#include <oleauto.h>

#include "gbi/rt64_gbi_f3d.h"
#include "gbi/rt64_gbi_f3dwave.h"
#include "gbi/rt64_gbi_rdp.h"
#include "hle/rt64_application.h"
#include "hle/rt64_state.h"

#include "ultramodern/config.hpp"
#include "ultramodern/ultramodern.hpp"

extern std::atomic<int> display_list_count;

namespace {

// RT64's HLE core interface includes storage for hardware that the modern
// runtime already schedules. These registers only provide the backing expected
// by RT64; live VI state is bridged from ultramodern below.
uint8_t dmem[0x1000]{};
uint8_t imem[0x1000]{};

uint32_t mi_intr = 0;
uint32_t dpc_start = 0;
uint32_t dpc_end = 0;
uint32_t dpc_current = 0;
uint32_t dpc_status = 0;
uint32_t dpc_clock = 0;
uint32_t dpc_bufbusy = 0;
uint32_t dpc_pipebusy = 0;
uint32_t dpc_tmem = 0;

void check_interrupts() {}

ultramodern::renderer::SetupResult map_setup_result(
    RT64::Application::SetupResult result) {
    using SetupResult = ultramodern::renderer::SetupResult;
    switch (result) {
        case RT64::Application::SetupResult::Success:
            return SetupResult::Success;
        case RT64::Application::SetupResult::DynamicLibrariesNotFound:
            return SetupResult::DynamicLibrariesNotFound;
        case RT64::Application::SetupResult::InvalidGraphicsAPI:
            return SetupResult::InvalidGraphicsAPI;
        case RT64::Application::SetupResult::GraphicsAPINotFound:
            return SetupResult::GraphicsAPINotFound;
        case RT64::Application::SetupResult::GraphicsDeviceNotFound:
            return SetupResult::GraphicsDeviceNotFound;
    }
    return SetupResult::GraphicsDeviceNotFound;
}

ultramodern::renderer::GraphicsApi map_graphics_api(
    RT64::UserConfiguration::GraphicsAPI api) {
    using GraphicsApi = ultramodern::renderer::GraphicsApi;
    switch (api) {
        case RT64::UserConfiguration::GraphicsAPI::D3D12:
            return GraphicsApi::D3D12;
        case RT64::UserConfiguration::GraphicsAPI::Vulkan:
            return GraphicsApi::Vulkan;
        case RT64::UserConfiguration::GraphicsAPI::Metal:
            return GraphicsApi::Metal;
        case RT64::UserConfiguration::GraphicsAPI::Automatic:
        default:
            return GraphicsApi::Auto;
    }
}

class RT64Renderer final : public ultramodern::renderer::RendererContext {
public:
    RT64Renderer(
        uint8_t* rdram,
        ultramodern::renderer::WindowHandle window_handle,
        bool developer_mode,
        const std::filesystem::path& data_directory) {
        static uint8_t dummy_rom_header[0x40]{};

        RT64::Application::Core core{};
        core.window = window_handle.window;
        core.checkInterrupts = check_interrupts;
        core.HEADER = dummy_rom_header;
        core.RDRAM = rdram;
        core.DMEM = dmem;
        core.IMEM = imem;
        core.MI_INTR_REG = &mi_intr;
        core.DPC_START_REG = &dpc_start;
        core.DPC_END_REG = &dpc_end;
        core.DPC_CURRENT_REG = &dpc_current;
        core.DPC_STATUS_REG = &dpc_status;
        core.DPC_CLOCK_REG = &dpc_clock;
        core.DPC_BUFBUSY_REG = &dpc_bufbusy;
        core.DPC_PIPEBUSY_REG = &dpc_pipebusy;
        core.DPC_TMEM_REG = &dpc_tmem;

        auto* vi = ultramodern::renderer::get_vi_regs();
        core.VI_STATUS_REG = &vi->VI_STATUS_REG;
        core.VI_ORIGIN_REG = &vi->VI_ORIGIN_REG;
        core.VI_WIDTH_REG = &vi->VI_WIDTH_REG;
        core.VI_INTR_REG = &vi->VI_INTR_REG;
        core.VI_V_CURRENT_LINE_REG = &vi->VI_V_CURRENT_LINE_REG;
        core.VI_TIMING_REG = &vi->VI_TIMING_REG;
        core.VI_V_SYNC_REG = &vi->VI_V_SYNC_REG;
        core.VI_H_SYNC_REG = &vi->VI_H_SYNC_REG;
        core.VI_LEAP_REG = &vi->VI_LEAP_REG;
        core.VI_H_START_REG = &vi->VI_H_START_REG;
        core.VI_V_START_REG = &vi->VI_V_START_REG;
        core.VI_V_BURST_REG = &vi->VI_V_BURST_REG;
        core.VI_X_SCALE_REG = &vi->VI_X_SCALE_REG;
        core.VI_Y_SCALE_REG = &vi->VI_Y_SCALE_REG;

        RT64::ApplicationConfiguration config{};
        config.appId = "sote-recomp";
        config.dataPath = data_directory;
        config.detectDataPath = false;
        config.useConfigurationFile =
            std::getenv("SOTE_DIAGNOSTIC_LEGACY_GRAPHICS") == nullptr;

        app = std::make_unique<RT64::Application>(core, config);
        app->userConfig.graphicsAPI =
            RT64::UserConfiguration::GraphicsAPI::Automatic;
        app->userConfig.resolution =
            RT64::UserConfiguration::Resolution::WindowIntegerScale;
        app->userConfig.resolutionMultiplier = 4;
        app->userConfig.downsampleMultiplier = 1;
        app->userConfig.aspectRatio =
            RT64::UserConfiguration::AspectRatio::Expand;
        app->userConfig.extAspectRatio =
            RT64::UserConfiguration::AspectRatio::Expand;
        app->userConfig.antialiasing =
            RT64::UserConfiguration::Antialiasing::MSAA4X;
        app->userConfig.refreshRate =
            RT64::UserConfiguration::RefreshRate::Original;
        app->userConfig.internalColorFormat =
            RT64::UserConfiguration::InternalColorFormat::Automatic;
        app->userConfig.displayBuffering =
            RT64::UserConfiguration::DisplayBuffering::Triple;
        app->userConfig.developerMode = developer_mode;
        if (std::getenv("SOTE_DIAGNOSTIC_LEGACY_GRAPHICS") != nullptr) {
            app->userConfig.aspectRatio =
                RT64::UserConfiguration::AspectRatio::Original;
            app->userConfig.extAspectRatio =
                RT64::UserConfiguration::AspectRatio::Original;
            app->userConfig.antialiasing =
                RT64::UserConfiguration::Antialiasing::None;
        }

        setup_result = map_setup_result(app->setup(window_handle.thread_id));
        chosen_api = map_graphics_api(app->chosenGraphicsAPI);
        if (setup_result != ultramodern::renderer::SetupResult::Success) {
            std::fprintf(
                stderr,
                "[sote] RT64 setup failed: result=%d api=%d\n",
                static_cast<int>(setup_result),
                static_cast<int>(chosen_api));
            app.reset();
            return;
        }

        // Match Zelda64Recomp's conservative defaults for F3DEX-family games.
        app->enhancementConfig.f3dex.forceBranch = true;
        app->enhancementConfig.textureLOD.scale = true;
        sote::graphics_menu::Settings loaded_settings{};
        switch (app->userConfig.resolution) {
            case RT64::UserConfiguration::Resolution::Original:
                loaded_settings.resolution =
                    sote::graphics_menu::ResolutionPreset::Native;
                break;
            case RT64::UserConfiguration::Resolution::Manual:
                if (app->userConfig.resolutionMultiplier >= 7.0) {
                    loaded_settings.resolution =
                        sote::graphics_menu::ResolutionPreset::Scale8x;
                } else if (app->userConfig.resolutionMultiplier >= 3.0) {
                    loaded_settings.resolution =
                        sote::graphics_menu::ResolutionPreset::Scale4x;
                } else {
                    loaded_settings.resolution =
                        sote::graphics_menu::ResolutionPreset::Scale2x;
                }
                break;
            case RT64::UserConfiguration::Resolution::WindowIntegerScale:
            default:
                loaded_settings.resolution =
                    sote::graphics_menu::ResolutionPreset::WindowScale;
                break;
        }
        loaded_settings.widescreen =
            app->userConfig.aspectRatio ==
            RT64::UserConfiguration::AspectRatio::Expand;
        switch (app->userConfig.antialiasing) {
            case RT64::UserConfiguration::Antialiasing::MSAA2X:
                loaded_settings.antialiasing =
                    sote::graphics_menu::AntialiasingPreset::MSAA2x;
                break;
            case RT64::UserConfiguration::Antialiasing::MSAA4X:
                loaded_settings.antialiasing =
                    sote::graphics_menu::AntialiasingPreset::MSAA4x;
                break;
            case RT64::UserConfiguration::Antialiasing::MSAA8X:
                loaded_settings.antialiasing =
                    sote::graphics_menu::AntialiasingPreset::MSAA8x;
                break;
            case RT64::UserConfiguration::Antialiasing::None:
            default:
                loaded_settings.antialiasing =
                    sote::graphics_menu::AntialiasingPreset::Off;
                break;
        }
        sote::graphics_menu::sync_from_renderer(loaded_settings);
        std::printf(
            "[sote] RT64 initialized: api=%d resolution=%d "
            "multiplier=%.2f aspect=%d target=%.4f "
            "ext_aspect=%d msaa=%u config=%s\n",
            static_cast<int>(chosen_api),
            static_cast<int>(app->userConfig.resolution),
            app->userConfig.resolutionMultiplier,
            static_cast<int>(app->userConfig.aspectRatio),
            app->userConfig.aspectTarget,
            static_cast<int>(app->userConfig.extAspectRatio),
            app->userConfig.msaaSampleCount(),
            config.useConfigurationFile
                ? app->userPaths.configurationPath.string().c_str()
                : "diagnostic-legacy");
        std::fflush(stdout);
    }

    bool valid() override {
        return app != nullptr;
    }

    bool update_config(
        const ultramodern::renderer::GraphicsConfig&,
        const ultramodern::renderer::GraphicsConfig&) override {
        return false;
    }

    void enable_instant_present() override {
        // SOTE submits several display lists per VI, including small
        // intermediate lists that do not contain the completed frame.
        // PresentEarly exposes those lists and produces severe alternating
        // black-frame flicker. Keep RT64's default SkipBuffering mode so only
        // the finished VI image reaches the swap chain.
    }

    void send_dl(const OSTask* task) override {
        ++display_list_count;
        const uint32_t ucode = task->t.ucode & 0x00FFFFF8;
        const uint32_t ucode_data = task->t.ucode_data & 0x00FFFFF8;
        const bool ucode_changed =
            ucode != last_ucode || ucode_data != last_ucode_data ||
            task->t.ucode_size != last_ucode_size ||
            task->t.ucode_data_size != last_ucode_data_size;
        if (ucode_changed) {
            std::printf(
                "[sote] graphics task ucode=%08X/%u data=%08X/%u "
                "dl=%08X/%u\n",
                task->t.ucode,
                task->t.ucode_size,
                task->t.ucode_data,
                task->t.ucode_data_size,
                task->t.data_ptr,
                task->t.data_size);
            std::fflush(stdout);
            last_ucode = ucode;
            last_ucode_data = ucode_data;
            last_ucode_size = task->t.ucode_size;
            last_ucode_data_size = task->t.ucode_data_size;
        }
        app->state->rsp->reset();
        app->interpreter->loadUCodeGBI(ucode, ucode_data, true);
        if (ucode_changed && app->interpreter->hleGBI != nullptr) {
            std::printf(
                "[sote] RT64 selected GBI=%u from microcode hashes\n",
                static_cast<unsigned>(
                    app->interpreter->hleGBI->ucode));
            std::fflush(stdout);
        }
        if (app->interpreter->hleGBI == nullptr) {
            // The supported v1.2 microcode hashes are registered with RT64.
            // Retain a project-local fallback for dynamically copied or
            // otherwise unrecognized instances of the same F3DBETA dialect.
            auto& gbi = app->interpreter->gbiManager.gbiCache[
                static_cast<uint32_t>(RT64::GBIUCode::F3DWAVE)];
            if (gbi.ucode == RT64::GBIUCode::Unknown) {
                gbi.ucode = RT64::GBIUCode::F3DWAVE;
                RT64::GBI_RDP::setup(&gbi, true);
                RT64::GBI_F3DWAVE::setup(&gbi);
            }
            gbi.flags = {};
            app->interpreter->hleGBI = &gbi;
            std::printf(
                "[sote] microcode hash lookup missed; "
                "forcing F3DBETA through RT64 F3DWAVE\n");
            std::fflush(stdout);
            app->state->rsp->setGBI(app->interpreter->hleGBI);
            if (app->interpreter->hleGBI->resetFromTask != nullptr) {
                app->interpreter->hleGBI->resetFromTask(app->state.get());
            }
        }
        app->processDisplayLists(
            app->core.RDRAM,
            task->t.data_ptr & 0x03FFFFFF,
            0,
            true);
    }

    void update_screen() override {
        const int screen_number = ++screen_count;
        const bool trace_every_vi =
            std::getenv("SOTE_TRACE_EVERY_VI") != nullptr;
        const auto* vi = ultramodern::renderer::get_vi_regs();
        const uint32_t origin = vi->VI_ORIGIN_REG;
        const uint32_t width = vi->VI_WIDTH_REG;
        const uint32_t status = vi->VI_STATUS_REG;
        sote::graphics_menu::Settings requested{};
        if (sote::graphics_menu::take_renderer_request(requested)) {
            using Resolution = RT64::UserConfiguration::Resolution;
            using AspectRatio = RT64::UserConfiguration::AspectRatio;
            using Antialiasing =
                RT64::UserConfiguration::Antialiasing;
            switch (requested.resolution) {
                case sote::graphics_menu::ResolutionPreset::Native:
                    app->userConfig.resolution = Resolution::Original;
                    app->userConfig.resolutionMultiplier = 1.0;
                    break;
                case sote::graphics_menu::ResolutionPreset::Scale2x:
                    app->userConfig.resolution = Resolution::Manual;
                    app->userConfig.resolutionMultiplier = 2.0;
                    break;
                case sote::graphics_menu::ResolutionPreset::Scale4x:
                    app->userConfig.resolution = Resolution::Manual;
                    app->userConfig.resolutionMultiplier = 4.0;
                    break;
                case sote::graphics_menu::ResolutionPreset::Scale8x:
                    app->userConfig.resolution = Resolution::Manual;
                    app->userConfig.resolutionMultiplier = 8.0;
                    break;
                case sote::graphics_menu::ResolutionPreset::WindowScale:
                default:
                    app->userConfig.resolution =
                        Resolution::WindowIntegerScale;
                    app->userConfig.resolutionMultiplier = 4.0;
                    break;
            }
            app->userConfig.aspectRatio = requested.widescreen
                ? AspectRatio::Expand
                : AspectRatio::Original;
            app->userConfig.extAspectRatio =
                app->userConfig.aspectRatio;
            switch (requested.antialiasing) {
                case sote::graphics_menu::AntialiasingPreset::MSAA2x:
                    app->userConfig.antialiasing =
                        Antialiasing::MSAA2X;
                    break;
                case sote::graphics_menu::AntialiasingPreset::MSAA4x:
                    app->userConfig.antialiasing =
                        Antialiasing::MSAA4X;
                    break;
                case sote::graphics_menu::AntialiasingPreset::MSAA8x:
                    app->userConfig.antialiasing =
                        Antialiasing::MSAA8X;
                    break;
                case sote::graphics_menu::AntialiasingPreset::Off:
                default:
                    app->userConfig.antialiasing =
                        Antialiasing::None;
                    break;
            }
            app->updateUserConfig(true);
            app->saveConfiguration();
            std::printf(
                "[sote] graphics configuration applied: "
                "resolution=%d multiplier=%.1f aspect=%d msaa=%u\n",
                static_cast<int>(app->userConfig.resolution),
                app->userConfig.resolutionMultiplier,
                static_cast<int>(app->userConfig.aspectRatio),
                app->userConfig.msaaSampleCount());
            std::fflush(stdout);
        }
        app->updateScreen();
        if (screen_number == 1) {
            std::printf(
                "[sote] graphics output: client=%ux%u "
                "resolution_scale=%.2f\n",
                app->sharedQueueResources->swapChainWidth,
                app->sharedQueueResources->swapChainHeight,
                get_resolution_scale());
            std::fflush(stdout);
        }
        if (std::getenv("SOTE_DIAGNOSTIC_CAPTURE_SYNC") != nullptr) {
            // Hidden diagnostic captures must observe the VI named by the
            // following trace line, not whichever swap-chain image happened
            // to finish first. This deliberately serializes presentation only
            // for the capture tool; normal gameplay remains asynchronous.
            app->presentQueue->waitForPresentId(app->state->presentId);
            app->presentQueue->waitForIdle();
        }
        if ((std::getenv("SOTE_TRACE_GBI") != nullptr &&
             screen_number % 30 == 0) ||
            trace_every_vi) {
            std::printf(
                "[sote] VI present=%d origin=%06X width=%u status=%08X\n",
                screen_number,
                origin,
                width,
                status);
            std::fflush(stdout);
        }
    }

    void shutdown() override {
        if (app != nullptr) {
            app->end();
        }
    }

    uint32_t get_display_framerate() const override {
        return app->presentQueue->ext.sharedResources->swapChainRate;
    }

    float get_resolution_scale() const override {
        constexpr int reference_height = 240;
        if (app->sharedQueueResources->swapChainHeight > 0) {
            return std::max(
                float(
                    (app->sharedQueueResources->swapChainHeight +
                     reference_height - 1) /
                    reference_height),
                1.0f);
        }
        return 1.0f;
    }

private:
    std::unique_ptr<RT64::Application> app;
    uint32_t last_ucode = UINT32_MAX;
    uint32_t last_ucode_data = UINT32_MAX;
    uint32_t last_ucode_size = UINT32_MAX;
    uint32_t last_ucode_data_size = UINT32_MAX;
    int screen_count = 0;
};

} // namespace

std::unique_ptr<ultramodern::renderer::RendererContext>
create_rt64_renderer(
    uint8_t* rdram,
    ultramodern::renderer::WindowHandle window_handle,
    bool developer_mode,
    const std::filesystem::path& data_directory) {
    return std::make_unique<RT64Renderer>(
        rdram,
        window_handle,
        developer_mode,
        data_directory);
}
