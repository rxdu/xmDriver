/*
 * io_service.hpp
 *
 * One shared asio io_context served by a single I/O thread for every async port
 * in the process (ADR 0002 / ADR 0003). Replaces the old thread-per-port model:
 * N ports no longer mean N io_contexts and N threads. Because a single thread
 * runs all completion handlers, per-port operations are already serialized with
 * respect to one another — a port posts its socket operations onto this context
 * (asio::post) and never touches a descriptor from a foreign thread.
 *
 * Lazily started on first use; the work guard keeps the thread alive until the
 * singleton is destroyed at program exit, when it stops the context and joins.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <thread>

#include "asio.hpp"

namespace xmotion {

class IoService {
 public:
  static IoService& Instance() {
    static IoService inst;
    return inst;
  }

  asio::io_context& context() { return io_; }

  IoService(const IoService&) = delete;
  IoService& operator=(const IoService&) = delete;

 private:
  IoService()
      : work_(asio::make_work_guard(io_)),
        thread_([this] { io_.run(); }) {}

  ~IoService() {
    work_.reset();  // let run() return once outstanding work drains
    io_.stop();     // stop promptly even if handlers are pending
    if (thread_.joinable()) thread_.join();
  }

  asio::io_context io_;
  asio::executor_work_guard<asio::io_context::executor_type> work_;
  std::thread thread_;
};

}  // namespace xmotion
