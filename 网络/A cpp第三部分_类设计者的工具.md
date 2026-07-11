---
title: C++标准库笔记（第三部分）
category: C++底层
tags:
  - cpp
  - cpp/mem
difficulty: 进阶
source: C++ Primer 第五版
skills:
  - obsidian-markdown
---

## C++ Primer 第三部分：类设计者的工具
## 第14章 操作重载与类型转换

---

> 💡 **工程权重**：⭐⭐⭐⭐⭐
> 操作重载是 C++ 区别于 C 的核心特性之一，让自定义类型拥有与内置类型一致的使用体验。

---

### 14.1 重载运算符的基本概念

#### 14.1.1 核心定义

运算符重载是**函数重载的一种特殊形式**，通过 `operator` 关键字 + 运算符符号定义运算符函数：

```cpp
// 运算符函数的两种等价调用形式
data1 + data2;                  // 表达式形式
operator+(data1, data2);        // 函数调用形式

// 当定义为成员函数时：
data1 += data2;                 // 等价于
data1.operator+=(data2);
```

#### 14.1.2 核心约束

| 约束 | 规则 |
| --- | --- |
| **最少操作数** | 至少一个操作数为**类类型**（不允许重载内置类型运算符） |
| **不改变优先级** | 重载不改变运算符的优先级、结合性、操作数个数 |
| **不能创造新运算符** | 只能重载已有的运算符，不能发明 `operator**` 等 |
| **不能重载的运算符** | `::`、`.*`、`.`、`?:` |
| **默认实参** | 除 `operator()` 外，运算符函数不允许默认实参 |

#### 14.1.3 表 14.1：可重载的运算符

```cpp
+   -   *   /   %   ^   &   |   ~
!   =   <   >   +=  -=  *=  /=  %=
^=  &=  |=  <<  >>  >>= <<= ==  !=
<=  >=  &&  ||  ++  --  ->* ,   ->
[]  ()  new new[] delete delete[]
```

> 共 40+ 个运算符可重载，唯一不可重载的四个：`::`、`.*`、`.`、`?:`。

#### 14.1.4 成员 vs 非成员函数的选择

| 运算符 | 应定义为成员 | 原因 |
| --- | --- | --- |
| `=`（赋值） | ✅ **必须** | 标准规定 |
| `[]`（下标） | ✅ **必须** | 标准规定 |
| `()`（调用） | ✅ **必须** | 标准规定 |
| `->`（箭头） | ✅ **必须** | 标准规定 |
| `+=`/`-=` 等**复合赋值** | ✅ **通常** | 修改左操作数状态 |
| `++`/`--`（自增减） | ✅ **通常** | 修改对象状态 |
| `*`/`->`（解引用） | ✅ **通常** | 需要访问内部数据 |
| `+`/`-`/`==`/`<` 等**对称运算符** | ❌ **推荐非成员** | 支持两侧的类型转换 |

```cpp
// 对称运算符应定义为非成员 — 支持两侧类型转换
class SmallInt {
    friend SmallInt operator+(const SmallInt&, const SmallInt&);
public:
    SmallInt(int i = 0) : val(i) {}
private:
    std::size_t val;
};

// 非成员版本：3 + smallInt 或 smallInt + 3 都合法
SmallInt operator+(const SmallInt& lhs, const SmallInt& rhs) {
    return SmallInt(lhs.val + rhs.val);
}

SmallInt s(42);
SmallInt s2 = s + 3;  // ✅ 3 隐式转为 SmallInt
SmallInt s3 = 3 + s;  // ✅ 3 隐式转为 SmallInt（非成员时才合法）
```

> ⚠️ **设为成员的后果**：`s + 3` 可以工作（`s.operator+(3)`），但 `3 + s` 不合法（`3.operator+(s)` 不存在）。对称运算符必须是非成员。

#### 14.1.5 重载与求值规则

```cpp
// 短路求值规则不适用于重载版本
bool b1 = expr1 && expr2;     // 内置 &&：expr1 为 false 则 expr2 不求值
bool b2 = expr1 || expr2;     // 内置 ||：expr1 为 true 则 expr2 不求值

// 若重载了 operator&& 或 operator||
bool b3 = operator&&(expr1, expr2);  // 两个表达式都会求值！
bool b4 = operator||(expr1, expr2);  // 两个表达式都会求值！
```

> ⚠️ **不推荐重载 `&&`、`||`、`,`**：重载版本失去短路求值和逗号表达式的顺序保证，行为与内置语义不一致，容易混淆。

#### 14.1.6 调用约定总结

| 调用形式 | 成员函数版本 | 非成员函数版本 |
| --- | --- | --- |
| `a @ b` | `a.operator@(b)` | `operator@(a, b)` |
| `@ a` | `a.operator@()` | `operator@(a)` |
| `a @` | `a.operator@(int)` | `operator@(a, int)` |

> 注：后置 `++`/`--` 通过额外的 `int` 参数区分，该参数在调用时被传 0（编译器隐式传递，用户不可显式传参）。

---

### 14.2 输入输出运算符

#### 14.2.1 输出运算符 `<<`

```cpp
// 标准签名：必须是非成员函数
ostream& operator<<(ostream& os, const Sales_data& item) {
    os << item.isbn() << " " << item.units_sold << " "
       << item.revenue << " " << item.avg_price();
    return os;
}
```

**设计要点**：

| 规则 | 原因 |
| --- | --- |
| 参数1 = `ostream&`（非常量） | 写操作修改流状态 |
| 参数2 = `const T&`（常量引用） | 打印不修改对象 |
| 返回 `ostream&` | 支持链式调用：`cout << a << b` |
| 通常定义为 **友元** | 需要访问类的非公有成员 |
| 不打印额外格式 | 如 `---`、`\n` 等由调用者决定，不是运算符的责任 |

```cpp
// 典型的友元声明
class Sales_data {
    friend std::ostream& operator<<(std::ostream&, const Sales_data&);
    friend std::istream& operator>>(std::istream&, Sales_data&);
    // ... 其他成员
};
```

#### 14.2.2 输入运算符 `>>`

```cpp
istream& operator>>(istream& is, Sales_data& item) {
    double price;
    is >> item.bookNo >> item.units_sold >> price;

    if (is)                // 检查输入是否成功
        item.revenue = item.units_sold * price;
    else
        item = Sales_data();  // 输入失败 → 将对象置为默认状态

    return is;
}
```

**错误处理**：

```cpp
// 错误的输入运算符（未处理输入失败）
istream& operator>>(istream& is, Sales_data& item) {
    is >> item.bookNo >> item.units_sold >> price;
    item.revenue = item.units_sold * price;  // ❌ 若输入失败，units_sold 未定义
    return is;
}

// 正确的做法：先检查输入状态，再进行运算
istream& operator>>(istream& is, Sales_data& item) {
    double price;
    is >> item.bookNo >> item.units_sold >> price;
    if (is) {
        item.revenue = item.units_sold * price;
    } else {
        item = Sales_data();  // 失败 → 重置对象避免部分写入
    }
    return is;
}
```

**设计要点**：

| 规则 | 原因 |
| --- | --- |
| 参数1 = `istream&`（非常量） | 读操作修改流状态 |
| 参数2 = `T&`（**非常量**） | 需要修改对象内容 |
| 返回 `istream&` | 支持链式 |
| **必须处理 IO 错误** | 输入可能失败（EOF、格式错误等） |
| 失败时重置对象 | 避免对象处于半赋值状态 |

> 💡 **输入运算符 vs 输出运算符的关键区别**：输出运算符的参数2是 `const`，输入运算符的参数2是**非** `const`。输出可以假定对象已构造完毕，输入则必须考虑对象在读取过程中可能处于不一致状态。

---

### 14.3 算术与关系运算符

#### 14.3.1 核心设计原则

**算术运算符**通常定义为**非成员函数**，利用复合赋值运算符实现：

```cpp
// 算术运算符使用复合赋值实现（推荐模式）
Sales_data operator+(const Sales_data& lhs, const Sales_data& rhs) {
    Sales_data sum = lhs;   // 拷贝左侧
    sum += rhs;              // 利用 += 完成实际运算
    return sum;
}
```

**为什么这么写**：
1. 代码复用——`+=` 中已经包含了完整的加法逻辑
2. 一致性——确保 `+` 和 `+=` 的行为完全一致
3. 维护性——修改 `+=` 的逻辑，`+` 自动同步

#### 14.3.2 相等运算符 `==` 和 `!=`

```cpp
// 相等运算符
bool operator==(const Sales_data& lhs, const Sales_data& rhs) {
    return lhs.isbn() == rhs.isbn() &&
           lhs.units_sold == rhs.units_sold &&
           lhs.revenue == rhs.revenue;
}

// 不等运算符委托给 ==
bool operator!=(const Sales_data& lhs, const Sales_data& rhs) {
    return !(lhs == rhs);
}
```

**相等运算符的要求**：

| 性质 | 含义 | 示例 |
| --- | --- | --- |
| **自反性** | 每个对象等于自身 | `a == a` 必须为 true |
| **对称性** | 交换顺序结果不变 | `a == b` 等价于 `b == a` |
| **传递性** | 链式等价的传递 | `a==b && b==c → a==c` |
| **与 `!=` 一致性** | `==` 与 `!=` 互为否定 | 如果定义了 `==`，通常也要定义 `!=` |

> 💡 **工程建议**：`!=` 始终委托给 `==` 实现，确保两者行为一致。一行代码的事，不要重复写完整比较逻辑。

#### 14.3.3 关系运算符 `<`

```cpp
// 为使用标准库容器和算法，通常需要定义 < 运算符
bool operator<(const Sales_data& lhs, const Sales_data& rhs) {
    return lhs.isbn() < rhs.isbn();
}
```

**`<` 运算符的要求**：

| 性质 | 含义 |
| --- | --- |
| **严格弱序（strict weak ordering）** | 关联容器和 `sort` 算法对 `<` 的要求 |
| **反对称性** | `a < b` 与 `b < a` 不能同时为 true |
| **传递性** | `a<b && b<c → a<c` |
| **等价性传递** | !(a<b) && !(b<a) 且 !(b<c) && !(c<b) → !(a<c) && !(c<a) |

**什么时候定义 `<`**：

```cpp
// ✅ 类只有一个自然排序方式 → 定义 <
class Book {
public:
    bool operator<(const Book& rhs) const { return isbn < rhs.isbn; }
private:
    string isbn;
};

// ❌ 类没有自然排序 → 不应该定义 <
class Sales_data {};  // 按 isbn 排序？按销量排序？都不合理
```

> ⚠️ **`<` 与 `==` 的一致性陷阱**：如果 `<` 的判断逻辑与 `==` 不一致，则会出现 `!(a<b) && !(b<a)` 但 `a!=b` 的情况，这在关联容器中会导致**等价但不等**的元素被视为重复键。

```cpp
// 反例：< 和 == 不一致的类
class BadExample {
    int id;     // 主键
    int score;  // 副键

    bool operator<(const BadExample& rhs) const { return id < rhs.id; }
    bool operator==(const BadExample& rhs) const { return id == rhs.id && score == rhs.score; }
    // 问题：a.id=1,a.score=100 与 b.id=1,b.score=200
    //   !(a<b) && !(b<a) → true  （id 相等）
    //   a==b → false               （score 不相等）
    // → set 会认为它们是"等价"的，只保留一个 — 违反直觉！
};
```

---

### 14.8 函数调用运算符与 lambda 表达式

> 💡 **工程权重**：⭐⭐⭐⭐⭐
> `operator()` 让对象变得"可调用"，lambda 本质上是编译器生成的匿名 `operator()` 类。

#### 14.8.1 函数调用运算符 `operator()`

**核心定义**：重载 `operator()` 的类被称为**函数对象（functor）**，其对象可像函数一样调用：

```cpp
struct absInt {
    int operator()(int val) const {   // 函数调用运算符
        return val < 0 ? -val : val;
    }
};

absInt absObj;
int i = absObj(-42);    // 等价于 absObj.operator()(-42)
```

**关键特性**：

| 特性 | 说明 |
| --- | --- |
| 可以重载多个 `operator()` | 通过不同的参数列表区分 |
| 可以有状态 | 类可以持有成员变量，记录调用次数、配置参数等 |
| 可以作为模板参数 | 标准库算法大量接受函数对象 |

```cpp
// 有状态的函数对象
class ShorterString {
public:
    bool operator()(const string& a, const string& b) const {
        return a.size() < b.size();
    }
};

// 标准库算法使用
stable_sort(words.begin(), words.end(), ShorterString());
```

#### 14.8.2 lambda 表达式

**lambda 的本质**：lambda 是编译器生成的一个**匿名函数对象**（匿名类 + 匿名实例）。

```cpp
// lambda 表达式
auto f = [](int a, int b) { return a + b; };

// 编译器展开等价于：
class unnamed_lambda {
public:
    auto operator()(int a, int b) const { return a + b; }
};
auto f = unnamed_lambda{};
```

**lambda 捕获的本质**：

```cpp
int c = 10;
// 值捕获
auto f1 = [c](int a, int b) { return a + b + c; };

// 编译器展开：
class unnamed_lambda {
    int c;                              // 捕获的变量成为成员变量
public:
    unnamed_lambda(int c_) : c(c_) {}   // 构造函数初始化
    auto operator()(int a, int b) const { return a + b + c; }
};
auto f1 = unnamed_lambda{c};

// 引用捕获
auto f2 = [&c](int a, int b) { return a + b + c; };

// 编译器展开：
class unnamed_lambda {
    int& c;                             // 引用成员
public:
    unnamed_lambda(int& c_) : c(c_) {}
    auto operator()(int a, int b) const { return a + b + c; }
};
```

#### 14.8.3 捕获列表详解

| 捕获方式 | 语法 | 含义 |
| --- | --- | --- |
| 值捕获 | `[x]` | x 被拷贝到 lambda 对象中（lambda 创建时确定） |
| 引用捕获 | `[&x]` | x 以引用方式捕获（必须保证 lambda 执行时 x 仍存活） |
| 隐式值捕获 | `[=]` | 使用到的局部变量都值捕获 |
| 隐式引用捕获 | `[&]` | 使用到的局部变量都引用捕获 |
| 混合捕获 | `[=, &x]` | x 引用捕获，其他值捕获 |
| 混合捕获 | `[&, x]` | x 值捕获，其他引用捕获 |

```cpp
// 捕获的坑：值捕获在 lambda 创建时确定
int x = 42;
auto f = [x]() { return x; };
x = 0;
f();     // 返回 42！不是 0。因为 x 在 lambda 创建时已被拷贝

// 引用捕获：随时读取最新值
int y = 42;
auto g = [&y]() { return y; };
y = 0;
g();     // 返回 0。每次访问的是外部的 y
```

#### 14.8.4 可变 lambda

默认 lambda 的 `operator()` 是 `const` 的。如果需要在 lambda 内部修改捕获的值，使用 `mutable`：

```cpp
int count = 0;

// ❌ 编译错误：不能修改值捕获的变量
auto f1 = [count]() { return ++count; };

// ✅ mutable 允许修改值捕获的拷贝
auto f2 = [count]() mutable { return ++count; };
f2();    // 1
f2();    // 2（修改的是 lambda 内部的拷贝，外部的 count 不变）
```

> ⚠️ `mutable` 只影响值捕获的变量。引用捕获的变量修改不需要 `mutable`。

#### 14.8.5 返回值类型推导

```cpp
// 单条 return → 自动推导
auto f = [](int i) { return i * 2; };    // 返回 int

// 多条 return → 必须尾置返回类型
auto f2 = [](int i) -> double {
    if (i < 0) return -i;
    else return i;
};
```

#### 14.8.6 lambda 捕获列表中的初始化（C++14）

```cpp
// C++14 允许在捕获列表中初始化变量
auto f = [x = 42](int a) { return a + x; };

// 支持移动捕获（无法移动的 unique_ptr 也能捕获）
auto p = make_unique<int>(42);
auto f2 = [p = move(p)]() { return *p; };
```

> 💡 移动捕获是 C++14 引入的 Lambda 核心增强，常用于需要将 `unique_ptr` 或 `future` 移入 lambda 内部管理生命周期。

#### 14.8.7 lambda 与标准库结合

```cpp
// 按长度排序
stable_sort(words.begin(), words.end(),
    [](const string& a, const string& b) {
        return a.size() < b.size();
    });

// 统计长度大于 5 的单词
auto cnt = count_if(words.begin(), words.end(),
    [](const string& s) { return s.size() > 5; });

// 配合 partition/find_if 等算法
auto it = partition(words.begin(), words.end(),
    [](const string& s) { return s.size() > 5; });
```

#### 14.8.8 function 类模板

标准库 `function` 类型可以存储任意可调用对象（函数指针、lambda、函数对象）：

```cpp
#include <functional>

// function<int(int, int)> 可以保存任何接受两个 int 返回 int 的可调用对象
function<int(int, int)> f;

f = [](int a, int b) { return a + b; };   // lambda
f = [](int a, int b) { return a * b; };   // 可以重新赋值

// 函数指针也可以
int add(int a, int b) { return a + b; }
f = add;

// 函数对象也可以
struct Divide {
    int operator()(int a, int b) { return a / b; }
};
f = Divide();
```

| 可调用对象类型 | 示例 | 能否存入 `function` |
| --- | --- | --- |
| 函数指针 | `int (*)(int, int)` | ✅ |
| lambda | `[](int,int)->int{...}` | ✅ |
| 函数对象 | `struct { int operator()(int,int); };` | ✅ |
| 成员函数指针 | `&Class::method` | ❌（需 bind 适配） |

---

## 第16章 模板与泛型编程

### §16.1 函数模板

> 💡 **工程权重**：⭐⭐⭐⭐⭐
> 模板是 C++ 泛型编程的核心。函数模板让算法与类型无关。

#### 16.1.1 核心定义

```cpp
// 函数模板：一个适用于多种类型的函数蓝图
template <typename T>
int compare(const T& v1, const T& v2) {
    if (v1 < v2) return -1;
    if (v2 < v1) return 1;
    return 0;
}
```

#### 16.1.2 模板实例化

编译器根据调用实参**推断模板类型参数**，并生成具体函数：

```cpp
compare(1, 2);                  // T 推断为 int → 实例化 compare(const int&, const int&)
compare(1.0, 2.0);              // T 推断为 double → 实例化 compare(const double&, const double&)
compare("abc", "cde");          // T 推断为 const char*

// 不同类型参数导致编译错误
compare(1, 2.0);                // ❌ 矛盾：T 同时被推断为 int 和 double
```

**修正**：使用两个类型参数或多个模板参数：

```cpp
template <typename T1, typename T2>
int compare(const T1& v1, const T2& v2) {
    if (v1 < v2) return -1;
    if (v2 < v1) return 1;
    return 0;
}

compare(1, 2.0);    // ✅ T1 = int, T2 = double
```

#### 16.1.3 模板类型参数与模板非类型参数

| 参数类型 | 语法 | 示例 |
| --- | --- | --- |
| **类型参数** | `typename T` 或 `class T` | `T` 可以是任意类型 |
| **非类型参数** | `size_t N` | `N` 必须是编译期常量 |

```cpp
// 非类型模板参数：编译期常量
template <typename T, size_t N>
T* my_begin(T (&arr)[N]) {
    return arr;        // N 是数组的大小（编译期决定）
}

int arr[10];
int *p = my_begin(arr);  // T = int, N = 10

// 非类型参数的常见形式
template <size_t N>                  // size_t 常量
template <int N>                     // int 常量
template <char* p>                   // 外部链接的指针
template <void(*func)(int)>          // 函数指针
```

> ⚠️ **非类型参数的限制**：必须是编译期常量表达式（整型字面量、枚举、具有外部链接的对象指针或函数指针）。

#### 16.1.4 模板编译模型

模板在两个阶段编译：

| 阶段 | 编译器做的事 | 何时检查 |
| --- | --- | --- |
| **第一阶段** | 检查模板语法本身（不检查类型相关操作） | 看到模板定义时 |
| **第二阶段** | 实例化时检查类型相关操作 | 看到实例化调用时 |

```cpp
template <typename T>
void foo(T t) {
    t.undefined_method();   // 第一阶段：不报错（T 未确定时不知道有没有此方法）
}

struct A {};
foo(A{});                  // 第二阶段：❌ 编译错误，A 没有 undefined_method
```

#### 16.1.5 函数模板重载

```cpp
// 重载模板 + 普通函数
template <typename T>
string debug_rep(const T& t) {
    ostringstream ret;
    ret << t;
    return ret.str();
}

template <typename T>
string debug_rep(T* p) {
    ostringstream ret;
    ret << "pointer: " << p;
    if (p) ret << " " << debug_rep(*p);  // 递归调用第一个版本
    else   ret << " null pointer";
    return ret.str();
}

string s("hello");
debug_rep(s);           // 调用 const T& 版本（T = string）
debug_rep(&s);          // 调用 T* 版本（T = string）
debug_rep(nullptr);     // T* 版本，T = nullptr_t
```

**重载排序规则**：

| 优先级 | 匹配类型 |
| --- | --- |
| 1（最高） | 普通函数（非模板） |
| 2 | 更特化的模板（如 `T*` 比 `const T&` 特化程度高） |
| 3 | 一般模板 |

```cpp
template <typename T> void f(T);       // 一般模板
template <typename T> void f(T*);      // 更特化的模板
void f(double);                        // 普通函数

f(42);      // 普通函数（int→double 不匹配）→ 一般模板 T = int
f('c');     // 普通函数（char→double 不匹配）→ 一般模板 T = char
f(3.14);    // ✅ 普通函数 f(double) 精确匹配，优先级最高
f(&i);      // ✅ T* 版本（比一般模板更特化）
```

> ⚠️ **模板重载的常见陷阱**：当普通函数和模板同时匹配时，编译器选择普通函数。但如果普通函数需要类型转换而模板完全匹配，模板优先。

---

### §16.2 模板实参推断

> 💡 **工程权重**：⭐⭐⭐⭐
> 编译器通过函数调用实参推断模板类型参数的过程。

#### 16.2.1 推断过程中的类型转换

唯一自动进行的转换：**顶层 const 忽略**和**数组/函数到指针的转换**。

```cpp
template <typename T> T fobj(T, T);      // 传值
template <typename T> T fref(const T&, const T&);  // 传引用

string s1("hello");
const string s2("world");

fobj(s1, s2);   // ✅ T = string，传值时顶层 const 被忽略
fref(s1, s2);   // ✅ T = const string 的引用？不，T = string，const& 是底层 const

// 数组到指针的转换
int arr[10];
fobj(arr, arr);  // ✅ T = int*，传值时数组退化为指针
fref(arr, arr);  // ❌ 错误？T = int[10]？两个数组类型不匹配
```

> ⚠️ **模板推断不允许隐式类型转换**（除上述例外）。`compare(1, 2.0)` 会编译错误，因为 T 不能同时是 int 和 double。

#### 16.2.2 显式模板实参

当编译器无法推断或希望控制推断结果时，显式指定模板实参：

```cpp
template <typename T1, typename T2, typename T3>
T1 sum(T2, T3) { /* ... */ }

// ❌ 编译器无法推断 T1（T1 不参与函数参数）

// ✅ 显式指定 T1
auto val = sum<long long>(1, 2);  // T1 = long long, T2 = int, T3 = int
```

**显式模板实参的规则**：

```cpp
template <typename T1, typename T2, typename T3>
T3 alternative_sum(T1, T2);        // 返回值类型 T3 在最后

// 最常用：返回类型放在最前面，以便只显式指定一个
auto val = alternative_sum<long long>(1, 2);  // ❌ 错误：T1 = long long, T2 推断, T3 未指定
auto val = alternative_sum<long long, int, long long>(1, 2);  // ✅ 全部指定
```

> 💡 **最佳实践**：将需要显式指定的模板参数放在最前面，可推断的放在后面。

#### 16.2.3 尾置返回类型

当返回类型依赖于模板参数时，必须使用尾置返回类型：

```cpp
// 需要返回两个序列中元素的公共类型
template <typename It>
auto fcn(It beg, It end) -> decltype(*beg) {  // 尾置返回类型
    return *beg;   // 返回迭代器指向元素的引用
}

// C++14 可以省略 decltype，编译器自动推导
template <typename It>
auto fcn2(It beg, It end) {  // C++14 自动推导返回类型
    return *beg;
}

// 需要类型转换时仍需 decltype
template <typename It>
auto fcn3(It beg, It end) -> typename remove_reference<decltype(*beg)>::type {
    return *beg;   // 返回值，非引用
}
```

#### 16.2.4 引用折叠与右值引用

**引用折叠规则**（C++11 引入，C++14 相同）：

| 原始类型 | 折叠后 |
| --- | --- |
| `T& &` | `T&` |
| `T& &&` | `T&` |
| `T&& &` | `T&` |
| **`T&& &&`** | **`T&&`** |

**转发（forwarding）引用**（也称为万能引用）：

```cpp
template <typename T>
void func(T&& arg) {        // T&& 不是右值引用！是转发引用
    // arg 可能是左值或右值
}

int x = 42;
func(x);        // T = int& → 折叠后 T&& = int&（左值）
func(42);       // T = int → T&& = int&&（右值）
```

**完美转发**：

```cpp
template <typename F, typename T1, typename T2>
void flip(F f, T1&& t1, T2&& t2) {
    f(std::forward<T1>(t1), std::forward<T2>(t2));
    // forward<t1>(t1) 保持 t1 的左值/右值属性
}

void g(int&& i, int& j) { /* ... */ }

int j = 42;
flip(g, 42, j);  // 42 传递时保持右值属性，j 保持左值属性
```

> 💡 `std::forward<T>(arg)` ≈ `static_cast<T&&>(arg)`，通过引用折叠规则保持值类别。

---

### §16.4 可变参数模板

> 💡 **工程权重**：⭐⭐⭐⭐
> 可变参数模板是 C++11 引入的核心特性，支持任意数量参数的模板。

#### 16.4.1 参数包

| 参数包类型 | 定义语法 | 示例 |
| --- | --- | --- |
| **模板参数包** | `typename... Args` | `template <typename T, typename... Args>` |
| **函数参数包** | `Args... rest` | 参数包展开为逗号分隔的参数列表 |

```cpp
// 可变参数函数模板
template <typename T, typename... Args>
void foo(const T& t, const Args&... rest) {
    cout << sizeof...(Args) << endl;   // 参数包中类型参数的个数
    cout << sizeof...(rest) << endl;   // 参数包中函数参数的个数
}

foo(1, "hello", 3.14, 'c');
// Args = {const char*, double, char} → sizeof...(Args) = 3
// rest = ("hello", 3.14, 'c') → sizeof...(rest) = 3
```

#### 16.4.2 包展开

```cpp
// 递归展开 — 最经典的实现模式
// 基础情形（终止递归）
template <typename T>
ostream& print(ostream& os, const T& t) {
    return os << t;           // 只有一个参数时直接输出
}

// 递归情形（展开参数包）
template <typename T, typename... Args>
ostream& print(ostream& os, const T& t, const Args&... rest) {
    os << t << ", ";
    return print(os, rest...);   // rest... 展开为剩余的参数
}

print(cout, 1, "hello", 3.14);
// 第一次：T=1, Args={const char*, double}, rest={"hello", 3.14}
// 第二次：T="hello", Args={double}, rest={3.14}
// 第三次：T=3.14, no Args → 调用基础情形
```

**包展开的模式匹配方式**：

```cpp
// 包展开不只是简单的"逗号分隔"，还可以应用模式
template <typename... Args>
void expand(Args... args) {
    // 每个参数乘以 2 后展开
    print(cout, (args * 2)...);     // 展开为 print(cout, args1*2, args2*2, ...)
}

expand(1, 2, 3);
// 展开为 print(cout, 2, 4, 6);
```

#### 16.4.3 sizeof... 运算符

```cpp
template <typename... Args>
void g(Args... args) {
    cout << sizeof...(Args) << endl;   // 类型参数个数（编译期）
    cout << sizeof...(args) << endl;   // 函数参数个数（编译期）
}
```

#### 16.4.4 完美转发与可变参数模板的经典组合

```cpp
template <typename... Args>
void emplace_back(Args&&... args) {
    // std::forward<Args>(args)... 保持每个参数的值类别
    new (this) value_type(std::forward<Args>(args)...);
}

// 使用示例
struct Foo {
    Foo(int, string, double) {}
};

// 工厂函数 + 完美转发 + 可变参数
template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

auto p = make_unique<Foo>(42, "hello", 3.14);
// Args = {int, const char(&)[6], double}
// forward 保持右值属性
```

> 💡 这个模式是现代 C++ 中最通用的工厂函数模式。`std::make_shared`、`std::make_unique`、`std::vector::emplace_back` 全部基于此。

#### 16.4.5 可变参数模板与继承

```cpp
// 通过可变参数模板实现递归继承（tuple 的核心实现）
template <typename... Types> class Tuple;

// 递归情形
template <typename Head, typename... Tail>
class Tuple<Head, Tail...> : private Tuple<Tail...> {
public:
    Head head;
    // ...
};

// 基础情形（空 tuple）
template <>
class Tuple<> {};

// Tuple<int, string, double>
// → 继承 Tuple<string, double>
//   → 继承 Tuple<double>
//     → 继承 Tuple<>
```

#### 16.4.6 可变参数模板与初始化列表展开

```cpp
// 另一种包展开技巧：利用初始化列表的求值顺序保证
template <typename... Args>
void expand(Args... args) {
    int arr[] = { (print(args), 0)... };  // 初始化列表保证从左到右求值
}

expand(1, "hello", 3.14);
// 展开为：int arr[] = { (print(1), 0), (print("hello"), 0), (print(3.14), 0) };
// arr = {0, 0, 0}
// 副作用：print 被依次调用
```

---

### ⚠️ 踩坑记录

#### 1. lambda 默认 const

```cpp
int x = 42;
auto f = [x]() { x++; };       // ❌ 编译错误：operator() 默认 const
auto f = [x]() mutable { x++; }; // ✅ mutable 允许修改值捕获的拷贝
```

#### 2. 引用捕获的生命周期

```cpp
function<int()> dangerous() {
    int x = 42;
    return [&x]() { return x; };  // ❌ x 在函数返回后销毁
}                                 // lambda 持有悬空引用！

auto f = dangerous();
f();    // 未定义行为！
```

#### 3. 模板定义必须在头文件中

模板的实例化发生在调用点，编译器必须看到完整定义。不能把模板定义放在 `.cpp`，声明放在 `.h`。

#### 4. 模板重载的歧义

```cpp
template <typename T> void f(T);       // ①
template <typename T> void f(const T&); // ②

int x;
f(x);    // ① T = int；② T = int → 都可以，造成歧义！
```

#### 5. 参数包递归的终止必须提供基础情形

```cpp
// ❌ 缺少基础情形：递归无限展开 → 编译错误
template <typename T, typename... Args>
void print(T t, Args... rest) {
    cout << t;
    print(rest...);   // 最后一个参数时，rest 为空包，T 无法推断
}

// ✅ 必须提供无参数或单参数的基础情形
void print() {}  // 空参数基础情形
```

#### 6. 转发引用必须保持值类别

```cpp
// ❌ 错误：失去了右值信息
template <typename T>
void wrapper(T arg) {
    foo(arg);       // 始终作为左值传递
}

// ✅ 正确：完美转发
template <typename T>
void wrapper(T&& arg) {            // T&& 是转发引用
    foo(std::forward<T>(arg));     // 左值时是左值，右值时是右值
}
```

---

### 关联笔记

- [[A cpp第一部分_cpp基础笔记|C++基础 — 函数重载基础]]
- [[C++ 可调用对象的统一理论框架|可调用对象 — operator() + function + lambda 统一理论]]
- [[A cpp第二部分_cpp标准库|C++ 标准库 — 模板在标准库中的应用]]
- [[6-2-1 POSIX同步原语深度剖析|POSIX 同步原语]]

