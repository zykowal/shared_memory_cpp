# 共享内存Map实现

这个项目实现了一个基于共享内存的类似map的数据结构，可以在多个进程间共享数据。项目包含了从原始实现到高性能优化版本的完整演进过程。

## 文件说明

### 实现版本
- `status.h` / `status.cpp` - 原始实现，存在一些问题
- `improved_status.h` / `improved_status.cpp` - 改进后的实现，修复了原始版本的问题
- `optimized_status.h` / `optimized_status.cpp` - **优化版本**，使用哈希表实现，性能提升11倍

### 测试程序
- `test_example.cpp` - 基本功能测试
- `multi_process_test.cpp` - 多进程测试
- `performance_test.cpp` - 性能对比测试
- `optimized_example.cpp` - 优化版本使用示例
- `Makefile` - 编译配置

### 文档
- `OPTIMIZATION_DESIGN.md` - 详细的优化设计文档
- `shared_constants.h` - 共享常量定义

## 性能对比

### 测试结果（500个条目）
| 操作类型 | 原始实现 | 优化实现 | 性能提升 |
|---------|---------|---------|---------|
| 插入     | 1.49ms  | 0.07ms  | **21.3x**   |
| 查找     | 0.31ms  | 0.05ms  | **6.2x**    |
| 更新     | 0.34ms  | 0.07ms  | **4.9x**    |
| 删除     | 0.20ms  | 0.03ms  | **6.7x**    |
| **总计** | **2.34ms** | **0.21ms** | **11.1x** |

## 主要改进

### 1. 数据结构优化（优化版本）
- **哈希表**: 从O(n)线性搜索优化到O(1)哈希访问
- **双重哈希**: 使用双重哈希解决冲突，减少聚集
- **开放寻址**: 更好的缓存局部性和内存效率
- **智能rehash**: 自动负载因子管理

### 2. 算法优化
- **MurmurHash3**: 高质量哈希函数，分布均匀
- **位运算优化**: 使用位运算代替模运算
- **哈希值缓存**: 避免重复计算哈希值
- **探测序列优化**: 高效的冲突解决策略

### 3. 内存布局优化
- **表大小**: 2048（2的幂次），便于位运算
- **负载因子**: 最大75%，平衡空间和性能
- **内存对齐**: 268字节条目，缓存友好

### 4. 线程安全改进（所有版本）
- 解决了批量操作中的死锁问题
- 改进了初始化过程中的竞争条件处理
- 正确处理多进程同时创建共享内存的情况

### 5. 功能增强（改进版本开始）
- 添加了 `is_used` 标志，支持删除和槽位重用
- 添加了 `upsertRsc` 功能（更新或插入）
- 添加了 `removeRsc` 删除功能
- 更好的资源清理机制

## API 说明

### 优化版本 API
```cpp
class OptimizedStatusRscManager {
public:
    static OptimizedStatusRscManager &getInstance();
    
    // 基本操作 - O(1)平均复杂度
    int addRsc(int key, const std::string &value);        // 添加（不允许重复key）
    int updateRsc(int key, const std::string &value);     // 更新（key必须存在）
    int upsertRsc(int key, const std::string &value);     // 更新或插入
    std::string getRsc(int key);                          // 获取
    int removeRsc(int key);                               // 删除
    bool isContain(int key);                              // 检查是否存在
    
    // 批量操作
    int batchUpdateRsc(const std::map<int, std::string> &updated_map);
    int batchGetRsc(std::map<int, std::string> &fetched_map);
    
    // 管理操作
    int rscNum();                                         // 获取条目数量
    int clearRsc();                                       // 清空所有数据
    double getLoadFactor();                               // 获取负载因子
    void printStats();                                    // 打印统计信息
    static int cleanup();                                 // 清理共享内存
};
```

### 改进版本 API
```cpp
class StatusRscManager {
    // 与优化版本相同的API，但内部使用线性数组实现
};
```

## 返回值说明

```cpp
#define OK 0              // 成功
#define NOT_FOUND -1      // 未找到
#define NO_SPACE_ERR -2   // 空间不足
#define DUPLICATE_KEY -3  // 重复key
```

## 编译和运行

```bash
# 查看所有可用目标
make help

# 编译优化版本和所有测试
make all

# 运行性能对比测试
make run-perf-test

# 运行优化版本示例
make run-opt-example

# 运行基本测试
make run-test

# 运行多进程测试
make run-multi-test

# 清理编译文件和共享内存
make clean
```

## 使用示例

### 优化版本使用（推荐）
```cpp
#include "optimized_status.h"

int main() {
    auto& manager = OptimizedStatusRscManager::getInstance();
    
    // 添加数据 - O(1)
    manager.addRsc(1, "value1");
    manager.addRsc(2, "value2");
    
    // 查询数据 - O(1)
    std::string value = manager.getRsc(1);
    
    // 更新数据 - O(1)
    manager.updateRsc(1, "new_value1");
    
    // 使用upsert（推荐）- O(1)
    manager.upsertRsc(3, "value3");  // 添加新的
    manager.upsertRsc(3, "updated_value3");  // 更新现有的
    
    // 性能监控
    std::cout << "负载因子: " << manager.getLoadFactor() << std::endl;
    manager.printStats();  // 详细统计信息
    
    return 0;
}
```

### 改进版本使用
```cpp
#include "improved_status.h"

int main() {
    auto& manager = StatusRscManager::getInstance();
    
    // API相同，但性能较低
    manager.addRsc(1, "value1");
    std::string value = manager.getRsc(1);
    
    return 0;
}
```

## 技术特点

### 优化版本特点
- **时间复杂度**: O(1)平均，O(n)最坏情况
- **空间复杂度**: 约548KB（2048条目 × 268字节）
- **负载因子**: 自动管理，最大75%
- **冲突解决**: 双重哈希 + 开放寻址
- **哈希函数**: MurmurHash3变种
- **并发安全**: 进程间互斥锁

### 改进版本特点
- **时间复杂度**: O(n)线性搜索
- **空间复杂度**: 约267KB（1024条目 × 261字节）
- **查找方式**: 线性遍历
- **并发安全**: 进程间互斥锁

## 容量限制

- **优化版本**: 最大1536个条目（75%负载因子）
- **改进版本**: 最大1024个条目
- **值长度**: 最大256字节（所有版本）

## 注意事项

1. **版本选择**: 推荐使用优化版本获得最佳性能
2. **线程安全**: 所有版本都是线程安全和进程安全的
3. **内存清理**: 程序结束时调用相应的 `cleanup()` 清理共享内存
4. **错误处理**: 所有操作都有返回值，请检查返回值确认操作是否成功
5. **共享内存名**: 不同版本使用不同的共享内存名，可以并存

## 性能建议

1. **优先使用优化版本**: 11倍性能提升
2. **使用upsert**: 比分别调用add/update更高效
3. **批量操作**: 对于大量数据操作使用批量API
4. **监控负载因子**: 保持在75%以下获得最佳性能
5. **预估容量**: 根据数据量选择合适的版本

## 进一步优化方向

1. **读写分离锁**: 提高并发读性能
2. **分段锁**: 减少锁竞争
3. **无锁设计**: 使用原子操作
4. **动态扩容**: 支持运行时扩容
5. **内存池**: 减少内存分配开销
6. **持久化**: 支持数据持久化到磁盘

详细的优化设计说明请参考 `OPTIMIZATION_DESIGN.md` 文档。
