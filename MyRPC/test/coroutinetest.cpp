#include "../core/coroutine.h"
#include "unittestcore.h"
#include <vector>
#include <iostream>
#include <cassert>

using namespace MyCoroutine;

static std::vector<int> g_run_order;

static void CoroutineFuncBasic(void* arg) {
  int id = *(int*)arg;
  g_run_order.push_back(id);
  CoroutineYield(SCHEDULE);
  g_run_order.push_back(id * 10);
  std::cout << "finish CoroutineFuncBasic, id = " << id << std::endl;
}

// 1. 测试基础调度与挂起/恢复
TEST_CASE(Coroutine_Basic) {
  g_run_order.clear();
  ScheduleInit(SCHEDULE, 100);

  int id1 = 1, id2 = 2;
  int cid1 = CoroutineCreate(SCHEDULE, CoroutineFuncBasic, &id1);
  int cid2 = CoroutineCreate(SCHEDULE, CoroutineFuncBasic, &id2);
  
  ASSERT_NE(cid1, INVALID_ROUTINE_ID);
  ASSERT_NE(cid2, INVALID_ROUTINE_ID);

  CoroutineResume(SCHEDULE); // 执行cid1到yield
  CoroutineResume(SCHEDULE); // 执行cid2到yield
  CoroutineResume(SCHEDULE); // 执行cid1到结束
  CoroutineResume(SCHEDULE); // 执行cid2到结束

  ASSERT_EQ(g_run_order.size(), 4);
  ASSERT_EQ(g_run_order[0], 1);
  ASSERT_EQ(g_run_order[1], 2);
  ASSERT_EQ(g_run_order[2], 10);
  ASSERT_EQ(g_run_order[3], 20);

  ScheduleClean(SCHEDULE);
}

// 2. 测试基于优先队列的优先级调度 (Priority)
TEST_CASE(Coroutine_Priority) {
  g_run_order.clear();
  ScheduleInit(SCHEDULE, 100);

  int id1 = 1, id2 = 2, id3 = 3;
  // 优先级数值越小，优先级越高
  CoroutineCreate(SCHEDULE, CoroutineFuncBasic, &id1, 10);
  CoroutineCreate(SCHEDULE, CoroutineFuncBasic, &id2, 5);
  CoroutineCreate(SCHEDULE, CoroutineFuncBasic, &id3, 20);

  // 队列初始状态：
  // id2 (priority=5, sequence=1)
  // id1 (priority=10, sequence=1)
  // id3 (priority=20, sequence=1)

  // 第一次 Resume 应该执行优先级最高的，即 priority = 5 的 id2
  // id2 运行，将 2 push 进 g_run_order，然后 Yield
  CoroutineResume(SCHEDULE);
  ASSERT_EQ(g_run_order.back(), 2);
  
  // Yield 之后，id2 会被重新推入优先队列。
  // 此时队列中有：
  // id2 (priority=5, sequence=2)  <-- 依然是优先级最高的！
  // id1 (priority=10, sequence=1)
  // id3 (priority=20, sequence=1)

  // 所以第二次 Resume，依然会调度 id2！
  // id2 会从 Yield 处恢复，将 2 * 10 = 20 push 进 g_run_order，然后执行完毕结束。
  CoroutineResume(SCHEDULE);
  ASSERT_EQ(g_run_order.back(), 20);

  // 此时 id2 已经出队并结束。队列中剩下：
  // id1 (priority=10, sequence=1)
  // id3 (priority=20, sequence=1)
  // 第三次 Resume 会调度优先级较高的 id1
  // id1 运行，将 1 push 进 g_run_order，然后 Yield
  CoroutineResume(SCHEDULE);
  ASSERT_EQ(g_run_order.back(), 1);

  // Yield 之后，id1 被重新推入队列。
  // 队列中有：
  // id1 (priority=10, sequence=2) <-- 依然比 id3 优先级高
  // id3 (priority=20, sequence=1)
  // 第四次 Resume，依然调度 id1
  // id1 从 Yield 处恢复，将 1 * 10 = 10 push 进 g_run_order，然后执行完毕。
  CoroutineResume(SCHEDULE); 
  ASSERT_EQ(g_run_order.back(), 10);

  // 第五次 Resume，调度仅剩的 id3
  // id3 运行，将 3 push 进 g_run_order，然后 Yield
  CoroutineResume(SCHEDULE); 
  ASSERT_EQ(g_run_order.back(), 3);

  // 第六次 Resume，唤醒 id3 完成剩余工作
  CoroutineResume(SCHEDULE); 
  ASSERT_EQ(g_run_order.back(), 30);

  ScheduleClean(SCHEDULE);
}

// 3. 测试协程池复用时的"懒删除"及 sequence 校验机制
static void CoroutineFuncEmpty(void* arg) {
  std::cout << "finish CoroutineFuncEmpty" << std::endl;
}

TEST_CASE(Coroutine_LazyDeletion) {
  ScheduleInit(SCHEDULE, 10);

  // 创建协程 1
  int id1 = 1;
  int cid = CoroutineCreate(SCHEDULE, CoroutineFuncBasic, &id1);
  
  // 让协程 1 运行并 yield，此时它在优先队列中 (sequence = 1)
  CoroutineResume(SCHEDULE);

  // 我们强制用 CoroutineResumeById 把它唤醒执行完
  CoroutineResumeById(SCHEDULE, cid);
  
  // 此时 cid 这个协程已经变为 Idle 状态，被放回了 idleQueue（通过 push_back 放在了队尾）
  // 但由于优先队列不支持随机删除，队列里还有一个 sequence=1 的废弃节点。

  // 为了触发复用，我们需要把 idleQueue 里面排在它前面的其他空闲协程全都消耗掉
  // 因为 schedule.coroutineCnt 是 10，目前只用了一个（且放回了队尾），我们需要创建 9 个协程才能再次拿到 cid
  std::vector<int> dummy_cids;
  for (int i = 0; i < 9; i++) {
    int dummy_cid = CoroutineCreate(SCHEDULE, CoroutineFuncEmpty, nullptr);
    dummy_cids.push_back(dummy_cid);
  }

  // 再次创建协程，此时一定会复用我们刚才的 cid，这会让该协程 sequence 变成 2
  int cid2 = CoroutineCreate(SCHEDULE, CoroutineFuncEmpty, nullptr);
  ASSERT_EQ(cid, cid2); // 验证的确发生了复用

  // 此时如果我们调用 CoroutineResume，调度器应该能通过 sequence 和状态识别出那个废弃节点并丢弃，
  // 而不是错误地去调度或者打乱顺序。它会正确调度 sequence=2 的节点（或者其他的协程，但不会崩溃）。
  int ret = CoroutineResume(SCHEDULE);
  ASSERT_EQ(ret, Success);

  // 我们刚才创建了 9 个 dummy 协程 + 1 个复用的 cid2 协程，总共 10 个协程
  // 加上上面已经 Resume 了一次，所以还需要再 Resume 9 次才能全部执行完
  int resume_count = 1;
  while ((ret = CoroutineResume(SCHEDULE)) == Success) {
    resume_count++;
  }
  
  // 验证确实所有的协程都被成功调度执行了
  ASSERT_EQ(resume_count, 10);
  // 验证最后一次 Resume 返回了 NotRunnable (表示队列已空)
  ASSERT_EQ(ret, NotRunnable);

  ScheduleClean(SCHEDULE);
}

// 4. 测试 isInsertBatch 逻辑 (Batch 优先级的降级处理)
static void CoroutineFuncBatchInsert(void* arg) {
  int id = *(int*)arg;
  g_run_order.push_back(id);
  // 模拟被插入了 batch，这会设置 isInsertBatch = true
  int batchId = BatchInit(SCHEDULE);
  // 模拟发起了一个子协程任务，如果不加子协程，BatchRun会因为没有子协程立刻结束卡点
  BatchAdd(SCHEDULE, batchId, CoroutineFuncEmpty, nullptr);
  BatchRun(SCHEDULE, batchId); // 这里会内部 yield 等待 batch 跑完
  std::cout << "finish CoroutineFuncBatchInsert, id = " << id << std::endl;
}

TEST_CASE(Coroutine_BatchPriority) {
  g_run_order.clear();
  ScheduleInit(SCHEDULE, 100);

  int id1 = 1, id2 = 2;
  // id1 优先级虽然为 0（最高），但是在运行中它将被标记为 isInsertBatch = true
  CoroutineCreate(SCHEDULE, CoroutineFuncBatchInsert, &id1, 0);
  // id2 优先级为 10，没有 batch 卡点
  CoroutineCreate(SCHEDULE, CoroutineFuncBasic, &id2, 10);

  // 第一次 Resume，id1 (因为还没标记 batch，此时它优先级最高) 运行。
  // 它内部调用了 BatchAdd 和 BatchRun，因此它被挂起了 (isInsertBatch=true)
  // 并且队列里多出了一个 priority=0 的空协程 (Batch里的子协程)
  CoroutineResume(SCHEDULE);
  ASSERT_EQ(g_run_order.back(), 1);

  // 第二次 Resume，此时队列中有：
  // cid2 (isInsertBatch=false, priority=10)
  // cid1的子协程 (isInsertBatch=false, priority=0)
  // cid1 (isInsertBatch=true, priority=0)
  // 注意，子协程继承了父协程的优先级0且没被插入batch，所以子协程会被优先执行！
  CoroutineResume(SCHEDULE);

  // 第三次 Resume，此时队列中有：
  // cid2 (isInsertBatch=false, priority=10)
  // cid1 (isInsertBatch=true, priority=0)
  // 此时 cid1 虽然优先级数字小(0)，但被标记了 isInsertBatch=true
  // 根据我们的比较器，无卡点的 cid2 (priority=10) 会越级调度！
  CoroutineResume(SCHEDULE); // 唤醒 cid2，它运行并 yield
  ASSERT_EQ(g_run_order.back(), 2);

  // 第四次 Resume，唤醒 cid2 继续执行完毕
  CoroutineResume(SCHEDULE);
  ASSERT_EQ(g_run_order.back(), 20); // id2 唤醒后输出 id * 10

  // 此时子协程执行完了，我们需要调用 CoroutineResumeBatchFinish
  // 注意：CoroutineResumeBatchFinish 内部会自动调用 CoroutineResumeById 把满足条件的 cid1 唤醒并执行！
  // 所以这里调用完之后，cid1 就会收尾并打印结束日志。
  CoroutineResumeBatchFinish(SCHEDULE);

  ScheduleClean(SCHEDULE);
}

// 5. 测试本地变量存取功能 (LocalData)
static void freeLocalData(void* data) {
  delete (int*)data;
}

static void CoroutineFuncLocal(void* arg) {
  LocalData ld;
  ld.data = new int(12345);
  ld.freeEntry = freeLocalData;
  CoroutineLocalSet(SCHEDULE, (void*)1, ld);

  LocalData ld_get;
  bool exist = CoroutineLocalGet(SCHEDULE, (void*)1, ld_get);
  assert(exist);
  
  if (exist && ld_get.data) {
    g_run_order.push_back(*(int*)ld_get.data);
  }
  std::cout << "finish CoroutineFuncLocal" << std::endl;
}

TEST_CASE(Coroutine_LocalData) {
  g_run_order.clear();
  ScheduleInit(SCHEDULE, 100);
  CoroutineCreate(SCHEDULE, CoroutineFuncLocal, nullptr);
  CoroutineResume(SCHEDULE);
  
  ASSERT_EQ(g_run_order.size(), 1);
  ASSERT_EQ(g_run_order.back(), 12345);
  
  ScheduleClean(SCHEDULE); // 退出时将自动触发 freeLocalData
}
