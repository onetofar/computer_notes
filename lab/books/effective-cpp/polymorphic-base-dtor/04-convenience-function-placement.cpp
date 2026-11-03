// @book: Effective C++
// @topic: polymorphic-base-dtor
// @source: Item 23 用非成员非友元函数取代成员函数（讲义）.md Item 23
// @expect: 成员便利函数与非成员非友元便利函数行为完全一致; 友元便利函数多出一条绕过 public 接口的特权路径
#include <cstdio>

namespace WebBrowserStuff {

class WebBrowser {
public:
    void clearCache()    { ++cache_;    ++publicCalls_; }
    void clearHistory()  { ++history_;  ++publicCalls_; }
    void removeCookies() { ++cookies_;  ++publicCalls_; }

    int publicCalls() const  { return publicCalls_; }
    int cacheEntries() const { return cache_; }

    // 成员便利函数: 它是 WebBrowser 的一部分, 也因此是「能碰 private 的函数」之一
    void clearEverything() {
        clearCache();
        clearHistory();
        removeCookies();
    }

private:
    int cache_ = 0;
    int history_ = 0;
    int cookies_ = 0;
    int publicCalls_ = 0;

    // 友元便利函数: 它能直接改 private, 于是多出一条绕过 public 接口的特权路径
    friend void clearEverythingFast(WebBrowser& wb);
};

void clearEverythingFast(WebBrowser& wb) {
    wb.cache_ = 0;
    wb.history_ = 0;
    wb.cookies_ = 0;          // 直接动 private, 不经过上面三个 public 函数
}

// 非成员非友元便利函数: 与 WebBrowser 在同一个 namespace, 但碰不到它的 private
void clearBrowser(WebBrowser& wb) {
    wb.clearCache();
    wb.clearHistory();
    wb.removeCookies();
}

}  // namespace WebBrowserStuff

int main() {
    using namespace WebBrowserStuff;

    {
        WebBrowser wb;
        wb.clearEverything();
        std::printf("成员便利函数   clearEverything()      : publicCalls=%d cache=%d\n",
                    wb.publicCalls(), wb.cacheEntries());
    }
    {
        WebBrowser wb;
        clearBrowser(wb);
        std::printf("非成员非友元   clearBrowser(wb)       : publicCalls=%d cache=%d\n",
                    wb.publicCalls(), wb.cacheEntries());
    }
    {
        WebBrowser wb;
        clearEverythingFast(wb);
        std::printf("友元便利函数   clearEverythingFast(wb): publicCalls=%d cache=%d\n",
                    wb.publicCalls(), wb.cacheEntries());
    }

    std::printf("\n成员与非成员非友元行为完全一致 -> 后者是前者的无痛替换,\n");
    std::printf("而且它没有增加「能看见 private 的函数数量」。\n");
    std::printf("友元版本则多出一条特权路径, 封装被削弱。\n");
    return 0;
}
