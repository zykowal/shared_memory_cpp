#include "improved_status.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

StatusRscManager &StatusRscManager::getInstance() {
  static StatusRscManager instance{};
  return instance;
}

StatusRscManager::StatusRscManager()
    : shared_data_(nullptr), shm_fd_(-1), is_creator_(false) {

  // 尝试打开已存在的共享内存
  shm_fd_ = shm_open("/status_rsc_memory", O_RDWR, 0666);

  if (shm_fd_ == -1) {
    // 共享内存不存在，创建新的
    shm_fd_ = shm_open("/status_rsc_memory", O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd_ == -1) {
      if (errno == EEXIST) {
        // 其他进程刚刚创建了，重新尝试打开
        shm_fd_ = shm_open("/status_rsc_memory", O_RDWR, 0666);
      }
      if (shm_fd_ == -1) {
        throw std::runtime_error("shm_open failed: " +
                                 std::string(strerror(errno)));
      }
    } else {
      is_creator_ = true;
    }
  }

  // 如果是创建者，设置大小
  if (is_creator_) {
    if (ftruncate(shm_fd_, sizeof(StatusSharedData)) == -1) {
      close(shm_fd_);
      shm_unlink("/status_rsc_memory");
      throw std::runtime_error("ftruncate failed: " +
                               std::string(strerror(errno)));
    }
  }

  // 映射共享内存
  shared_data_ = static_cast<StatusSharedData *>(
      mmap(NULL, sizeof(StatusSharedData), PROT_READ | PROT_WRITE, MAP_SHARED,
           shm_fd_, 0));
  if (shared_data_ == MAP_FAILED) {
    close(shm_fd_);
    if (is_creator_) {
      shm_unlink("/status_rsc_memory");
    }
    throw std::runtime_error("mmap failed: " + std::string(strerror(errno)));
  }

  // 初始化共享数据结构（只有创建者执行）
  if (is_creator_) {
    // 初始化进程间互斥锁
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&shared_data_->init_mutex, &attr);
    pthread_mutex_init(&shared_data_->map_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    // 初始化数据
    memset(shared_data_->status_map, 0,
           sizeof(SharedEntry) * STATUS_MAX_ENTRIES);
    shared_data_->current_count = 0;

    // 使用内存屏障确保初始化完成后再设置标志
    __sync_synchronize();
    shared_data_->initialized = true;
  } else {
    // 等待初始化完成
    while (!shared_data_->initialized) {
      usleep(1000); // 等待1ms
    }
  }
}

StatusRscManager::~StatusRscManager() {
  if (shared_data_ != MAP_FAILED && shared_data_ != nullptr) {
    munmap(shared_data_, sizeof(StatusSharedData));
  }
  if (shm_fd_ != -1) {
    close(shm_fd_);
  }

  // 注意：不在析构函数中自动清理共享内存
  // 这样可以让多个进程共享数据，需要手动调用cleanup()来清理
}

int StatusRscManager::findEntryIndex(int key) {
  for (int i = 0; i < STATUS_MAX_ENTRIES; i++) {
    if (shared_data_->status_map[i].is_used &&
        shared_data_->status_map[i].key == key) {
      return i;
    }
  }
  return -1;
}

int StatusRscManager::findEmptySlot() {
  for (int i = 0; i < STATUS_MAX_ENTRIES; i++) {
    if (!shared_data_->status_map[i].is_used) {
      return i;
    }
  }
  return -1;
}

int StatusRscManager::updateRsc(int key, const std::string &value) {
  if (value.length() >= MAX_VALUE_LEN) {
    return NO_SPACE_ERR;
  }

  pthread_mutex_lock(&shared_data_->map_mutex);

  int index = findEntryIndex(key);
  if (index == -1) {
    pthread_mutex_unlock(&shared_data_->map_mutex);
    return NOT_FOUND;
  }

  strncpy(shared_data_->status_map[index].value, value.c_str(),
          MAX_VALUE_LEN - 1);
  shared_data_->status_map[index].value[MAX_VALUE_LEN - 1] = '\0';

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return OK;
}

int StatusRscManager::addRsc(int key, const std::string &value) {
  if (value.length() >= MAX_VALUE_LEN) {
    return NO_SPACE_ERR;
  }

  pthread_mutex_lock(&shared_data_->map_mutex);

  // 检查是否已存在
  if (findEntryIndex(key) != -1) {
    pthread_mutex_unlock(&shared_data_->map_mutex);
    return DUPLICATE_KEY;
  }

  // 查找空槽位
  int empty_slot = findEmptySlot();
  if (empty_slot == -1) {
    pthread_mutex_unlock(&shared_data_->map_mutex);
    return NO_SPACE_ERR;
  }

  // 添加新条目
  shared_data_->status_map[empty_slot].key = key;
  strncpy(shared_data_->status_map[empty_slot].value, value.c_str(),
          MAX_VALUE_LEN - 1);
  shared_data_->status_map[empty_slot].value[MAX_VALUE_LEN - 1] = '\0';
  shared_data_->status_map[empty_slot].is_used = true;
  shared_data_->current_count++;

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return OK;
}

int StatusRscManager::upsertRsc(int key, const std::string &value) {
  if (value.length() >= MAX_VALUE_LEN) {
    return NO_SPACE_ERR;
  }

  pthread_mutex_lock(&shared_data_->map_mutex);

  int index = findEntryIndex(key);
  if (index != -1) {
    // 更新现有条目
    strncpy(shared_data_->status_map[index].value, value.c_str(),
            MAX_VALUE_LEN - 1);
    shared_data_->status_map[index].value[MAX_VALUE_LEN - 1] = '\0';
  } else {
    // 添加新条目
    int empty_slot = findEmptySlot();
    if (empty_slot == -1) {
      pthread_mutex_unlock(&shared_data_->map_mutex);
      return NO_SPACE_ERR;
    }

    shared_data_->status_map[empty_slot].key = key;
    strncpy(shared_data_->status_map[empty_slot].value, value.c_str(),
            MAX_VALUE_LEN - 1);
    shared_data_->status_map[empty_slot].value[MAX_VALUE_LEN - 1] = '\0';
    shared_data_->status_map[empty_slot].is_used = true;
    shared_data_->current_count++;
  }

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return OK;
}

std::string StatusRscManager::getRsc(int key) {
  std::string result;

  pthread_mutex_lock(&shared_data_->map_mutex);

  int index = findEntryIndex(key);
  if (index != -1) {
    result = shared_data_->status_map[index].value;
  }

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return result;
}

int StatusRscManager::removeRsc(int key) {
  pthread_mutex_lock(&shared_data_->map_mutex);

  int index = findEntryIndex(key);
  if (index == -1) {
    pthread_mutex_unlock(&shared_data_->map_mutex);
    return NOT_FOUND;
  }

  shared_data_->status_map[index].is_used = false;
  memset(&shared_data_->status_map[index], 0, sizeof(SharedEntry));
  shared_data_->current_count--;

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return OK;
}

int StatusRscManager::batchUpdateRsc(
    const std::map<int, std::string> &updated_map) {
  pthread_mutex_lock(&shared_data_->map_mutex);

  for (const auto &entry : updated_map) {
    if (entry.second.length() >= MAX_VALUE_LEN) {
      pthread_mutex_unlock(&shared_data_->map_mutex);
      return NO_SPACE_ERR;
    }

    int index = findEntryIndex(entry.first);
    if (index == -1) {
      pthread_mutex_unlock(&shared_data_->map_mutex);
      return NOT_FOUND;
    }

    strncpy(shared_data_->status_map[index].value, entry.second.c_str(),
            MAX_VALUE_LEN - 1);
    shared_data_->status_map[index].value[MAX_VALUE_LEN - 1] = '\0';
  }

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return OK;
}

int StatusRscManager::batchGetRsc(std::map<int, std::string> &fetched_map) {
  pthread_mutex_lock(&shared_data_->map_mutex);

  for (auto &entry : fetched_map) {
    int index = findEntryIndex(entry.first);
    if (index != -1) {
      entry.second = shared_data_->status_map[index].value;
    } else {
      entry.second = ""; // 未找到的key设为空字符串
    }
  }

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return OK;
}

bool StatusRscManager::isContain(int key) {
  pthread_mutex_lock(&shared_data_->map_mutex);
  bool result = (findEntryIndex(key) != -1);
  pthread_mutex_unlock(&shared_data_->map_mutex);
  return result;
}

int StatusRscManager::rscNum() { return shared_data_->current_count; }

int StatusRscManager::clearRsc() {
  pthread_mutex_lock(&shared_data_->map_mutex);

  memset(shared_data_->status_map, 0, sizeof(SharedEntry) * STATUS_MAX_ENTRIES);
  shared_data_->current_count = 0;

  pthread_mutex_unlock(&shared_data_->map_mutex);
  return OK;
}

int StatusRscManager::cleanup() {
  if (shm_unlink("/status_rsc_memory") != 0) {
    std::cerr << "Failed to unlink shared memory: " << strerror(errno)
              << std::endl;
    return -1;
  }
  return OK;
}
