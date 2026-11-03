// @book: 网络多人在线游戏开发实战教程
// @topic: oop-vs-ecs
// @source: 第6章 搭建ECS框架（讲义）.md §6.1
// @expect: 继承模型下 64 个实体每帧都被虚调用一次, 其中 56 个调的是被迫写的空 Update, 有效更新只有 8 个
#include <cstdio>
#include <memory>
#include <vector>

namespace {

constexpr int kFrames = 100;
constexpr int kDynamic = 8;
constexpr int kStatic = 56;

struct ThreadObject {
    virtual ~ThreadObject() = default;
    virtual void Update() = 0;
};

struct Account : ThreadObject {  // 真需要每帧更新
    void Update() override { ++effective_updates; }
    long long effective_updates = 0;
};

struct AccountStatic : ThreadObject {  // 根本不需要更新, 只能写个空函数交差
    void Update() override { ++empty_updates; }  // 业务上它什么都没做, 这里只为了计数
    long long empty_updates = 0;
};

long long g_virtual_calls = 0;
long long effective_updates = 0;
long long empty_updates = 0;

}  // namespace

int main() {
    std::vector<std::unique_ptr<ThreadObject>> objects;
    for (int i = 0; i < kDynamic; ++i) objects.push_back(std::make_unique<Account>());
    for (int i = 0; i < kStatic; ++i) objects.push_back(std::make_unique<AccountStatic>());

    for (int frame = 0; frame < kFrames; ++frame) {
        for (auto& obj : objects) {
            ++g_virtual_calls;
            obj->Update();
        }
    }
    for (auto& obj : objects) {
        if (auto* a = dynamic_cast<Account*>(obj.get())) effective_updates += a->effective_updates;
        if (auto* s = dynamic_cast<AccountStatic*>(obj.get())) empty_updates += s->empty_updates;
    }

    std::printf("oop: entities=%d frames=%d virtual_calls=%lld effective_updates=%lld empty_updates=%lld\n",
                kDynamic + kStatic, kFrames, g_virtual_calls, effective_updates, empty_updates);
    std::printf("empty_ratio=%.1f%%\n",
                100.0 * static_cast<double>(empty_updates) / static_cast<double>(g_virtual_calls));
    return 0;
}
