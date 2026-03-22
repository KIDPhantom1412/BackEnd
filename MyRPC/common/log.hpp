#pragma once
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "robustio.hpp"
#include "singleton.hpp"
#include "strings.hpp"
#include "timedeal.hpp"
#include "utils.hpp"

namespace Common {
enum LogLevel {  //日志输出级别
  LEVEL_TRACE = 0,
  LEVEL_DEBUG = 1,
  LEVEL_INFO = 2,
  LEVEL_WARN = 3,
  LEVEL_ERROR = 4,
};
constexpr int32_t logMsgBytes = 1024;

class Logger {
 public:
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(const Logger&&) = delete;
  Logger& operator=(const Logger&&) = delete;
  Logger() {
    std::string programName = Utils::GetSelfName();
    const char *cStr = programName.c_str();
    std::string fileName = Strings::StrFormat((char *)"/home/backend/log/%s/%s.log", cStr, cStr);
    fd_ = open(fileName.c_str(), O_APPEND | O_CREAT | O_WRONLY,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);  //追加写的方式打开文件
    assert(fd_ >= 0);
  }
  ~Logger() { Shutdown(); }
  void Shutdown() {
    bool expect = false;
    if (!shutdown_.compare_exchange_strong(expect, true)) return;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      exit_ = true;
      isAsync_.store(false, std::memory_order_release);
    }
    condVar_.notify_one();
    if (thread_.joinable()) {
      thread_.join();
    }
    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }
  }

  void SetLevel(LogLevel level) { level_ = level; }
  void Log(std::string logId, LogLevel level, char *format, ...) {
    if (shutdown_.load(std::memory_order_acquire)) return;
    if (level < level_) return;
    int32_t ret = 0;
    static thread_local struct Buffer {
      char* data;
      Buffer() { data = (char*)malloc(logMsgBytes); }
      ~Buffer() { free(data); }
    } buf;
    va_list plist;
    va_start(plist, format);
    ret = vsnprintf(buf.data, logMsgBytes, format, plist);
    va_end(plist);
    assert(ret > 0);
    if (ret >= logMsgBytes) {  //缓冲区长度不足，需要重新分配内存
      buf.data = (char *)realloc(buf.data, ret + 1);
      va_start(plist, format);
      ret = vsnprintf(buf.data, ret + 1, format, plist);
      va_end(plist);
    }
    if (logId == "") {
      logId = GetLogId();
    }
    std::string timeStr = TimeFormat::GetTimeStr("%F %T", true);
    std::string logMsg =
        levelStr(level) + " " + timeStr + " " + std::to_string(getpid()) + "," + logId + " " + buf.data + "\n";
    if (!isAsync_.load(std::memory_order_acquire)) {
      std::lock_guard<std::mutex> lock(mtx_);
      if (exit_ || fd_ < 0) return;
      static RobustIo io(fd_);
      io.Write((uint8_t *)logMsg.data(), logMsg.size());
    } else {
      bool needNotify = false;
      {
        std::lock_guard<std::mutex> lock(mtx_);
        if (exit_) return;
        queue_.push(std::move(logMsg));
        if (queue_.size() == 1 || queue_.size() > 100) needNotify = true;
      }
      if (needNotify) condVar_.notify_one();
    }
  }
  static std::string GetLogId() {
    static std::string ip = Common::Utils::GetIpStr("eth0");  //默认取eth0的ip
    std::string curTime = TimeFormat::GetTimeStr("%Y%m%d%H%M%S");
    static std::atomic<uint64_t> seq{0};
    return curTime + ip + std::to_string(seq.fetch_add(1, std::memory_order_relaxed) % 1000000);
  }
  void EnableAsync() {
    if (shutdown_.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lock(mtx_);
    if (!thread_.joinable()) {
      thread_ = std::thread(&Logger::process, this);
    }
    isAsync_.store(true, std::memory_order_release);
  }

 private:
  std::string levelStr(LogLevel level) {
    if (LEVEL_TRACE == level) return "[TRACE]";
    if (LEVEL_DEBUG == level) return "[DEBUG]";
    if (LEVEL_INFO == level) return "[INFO]";
    if (LEVEL_WARN == level) return "[WARN]";
    if (LEVEL_ERROR == level) return "[ERROR]";
    return "UNKNOWN";
  }
  void process() {
    std::unique_lock<std::mutex> lock(mtx_);
    std::queue<std::string> localQueue;
    while (true) {
      if (!exit_) {
        if (queue_.empty()) {
          condVar_.wait(lock);
        }
      } else {
        writeLogQueue(queue_);
        break;
      }
      localQueue.swap(queue_);
      lock.unlock();
      writeLogQueue(localQueue);
      lock.lock();
    }
  }
  void writeLogQueue(std::queue<std::string>& queue) const {
    static RobustIo io(fd_);
    while (!queue.empty()) {
        std::string& logMsg = queue.front();
        io.Write((uint8_t *)logMsg.data(), logMsg.size());
        queue.pop();
    }
  }

  private:
    int fd_{-1};
    LogLevel level_{LEVEL_TRACE};
    bool exit_{false};
    std::atomic<bool> isAsync_{false};
    std::atomic<bool> shutdown_{false};
    std::queue<std::string> queue_;
    std::mutex mtx_;
    std::condition_variable condVar_;
    std::thread thread_;
};
}  // namespace Common

#define LOGGER Common::Singleton<Common::Logger>::Instance()
#define FILENAME(x) strrchr(x, '/') ? strrchr(x, '/') + 1 : x
#define TRACE(format, ...)                                                                                      \
  LOGGER.Log("", Common::LEVEL_TRACE, (char *)"(%s:%s:%d):" format, FILENAME(__FILE__), __FUNCTION__, __LINE__, \
             ##__VA_ARGS__)
#define DEBUG(format, ...)                                                                                      \
  LOGGER.Log("", Common::LEVEL_DEBUG, (char *)"(%s:%s:%d):" format, FILENAME(__FILE__), __FUNCTION__, __LINE__, \
             ##__VA_ARGS__)
#define INFO(format, ...)                                                                                      \
  LOGGER.Log("", Common::LEVEL_INFO, (char *)"(%s:%s:%d):" format, FILENAME(__FILE__), __FUNCTION__, __LINE__, \
             ##__VA_ARGS__)
#define WARN(format, ...)                                                                                      \
  LOGGER.Log("", Common::LEVEL_WARN, (char *)"(%s:%s:%d):" format, FILENAME(__FILE__), __FUNCTION__, __LINE__, \
             ##__VA_ARGS__)
#define ERROR(format, ...)                                                                                      \
  LOGGER.Log("", Common::LEVEL_ERROR, (char *)"(%s:%s:%d):" format, FILENAME(__FILE__), __FUNCTION__, __LINE__, \
             ##__VA_ARGS__)
#define CTX_TRACE(ctx, format, ...)                                              \
  LOGGER.Log(ctx.log_id(), Common::LEVEL_TRACE, (char *)"(%d:%s:%s:%d):" format, \
             MyCoroutine::ScheduleGetRunCid(SCHEDULE), FILENAME(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CTX_DEBUG(ctx, format, ...)                                              \
  LOGGER.Log(ctx.log_id(), Common::LEVEL_DEBUG, (char *)"(%d:%s:%s:%d):" format, \
             MyCoroutine::ScheduleGetRunCid(SCHEDULE), FILENAME(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CTX_INFO(ctx, format, ...)                                              \
  LOGGER.Log(ctx.log_id(), Common::LEVEL_INFO, (char *)"(%d:%s:%s:%d):" format, \
             MyCoroutine::ScheduleGetRunCid(SCHEDULE), FILENAME(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CTX_WARN(ctx, format, ...)                                              \
  LOGGER.Log(ctx.log_id(), Common::LEVEL_WARN, (char *)"(%d:%s:%s:%d):" format, \
             MyCoroutine::ScheduleGetRunCid(SCHEDULE), FILENAME(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define CTX_ERROR(ctx, format, ...)                                              \
  LOGGER.Log(ctx.log_id(), Common::LEVEL_ERROR, (char *)"(%d:%s:%s:%d):" format, \
             MyCoroutine::ScheduleGetRunCid(SCHEDULE), FILENAME(__FILE__), __FUNCTION__, __LINE__, ##__VA_ARGS__)
