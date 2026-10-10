---
title: Vue3 前置认知基础—老式 JS 与 DOM
category: VUE3
tags:
  - VUE3
  - 前置知识
  - JavaScript
  - DOM
difficulty: 入门
created: 2026-07-31
modified: 2026-07-31
maturity: 完善
---

> [!info] 一句话
> 这是系列的第 0 篇——**你早已遗忘的老式 JS 底子**。目标是：看到 `getElementById`、`addEventListener` 这类函数不再陌生，能读懂老式代码。只讲会用到的，不追求系统。

## 1. 两个重要事实

### ① 大多数"函数"不是 JS 语言自带的，是浏览器给的

JavaScript 分两部分：

| 部分 | 是什么 | 例子 |
|------|--------|------|
| **JS 语言本身**（ECMAScript） | 语法、变量、运算、循环 | `let`、`if`、`for`、`function`、`Math.max()` |
| **浏览器 Web API**（DOM 等） | 浏览器提供的"操作页面"的函数 | `document.getElementById`、`addEventListener` |

> [!tip] 类比：C++ 标准库 vs 系统调用
> JS 语言本身 ≈ C++ 标准库（`std::sort` 等）。
> 浏览器 Web API ≈ 系统调用/你链接的库（`socket()`、`pthread_create`）——**不在语言里，但环境给你**。
>
> 老式 JS 写页面时，大量用的其实是"浏览器给的库"，不是 JS 语法本身。

### ② 页面怎么被 JS 看到

浏览器把整个 HTML 页面组织成一个"对象树"，入口是 `document`：

```
document（整个页面）
├── body
│   ├── div id="btn"   ← 按钮
│   ├── p   id="count" ← 显示数字
│   └── ...
```

`document.getElementById('count')` 就是在**这棵树上按 id 找元素**，找到后返回那个元素对象，然后你就能操作它（改文字、改样式、绑事件）。

> [!tip] C++ 类比
> `document` ≈ 一棵树的数据结构，`getElementById` ≈ 按 key 查找（类似 `std::map`），返回的"元素对象"≈ 指向树节点的指针。

## 2. 核心函数速查表（记住这些就够）

### A. 查找元素（用的最多）

| 函数                                 | 作用            | 返回    | 例子                                                |
| ---------------------------------- | ------------- | ----- | ------------------------------------------------- |
| `document.getElementById('id')`    | 按 id 找元素      | 1 个元素 | `getElementById('btn')`                           |
| `document.querySelector('选择器')`    | 按 CSS 选择器找第一个 | 1 个元素 | `querySelector('.btn')` / `querySelector('#btn')` |
| `document.querySelectorAll('选择器')` | 按 CSS 选择器找全部  | 元素列表  | `querySelectorAll('li')`                          |

> [!note] CSS 选择器（老式 CSS 你一定熟）
> `#btn` = id 是 btn；`.class` = class 含 btn；`li` = 所有 li 标签。`querySelector` 用的就是这语法。

### B. 修改页面（改内容/样式/类）

| 函数/属性                             | 作用               | 例子                              |
| --------------------------------- | ---------------- | ------------------------------- |
| `element.textContent = 'xxx'`     | 改文字内容（最常用）       | `el.textContent = count`        |
| `element.innerHTML = '<b>xx</b>'` | 改 HTML 结构（可注入标签） | `el.innerHTML = '<b>加粗</b>'`    |
| `element.style.color = 'red'`     | 改行内样式            | `el.style.display = 'none'`     |
| `element.classList.add('x')`      | 加 CSS 类          | `el.classList.add('active')`    |
| `element.classList.remove('x')`   | 删 CSS 类          | `el.classList.remove('active')` |

### C. 事件（用户交互）

| 函数                                         | 作用            | 例子      |
| ------------------------------------------ | ------------- | ------- |
| `element.addEventListener('click', fn)`    | 绑定事件，触发时执行 fn | 按钮点击    |
| `element.removeEventListener('click', fn)` | 移除事件绑定        | 清理（防泄漏） |

> [!note] 事件是回调函数
> `addEventListener('click', fn)` 不会立刻执行 fn——它是**注册**：用户点了才执行。这跟你熟悉的 `epoll` 注册回调一个思路：**事件来了才调你**。

### D. 创建/删除元素

| 函数 | 作用 |
|------|------|
| `document.createElement('div')` | 新建元素（还没进页面） |
| `parent.appendChild(el)` | 把 el 追加到父元素末尾 |
| `parent.removeChild(el)` | 从父元素移除 el |

### E. 数据/工具（也是"内置"但环境无关）

| 函数 | 作用 | 例子 |
|------|------|------|
| `console.log(x)` | 打印调试（≈ printf） | `console.log('count=', count)` |
| `JSON.parse(str)` | 字符串 → 对象 | `JSON.parse('{"a":1}')` → `{a:1}` |
| `JSON.stringify(obj)` | 对象 → 字符串 | 发给后端/存 localStorage 都要它 |
| `setTimeout(fn, ms)` | 延迟 ms 毫秒后执行 fn | 重连退避 |
| `setInterval(fn, ms)` | 每 ms 毫秒执行一次 | 定时轮询 |

## 3. Demo 拆解：点按钮计数（对照上面表格读）

```html
<!-- HTML -->
<button id="btn">点我</button>
<p id="count">0</p>
```

```javascript
// JS
let count = 0                                   // ① JS 语法：变量

document.getElementById('btn')                  // ② 查表 A：找按钮元素
  .addEventListener('click', () => {            // ③ 查表 C：绑定点击事件
    count++                                     // ④ JS 语法：改数据
    document.getElementById('count')            // ⑤ 查表 A：找 <p> 元素
      .textContent = count                      // ⑥ 查表 B：改文字
  })
```

**逐行对照**：

| 行 | 用了哪个"函数" | 归属 | 在干嘛 |
|----|---------------|------|--------|
| ① `let count = 0` | 无（语法） | JS 语言 | 定义变量 |
| ② `getElementById('btn')` | 查找函数 | 浏览器 API | 找到按钮 |
| ③ `addEventListener('click', fn)` | 事件函数 | 浏览器 API | 注册点击回调 |
| ④ `count++` | 无（语法） | JS 语言 | 数据加 1 |
| ⑤ `getElementById('count')` | 查找函数 | 浏览器 API | 找到 `<p>` |
| ⑥ `.textContent = count` | 修改属性 | 浏览器 API | 把新值写进页面 |

> [!tip] 看出来的规律
> - **数据操作**（定义、自增）用的是 JS 语法
> - **页面操作**（找元素、绑事件、改文字）全是浏览器 API
> - 所以"老式 JS 难"不是难在语法，是难在要背一堆浏览器函数名——而 Vue3 把这些全替你封装了

## 4. 为什么学 Vue3 前要先认识这些

Vue3 不是不用这些函数，而是**把它们藏起来**了：

```
老式 JS：你亲手  getElementById + textContent + addEventListener
Vue3：   你写   <p>{{ count }}</p> + @click
           ↑ Vue 内部自动帮你调 getElementById/textContent/addEventListener
```

> [!tip] 一句话
> 老式 JS 是"手动挡"（每步亲手操作），Vue3 是"自动挡"（声明一下，内部帮你调这些函数）。
> 先认识手动挡的零件（本篇），再看自动挡怎么封装（[[3-Vue3响应式系统-ref与computed]]），你就不是背，而是真的懂了。

## 5. 自测

1. `getElementById` 是 JS 语言自带的，还是浏览器给的？
2. `addEventListener('click', fn)` 里的 fn 什么时候执行？
3. `document.querySelector('#btn')` 和 `document.getElementById('btn')` 找的是同一个元素吗？

> 答案：
> 1. 浏览器给的（Web API），JS 语言本身没有页面概念。
> 2. 用户点击时才执行（注册回调，事件来了才调）。
> 3. 是。`#btn` 就是 id 选择器，两种写法等价的（`querySelector` 更通用）。

## 下一篇

认识完老式 JS，进入正题——为什么需要 Vue3：[[2-从老式JS到Vue3-为什么需要框架]]
