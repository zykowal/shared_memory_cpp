#include "shared_memory_export.h"
#include <dlfcn.h>
#include <iostream>
#include <map>

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

  std::cout << "\n--- Reading Data Written by Writer Process ---" << std::endl;

  int test_keys[] = {7001, 7002, 7003, 7004, 7005, 8001};

  for (int key : test_keys) {
    int exists = manager->isContain(key);
    std::cout << "Key " << key << ": ";

    if (exists) {
      std::string value = manager->getRsc(key);
      if (!value.empty()) {
        std::cout << "✓ " << value << std::endl;
      } else {
        std::cout << "✗ Failed to read" << std::endl;
      }
    } else {
      std::cout << "NOT FOUND" << std::endl;
    }
  }

  // 更新一个值
  const std::map<int, std::string> updated_map = {
      {7003, "Hello, World!"}, {7004, "Updated"}, {7005, "Updated"}};
  manager->batchUpdateRsc(updated_map);

  // 显示统计信息
  std::cout << "\n--- Statistics ---" << std::endl;
  std::cout << "Total entries: " << manager->rscNum() << std::endl;
  std::cout << "Load factor: " << manager->getLoadFactor() << std::endl;
  manager->printStats();

  // 关闭动态库
  dlclose(lib_handle);
  std::cout << "✓ Dynamic library closed" << std::endl;

  return 0;
}
