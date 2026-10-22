#include "san_movies.hpp"
extern "C" {
#include "sandec.h"
}

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

uint64_t reference_audio_samples(const fs::path& path) {
    struct Input { std::ifstream file; uint64_t samples = 0; } input{std::ifstream(path, std::ios::binary)};
    sanio io{};
    io.userctx = &input;
    io.ioread = [](void* p, void* dst, uint32_t size) {
        auto& in = *static_cast<Input*>(p);
        in.file.read(static_cast<char*>(dst), size);
        return int(in.file.gcount() == size);
    };
    io.queue_video = [](void*, unsigned char*, uint32_t, uint16_t, uint16_t, uint16_t, uint32_t*, uint16_t, uint32_t) {};
    io.queue_audio = [](void* p, unsigned char*, uint32_t size) { static_cast<Input*>(p)->samples += size / 2; };
    void* decoder = nullptr;
    int status = sandec_init(&decoder);
    if (status == SANDEC_OK) status = sandec_open(decoder, &io);
    while (status == SANDEC_OK) status = sandec_decode_next_frame(decoder);
    sandec_exit(&decoder);
    check(status == SANDEC_DONE, "reference audio decodes completely");
    return input.samples;
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

    sote::san_movies::initialize(scratch);
    check(!sote::san_movies::play_movie("missing.SAN"), "missing SAN fails without cache fallback");
    if (argc > 2 && fs::is_directory(argv[2])) {
        const auto runtime = fs::path{argv[2]}.parent_path();
        sote::san_movies::initialize(runtime);
        for (int index = 0; index < sote::san_movies::expected_movie_count(); ++index) {
            const auto name = sote::san_movies::expected_movie_name(index);
            std::cout << "Direct movie: " << name << std::endl;
            check(sote::san_movies::play_movie(name), "loose SAN starts directly");
            const auto first = sote::san_movies::latest_frame();
            check(first.valid(), "direct decoder supplies a video frame");
            const auto token = sote::san_movies::playback_token();
            check(token != 0, "direct playback receives a token");
            (void)sote::san_movies::next_audio();
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            const auto second = sote::san_movies::latest_frame();
            check(second.valid() && second.serial > first.serial, "direct video advances with clock");
            (void)sote::san_movies::next_audio();
            check(sote::san_movies::finish_playback(), "entire SAN decodes to final frame");
            check(sote::san_movies::latest_frame().valid(), "held final frame remains visible");
            check(sote::san_movies::playback_remaining_seconds() <= 0.001, "finished stream has no remaining duration");
            check(sote::san_movies::next_audio().empty(), "finish discards unplayed audio");
            check(sote::san_movies::stop_playback(), "skip releases decoder");
            check(!sote::san_movies::playback_active() && sote::san_movies::playback_token() == 0,
                  "skip clears playback token");
        }
        for (int index = 0; index < sote::san_movies::expected_movie_count(); ++index) {
            const auto name = sote::san_movies::expected_movie_name(index);
            check(sote::san_movies::play_movie(name), "start complete timeline");
            uint64_t audio_samples = 0;
            bool nonzero_audio = false;
            int ticks = 0;
            while (sote::san_movies::playback_active() && ticks++ < 18000) {
                sote::san_movies::advance_test_clock(1.0 / 60.0);
                (void)sote::san_movies::latest_frame();
                const auto audio = sote::san_movies::next_audio();
                audio_samples += audio.size();
                for (auto sample : audio) nonzero_audio |= sample != 0;
            }
            check(ticks < 18000, "complete timeline terminates");
            check(audio_samples == reference_audio_samples(sote::san_movies::find_movie(runtime, name)),
                  "playback delivers every decoded audio sample, including the tail");
            check(name == "LONGTIME.SAN" || (audio_samples > 0 && nonzero_audio),
                  "movie delivers audible PCM through its complete timeline");
            std::cout << "Timeline " << name << " ticks=" << ticks << " audio_samples=" << audio_samples << std::endl;
        }
        check(sote::san_movies::play_startup_sequence(), "direct startup sequence starts");
        const auto first_token = sote::san_movies::playback_token();
        check(sote::san_movies::finish_playback(), "startup logo decodes to completion");
        sote::san_movies::set_hold_last_frame(false);
        check(sote::san_movies::latest_frame().valid() &&
              sote::san_movies::playback_token() != first_token, "startup advances to LONGTIME");
        check(sote::san_movies::stop_playback(), "skip clears queued direct movies");
        check(!sote::san_movies::latest_frame().valid(), "skipped sequence does not restart");
    }

    std::cout << "SAN checks: " << checks << "; failures: " << failures << "\n";
    return failures == 0 ? 0 : 1;
}
