#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#define OK 0
#define NOT_FOUND -1
#define NO_SPACE_ERR -2
#define DUPLICATE_KEY -3

const int MAX_VALUE_LEN = 256;
const int STATUS_MAX_ENTRIES = 1024;

struct SharedEntry {
  int key;
  char value[MAX_VALUE_LEN];
  bool is_used;  // 标记槽位是否被使用
};

struct StatusSharedData {
  volatile bool initialized;
  int current_count;  // 实际使用的条目数
  pthread_mutex_t map_mutex;
  pthread_mutex_t init_mutex;  // 用于初始化同步的进程间互斥锁
  SharedEntry status_map[STATUS_MAX_ENTRIES];
};

class StatusRscManager {
public:
  static StatusRscManager &getInstance();

  // Delete copy constructor and copy assignment
  StatusRscManager(const StatusRscManager &) = delete;
  StatusRscManager &operator=(const StatusRscManager &) = delete;

  int updateRsc(int rsc_key, const std::string &rsc_value);
  int addRsc(int rsc_key, const std::string &rsc_value);
  int upsertRsc(int rsc_key, const std::string &rsc_value);  // 新增：更新或插入
  std::string getRsc(int rsc_key);
  int removeRsc(int rsc_key);  // 新增：删除功能
  
  int batchUpdateRsc(const std::map<int, std::string> &updated_map);
  int batchGetRsc(std::map<int, std::string> &fetched_map);
  
  bool isContain(int rsc_key);
  int rscNum();
  int clearRsc();
  
  // 手动清理共享内存（通常由最后一个进程调用）
  static int cleanup();

private:
  StatusRscManager();
  ~StatusRscManager();
  
  // 内部辅助函数，不加锁
  int findEntryIndex(int key);
  int findEmptySlot();

private:
  StatusSharedData *shared_data_;
  int shm_fd_;
  bool is_creator_;  // 标记是否是创建者
};
