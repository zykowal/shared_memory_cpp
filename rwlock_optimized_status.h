#pragma once

#include "shared_constants.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>

const int RWLOCK_HASH_TABLE_SIZE = 2048;
const double RWLOCK_MAX_LOAD_FACTOR = 0.75;
const int RWLOCK_MAX_ENTRIES = static_cast<int>(RWLOCK_HASH_TABLE_SIZE * RWLOCK_MAX_LOAD_FACTOR);

// 哈希表条目状态
enum RWLockEntryState {
    RWLOCK_EMPTY = 0,
    RWLOCK_OCCUPIED = 1,
    RWLOCK_DELETED = 2
};

struct RWLockHashEntry {
    int key;
    char value[MAX_VALUE_LEN];
    RWLockEntryState state;
    uint32_t hash_value;
};

struct RWLockOptimizedSharedData {
    volatile bool initialized;
    int current_count;
    int deleted_count;
    uint32_t hash_seed;
    pthread_rwlock_t table_rwlock;     // 读写锁替代互斥锁
    pthread_mutex_t init_mutex;        // 初始化仍使用互斥锁
    RWLockHashEntry hash_table[RWLOCK_HASH_TABLE_SIZE];
};

class RWLockOptimizedStatusRscManager {
public:
    static RWLockOptimizedStatusRscManager &getInstance();

    RWLockOptimizedStatusRscManager(const RWLockOptimizedStatusRscManager &) = delete;
    RWLockOptimizedStatusRscManager &operator=(const RWLockOptimizedStatusRscManager &) = delete;

    // 基本操作
    int addRsc(int rsc_key, const std::string &rsc_value);
    int updateRsc(int rsc_key, const std::string &rsc_value);
    int upsertRsc(int rsc_key, const std::string &rsc_value);
    std::string getRsc(int rsc_key);                      // 只读操作，使用读锁
    int removeRsc(int rsc_key);
    bool isContain(int rsc_key);                          // 只读操作，使用读锁
    
    // 批量操作
    int batchUpdateRsc(const std::map<int, std::string> &updated_map);
    int batchGetRsc(std::map<int, std::string> &fetched_map);  // 只读操作，使用读锁
    
    // 管理操作
    int rscNum();                                         // 只读操作，使用读锁
    int clearRsc();
    double getLoadFactor();                               // 只读操作，使用读锁
    void printStats();                                    // 只读操作，使用读锁
    
    static int cleanup();

private:
    RWLockOptimizedStatusRscManager();
    ~RWLockOptimizedStatusRscManager();
    
    // 哈希函数
    uint32_t hash(int key) const;
    uint32_t hash2(int key) const;
    
    // 内部辅助函数（调用前需要已经获取锁）
    int findEntry(int key, uint32_t hash_val);
    int findEmptySlot(int key, uint32_t hash_val);
    bool needRehash() const;
    int rehashIfNeeded();
    
    int getNextProbe(int current_pos, int step, uint32_t hash2_val) const;

private:
    RWLockOptimizedSharedData *shared_data_;
    int shm_fd_;
    bool is_creator_;
};

// 内联函数实现
inline uint32_t RWLockOptimizedStatusRscManager::hash(int key) const {
    uint32_t k = static_cast<uint32_t>(key);
    k ^= shared_data_->hash_seed;
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k & (RWLOCK_HASH_TABLE_SIZE - 1);
}

inline uint32_t RWLockOptimizedStatusRscManager::hash2(int key) const {
    uint32_t k = static_cast<uint32_t>(key);
    k ^= shared_data_->hash_seed + 0x9e3779b9;
    k ^= k >> 16;
    k *= 0x21f0aaad;
    k ^= k >> 15;
    k *= 0x735a2d97;
    k ^= k >> 15;
    return (k & (RWLOCK_HASH_TABLE_SIZE - 1)) | 1;
}

inline int RWLockOptimizedStatusRscManager::getNextProbe(int current_pos, int step, uint32_t hash2_val) const {
    return (current_pos + step * hash2_val) & (RWLOCK_HASH_TABLE_SIZE - 1);
}

// RAII锁管理器
class ReadLockGuard {
public:
    explicit ReadLockGuard(pthread_rwlock_t* rwlock) : rwlock_(rwlock) {
        pthread_rwlock_rdlock(rwlock_);
    }
    ~ReadLockGuard() {
        pthread_rwlock_unlock(rwlock_);
    }
private:
    pthread_rwlock_t* rwlock_;
};

class WriteLockGuard {
public:
    explicit WriteLockGuard(pthread_rwlock_t* rwlock) : rwlock_(rwlock) {
        pthread_rwlock_wrlock(rwlock_);
    }
    ~WriteLockGuard() {
        pthread_rwlock_unlock(rwlock_);
    }
private:
    pthread_rwlock_t* rwlock_;
};
