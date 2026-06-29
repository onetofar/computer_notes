---
title: "Graal编译器公共子表达式消除：从源码到理想图的完整链路"
category: JVM
tags:
  - jvm
  - jvm/jit
difficulty: 深入
source: "深入理解Java虚拟机"
---
# Graal编译器公共子表达式消除：从源码到理想图的完整链路
> 本文档基于《深入理解Java虚拟机》第11章案例，完整梳理Graal编译器从**Java源码→字节码→初始理想图→优化后理想图→机器码**的全链路过程，对应书中代码清单11-17和图11-17，深入Graal源码实现，拆解其核心优化逻辑。
## 一、问题引入：两段代码的本质差异
我们先从书中的两段代码入手，理解公共子表达式消除的核心矛盾：
### 代码1：可直接消除的公共子表达式
```java
// 公共子表达式可被消除
int workload(int a, int b) {
    return (a + b) * (a + b);
}
```
- 核心特征：`a + b`是纯参数运算，无任何副作用，编译器可直接识别为公共子表达式，优化为一次计算：
  ```java
  int tmp = a + b;
  return tmp * tmp;
  ```
### 代码2：传统编译器无法消除的公共子表达式
```java
// 公共子表达式无法被消除（传统编译器视角）
int workload() {
    return (getA() + getB()) * (getA() + getB());
}
```
- 核心矛盾：`getA()`和`getB()`是方法调用，传统编译器默认方法可能存在副作用（如修改全局变量、抛出异常），无法判断两次调用的结果是否一致，因此**不敢消除重复调用**。
- 但Graal编译器可以突破这个限制，安全地消除重复的`getA()`和`getB()`调用——这正是我们要拆解的核心优化。
## 二、阶段1：源码→字节码：栈式指令的隐式依赖
首先，我们看第二段代码编译后的字节码，理解栈式指令的局限性：
```bytecode
// 方法：int workload()
0: invokestatic  #2                  // 调用Demo.getA()，结果压入操作数栈
3: invokestatic  #3                  // 调用Demo.getB()，结果压入操作数栈
6: iadd                              // 弹出栈顶两个数相加，结果压栈
7: invokestatic  #2                  // 再次调用Demo.getA()
10: invokestatic  #3                 // 再次调用Demo.getB()
13: iadd                             // 再次相加
14: imul                             // 弹出两次加法结果相乘
15: ireturn                          // 返回结果
```
### 栈式字节码的致命局限
- **隐式依赖**：数据依赖完全隐藏在操作数栈的状态中，编译器无法直接看到“两次`getA()`是同一个调用”，只能按顺序执行指令。
- **优化盲区**：传统编译器无法跨栈状态识别重复计算，更无法分析方法调用的副作用，因此无法消除重复的`getA()`/`getB()`调用。
---

## 三、阶段2：字节码→初始理想图：`BytecodeParser`的转换过程
Graal的核心解法是：**把隐式的栈式字节码，转换成显式的数据流图（理想图）**，让所有依赖关系一目了然。书中的图11-17就是这个转换过程的直接输出。

### 1. 节点与字节码的一一对应
| 节点编号 | 节点类型                  | 对应字节码指令            | 作用                     |
| -------- | ------------------------- | ------------------------- | ------------------------ |
| 0        | `StartNode`               | 方法入口                  | 控制流起点               |
| 3        | `InvokeNode`              | `invokestatic #Demo.getA` | 第一次调用`getA()`       |
| 6        | `InvokeNode`              | `invokestatic #Demo.getB` | 第一次调用`getB()`       |
| 10       | `InvokeNode`              | `invokestatic #Demo.getA` | 第二次调用`getA()`       |
| 13       | `InvokeNode`              | `invokestatic #Demo.getB` | 第二次调用`getB()`       |
| 15       | `BinaryArithmeticNode(+)` | `iadd`                    | 第一次加法               |
| 8        | `BinaryArithmeticNode(+)` | `iadd`                    | 第二次加法               |
| 16       | `BinaryArithmeticNode(*)` | `imul`                    | 乘法运算                 |
| 17       | `ReturnNode`              | `ireturn`                 | 方法返回                 |
| 2/5/9/12 | `MethodCallTarget`        | 方法调用的目标元数据      | 为后续内联优化准备元信息 |

### 2. 依赖关系的显式化
理想图通过两种边，把字节码的隐式依赖完全显式化：
- **控制流边（实线）**：表示指令执行顺序，如`Start → 3 → 6 → 10 → 13 → 15/8 → 16 → 17`。
- **数据流边（虚线）**：表示数据依赖关系，如：
  - `3 → 15`、`6 → 15`：第一次加法依赖第一次`getA()`和`getB()`的结果；
  - `10 → 8`、`13 → 8`：第二次加法依赖第二次`getA()`和`getB()`的结果；
  - `15 → 16`、`8 → 16`：乘法依赖两次加法的结果。

### 3. 关键源码链路：`BytecodeParser`的解析过程
Graal通过`BytecodeParser`遍历字节码，把每个指令转换成理想图节点，并根据操作数栈的状态添加数据流边，简化版实现如下：
```java
// 1. JVMCI入口：HotSpot向Graal发送编译请求
public class HotSpotGraalCompiler implements JVMCICompiler {
    public InstalledCode compileMethod(CompilationRequest request) {
        // 初始化编译上下文（目标方法、架构、优化级别）
        CompilationContext context = createContext(request);
        // 核心：创建初始理想图
        StructuredGraph graph = createGraph(request.getMethod(), context);
        // 后续优化阶段...
        return installCode(graph);
    }
}

// 2. 创建初始理想图的核心方法
private StructuredGraph createGraph(ResolvedJavaMethod method, CompilationContext context) {
    StructuredGraph graph = new StructuredGraph(method, context);
    GraphBuilderConfiguration config = GraphBuilderConfiguration.getDefault();
    // 调用GraphBuilderPhase，核心是BytecodeParser
    new GraphBuilderPhase(config).apply(graph, context);
    return graph;
}

// 3. BytecodeParser的核心解析逻辑（简化版）
public class BytecodeParser {
    public void build() {
        while (!endOfCode()) {
            switch (currentOpcode()) {
                case INVOKESTATIC:
                    // 创建InvokeNode，关联MethodCallTarget
                    InvokeNode invoke = appendInvoke(opcode, getTargetMethod());
                    // 添加控制流边
                    addControlEdge(currentControl(), invoke);
                    // 根据操作数栈状态添加数据流边
                    addDataDependencies(invoke, stack.popArgs());
                    break;
                case IADD:
                    // 从操作数栈弹出两个元素，创建加法节点
                    ValueNode left = stack.pop();
                    ValueNode right = stack.pop();
                    AddNode add = new AddNode(left, right);
                    append(add);
                    break;
                case IMUL:
                    // 类似创建乘法节点
                    break;
            }
        }
    }
}
```

---

## 四、阶段3：Graal核心优化1：方法内联（消除调用边界）
Graal能优化带方法调用的公共子表达式，第一步就是**方法内联**——把`getA()`和`getB()`的方法体直接展开到调用点，消除方法调用的边界。

### 1. 内联的核心作用
- 消除`InvokeNode`和`MethodCallTarget`节点，把`getA()`/`getB()`的逻辑直接合并到当前理想图中；
- 让Graal可以分析方法的副作用：如果`getA()`是纯函数（如`return 2;`），内联后会变成`ConstantNode(2)`，无任何副作用；
- 为后续的公共子表达式消除创造条件。

### 2. 内联后的理想图变化
以`getA()`实现为`return 2;`、`getB()`实现为`return 3;`为例：
- 原来的4个`InvokeNode`被替换为`ConstantNode(2)`和`ConstantNode(3)`；
- 两个`AddNode`的输入都变成了`ConstantNode(2)`和`ConstantNode(3)`，输出都是`5`；
- 此时理想图中出现了两个完全相同的`AddNode`，为CSE优化提供了目标。

### 3. 关键源码：内联优化阶段
```java
public class InliningPhase extends Phase {
    @Override
    protected void run(StructuredGraph graph, CompilationContext context) {
        for (InvokeNode invoke : graph.getInvokes()) {
            ResolvedJavaMethod target = invoke.getTargetMethod();
            // 判断是否可以内联：无副作用、方法体足够小、调用次数达标
            if (canInline(target, context)) {
                // 把目标方法的理想图合并到当前图中，替换InvokeNode
                inlineInvoke(invoke, target, context);
            }
        }
    }
}
```

---

## 五、阶段4：Graal核心优化2：公共子表达式消除（CSE）
内联完成后，Graal的`CommonSubexpressionEliminationPhase`会遍历理想图，识别并合并重复节点，实现公共子表达式消除。

### 1. CSE的核心原理
- **节点识别**：遍历理想图，通过“操作类型+输入节点”的组合，识别重复节点（如两个输入都是`2`和`3`的`AddNode`）；
- **节点合并**：把重复节点替换为同一个实例，所有依赖原来节点的边，都重定向到合并后的节点；
- **安全保证**：基于理想图的显式依赖和无副作用分析，确保替换不会影响程序语义。

### 2. 对应案例的优化过程
| 优化前节点              | 优化后节点                | 变化说明                        |
| ----------------------- | ------------------------- | ------------------------------- |
| `Invoke#getA()`（两次） | `ConstantNode(2)`（一次） | 两次调用合并为一个常量节点      |
| `Invoke#getB()`（两次） | `ConstantNode(3)`（一次） | 两次调用合并为一个常量节点      |
| `AddNode`（两次）       | `AddNode`（一次）         | 两次加法合并为一个加法节点      |
| `MultiplyNode`（一次）  | `MultiplyNode`（一次）    | 输入边重定向到合并后的`AddNode` |

优化后的理想图简化为：`Start → getA → getB → Add → Multiply → Return`，仅需一次`getA()`、一次`getB()`、一次加法和一次乘法。

### 3. 关键源码：CSE优化阶段
```java
public class CommonSubexpressionEliminationPhase extends Phase {
    @Override
    protected void run(StructuredGraph graph, CompilationContext context) {
        Map<NodeKey, Node> existingNodes = new HashMap<>();
        for (Node node : graph.getNodes()) {
            // 只处理无副作用的节点（如加法、常量）
            if (node instanceof BinaryArithmeticNode && !node.hasSideEffect()) {
                // 基于操作类型和输入节点生成唯一键
                NodeKey key = new NodeKey(node);
                if (existingNodes.containsKey(key)) {
                    // 重定向所有依赖当前节点的边到已存在的节点
                    graph.replaceAllUsages(node, existingNodes.get(key));
                    // 删除重复节点
                    graph.remove(node);
                } else {
                    existingNodes.put(key, node);
                }
            }
        }
    }
}
```

---

## 六、阶段5：优化后理想图→机器码：从显式依赖到原生指令
优化后的理想图，最终会被转换成目标CPU可执行的机器码，Graal通过两个步骤完成这个过程：

### 1. 低级中间表示（LIR）生成
把高层理想图节点转换成目标架构（x86/ARM）的低级指令节点，如：
- `ConstantNode(2)` → `mov $0x2, %rax`
- `ConstantNode(3)` → `mov $0x3, %rbx`
- `AddNode` → `add %rbx, %rax`
- `MultiplyNode` → `imul %rax, %rax`

### 2. 寄存器分配与机器码发射
- 把节点映射到CPU寄存器，彻底消除抽象节点，变成直接的寄存器操作；
- 生成机器码字节数组，通过JVMCI接口安装到HotSpot的`CodeCache`中，后续调用直接执行机器码。

### 3. 优化前后性能对比
| 阶段         | 指令数 | 调用开销    | 内存访问     |
| ------------ | ------ | ----------- | ------------ |
| 原始字节码   | 15条   | 4次方法调用 | 多次栈操作   |
| 优化后机器码 | 5条    | 0次方法调用 | 仅寄存器操作 |

---

## 七、Graal的究极优势：为什么它能做到传统编译器做不到的事？
通过这个案例，我们可以总结Graal编译器的核心竞争力：

1.  **显式数据流依赖：为优化提供安全基础**
    理想图把隐式的栈依赖变成显式的节点+边，让Graal可以安全地做跨节点的优化（如CSE），而传统编译器只能在栈式指令上做简单的常量折叠。

2.  **方法内联+跨过程分析：突破调用边界**
    传统编译器的优化局限在单个方法内，而Graal可以通过内联，把多个方法的理想图合并，做跨过程的优化，消除方法调用的边界，让带方法调用的公共子表达式也能被消除。

3.  **无副作用分析：安全地做激进优化**
    Graal可以通过内联后的理想图，分析方法是否有副作用（如是否修改全局变量），从而安全地消除重复调用，而传统编译器默认认为方法调用有副作用，不敢优化。

4.  **模块化优化流水线：多轮递进式优化**
    Graal的优化是多轮递进的：内联为CSE创造条件，CSE为后续的常量传播、死代码消除创造条件，层层递进，把代码优化到极致。

