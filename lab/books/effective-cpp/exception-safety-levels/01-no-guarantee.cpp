// @book: Effective C++
// @topic: exception-safety-levels
// @source: Item 29 争取异常安全（exception-safe）的代码（讲义）.md Item 29
// @expect: 无保证: 抛出后计数已加, 但旧图已删新图没装上 -> 对象状态被破坏 (指针悬垂)
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class Image;
std::vector<const Image*> g_dead_images;

}  // namespace

namespace {

class Image {
public:
    explicit Image(const std::string& data) {
        if (data.size() < 4) {
            throw std::runtime_error("Image: 图像数据不足, 构造失败");
        }
        id_ = data[0];
        std::printf("    [Image] ctor (id=%d)\n", id_);
    }

    ~Image() {
        std::printf("    [Image] dtor\n");
        g_dead_images.push_back(this);
    }

    int id() const { return id_; }

private:
    int id_ = 0;
};

bool isAlive(const Image* p) {
    return std::find(g_dead_images.begin(), g_dead_images.end(), p) == g_dead_images.end();
}

// 教材原版: 裸指针 + 手动 delete
class PrettyMenu {
public:
    PrettyMenu() : bgImage_(new Image(std::string("INIT"))) {}
    ~PrettyMenu() { if (isAlive(bgImage_)) delete bgImage_; }

    void changeBackground(const std::string& data) {
        delete bgImage_;                 // 先删掉旧背景
        ++imageChanges_;                 // 再更新计数
        bgImage_ = new Image(data);      // 最后装新背景 <- 可能抛异常
    }

    int imageChanges() const { return imageChanges_; }
    bool imageAlive() const { return isAlive(bgImage_); }

private:
    Image* bgImage_ = nullptr;
    int imageChanges_ = 0;
};

}  // namespace

int main() {
    PrettyMenu menu;
    std::printf("初始: imageChanges=%d, 旧图存活=%d\n", menu.imageChanges(), (int)menu.imageAlive());

    std::printf("\nchangeBackground 传入坏数据 -> Image 构造抛异常\n");
    try {
        menu.changeBackground(std::string(""));
    } catch (const std::exception& e) {
        std::printf("  捕获异常: %s\n", e.what());
    }

    std::printf("  异常之后: imageChanges=%d, 图还活着=%d\n",
                menu.imageChanges(), (int)menu.imageAlive());
    std::printf("\n计数说「换过了」, 但图没了 -> 对象状态被破坏, 这是「无保证」。\n");
    return 0;
}
