#include "improved_status.h"
#include "optimized_status.h"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <iomanip>

class PerformanceTimer {
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        return duration.count() / 1000.0; // 返回毫秒
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_time;
};

void testOriginalImplementation(const std::vector<int>& keys, const std::vector<std::string>& values) {
    std::cout << "\n=== Testing Original Implementation ===" << std::endl;
    
    auto& manager = StatusRscManager::getInstance();
    manager.clearRsc();
    
    PerformanceTimer timer;
    
    // 测试插入性能
    timer.start();
    for (size_t i = 0; i < keys.size(); ++i) {
        manager.addRsc(keys[i], values[i]);
    }
    double insert_time = timer.stop();
    
    // 测试查找性能
    timer.start();
    for (int key : keys) {
        manager.getRsc(key);
    }
    double lookup_time = timer.stop();
    
    // 测试更新性能
    timer.start();
    for (size_t i = 0; i < keys.size(); ++i) {
        manager.updateRsc(keys[i], "updated_" + values[i]);
    }
    double update_time = timer.stop();
    
    // 测试删除性能
    timer.start();
    for (int key : keys) {
        manager.removeRsc(key);
    }
    double delete_time = timer.stop();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Insert time: " << insert_time << " ms" << std::endl;
    std::cout << "Lookup time: " << lookup_time << " ms" << std::endl;
    std::cout << "Update time: " << update_time << " ms" << std::endl;
    std::cout << "Delete time: " << delete_time << " ms" << std::endl;
    std::cout << "Total time: " << (insert_time + lookup_time + update_time + delete_time) << " ms" << std::endl;
}

void testOptimizedImplementation(const std::vector<int>& keys, const std::vector<std::string>& values) {
    std::cout << "\n=== Testing Optimized Implementation ===" << std::endl;
    
    auto& manager = OptimizedStatusRscManager::getInstance();
    manager.clearRsc();
    
    PerformanceTimer timer;
    
    // 测试插入性能
    timer.start();
    for (size_t i = 0; i < keys.size(); ++i) {
        manager.addRsc(keys[i], values[i]);
    }
    double insert_time = timer.stop();
    
    // 测试查找性能
    timer.start();
    for (int key : keys) {
        manager.getRsc(key);
    }
    double lookup_time = timer.stop();
    
    // 测试更新性能
    timer.start();
    for (size_t i = 0; i < keys.size(); ++i) {
        manager.updateRsc(keys[i], "updated_" + values[i]);
    }
    double update_time = timer.stop();
    
    // 测试删除性能
    timer.start();
    for (int key : keys) {
        manager.removeRsc(key);
    }
    double delete_time = timer.stop();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Insert time: " << insert_time << " ms" << std::endl;
    std::cout << "Lookup time: " << lookup_time << " ms" << std::endl;
    std::cout << "Update time: " << update_time << " ms" << std::endl;
    std::cout << "Delete time: " << delete_time << " ms" << std::endl;
    std::cout << "Total time: " << (insert_time + lookup_time + update_time + delete_time) << " ms" << std::endl;
    
    // 打印哈希表统计信息
    manager.printStats();
}

void testBatchOperations(int num_items) {
    std::cout << "\n=== Testing Batch Operations (" << num_items << " items) ===" << std::endl;
    
    // 生成测试数据
    std::map<int, std::string> test_data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100000);
    
    for (int i = 0; i < num_items; ++i) {
        test_data[dis(gen)] = "batch_value_" + std::to_string(i);
    }
    
    PerformanceTimer timer;
    
    // 测试原始实现的批量操作
    auto& original_manager = StatusRscManager::getInstance();
    original_manager.clearRsc();
    
    timer.start();
    original_manager.batchUpdateRsc(test_data);
    double original_batch_time = timer.stop();
    
    // 测试优化实现的批量操作
    auto& optimized_manager = OptimizedStatusRscManager::getInstance();
    optimized_manager.clearRsc();
    
    // 先插入数据
    for (const auto& pair : test_data) {
        optimized_manager.addRsc(pair.first, pair.second);
    }
    
    timer.start();
    optimized_manager.batchUpdateRsc(test_data);
    double optimized_batch_time = timer.stop();
    
    std::cout << "Original batch update time: " << original_batch_time << " ms" << std::endl;
    std::cout << "Optimized batch update time: " << optimized_batch_time << " ms" << std::endl;
    std::cout << "Speedup: " << (original_batch_time / optimized_batch_time) << "x" << std::endl;
}

int main() {
    std::cout << "=== Shared Memory Map Performance Comparison ===" << std::endl;
    
    // 生成测试数据
    const int num_items = 500;  // 测试500个条目
    std::vector<int> keys;
    std::vector<std::string> values;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100000);
    
    for (int i = 0; i < num_items; ++i) {
        keys.push_back(dis(gen));
        values.push_back("test_value_" + std::to_string(i));
    }
    
    std::cout << "Testing with " << num_items << " items..." << std::endl;
    
    try {
        // 测试原始实现
        testOriginalImplementation(keys, values);
        
        // 测试优化实现
        testOptimizedImplementation(keys, values);
        
        // 测试批量操作
        testBatchOperations(100);
        
        std::cout << "\n=== Performance Summary ===" << std::endl;
        std::cout << "The optimized implementation uses a hash table with:" << std::endl;
        std::cout << "- O(1) average time complexity for all operations" << std::endl;
        std::cout << "- Double hashing for collision resolution" << std::endl;
        std::cout << "- Automatic rehashing when load factor is too high" << std::endl;
        std::cout << "- Better cache locality and memory efficiency" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
