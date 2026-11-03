// @book: Effective C++
// @topic: exception-safety-levels
// @source: Item 29 争取异常安全（exception-safe）的代码（讲义）.md Item 29
// @expect: 基本保证: 资源由对象管理, 异常后没有泄漏也没有悬垂; 但计数已经加过, 状态不可预期
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

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

    ~Image() { std::printf("    [Image] dtor\n"); }

    int id() const { return id_; }

private:
    int id_ = 0;
};

// 用对象管理资源 (Item 13) -> 不会有泄漏, 也不会有悬垂指针
class PrettyMenu {
public:
    PrettyMenu() : bgImage_(std::make_shared<Image>(std::string("INIT"))) {}

    void changeBackground(const std::string& data) {
        ++imageChanges_;                          // 先更新计数
        bgImage_.reset(new Image(data));          // 后换图, 这里可能抛
    }

    int imageChanges() const { return imageChanges_; }
    int currentImageId() const { return bgImage_->id(); }

private:
    std::shared_ptr<Image> bgImage_;
    int imageChanges_ = 0;
};

}  // namespace

int main() {
    PrettyMenu menu;
    std::printf("初始: imageChanges=%d, 当前图 id=%d\n",
                menu.imageChanges(), menu.currentImageId());

    std::printf("\nchangeBackground 传入坏数据 -> Image 构造抛异常\n");
    try {
        menu.changeBackground(std::string(""));
    } catch (const std::exception& e) {
        std::printf("  捕获异常: %s\n", e.what());
    }

    std::printf("  异常之后: imageChanges=%d, 当前图 id=%d\n",
                menu.imageChanges(), menu.currentImageId());
    std::printf("\n没有泄漏, 没有悬垂, 但计数说「换过了」而图还是旧的 ->\n");
    std::printf("对象仍然合法, 程序的精确状态却不可预期 -> 这是「基本保证」。\n");
    return 0;
}
