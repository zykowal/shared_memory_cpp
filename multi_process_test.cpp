#include "improved_status.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <thread>

void child_process_writer(int process_id) {
    try {
        auto& manager = StatusRscManager::getInstance();
        
        // 每个子进程写入不同的数据
        for (int i = 0; i < 5; i++) {
            int key = process_id * 100 + i;
            std::string value = "process_" + std::to_string(process_id) + "_value_" + std::to_string(i);
            
            int ret = manager.addRsc(key, value);
            if (ret == OK) {
                std::cout << "进程 " << process_id << " 成功添加: key=" << key << ", value=" << value << std::endl;
            } else {
                std::cout << "进程 " << process_id << " 添加失败: key=" << key << ", ret=" << ret << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "进程 " << process_id << " 完成写入，当前总数: " << manager.rscNum() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "进程 " << process_id << " 错误: " << e.what() << std::endl;
    }
}

void child_process_reader(int process_id) {
    try {
        // 等待一段时间让写入进程先执行
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto& manager = StatusRscManager::getInstance();
        
        std::cout << "读取进程 " << process_id << " 开始读取，当前总数: " << manager.rscNum() << std::endl;
        
        // 尝试读取其他进程写入的数据
        for (int writer_id = 1; writer_id <= 2; writer_id++) {
            for (int i = 0; i < 5; i++) {
                int key = writer_id * 100 + i;
                std::string value = manager.getRsc(key);
                if (!value.empty()) {
                    std::cout << "读取进程 " << process_id << " 读取到: key=" << key << ", value=" << value << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "读取进程 " << process_id << " 错误: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== 多进程共享内存测试 ===" << std::endl;
    
    // 创建两个写入进程
    pid_t writer1 = fork();
    if (writer1 == 0) {
        child_process_writer(1);
        exit(0);
    }
    
    pid_t writer2 = fork();
    if (writer2 == 0) {
        child_process_writer(2);
        exit(0);
    }
    
    // 创建一个读取进程
    pid_t reader = fork();
    if (reader == 0) {
        child_process_reader(3);
        exit(0);
    }
    
    // 父进程等待所有子进程完成
    int status;
    waitpid(writer1, &status, 0);
    waitpid(writer2, &status, 0);
    waitpid(reader, &status, 0);
    
    // 父进程最后读取所有数据
    try {
        auto& manager = StatusRscManager::getInstance();
        std::cout << "\n=== 父进程最终验证 ===" << std::endl;
        std::cout << "最终总数: " << manager.rscNum() << std::endl;
        
        std::cout << "所有数据:" << std::endl;
        for (int writer_id = 1; writer_id <= 2; writer_id++) {
            for (int i = 0; i < 5; i++) {
                int key = writer_id * 100 + i;
                std::string value = manager.getRsc(key);
                if (!value.empty()) {
                    std::cout << "  key=" << key << ", value=" << value << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "父进程错误: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== 多进程测试完成 ===" << std::endl;
    
    // 最后清理共享内存
    StatusRscManager::cleanup();
    
    return 0;
}
