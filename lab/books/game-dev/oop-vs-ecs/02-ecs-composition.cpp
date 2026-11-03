// @book: 网络多人在线游戏开发实战教程
// @topic: oop-vs-ecs
// @source: 第6章 搭建ECS框架（讲义）.md §6.1
// @expect: 同样 64 个实体, ECS 只把 8 个实现了更新接口的组件放进更新队列, 每帧 8 次虚调用, 空更新为 0
#include <cstdio>
#include <map>
#include <memory>
#include <vector>

namespace {

constexpr int kFrames = 100;
constexpr int kDynamic = 8;
constexpr int kStatic = 56;

struct IInitializeSystem {
    virtual ~IInitializeSystem() = default;
    virtual void Initialize() = 0;
};

struct IUpdateSystem {
    virtual ~IUpdateSystem() = default;
    virtual void Update() = 0;
};

struct Component {
    virtual ~Component() = default;
};

struct Entity : Component {  // 实体本身也是组件, 下面还能挂子组件
    std::map<long, Component*> components;
    void AddComponent(long sn, Component* c) { components[sn] = c; }
};

struct Account : Component, IUpdateSystem {
    void Update() override { ++updates; }
    long long updates = 0;
};

struct AccountStatic : Component {  // 不实现更新接口, 就不必写空函数
    int tag = 7;
};

struct EntitySystem {
    std::vector<IInitializeSystem*> initialize_q;
    std::vector<IUpdateSystem*> update_q;

    void Register(Component* c) {  // 靠 dynamic_cast 探测它实现了哪些动作
        if (auto* i = dynamic_cast<IInitializeSystem*>(c)) initialize_q.push_back(i);
        if (auto* u = dynamic_cast<IUpdateSystem*>(c)) update_q.push_back(u);
    }
    void Update() {
        while (!initialize_q.empty()) {  // 初始化队列清完, 才轮到更新队列
            initialize_q.front()->Initialize();
            initialize_q.erase(initialize_q.begin());
        }
        for (auto* u : update_q) u->Update();
    }
};

}  // namespace

int main() {
    EntitySystem system;
    std::vector<std::unique_ptr<Component>> owned;
    std::vector<Entity*> entities;

    for (int i = 0; i < kDynamic; ++i) {
        auto* acc = new Account();
        owned.emplace_back(acc);
        auto* ent = new Entity();
        owned.emplace_back(ent);
        ent->AddComponent(i, acc);
        entities.push_back(ent);
        system.Register(acc);
    }
    for (int i = 0; i < kStatic; ++i) {
        auto* st = new AccountStatic();
        owned.emplace_back(st);
        auto* ent = new Entity();
        owned.emplace_back(ent);
        ent->AddComponent(1000 + i, st);
        entities.push_back(ent);
        system.Register(st);
    }

    long long virtual_calls = 0;
    for (int frame = 0; frame < kFrames; ++frame) {
        virtual_calls += static_cast<long long>(system.update_q.size());
        system.Update();
    }
    long long effective = 0;
    for (auto& c : owned) {
        if (auto* a = dynamic_cast<Account*>(c.get())) effective += a->updates;
    }

    std::printf("ecs: entities=%d frames=%d virtual_calls=%lld effective_updates=%lld empty_updates=0\n",
                kDynamic + kStatic, kFrames, virtual_calls, effective);
    std::printf("update_queue_size=%zu (静态组件不实现接口, 不进队列)\n", system.update_q.size());
    return 0;
}
