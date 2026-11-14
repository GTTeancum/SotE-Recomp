#pragma once

namespace sote {
// Physical A is reserved only while a communicator is open. Retain
// ownership until release so dismissing the final page cannot also jump/fire.
struct CommunicatorInput {
    struct Decision { bool consume; bool advance; };
    bool previous = false;
    bool owned = false;
    Decision update(bool prompt, bool down) {
        if (!down) owned = false;
        const bool advance = prompt && down && !previous;
        if (prompt && down) owned = true;
        previous = down;
        return {owned, advance};
    }
};
}
