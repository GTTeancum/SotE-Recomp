#include "first_person_blaster.hpp"
#include "recomp.h"

#include <cmath>
#include <cstring>
#ifdef SOTE_XBOX
#include "rdram_guard.h"
#endif

extern "C" void func_800BBAA0(uint8_t*, recomp_context*);

namespace {
using sote::first_person_blaster::Matrix;

// USA Rev 1.2. BB11C builds the foreground HUD immediately after BCAA4
// finishes the world scene. BA9C8 selects one of two frame arenas.
constexpr uint32_t frame_pointer = 0x800D256C, list_pointer = 0x80192F28;
constexpr uint32_t arena_base = 0x801158E0, arena_size = 0x139C8;
constexpr uint32_t viewport = 0x800E7AE0, mesh_stack_flag = 0x800E7C38;

bool valid(uint32_t addr, unsigned bytes) {
#ifdef SOTE_XBOX
    if (addr < 0x80000000U || !xb_rdram_range_valid(addr-0x80000000U,bytes,xb_rdram_committed_limit)) return false;
#endif
    return !(addr & 3) && addr >= 0x80000000U && addr <= 0x80800000U - bytes;
}
template<class T> T read(const uint8_t* ram, uint32_t addr) {
    T v;
    std::memcpy(&v, ram + ((addr & 0x7fffff) ^ (sizeof(T) == 2 ? 2 : 0)), sizeof(v));
    return v;
}
template<class T> void write(uint8_t* ram, uint32_t addr, T value) {
    std::memcpy(ram + ((addr & 0x7fffff) ^ (sizeof(T) == 2 ? 2 : 0)), &value, sizeof(value));
}

uint32_t child(const uint8_t* ram, uint32_t transform, unsigned index) {
    if (!valid(transform, 0x48) || read<uint16_t>(ram, transform) != 0xD065) return 0;
    const auto count = read<uint32_t>(ram, transform + 0xC);
    const auto list = read<uint32_t>(ram, transform + 0x10);
    if (count <= index || count > 32 || !valid(list, count * 4)) return 0;
    return read<uint32_t>(ram, list + index * 4);
}

// Resolve from this actor every draw. The three 3064 nodes retain the
// game's own bounds, submeshes, material bindings, vertices and triangles.
struct RightArm {
    uint32_t wrist, forearm, hand, blaster;
    bool resolve(const uint8_t* ram, uint32_t player, bool train) {
        if (!valid(player, train ? 0x264 : 0x258) ||
            read<uint32_t>(ram, player) != (train ? 0x48756D54u : 0x48756D6Eu)) return false;
        const auto chest = read<uint32_t>(ram, player + (train ? 0x260 : 0x254));
        const auto shoulder = child(ram, chest, 0);
        const auto elbow = child(ram, shoulder, 0);
        wrist = child(ram, elbow, 0);
        forearm = child(ram, elbow, 1);
        hand = child(ram, wrist, 0);
        // The train actor has a single wrist mesh containing both glove and gun.
        blaster = train ? 0 : child(ram, wrist, 1);
        const auto mesh_valid = [ram](uint32_t part) {
            if (!valid(part, 0x18) || read<uint16_t>(ram, part) != 0x3064) return false;
            const auto groups = read<uint32_t>(ram, part + 0xC);
            return groups && groups <= 8 && valid(read<uint32_t>(ram, part + 0x10), groups * 4);
        };
        return mesh_valid(forearm) && mesh_valid(hand) && (train || mesh_valid(blaster));
    }
};

Matrix product(const Matrix& left, const Matrix& right) {
    Matrix result{};
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned col = 0; col < 4; ++col) {
            for (unsigned k = 0; k < 4; ++k) result[row][col] += left[row][k] * right[k][col];
        }
    }
    return result;
}

// Pose the original forearm around the wrist. The authored wrist offset is
// in bone units, with eight mesh units per bone unit. Keeping this pivot
// fixed lets the elbow drop below the image without separating the glove.
Matrix elbow_from_wrist(const uint8_t* ram, uint32_t wrist) {
    constexpr float bend = 0.65f;
    const float c = std::cos(bend), s = std::sin(bend);
    Matrix elbow = {{{c, s, 0, 0}, {-s, c, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    for (unsigned axis = 0; axis < 3; ++axis) {
        const float offset = 8.0f * read<float>(ram, wrist + 0x38 + 4 * axis);
        for (unsigned i = 0; i < 3; ++i) elbow[3][i] -= offset * elbow[axis][i];
    }
    return elbow;
}

bool write_matrix(uint8_t* ram, uint32_t address, const Matrix& matrix) {
    // N64 Mtx stores the integer parts of all 16 elements before the fractions.
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned col = 0; col < 4; ++col) {
            const auto value = matrix[row][col];
            if (!std::isfinite(value) || std::abs(value) > 32767) return false;
            const auto fixed = static_cast<uint32_t>(static_cast<int32_t>(std::round(value * 65536)));
            const auto offset = (row * 4 + col) * 2;
            write<uint16_t>(ram, address + offset, static_cast<uint16_t>(fixed >> 16));
            write<uint16_t>(ram, address + 32 + offset, static_cast<uint16_t>(fixed));
        }
    }
    return true;
}

void command(uint8_t* ram, uint32_t op, uint32_t argument) {
    const auto cursor = read<uint32_t>(ram, list_pointer);
    write(ram, cursor, op);
    write(ram, cursor + 4, argument);
    write(ram, list_pointer, cursor + 8);
}

void draw_part(uint8_t* ram, recomp_context& ctx, uint32_t scratch_node, uint32_t mesh) {
    // The retail visitor caches generated commands in each leaf. Only the
    // instance gets those transient fields. Dash's world model stays intact.
    std::memcpy(ram + (scratch_node & 0x7fffff), ram + (mesh & 0x7fffff), 0x18);
    write(ram, scratch_node + 4, ~read<uint32_t>(ram, 0x8018E9B0));
    ctx.r4 = static_cast<int32_t>(scratch_node);
    func_800BBAA0(ram, &ctx);
}
}

extern "C" void sote_draw_first_person_blaster(uint8_t* ram, void* context) {
    auto pose = sote::first_person_blaster::animation.current();
    if (!pose.visible || read<uint32_t>(ram, 0x800DD2B0) != 2) return;
    const bool train = read<uint16_t>(ram, 0x8013CE0E) == 8;
    if (train ? read<uint32_t>(ram, 0x800D08A8) != 4 :
                (read<uint16_t>(ram, 0x8018DDE8) != 0 || pose.player != read<uint32_t>(ram, 0x80112B30))) return;
    RightArm arm{};
    if (!arm.resolve(ram, pose.player, train)) return;
    auto ctx = *static_cast<recomp_context*>(context);
    const auto stack = static_cast<uint32_t>(ctx.r29);
    const auto frame = read<uint32_t>(ram, frame_pointer);
    const auto dl = read<uint32_t>(ram, list_pointer);
    if (!valid(stack - 0x300, 0x300) || (frame != arena_base && frame != arena_base + arena_size) ||
        (dl & 7) || dl < frame + 0x148 || dl > frame + arena_size - 0x1000) return;
    const uint32_t scratch = (stack - 0x30) & ~15U;
    ctx.r29 = static_cast<int32_t>(scratch - 0x30);
    ctx.f_odd = ctx.mips3_float_mode ? &ctx.f1.u32l : &ctx.f0.u32h;

    // Place immutable RSP inputs in this frame's display-list arena, behind
    // a branch. They live until RT64 finishes this frame, including on Linux.
    const uint32_t projection_addr = dl + 8;
    const uint32_t wrist = projection_addr + 64, elbow = wrist + 64;
    const uint32_t vp = elbow + 64, next = vp + 16;
    // 60 degrees vertically, 4:3 before RT64's widescreen correction, near 4 / far 320.
    const Matrix projection = {{{1.299038f, 0, 0, 0}, {0, 1.732051f, 0, 0},
                               {0, 0, -1.025316f, -1}, {0, 0, -8.101266f, 0}}};
    // HumT is authored at 3/5 the size of HumN, including the wrist mesh
    // and skeleton translations. Normalize both before posing the weapon.
    const float scale = train ? 5.f / 3.f : 1.f;
    const auto skeleton = read<uint32_t>(ram, pose.player + (train ? 0x19c : 0x188));
    const auto hips = child(ram, skeleton, 0);
    const auto spine = child(ram, hips, 2);
    const auto chest = child(ram, spine, 0);
    const auto shoulder = child(ram, chest, 0);
    if (valid(shoulder, 0x48) && read<uint16_t>(ram, shoulder) == 0xD065) {
        // Follow Dash's animated right shoulder in actor space, before the
        // actor's world transform moves him with the train. The translations
        // between bones are fixed; the parent rotations produce the stride.
        std::array<float, 3> position{};
        for (uint32_t joint : {shoulder, chest, spine, hips}) {
            const auto local = position;
            for (unsigned i = 0; i < 3; ++i) {
                position[i] = read<float>(ram, joint + 0x38 + 4 * i);
                for (unsigned j = 0; j < 3; ++j)
                    position[i] += local[j] * read<float>(ram, joint + 0x14 + 4 * (3*j + i));
            }
        }
        // Neutral shoulder coordinates in HumN bone units.
        pose.sway = scale * position[0] + 3.77506f;
        pose.lift = scale * position[2] - 10.54454f;
    }
    auto hand = wrist_in_view(pose);
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col) hand[row][col] *= scale;
    if (!write_matrix(ram, projection_addr, projection) || !write_matrix(ram, wrist, hand) ||
        !write_matrix(ram, elbow, product(elbow_from_wrist(ram, arm.wrist), hand))) return;
    std::memcpy(ram + (vp & 0x7fffff), ram + (viewport & 0x7fffff), 16);
    // Foreground depth range keeps the hand in front of scenery while the
    // three original meshes still depth-test against each other.
    write<int16_t>(ram, vp + 4, 48);
    write<int16_t>(ram, vp + 12, 48);
    command(ram, 0x06010000, next);
    write(ram, list_pointer, next);
    command(ram, 0xE7000000, 0);
    command(ram, 0x03800010, vp);
    command(ram, 0x01030040, projection_addr);
    command(ram, 0x01060040, elbow); // load and push modelview
    const auto old_stack = read<uint16_t>(ram, mesh_stack_flag);
    write<uint16_t>(ram, mesh_stack_flag, 1);
    draw_part(ram, ctx, scratch, arm.forearm);
    command(ram, 0x01020040, wrist);
    draw_part(ram, ctx, scratch, arm.hand);
    if (arm.blaster) draw_part(ram, ctx, scratch, arm.blaster);
    write(ram, mesh_stack_flag, old_stack);
    command(ram, 0xBD000000, 0);
    command(ram, 0x01030040, frame + 0xC0);
    command(ram, 0x03800010, viewport);
    command(ram, 0xE7000000, 0);
    // The HUD sets up its own render modes after this hook.
    write(ram, 0x80192F3C, UINT32_MAX);
    write(ram, 0x80192F44, UINT32_MAX);
    static const bool trace=std::getenv("SOTE_TRACE_FIRST_PERSON_BLASTER")!=nullptr;
    if (trace) {
        ::printf("[sote][blaster-pose] frame=%u sway=%.5f lift=%.5f recoil=%.3f\n",
            read<uint32_t>(ram, 0x8018E9B0), pose.sway, pose.lift, pose.recoil);
        static int previous=-1;
        const int state=pose.flash ? 2 : std::hypot(pose.lift,pose.sway)>.08f;
        if (state!=previous) {
            previous=state;
            ::printf("[sote][blaster-instance] forearm=%08X hand=%08X blaster=%08X motion=%d recoil=%.3f\n",
                arm.forearm,arm.hand,train ? arm.hand : arm.blaster,state,pose.recoil);
        }
    }
}
