/*
 * 简单的 cleanup 函数使用示例
 * 编译: g++ -o simple_cleanup_example simple_cleanup_example.cpp -ldl
 * 运行: ./simple_cleanup_example
 */

#include <dlfcn.h>
#include <iostream>

class OptimizedStatusRscManager; // 前向声明

int main() {
  std::cout << "=== Simple Cleanup Example ===" << std::endl;

  // 1. 加载库
  void *lib = dlopen("./libSHARED_MEM_MAP.dylib", RTLD_LAZY);
  if (!lib) {
    std::cerr << "Failed to load library" << std::endl;
    return 1;
  }

  // 2. 获取函数
  typedef int (*CleanupFunc)();

  auto cleanup = (CleanupFunc)dlsym(lib, "cleanupSharedMemory");

  if (!cleanup) {
    std::cerr << "Failed to load functions" << std::endl;
    dlclose(lib);
    return 1;
  }

  // 4. 执行cleanup
  std::cout << "\nPerforming cleanup..." << std::endl;
  int result = cleanup();

  if (result == 0) {
    std::cout << "✓ Cleanup successful - shared memory segment removed"
              << std::endl;
  } else {
    std::cout << "ℹ️  Cleanup completed (shared memory may not have existed)"
              << std::endl;
  }

  dlclose(lib);
  std::cout << "\n=== Cleanup Complete ===" << std::endl;

  return 0;
}
