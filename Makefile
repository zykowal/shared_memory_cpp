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
OPTIMIZED_LIB = liboptimized_status.so
RWLOCK_LIB = librwlock_optimized_status.so
TEST_EXEC = test_example
MULTI_TEST_EXEC = multi_process_test
PERF_TEST_EXEC = performance_test
OPT_EXAMPLE_EXEC = optimized_example
RWLOCK_PERF_EXEC = rwlock_performance_test

# 源文件
ORIGINAL_SOURCES = status.cpp
IMPROVED_SOURCES = improved_status.cpp
OPTIMIZED_SOURCES = optimized_status.cpp
RWLOCK_SOURCES = rwlock_optimized_status.cpp
TEST_SOURCES = test_example.cpp
MULTI_TEST_SOURCES = multi_process_test.cpp
PERF_TEST_SOURCES = performance_test.cpp
OPT_EXAMPLE_SOURCES = optimized_example.cpp
RWLOCK_PERF_SOURCES = rwlock_performance_test.cpp

# 对象文件
ORIGINAL_OBJECTS = $(ORIGINAL_SOURCES:.cpp=.o)
IMPROVED_OBJECTS = $(IMPROVED_SOURCES:.cpp=.o)
OPTIMIZED_OBJECTS = $(OPTIMIZED_SOURCES:.cpp=.o)
RWLOCK_OBJECTS = $(RWLOCK_SOURCES:.cpp=.o)

.PHONY: all clean original improved optimized rwlock test multi-test perf-test opt-example rwlock-perf

all: optimized rwlock test multi-test perf-test opt-example rwlock-perf

# 编译读写锁优化版本
rwlock: $(RWLOCK_LIB)

$(RWLOCK_LIB): $(RWLOCK_OBJECTS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# 编译优化版本
optimized: $(OPTIMIZED_LIB)

$(OPTIMIZED_LIB): $(OPTIMIZED_OBJECTS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

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

# 编译性能测试程序
perf-test: $(PERF_TEST_EXEC)

$(PERF_TEST_EXEC): $(PERF_TEST_SOURCES) $(IMPROVED_OBJECTS) $(OPTIMIZED_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译优化版本示例程序
opt-example: $(OPT_EXAMPLE_EXEC)

$(OPT_EXAMPLE_EXEC): $(OPT_EXAMPLE_SOURCES) $(OPTIMIZED_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译读写锁性能测试程序
rwlock-perf: $(RWLOCK_PERF_EXEC)

$(RWLOCK_PERF_EXEC): $(RWLOCK_PERF_SOURCES) $(OPTIMIZED_OBJECTS) $(RWLOCK_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译对象文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
clean:
	rm -f *.o $(SHARED_LIB) $(IMPROVED_LIB) $(OPTIMIZED_LIB) $(RWLOCK_LIB) $(TEST_EXEC) $(MULTI_TEST_EXEC) $(PERF_TEST_EXEC) $(OPT_EXAMPLE_EXEC) $(RWLOCK_PERF_EXEC)
	# 清理共享内存（如果存在）
	-rm /dev/shm/status_rsc_memory 2>/dev/null || true
	-rm /dev/shm/optimized_status_memory 2>/dev/null || true
	-rm /dev/shm/rwlock_optimized_status_memory 2>/dev/null || true

# 运行测试
run-test: test
	./$(TEST_EXEC)

# 运行多进程测试
run-multi-test: multi-test
	./$(MULTI_TEST_EXEC)

# 运行性能测试
run-perf-test: perf-test
	./$(PERF_TEST_EXEC)

# 运行优化版本示例
run-opt-example: opt-example
	./$(OPT_EXAMPLE_EXEC)

# 运行读写锁性能测试
run-rwlock-perf: rwlock-perf
	./$(RWLOCK_PERF_EXEC)

# 显示帮助
help:
	@echo "可用目标:"
	@echo "  all              - 编译所有版本和测试程序"
	@echo "  original         - 编译原始版本动态库"
	@echo "  improved         - 编译改进版本动态库"
	@echo "  optimized        - 编译优化版本动态库"
	@echo "  rwlock           - 编译读写锁优化版本动态库"
	@echo "  test             - 编译基本测试程序"
	@echo "  multi-test       - 编译多进程测试程序"
	@echo "  perf-test        - 编译性能测试程序"
	@echo "  opt-example      - 编译优化版本示例程序"
	@echo "  rwlock-perf      - 编译读写锁性能测试程序"
	@echo "  run-test         - 编译并运行基本测试程序"
	@echo "  run-multi-test   - 编译并运行多进程测试程序"
	@echo "  run-perf-test    - 编译并运行性能测试程序"
	@echo "  run-opt-example  - 编译并运行优化版本示例程序"
	@echo "  run-rwlock-perf  - 编译并运行读写锁性能测试程序"
	@echo "  clean            - 清理编译文件和共享内存"
	@echo "  help             - 显示此帮助信息"
