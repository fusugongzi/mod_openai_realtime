#ifndef AUDIO_INTERRUPTION_POLICY_H
#define AUDIO_INTERRUPTION_POLICY_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

// Keeps caller speech intact when interruptions are disabled. Playback boundaries
// come from the media thread, rather than the server's faster audio generation.
class AudioInterruptionPolicy {
  public:
    explicit AudioInterruptionPolicy(bool allow_interrupt = true) : m_allow_interrupt(allow_interrupt) {}

    bool allows_interrupt() const {
        return m_allow_interrupt;
    }

    void playback_started() {
        m_playback_pending = true;
    }

    void playback_finished() {
        m_playback_pending = false;
    }

    template <typename SendAudio>
    void send_user_audio(uint8_t *buffer, size_t len, SendAudio send_audio) {
        if (m_allow_interrupt) {
            send_audio(buffer, len);
            return;
        }

        std::lock_guard<std::mutex> lock(m_user_audio_mutex);
        m_deferred_user_audio.emplace(buffer, buffer + len);
        while (!m_playback_pending && !m_deferred_user_audio.empty()) {
            auto& audio = m_deferred_user_audio.front();
            send_audio(audio.data(), audio.size());
            m_deferred_user_audio.pop();
        }
    }

  private:
    const bool m_allow_interrupt;
    std::atomic<bool> m_playback_pending{false};
    std::mutex m_user_audio_mutex;
    std::queue<std::vector<uint8_t>> m_deferred_user_audio;
};

#endif
