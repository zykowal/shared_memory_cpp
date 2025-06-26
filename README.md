# 共享内存Map实现

这个项目实现了一个基于共享内存的类似map的数据结构，可以在多个进程间共享数据。

## 文件说明

### 原始实现
- `status.h` / `status.cpp` - 原始实现，存在一些问题

### 改进实现  
- `improved_status.h` / `improved_status.cpp` - 改进后的实现，修复了原始版本的问题

### 测试程序
- `test_example.cpp` - 基本功能测试
- `multi_process_test.cpp` - 多进程测试
- `Makefile` - 编译配置

## 主要改进

### 1. 修复线程安全问题
- 解决了批量操作中的死锁问题
- 改进了初始化过程中的竞争条件处理

### 2. 改进数据结构
- 添加了 `is_used` 标志，支持删除和槽位重用
- 添加了 `upsertRsc` 功能（更新或插入）
- 添加了 `removeRsc` 删除功能

### 3. 改进进程间同步
- 正确处理多进程同时创建共享内存的情况
- 使用进程间互斥锁进行同步
- 区分创建者和使用者的职责

### 4. 改进内存管理
- 更好的资源清理机制
- 手动控制共享内存的生命周期

## API 说明

```cpp
class StatusRscManager {
public:
    static StatusRscManager &getInstance();
    
    // 基本操作
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
    static int cleanup();                                 // 清理共享内存
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

# 编译改进版本和所有测试
make all

# 运行基本测试
make run-test

# 运行多进程测试
make run-multi-test

# 清理编译文件和共享内存
make clean
```

## 使用示例

### 基本使用
```cpp
#include "improved_status.h"

int main() {
    auto& manager = StatusRscManager::getInstance();
    
    // 添加数据
    manager.addRsc(1, "value1");
    manager.addRsc(2, "value2");
    
    // 查询数据
    std::string value = manager.getRsc(1);
    
    // 更新数据
    manager.updateRsc(1, "new_value1");
    
    // 使用upsert（推荐）
    manager.upsertRsc(3, "value3");  // 添加新的
    manager.upsertRsc(3, "updated_value3");  // 更新现有的
    
    return 0;
}
```

### 多进程使用
每个进程都可以通过 `StatusRscManager::getInstance()` 获取同一个共享内存实例，实现进程间数据共享。

## 注意事项

1. **线程安全**: 所有操作都是线程安全和进程安全的
2. **容量限制**: 最大支持1024个条目，每个值最大256字节
3. **内存清理**: 程序结束时调用 `StatusRscManager::cleanup()` 清理共享内存
4. **错误处理**: 所有操作都有返回值，请检查返回值确认操作是否成功

## 性能特点

- **查找复杂度**: O(n) - 使用线性搜索
- **并发性能**: 支持多进程并发访问
- **内存效率**: 固定大小的共享内存区域

## 可能的进一步优化

1. 使用哈希表或B+树提高查找效率
2. 实现动态扩容机制
3. 添加数据持久化功能
4. 实现更细粒度的锁机制
