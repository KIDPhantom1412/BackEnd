#======================== 编译目标 开始 ========================#
TARGET = 
#======================== 编译目标 结束 ========================#

#======================= 自定义设置部分 开始 ====================#
# c编译选项
CFLAGS = -g -O2 -Wall -Werror -pipe -m64
# c++编译选项
CXXFLAGS = -g -O2 -Wall -Werror -pipe -m64 -std=c++11
# 连接选项 (增加 -lboost_context)
LDFLAGS = -pthread -lprotobuf -L/usr/local/protobuf/lib -ljson -L/usr/local/jsoncpp/libs -lsnappy -L/usr/local/snappy/lib -lboost_context -L/usr/local/lib -lrt -Wl,-rpath=/usr/local/jsoncpp/libs:/usr/local/protobuf/lib:/usr/local/snappy/lib
# 头文件目录
INCFLAGS = -I../../common -I../../core -I../../protocol -I/usr/local/protobuf/include -I/usr/local/jsoncpp/include -I/usr/local/snappy/include -I/usr/local/include/boost
# 源文件目录 (只需要当前目录和 core，因为只测协程不测网络)
SRCDIRS = . ../../core
# 单独的源文件
ALONE_SOURCES = ../../common/cmdline.cpp
#======================= 自定义设置部分 结束 =====================#

#======================= 固定设置部分 开始 =======================#
# c编译器
CC = gcc
# c++编译器
CXX = g++
# 源文件类型扩展：c为c源文件，其他的为c++源文件
SRCEXTS = .c .C .cc .cpp .CPP .c++ .cxx .cp
# 头文件类型扩展
HDREXTS = .h .H .hh .hpp .HPP .h++ .hxx .hp

# 如果TARGET为空，则取当前目录的basename作为目标名词
ifeq ($(TARGET),)
	# 取当前路径名列中最后一个名词，CURDIR是make的内置变量，自动会被设置为当前目录
	TARGET = $(shell basename $(CURDIR))
	ifeq ($(TARGET),)
		TARGET = a.out
	endif
endif

# 如果源文件目录为空，则默认当前目录为源文件目录
ifeq ($(SRCDIRS),)
	SRCDIRS = .
endif

# foreach函数用于遍历源文件目录，针对每个目录再调用addprefix函数添加目录前缀，生成各种指定源文件后缀类型的通用匹配模式（类似正则表达式）
# 使用wildcard函数对每个目录下文件，进行通配符扩展，最后得到所有的TARGET依赖的源文件列表，保存到SOURCES中
SOURCES = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(SRCEXTS))))
SOURCES += $(ALONE_SOURCES)
# 和上面的SOURCES类似
HEADERS = $(foreach d,$(SRCDIRS),$(wildcard $(addprefix $(d)/*,$(HDREXTS))))

# 过滤掉c语言相关的源文件，这个后续用于判断时采用c编译还是c++编译
SRC_CXX = $(filter-out %.c,$(SOURCES))

# 目标文件列表，先调用basename函数取源文件的前缀，然后再调用addsuffix函数添加.o的后缀
OBJS = $(addsuffix .o, $(basename $(SOURCES)))

# 定义编译和链接使用的变量
COMPILE.c   = $(CC)  $(CFLAGS)   $(INCFLAGS) -c
COMPILE.cxx = $(CXX) $(CXXFLAGS) $(INCFLAGS) -c
LINK.c      = $(CC)  $(CFLAGS)
LINK.cxx    = $(CXX) $(CXXFLAGS)

.PHONY: all objs clean help debug

# all生成的依赖规则，就是用于生成TARGET
all: $(TARGET)

# objs生成的依赖规则，就是用于生成各个链接使用的目标文件
objs: $(OBJS)

# 下面的是生成目标文件的通用规则
%.o:%.c
	$(COMPILE.c) $< -o $@

%.o:%.C
	$(COMPILE.cxx) $< -o $@

%.o:%.cc
	$(COMPILE.cxx) $< -o $@

%.o:%.cpp
	$(COMPILE.cxx) $< -o $@

%.o:%.CPP
	$(COMPILE.cxx) $< -o $@

%.o:%.c++
	$(COMPILE.cxx) $< -o $@

%.o:%.cp
	$(COMPILE.cxx) $< -o $@

%.o:%.cxx
	$(COMPILE.cxx) $< -o $@

# 最终目标文件的依赖规则
$(TARGET): $(OBJS)
ifeq ($(SRC_CXX),)              # c程序
	$(LINK.c)   $(OBJS) -o $@ $(LDFLAGS)
	@echo Type $@ to execute the program.
else                            # c++程序
	$(LINK.cxx) $(OBJS) -o $@ $(LDFLAGS)
	@echo Type $@ to execute the program.
endif

# 清理规则
clean:
	rm -f $(OBJS) $(TARGET) *~

# 帮助规则
help:
	@echo "===============A common Makefilefor c/c++ programs=============="
	@echo "Copyright (C) 2014 liigo.com"
	@echo "The following targets are support:"
	@echo
	@echo " all              - (==make) compile and link"
	@echo " objs             - just compile, without link"
	@echo " clean            - clean target"
	@echo " debug            - show variables"
	@echo " help             - print help information"
	@echo
	@echo "To make a target, do 'make [target]'"
	@echo "========================= Version2.0 ==========================="

# 调试规则，用于显示变量的值
debug:
	@echo "TARGET :" $(TARGET)
	@echo "SRCDIRS :" $(SRCDIRS)
	@echo "SOURCES :" $(SOURCES)
	@echo "OBJS :" $(OBJS)
	@echo "COMPILE.c :" $(COMPILE.c)
	@echo "COMPILE.cxx :" $(COMPILE.cxx)
	@echo "LINK.c :" $(LINK.c)
	@echo "LINK.cxx :" $(LINK.cxx)
