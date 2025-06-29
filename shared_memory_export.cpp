#include "shared_memory_export.h"
#include <iostream>

extern "C" {

ISharedMemoryManager *getSharedMemoryManager() {
  try {
    return &OptimizedStatusRscManager::getInstance();
  } catch (const std::exception &e) {
    std::cerr << "Error getting shared memory manager: " << e.what()
              << std::endl;
    return nullptr;
  }
}

int cleanupSharedMemory() {
  try {
    return OptimizedStatusRscManager::cleanup();
  } catch (const std::exception &e) {
    std::cerr << "Error cleaning up shared memory: " << e.what() << std::endl;
    return -1;
  }
}

} // extern "C"
