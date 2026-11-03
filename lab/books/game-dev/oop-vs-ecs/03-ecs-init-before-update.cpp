// @book: 网络多人在线游戏开发实战教程
// @topic: oop-vs-ecs
// @source: 第6章 搭建ECS框架（讲义）.md §6.1
// @expect: 先初始化后更新时 A 的 Update 读到 B 的已初始化值 42; 顺序颠倒时读到未初始化的 0
#include <cstdio>
#include <vector>

namespace {

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

struct Health : Component, IInitializeSystem {
    void Initialize() override { value = 42; }
    int value = 0;
};

struct Shield : Component, IUpdateSystem {
    Health* health = nullptr;
    int observed = -1;
    void Update() override { observed = health ? health->value : -1; }
};

struct EntitySystem {
    std::vector<IInitializeSystem*> initialize_q;
    std::vector<IUpdateSystem*> update_q;
    void Register(Component* c) {
        if (auto* i = dynamic_cast<IInitializeSystem*>(c)) initialize_q.push_back(i);
        if (auto* u = dynamic_cast<IUpdateSystem*>(c)) update_q.push_back(u);
    }
    void DrainInitialize() {
        while (!initialize_q.empty()) {
            initialize_q.front()->Initialize();
            initialize_q.erase(initialize_q.begin());
        }
    }
    void DrainUpdate() {
        for (auto* u : update_q) u->Update();
    }
};

}  // namespace

int main() {
    Health health;
    Shield shield;
    shield.health = &health;

    EntitySystem right;
    right.Register(&health);
    right.Register(&shield);
    right.DrainInitialize();
    right.DrainUpdate();
    std::printf("先初始化后更新: Shield 读到 Health = %d\n", shield.observed);

    Health health2;
    Shield shield2;
    shield2.health = &health2;
    EntitySystem wrong;
    wrong.Register(&health2);
    wrong.Register(&shield2);
    wrong.DrainUpdate();       // 更新先跑
    wrong.DrainInitialize();   // 初始化后跑, 已经晚了
    std::printf("先更新后初始化: Shield 读到 Health = %d\n", shield2.observed);

    std::printf("依赖检查放到初始化做一次, 比在更新里查一万次强\n");
    return 0;
}
