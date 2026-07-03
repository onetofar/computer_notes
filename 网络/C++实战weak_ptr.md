---
title: C++标准库笔记（第二部分）
category: C++底层
tags:
  - cpp
difficulty: 进阶
source: 自整理
link:
  - "[[A cpp第一部分_cpp基础笔记|C++基础]]"
---
### 先上完整可运行代码（整合 StrBlob + StrBlobPtr）
先把两个类拼在一起，后面逐部分拆解开讲，你可以直接编译运行看效果。
```cpp
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <initializer_list>

// 前置声明：告诉编译器 StrBlobPtr 是一个类，后面会定义
class StrBlobPtr;

class StrBlob {
    // 声明友元：StrBlobPtr 可以访问 StrBlob 的私有成员 data
    friend class StrBlobPtr;
public:
    typedef std::vector<std::string>::size_type size_type;

    StrBlob();
    StrBlob(std::initializer_list<std::string> il);

    size_type size() const { return data->size(); }
    bool empty() const { return data->empty(); }
    void push_back(const std::string &t) { data->push_back(t); }
    void pop_back();

    std::string& front();
    std::string& back();
    const std::string& front() const;
    const std::string& back() const;

    // 新增：返回指向首元素、尾后元素的“指针”
    StrBlobPtr begin();
    StrBlobPtr end();

private:
    std::shared_ptr<std::vector<std::string>> data;
    void check(size_type i, const std::string &msg) const;
};

// ===================== 伴随指针类：StrBlobPtr =====================
// 作用：像指针一样遍历 StrBlob 里的元素，同时保证安全
class StrBlobPtr {
public:
    StrBlobPtr(): curr(0) {}
    // 绑定到某个 StrBlob 的第 sz 个元素
    StrBlobPtr(StrBlob &a, size_t sz = 0);

    // 解引用：获取当前位置的元素
    std::string& deref() const;
    // 前置递增：指针往后走一步
    StrBlobPtr& incr();

private:
    // 核心检查：1. vector还活着吗？ 2. 下标合法吗？
    std::shared_ptr<std::vector<std::string>> 
        check(size_t i, const std::string &msg) const;

    // 弱引用：只观察 vector，不延长它的生命周期
    std::weak_ptr<std::vector<std::string>> wptr;
    // 当前指向的元素下标
    size_t curr;
};

// ===================== StrBlob 成员实现 =====================
StrBlob::StrBlob() 
    : data(std::make_shared<std::vector<std::string>>()) {}

StrBlob::StrBlob(std::initializer_list<std::string> il)
    : data(std::make_shared<std::vector<std::string>>(il)) {}

void StrBlob::check(size_type i, const std::string &msg) const {
    if (i >= data->size())
        throw std::out_of_range(msg);
}

void StrBlob::pop_back() {
    check(0, "pop_back on empty StrBlob");
    data->pop_back();
}

std::string& StrBlob::front() {
    check(0, "front on empty StrBlob");
    return data->front();
}

std::string& StrBlob::back() {
    check(0, "back on empty StrBlob");
    return data->back();
}

const std::string& StrBlob::front() const {
    check(0, "front on empty StrBlob");
    return data->front();
}

const std::string& StrBlob::back() const {
    check(0, "back on empty StrBlob");
    return data->back();
}

// 返回指向第一个元素的指针
StrBlobPtr StrBlob::begin() {
    return StrBlobPtr(*this);
}

// 返回指向“尾后位置”的指针（和 end() 迭代器语义一致）
StrBlobPtr StrBlob::end() {
    return StrBlobPtr(*this, data->size());
}

// ===================== StrBlobPtr 成员实现 =====================

// 构造函数：用 StrBlob 的 data 初始化弱引用
StrBlobPtr::StrBlobPtr(StrBlob &a, size_t sz)
    : wptr(a.data), curr(sz) {}

// 最核心的安全检查函数
std::shared_ptr<std::vector<std::string>> 
StrBlobPtr::check(size_t i, const std::string &msg) const {
    // lock()：尝试把弱引用升级成强引用 shared_ptr
    // 升级成功：vector 还活着，返回指向它的 shared_ptr
    // 升级失败：vector 已经被销毁了，返回空指针
    auto ret = wptr.lock();
    
    if (!ret)
        throw std::runtime_error("unbound StrBlobPtr"); // vector 没了，抛异常
    if (i >= ret->size())
        throw std::out_of_range(msg); // 下标越界，抛异常
    
    return ret; // 安全，返回强引用指针
}

// 解引用：拿到当前位置的元素，相当于 *p
std::string& StrBlobPtr::deref() const {
    auto p = check(curr, "dereference past end");
    return (*p)[curr]; // (*p) 就是 vector 对象，[curr] 取下标
}

// 前置递增：指针往后走一步，相当于 ++p
StrBlobPtr& StrBlobPtr::incr() {
    check(curr, "increment past end of StrBlobPtr");
    ++curr;
    return *this;
}

// ===================== 测试主函数 =====================
int main() {
    // 1. 基础遍历测试
    StrBlob b1 = {"apple", "banana", "cherry"};
    auto it = b1.begin();
    
    std::cout << "第一个元素: " << it.deref() << "\n";
    it.incr();
    std::cout << "第二个元素: " << it.deref() << "\n";
    it.incr();
    std::cout << "第三个元素: " << it.deref() << "\n";

    // 2. 安全测试：StrBlob 销毁后，指针再访问会抛异常
    try {
        StrBlobPtr dangling;
        {
            StrBlob temp = {"test"};
            dangling = temp.begin();
            std::cout << "\n临时对象内访问: " << dangling.deref() << "\n";
        } // temp 离开作用域，vector 被销毁
        dangling.deref(); // 此时再访问，触发 runtime_error
    } catch (const std::runtime_error &e) {
        std::cout << "捕获异常: " << e.what() << "（vector 已销毁，访问被安全拦截）\n";
    }

    return 0;
}
```

---

### 一、先搞懂：这个类到底是干嘛的？
教材里叫「伴随指针类」，你可以直接理解成：
> **给 StrBlob 专门做的一个“安全迭代器”，模拟原生指针的行为，但不会出现野指针。**

之前的 `StrBlob` 把 `vector` 封装起来了，用户不能直接拿到 `vector` 的迭代器，也不能用 `[]` 随便访问元素。那我想逐个遍历里面的字符串怎么办？
—— 就做一个 `StrBlobPtr` 类，它像指针一样：
- `deref()`  对应 `*p` ：解引用，拿当前元素
- `incr()`   对应 `++p` ：指针往后挪一位
- `begin()`/`end()` 对应迭代器的首尾

但它比原生指针强的地方：**永远不会访问到已经被销毁的内存**。
原生指针如果指向的对象释放了，再访问就是「野指针」，直接崩溃；`StrBlobPtr` 会先检查对象还在不在，不在就直接抛异常，安全拦截。

---

### 二、核心设计：为什么用 `weak_ptr` 而不是 `shared_ptr`？
这是整个设计最精髓的地方，也是教材讲得最绕的地方。

#### 回忆两个指针的区别
- **`shared_ptr`（强引用）**：持有它就会让对象的引用计数+1，只要有一个 `shared_ptr` 活着，对象就永远不会被释放。
- **`weak_ptr`（弱引用）**：只“观察”对象，不增加引用计数，不会影响对象的生命周期。对象释放了，它能感知到。

#### 为什么不用 shared_ptr？
如果 `StrBlobPtr` 里存 `shared_ptr<vector>`，会出现一个问题：
> 所有 `StrBlob` 对象都销毁了，只要还有一个 `StrBlobPtr` 活着，底层的 `vector` 就永远不会被释放。

这不符合“指针”的语义——指针只是指向一个东西，不应该决定东西的生死。
就像你拿钥匙开别人家房门，你不能因为你有钥匙，房子就永远不能拆。

#### 用 weak_ptr 正好
`weak_ptr` 就像一个「访客门禁」：
1.  它不占房子产权（不增加引用计数），房子该拆就拆，不受它影响。
2.  你每次想进门（访问元素），都要先刷门禁 `lock()`：
    - 房子还在 → 门禁通过，给你一个临时的 `shared_ptr`（临时使用权），你可以进去操作。
    - 房子拆了 → 门禁失败，返回空指针，直接告诉你“进不去了”，不会让你闯空门（野指针崩溃）。

这就是「弱引用」的核心价值：**只观察，不续命，能检测对象是否存活**。

---

### 三、逐部分拆解代码
#### 1. 成员变量
```cpp
std::weak_ptr<std::vector<std::string>> wptr; // 弱引用，指向底层vector
size_t curr;                                   // 当前指向第几个元素
```
非常简洁：一个管“对象还活没活着”，一个管“指到哪了”。

#### 2. 构造函数
```cpp
// 默认构造：空指针，curr=0
StrBlobPtr(): curr(0) {}

// 绑定到某个StrBlob，默认指向第0个元素
StrBlobPtr::StrBlobPtr(StrBlob &a, size_t sz = 0)
    : wptr(a.data), curr(sz) {}
```
- 直接把 `StrBlob` 的 `data`（shared_ptr）赋值给 `wptr`
- `shared_ptr` 可以直接赋值给 `weak_ptr`，自动从强引用转成弱引用，引用计数不会增加

#### 3. 最核心：`check` 函数
这是整个类的安全基石，做**两道检查**：
```cpp
auto ret = wptr.lock();  // 第1道：vector 还活着吗？
if (!ret)
    throw runtime_error("unbound StrBlobPtr"); // 死了就抛异常

if (i >= ret->size())    // 第2道：下标越界了吗？
    throw out_of_range(msg); // 越界也抛异常

return ret; // 都安全，返回强引用shared_ptr
```
- `wptr.lock()` 是 `weak_ptr` 的核心方法：尝试提升为 `shared_ptr`
  - 成功：对象还在，返回 `shared_ptr`，此时引用计数临时+1，访问过程中对象不会被释放
  - 失败：对象已经销毁，返回空指针
- 两道检查都过了，才敢返回指针让你用，从根源上杜绝野指针和越界。

#### 4. `deref()` 解引用
```cpp
std::string& StrBlobPtr::deref() const {
    auto p = check(curr, "dereference past end");
    return (*p)[curr];
}
```
- 先调用 `check` 保证安全
- `*p` 拿到 `vector` 对象本身，`[curr]` 取下标对应的字符串
- 对应原生指针的 `*p` 操作

#### 5. `incr()` 前置递增
```cpp
StrBlobPtr& StrBlobPtr::incr() {
    check(curr, "increment past end of StrBlobPtr");
    ++curr;
    return *this;
}
```
- 先检查当前位置是不是已经到末尾了，不能再往后走了
- 没问题就把下标 `curr` +1
- 返回自身引用，支持链式调用，对应原生指针的 `++p`

---

### 四、StrBlob 里的配套修改
#### 1. 为什么要声明友元？
```cpp
friend class StrBlobPtr;
```
`StrBlobPtr` 的构造函数需要访问 `StrBlob` 的私有成员 `data`。
普通类不能访问别人的私有成员，声明友元之后就可以了——相当于给 StrBlobPtr 开了专属权限。

#### 2. `begin()` 和 `end()`
```cpp
StrBlobPtr StrBlob::begin() { return StrBlobPtr(*this); }
StrBlobPtr StrBlob::end()   { return StrBlobPtr(*this, data->size()); }
```
- `begin()`：返回指向第 0 个元素的指针
- `end()`：返回指向「尾后位置」的指针（下标等于元素个数），和标准库迭代器的 `end()` 语义完全一致，用来判断遍历是否结束

---

### 五、一句话总结设计思想
> 用 `weak_ptr` 做弱引用观察，用 `check` 函数做双重安全校验，在不延长对象生命周期的前提下，实现了一个永远不会野、不会越界的安全指针类。

这也是 C++ 标准库迭代器的底层设计思路之一——把指针的行为封装成类，在类内部做安全检查，把未定义行为变成可控的异常。