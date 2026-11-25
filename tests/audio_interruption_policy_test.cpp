#include "audio_interruption_policy.h"

#include <cassert>
#include <iostream>
#include <thread>

int main() {
    // Default mode and explicitly enabled interruptions forward speech immediately.
    auto check_interruptions_allowed = [](AudioInterruptionPolicy& policy) {
        assert(policy.allows_interrupt());
        policy.playback_started();
        std::vector<uint8_t> received;
        uint8_t input[] = {1, 2, 3};
        policy.send_user_audio(input, sizeof(input), [&](uint8_t *data, size_t len) {
            received.insert(received.end(), data, data + len);
        });
        assert(received == std::vector<uint8_t>({1, 2, 3}));
    };
    AudioInterruptionPolicy default_policy;
    check_interruptions_allowed(default_policy);
    AudioInterruptionPolicy enabled_policy(true);
    check_interruptions_allowed(enabled_policy);

    {
        AudioInterruptionPolicy policy(false);
        assert(!policy.allows_interrupt());
        std::vector<uint8_t> received;
        auto send = [&](uint8_t *data, size_t len) { received.insert(received.end(), data, data + len); };
        uint8_t input[] = {1, 2};
        policy.send_user_audio(input, sizeof(input), send);
        policy.playback_started();
        input[0] = 3;
        input[1] = 4;
        policy.send_user_audio(input, sizeof(input), send);
        input[0] = 5;
        input[1] = 6;
        policy.send_user_audio(input, sizeof(input), send);
        assert(received == std::vector<uint8_t>({1, 2}));
        // Buffers are owned copies, so subsequent media frames cannot overwrite speech.
        input[0] = 7;
        input[1] = 8;
        policy.playback_finished();
        policy.send_user_audio(input, sizeof(input), send);
        assert(received == std::vector<uint8_t>({1, 2, 3, 4, 5, 6, 7, 8}));
    }

    {
        AudioInterruptionPolicy policy(false);
        policy.playback_started();
        std::vector<uint8_t> received;
        auto send = [&](uint8_t *data, size_t len) {
            received.insert(received.end(), data, data + len);
            if (received.size() == 1) {
                // The server can enqueue a new response while buffered speech flushes.
                policy.playback_started();
            }
        };
        uint8_t input = 1;
        policy.send_user_audio(&input, 1, send);
        input = 2;
        policy.send_user_audio(&input, 1, send);
        policy.playback_finished();
        input = 3;
        policy.send_user_audio(&input, 1, send);
        assert(received == std::vector<uint8_t>({1}));
        policy.playback_finished();
        input = 4;
        policy.send_user_audio(&input, 1, send);
        assert(received == std::vector<uint8_t>({1, 2, 3, 4}));
    }

    {
        AudioInterruptionPolicy policy(false);
        policy.playback_started();
        std::vector<uint8_t> received;
        auto send = [&](uint8_t *data, size_t len) { received.insert(received.end(), data, data + len); };
        std::thread media([&]() {
            for (unsigned i = 0; i < 200; ++i) {
                uint8_t input = static_cast<uint8_t>(i);
                policy.send_user_audio(&input, 1, send);
            }
        });
        media.join();
        assert(received.empty());
        std::thread playback([&]() { policy.playback_finished(); });
        playback.join();
        uint8_t input = 200;
        policy.send_user_audio(&input, 1, send);
        assert(received.size() == 201);
        for (unsigned i = 0; i < received.size(); ++i) {
            assert(received[i] == i);
        }
    }

    std::cout << "Audio interruption policy tests passed\n";
}
