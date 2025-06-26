#include "optimized_status.h"
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

OptimizedStatusRscManager &OptimizedStatusRscManager::getInstance() {
    static OptimizedStatusRscManager instance{};
    return instance;
}

OptimizedStatusRscManager::OptimizedStatusRscManager()
    : shared_data_(nullptr), shm_fd_(-1), is_creator_(false) {

    // 尝试打开已存在的共享内存
    shm_fd_ = shm_open("/optimized_status_memory", O_RDWR, 0666);

    if (shm_fd_ == -1) {
        // 共享内存不存在，创建新的
        shm_fd_ = shm_open("/optimized_status_memory", O_CREAT | O_EXCL | O_RDWR, 0666);
        if (shm_fd_ == -1) {
            if (errno == EEXIST) {
                // 其他进程刚刚创建了，重新尝试打开
                shm_fd_ = shm_open("/optimized_status_memory", O_RDWR, 0666);
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
        if (ftruncate(shm_fd_, sizeof(OptimizedSharedData)) == -1) {
            close(shm_fd_);
            shm_unlink("/optimized_status_memory");
            throw std::runtime_error("ftruncate failed: " + std::string(strerror(errno)));
        }
    }

    // 映射共享内存
    shared_data_ = static_cast<OptimizedSharedData *>(
        mmap(nullptr, sizeof(OptimizedSharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0));

    if (shared_data_ == MAP_FAILED) {
        close(shm_fd_);
        if (is_creator_) {
            shm_unlink("/optimized_status_memory");
        }
        throw std::runtime_error("mmap failed: " + std::string(strerror(errno)));
    }

    // 初始化共享数据
    if (is_creator_) {
        // 初始化进程间互斥锁属性
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);

        // 初始化互斥锁
        pthread_mutex_init(&shared_data_->table_mutex, &mutex_attr);
        pthread_mutex_init(&shared_data_->init_mutex, &mutex_attr);

        pthread_mutexattr_destroy(&mutex_attr);

        // 初始化哈希表
        shared_data_->current_count = 0;
        shared_data_->deleted_count = 0;
        
        // 生成随机哈希种子
        std::random_device rd;
        shared_data_->hash_seed = rd();

        // 初始化所有条目为空
        for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
            shared_data_->hash_table[i].state = EMPTY;
            shared_data_->hash_table[i].key = 0;
            shared_data_->hash_table[i].value[0] = '\0';
            shared_data_->hash_table[i].hash_value = 0;
        }

        // 标记初始化完成
        shared_data_->initialized = true;
    } else {
        // 等待初始化完成
        while (!shared_data_->initialized) {
            usleep(1000);  // 等待1ms
        }
    }
}

OptimizedStatusRscManager::~OptimizedStatusRscManager() {
    if (shared_data_ != nullptr && shared_data_ != MAP_FAILED) {
        munmap(shared_data_, sizeof(OptimizedSharedData));
    }
    if (shm_fd_ != -1) {
        close(shm_fd_);
    }
}

int OptimizedStatusRscManager::findEntry(int key, uint32_t hash_val) {
    uint32_t hash2_val = hash2(key);
    int pos = hash_val;
    
    for (int step = 0; step < HASH_TABLE_SIZE; ++step) {
        HashEntry &entry = shared_data_->hash_table[pos];
        
        if (entry.state == EMPTY) {
            return -1;  // 未找到
        }
        
        if (entry.state == OCCUPIED && entry.key == key) {
            return pos;  // 找到
        }
        
        // 继续探测
        pos = getNextProbe(pos, step + 1, hash2_val);
    }
    
    return -1;  // 表满，未找到
}

int OptimizedStatusRscManager::findEmptySlot(int key, uint32_t hash_val) {
    uint32_t hash2_val = hash2(key);
    int pos = hash_val;
    int first_deleted = -1;
    
    for (int step = 0; step < HASH_TABLE_SIZE; ++step) {
        HashEntry &entry = shared_data_->hash_table[pos];
        
        if (entry.state == EMPTY) {
            return first_deleted != -1 ? first_deleted : pos;
        }
        
        if (entry.state == DELETED && first_deleted == -1) {
            first_deleted = pos;
        }
        
        if (entry.state == OCCUPIED && entry.key == key) {
            return -1;  // 键已存在
        }
        
        pos = getNextProbe(pos, step + 1, hash2_val);
    }
    
    return first_deleted;  // 返回第一个删除的位置，如果没有则返回-1
}

bool OptimizedStatusRscManager::needRehash() const {
    return (shared_data_->current_count + shared_data_->deleted_count) > 
           static_cast<int>(HASH_TABLE_SIZE * MAX_LOAD_FACTOR);
}

int OptimizedStatusRscManager::rehashIfNeeded() {
    if (!needRehash()) {
        return OK;
    }
    
    // 简单的清理策略：重新插入所有有效条目
    // 在实际应用中，可能需要更复杂的rehash策略
    std::map<int, std::string> temp_data;
    
    // 收集所有有效数据
    for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
        if (shared_data_->hash_table[i].state == OCCUPIED) {
            temp_data[shared_data_->hash_table[i].key] = shared_data_->hash_table[i].value;
        }
    }
    
    // 清空表
    for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
        shared_data_->hash_table[i].state = EMPTY;
    }
    shared_data_->current_count = 0;
    shared_data_->deleted_count = 0;
    
    // 重新插入数据
    for (const auto &pair : temp_data) {
        uint32_t hash_val = hash(pair.first);
        int pos = findEmptySlot(pair.first, hash_val);
        if (pos == -1) {
            return NO_SPACE_ERR;
        }
        
        HashEntry &entry = shared_data_->hash_table[pos];
        entry.key = pair.first;
        strncpy(entry.value, pair.second.c_str(), MAX_VALUE_LEN - 1);
        entry.value[MAX_VALUE_LEN - 1] = '\0';
        entry.state = OCCUPIED;
        entry.hash_value = hash_val;
        shared_data_->current_count++;
    }
    
    return OK;
}

int OptimizedStatusRscManager::addRsc(int rsc_key, const std::string &rsc_value) {
    if (rsc_value.length() >= MAX_VALUE_LEN) {
        return NO_SPACE_ERR;
    }

    pthread_mutex_lock(&shared_data_->table_mutex);
    
    // 检查是否需要rehash
    if (rehashIfNeeded() != OK) {
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return NO_SPACE_ERR;
    }
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEmptySlot(rsc_key, hash_val);
    
    if (pos == -1) {
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return shared_data_->current_count >= MAX_ENTRIES ? NO_SPACE_ERR : DUPLICATE_KEY;
    }
    
    HashEntry &entry = shared_data_->hash_table[pos];
    if (entry.state == DELETED) {
        shared_data_->deleted_count--;
    }
    
    entry.key = rsc_key;
    strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
    entry.value[MAX_VALUE_LEN - 1] = '\0';
    entry.state = OCCUPIED;
    entry.hash_value = hash_val;
    shared_data_->current_count++;
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return OK;
}

int OptimizedStatusRscManager::updateRsc(int rsc_key, const std::string &rsc_value) {
    if (rsc_value.length() >= MAX_VALUE_LEN) {
        return NO_SPACE_ERR;
    }

    pthread_mutex_lock(&shared_data_->table_mutex);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos == -1) {
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return NOT_FOUND;
    }
    
    HashEntry &entry = shared_data_->hash_table[pos];
    strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
    entry.value[MAX_VALUE_LEN - 1] = '\0';
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return OK;
}

int OptimizedStatusRscManager::upsertRsc(int rsc_key, const std::string &rsc_value) {
    if (rsc_value.length() >= MAX_VALUE_LEN) {
        return NO_SPACE_ERR;
    }

    pthread_mutex_lock(&shared_data_->table_mutex);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos != -1) {
        // 更新现有条目
        HashEntry &entry = shared_data_->hash_table[pos];
        strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
        entry.value[MAX_VALUE_LEN - 1] = '\0';
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return OK;
    }
    
    // 添加新条目
    if (rehashIfNeeded() != OK) {
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return NO_SPACE_ERR;
    }
    
    pos = findEmptySlot(rsc_key, hash_val);
    if (pos == -1) {
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return NO_SPACE_ERR;
    }
    
    HashEntry &entry = shared_data_->hash_table[pos];
    if (entry.state == DELETED) {
        shared_data_->deleted_count--;
    }
    
    entry.key = rsc_key;
    strncpy(entry.value, rsc_value.c_str(), MAX_VALUE_LEN - 1);
    entry.value[MAX_VALUE_LEN - 1] = '\0';
    entry.state = OCCUPIED;
    entry.hash_value = hash_val;
    shared_data_->current_count++;
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return OK;
}

std::string OptimizedStatusRscManager::getRsc(int rsc_key) {
    pthread_mutex_lock(&shared_data_->table_mutex);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos == -1) {
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return "";
    }
    
    std::string result(shared_data_->hash_table[pos].value);
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return result;
}

int OptimizedStatusRscManager::removeRsc(int rsc_key) {
    pthread_mutex_lock(&shared_data_->table_mutex);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    if (pos == -1) {
        pthread_mutex_unlock(&shared_data_->table_mutex);
        return NOT_FOUND;
    }
    
    shared_data_->hash_table[pos].state = DELETED;
    shared_data_->current_count--;
    shared_data_->deleted_count++;
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return OK;
}

bool OptimizedStatusRscManager::isContain(int rsc_key) {
    pthread_mutex_lock(&shared_data_->table_mutex);
    
    uint32_t hash_val = hash(rsc_key);
    int pos = findEntry(rsc_key, hash_val);
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return pos != -1;
}

int OptimizedStatusRscManager::batchUpdateRsc(const std::map<int, std::string> &updated_map) {
    pthread_mutex_lock(&shared_data_->table_mutex);
    
    int success_count = 0;
    for (const auto &pair : updated_map) {
        if (pair.second.length() >= MAX_VALUE_LEN) {
            continue;
        }
        
        uint32_t hash_val = hash(pair.first);
        int pos = findEntry(pair.first, hash_val);
        
        if (pos != -1) {
            HashEntry &entry = shared_data_->hash_table[pos];
            strncpy(entry.value, pair.second.c_str(), MAX_VALUE_LEN - 1);
            entry.value[MAX_VALUE_LEN - 1] = '\0';
            success_count++;
        }
    }
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return success_count;
}

int OptimizedStatusRscManager::batchGetRsc(std::map<int, std::string> &fetched_map) {
    pthread_mutex_lock(&shared_data_->table_mutex);
    
    fetched_map.clear();
    for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
        if (shared_data_->hash_table[i].state == OCCUPIED) {
            fetched_map[shared_data_->hash_table[i].key] = shared_data_->hash_table[i].value;
        }
    }
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return fetched_map.size();
}

int OptimizedStatusRscManager::rscNum() {
    pthread_mutex_lock(&shared_data_->table_mutex);
    int count = shared_data_->current_count;
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return count;
}

int OptimizedStatusRscManager::clearRsc() {
    pthread_mutex_lock(&shared_data_->table_mutex);
    
    for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
        shared_data_->hash_table[i].state = EMPTY;
    }
    shared_data_->current_count = 0;
    shared_data_->deleted_count = 0;
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return OK;
}

double OptimizedStatusRscManager::getLoadFactor() {
    pthread_mutex_lock(&shared_data_->table_mutex);
    double load_factor = static_cast<double>(shared_data_->current_count) / HASH_TABLE_SIZE;
    pthread_mutex_unlock(&shared_data_->table_mutex);
    return load_factor;
}

void OptimizedStatusRscManager::printStats() {
    pthread_mutex_lock(&shared_data_->table_mutex);
    
    std::cout << "=== Hash Table Statistics ===" << std::endl;
    std::cout << "Table Size: " << HASH_TABLE_SIZE << std::endl;
    std::cout << "Current Count: " << shared_data_->current_count << std::endl;
    std::cout << "Deleted Count: " << shared_data_->deleted_count << std::endl;
    std::cout << "Load Factor: " << static_cast<double>(shared_data_->current_count) / HASH_TABLE_SIZE << std::endl;
    std::cout << "Hash Seed: " << shared_data_->hash_seed << std::endl;
    
    // 计算探测距离统计
    int total_probes = 0;
    int max_probes = 0;
    int occupied_slots = 0;
    
    for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
        if (shared_data_->hash_table[i].state == OCCUPIED) {
            occupied_slots++;
            int expected_pos = shared_data_->hash_table[i].hash_value;
            int actual_pos = i;
            int probes = 1;
            
            // 计算探测距离
            if (actual_pos != expected_pos) {
                uint32_t hash2_val = hash2(shared_data_->hash_table[i].key);
                int pos = expected_pos;
                for (int step = 0; step < HASH_TABLE_SIZE; ++step) {
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
    
    pthread_mutex_unlock(&shared_data_->table_mutex);
}

int OptimizedStatusRscManager::cleanup() {
    if (shm_unlink("/optimized_status_memory") == -1) {
        if (errno != ENOENT) {
            return -1;
        }
    }
    return OK;
}
