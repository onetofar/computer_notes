// @book: Effective C++
// @topic: exception-safety-levels
// @source: Item 29 争取异常安全（exception-safe）的代码（讲义）.md Item 29
// @expect: 强保证: copy-and-swap, 异常后对象与调用前完全一样 (计数与图都不变)
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

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

struct PMImpl {
    std::shared_ptr<Image> bgImage;
    int imageChanges = 0;
};

// copy-and-swap: 所有副作用先发生在局部副本上, 成功后才整体换上去
class PrettyMenu {
public:
    PrettyMenu() : pImpl_(std::make_shared<PMImpl>()) {
        pImpl_->bgImage = std::make_shared<Image>(std::string("INIT"));
    }

    void changeBackground(const std::string& data) {
        auto pNew = std::make_shared<PMImpl>(*pImpl_);   // 拷贝一份数据
        pNew->bgImage.reset(new Image(data));            // 在拷贝上做改动
        ++pNew->imageChanges;
        std::swap(pImpl_, pNew);                         // 不抛异常的交换
    }

    int imageChanges() const { return pImpl_->imageChanges; }
    int currentImageId() const { return pImpl_->bgImage->id(); }

private:
    std::shared_ptr<PMImpl> pImpl_;
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
    std::printf("\n对象与调用前完全一样 -> 「如果失败了, 程序的状态就像它从没有被调用过一样」,\n");
    std::printf("这就是「强力保证」。\n");
    return 0;
}
