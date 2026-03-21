#include <iostream>
#include <iomanip>
#include <limits>
#include <vector>
#include <sys/time.h>
#include <unistd.h>

#include "../../common/cmdline.h"
#include "../../core/coroutine.h"

using namespace std;

#define GREEN_BEGIN "\033[32m"
#define RED_BEGIN "\033[31m"
#define COLOR_END "\033[0m"

int64_t concurrency = 50000;
int64_t yieldTimes = 10;
int64_t rounds = 10;
int64_t warmupRounds = 3;
int64_t createRounds = 5;
vector<uint32_t> priorityRing;
size_t priorityRingIndex = 0;

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
  cout << "coroutineb -c 50000 -y 10 -r 10 -w 3 -cr 5" << endl;
  cout << "options:" << endl;
  cout << "    -h,--help     print usage" << endl;
  cout << "    -c            benchmark concurrency (number of coroutines, default 50000)" << endl;
  cout << "    -y            yield times per coroutine (default 10)" << endl;
  cout << "    -r            benchmark rounds for schedule/full-flow test (default 10)" << endl;
  cout << "    -w            warmup rounds for full flow test (default 3)" << endl;
  cout << "    -cr           benchmark rounds for cold-create test (default 5)" << endl;
  cout << endl;
}

void createCoroutines() {
  for (int64_t i = 0; i < concurrency; i++) {
    uint32_t priority = priorityRing[priorityRingIndex];
    priorityRingIndex++;
    if (priorityRingIndex >= priorityRing.size()) {
      priorityRingIndex = 0;
    }
    int cid = MyCoroutine::CoroutineCreate(SCHEDULE, CoroutineFunc, &yieldTimes, priority);
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

typedef struct StatAgg {
  int64_t minUs{std::numeric_limits<int64_t>::max()};
  int64_t maxUs{0};
  int64_t sumUs{0};
  int64_t rounds{0};
} StatAgg;

void statUpdate(StatAgg& stat, int64_t us) {
  if (us < stat.minUs) stat.minUs = us;
  if (us > stat.maxUs) stat.maxUs = us;
  stat.sumUs += us;
  stat.rounds++;
}

double usToMs(double us) { return us / 1000.0; }

void initPriorityRing() {
  size_t ringSize = (size_t)concurrency;
  if (ringSize < 1024) ringSize = 1024;
  priorityRing.clear();
  priorityRing.reserve(ringSize);
  uint32_t seed = 0x9E3779B9u;
  for (size_t i = 0; i < ringSize; i++) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    priorityRing.push_back(seed);
  }
  priorityRingIndex = 0;
}

void printTableHead() {
  cout << "+--------------------------------------+--------+----------+----------+----------+------------+" << endl;
  cout << "| Metric                               | Rounds | Min(ms)  | Avg(ms)  | Max(ms)  | QPS        |" << endl;
  cout << "+--------------------------------------+--------+----------+----------+----------+------------+" << endl;
}

void printTableRow(const string& metric, int64_t roundsValue, double minMs, double avgMs, double maxMs, int64_t qps) {
  cout << "| " << left << setw(36) << metric << " | " << right << setw(6) << roundsValue << " | " << fixed
       << setprecision(3) << setw(8) << minMs << " | " << setw(8) << avgMs << " | " << setw(8) << maxMs << " | "
       << setw(10) << qps << " |" << endl;
}

void printTableTail() {
  cout << "+--------------------------------------+--------+----------+----------+----------+------------+" << endl;
}

void execBenchMark() {
  if (concurrency <= 0 || yieldTimes < 0 || rounds <= 0 || warmupRounds < 0 || createRounds <= 0) {
    cout << RED_BEGIN << "Invalid args: c > 0, y >= 0, r > 0, w >= 0, cr > 0" << COLOR_END << endl;
    exit(-1);
  }
  if (concurrency > MyCoroutine::MAX_COROUTINE_SIZE) {
    cout << RED_BEGIN << "concurrency too large, max = " << MyCoroutine::MAX_COROUTINE_SIZE << COLOR_END << endl;
    exit(-1);
  }
  initPriorityRing();

  cout << GREEN_BEGIN << "Start coroutine benchmark..." << COLOR_END << endl;
  cout << "Concurrency (coroutines count): " << concurrency << endl;
  cout << "Yield times per coroutine: " << yieldTimes << endl;
  cout << "Full flow rounds: " << rounds << endl;
  cout << "Warmup rounds: " << warmupRounds << endl;
  cout << "Cold create rounds: " << createRounds << endl;
  cout << "Total Resume operations expected: " << concurrency * (yieldTimes + 1) << endl;
  cout << "----------------------------------------" << endl;

  timeval begin, end;

  StatAgg coldCreateStat;
  for (int64_t i = 0; i < createRounds; i++) {
    MyCoroutine::ScheduleInit(SCHEDULE, (int)concurrency, 8 * 1024);
    gettimeofday(&begin, NULL);
    createCoroutines();
    gettimeofday(&end, NULL);
    statUpdate(coldCreateStat, getSpendUs(begin, end));
    MyCoroutine::ScheduleClean(SCHEDULE);
  }

  MyCoroutine::ScheduleInit(SCHEDULE, (int)concurrency, 8 * 1024);
  for (int64_t i = 0; i < warmupRounds; i++) {
    createCoroutines();
    runSchedule();
  }

  StatAgg hotCreateStat;
  for (int64_t i = 0; i < rounds; i++) {
    gettimeofday(&begin, NULL);
    createCoroutines();
    gettimeofday(&end, NULL);
    statUpdate(hotCreateStat, getSpendUs(begin, end));
    runSchedule();
  }

  StatAgg scheduleStat;
  int64_t scheduleTotalOps = 0;
  for (int64_t i = 0; i < rounds; i++) {
    createCoroutines();
    gettimeofday(&begin, NULL);
    int64_t ops = runSchedule();
    gettimeofday(&end, NULL);
    statUpdate(scheduleStat, getSpendUs(begin, end));
    scheduleTotalOps += ops;
  }

  StatAgg fullFlowStat;
  int64_t fullFlowTotalOps = 0;
  int64_t fullFlowTotalCoroutineCount = 0;
  for (int64_t i = 0; i < rounds; i++) {
    gettimeofday(&begin, NULL);
    createCoroutines();
    int64_t ops = runSchedule();
    gettimeofday(&end, NULL);
    statUpdate(fullFlowStat, getSpendUs(begin, end));
    fullFlowTotalOps += ops;
    fullFlowTotalCoroutineCount += concurrency;
  }

  printTableHead();
  int64_t coldCreateQps = (int64_t)safeAvg((double)createRounds * concurrency * 1000000, coldCreateStat.sumUs);
  printTableRow(
    "ColdCreate(with ScheduleClean)",
    createRounds,
    usToMs((double)coldCreateStat.minUs),
    usToMs(safeAvg((double)coldCreateStat.sumUs, coldCreateStat.rounds)),
    usToMs((double)coldCreateStat.maxUs),
    coldCreateQps
  );
  int64_t hotCreateQps = (int64_t)safeAvg((double)rounds * concurrency * 1000000, hotCreateStat.sumUs);
  printTableRow(
    "HotCreateOnly(HotPool)",
    rounds,
    usToMs((double)hotCreateStat.minUs),
    usToMs(safeAvg((double)hotCreateStat.sumUs, hotCreateStat.rounds)),
    usToMs((double)hotCreateStat.maxUs),
    hotCreateQps
  );
  int64_t scheduleQps = (int64_t)safeAvg((double)scheduleTotalOps * 1000000, scheduleStat.sumUs);
  printTableRow(
    "ScheduleOnly(HotPool)",
    rounds,
    usToMs((double)scheduleStat.minUs),
    usToMs(safeAvg((double)scheduleStat.sumUs, scheduleStat.rounds)),
    usToMs((double)scheduleStat.maxUs),
    scheduleQps
  );
  int64_t fullFlowQps = (int64_t)safeAvg((double)fullFlowTotalCoroutineCount * 1000000, fullFlowStat.sumUs);
  printTableRow(
    "FullFlow(Create+Schedule, HotPool)",
    rounds,
    usToMs((double)fullFlowStat.minUs),
    usToMs(safeAvg((double)fullFlowStat.sumUs, fullFlowStat.rounds)),
    usToMs((double)fullFlowStat.maxUs),
    fullFlowQps
  );
  printTableTail();
  cout << "FullFlow schedule qps: " << (int64_t)safeAvg((double)fullFlowTotalOps * 1000000, fullFlowStat.sumUs)
       << endl;

  MyCoroutine::ScheduleClean(SCHEDULE);
  cout << GREEN_BEGIN << "Benchmark finished successfully." << COLOR_END << endl;
}

int main(int argc, char* argv[]) {
  Common::CmdLine::Int64Opt(&concurrency, "c", 50000);
  Common::CmdLine::Int64Opt(&yieldTimes, "y", 10);
  Common::CmdLine::Int64Opt(&rounds, "r", 10);
  Common::CmdLine::Int64Opt(&warmupRounds, "w", 3);
  Common::CmdLine::Int64Opt(&createRounds, "cr", 5);
  Common::CmdLine::SetUsage(usage);
  Common::CmdLine::Parse(argc, argv);
  
  execBenchMark();
  
  return 0;
}
