---
title: "Windows做服务器 MSYS2 C++ Docker做客户端"
category: 计算机网络
tags:
  - net/practice
  - networking
difficulty: 进阶
source: "自整理"
---

# Windows做服务器 MSYS2 C++ 开发环境  Docker 做客户端
## 1. 子系统选择（核心坑）
- 必选：**UCRT64/MINGW64**（原生Windows程序，支持WinSock）
- 禁用：**MSYS**（POSIX模拟，依赖cygwin1.dll，网络编程必报错）

## 2. pacman 包安装（必记）
- 安装必须带前缀：`mingw-w64-ucrt-x86_64-`
- 必备工具：
  ```bash
  pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake
  ```
- 别装裸包（如`gcc/cmake`），会装成MSYS版本，无法用

## 3. WinSock2 编程（高频报错）
1. 头文件顺序（必对）
   ```cpp
   #define WIN32_LEAN_AND_MEAN
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <windows.h>
   ```
2. 必须初始化：`WSAStartup`/`WSACleanup`
3. 链接库：CMake加`target_link_libraries(xxx PRIVATE ws2_32)`；命令行`-lws2_32`放源文件后

## 4. ASan 内存检测（避错）
- 在 Windows 下的 GCC 中使用 ASan 经常会遇到各种运行时问题。

  前置知识点:

  1. ### ASan (AddressSanitizer) 地址检测器

  - **作用**：C/C++ 内存错误检测工具，**检测内存越界、野指针、重复释放、使用已释放内存**
  - **定位**：运行时检测，编译时注入检测代码
  - **场景**：解决 90% 内存崩溃问题（网络编程、Socket 开发必备）

  ### 2. LSan (LeakSanitizer) 内存泄漏检测器

  - **作用**：ASan 的**子模块**，专门检测 **malloc/new 申请但未释放的内存**
  - **定位**：仅负责泄漏，不负责越界 / 野指针
  - **冲突点**：Windows 下 GCC 版本的 LSan 兼容性极差，**和调试器强冲突**

  ### 3. ptrace（补充说明）

  - **定义**：Linux 系统调用，用于调试器追踪 / 控制进程

  ### 4.1 编译选项

  编译时必须同时添加 `-fsanitize=address` 和 `-g`（生成调试信息），否则错误栈将无法解析。

   复制 插入 新文件

  ```
  g++ -fsanitize=address -g main.cpp -o main -lws2_32
  ```

  ### 4.2 LeakSanitizer (LSan) 与 ptrace 冲突

  昨晚我们遇到了 `LeakSanitizer has encountered a fatal error` 且提示 `does not work under ptrace`。这是因为 VSCode 的调试器底层使用了 `ptrace` 机制，与 LSan 冲突。

  **避坑指南**：

  - **不要在 VSCode 的调试模式 (F5) 下运行启用了 ASan 的程序**。
  - 请在终端命令行直接运行生成的 `.exe` 文件。
  - 如果只想检测越界/空指针，不需要内存泄漏检测，可以关闭 LSan：
    - PowerShell: `$env:ASAN_OPTIONS="detect_leaks=0"; ./main.exe`
    - CMD: `set ASAN_OPTIONS=detect_leaks=0 && main.exe`

  ### 4.3 野指针与空指针的 ASan 表现差异

  昨晚的测试发现，向 `::recv` 传入 `nullptr` 时 ASan 不报错，但传入 `0xdeadbeef` 时会报错。这是因为：

  - Windows 的 `recv` 系统调用在内核层会拦截 `NULL` 指针，直接返回 `-1` 并设置 `errno` 为 `EFAULT`，**不会**发生实际内存写入，ASan 无法拦截。
  - 传入 `0xdeadbeef` 等野指针时，系统可能认为地址合法并尝试写入，此时 ASan 才能成功拦截并报告 `SEGV`。

  **避坑指南**：不要指望 ASan 能捕获所有传入系统调用的空指针，C++ 层面必须手动检查返回值（如 `< 0`）和 `errno`。

- 要处理一场perror 不然程序直接就return出去了

- ```c++
   		// 当 recv 返回 <= 0 时，检查是否发生了系统错误
          if (bytes_read < 0) {
              perror("❌ recv failed"); // perror 会自动打印 errno 对应的错误信息
              // 如果是非法地址，这里会打印：recv failed: Bad address
          }else if (bytes_read == 0) {
              std::cout << "✅ Connection closed by peer" << std::endl;
          }
  ```

  ##### ASan 是干嘛的？
  AddressSanitizer (ASan) 是一款由编译器插桩驱动的内存错误检测工具。它的核心职责是在**用户空间**拦截非法的内存访问。
  主要检测类型包括：

  - **堆/栈/全局变量的越界访问** (Buffer Overflow)
  - **释放后使用** (Use-After-Free)
  - **双重释放** (Double Free)
  - **野指针访问** (Wild Pointer Access)

  **工作原理**：编译时，ASan 会在你的内存访问指令前后插入检查代码（影子内存映射）。当程序在用户态尝试读写非法地址时，ASan 会第一时间拦截并让程序崩溃，输出详细的错误堆栈。

  

  ##### 2为什么传野指针只打印了 `Bad address`，ASan 却没报错？
  这是**用户态拦截**与**内核态拦截**的优先级差异导致的。

  - **ASan 的盲区**：ASan 只能拦截发生在**用户态**的内存读写。当你把 `0xdeadbeef` 这样的野指针传给 `recv()` 时，数据并没有在用户态被写入，而是直接作为参数传入了系统调用。
  - **内核态截胡**：`recv()` 是系统调用，执行瞬间陷入内核态。Linux 内核尝试将网络数据拷贝到 `0xdeadbeef`，发现该地址非法，直接在内核态拒绝，返回 `-1` 并设置 `errno` 为 `EFAULT` (Bad address)。
  - **结论**：因为数据根本没在用户态落地，ASan 的插桩代码没有机会执行。错误被操作系统内核提前拦截了，所以你的 `if (bytes_read < 0) perror(...)` 捕获了它，而 ASan 毫无察觉。

  *(注：如果想让 ASan 报错，必须在用户态直接解引用野指针，例如 `*i_buffer = 'A';`)*

  

  ##### LSan 为什么要关？
  LSan (LeakSanitizer) 是 ASan 的子组件，负责在程序退出时检测内存泄漏。关闭它 (`ASAN_OPTIONS=detect_leaks=0`) 主要有两个原因：

  1. **避免与调试器/终端冲突**：LSan 在程序退出时通过特殊机制扫描内存，这在 Linux 下极易与 `ptrace` (调试器底层机制) 冲突。如果在 VSCode 调试模式 (F5) 下运行，LSan 会直接崩溃并抛出 `LeakSanitizer has encountered a fatal error`，掩盖真正的逻辑错误。
  2. **聚焦核心内存错误**：在开发网络程序时，越界和野指针是致命的硬伤，必须立刻排查；而内存泄漏属于资源管理问题，不影响程序当下的逻辑正确性。关闭 LSan 可以减少干扰，让我们专注解决 ASan 捕获的越界等核心问题，待逻辑跑通后再开启 LSan 排查泄漏。

## 5. VSCode + CMake 配置
1. CMake路径指向MSYS2：`F:/lib__/msys2/ucrt64/bin/cmake.exe`
2. 生成器：`MinGW Makefiles`
3. 编译器指定MSYS2的`gcc/g++.exe`
4. 旧CMake路径清空，避免冲突

## 6. HTTP 交互
- 服务端必须返回**完整HTTP头**：`HTTP/1.1 200 OK\r\n\r\n`，否则客户端阻塞
- 不乱改缓冲区，避免返回乱码

## 7. 配置环境变量

记得去windows环境变量配 msys2的bin目录,vscode和windwos的sh, cmd都能找到对应的编译器工具