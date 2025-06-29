#pragma once

#include "shared_memory_inteface.h"
#include <map>
#include <memory>
#include <string>

#define OK 0
#define NOT_FOUND -1
#define NO_SPACE_ERR -2
#define DUPLICATE_KEY -3

const int MAX_VALUE_LEN = 256;
const int HASH_TABLE_SIZE = 2048;    // 使用2的幂次，便于位运算优化
const double MAX_LOAD_FACTOR = 0.75; // 最大负载因子
const int MAX_ENTRIES = static_cast<int>(HASH_TABLE_SIZE * MAX_LOAD_FACTOR);

// 哈希表条目状态
enum EntryState {
  EMPTY = 0,    // 空槽位
  OCCUPIED = 1, // 已占用
  DELETED = 2   // 已删除（用于开放寻址的删除标记）
};

struct HashEntry {
  int key;
  char value[MAX_VALUE_LEN];
  EntryState state;
  uint32_t hash_value; // 缓存哈希值，减少重复计算
};

struct OptimizedSharedData {
  volatile bool initialized;
  int current_count;  // 实际使用的条目数
  int deleted_count;  // 已删除的条目数
  uint32_t hash_seed; // 哈希种子，用于防止哈希攻击
  pthread_mutex_t table_mutex;
  pthread_mutex_t init_mutex;
  HashEntry hash_table[HASH_TABLE_SIZE];
};

class OptimizedStatusRscManager : public ISharedMemoryManager {
public:
  static OptimizedStatusRscManager &getInstance();

  // Delete copy constructor and copy assignment
  OptimizedStatusRscManager(const OptimizedStatusRscManager &) = delete;
  OptimizedStatusRscManager &
  operator=(const OptimizedStatusRscManager &) = delete;

  // 实现接口方法
  int addRsc(int key, const std::string &value) override;
  std::string getRsc(int key) override;
  int updateRsc(int key, const std::string &value) override;
  int upsertRsc(int key, const std::string &value) override;
  int removeRsc(int key) override;
  int isContain(int key) override;
  int rscNum() override;
  int clearRsc() override;
  double getLoadFactor() override;
  void printStats() override;

  // 批量操作
  int batchUpdateRsc(const std::map<int, std::string> &updated_map) override;
  int batchGetRsc(std::map<int, std::string> &fetched_map) override;

  // 清理共享内存
  static int cleanup();

private:
  OptimizedStatusRscManager();
  ~OptimizedStatusRscManager();

  // 哈希函数相关
  uint32_t hash(int key) const;
  uint32_t hash2(int key) const; // 双重哈希的第二个哈希函数

  // 内部辅助函数（不加锁）
  int findEntry(int key, uint32_t hash_val);
  int findEmptySlot(int key, uint32_t hash_val);
  bool needRehash() const;
  int rehashIfNeeded();

  // 探测序列生成
  int getNextProbe(int current_pos, int step, uint32_t hash2_val) const;

private:
  OptimizedSharedData *shared_data_;
  int shm_fd_;
  bool is_creator_;
};

// 内联哈希函数实现
inline uint32_t OptimizedStatusRscManager::hash(int key) const {
  // MurmurHash3的简化版本，针对32位整数优化
  uint32_t k = static_cast<uint32_t>(key);
  k ^= shared_data_->hash_seed;
  k ^= k >> 16;
  k *= 0x85ebca6b;
  k ^= k >> 13;
  k *= 0xc2b2ae35;
  k ^= k >> 16;
  return k & (HASH_TABLE_SIZE - 1); // 使用位运算代替模运算
}

inline uint32_t OptimizedStatusRscManager::hash2(int key) const {
  // 第二个哈希函数，用于双重哈希
  uint32_t k = static_cast<uint32_t>(key);
  k ^= shared_data_->hash_seed + 0x9e3779b9;
  k ^= k >> 16;
  k *= 0x21f0aaad;
  k ^= k >> 15;
  k *= 0x735a2d97;
  k ^= k >> 15;
  return (k & (HASH_TABLE_SIZE - 1)) | 1; // 确保结果是奇数
}

inline int OptimizedStatusRscManager::getNextProbe(int current_pos, int step,
                                                   uint32_t hash2_val) const {
  // 双重哈希探测
  return (current_pos + step * hash2_val) & (HASH_TABLE_SIZE - 1);
}
