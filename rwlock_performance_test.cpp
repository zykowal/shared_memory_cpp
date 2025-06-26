#include "optimized_status.h"
#include "rwlock_optimized_status.h"
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <iomanip>
#include <thread>
#include <atomic>

class RWLockPerformanceTimer {
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

// 测试并发读性能
void testConcurrentReads() {
    std::cout << "\n=== 并发读性能测试 ===" << std::endl;
    
    const int num_keys = 1000;
    const int num_readers = 4;  // 4个并发读线程
    const int reads_per_thread = 10000;
    
    // 准备测试数据
    std::vector<int> keys;
    for (int i = 0; i < num_keys; ++i) {
        keys.push_back(i + 1);
    }
    
    double mutex_time, rwlock_time;
    
    // 测试互斥锁版本
    {
        auto& mutex_manager = OptimizedStatusRscManager::getInstance();
        mutex_manager.clearRsc();
        
        // 插入测试数据
        for (int key : keys) {
            mutex_manager.addRsc(key, "test_value_" + std::to_string(key));
        }
        
        std::atomic<int> completed_reads{0};
        RWLockPerformanceTimer timer;
        timer.start();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_readers; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd() + t);
                std::uniform_int_distribution<> dis(0, num_keys - 1);
                
                for (int i = 0; i < reads_per_thread; ++i) {
                    int key = keys[dis(gen)];
                    mutex_manager.getRsc(key);
                    completed_reads++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        mutex_time = timer.stop();
        std::cout << "互斥锁版本 - " << num_readers << "个线程并发读:" << std::endl;
        std::cout << "  总时间: " << std::fixed << std::setprecision(2) << mutex_time << " ms" << std::endl;
        std::cout << "  总读取次数: " << completed_reads.load() << std::endl;
        std::cout << "  平均读取速度: " << (completed_reads.load() / mutex_time * 1000) << " 次/秒" << std::endl;
    }
    
    // 测试读写锁版本
    {
        auto& rwlock_manager = RWLockOptimizedStatusRscManager::getInstance();
        rwlock_manager.clearRsc();
        
        // 插入测试数据
        for (int key : keys) {
            rwlock_manager.addRsc(key, "test_value_" + std::to_string(key));
        }
        
        std::atomic<int> completed_reads{0};
        RWLockPerformanceTimer timer;
        timer.start();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_readers; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd() + t);
                std::uniform_int_distribution<> dis(0, num_keys - 1);
                
                for (int i = 0; i < reads_per_thread; ++i) {
                    int key = keys[dis(gen)];
                    rwlock_manager.getRsc(key);
                    completed_reads++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        rwlock_time = timer.stop();
        std::cout << "读写锁版本 - " << num_readers << "个线程并发读:" << std::endl;
        std::cout << "  总时间: " << std::fixed << std::setprecision(2) << rwlock_time << " ms" << std::endl;
        std::cout << "  总读取次数: " << completed_reads.load() << std::endl;
        std::cout << "  平均读取速度: " << (completed_reads.load() / rwlock_time * 1000) << " 次/秒" << std::endl;
        
        std::cout << "读写锁性能提升: " << std::fixed << std::setprecision(1) 
                  << (mutex_time / rwlock_time) << "x" << std::endl;
    }
}

// 测试读写混合场景
void testMixedReadWrite() {
    std::cout << "\n=== 读写混合性能测试 ===" << std::endl;
    
    const int num_keys = 500;
    const int num_threads = 4;
    const int operations_per_thread = 5000;
    const double read_ratio = 0.8;  // 80%读操作，20%写操作
    
    // 准备测试数据
    std::vector<int> keys;
    for (int i = 0; i < num_keys; ++i) {
        keys.push_back(i + 1);
    }
    
    double mutex_time, rwlock_time;
    
    // 测试互斥锁版本
    {
        auto& mutex_manager = OptimizedStatusRscManager::getInstance();
        mutex_manager.clearRsc();
        
        // 插入初始数据
        for (int key : keys) {
            mutex_manager.addRsc(key, "initial_value_" + std::to_string(key));
        }
        
        std::atomic<int> reads{0}, writes{0};
        RWLockPerformanceTimer timer;
        timer.start();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd() + t);
                std::uniform_int_distribution<> key_dis(0, num_keys - 1);
                std::uniform_real_distribution<> op_dis(0.0, 1.0);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    int key = keys[key_dis(gen)];
                    
                    if (op_dis(gen) < read_ratio) {
                        // 读操作
                        mutex_manager.getRsc(key);
                        reads++;
                    } else {
                        // 写操作
                        mutex_manager.updateRsc(key, "updated_value_" + std::to_string(key));
                        writes++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        mutex_time = timer.stop();
        std::cout << "互斥锁版本 - 混合读写 (80%读/20%写):" << std::endl;
        std::cout << "  总时间: " << std::fixed << std::setprecision(2) << mutex_time << " ms" << std::endl;
        std::cout << "  读操作: " << reads.load() << " 次" << std::endl;
        std::cout << "  写操作: " << writes.load() << " 次" << std::endl;
        std::cout << "  总操作速度: " << ((reads.load() + writes.load()) / mutex_time * 1000) << " 次/秒" << std::endl;
    }
    
    // 测试读写锁版本
    {
        auto& rwlock_manager = RWLockOptimizedStatusRscManager::getInstance();
        rwlock_manager.clearRsc();
        
        // 插入初始数据
        for (int key : keys) {
            rwlock_manager.addRsc(key, "initial_value_" + std::to_string(key));
        }
        
        std::atomic<int> reads{0}, writes{0};
        RWLockPerformanceTimer timer;
        timer.start();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::random_device rd;
                std::mt19937 gen(rd() + t);
                std::uniform_int_distribution<> key_dis(0, num_keys - 1);
                std::uniform_real_distribution<> op_dis(0.0, 1.0);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    int key = keys[key_dis(gen)];
                    
                    if (op_dis(gen) < read_ratio) {
                        // 读操作
                        rwlock_manager.getRsc(key);
                        reads++;
                    } else {
                        // 写操作
                        rwlock_manager.updateRsc(key, "updated_value_" + std::to_string(key));
                        writes++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        rwlock_time = timer.stop();
        std::cout << "读写锁版本 - 混合读写 (80%读/20%写):" << std::endl;
        std::cout << "  总时间: " << std::fixed << std::setprecision(2) << rwlock_time << " ms" << std::endl;
        std::cout << "  读操作: " << reads.load() << " 次" << std::endl;
        std::cout << "  写操作: " << writes.load() << " 次" << std::endl;
        std::cout << "  总操作速度: " << ((reads.load() + writes.load()) / rwlock_time * 1000) << " 次/秒" << std::endl;
        
        std::cout << "读写锁性能提升: " << std::fixed << std::setprecision(1) 
                  << (mutex_time / rwlock_time) << "x" << std::endl;
    }
}

// 测试单线程性能（基准测试）
void testSingleThreadPerformance() {
    std::cout << "\n=== 单线程性能对比 ===" << std::endl;
    
    const int num_operations = 10000;
    std::vector<int> keys;
    for (int i = 0; i < num_operations; ++i) {
        keys.push_back(i + 1);
    }
    
    double mutex_time, rwlock_time;
    
    // 测试互斥锁版本
    {
        auto& mutex_manager = OptimizedStatusRscManager::getInstance();
        mutex_manager.clearRsc();
        
        RWLockPerformanceTimer timer;
        timer.start();
        
        // 插入
        for (int key : keys) {
            mutex_manager.addRsc(key, "value_" + std::to_string(key));
        }
        
        // 读取
        for (int key : keys) {
            mutex_manager.getRsc(key);
        }
        
        mutex_time = timer.stop();
        std::cout << "互斥锁版本单线程时间: " << std::fixed << std::setprecision(2) << mutex_time << " ms" << std::endl;
    }
    
    // 测试读写锁版本
    {
        auto& rwlock_manager = RWLockOptimizedStatusRscManager::getInstance();
        rwlock_manager.clearRsc();
        
        RWLockPerformanceTimer timer;
        timer.start();
        
        // 插入
        for (int key : keys) {
            rwlock_manager.addRsc(key, "value_" + std::to_string(key));
        }
        
        // 读取
        for (int key : keys) {
            rwlock_manager.getRsc(key);
        }
        
        rwlock_time = timer.stop();
        std::cout << "读写锁版本单线程时间: " << std::fixed << std::setprecision(2) << rwlock_time << " ms" << std::endl;
        std::cout << "单线程开销比较: " << std::fixed << std::setprecision(2) << (rwlock_time / mutex_time) << "x" << std::endl;
    }
}

int main() {
    std::cout << "=== 读写锁 vs 互斥锁性能对比测试 ===" << std::endl;
    
    try {
        // 单线程基准测试
        testSingleThreadPerformance();
        
        // 并发读测试
        testConcurrentReads();
        
        // 读写混合测试
        testMixedReadWrite();
        
        std::cout << "\n=== 总结 ===" << std::endl;
        std::cout << "读写锁的优势:" << std::endl;
        std::cout << "1. 并发读性能显著提升" << std::endl;
        std::cout << "2. 读多写少场景下整体性能更好" << std::endl;
        std::cout << "3. 适合查询频繁的共享内存应用" << std::endl;
        std::cout << "\n注意事项:" << std::endl;
        std::cout << "1. 单线程下可能有轻微开销" << std::endl;
        std::cout << "2. 写操作仍然是独占的" << std::endl;
        std::cout << "3. 读写锁本身比互斥锁复杂" << std::endl;
        
        // 清理资源
        OptimizedStatusRscManager::cleanup();
        RWLockOptimizedStatusRscManager::cleanup();
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
