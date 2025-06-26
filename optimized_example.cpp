#include "optimized_status.h"
#include <iostream>
#include <chrono>
#include <random>

void demonstrateBasicOperations() {
    std::cout << "=== 基本操作演示 ===" << std::endl;
    
    auto& manager = OptimizedStatusRscManager::getInstance();
    manager.clearRsc();
    
    // 添加数据
    std::cout << "1. 添加数据:" << std::endl;
    manager.addRsc(1001, "用户数据1");
    manager.addRsc(1002, "用户数据2");
    manager.addRsc(1003, "用户数据3");
    std::cout << "   添加了3条记录" << std::endl;
    
    // 查询数据
    std::cout << "2. 查询数据:" << std::endl;
    std::cout << "   Key 1001: " << manager.getRsc(1001) << std::endl;
    std::cout << "   Key 1002: " << manager.getRsc(1002) << std::endl;
    
    // 更新数据
    std::cout << "3. 更新数据:" << std::endl;
    manager.updateRsc(1001, "更新后的用户数据1");
    std::cout << "   Key 1001: " << manager.getRsc(1001) << std::endl;
    
    // 使用upsert
    std::cout << "4. Upsert操作:" << std::endl;
    manager.upsertRsc(1004, "新增数据");  // 新增
    manager.upsertRsc(1002, "Upsert更新数据");  // 更新
    std::cout << "   Key 1004: " << manager.getRsc(1004) << std::endl;
    std::cout << "   Key 1002: " << manager.getRsc(1002) << std::endl;
    
    // 检查存在性
    std::cout << "5. 检查存在性:" << std::endl;
    std::cout << "   Key 1001 存在: " << (manager.isContain(1001) ? "是" : "否") << std::endl;
    std::cout << "   Key 9999 存在: " << (manager.isContain(9999) ? "是" : "否") << std::endl;
    
    // 删除数据
    std::cout << "6. 删除数据:" << std::endl;
    manager.removeRsc(1003);
    std::cout << "   删除Key 1003后，存在性: " << (manager.isContain(1003) ? "是" : "否") << std::endl;
    
    // 统计信息
    std::cout << "7. 统计信息:" << std::endl;
    std::cout << "   当前记录数: " << manager.rscNum() << std::endl;
    std::cout << "   负载因子: " << manager.getLoadFactor() << std::endl;
}

void demonstratePerformance() {
    std::cout << "\n=== 性能演示 ===" << std::endl;
    
    auto& manager = OptimizedStatusRscManager::getInstance();
    manager.clearRsc();
    
    const int test_count = 1000;
    std::vector<int> keys;
    std::vector<std::string> values;
    
    // 生成测试数据
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100000);
    
    for (int i = 0; i < test_count; ++i) {
        keys.push_back(dis(gen));
        values.push_back("test_value_" + std::to_string(i));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 批量插入
    for (int i = 0; i < test_count; ++i) {
        manager.addRsc(keys[i], values[i]);
    }
    
    auto insert_end = std::chrono::high_resolution_clock::now();
    
    // 批量查询
    for (int key : keys) {
        manager.getRsc(key);
    }
    
    auto lookup_end = std::chrono::high_resolution_clock::now();
    
    // 计算时间
    auto insert_time = std::chrono::duration_cast<std::chrono::microseconds>(insert_end - start);
    auto lookup_time = std::chrono::duration_cast<std::chrono::microseconds>(lookup_end - insert_end);
    
    std::cout << "测试" << test_count << "条记录:" << std::endl;
    std::cout << "  插入时间: " << insert_time.count() / 1000.0 << " ms" << std::endl;
    std::cout << "  查询时间: " << lookup_time.count() / 1000.0 << " ms" << std::endl;
    std::cout << "  平均插入时间: " << insert_time.count() / (double)test_count << " μs/条" << std::endl;
    std::cout << "  平均查询时间: " << lookup_time.count() / (double)test_count << " μs/条" << std::endl;
    
    // 打印哈希表统计
    std::cout << "\n哈希表统计信息:" << std::endl;
    manager.printStats();
}

void demonstrateBatchOperations() {
    std::cout << "\n=== 批量操作演示 ===" << std::endl;
    
    auto& manager = OptimizedStatusRscManager::getInstance();
    manager.clearRsc();
    
    // 先添加一些数据
    for (int i = 1; i <= 5; ++i) {
        manager.addRsc(i, "初始值_" + std::to_string(i));
    }
    
    // 批量更新
    std::map<int, std::string> update_data;
    update_data[1] = "批量更新值_1";
    update_data[3] = "批量更新值_3";
    update_data[5] = "批量更新值_5";
    update_data[7] = "不存在的键_7";  // 这个不会更新成功
    
    int updated_count = manager.batchUpdateRsc(update_data);
    std::cout << "批量更新操作，成功更新: " << updated_count << " 条记录" << std::endl;
    
    // 批量获取
    std::map<int, std::string> all_data;
    int fetched_count = manager.batchGetRsc(all_data);
    std::cout << "批量获取操作，获取到: " << fetched_count << " 条记录" << std::endl;
    
    std::cout << "所有数据:" << std::endl;
    for (const auto& pair : all_data) {
        std::cout << "  Key " << pair.first << ": " << pair.second << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== 优化版共享内存Map使用示例 ===" << std::endl;
        
        demonstrateBasicOperations();
        demonstratePerformance();
        demonstrateBatchOperations();
        
        std::cout << "\n=== 清理资源 ===" << std::endl;
        OptimizedStatusRscManager::cleanup();
        std::cout << "共享内存已清理" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
