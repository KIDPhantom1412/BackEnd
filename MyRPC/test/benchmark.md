# Benchmark

我在vmware17.5的虚拟机中进行测试，测试环境如下：

```
宿主机：华硕天选4锐龙版（CPU为
虚拟机OS：CentOS7.9
虚拟机内存：6GB
虚拟机CPU：两核四线程
```

## 系统设置

在测试前实现对操作系统进行一些设置。

### 工具

安装perf，perf的版本为`perf version 3.10.0-1160.119.1.el7.x86_64.debug`。

安装debuginfo，

```bash
# 查看当前运行的 glibc 版本
rpm -qa glibc
# 查看安装的 debuginfo 版本
rpm -qa glibc-debuginfo
```

根据glibc的版本在下面的网站里面找。

http://debuginfo.centos.org/7/x86_64/

### 脚本

```bash
# --- 观测增强 ---
sudo sysctl -w kernel.perf_event_paranoid=0
sudo sysctl -w kernel.kptr_restrict=0
sudo sysctl -w kernel.perf_event_max_sample_rate=100000

# --- 网络吞吐 ---
sudo sysctl -w net.core.somaxconn=65535
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"
sudo sysctl -w net.ipv4.tcp_tw_reuse=1
sudo sysctl -w net.ipv4.tcp_fin_timeout=15
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=16384
sudo sysctl -w net.core.netdev_max_backlog=16384
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# --- 资源限制 ---
ulimit -n 1000000
```

### 虚拟机环境的设置

关闭NMI Watchdog：修改 `/etc/sysctl.conf`，在文件末尾添加以下行：`kernel.nmi_watchdog = 0`。

虚拟机环境下要开启 **“虚拟化 CPU 性能计数器” (Virtualize CPU performance counters)**方便perf采集。

## 协程测试

### 协程的正确性测试

在`MyRPC\test\readme.md`中我附加了如何正确的进行测试的流程。我添加了测试协程创建、调度、Batch等特性的测试代码`MyRPC\test\coroutinetest.cpp`。

### 协程压测

协程压测代码`MyRPC\tool\coroutineb`。

#### 优化前的协程

代码基于https://github.com/KIDPhantom1412/BackEnd

```bash
[root@localhost coroutineb]# ./coroutineb
Start coroutine benchmark...
Concurrency (coroutines count): 50000
Yield times per coroutine: 10
Full flow rounds: 10
Warmup rounds: 3
Cold create rounds: 5
Total Resume operations expected: 550000
----------------------------------------
+--------------------------------------+--------+----------+----------+----------+------------+
| Metric                               | Rounds | Min(ms)  | Avg(ms)  | Max(ms)  | QPS        |
+--------------------------------------+--------+----------+----------+----------+------------+
| ColdCreate(with ScheduleClean)       |      5 | 2372.140 | 2445.867 | 2497.538 |      20442 |
| HotCreateOnly(HotPool)               |     10 | 1998.330 | 2136.309 | 2237.203 |      23404 |
| ScheduleOnly(HotPool)                |     10 | 96784.272 | 102600.193 | 120085.109 |       5360 |
| FullFlow(Create+Schedule, HotPool)   |     10 | 103265.015 | 109008.519 | 120988.433 |        458 |
+--------------------------------------+--------+----------+----------+----------+------------+
FullFlow schedule qps: 5045
Benchmark finished successfully.
```

#### 优化后的协程

代码基于https://github.com/KIDPhantom1412/BackEnd/tree/refactor/coroutine

```bash
[root@localhost coroutineb]# ./coroutineb
Start coroutine benchmark...
Concurrency (coroutines count): 50000
Yield times per coroutine: 10
Full flow rounds: 10
Warmup rounds: 3
Cold create rounds: 5
Total Resume operations expected: 550000
----------------------------------------
+--------------------------------------+--------+----------+----------+----------+------------+
| Metric                               | Rounds | Min(ms)  | Avg(ms)  | Max(ms)  | QPS        |
+--------------------------------------+--------+----------+----------+----------+------------+
| ColdCreate(with ScheduleClean)       |      5 |   60.898 |   73.911 |   84.858 |     676485 |
| HotCreateOnly(HotPool)               |     10 |    7.487 |    9.377 |   12.557 |    5332480 |
| ScheduleOnly(HotPool)                |     10 |  108.934 |  120.769 |  129.165 |    4554133 |
| FullFlow(Create+Schedule, HotPool)   |     10 |  120.941 |  130.246 |  139.579 |     383888 |
+--------------------------------------+--------+----------+----------+----------+------------+
FullFlow schedule qps: 4222768
Benchmark finished successfully.
```

#### 测试结果分析

1. 线程创建`CoroutineCreate`：用空闲队列优化，时间复杂度从`O(N)`降低到`O(1)`；
2. 线程调度`CoroutineResume`：优先队列+懒删除优化，时间复杂度从`O(N)`降低到`O(logn)`；
3. 上下文切换：用fcontext替换ucontext，优化的大概时间详见：https://www.boost.org/doc/libs/1_69_0/libs/context/doc/html/context/performance.html

但是于此同时增加了空间消耗，但空间消耗的大头是栈空间。每个协程增加的空间消耗为13B不到。相比较一个协程8KB-64KB的栈空间开销，大概只占了千分之一左右。

在最能反映综合性能的指标中，FullFlow的全流程协程测试的性能提高了数百倍。
