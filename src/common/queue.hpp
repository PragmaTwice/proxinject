#ifndef PROXINJECT_COMMON_QUEUE
#define PROXINJECT_COMMON_QUEUE

#include <queue>
#include <condition_variable>
#include <mutex>

template <typename T> class blocking_queue {
  std::condition_variable _cvCanPop;
  std::mutex _sync;
  std::queue<T> _qu;
  bool _bShutdown = false;

public:
  void push(const T &item) {
    {
      std::unique_lock<std::mutex> lock(_sync);
      _qu.push(item);
    }
    _cvCanPop.notify_one();
  }

  void shutdown() {
    {
      std::unique_lock<std::mutex> lock(_sync);
      _bShutdown = true;
    }
    _cvCanPop.notify_all();
  }

  bool pop(T &item) {
    std::unique_lock<std::mutex> lock(_sync);
    for (;;) {
      if (_qu.empty()) {
        if (_bShutdown) {
          return false;
        }
      } else {
        break;
      }
      _cvCanPop.wait(lock);
    }
    item = std::move(_qu.front());
    _qu.pop();
    return true;
  }
};

#endif
