#pragma once

#include <signal.h>
#include <unistd.h>

#include "log.hpp"
#include "service.h"

namespace Core {
typedef void (*signalHandler)(int signalNo);
class Signal {
 public:
  static void SignalDealReg() {
    signalDeal(SIGCHLD, SIG_IGN);  //子进程退出信号时，触发的信号，这里直接忽略这个信号，子进程相关资源被释放
    signalDeal(SIGTERM, signalExit);        // kill进程时，触发的信号
    signalDeal(SIGINT, signalExit);         //进程前台运行时，按ctrl + C触发的信号
    signalDeal(SIGQUIT, signalExit);        //进程前台运行时，按ctrl + \触发的信号
    signalDeal(SIGHUP, signalExit);         //关联终端退出时，触发的信号
    signalDeal(SIGPIPE, signalPipeBroken);  //发生管道错误时，触发的信号
  }

 private:
  static void signalExit(int signalNo) {
    SERVICE.RequestStop();
  }
  static void signalPipeBroken(int signalNo) { WARN("pipe broken happen"); }
  static void signalDeal(int signalNo, signalHandler handler) {
    struct sigaction act;
    act.sa_handler = handler;   //设置信号处理函数
    sigemptyset(&act.sa_mask);  //信号屏蔽设置为空
    act.sa_flags = 0;           //标志位设置为0
    assert(0 == sigaction(signalNo, &act, NULL));
  }
};
}  // namespace Core
