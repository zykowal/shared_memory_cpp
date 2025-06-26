#include "rwlock_optimized_status.h"
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
#include <random>

RWLockOptimizedStatusRscManager &RWLockOptimizedStatusRscManager::getInstance() {
    static RWLockOptimizedStatusRscManager instance{};
    return instance;
}

RWLockOptimizedStatusRscManager::RWLockOptimizedStatusRscManager()
    : shared_data_(nullptr), shm_fd_(-1), is_creator_(false) {

    // 尝试打开已存在的共享内存
    shm_fd_ = shm_open("/rwlock_optimized_status_memory", O_RDWR, 0666);

    if (shm_fd_ == -1) {
        // 共享内存不存在，创建新的
        shm_fd_ = shm_open("/rwlock_optimized_status_memory", O_CREAT | O_EXCL | O_RDWR, 0666);
        if (shm_fd_ == -1) {
            if (errno == EEXIST) {
                // 其他进程刚刚创建了，重新尝试打开
                shm_fd_ = shm_open("/rwlock_optimized_status_memory", O_RDWR, 0666);
            }
            if (shm_fd_ == -1) {
                throw std::runtime_error("shm_open failed: " + std::string(strerror(errno)));
            }
        } else {
            is_creator_ = true;
        }
    }

    // 如果是创建者，设置大小
    if (is_creator_) {
        if (ftruncate(shm_fd_, sizeof(RWLockOptimizedSharedData)) == -1) {
            close(shm_fd_);
            shm_unlink("/rwlock_optimized_status_memory");
            throw std::runtime_error("ftruncate failed: " + std::string(strerror(errno)));
        }
    }

    // 映射共享内存
    shared_data_ = static_cast<RWLockOptimizedSharedData *>(
        mmap(nullptr, sizeof(RWLockOptimizedSharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0));

    if (shared_data_ == MAP_FAILED) {
        close(shm_fd_);
        if (is_creator_) {
            shm_unlink("/rwlock_optimized_status_memory");
        }
        throw std::runtime_error("mmap failed: " + std::string(strerror(errno)));
    }

    // 初始化共享数据
    if (is_creator_) {
        // 初始化读写锁属性
        pthread_rwlockattr_t rwlock_attr;
        pthread_rwlockattr_init(&rwlock_attr);
        pthread_rwlockattr_setpshared(&rwlock_attr, PTHREAD_PROCESS_SHARED);

        // 初始化互斥锁属性
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);

        // 初始化锁
        pthread_rwlock_init(&shared_data_->table_rwlock, &rwlock_attr);
        pthread_mutex_init(&shared_data_->init_mutex, &mutex_attr);

        pthread_rwlockattr_destroy(&rwlock_attr);
        pthread_mutexattr_destroy(&mutex_attr);

        // 初始化哈希表
        shared_data_->current_count = 0;
        shared_data_->deleted_count = 0;
        
        // 生成随机哈希种子
        std::random_device rd;
        shared_data_->hash_seed = rd();

        // 初始化所有条目为空
        for (int i = 0; i < RWLOCK_HASH_TABLE_SIZE; ++i) {
            shared_data_->hash_table[i].state = RWLOCK_EMPTY;
            shared_data_->hash_table[i].key = 0;
            shared_data_->hash_table[i].value[0] = '\0';
            shared_data_->hash_table[i].hash_value = 0;
        }

        // 标记初始化完成
        shared_data_->initialized = true;
    } else {
        // 等待初始化完成
        while (!shared_data_->initialized) {
            usleep(1000);
        }
    }
}

RWLockOptimizedStatusRscManager::~RWLockOptimizedStatusRscManager() {
    if (shared_data_ != nullptr && shared_data_ != MAP_FAILED) {
        munmap(shared_data_, sizeof(RWLockOptimizedSharedData));
    }
    if (shm_fd_ != -1) {
        close(shm_fd_);
    }
}

int RWLockOptimizedStatusRscManager::findEntry(int key, uint32_t hash_val) {
    uint32_t hash2_val = hash2(key);
    int pos = hash_val;
    
    for (int step = 0; step < RWLOCK_HASH_TABLE_SIZE; ++step) {
        RWLockHashEntry &entry = shared_data_->hash_table[pos];
        
        if (entry.state == RWLOCK_EMPTY) {
            return -1;
        }
        
        if (entry.state == RWLOCK_OCCUPIED && entry.key == key) {
            return pos;
        }
        
        pos = getNextProbe(pos, step + 1, hash2_val);
    }
    
    return -1;
}

int RWLockOptimizedStatusRscManager::findEmptySlot(int key, uint32_t hash_val) {
    uint32_t hash2_val = hash2(key);
    int pos = hash_val;
    int first_deleted = -1;
    
    for (int step = 0; step < RWLOCK_HASH_TABLE_SIZE; ++step) {
        RWLockHashEntry &entry = shared_data_->hash_table[pos];
        
        if (entry.state == RWLOCK_EMPTY) {
            return first_deleted != -1 ? first_deleted : pos;
        }
        
        if (entry.state == RWLOCK_DELETED && first_deleted == -1) {
            first_deleted = pos;
        }
        
        if (entry.state == RWLOCK_OCCUPIED && entry.key == key) {
            return -1;
        }
        
        pos = getNextProbe(pos, step + 1, hash2_val);
    }
    
    return first_deleted;
}

bool RWLockOptimizedStatusRscManager::needRehash() const {
    return (shared_data_->current_count + shared_data_->deleted_count) > 
           static_cast<int>(RWLOCK_HASH_TABLE_SIZE * RWLOCK_MAX_LOAD_FACTOR);
}

int RWLockOptimizedStatusRscManager::rehashIfNeeded() {
    if (!needRehash()) {
        return OK;
    }
    
    std::map<int, std::string> temp_data;
    
    for (int i = 0; i < RWLOCK_HASH_TABLE_SIZE; ++i) {
        if (shared_data_->hash_table[i].state == RWLOCK_OCCUPIED) {
            temp_data[shared_data_->hash_table[i].key] = shared_data_->hash_table[i].value;
        }
    }
    
    for (int i = 0; i < RWLOCK_HASH_TABLE_SIZE; ++i) {
        shared_data_->hash_table[i].state = RWLOCK_EMPTY;
    }
    shared_data_->current_count = 0;
    shared_data_->deleted_count = 0;
    
    for (const auto &pair : temp_data) {
        uint32_t hash_val = hash(pair.first);
        int pos = findEmptySlot(pair.first, hash_val);
        if (pos == -1) {
            return NO_SPACE_ERR;
        }
        
        RWLockHashEntry &entry = shared_data_->hash_table[pos];
        entry.key = pair.first;
        strncpy(entry.value, pair.second.c_str(), MAX_VALUE_LEN - 1);
        entry.value[MAX_VALUE_LEN - 1] = '\0';
        entry.state = RWLOCK_OCCUPIED;
        entry.hash_value = hash_val;
        shared_data_->current_count++;
    }
    
    return OK;
}

// 写操作 - 使用写锁
int RWLockOptimizedStatusRscManager::addRsc(int rsc_key, const std::string &rsc_value) {
    if (rsc_value.length() >= MAX_VALUE_LEN) {
        return NO_SPACE_ERR;
    }

    WriteLockGuard lock(&shared_data_->table_rwlock);
    
    if (rehashIfNeeded() != OK) {
        return NO_SPACE_ERR;
    }
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEmptySlot(rsc_key, hash_val);
    
    if (pos == -1) {
        return shared_data_->current_count >= RWLOCK_MAX_ENTRIES ? NO_SPACE_ERR : DUPLICATE_KEY;
    }
    
    RWLockHashEntry &entry = shared_data_->hash_table[pos];
    if (entry.state == RWLOCK_DELETED) {
        shared_data_->deleted_count--;
    }
    
    entry.key = rsc_key;
    strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
    entry.value[MAX_VALUE_LEN - 1] = '\0';
    entry.state = RWLOCK_OCCUPIED;
    entry.hash_value = hash_val;
    shared_data_->current_count++;
    
    return OK;
}

// 写操作 - 使用写锁
int RWLockOptimizedStatusRscManager::updateRsc(int rsc_key, const std::string &rsc_value) {
    if (rsc_value.length() >= MAX_VALUE_LEN) {
        return NO_SPACE_ERR;
    }

    WriteLockGuard lock(&shared_data_->table_rwlock);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos == -1) {
        return NOT_FOUND;
    }
    
    RWLockHashEntry &entry = shared_data_->hash_table[pos];
    strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
    entry.value[MAX_VALUE_LEN - 1] = '\0';
    
    return OK;
}

// 写操作 - 使用写锁
int RWLockOptimizedStatusRscManager::upsertRsc(int rsc_key, const std::string &rsc_value) {
    if (rsc_value.length() >= MAX_VALUE_LEN) {
        return NO_SPACE_ERR;
    }

    WriteLockGuard lock(&shared_data_->table_rwlock);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos != -1) {
        // 更新现有条目
        RWLockHashEntry &entry = shared_data_->hash_table[pos];
        strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
        entry.value[MAX_VALUE_LEN - 1] = '\0';
        return OK;
    }
    
    // 添加新条目
    if (rehashIfNeeded() != OK) {
        return NO_SPACE_ERR;
    }
    
    pos = findEmptySlot(rsc_key, hash_val);
    if (pos == -1) {
        return NO_SPACE_ERR;
    }
    
    RWLockHashEntry &entry = shared_data_->hash_table[pos];
    if (entry.state == RWLOCK_DELETED) {
        shared_data_->deleted_count--;
    }
    
    entry.key = rsc_key;
    strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
    entry.value[MAX_VALUE_LEN - 1] = '\0';
    entry.state = RWLOCK_OCCUPIED;
    entry.hash_value = hash_val;
    shared_data_->current_count++;
    
    return OK;
}

// 读操作 - 使用读锁
std::string RWLockOptimizedStatusRscManager::getRsc(int rsc_key) {
    ReadLockGuard lock(&shared_data_->table_rwlock);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos == -1) {
        return "";
    }
    
    return std::string(shared_data_->hash_table[pos].value);
}

// 写操作 - 使用写锁
int RWLockOptimizedStatusRscManager::removeRsc(int rsc_key) {
    WriteLockGuard lock(&shared_data_->table_rwlock);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos == -1) {
        return NOT_FOUND;
    }
    
    shared_data_->hash_table[pos].state = RWLOCK_DELETED;
    shared_data_->current_count--;
    shared_data_->deleted_count++;
    
    return OK;
}

// 读操作 - 使用读锁
bool RWLockOptimizedStatusRscManager::isContain(int rsc_key) {
    ReadLockGuard lock(&shared_data_->table_rwlock);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    return pos != -1;
}

// 写操作 - 使用写锁
int RWLockOptimizedStatusRscManager::batchUpdateRsc(const std::map<int, std::string> &updated_map) {
    WriteLockGuard lock(&shared_data_->table_rwlock);
    
    int success_count = 0;
    for (const auto &pair : updated_map) {
        if (pair.second.length() >= MAX_VALUE_LEN) {
            continue;
        }
        
        uint32_t hash_val = hash(pair.first);
        int pos = findEntry(pair.first, hash_val);
        
        if (pos != -1) {
            RWLockHashEntry &entry = shared_data_->hash_table[pos];
            strncpy(entry.value, pair.second.c_str(), MAX_VALUE_LEN - 1);
            entry.value[MAX_VALUE_LEN - 1] = '\0';
            success_count++;
        }
    }
    
    return success_count;
}

// 读操作 - 使用读锁
int RWLockOptimizedStatusRscManager::batchGetRsc(std::map<int, std::string> &fetched_map) {
    ReadLockGuard lock(&shared_data_->table_rwlock);
    
    fetched_map.clear();
    for (int i = 0; i < RWLOCK_HASH_TABLE_SIZE; ++i) {
        if (shared_data_->hash_table[i].state == RWLOCK_OCCUPIED) {
            fetched_map[shared_data_->hash_table[i].key] = shared_data_->hash_table[i].value;
        }
    }
    
    return fetched_map.size();
}

// 读操作 - 使用读锁
int RWLockOptimizedStatusRscManager::rscNum() {
    ReadLockGuard lock(&shared_data_->table_rwlock);
    return shared_data_->current_count;
}

// 写操作 - 使用写锁
int RWLockOptimizedStatusRscManager::clearRsc() {
    WriteLockGuard lock(&shared_data_->table_rwlock);
    
    for (int i = 0; i < RWLOCK_HASH_TABLE_SIZE; ++i) {
        shared_data_->hash_table[i].state = RWLOCK_EMPTY;
    }
    shared_data_->current_count = 0;
    shared_data_->deleted_count = 0;
    
    return OK;
}

// 读操作 - 使用读锁
double RWLockOptimizedStatusRscManager::getLoadFactor() {
    ReadLockGuard lock(&shared_data_->table_rwlock);
    return static_cast<double>(shared_data_->current_count) / RWLOCK_HASH_TABLE_SIZE;
}

// 读操作 - 使用读锁
void RWLockOptimizedStatusRscManager::printStats() {
    ReadLockGuard lock(&shared_data_->table_rwlock);
    
    std::cout << "=== RWLock Hash Table Statistics ===" << std::endl;
    std::cout << "Table Size: " << RWLOCK_HASH_TABLE_SIZE << std::endl;
    std::cout << "Current Count: " << shared_data_->current_count << std::endl;
    std::cout << "Deleted Count: " << shared_data_->deleted_count << std::endl;
    std::cout << "Load Factor: " << static_cast<double>(shared_data_->current_count) / RWLOCK_HASH_TABLE_SIZE << std::endl;
    std::cout << "Hash Seed: " << shared_data_->hash_seed << std::endl;
    
    // 计算探测距离统计
    int total_probes = 0;
    int max_probes = 0;
    int occupied_slots = 0;
    
    for (int i = 0; i < RWLOCK_HASH_TABLE_SIZE; ++i) {
        if (shared_data_->hash_table[i].state == RWLOCK_OCCUPIED) {
            occupied_slots++;
            int expected_pos = shared_data_->hash_table[i].hash_value;
            int actual_pos = i;
            int probes = 1;
            
            if (actual_pos != expected_pos) {
                uint32_t hash2_val = hash2(shared_data_->hash_table[i].key);
                int pos = expected_pos;
                for (int step = 0; step < RWLOCK_HASH_TABLE_SIZE; ++step) {
                    if (pos == actual_pos) {
                        probes = step + 1;
                        break;
                    }
                    pos = getNextProbe(pos, step + 1, hash2_val);
                }
            }
            
            total_probes += probes;
            max_probes = std::max(max_probes, probes);
        }
    }
    
    if (occupied_slots > 0) {
        std::cout << "Average Probe Distance: " << static_cast<double>(total_probes) / occupied_slots << std::endl;
        std::cout << "Max Probe Distance: " << max_probes << std::endl;
    }
}

int RWLockOptimizedStatusRscManager::cleanup() {
    if (shm_unlink("/rwlock_optimized_status_memory") == -1) {
        if (errno != ENOENT) {
            return -1;
        }
    }
    return OK;
}
