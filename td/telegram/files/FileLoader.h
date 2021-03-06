//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2018
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/actor/actor.h"
#include "td/actor/PromiseFuture.h"

#include "td/telegram/files/FileLoaderActor.h"
#include "td/telegram/files/FileLocation.h"
#include "td/telegram/files/PartsManager.h"
#include "td/telegram/files/ResourceManager.h"
#include "td/telegram/files/ResourceState.h"
#include "td/telegram/net/NetQuery.h"

#include "td/utils/OrderedEventsProcessor.h"
#include "td/utils/Status.h"

#include <map>
#include <utility>

namespace td {
class FileLoader : public FileLoaderActor {
 public:
  class Callback {
   public:
    Callback() = default;
    Callback(const Callback &) = delete;
    Callback &operator=(const Callback &) = delete;
    virtual ~Callback() = default;
  };
  void set_resource_manager(ActorShared<ResourceManager> resource_manager) override;
  void update_priority(int8 priority) override;
  void update_resources(const ResourceState &other) override;

  void update_local_file_location(const LocalFileLocation &local) override;

 protected:
  void set_ordered_flag(bool flag);
  size_t get_part_size() const;

  struct PrefixInfo {
    int64 size = -1;
    bool is_ready = false;
  };
  struct FileInfo {
    int64 size;
    int64 expected_size = 0;
    bool is_size_final;
    int32 part_size;
    std::vector<int> ready_parts;
    bool use_part_count_limit = true;
  };
  virtual Result<FileInfo> init() TD_WARN_UNUSED_RESULT = 0;
  virtual Status on_ok(int64 size) TD_WARN_UNUSED_RESULT = 0;
  virtual void on_error(Status status) = 0;
  virtual Status before_start_parts() {
    return Status::OK();
  }
  virtual Result<std::pair<NetQueryPtr, bool>> start_part(Part part, int part_count) TD_WARN_UNUSED_RESULT = 0;
  virtual void after_start_parts() {
  }
  virtual Result<size_t> process_part(Part part, NetQueryPtr net_query) TD_WARN_UNUSED_RESULT = 0;
  virtual void on_progress(int32 part_count, int32 part_size, int32 ready_part_count, bool is_ready,
                           int64 ready_size) = 0;
  virtual Callback *get_callback() = 0;
  virtual Result<PrefixInfo> on_update_local_location(const LocalFileLocation &location) TD_WARN_UNUSED_RESULT {
    return Status::Error("unsupported");
  }
  virtual Result<bool> should_restart_part(Part part, NetQueryPtr &net_query) TD_WARN_UNUSED_RESULT {
    return false;
  }

  virtual Status process_check_query(NetQueryPtr net_query) {
    return Status::Error("unsupported");
  }
  struct CheckInfo {
    bool need_check{false};
    int64 checked_prefix_size{0};
    std::vector<NetQueryPtr> queries;
  };
  virtual Result<CheckInfo> check_loop(int64 checked_prefix_size, int64 ready_prefix_size, bool is_ready) {
    return CheckInfo{};
  }

  virtual void keep_fd_flag(bool keep_fd) {
  }

 private:
  enum { CommonQueryKey = 2 };
  bool stop_flag_ = false;
  ActorShared<ResourceManager> resource_manager_;
  ResourceState resource_state_;
  PartsManager parts_manager_;
  uint64 blocking_id_{0};
  std::map<uint64, std::pair<Part, ActorShared<>>> part_map_;
  // std::map<uint64, std::pair<Part, NetQueryRef>> part_map_;
  bool ordered_flag_ = false;
  OrderedEventsProcessor<std::pair<Part, NetQueryPtr>> ordered_parts_;

  void start_up() override;
  void loop() override;
  Status do_loop();
  void hangup() override;
  void tear_down() override;

  void update_estimated_limit();
  void on_progress_impl(size_t size);

  void on_result(NetQueryPtr query) override;
  void on_part_query(Part part, NetQueryPtr query);
  void on_common_query(NetQueryPtr query);
  Status try_on_part_query(Part part, NetQueryPtr query);
};
}  // namespace td
