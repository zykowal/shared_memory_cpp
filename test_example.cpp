#include "improved_status.h"
#include <iostream>
#include <map>
#include <thread>
#include <chrono>

void test_basic_operations() {
    std::cout << "=== 基本操作测试 ===" << std::endl;
    
    auto& manager = StatusRscManager::getInstance();
    
    // 添加数据
    int ret = manager.addRsc(1, "value1");
    std::cout << "Add key=1: " << (ret == OK ? "成功" : "失败") << std::endl;
    
    ret = manager.addRsc(2, "value2");
    std::cout << "Add key=2: " << (ret == OK ? "成功" : "失败") << std::endl;
    
    // 重复添加应该失败
    ret = manager.addRsc(1, "duplicate");
    std::cout << "Add duplicate key=1: " << (ret == DUPLICATE_KEY ? "正确拒绝" : "错误") << std::endl;
    
    // 查询数据
    std::string value = manager.getRsc(1);
    std::cout << "Get key=1: " << value << std::endl;
    
    // 更新数据
    ret = manager.updateRsc(1, "updated_value1");
    std::cout << "Update key=1: " << (ret == OK ? "成功" : "失败") << std::endl;
    
    value = manager.getRsc(1);
    std::cout << "Get updated key=1: " << value << std::endl;
    
    // 使用upsert
    ret = manager.upsertRsc(3, "value3");
    std::cout << "Upsert new key=3: " << (ret == OK ? "成功" : "失败") << std::endl;
    
    ret = manager.upsertRsc(3, "updated_value3");
    std::cout << "Upsert existing key=3: " << (ret == OK ? "成功" : "失败") << std::endl;
    
    // 检查数量
    std::cout << "Total entries: " << manager.rscNum() << std::endl;
}

void test_batch_operations() {
    std::cout << "\n=== 批量操作测试 ===" << std::endl;
    
    auto& manager = StatusRscManager::getInstance();
    
    // 批量查询
    std::map<int, std::string> query_map = {{1, ""}, {2, ""}, {3, ""}, {999, ""}};
    manager.batchGetRsc(query_map);
    
    std::cout << "批量查询结果:" << std::endl;
    for (const auto& pair : query_map) {
        std::cout << "  key=" << pair.first << ", value=" << pair.second << std::endl;
    }
    
    // 批量更新
    std::map<int, std::string> update_map = {{1, "batch_updated1"}, {2, "batch_updated2"}};
    int ret = manager.batchUpdateRsc(update_map);
    std::cout << "批量更新: " << (ret == OK ? "成功" : "失败") << std::endl;
    
    // 验证更新结果
    std::cout << "验证批量更新:" << std::endl;
    std::cout << "  key=1: " << manager.getRsc(1) << std::endl;
    std::cout << "  key=2: " << manager.getRsc(2) << std::endl;
}

int main() {
    try {
        test_basic_operations();
        test_batch_operations();
        
        std::cout << "\n=== 测试完成 ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
