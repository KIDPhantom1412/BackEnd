# 测试指南

为了能够合理运行测试，需要一个具体的指南。

## 编译、安装运行所有的服务

先删除曾经安装的目录，
```bash
cd /home/backend
rm -rf BackEnd lock log route script service
```

### 除了redis之外的服务安装

编译，
```bash
cd /home/backend/BackEnd/MyRPC/service/
./build.sh
```
安装和运行，
```bash
cd /home/backend/BackEnd/MyRPC/service/
./install.sh
./service.sh start
```

### redis

解压和编译，
```bash
cp /home/backend/BackEnd/MyRPC/thirdparty /home/backend
cd /home/backend
tar -zxf redis-stable.tar.gz
cd redis-stable
make -j$(nproc)
```

之后需要设置`redis.conf`文件，修改daemonize配置为yes，开启requirepass认证配置并**设置密码为backend**。

启动redis，
```bash
cd /home/backend/redis-stable/
./src/redis-server ./redis.conf
```

配置redis的路由，
```bash
cd /home/backend/BackEnd/MyRPC/service/redis/
./install.sh
```

## 创建必要的目录

日志和锁的目录，
```bash
cd /home/backend
mkdir -p /home/backend/lock/subsys/
mkdir -p /home/backend/log/UnitTest/
```

## 编译运行测试代码

编译测试代码，
```bash
cd /home/backend/BackEnd/MyRPC/test
./build.sh
```
运行测试，
```bash
cd /home/backend/BackEnd/MyRPC/test
./UnitTest
```