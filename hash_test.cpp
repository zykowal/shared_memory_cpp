#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <bitset>
#include <unordered_map>

// 测试不同哈希常数的效果
class HashTester {
public:
    // 使用我们的魔数
    static uint32_t good_hash(uint32_t key) {
        uint32_t k = key;
        k ^= k >> 16;
        k *= 0x85ebca6b;  // MurmurHash魔数
        k ^= k >> 13;
        k *= 0xc2b2ae35;  // MurmurHash魔数
        k ^= k >> 16;
        return k;
    }
    
    // 使用普通常数对比
    static uint32_t bad_hash(uint32_t key) {
        uint32_t k = key;
        k ^= k >> 16;
        k *= 0x12345678;  // 随意选择的数
        k ^= k >> 13;
        k *= 0x87654321;  // 随意选择的数
        k ^= k >> 16;
        return k;
    }
    
    // 测试分布均匀性
    static double test_distribution(uint32_t (*hash_func)(uint32_t), int samples = 100000) {
        const int buckets = 1024;
        std::vector<int> counts(buckets, 0);
        
        for (int i = 0; i < samples; ++i) {
            uint32_t hash_val = hash_func(i);
            counts[hash_val % buckets]++;
        }
        
        // 计算标准差
        double expected = samples / (double)buckets;
        double variance = 0;
        for (int count : counts) {
            double diff = count - expected;
            variance += diff * diff;
        }
        variance /= buckets;
        return sqrt(variance);
    }
    
    // 测试雪崩效应
    static double test_avalanche(uint32_t (*hash_func)(uint32_t), int samples = 10000) {
        int total_flipped_bits = 0;
        
        for (int i = 0; i < samples; ++i) {
            uint32_t hash1 = hash_func(i);
            uint32_t hash2 = hash_func(i + 1);  // 输入只差1
            
            // 计算输出中不同的位数
            total_flipped_bits += __builtin_popcount(hash1 ^ hash2);
        }
        
        // 理想情况下应该是16位（32位的一半）
        return total_flipped_bits / (double)samples;
    }
    
    // 验证黄金比例常数
    static void verify_golden_ratio() {
        std::cout << "=== 黄金比例常数验证 ===" << std::endl;
        
        double phi = (1.0 + sqrt(5.0)) / 2.0;
        double inv_phi = 1.0 / phi;
        uint32_t magic = static_cast<uint32_t>(inv_phi * (1ULL << 32));
        
        std::cout << "黄金比例 φ: " << std::fixed << std::setprecision(10) << phi << std::endl;
        std::cout << "黄金比例倒数 1/φ: " << inv_phi << std::endl;
        std::cout << "32位表示: 0x" << std::hex << magic << std::dec << std::endl;
        std::cout << "我们使用的常数: 0x9e3779b9" << std::endl;
        std::cout << "是否匹配: " << (magic == 0x9e3779b9 ? "是" : "否") << std::endl;
    }
    
    // 分析魔数的二进制特性
    static void analyze_magic_numbers() {
        std::cout << "\n=== 魔数二进制分析 ===" << std::endl;
        
        uint32_t constants[] = {0x85ebca6b, 0xc2b2ae35, 0x21f0aaad, 0x735a2d97, 0x9e3779b9};
        const char* names[] = {"0x85ebca6b", "0xc2b2ae35", "0x21f0aaad", "0x735a2d97", "0x9e3779b9"};
        
        for (int i = 0; i < 5; ++i) {
            uint32_t c = constants[i];
            std::bitset<32> bits(c);
            int ones = __builtin_popcount(c);
            
            std::cout << names[i] << ":" << std::endl;
            std::cout << "  二进制: " << bits << std::endl;
            std::cout << "  1的个数: " << ones << " (理想范围: 12-20)" << std::endl;
            std::cout << "  是否为奇数: " << ((c & 1) ? "是" : "否") << std::endl;
            std::cout << "  高16位1的个数: " << __builtin_popcount(c >> 16) << std::endl;
            std::cout << "  低16位1的个数: " << __builtin_popcount(c & 0xFFFF) << std::endl;
            std::cout << std::endl;
        }
    }
};

int main() {
    std::cout << "=== 哈希函数魔数效果测试 ===" << std::endl;
    
    // 验证黄金比例
    HashTester::verify_golden_ratio();
    
    // 分析魔数特性
    HashTester::analyze_magic_numbers();
    
    // 测试分布均匀性
    std::cout << "=== 分布均匀性测试 ===" << std::endl;
    double good_std = HashTester::test_distribution(HashTester::good_hash);
    double bad_std = HashTester::test_distribution(HashTester::bad_hash);
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "使用MurmurHash魔数的标准差: " << good_std << std::endl;
    std::cout << "使用普通常数的标准差: " << bad_std << std::endl;
    std::cout << "改进倍数: " << (bad_std / good_std) << "x" << std::endl;
    
    // 测试雪崩效应
    std::cout << "\n=== 雪崩效应测试 ===" << std::endl;
    double good_avalanche = HashTester::test_avalanche(HashTester::good_hash);
    double bad_avalanche = HashTester::test_avalanche(HashTester::bad_hash);
    
    std::cout << "使用MurmurHash魔数的平均翻转位数: " << good_avalanche << " (理想值: 16)" << std::endl;
    std::cout << "使用普通常数的平均翻转位数: " << bad_avalanche << " (理想值: 16)" << std::endl;
    std::cout << "MurmurHash魔数更接近理想值: " << (abs(good_avalanche - 16) < abs(bad_avalanche - 16) ? "是" : "否") << std::endl;
    
    // 实际碰撞测试
    std::cout << "\n=== 碰撞测试 ===" << std::endl;
    std::unordered_map<uint32_t, int> good_collisions, bad_collisions;
    
    const int test_range = 100000;
    for (int i = 0; i < test_range; ++i) {
        uint32_t good_hash = HashTester::good_hash(i) % 8192;  // 映射到8K空间
        uint32_t bad_hash = HashTester::bad_hash(i) % 8192;
        
        good_collisions[good_hash]++;
        bad_collisions[bad_hash]++;
    }
    
    int good_collision_count = 0, bad_collision_count = 0;
    for (const auto& pair : good_collisions) {
        if (pair.second > 1) good_collision_count += pair.second - 1;
    }
    for (const auto& pair : bad_collisions) {
        if (pair.second > 1) bad_collision_count += pair.second - 1;
    }
    
    std::cout << "MurmurHash魔数碰撞次数: " << good_collision_count << std::endl;
    std::cout << "普通常数碰撞次数: " << bad_collision_count << std::endl;
    std::cout << "碰撞减少: " << ((bad_collision_count - good_collision_count) * 100.0 / bad_collision_count) << "%" << std::endl;
    
    return 0;
}
