CXX = g++
CXXFLAGS = -g -O2 -Wall -Werror -pipe -m64 -std=c++11
INCFLAGS = -I../../common -I../../core -I/usr/local/include/boost
LDFLAGS = -pthread -lboost_context -L/usr/local/lib -lrt

TARGET = coroutineb

SOURCES = ./coroutineb.cpp ../../core/coroutine.cpp ../../common/cmdline.cpp
OBJS = $(addsuffix .o, $(basename $(SOURCES)))

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) *~

debug:
	@echo "TARGET :" $(TARGET)
	@echo "SOURCES :" $(SOURCES)
	@echo "OBJS :" $(OBJS)
	@echo "CXXFLAGS :" $(CXXFLAGS)
	@echo "INCFLAGS :" $(INCFLAGS)
	@echo "LDFLAGS :" $(LDFLAGS)
