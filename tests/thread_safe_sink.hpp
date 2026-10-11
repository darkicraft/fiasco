#pragma once

#include <mutex>
#include <ostream>
#include <streambuf>
#include <string>

namespace fiasco_test {

/// A thread-safe in-memory sink for log output.
///
/// fiasco's logger writes records from a dedicated consumer thread, while tests
/// read the accumulated text from the main thread. A plain std::ostringstream
/// would therefore be a data race (writes in the consumer vs. `str()` in the
/// test), which ThreadSanitizer reports. Both the streambuf writes and the read
/// here share one mutex.
class thread_safe_sink {
  public:
    thread_safe_sink() = default;

    /// The ostream to install with log_registry::set_sink().
    std::ostream& stream() { return m_stream; }

    /// Snapshot of everything written so far.
    std::string str() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffer;
    }

  private:
    struct sink_buf : std::streambuf {
        sink_buf(thread_safe_sink* owner, std::mutex* mutex)
            : m_owner(owner),
              m_mutex(mutex) {}

        std::streamsize xsputn(const char* s, std::streamsize n) override {
            std::lock_guard<std::mutex> lock(*m_mutex);
            m_owner->m_buffer.append(s, static_cast<std::size_t>(n));
            return n;
        }

        int overflow(int c) override {
            if (c == EOF) {
                return EOF;
            }
            std::lock_guard<std::mutex> lock(*m_mutex);
            m_owner->m_buffer.push_back(static_cast<char>(c));
            return c;
        }

        int sync() override { return 0; }

      private:
        thread_safe_sink* m_owner;
        std::mutex* m_mutex;
    };

    // Declaration order matters: m_buf/m_stream are built from m_mutex.
    mutable std::mutex m_mutex;
    std::string m_buffer;
    sink_buf m_buf{this, &m_mutex};
    std::ostream m_stream{&m_buf};
};

}  // namespace fiasco_test
