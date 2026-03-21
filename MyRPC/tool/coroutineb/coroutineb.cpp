#include <iostream>
#include <sys/time.h>
#include <unistd.h>

#include "../../common/cmdline.h"
#include "../../core/coroutine.h"

using namespace std;

#define GREEN_BEGIN "\033[32m"
#define RED_BEGIN "\033[31m"
#define COLOR_END "\033[0m"

int64_t concurrency = 10000;  // 默认并发创建和调度10000个协程
int64_t yieldTimes = 10;      // 默认每个协程yield 10次
int64_t rounds = 10;          // 默认综合压测10轮
int64_t warmupRounds = 3;

// 统计时间消耗（单位：毫秒）
int getSpendMs(timeval begin, timeval end) {
  end.tv_sec -= begin.tv_sec;
  end.tv_usec -= begin.tv_usec;
  if (end.tv_usec < 0) {
    end.tv_sec -= 1;
    end.tv_usec += 1000000;
  }
  return end.tv_sec * 1000 + end.tv_usec / 1000;
}

// 统计时间消耗（单位：微秒）
int getSpendUs(timeval begin, timeval end) {
  end.tv_sec -= begin.tv_sec;
  end.tv_usec -= begin.tv_usec;
  if (end.tv_usec < 0) {
    end.tv_sec -= 1;
    end.tv_usec += 1000000;
  }
  return end.tv_sec * 1000000 + end.tv_usec;
}

void CoroutineFunc(void* arg) {
  int64_t yield_count = *(int64_t*)arg;
  for (int64_t i = 0; i < yield_count; i++) {
    MyCoroutine::CoroutineYield(SCHEDULE);
  }
}

void usage() {
  cout << "coroutineb -c 10000 -y 10 -r 10 -w 3" << endl;
  cout << "options:" << endl;
  cout << "    -h,--help     print usage" << endl;
  cout << "    -c            benchmark concurrency (number of coroutines, default 10000)" << endl;
  cout << "    -y            yield times per coroutine (default 10)" << endl;
  cout << "    -r            benchmark rounds for full flow test (default 10)" << endl;
  cout << "    -w            warmup rounds for full flow test (default 3)" << endl;
  cout << endl;
}

void createCoroutines() {
  for (int64_t i = 0; i < concurrency; i++) {
    int cid = MyCoroutine::CoroutineCreate(SCHEDULE, CoroutineFunc, &yieldTimes);
    if (cid == MyCoroutine::INVALID_ROUTINE_ID) {
      cout << RED_BEGIN << "Create coroutine failed at index " << i << COLOR_END << endl;
      exit(-1);
    }
  }
}

int64_t runSchedule() {
  int64_t totalResumeCount = 0;
  while (MyCoroutine::CoroutineResume(SCHEDULE) == MyCoroutine::Success) {
    totalResumeCount++;
  }
  return totalResumeCount;
}

double safeAvg(double numerator, int64_t denominator) {
  if (denominator <= 0) return 0;
  return numerator / denominator;
}

void execBenchMark() {
  if (concurrency <= 0 || yieldTimes < 0 || rounds <= 0 || warmupRounds < 0) {
    cout << RED_BEGIN << "Invalid args: c > 0, y >= 0, r > 0, w >= 0" << COLOR_END << endl;
    exit(-1);
  }
  if (concurrency > MyCoroutine::MAX_COROUTINE_SIZE) {
    cout << RED_BEGIN << "concurrency too large, max = " << MyCoroutine::MAX_COROUTINE_SIZE << COLOR_END << endl;
    exit(-1);
  }

  cout << GREEN_BEGIN << "Start coroutine benchmark..." << COLOR_END << endl;
  cout << "Concurrency (coroutines count): " << concurrency << endl;
  cout << "Yield times per coroutine: " << yieldTimes << endl;
  cout << "Full flow rounds: " << rounds << endl;
  cout << "Warmup rounds: " << warmupRounds << endl;
  cout << "Total Resume operations expected: " << concurrency * (yieldTimes + 1) << endl;
  cout << "----------------------------------------" << endl;

  MyCoroutine::ScheduleInit(SCHEDULE, (int)concurrency, 8 * 1024);
  for (int64_t i = 0; i < warmupRounds; i++) {
    createCoroutines();
    runSchedule();
  }

  timeval begin, end;

  gettimeofday(&begin, NULL);
  createCoroutines();
  gettimeofday(&end, NULL);
  int createSpendUs = getSpendUs(begin, end);
  cout << "1. Create " << concurrency << " coroutines spend: " << createSpendUs / 1000.0 << " ms." << endl;
  cout << "   Avg create time per coroutine: " << safeAvg((double)createSpendUs, concurrency) << " us." << endl;

  gettimeofday(&begin, NULL);
  int64_t totalResumeCount = runSchedule();
  gettimeofday(&end, NULL);
  int scheduleSpendUs = getSpendUs(begin, end);
  cout << "2. Schedule (Resume/Yield) " << totalResumeCount << " times spend: " << scheduleSpendUs / 1000.0 << " ms."
       << endl;
  cout << "   Avg schedule time per operation: " << safeAvg((double)scheduleSpendUs, totalResumeCount) << " us."
       << endl;
  cout << "   QPS (Operations per second): "
       << (int64_t)(safeAvg((double)totalResumeCount * 1000000, scheduleSpendUs)) << endl;

  int64_t fullFlowTotalResumeCount = 0;
  int64_t fullFlowTotalCoroutineCount = 0;
  gettimeofday(&begin, NULL);
  for (int64_t i = 0; i < rounds; i++) {
    createCoroutines();
    fullFlowTotalResumeCount += runSchedule();
    fullFlowTotalCoroutineCount += concurrency;
  }
  gettimeofday(&end, NULL);
  int fullFlowSpendUs = getSpendUs(begin, end);
  int fullFlowSpendMs = getSpendMs(begin, end);

  cout << "3. FullFlow(Create->Schedule, HotPool) rounds: " << rounds << ", total spend: " << fullFlowSpendMs
       << " ms." << endl;
  cout << "   Total coroutines: " << fullFlowTotalCoroutineCount
       << ", Create+Schedule QPS: "
       << (int64_t)(safeAvg((double)fullFlowTotalCoroutineCount * 1000000, fullFlowSpendUs))
       << endl;
  cout << "   Total schedule operations: " << fullFlowTotalResumeCount
       << ", Schedule QPS in full flow: "
       << (int64_t)(safeAvg((double)fullFlowTotalResumeCount * 1000000, fullFlowSpendUs)) << endl;

  MyCoroutine::ScheduleClean(SCHEDULE);
  cout << GREEN_BEGIN << "Benchmark finished successfully." << COLOR_END << endl;
}

int main(int argc, char* argv[]) {
  Common::CmdLine::Int64Opt(&concurrency, "c", 10000);
  Common::CmdLine::Int64Opt(&yieldTimes, "y", 10);
  Common::CmdLine::Int64Opt(&rounds, "r", 10);
  Common::CmdLine::Int64Opt(&warmupRounds, "w", 3);
  Common::CmdLine::SetUsage(usage);
  Common::CmdLine::Parse(argc, argv);
  
  execBenchMark();
  
  return 0;
}
