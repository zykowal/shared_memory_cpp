CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -fPIC
# macOS不需要-lrt库
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS = -pthread -lrt
else
    LDFLAGS = -pthread
endif

# 目标文件
SHARED_LIB = libstatus.so
IMPROVED_LIB = libimproved_status.so
TEST_EXEC = test_example
MULTI_TEST_EXEC = multi_process_test

# 源文件
ORIGINAL_SOURCES = status.cpp
IMPROVED_SOURCES = improved_status.cpp
TEST_SOURCES = test_example.cpp
MULTI_TEST_SOURCES = multi_process_test.cpp

# 对象文件
ORIGINAL_OBJECTS = $(ORIGINAL_SOURCES:.cpp=.o)
IMPROVED_OBJECTS = $(IMPROVED_SOURCES:.cpp=.o)

.PHONY: all clean original improved test multi-test

all: improved test multi-test

# 编译改进版本
improved: $(IMPROVED_LIB)

$(IMPROVED_LIB): $(IMPROVED_OBJECTS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# 编译原始版本
original: $(SHARED_LIB)

$(SHARED_LIB): $(ORIGINAL_OBJECTS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# 编译测试程序
test: $(TEST_EXEC)

$(TEST_EXEC): $(TEST_SOURCES) $(IMPROVED_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译多进程测试程序
multi-test: $(MULTI_TEST_EXEC)

$(MULTI_TEST_EXEC): $(MULTI_TEST_SOURCES) $(IMPROVED_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译对象文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
clean:
	rm -f *.o $(SHARED_LIB) $(IMPROVED_LIB) $(TEST_EXEC) $(MULTI_TEST_EXEC)
	# 清理共享内存（如果存在）
	-rm /dev/shm/status_rsc_memory 2>/dev/null || true

# 运行测试
run-test: test
	./$(TEST_EXEC)

# 运行多进程测试
run-multi-test: multi-test
	./$(MULTI_TEST_EXEC)

# 显示帮助
help:
	@echo "可用目标:"
	@echo "  all           - 编译改进版本和所有测试程序"
	@echo "  original      - 编译原始版本动态库"
	@echo "  improved      - 编译改进版本动态库"
	@echo "  test          - 编译基本测试程序"
	@echo "  multi-test    - 编译多进程测试程序"
	@echo "  run-test      - 编译并运行基本测试程序"
	@echo "  run-multi-test - 编译并运行多进程测试程序"
	@echo "  clean         - 清理编译文件和共享内存"
	@echo "  help          - 显示此帮助信息"
