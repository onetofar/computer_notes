// @book: Effective Modern C++
// @topic: move-vs-copy-semantics
// @source: Effective Modern C++/5. Rvalue References, Move Semantics, and Perfect Forwarding（讲义）.md §6 Item 29（别高估移动）
// @expect: 短串没有堆指针可搬(移动前后 data 指针不变), 长串移动后目的接管源的同一块堆内存且源被掏空
#include <cstdio>
#include <string>
#include <utility>

int main() {
    std::string shortStr = "hi";
    std::string longStr  = "a string that is definitely longer than the small buffer";

    std::printf("短串 capacity=%zu   长串 capacity=%zu\n", shortStr.capacity(), longStr.capacity());

    const void* shortBefore = shortStr.data();
    const void* longBefore  = longStr.data();

    std::string shortMoved = std::move(shortStr);
    std::string longMoved  = std::move(longStr);

    std::printf("短串: 移动前 data=%p 移动后 data=%p 目的 data=%p 源 size=%zu 源内容=\"%s\"\n",
                shortBefore, static_cast<void*>(shortStr.data()),
                static_cast<void*>(shortMoved.data()), shortStr.size(), shortStr.c_str());
    std::printf("长串: 移动前 data=%p 移动后 data=%p 目的 data=%p 源 size=%zu\n",
                longBefore, static_cast<void*>(longStr.data()),
                static_cast<void*>(longMoved.data()), longStr.size());
    std::printf("长串: 目的接管了源的堆指针? %d\n",
                static_cast<const void*>(longMoved.data()) == longBefore ? 1 : 0);
    std::printf("短串: 移动前后 data 指针相同? %d  (SSO 没有堆指针可搬)\n",
                shortBefore == static_cast<const void*>(shortStr.data()) ? 1 : 0);
    return 0;
}
