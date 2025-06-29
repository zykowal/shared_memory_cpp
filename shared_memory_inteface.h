#pragma once

#include <map>
#include <string>

class ISharedMemoryManager {
public:
  virtual ~ISharedMemoryManager() = default;

  // 基本操作
  virtual int addRsc(int key, const std::string &value) = 0;
  virtual std::string getRsc(int key) = 0; // 调用者负责释放内存
  virtual int updateRsc(int key, const std::string &value) = 0;
  virtual int upsertRsc(int key, const std::string &value) = 0;
  virtual int removeRsc(int key) = 0;
  virtual int isContain(int key) = 0;

  // 管理操作
  virtual int rscNum() = 0;
  virtual int clearRsc() = 0;
  virtual double getLoadFactor() = 0;
  virtual void printStats() = 0;

  virtual int batchUpdateRsc(const std::map<int, std::string> &updated_map) = 0;
  virtual int batchGetRsc(std::map<int, std::string> &fetched_map) = 0;
};
