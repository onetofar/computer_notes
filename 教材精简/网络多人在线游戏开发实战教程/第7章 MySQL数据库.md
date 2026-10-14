---
title: 第7章 MySQL数据库（精读）
category: 多人网络
tags:
  - MySQL
  - 持久化
  - protobuf
  - 多人网络
difficulty: 进阶
source: 《网络多人在线游戏开发实战教程》（彭放）第7章 MySQL数据库
source_section: "精读 §7.1–§7.9(连接/写入/查询的 Connector/C 函数族 · 数据表与 protobuf 结构的映射;工程示例收骨架)"
skills:
  - obsidian-markdown
---

# 第7章 MySQL数据库

> **小节地图**:§7.1 Connector/C / §7.2–7.3 连接函数与连接组件 / §7.4–7.5 写入 / §7.6–7.7 查询 / §7.8 数据表创建与更新 / §7.9 表结构 ↔ protobuf——全章精读(逐函数细节收骨架)。

## 7.1–7.3 连接:从 C API 到连接组件

MySQL Connector/C 是**阻塞式 C API**:`mysql_init` → `mysql_options`(超时/重连)→ `mysql_real_connect`(主机/账号/库)→ 之后 `mysql_query/mysql_store_result` 全部同步等待。本章把它包成框架里的**数据库连接组件**(注册进 ECS,像网络对象一样被管理),而不是让业务代码直呼 C API。

**架构含义**:持久化 IO 也是 IO——它和 socket 一样是「结果未到」的异步源(§8.6 时间堆的 HttpRequest 卡死案例正是这条线上的)。把连接包成组件,才能给它挂超时、挂线程、挂监控。

## 7.4–7.7 写入与查询:两套函数族

| 操作 | 函数链 | 要点 |
|---|---|---|
| 写入 | `mysql_query(INSERT/UPDATE)` → `mysql_affected_rows` | 只返回影响行数;SQL 注入面在**拼接**处,参数化或转义 |
| 查询 | `mysql_query(SELECT)` → `mysql_store_result` → `mysql_fetch_row` 遍历 → `mysql_free_result` | `store` 把结果集**整体拉进内存**;大结果集要按字段类型(`mysql_fetch_field`)解析 |

工程示例(§7.5/§7.7)演示的是「游戏对象 ↔ 表行」的双向搬运;**真正有架构含量的是 §7.9**。

## 7.9 表结构 ↔ protobuf:一处定义两处用

数据表的字段与 protobuf 的消息结构**手工保持同步**是必错的双头账——本章的做法是**以 protobuf 定义为单一来源**:消息结构即表结构语义的载体,存库时序列化/反序列化经由同一份 `.proto`。

> 📖 [[第12章 玩家服务|格雷泽第12章 §12.5–12.8]]:平台把统计/成就的 schema 收在平台侧,游戏按 API 名称调用——**schema 单一来源**与彭放的「表结构跟着 proto 走」是同一纪律。X 宏([[第12章 玩家服务|第12章 §12.5]])是它的另一实现形态。

## 章末判据

| 决策点 | 判据 |
|---|---|
| 直连 C API 还是封装 | 封装成组件(ECS 实体)——持久化 IO 也需要超时/线程/监控 |
| 表结构与消息结构 | 单一来源(proto 为准);两处手改必漂移 |
| 结果集 | store 整体入内存的量级要控制;大结果按需分页 |

一句话结论:**MySQL 章在架构上的贡献只有一句——「数据库连接是另一种 IO 设备」,把它纳入与 socket 同一套「组件化+可观测+有超时」的治理,剩下的都是 API 手册。**
