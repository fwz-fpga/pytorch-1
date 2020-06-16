#include <torch/csrc/distributed/rpc/profiler/remote_profiler_manager.h>
#include <torch/csrc/distributed/rpc/rpc_agent.h>
#include <torch/csrc/distributed/rpc/rpc_command_base.h>
#include <torch/csrc/jit/serialization/pickle.h>
#include <torch/csrc/utils/byte_order.h>

namespace torch {
namespace distributed {
namespace rpc {

constexpr int kAutoIncrementBits = 48;
thread_local c10::optional<std::string>
    RemoteProfilerManager::currentThreadLocalKey_ = c10::nullopt;
/*static */ RemoteProfilerManager& RemoteProfilerManager::getInstance() {
  static RemoteProfilerManager* handler = new RemoteProfilerManager();
  return *handler;
}

void RemoteProfilerManager::setCurrentKey(const std::string key) {
  // We should not allow overriding the current key, it needs to be committed
  // with writeKey() explicitly first.
  if (RemoteProfilerManager::currentThreadLocalKey_) {
    TORCH_CHECK(
        false,
        "Cannot call RemoteProfilerManager::setCurrentKey when current key is already set.");
  }
  currentThreadLocalKey_ = std::move(key);
}

void RemoteProfilerManager::unsetCurrentKey() {
  currentThreadLocalKey_ = c10::nullopt;
}

void RemoteProfilerManager::eraseKey(const ProfilingId& globallyUniqueId) {
  std::lock_guard<std::mutex> guard(mutex_);
  auto it = profiledRpcKeys_.find(globallyUniqueId);
  TORCH_INTERNAL_ASSERT(it != profiledRpcKeys_.end());
  profiledRpcKeys_.erase(it);
}

std::string RemoteProfilerManager::retrieveRPCProfilingKey(
    const ProfilingId& globallyUniqueId) {
  std::lock_guard<std::mutex> guard(mutex_);
  auto it = profiledRpcKeys_.find(globallyUniqueId);
  TORCH_INTERNAL_ASSERT(it != profiledRpcKeys_.end());
  return it->second;
}

local_id_t RemoteProfilerManager::getNextLocalId() {
  std::lock_guard<std::mutex> guard(mutex_);
  return currentLocalId_++;
}

std::string RemoteProfilerManager::getCurrentProfilingKey() {
  TORCH_CHECK(
      RemoteProfilerManager::currentThreadLocalKey_,
      "Must set currentThreadLocalKey_ before calling getCurrentProfilingKey");
  return *currentThreadLocalKey_;
}

void RemoteProfilerManager::saveRPCKey(
    ProfilingId globallyUniqueId,
    std::string rpcProfilingKey) {
  std::lock_guard<std::mutex> guard(mutex_);
  profiledRpcKeys_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(globallyUniqueId)),
      std::forward_as_tuple(std::move(rpcProfilingKey)));
}

RemoteProfilerManager::RemoteProfilerManager() {
  auto workerId =
      static_cast<int64_t>(RpcAgent::getCurrentRpcAgent()->getWorkerInfo().id_);
  currentLocalId_ = workerId << kAutoIncrementBits;
}
} // namespace rpc
} // namespace distributed
} // namespace torch
