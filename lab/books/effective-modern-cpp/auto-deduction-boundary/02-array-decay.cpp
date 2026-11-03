// @book: Effective Modern C++
// @topic: auto-deduction-boundary
// @source: Effective Modern C++/1. Deducing Types（讲义）.md §Item 1 · 1.5 Array Arguments
// @expect: 数组按值接退化成指针(sizeof=8), 按引用接保留数组类型(sizeof=13); arraySize 把长度变成类型的一部分
#include <array>
#include <cstddef>
#include <cstdio>
#include <type_traits>

template <typename T, std::size_t N>
constexpr std::size_t arraySize(T (&)[N]) noexcept { return N; }

template <typename T>
void byValue(T param) {
    std::printf("  按值接  : sizeof=%zu  T 仍是数组? %d\n", sizeof(param),
                static_cast<int>(std::is_array_v<T>));
}

template <typename T>
void byRef(T& param) {
    std::printf("  按引用接: sizeof=%zu  T 仍是数组? %d\n", sizeof(param),
                static_cast<int>(std::is_array_v<T>));
}

int main() {
    const char name[] = "J. P. Briggs";     // 类型是 const char[13]

    auto  p = name;                        // 退化成 const char*
    auto& r = name;                        // 保留 const char(&)[13]

    static_assert(std::is_same_v<decltype(p), const char*>);
    static_assert(std::is_same_v<decltype(r), const char (&)[13]>);

    std::printf("auto  接数组: sizeof=%zu\n", sizeof(p));
    std::printf("auto& 接数组: sizeof=%zu\n", sizeof(r));
    byValue(name);
    byRef(name);

    int keyVals[] = {1, 3, 7, 9, 11, 22, 35};
    std::array<int, arraySize(keyVals)> mapped{};
    std::printf("arraySize(keyVals)=%zu  mapped.size()=%zu\n",
                arraySize(keyVals), mapped.size());
    return 0;
}
