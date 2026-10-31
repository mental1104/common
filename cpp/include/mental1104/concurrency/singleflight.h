#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace mental1104 {

// SingleFlightResult copies the completed value for each caller. Value therefore
// needs to be copy-constructible when a result is returned to multiple waiters.
template <typename Value> struct SingleFlightResult {
  Value value;
  bool shared;
};

template <typename Key, typename Value, typename Hash = std::hash<Key> >
class SingleFlightGroup {
private:
  struct Call {
    Call() : done(false) {}

    std::mutex mutex;
    std::condition_variable condition;
    bool done;
    std::shared_ptr<Value> value;
    std::exception_ptr error;
  };

public:
  template <typename Loader>
  SingleFlightResult<Value> do_call(const Key &key, Loader loader) {
    std::shared_ptr<Call> call;
    bool leader = false;

    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      typename CallMap::iterator found = this->calls_.find(key);
      if (found == this->calls_.end()) {
        call = std::make_shared<Call>();
        this->calls_.insert(std::make_pair(key, call));
        leader = true;
      } else {
        call = found->second;
      }
    }

    if (!leader) {
      return this->wait_for_call(call, true);
    }

    try {
      std::shared_ptr<Value> value = std::make_shared<Value>(loader());
      {
        std::lock_guard<std::mutex> lock(call->mutex);
        call->value = value;
        call->done = true;
      }
    } catch (...) {
      {
        std::lock_guard<std::mutex> lock(call->mutex);
        call->error = std::current_exception();
        call->done = true;
      }
    }

    {
      std::lock_guard<std::mutex> lock(this->mutex_);
      typename CallMap::iterator found = this->calls_.find(key);
      if (found != this->calls_.end() && found->second == call) {
        this->calls_.erase(found);
      }
    }
    call->condition.notify_all();

    return this->wait_for_call(call, false);
  }

private:
  typedef std::unordered_map<Key, std::shared_ptr<Call>, Hash> CallMap;

  SingleFlightResult<Value>
  wait_for_call(const std::shared_ptr<Call> &call, bool shared) const {
    std::unique_lock<std::mutex> lock(call->mutex);
    while (!call->done) {
      call->condition.wait(lock);
    }
    if (call->error) {
      std::rethrow_exception(call->error);
    }
    return SingleFlightResult<Value>{*call->value, shared};
  }

  std::mutex mutex_;
  CallMap calls_;
};

} // namespace mental1104
