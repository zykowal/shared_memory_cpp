# 共享内存Map优化设计文档

## 概述

本文档详细介绍了对共享内存Map数据结构的优化设计，将原始的线性数组结构优化为高效的哈希表结构，实现了显著的性能提升。

## 性能对比

### 测试结果（500个条目）
| 操作类型 | 原始实现 | 优化实现 | 性能提升 |
|---------|---------|---------|---------|
| 插入     | 1.49ms  | 0.07ms  | 21.3x   |
| 查找     | 0.31ms  | 0.05ms  | 6.2x    |
| 更新     | 0.34ms  | 0.07ms  | 4.9x    |
| 删除     | 0.20ms  | 0.03ms  | 6.7x    |
| **总计** | **2.34ms** | **0.21ms** | **11.1x** |

## 核心优化策略

### 1. 数据结构优化

#### 原始实现问题
```cpp
// 线性数组结构 - O(n)复杂度
struct SharedEntry {
    int key;
    char value[MAX_VALUE_LEN];
    bool is_used;
};
SharedEntry status_map[STATUS_MAX_ENTRIES];  // 线性搜索
```

#### 优化后设计
```cpp
// 哈希表结构 - O(1)平均复杂度
struct HashEntry {
    int key;
    char value[MAX_VALUE_LEN];
    EntryState state;           // EMPTY/OCCUPIED/DELETED
    uint32_t hash_value;        // 缓存哈希值
};
HashEntry hash_table[HASH_TABLE_SIZE];  // 哈希访问
```

### 2. 哈希算法设计

#### 主哈希函数
使用MurmurHash3的简化版本，针对32位整数优化：
```cpp
uint32_t hash(int key) const {
    uint32_t k = static_cast<uint32_t>(key);
    k ^= shared_data_->hash_seed;  // 防哈希攻击
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k & (HASH_TABLE_SIZE - 1);  // 位运算代替模运算
}
```

#### 双重哈希冲突解决
```cpp
uint32_t hash2(int key) const {
    // 第二个哈希函数，确保步长为奇数
    uint32_t k = static_cast<uint32_t>(key);
    k ^= shared_data_->hash_seed + 0x9e3779b9;
    // ... 哈希计算 ...
    return (k & (HASH_TABLE_SIZE - 1)) | 1;  // 确保奇数
}
```

### 3. 冲突解决策略

#### 开放寻址法 + 双重哈希
- **优势**: 内存局部性好，缓存友好
- **探测序列**: `pos = (hash1 + i * hash2) % table_size`
- **删除处理**: 使用DELETED标记，支持槽位重用

#### 探测序列优化
```cpp
int getNextProbe(int current_pos, int step, uint32_t hash2_val) const {
    return (current_pos + step * hash2_val) & (HASH_TABLE_SIZE - 1);
}
```

### 4. 内存布局优化

#### 表大小设计
- **大小**: 2048 (2的幂次，便于位运算)
- **负载因子**: 最大75%，平衡空间和性能
- **最大条目**: 1536个

#### 缓存友好设计
```cpp
struct HashEntry {
    int key;                    // 4字节
    char value[MAX_VALUE_LEN];  // 256字节
    EntryState state;           // 4字节
    uint32_t hash_value;        // 4字节 - 缓存哈希值
};  // 总计268字节，内存对齐友好
```

### 5. 并发控制优化

#### 锁粒度
- 使用单个表级锁，简化实现
- 支持进程间同步
- 递归锁支持批量操作

#### 初始化优化
```cpp
// 随机哈希种子，防止哈希攻击
std::random_device rd;
shared_data_->hash_seed = rd();
```

## 关键算法实现

### 1. 查找算法
```cpp
int findEntry(int key, uint32_t hash_val) {
    uint32_t hash2_val = hash2(key);
    int pos = hash_val;
    
    for (int step = 0; step < HASH_TABLE_SIZE; ++step) {
        HashEntry &entry = shared_data_->hash_table[pos];
        
        if (entry.state == EMPTY) return -1;        // 未找到
        if (entry.state == OCCUPIED && entry.key == key) return pos;  // 找到
        
        pos = getNextProbe(pos, step + 1, hash2_val);  // 继续探测
    }
    return -1;  // 表满
}
```

### 2. 插入算法
```cpp
int findEmptySlot(int key, uint32_t hash_val) {
    uint32_t hash2_val = hash2(key);
    int pos = hash_val;
    int first_deleted = -1;
    
    for (int step = 0; step < HASH_TABLE_SIZE; ++step) {
        HashEntry &entry = shared_data_->hash_table[pos];
        
        if (entry.state == EMPTY) {
            return first_deleted != -1 ? first_deleted : pos;
        }
        if (entry.state == DELETED && first_deleted == -1) {
            first_deleted = pos;  // 记录第一个删除位置
        }
        if (entry.state == OCCUPIED && entry.key == key) {
            return -1;  // 键已存在
        }
        
        pos = getNextProbe(pos, step + 1, hash2_val);
    }
    return first_deleted;
}
```

### 3. 自动Rehash
```cpp
bool needRehash() const {
    return (shared_data_->current_count + shared_data_->deleted_count) > 
           static_cast<int>(HASH_TABLE_SIZE * MAX_LOAD_FACTOR);
}
```

## 性能特征分析

### 时间复杂度
| 操作 | 平均情况 | 最坏情况 | 原始实现 |
|------|---------|---------|---------|
| 查找 | O(1)    | O(n)    | O(n)    |
| 插入 | O(1)    | O(n)    | O(n)    |
| 删除 | O(1)    | O(n)    | O(n)    |
| 更新 | O(1)    | O(n)    | O(n)    |

### 空间复杂度
- **哈希表**: 2048 * 268字节 ≈ 548KB
- **原始数组**: 1024 * 261字节 ≈ 267KB
- **空间开销**: 约2倍，换取显著性能提升

### 负载因子影响
```
负载因子 0.25: 平均探测距离 ~1.15
负载因子 0.50: 平均探测距离 ~1.50
负载因子 0.75: 平均探测距离 ~2.50
```

## 使用示例

### 基本操作
```cpp
#include "optimized_status.h"

auto& manager = OptimizedStatusRscManager::getInstance();

// 添加数据 - O(1)
manager.addRsc(1001, "user_data");

// 查找数据 - O(1)
std::string value = manager.getRsc(1001);

// 更新或插入 - O(1)
manager.upsertRsc(1001, "updated_data");

// 删除数据 - O(1)
manager.removeRsc(1001);
```

### 性能监控
```cpp
// 获取负载因子
double load_factor = manager.getLoadFactor();

// 打印详细统计
manager.printStats();
```

## 进一步优化建议

### 1. 读写分离锁
```cpp
// 使用读写锁提高并发性能
pthread_rwlock_t rw_lock;
```

### 2. 分段锁
```cpp
// 将哈希表分段，减少锁竞争
const int SEGMENT_COUNT = 16;
pthread_mutex_t segment_locks[SEGMENT_COUNT];
```

### 3. 无锁设计
```cpp
// 使用原子操作和CAS实现无锁哈希表
std::atomic<HashEntry> hash_table[HASH_TABLE_SIZE];
```

### 4. 动态扩容
```cpp
// 实现动态扩容机制
int resize_table(int new_size);
```

### 5. 内存池
```cpp
// 使用内存池减少内存分配开销
class MemoryPool {
    // 预分配内存块
};
```

## 总结

通过将线性数组结构优化为哈希表结构，我们实现了：

1. **性能提升**: 11倍整体性能提升
2. **算法优化**: O(n) → O(1) 时间复杂度
3. **内存效率**: 更好的缓存局部性
4. **扩展性**: 支持更大规模的数据存储
5. **可维护性**: 清晰的模块化设计

这个优化版本在保持原有API兼容性的同时，显著提升了性能，特别适合高频访问的多进程共享内存场景。
