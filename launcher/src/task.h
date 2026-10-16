// Forza3Launcher - progress shared between a worker thread and the UI.

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace launcher {

struct TaskProgress {
  enum class State { kIdle, kRunning, kDone, kFailed, kCancelled };

  std::atomic<State> state{State::kIdle};
  std::atomic<uint64_t> done{0};   // Bytes (or files) processed.
  std::atomic<uint64_t> total{0};  // 0 while unknown.
  std::atomic<bool> cancel{false};

  void SetStatus(std::string text) {
    std::lock_guard lock(mutex_);
    status_ = std::move(text);
  }
  std::string Status() {
    std::lock_guard lock(mutex_);
    return status_;
  }
  bool Busy() const { return state.load() == State::kRunning; }

 private:
  std::mutex mutex_;
  std::string status_;
};

}  // namespace launcher
