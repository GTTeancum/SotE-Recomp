#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace sote {
// Only the documented Sdata folder beside the executable enables PC mode.
inline bool pc_cutscene_files_present(const std::filesystem::path& runtime) {
    constexpr std::array names{
        "GAMEOVER.SAN", "L00LOGO.SAN", "L01INTRO.SAN", "L02INTRO.SAN",
        "L03INTRO.SAN", "L04BOSS.SAN", "L04INTRO.SAN", "L05BOSS.SAN",
        "L05INTRO.SAN", "L06INTRO.SAN", "L07INTRO.SAN", "L08INTRO.SAN",
        "L09BOSS.SAN", "L10INTRO.SAN", "L11LOSE.SAN", "L11WIN.SAN", "LONGTIME.SAN"};
    for (const char* name : names) {
        std::ifstream input(runtime / "Sdata" / name, std::ios::binary);
        char header[8]{};
        if (input.read(header, sizeof(header)) &&
            (std::string_view(header,4) == "ANIM" || std::string_view(header,4) == "SANM"))
            return true;
    }
    return false;
}
}
