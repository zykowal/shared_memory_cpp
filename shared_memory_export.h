#pragma once

#include "optimized_status.h"

extern "C" {
// 只导出这两个函数
ISharedMemoryManager *getSharedMemoryManager();
int cleanupSharedMemory();
}
