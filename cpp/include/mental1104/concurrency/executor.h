// include/mental1104/concurrency/executor.h
#ifndef MENTAL1104_EXECUTOR_H
#define MENTAL1104_EXECUTOR_H

#pragma once

#include <functional>

namespace mental1104 {

// 抽象执行器接口：为将来适配 Boost / 阿里等预留
class IExecutor {
public:
  virtual ~IExecutor() = default;

  // fire-and-forget 语义，不关心返回值
  virtual void execute(std::function<void()> fn) = 0;
};

} // namespace mental1104

#endif // MENTAL1104_EXECUTOR_H
