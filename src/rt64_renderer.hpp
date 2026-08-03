#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "ultramodern/renderer_context.hpp"

std::unique_ptr<ultramodern::renderer::RendererContext>
create_rt64_renderer(
    uint8_t* rdram,
    ultramodern::renderer::WindowHandle window_handle,
    bool developer_mode,
    const std::filesystem::path& data_directory);
