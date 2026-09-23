#include "san_movies.hpp"

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

int checks = 0;
int failures = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << name << "\n";
    }
}

void be32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value >> 24));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value));
}

void le16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void le32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value >> 16));
    out.push_back(static_cast<uint8_t>(value >> 24));
}

uint32_t tag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(static_cast<unsigned char>(a)) << 24) |
        (static_cast<uint32_t>(static_cast<unsigned char>(b)) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(c)) << 8) |
        static_cast<uint32_t>(static_cast<unsigned char>(d));
}

void chunk(std::vector<uint8_t>& out, uint32_t name, const std::vector<uint8_t>& data) {
    be32(out, name);
    be32(out, static_cast<uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    if ((data.size() & 1U) != 0) {
        out.push_back(0);
    }
}

void write_file(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
}

void write_text_file(const fs::path& path, const char* text) {
    fs::create_directories(path.parent_path());
    std::ofstream output{path};
    output << text;
}

std::vector<uint8_t> anim_fixture() {
    std::vector<uint8_t> out;
    be32(out, tag('A', 'N', 'I', 'M'));
    be32(out, 0);
    std::vector<uint8_t> ahdr;
    le16(ahdr, 2);
    le16(ahdr, 1);
    ahdr.push_back(0);
    ahdr.push_back(0);
    ahdr.resize(0x306, 0);
    le32(ahdr, 15);
    le32(ahdr, 0);
    le32(ahdr, 22050);
    chunk(out, tag('A', 'H', 'D', 'R'), ahdr);

    std::vector<uint8_t> frame;
    std::vector<uint8_t> fobj;
    fobj.push_back(48);
    fobj.push_back(1);
    le16(fobj, 12);
    le16(fobj, 8);
    le16(fobj, 320);
    le16(fobj, 200);
    le16(fobj, 0);
    le16(fobj, 0);
    fobj.push_back(0);
    fobj.push_back(0);
    chunk(frame, tag('F', 'O', 'B', 'J'), fobj);
    chunk(frame, tag('I', 'A', 'C', 'T'), {4, 5});
    chunk(frame, tag('X', 'P', 'A', 'L'), {6, 7, 8});
    chunk(out, tag('F', 'R', 'M', 'E'), frame);
    return out;
}

std::vector<uint8_t> sanm_fixture() {
    std::vector<uint8_t> out;
    be32(out, tag('S', 'A', 'N', 'M'));
    be32(out, 0);
    std::vector<uint8_t> shdr;
    le16(shdr, 3);
    le32(shdr, 1);
    le16(shdr, 0);
    le16(shdr, 320);
    le16(shdr, 200);
    le16(shdr, 0);
    le32(shdr, 15);
    le16(shdr, 0);
    chunk(out, tag('S', 'H', 'D', 'R'), shdr);
    std::vector<uint8_t> frame;
    chunk(frame, tag('B', 'l', '1', '6'), std::vector<uint8_t>(20, 0));
    chunk(frame, tag('W', 'a', 'v', 'e'), std::vector<uint8_t>(12, 1));
    chunk(out, tag('F', 'R', 'M', 'E'), frame);
    return out;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    const fs::path scratch = argv[1];
    fs::create_directories(scratch);
    const fs::path anim = scratch / "demo.SAN";
    const fs::path sanm = scratch / "retail.SAN";
    write_file(anim, anim_fixture());
    write_file(sanm, sanm_fixture());

    auto info = sote::san_movies::inspect_file(anim);
    check(info.container == sote::san_movies::ContainerKind::AnimAhdr, "ANIM/AHDR fixture detected");
    check(info.version == 2 && info.frame_count == 1, "ANIM header fields parsed");
    check(info.frme_chunks == 1 && info.fobj_chunks == 1 &&
        info.iact_chunks == 1 && info.xpal_chunks == 1, "ANIM frame chunks parsed");
    check(info.codec48_chunks == 1 && info.codec47_chunks == 0,
        "ANIM FOBJ codec parsed");
    check(info.first_x == 12 && info.first_y == 8 &&
        info.first_width == 320 && info.first_height == 200,
        "ANIM FOBJ dimensions parsed");
    check(info.audio_rate == 22050, "ANIM audio rate parsed");

    info = sote::san_movies::inspect_file(sanm);
    check(info.container == sote::san_movies::ContainerKind::SanmShdr, "SANM/SHDR fixture detected");
    check(info.frame_count == 1 && info.frame_rate == 15, "SANM header fields parsed");
    check(info.frme_chunks == 1 && info.bl16_chunks == 1 &&
        info.wave_chunks == 1, "SANM frame chunks parsed");

    write_file(scratch / "bad.SAN", {0, 1, 2, 3});
    check(!sote::san_movies::inspect_file(scratch / "bad.SAN").error.empty(),
        "malformed SAN rejected");
    check(sote::san_movies::expected_movie_count() == 17,
        "expected SAN manifest size");
    check(sote::san_movies::expected_movie_name(0) == "GAMEOVER.SAN",
        "expected SAN first entry");
    check(sote::san_movies::find_movie(scratch, "missing.SAN").empty(),
        "missing SAN lookup is empty");

    const fs::path cache_dir = scratch / "Sdata" / "SAN_CACHE" / "GAMEOVER";
    write_text_file(
        cache_dir / "metadata.tsv",
        "width\t2\nheight\t2\nfps\t20/1\nframes\t3\n");
    write_file(
        cache_dir / "frames.rgba",
        {
            255, 0, 0, 255,
            0, 255, 0, 255,
            0, 0, 255, 255,
            255, 255, 255, 255,
            10, 20, 30, 255,
            40, 50, 60, 255,
            70, 80, 90, 255,
            100, 110, 120, 255,
            210, 220, 230, 255,
            211, 221, 231, 255,
            212, 222, 232, 255,
            213, 223, 233, 255,
        });
    sote::san_movies::initialize(scratch);
    check(sote::san_movies::play_cached_preview("GAMEOVER.SAN"),
        "cached preview can be loaded from generated cache");
    const auto frame = sote::san_movies::latest_cached_frame();
    check(frame.valid() && frame.width == 2 && frame.height == 2,
        "cached preview exposes a valid RGBA frame");
    check(frame.rgba.size() == 16 && frame.rgba[0] == 255 &&
        frame.rgba[5] == 255 && frame.rgba[10] == 255,
        "cached preview preserves RGBA pixel data");
    check(sote::san_movies::cached_playback_active(),
        "cached preview reports active playback");
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    const auto advanced_frame = sote::san_movies::latest_cached_frame();
    check(advanced_frame.valid() && advanced_frame.serial > frame.serial &&
        advanced_frame.rgba[0] != frame.rgba[0],
        "cached preview advances frames using metadata timing");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    check(!sote::san_movies::latest_cached_frame().valid() &&
        !sote::san_movies::cached_playback_active(),
        "cached preview stops after the final frame");

    const fs::path short_cache_dir = scratch / "Sdata" / "SAN_CACHE" / "L00LOGO";
    write_text_file(
        short_cache_dir / "metadata.tsv",
        "width\t2\nheight\t2\nfps\t120/1\nframes\t99\n");
    write_file(
        short_cache_dir / "frames.rgba",
        {
            1, 2, 3, 255,
            4, 5, 6, 255,
            7, 8, 9, 255,
            10, 11, 12, 255,
        });
    check(sote::san_movies::play_cached_preview("L00LOGO.SAN"),
        "short decoded cache starts");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    check(!sote::san_movies::latest_cached_frame().valid() &&
        !sote::san_movies::cached_playback_active(),
        "short decoded cache clamps to actual frame count");

    const fs::path startup_logo_cache = scratch / "Sdata" / "SAN_CACHE" / "L00LOGO";
    const fs::path startup_long_cache = scratch / "Sdata" / "SAN_CACHE" / "LONGTIME";
    const fs::path startup_intro_cache = scratch / "Sdata" / "SAN_CACHE" / "L01INTRO";
    write_text_file(
        startup_logo_cache / "metadata.tsv",
        "width\t2\nheight\t2\nfps\t60/1\nframes\t1\n");
    write_text_file(
        startup_long_cache / "metadata.tsv",
        "width\t2\nheight\t2\nfps\t60/1\nframes\t1\n");
    write_text_file(
        startup_intro_cache / "metadata.tsv",
        "width\t2\nheight\t2\nfps\t60/1\nframes\t1\n");
    write_file(
        startup_logo_cache / "frames.rgba",
        {
            21, 0, 0, 255,
            21, 0, 0, 255,
            21, 0, 0, 255,
            21, 0, 0, 255,
        });
    write_file(
        startup_long_cache / "frames.rgba",
        {
            0, 22, 0, 255,
            0, 22, 0, 255,
            0, 22, 0, 255,
            0, 22, 0, 255,
        });
    write_file(
        startup_intro_cache / "frames.rgba",
        {
            0, 0, 23, 255,
            0, 0, 23, 255,
            0, 0, 23, 255,
            0, 0, 23, 255,
        });
    check(sote::san_movies::play_startup_sequence(),
        "startup sequence starts with cached logo");
    auto startup_frame = sote::san_movies::latest_cached_frame();
    check(startup_frame.valid() && startup_frame.rgba[0] == 21,
        "startup sequence first frame is logo");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    startup_frame = sote::san_movies::latest_cached_frame();
    check(startup_frame.valid() && startup_frame.rgba[1] == 22,
        "startup sequence advances to LONGTIME");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    startup_frame = sote::san_movies::latest_cached_frame();
    check(startup_frame.valid() && startup_frame.rgba[2] == 23,
        "startup sequence advances to intro crawl");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    check(!sote::san_movies::latest_cached_frame().valid(),
        "startup sequence stops after queued movies");

    if (argc > 2 && fs::is_directory(argv[2])) {
        int real_count = 0;
        int anim_count = 0;
        for (const auto& entry : fs::directory_iterator(argv[2])) {
            if (!entry.is_regular_file() || entry.path().extension() != ".SAN") {
                continue;
            }
            ++real_count;
            const auto real = sote::san_movies::inspect_file(entry.path());
            check(real.error.empty(), "real SAN parses without structural errors");
            if (real.container == sote::san_movies::ContainerKind::AnimAhdr) {
                ++anim_count;
                check(real.frme_chunks > 0, "real ANIM SAN has frames");
                check(real.fobj_chunks > 0, "real ANIM SAN has video payload");
                check(real.first_width > 0 && real.first_height > 0,
                    "real ANIM SAN exposes frame dimensions");
                check(real.codec47_chunks + real.codec48_chunks == real.fobj_chunks,
                    "real ANIM SAN video payload declares codec 47 or 48");
            }
        }
        check(real_count == 0 || anim_count == real_count,
            "real staged SAN files use ANIM/AHDR path");
        for (int index = 0; index < sote::san_movies::expected_movie_count(); ++index) {
            const auto name = sote::san_movies::expected_movie_name(index);
            check(!sote::san_movies::find_movie(
                    fs::path{argv[2]}.parent_path(),
                    name).empty(),
                "expected staged SAN is discoverable");
        }
        const fs::path cache_root = fs::path{argv[2]} / "SAN_CACHE";
        if (fs::is_directory(cache_root)) {
            sote::san_movies::initialize(fs::path{argv[2]}.parent_path());
            for (int index = 0; index < sote::san_movies::expected_movie_count(); ++index) {
                const auto name = sote::san_movies::expected_movie_name(index);
                check(sote::san_movies::play_cached_preview(name),
                    "expected staged SAN cache starts playback");
                check(sote::san_movies::latest_cached_frame().valid(),
                    "expected staged SAN cache exposes a frame");
                (void)sote::san_movies::stop_cached_playback();
            }
        }
    }

    std::cout << "SAN checks: " << checks << "; failures: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
