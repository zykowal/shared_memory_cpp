#include "shared_memory_export.h"
#include <chrono>
#include <dlfcn.h>
#include <iostream>
#include <thread>

// 前向声明，避免包含头文件
class OptimizedStatusRscManager;

int main() {
  std::cout << "=== C++ Dynamic Loading Demo (Reader) ===" << std::endl;

  // 动态加载库
  void *lib_handle = dlopen("./libSHARED_MEM_MAP.dylib", RTLD_LAZY);
  if (!lib_handle) {
    std::cerr << "Cannot load library: " << dlerror() << std::endl;
    return 1;
  }

  std::cout << "✓ Successfully loaded dynamic library" << std::endl;

  // 清除之前的错误
  dlerror();

  // 获取函数指针 - 现在只需要获取管理器创建函数
  typedef ISharedMemoryManager *(*GetManagerFunc)();

  GetManagerFunc getManager =
      (GetManagerFunc)dlsym(lib_handle, "getSharedMemoryManager");

  // 检查是否有加载错误
  const char *dlsym_error = dlerror();
  if (dlsym_error) {
    std::cerr << "Cannot load symbols: " << dlsym_error << std::endl;
    dlclose(lib_handle);
    return 1;
  }

  std::cout << "✓ Successfully loaded function symbols" << std::endl;

  // 获取共享内存管理器实例
  ISharedMemoryManager *manager = getManager();
  if (!manager) {
    std::cerr << "Failed to get shared memory manager" << std::endl;
    dlclose(lib_handle);
    return 1;
  }

  std::cout << "✓ Successfully got shared memory manager instance" << std::endl;
  std::cout << "✓ Manager pointer: " << manager << std::endl;

  struct TestData {
    int key;
    const char *value;
  };

  TestData test_data[] = {{7001, "C++ Dynamic Load Test 1"},
                          {7002, "C++ Dynamic Load Test 2"},
                          {7003, "C++ Dynamic Load Test 3"},
                          {7004, "C++ Dynamic Load Test 4"},
                          {7005, "C++ Dynamic Load Test 5"}};

  int success_count = 0;
  for (const auto &data : test_data) {
    int result = manager->addRsc(data.key, data.value);
    if (result == 0) { // OK = 0
      std::cout << "✓ Added: " << data.key << " -> " << data.value << std::endl;
      success_count++;
    } else {
      std::cout << "✗ Failed to add: " << data.key << " (error: " << result
                << ")" << std::endl;
    }
  }

  std::cout << "Successfully added " << success_count << " entries"
            << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(10));

  std::cout << "\n--- Testing BatchRead Operations ---" << std::endl;

  std::map<int, std::string> fetched_map = {{7001, "C++ Dynamic Load Test 1"},
                                            {7002, "C++ Dynamic Load Test 2"},
                                            {7003, "Hello, World!"},
                                            {7004, "Updated"},
                                            {7005, "Updated"},
                                            {8001, "Not Found"}};

  manager->batchGetRsc(fetched_map);

  for (const auto &data : fetched_map) {
    std::cout << "Key " << data.first << ": " << data.second << std::endl;
  }

  // 关闭动态库
  dlclose(lib_handle);
  std::cout << "✓ Dynamic library closed" << std::endl;

  return 0;
}
