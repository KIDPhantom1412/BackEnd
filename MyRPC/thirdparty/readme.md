# 第三方库/服务-安装和使用
## 1.jsoncpp
### 1.1编译
- jsoncpp-0.5.0使用scons进行编译，在centos下，执行`yum install scons`命令来安装scons。
- 执行`tar -zxf jsoncpp-src-0.5.0.tar.gz`命令来解压源文件。
- 执行`cd jsoncpp-src-0.5.0`命令切换到源文件目录。
- 执行`yum install scons -y`命令安装scons。
- 执行`scons platform=linux-gcc`命令对源文件进行编译。
### 1.2安装
- 执行`mkdir -p /usr/local/jsoncpp/libs`创建目录。
- 在源文件jsoncpp-src-0.5.0目录下执行`cp -r include /usr/local/jsoncpp/`命令，发布头文件。
- 在源文件jsoncpp-src-0.5.0目录下执行`find ./libs -name '*.so' | xargs -I{} cp {} /usr/local/jsoncpp/libs/libjson.so`命令，发布动态链接库。
### 1.3使用
- 在源代码中include jsoncpp相关的头文件。
- 编译的时指定头文件路径，并链接libjson.so库。

## 2.protobuf
### 2.1编译
- 执行`tar -zxf protobuf-cpp-3.6.1.tar.gz`命令来解压源文件。
- 执行`cd protobuf-3.6.1`命令切换到源文件目录。
- 执行`./configure --prefix=/usr/local/protobuf`命令生成编译的makefile。
- 执行`make -j$(nproc)`命令对源文件进行编译。
### 2.2安装
- 执行`make install`发布相关的头文件和链接库。
### 2.3使用
- 在源文件中include protobuf相关的头文件。
- 编译的时指定头文件路径，并链接libprotobuf.so库。

## 3.redis
### 3.1编译
- 执行`tar -zxf redis-stable.tar.gz`命令来解压源文件。
- 执行`cd redis-stable`命令切换到源文件目录。
- 执行`make -j$(nproc)`命令对源文件进行编译。
### 3.2启动
- 编辑当前目录的redis.conf文件，修改daemonize配置为yes，开启requirepass认证配置并设置密码。
- 执行`./src/redis-server ./redis.conf`命令来启动redis服务。

## 4.snappy
### 4.1编译
- 执行`tar -zxf snappy-1.0.5.tar.gz`命令来解压源文件。
- 执行`cd snappy-1.0.5`命令切换到源文件目录。
- 执行`./configure --prefix=/usr/local/snappy`命令生成编译的makefile。
- 执行`make -j$(nproc)`命令对源文件进行编译。
### 4.2安装
- 执行`make install`发布相关的头文件和链接库。
### 4.3使用
- 在源文件中include snappy相关的头文件。
- 编译的时指定头文件路径，并链接libsnappy.so库。

## 5. Boost.Context
### 1. 准备编译环境与源码

与之前相同，确保基础工具已安装，并下载解压源码。

```bash
# 安装基础依赖
sudo yum install -y gcc gcc-c++ make wget bzip2

# 下载并解压 1.69.0 版本
cd /opt
sudo wget https://sourceforge.net/projects/boost/files/boost/1.69.0/boost_1_69_0.tar.gz/download -O boost_1_69_0.tar.gz
sudo tar -zxvf boost_1_69_0.tar.gz
cd boost_1_69_0
```

### 2. 配置（Bootstrap）指定目标库

在执行 `bootstrap.sh` 时，使用 `--with-libraries=context` 显式告诉构建系统：**只准备编译 context 库**（构建系统会自动解析并包含它依赖的其他内部基础库，如 `thread`、`system` 等，如果有的话）。

```bash
# 只配置 context 库，指定安装到 /usr/local
sudo ./bootstrap.sh --with-libraries=context --prefix=/usr/local
```

*注：你可以运行 `./bootstrap.sh --show-libraries` 查看所有支持单独指定的库名称。*

### 3. 编译与安装

同样建议带上 `fPIC` 标志。此时 `b2` 只会编译 `Context` 相关的动态库和静态库。

```bash
sudo ./b2 install cxxflags=-fPIC cflags=-fPIC -j$(nproc)
```

*你会发现编译过程非常快，通常不到 1 分钟即可完成。*

### 4. 配置动态链接库路径

```bash
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/boost.conf
sudo ldconfig
```

### 5. 验证 Boost.Context 安装

为了验证 `Boost.Context` 是否安装成功并且能正常链接，我们需要写一段真实使用了 `context` 库的协程/上下文切换代码。

**创建 `test_context.cpp`：**

```cpp
#include <iostream>
#include <boost/context/continuation.hpp>

namespace ctx = boost::context;

int main() {
    int a = 0;
    // 创建一个上下文/协程
    ctx::continuation source = ctx::callcc(
        [&a](ctx::continuation && sink) {
            a = 1;
            std::cout << "Inside context: a = " << a << std::endl;
            // 切回主上下文
            sink = sink.resume();
            a = 2;
            std::cout << "Inside context again: a = " << a << std::endl;
            return std::move(sink);
        });

    std::cout << "Back in main, a = " << a << std::endl;
    // 再次切入上下文
    source = source.resume();
    std::cout << "Back in main again, a = " << a << std::endl;

    return 0;
}
```

**编译并运行：**
注意：`Boost.Context` 是需要链接二进制库的组件（非 header-only），因此在编译时必须显式链接 `-lboost_context`。由于它还强依赖 C++11 的一些特性，需加上 `-std=c++11`。

```bash
# 编译并链接 boost_context 库
g++ test_context.cpp -o test_context -std=c++11 -I/usr/local/include -L/usr/local/lib -lboost_context

# 运行
./test_context
```

**预期输出结果：**

```text
Inside context: a = 1
Back in main, a = 1
Inside context again: a = 2
Back in main again, a = 2
```

如果能成功编译并输出上述内容，说明 `Boost.Context` 已经完美地独立安装并在你的 CentOS 7 上运行了。

### 参考资料

*   [Boost.Build 官方文档: Invocation / Limiting Which Libraries are Built](https://www.boost.org/doc/libs/1_69_0/more/getting_started/unix-variants.html#show-libraries)
*   [Boost.Context 官方文档 (1.69.0)](https://www.boost.org/doc/libs/1_69_0/libs/context/doc/html/index.html)