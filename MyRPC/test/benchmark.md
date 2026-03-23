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

## 综合测试

### 测试脚本 

单进程的监测，比较简单。

#### wrk测试脚本

测试脚本位于`/home/backend/wrk/echo_post.lua`。

```lua
-- 1. 设置请求方法为 POST

wrk.method = "POST"

-- 2. 设置 JSON 格式的请求体（等价于 curl 的 --data-raw）

-- 这里可以自定义 body 内容，比如 {"msg": "EchoMySelf"}

wrk.body = '{"message": "hello world" }'

-- 3. 配置请求头（等价于命令行的 -H 参数，也可混合使用）

wrk.headers["service_name"] = "Echo"

wrk.headers["rpc_name"] = "EchoMySelf"

wrk.headers["Content-Type"] = "application/json"
```

#### 测试命令

```bash
wrk -c 1000 -t 4 -d 60 --latency -s /home/backend/wrk/echo_post.lua 'http://127.0.0.1:1693/index'
```

#### perf测试命令

```bash
perf record -F 99 -p $(pidof echo) -g --call-graph fp -o perf_debug.data -- sleep 30
```

#### perf分析

```bash
sudo perf report -i perf_debug.data
```

#### 生成火焰图

```bash
sudo perf script -i perf_debug.data > out.perf
# 生成折叠栈
../FlameGraph/stackcollapse-perf.pl out.perf > out.folded
# 生成火焰图
../FlameGraph/flamegraph.pl out.folded > perf.svg
```

### 测试场景

以HTTP echo为例，测试日志写入多和关闭日志的场景。测试三次，取中间值。

#### 日志写入

```ini
[MyRPC]
port = 1693
listen_if = any
coroutine_count = 10240
process_count = 4
log_level = 0
```

##### 原版

```bash
[root@localhost backend]# wrk -c 1000 -t 4 -d 60 --latency -s /home/backend/wrk/echo_post.lua 'http://127.0.0.1:1693/index'
Running 1m test @ http://127.0.0.1:1693/index
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    16.39ms    7.67ms  96.48ms   77.69%
    Req/Sec    14.95k     2.35k   21.23k    69.96%
  Latency Distribution
     50%   14.59ms
     75%   20.76ms
     90%   25.56ms
     99%   41.23ms
  3575296 requests in 1.00m, 524.71MB read
Requests/sec:  59522.15
Transfer/sec:      8.74MB
```

##### 优化

```bash
[root@localhost backend]# wrk -c 1000 -t 4 -d 60 --latency -s /home/backend/wrk/echo_post.lua 'http://127.0.0.1:1693/index'
Running 1m test @ http://127.0.0.1:1693/index
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    17.49ms    7.23ms  83.51ms   76.82%
    Req/Sec    13.94k     1.68k   19.50k    71.97%
  Latency Distribution
     50%   15.20ms
     75%   22.50ms
     90%   26.27ms
     99%   40.60ms
  3335406 requests in 1.00m, 489.43MB read
Requests/sec:  55497.85
Transfer/sec:      8.14MB
```

#### 关闭日志

```ini
[MyRPC]
port = 1693
listen_if = any
coroutine_count = 10240
process_count = 4
log_level = 4
```

##### 原版

```bash
[root@localhost backend]# wrk -c 1000 -t 4 -d 60 --latency -s /home/backend/wrk/echo_post.lua 'http://127.0.0.1:1693/index'
Running 1m test @ http://127.0.0.1:1693/index
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    11.27ms    4.67ms  58.34ms   78.07%
    Req/Sec    20.94k     2.44k   47.32k    72.21%
  Latency Distribution
     50%   11.40ms
     75%   12.77ms
     90%   15.84ms
     99%   25.63ms
  5006540 requests in 1.00m, 734.76MB read
Requests/sec:  83301.77
Transfer/sec:     12.23MB
```

##### 优化

```bash
[root@localhost backend]# wrk -c 1000 -t 4 -d 60 --latency -s /home/backend/wrk/echo_post.lua 'http://127.0.0.1:1693/index'
Running 1m test @ http://127.0.0.1:1693/index
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    14.12ms    5.20ms  81.12ms   76.23%
    Req/Sec    16.82k     1.50k   32.96k    70.79%
  Latency Distribution
     50%   12.34ms
     75%   15.02ms
     90%   23.20ms
     99%   27.08ms
  4019347 requests in 1.00m, 589.86MB read
Requests/sec:  66909.58
Transfer/sec:      9.82MB
```

