// @book: Systems Performance
// @topic: cpu-affinity-and-cache
// @source: Chapter 6. CPUs（讲义）.md §6.4.1
// @expect: 共享辅助代码: 挑两个不同物理核 / 绑核 / 计时
#ifndef SP_PICK_CORES_H
#define SP_PICK_CORES_H

#include <pthread.h>
#include <sched.h>

#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

inline double now_seconds()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

// 挑两个**不同物理核**: 同核(超线程兄弟)共享 L1, 看不到假共享的代价
inline std::vector<int> pick_two_cores()
{
    std::vector<int> cpus;
    std::vector<std::string> seen;
    for (int cpu = 0; cpu < 64; cpu++) {
        std::ifstream in("/sys/devices/system/cpu/cpu" + std::to_string(cpu) +
                         "/topology/core_id");
        std::string core;
        if (!in.good() || !(in >> core))
            break;
        bool dup = false;
        for (const auto& c : seen)
            if (c == core) dup = true;
        if (dup)
            continue;
        seen.push_back(core);
        cpus.push_back(cpu);
        if (cpus.size() == 2)
            break;
    }
    if (cpus.size() < 2) {
        cpus.clear();
        cpus.push_back(0);
        cpus.push_back(1);
    }
    return cpus;
}

inline void pin(pthread_attr_t* attr, int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_attr_setaffinity_np(attr, sizeof set, &set);
}

#endif  // SP_PICK_CORES_H
