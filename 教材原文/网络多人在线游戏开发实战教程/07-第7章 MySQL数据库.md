---
title: "网络多人在线游戏开发实战教程 · 第7章 MySQL数据库"
category: 教材原文
type: 原文章节稿
source_book: "网络多人在线游戏开发实战教程"
source_section: "第7章 MySQL数据库"
tags:
  - 教材原文
  - 彭放教程
  - C++服务器
source: "《网络多人在线游戏开发实战教程》（彭放，机械工业出版社 2019，ISBN 9787111667926）第7章 MySQL数据库 · md 稿提取（文字层 + 图表 OCR 回填），保留原文未净化"
created: 2026-10-07
skills:
  - obsidian-markdown
---

# 第7章 MySQL数据库

> **原文章节稿 · 未经净化。** [[网络多人在线游戏开发实战教程|← 返回本书目录]]
> 提取自 PDF（文字层 + 图表视觉 OCR 回填，138 张图表）。**保留原书全部技术细节与代码，未做删减或改写**。各章末尾的「图表转写」是该章配图的文字转录；原始裁图在 `_raw/B_img/`。

## 本章小节

- [[#7.1 MySQL Connector / C|7.1 MySQL Connector / C]]
- [[#7.2 连接时使用的函数说明|7.2 连接时使用的函数说明]]
- [[#7.3 数据库连接组件|7.3 数据库连接组件]]
- [[#7.4 写入数据时使用的函数说明|7.4 写入数据时使用的函数说明]]
- [[#7.5 写入数据示例|7.5 写入数据示例]]
- [[#7.6 查询数据时使用的函数说明|7.6 查询数据时使用的函数说明]]
- [[#7.7 查询数据示例|7.7 查询数据示例]]
- [[#7.8 数据表的创建与更新|7.8 数据表的创建与更新]]
- [[#7.9 数据表中的数据结构与protobuf结构|7.9 数据表中的数据结构与protobuf结构]]
- [[#7.1 0|7.1 0]]
- [[#7.1 0 . 1 为login进程与dbmgr进程创建连接类|7.1 0 . 1 为login进程与dbmgr进程创建连接类]]
- [[#7.1 0 . 2 找到指定的NetworkConnector实例|7.1 0 . 2 找到指定的NetworkConnector实例]]
- [[#7.1 0 . 3 创建角色|7.1 0 . 3 创建角色]]
- [[#7.1 0 . 4 机器人登录创建角色测试|7.1 0 . 4 机器人登录创建角色测试]]
- [[#7.1 1|7.1 1]]

> **尚未净化** —— 本章还没有对应的详解笔记。

---

存储是网络游戏开发中不可缺少的部分。在存储方面，不同的游戏公司采

用不同的数据库，有使用 MySQL 的，也有使用 MongoDB 的，本书中采用主流的

MySQL 数据库。本章主要介绍 C++与 MySQL 之间的交互，本章包括以下内容：

·对数据库进行读、写操作。

·对数据库进行更新。


## 7.1 MySQL Connector / C

调用 MySQL 时，使用的第三方库为 MySQL 的 Connector/C。本书没有使用 Connector/C++。Connector/C++与

Connector/C 的区别在于 C++版本采用了类的方式操作数据库，而 C 语言版本只提供 API，根据框架的自身特点，C 语言版

本可能更适合一些，而且它更方便、高效。

> **关键点 1：Linux系统配置**

在 Linux 下不需要进行任何操作，在安装数据库环境时已经在系统中安装了开发环境，只需要在使用时修改

CMakeLists.txt 文件即可。本节的源代码位于 07_01_db 目录，打开 dbmgr 工程中的 CMakeLists.txt，新增一行：

target_link_libraries(${MyProjectName} -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lssl -lcrypto

> **关键点 2：Windows系统配置**

在 Windows 系统中，使用该库的简单方式是直接在本机上安装 MySQL 5.7，因为本书在 Linux 系统中使用的是 5.X 版

本，所以在 Windows 系统中也使用 5.X 版本，以免接口不一致。安装 MySQL 之后，头文件和需要的动态库或静态库都有

了，可以直接使用 libmysql.lib 与 libmysql.dll 两个文件。如果不想在本地安装 MySQL，又想使用静态库，那么可以自

行编译。MySQL 自身使用的是 Connector/C，所以编译最新的 Connector/C 库可以从 MySQL 源代码中进行编译。

本书已经在 lib 目录中准备好了 Windows 版本的动态库，可以直接使用。使用时需要修改系统的环境变量 Path，指

向本书源代码根目录下的 lib\windows 目录，否则在运行 dbmgr 时会提示找不到 libmysql.dll。


## 7.2 连接时使用的函数说明

要使用 MySQL Connector/C，就需要对它的基础 API 有一个了解，本节先来了解需要用到的函数。


```cpp
int mysql_library_init(int argc, char **argv, char **groups)
void mysql_library_end(void)
```

函数 1 ：初始化库

函数 mysql_library_init 和 mysql_library_end 是一对用于初始化和释放的函数。

mysql_library_init 需要在调用其他函数之前调用，用来初始化 MySQL 库。而 mysql_library_end 调用可以释放内

存，在调用 mysql_close 之后调用，以帮助释放内存数据。


```cpp
MYSQL *mysql_init(MYSQL *mysql)
```

函数 2 ：创建一个数据库对象指针

该函数有两种用法：一种是传入 MySQL 对象;另一种是传入 nullptr 参数。两种调用都会返回 MySQL 对象指针，如果

有传入值，传入返回的对象就是同一个;如果传入的参数为 nullptr，就会返回一个实例。这两者的差别在于谁来管理

MySQL 对象的实例，如果是在外部创建的对象，在调用 mysql_close 之后就需要手动释放该对象，以免造成内存泄漏。

如果是由 mysql_init 创建的对象，就只需要关闭连接。


```cpp
void mysql_close(MYSQL *mysql)
```

函数 3 ：销毁现有对象指针

该函数用于关闭一个连接，如果 MySQL 实例是库生成的，该函数就会同时释放该对象。

函数 4 ：连接函数

MYSQL *mysql_real_connect(MYSQL *mysql, const char *host, const char *user, const char *passwd, const cha

在这个函数中，第 5 个参数为想要连接的数据库的名字，可以为 nullptr，表示只是想产生一个连接，但不选择具

体数据库，后续调用其他函数选定具体的数据库。

当我们连接一个给定名字的数据库时，如果这个数据库不存在，就会出现错误，而 MySQL 对象也不可再用，需要关

闭。鉴于这种情况，在第一次连接时就可以将数据库名设为 nullptr，让它进行一个默认的连接，再调用

mysql_select_db 函数进行数据库的选择，若选择失败，则说明所需的数据库不存在，这时可以创建需要的数据库。

在连接函数中，最后一个参数 client_flag 是一系列的常量，在本书的示例程序中使用了 CLIENT_FOUND_ROWS，官


```cpp
CLIENT_FOUND_ROWS: Return the number of found (matched) rows, not the number of changed rows
```

方的解释如下：

即返回匹配的行数，而不是修改行数。

若函数 mysql_real_connect 的返回值是 nullptr，则连接失败;若成功，则返回值为 MySQL 的对象指针。


```cpp
int mysql_options(MYSQL *mysql, enum mysql_option option, const void *arg)
```

函数 5 ：设置属性

该函数是在 mysql_init 之后、mysql_real_connect 之前调用的，对 MySQL 对象进行一些属性设置。在本书的框架中

用到了两个常用的属性：

·MYSQL_OPT_CONNECT_TIMEOUT：设置连接超时。

·MYSQL_OPT_RECONNECT：是否自动连接。

更多的枚举可以在头文件 mysql.h 中查看。


```cpp
int mysql_select_db(MYSQL *mysql, const char *db)
```

函数 6 ：选择数据库

该函数输入一个需要连接的数据库名。返回值 0 表示成功，非 0 即为出错编号。该函数指定一个数据库作为当前选

中的数据库。在调用该函数时，如果用户没有指定数据库的权限就会出错。


```cpp
unsigned int mysql_errno(MYSQL *mysql)
```

函数 7 ：错误代码

在每一个函数调用之后，如果出现错误，就可以通过调用函数 mysql_errno 得到当前错误的编号。


```cpp
int mysql_ping(MYSQL *mysql)
```

函数 8 ： ping 函数

该函数检查连接是否处于正常工作中。

在函数 mysql_options 的设置中，可以打开自动连接的开关，当网络断开时，调用 mysql_ping 会自动重新连接。


## 7.3 数据库连接组件

介绍完了主要函数，接下来看看框架是如何应用这些 API 函数来连接 MySQL

的。用一个实际例子说明，本节源代码位于 07_01_db 目录中。在源代码中新增

一个 dbmgr 工程，用来处理与数据库相关的操作。


### 7.3.1 MysqlConnector组件

在 dbmgr 工程中新增一个 MysqlConnector 类，提供一些数据库操作，其定义可以在 mysql_connector.h 文件中找

到。


```cpp
class MysqlBase {
public:
    bool ConnectInit();
    virtual void Disconnect();
    ...
protected:
    DBConfig _dbCfg;
    MYSQL* _pMysql{ nullptr };
    MYSQL_RES* _pMysqlRes{ nullptr };
    ...
};
```

class MysqlConnector : public MysqlBase, public Entity<MysqlConnector>, public IMessageSystem, public IAw

MysqlConnector 类基于 MysqlBase 类，该类中实现了 IMessageSystem 接口，处理诸如查询角色或创建角色这样的协

议。MysqlBase 类则提供了连接 MySQL 数据库以及读取与查询的一些基本功能。下面详细分析一下代码。


### 7.3.2 连接数据库

对于连接数据库来说，首先需要连接给定名字的数据库，这部分功能是在初始化时


```cpp
void MysqlConnector::AwakeFromPool() {
    auto pYaml = Yaml::GetInstance();
    auto pConfig = pYaml->GetConfig(APP_DB_MGR);
    auto pDbCfig = dynamic_cast<DBMgrConfig*>(pConfig);
    _pDbCfg = pDbCfig->GetDBConfig(DBMgrConfig::DBTypeMysql);
    if (_pDbCfg == nullptr) {
        LOG_ERROR("Failed to get mysql config.");
        return;
    }
    Connect();
}
```

完成的：

当 MysqlConnector 类从对象池中唤醒时，它会首先执行 AwakeFromPool 函数，在执

行该函数时，从配置数据中读出数据库的配置信息，配置数据位于 engine.yaml 文件

中。在 dbmgr 的配置中有两组数据：一组是关于 Redis 的;另一组是关于 MySQL 的。这些

配置包括数据库 IP、端口、数据库名、字符集等，在本例中，取出 MySQL 相关的配置并

保存在 MysqlConnector 类中。初始化函数最后调用了函数 Connect 进行连接，调用了基

类的连接函数 MysqlBase::ConnectInit。该函数进行了一些连接前的基本操作，初始

化操作成功之后，调用 mysql_real_connect 函数发起一个连接。如果经检查没有错误，

就认为连接成功。


```cpp
bool MysqlBase::ConnectInit() {
```

在基类中初始化 MysqlBase::ConnectInit 函数的实现代码如下：


```cpp
    _pMysql = mysql_init(nullptr);             // 由库创建出 MySQL 对象
    if (_pMysql == nullptr) {
        CheckMysqlError();
        LOG_ERROR("mysql_init == nullptr");
        return false;
    }
    // 设置连接等待时间，这里设置的是 10 秒
    int outtime = 10;
    mysql_options(_pMysql, MYSQL_OPT_CONNECT_TIMEOUT, &outtime);
    bool reConnect = false;                        // 不自动重连
    mysql_options(_pMysql, MYSQL_OPT_RECONNECT, &reConnect);
    return true;
}
```

Disconnect();                                   // 如果已有一个连接，就先销毁

在基类函数中，创建了 MySQL 指针并对它进行了属性的设置，在此设置了两个属

性：一个是连接时的超时设定;另一个用于设定是否自动重连。本例中采用了不自动重

连，手动定时查看的方式。


### 7.3.3 关闭连接

如果在连接过程中出现异常，就可以调用 Disconnect 函数关闭已生成的连


```cpp
void MysqlConnector::Disconnect() {
    CleanStmts();
    MysqlBase::Disconnect();
}
void MysqlBase::Disconnect() {
    ...
    if (_pMysql != nullptr) {
        mysql_close(_pMysql);
        _pMysql = nullptr;
    }
}
```

接，以便重新连接。关闭连接函数的代码如下：

在关闭连接时调用了库函数 mysql_close 以释放内存。虽然在游戏框架上

使用了多线程，但是对于一个 MysqlConnector 对象而言，它遵照 Actor 原则，

代码没有耦合性，因而线程是安全的。如果需要，那么可以启动多个线程来执

行数据库操作，每一个线程都生成一个独立的 MysqlConnector 实例，每一个线

程都相当于一个 MySQL 客户端，互不影响。


## 7.4 写入数据时使用的函数说明

厘清了数据库的连接与关闭操作之后，再来说说对数据库的插入和更改操作。MySQL 库同样

提供了一系列函数供我们使用，首先要了解“预处理”这个概念。

什么是预处理呢？

对于需要多次执行的语句，预处理是一种非常高效的方式，其原理是一次生成语句，每次

执行时传入参数，以减少数据的传递。一次生成语句的好处是不用每次对 SQL 语句进行解析，极

大地提高了效率。预处理就像一个函数，每次执行时只需要填入不同的参数，就能得到不同的

结果。几个重要的预处理函数如下：


```cpp
MYSQL_STMT *mysql_stmt_init(MYSQL *mysql)
```

函数 1 ：创建一个预处理

MySQL 库的预处理使用一个名为 MYSQL_STMT 的结构，调用该函数即可创建一个 MYSQL_STMT 指

针，返回值不是 nullptr 时则为成功。


```cpp
my_bool mysql_stmt_close(MYSQL_STMT *stmt)
```

函数 2 ：销毁一个预处理

该函数销毁传入的 MYSQL_STMT 指针。


```cpp
int mysql_stmt_prepare(MYSQL_STMT *stmt, const char *stmt_str, unsigned long length)
```

函数 3 ：初始化预处理

该函数将一个 SQL 语句写入预处理结构中，第二个参数即为 SQL 语句，该语句不需要有结束

符分号，即不需要符号“;”。传入的 SQL 语句在参数的位置用“？”代替。

调用该函数，返回值为非 0 时，表示有错误，可以用 mysql_stmt_error 函数查看错误编码。

函数 4 ：出错检查


```cpp
const char *mysql_stmt_error(MYSQL_STMT *stmt)
```

一旦发现某个预处理函数有异常或出错，就可以通过调用该函数来获取错误描述。


```cpp
my_bool mysql_stmt_bind_param(MYSQL_STMT *stmt, MYSQL_BIND *bind)
```

函数 5 ：绑定参数

前面传入的 SQL 语句中，关于动态参数的部分是用“？”来代替的。而函数

mysql_stmt_bind_param 是专门为这些“？”准备的，利用 MYSQL_BIND 结构提供参数。像函数一

样，一个预处理在实际执行阶段需要绑定实际的参数。


```cpp
int mysql_stmt_execute(MYSQL_STMT *stmt)
```

函数 6 ：执行

绑定完参数之后的预处理指针就可以调用执行函数来执行。如果返回结果不为 0，就表示有

错误。


```cpp
my_ulonglong mysql_stmt_affected_rows(MYSQL_STMT *stmt)
```

函数 7 ：获取执行结果个数

预处理执行之后，我们可以通过该函数来获取结果行的个数，即当前执行的预处理结果中

有多少行数据。


## 7.5 写入数据示例

一个完整的插入数据的步骤是这样的：首先根据 SQL 语句创建预处理，在

需要使用时传入参数，然后调用执行函数得到结果。

以创建角色为例，先创建“创建角色的预处理”，并将这个预处理实例存

入一个字典中，便于每次使用时快速调用。当创建角色的协议发送到 dbmgr 进

程时，调用预处理实例并传入参数，这些参数是玩家名字和玩家的初始数据。

最后，调用执行函数写入数据库中。下面用一个实际例子加以说明，本节的源

代码位于 07_01_db 目录中。


### 7.5.1 创建预处理


```cpp
void MysqlConnector::InitStmts() {
    DatabaseStmt* stmt = CreateStmt("insert into player ( sn, account, name, savetime, createtime ) value
    _mapStmt.insert(std::make_pair(DatabaseStmtKey::StmtCreate, stmt));
    stmt = CreateStmt("update player set base=?, misc=?,savetime=now() where sn = ?");
    _mapStmt.insert(std::make_pair(DatabaseStmtKey::StmtSave, stmt));
}
```

在 MysqlConnector 类创建之初就初始化两个预处理：一个是插入玩家数据的预处理;另一个用于更新玩家数据。

代码中为两个预处理绑定了两个枚举：DatabaseStmtKey::StmtCreate 用于创建角色;DatabaseStmtKey::

StmtSave 用于更新角色数据。同时，将生成的对象存入字典中，以便使用时方便索引。在上面的代码中，预处理被写


```cpp
struct DatabaseStmt {
    MYSQL_STMT *stmt{ nullptr };
    MYSQL_BIND *bind{ nullptr };
    char *bind_buffer{ nullptr };
    int bind_index;
    int bind_buffer_index;
};
```

入了一个封装好 DatabaseStmt 的结构中。DatabaseStmt 的原型如下：

在这个结构体中存储了预处理结构 MYSQL_STMT 的指针以及一个 MYSQL_BIND 的指针，该指针是为了绑定参数而服务

的，是一个数组。表 7-1 列举了 DatabaseStmt 结构的每一个值的定义。

表 7-1 DatabaseStmt 结构表


```cpp
DatabaseStmt* MysqlConnector::CreateStmt(const char* sql) const {
    int str_len = strlen(sql);
    DatabaseStmt* stmt = new DatabaseStmt();
    int param_count = 0;
    stmt->stmt = mysql_stmt_init(_pMysql);  // 从 MySQL 库中创建 MYSQL_STMT 指针
    if (mysql_stmt_prepare(stmt->stmt, sql, str_len) != 0) {
        return nullptr;  // 失败
    }
    for (int i = 0; i < str_len; i++) {
        if ((sql[i] == '?') || (sql[i] == '@'))
```

生成 DatabaseStmt 结构的代码如下：


```cpp
    }
    if (param_count > 0) {
        stmt->bind = new MYSQL_BIND[param_count];// 为每个参数创建 MYSQL_BIND 空间
        memset(stmt->bind, 0, sizeof(MYSQL_BIND) * param_count);
        stmt->bind_buffer = new char[MAX_BIND_BUFFER]; // 数据备用空间
    } else {
        stmt->bind = nullptr;
```

param_count++; // 统计参数个数


```cpp
        stmt->bind_buffer = nullptr;
    }
    return stmt;
}
```

从上面的代码中不难看出，有多少个参数就要生成多少个 MYSQL_BIND 数据结构。图 7-1 展示了结构的内存数据。例

如 INSERT INTO tabel1 VALUES（？，？，？），参数个数为 3，所以这里 DatabaseStmt 中的属性 bind 是一个

MYSQL_BIND[3]的数组指针。在生成 DatabaseStmt 结构时，在结构中生成了一块用于缓存数据的 buffer，用于存放参

数。

**图 7-1 DatabaseStmt内存结构**

下面用一个实际例子来说明结构中这些数据的用途。


### 7.5.2 用预处理创建角色

创建角色时，创建角色的协议被 MysqlConnector 类捕捉到，它的处理函数为


```cpp
void MysqlConnector::HandleCreatePlayer(Packet* pPacket) {
    auto protoCreate = pPacket->ParseToProto<Proto::CreatePlayerToDB>();
    auto protoPlayer = protoCreate.player();
    DatabaseStmt* stmt = GetStmt(DatabaseStmtKey::StmtCreate);
    if (stmt == nullptr)
        return;
    ...
    // create
    ClearStmtParam(stmt);
    AddParamUint64(stmt, protoPlayer.sn());
    AddParamStr(stmt, protoCreate.account().c_str());
    AddParamStr(stmt, protoPlayer.name().c_str());
    ...
    if (ExecuteStmt(stmt)) {
        protoRs.set_return_code(Proto::CreatePlayerReturnCode::CPR_Create_OK);
    }
    ...
}
```

HandleCreatePlayer，其实现如下：

收到创建协议之后，从预处理字典中取出了已经准备好的 DatabaseStmt。随后调用了

ClearStmtParam、AddParamUint64 和 AddParamStr 三个函数，分别是为了清理旧数据、压入一个 uint64


```cpp
void MysqlConnector::ClearStmtParam(DatabaseStmt* stmt) {
    stmt->bind_index = 0;
    stmt->bind_buffer_index = 0;
}
```

参数和压入一个字符串到预处理结构中。下面来分别看一看这几个参数的实现。

函数 ClearStmtParam 用于清除上一次的数据，让缓存回到最初状态。

创建角色的 SQL 语句“insert into player（sn，account，name，savetime，createtime）

value（？，？，？，now（），now（））”有 3 个参数，分别是 uint64 和两个 string。下面重点来看


```cpp
void MysqlConnector::AddParamUint64(DatabaseStmt* stmt, uint64 val) {
    MYSQL_BIND* pBind = &stmt->bind[stmt->bind_index];
    pBind->buffer_type = MYSQL_TYPE_LONGLONG;
    pBind->buffer = &stmt->bind_buffer[stmt->bind_buffer_index];
    pBind->is_unsigned = true;
    *static_cast<uint64*>(pBind->buffer) = val;
    stmt->bind_index++;
    stmt->bind_buffer_index += sizeof(uint64);
}
```

AddParamUint64 和 AddParamStr 函数。

函数 AddParamUint64 将一个 uint64 的值绑定到 MYSQL_BIND 结构中。首先需要取出对应这个 uint64


```cpp
MYSQL_BIND *mbind = &stmt->bind[stmt->bind_index];
```

属性的 MYSQL_BIND，即如下一句代码：

这里有一个潜规则，默认在设置参数时是按 SQL 语句的“？”顺序设置的，即（sn，account，

name），顺序为 uint64、string、string，如果按 string、string、uint64 就会出错。


```cpp
typedef struct st_mysql_bind {
```

结构 MYSQL_BIND 非常复杂，简单列举一下需要用到的几个重要属性：

unsigned long *length;                      /* 指向输出长度的指针  */


```protobuf
  enum enum_field_types buffer_type;     /* buffer type */
  my_bool is_unsigned;                         /* 设置，如果整数类型是无符号整数  */
```

void *buffer;                                 /* 读写数据的缓冲区  */


```cpp
} MYSQL_BIND;
```

...

下面就参数逐一说明。

• （1）buffer 为数据缓冲区。输入时，参数数据值绑定到缓冲区 buffer 中，供 mysql_stmt_execute

使用;输出时，将结果绑定到缓冲区 buffer 中，供 mysql_stmt_fetch 使用。对于日期和时间数据类

型，缓冲区应指向 MYSQL_TIME 结构。

• （2）buffer_type 为数据类型，常用的有 MYSQL_TYPE_LONG、MYSQL_TYPE_BLOB 等。

• （3）is_unsigned 表示数据是否是无符号类型。

• （4）*length 属性指向一个 unsigned long*无符号长整型指针。该属性只用在字符或二进制数据

上。输入时，设置*length 为 buffer 中的参数值的实际长度，供 mysql_stmt_execute 使用。数值类数据

如 long 或 long long，*length 没有用，因为 buffer_type 值决定了数值的长度。

得到 MYSQL_BIND 指针之后，需要对其进行初始化。每一个 MYSQL_BIND 实例都需要外部提供一块内

存给它存储数据。而我们创建的 DatabaseStmt 结构正好提供了一块内存用来存储这些数据。以下语句


```cpp
pBind->buffer_type = MYSQL_TYPE_LONGLONG;
pBind->buffer = &stmt->bind_buffer[stmt->bind_buffer_index];
pBind->is_unsigned = true;
*static_cast<uint64*>( pBind->buffer ) = val;
```

是基于 uint64 数据进行的一个赋值。

首先设置这个 MYSQL_BIND 的数据类型为 MYSQL_TYPE_LONGLONG，再设置缓存区，自定义结构中的

buffer 提供了一大块连续的数据块，这里派上了用场，在第二行代码中，将数据地址赋值给缓存。是


```cpp
stmt->bind_index++;
stmt->bind_buffer_index += sizeof( uint64 );
```

无符号类型，第三行代码属性 is_unsigned 设为 true。最后的语句将缓存的可写位移进行偏移。

偏移的值为写入数值的内存长度。这里的 uint64 即为 sizeof（uint64）。以上是给一个数值赋

值，AddParamStr 函数用于给字符串赋值，在 uint64 的例子中，数值的长度是一定的，而字符串的长度


```cpp
void MysqlConnector::AddParamStr(DatabaseStmt* stmt, const char* val) {
    MYSQL_BIND* pBind = &stmt->bind[stmt->bind_index];
    int len = strlen(val);
    pBind->buffer_type = MYSQL_TYPE_STRING;
    pBind->buffer = &stmt->bind_buffer[stmt->bind_buffer_index];
    pBind->length = (unsigned long*)& stmt->bind_buffer[stmt->bind_buffer_index + len + 1];
    engine_strncpy((char*)pBind->buffer, len + 1, val, len + 1);
    *(pBind->length) = len;
    pBind->buffer_length = len;
    stmt->bind_index++;
    stmt->bind_buffer_index += (len + 1 + sizeof(unsigned long*));
}
```

却不一定，AddParamStr 的实现如下：

同样，先得到 MYSQL_BIND 指针，设置 buffer_type 为 MYSQL_TYPE_STRING，参数是字符串时，

MYSQL_BIND 的 buffer 属性就有了意义，它会指向字符串的地址，这里将 DatabaseStmt 结构中 buffer 的

当前指针分配给它，并调用 strncpy 函数将传入的串复制到这块内存块中，同时 MYSQL_BIND 的 length 需

要指定一个 unsigned long*指针，这个指针指向的地址赋值为输入字符串的长度。

值得注意的是，在 MYSQL_BIND 结构中，buffer 和 length 都是指针，指针指向的是一个外部地址，

这样的做法有利于精减内存，没必要在 MySQL 库内部再为这些参数开辟内存块来存储数据。注意最后一


```cpp
stmt->bind_buffer_index += ( len + 1 + sizeof( unsigned long* ) );
```

行代码：

DatabaseStmt 中已经用了缓存，不能再使用，需要设置一个偏移值。

**图 7-2 展示了DatabaseStmt的真实内存结构。从内存结构中可以清楚地看到所有参数的值都被保存**

在了缓存中，数值型类型因为长度是固定的，不需要开辟空间保存长度。而字符型参数除了要保存内

容之外，还要有一个空间来保存它的长度。

在图 7-2 的内存结构中，第一个参数为 uint64，第二个参数为 string。DatabaseStmt 结构中

MYSQL_BIND 数组的长度为 2，保存了两个 MYSQL_BIND 数据。第一个 MYSQL_BIND 结构中的 buffer 指向了

uint64 的地址，取数据时，直接从这块缓冲中取出对应类型的大小即可。第二个参数是 string 类型

的，MYSQL_BIND 结构中的 buffer 指向了缓冲区中的某个地址，从这个地址上可以读出一个字符串，而

字符串的长度则存储在 MYSQL_BIND 结构中的 length 中，length 也是一个指针，该指针指向了一个

unsigned long 类型的指针，在这个指针上存储着长度。该指针的真实地址值也在 buffer 缓冲中。

**图 7-2 DatabaseStmt内存结构实例**

因为采用了缓存的方式，所以看上去比较复杂。在这一小节中，我们直接操纵了内存块，这是

C++与其他语言的一个区别。从这个例子中，我们能更好地理解内存和类型的关系。写数据时提供的缓

存类型是 char*，但在这个缓存中放置了 unit64、string 和 long。C++的这种特性虽然好用，但使用时

需要特别注意，以免产生内存越界。


## 7.6 查询数据时使用的函数说明

前面介绍了写入数据操作，本节介绍查询数据的方法。其步骤为：使用 SQL 语句进行

查询时，不同的 SQL 语句获取的数据不同，但是都可以得到一个 MYSQL_RES 集合，遍历集

合，最后将行数据转换成自定义的内存数据。其中涉及的 API 函数如下：


```cpp
int mysql_query(MYSQL *mysql, const char *stmt_str)
int mysql_real_query(MYSQL *mysql, const char *stmt_str, unsigned long length)
```

函数 1 ：查询函数

库中提供了两个查询函数。函数 mysql_query 执行指定的 SQL 语句，参数 stmt_str 可

以不带 SQL 语句的结束符“;”，但必须是有结束符的字符串，即最后以'\0'字符结尾。

mysql_query 函数使用的 SQL 语句不能带二进制数据，如果需要带二进制数据，就需要使

用函数 mysql_real_query。从函数定义上能看得出来，mysql_real_query 函数执行 SQL 语

句的时候，使用的参数是 char*和它的长度。这个 char*的字符串是允许存在'\0'这种结

束符的。这就是这两个函数本质上的区别。

如果要查看执行该语句后的结果，那么可以在 mysql_query 调用之后调用函数

mysql_field_count 查看有多少列数据。如果执行的语句不是一个 select，那么

mysql_field_count 调用的结果可能为 0。另外，执行操作也可以由 mysql_query 来完成，

即传入的 SQL 语句可以不是一个 select 语句。


```cpp
MYSQL_RES *mysql_store_result(MYSQL *mysql)
```

函数 2 ：读取结果

调用 mysql_query 函数之后可以用 mysql_store_result 得到结果，该函数将全部结果

缓存到 MYSQL_RES 结构中并返回，MYSQL_RES 用完之后需要使用 mysql_free_result 释放数

据。

函数 mysql_store_result 返回为空时，不意味着失败。如果执行语句是 insert 语

句，mysql_store_result 就会返回空，因为 insert 语句并没有集合可以返回。

函数 3 ：获取结果中有多少列


```cpp
unsigned int mysql_num_fields(MYSQL_RES *result)
```

调用函数 mysql_store_result 的结果不为空时，可以调用 mysql_num_fields 来判断

有多少列。


```cpp
MYSQL_FIELD *mysql_fetch_field(MYSQL_RES *result)
```

函数 4 ：读取字段

该函数的使用相当于一个迭代器，对 MYSQL_RES 的列数据进行一个迭代，当返回值为

空时表示没有更多的列了。


```cpp
MYSQL_ROW mysql_fetch_row(MYSQL_RES *result)
```

函数 5 ：获取行

同理，函数 mysql_fetch_row 也是一个迭代器，迭代的是 MYSQL_RES 集合，也就是

mysql_store_result 得到的集合。


## 7.7 查询数据示例

一个完整的查询数据步骤为：首先组织一条 SQL 语句，调用 mysql_query 得

到一个结果集，再通过调用 mysql_fetch_row 函数得到 MYSQL_ROW 行数据。而每

一行的具体数据则是由 MYSQL_ROW 类的操作函数读取数据的。下面我们用一个

例子加以说明，本节的源代码位于 07_01_db 目录中。


### 7.7.1 Query查询函数


```cpp
bool MysqlBase::Query(const char* sql, my_ulonglong& affected_rows) {
    if (nullptr != _pMysqlRes) {
        mysql_free_result(_pMysqlRes);
        _pMysqlRes = nullptr;
    }
    if (mysql_query(_pMysql, sql) != 0) {
        LOG_ERROR("Query error:" << mysql_error(_pMysql) << " sql:" << sql);
        return false;
    }
    // maybe query is not a select
    _pMysqlRes = mysql_store_result(_pMysql);
    if (_pMysqlRes != nullptr) {
        _numFields = mysql_num_fields(_pMysqlRes);
        _pMysqlFields = mysql_fetch_fields(_pMysqlRes);
    }
    affected_rows = mysql_affected_rows(_pMysql);
    return true;
}
```

在 MysqlBase 类中，函数 Query 封装了 mysql_query 函数。其实现如下：

上面的代码使用了 MySQL 库提供的函数 mysql_query，得到了一个 MYSQL_RES 集合，

将这个集合的指针保存在类变量 pMysqlRes 中，该属性是结构 MYSQL_RES 的指针。同

时，将列数据保存在变量 pMysqlFields 中，该属性是结构 MYSQL_FIELD 的指针，可以称

它为字段数据。

这里一下解释为什么会有字段数据，以及它的作用是什么。

使用 select 语句时，例如“select sn，name from player”和“select name，

sn from player”的字段数据是不一样的。调用函数 mysql_fetch_fields 得到返回集

合的字段信息，对于第一条语句来说，sn 在前，name 在后。第二条语句正好相反。

MYSQL_FIELD 主要是对字段描述的一个结构，对于 sn 来说，描述它的结构为

MYSQL_FIELD，name 也会生成一个 MYSQL_FIELD。结构 MYSQL_FIELD 其中的一个字段

enum_field_types type 表示当前这个字段的真实类型。那么，对于上面两条 select 语

句，第一条语句的第一个字段类型和第二条语句的第一个字段类型显然是不相同的。


### 7.7.2 查询玩家数据

以查询玩家数据这一流程来具体说明查询函数的使用方法。首先，当收到查询玩家协议 L2DB_QueryPlayerList


```cpp
void MysqlConnector::HandleQueryPlayerList(Packet* pPacket) {
    auto protoQuery = pPacket->ParseToProto<Proto::QueryPlayerList>();
    QueryPlayerList(protoQuery.account(), pPacket->GetSocket());
}
void MysqlConnector::QueryPlayerList(std::string account, SOCKET socket) {
    my_ulonglong affected_rows;
    std::string sql = strutil::format("select sn, name, base, item, misc from player where account = '%s'
    if (!Query(sql.c_str(), affected_rows)) {
        LOG_ERROR("!!! Failed. MysqlConnector::HandleQueryPlayerList. sql:" << sql.c_str());
        return;
    }
    Proto::PlayerList protoRs;
    protoRs.set_account(account.c_str());
    Proto::PlayerBase protoBase;
    if (affected_rows > 0) {
        std::string tempStr;
        MYSQL_ROW row;
        while ((row = Fetch())) {
            auto pProtoPlayer = protoRs.add_player();
            pProtoPlayer->set_sn(GetUint64(row, 0));
            pProtoPlayer->set_name(GetString(row, 1));
            GetBlob(row, 2, tempStr);
            protoBase.ParseFromString(tempStr);
            pProtoPlayer->set_level(protoBase.level());
            pProtoPlayer->set_gender(protoBase.gender());
        }
    }
    // 没有找到也需要返回 pResultPacket
    SendPacket(Proto::MsgId::L2DB_QueryPlayerListRs, socket, protoRs);
}
```

时，MysqlConnector 类的处理函数 HandleQueryPlayerList 的实现如下：

在查询玩家数据时，直接使用了 SQL 语句“select sn，name，base，item，misc from player where


```cpp
MYSQL_ROW MysqlBase::Fetch() const {
    if (_pMysqlRes == nullptr)
        return nullptr;
    return mysql_fetch_row(_pMysqlRes);
}
```

account='%s'”，接着调用了之前封装好的函数 Query，当返回数据时，调用了 Fetch 函数，其定义如下：

从上面的代码可以看出，调用 mysql_fetch_row 函数从集合中取出了当前行的数据。while（（row=Fetch（）））

不断取出当前行的数据，直到这个集合被遍历完为止。对于行数据，首先调用 GetUint64 函数取出当前行中的 sn 数据，


```cpp
uint64 MysqlBase::GetUint64(MYSQL_ROW row, int index) {
    if (row[index] == nullptr) {
        LOG_ERROR("!!! Failed. MysqlConnector::GetUint64");
        return 0;
    }
    return atoll(row[index]);
}
```

调用 GetString 函数取出当前行中的 name 数据，后面以此类推。函数 GetUint64 的实现方法如下：


```cpp
typedef char **MYSQL_ROW; /* return data as array of strings */
```

看看 MYSQL_ROW 的定义：

它指向了一个数组地址，可以根据不同的类型直接转换成不同的数据。在这些类型中，有一个特别需要注意的是


```cpp
void MysqlBase::GetBlob(MYSQL_ROW row, int index, std::string& protoStr) const {
    unsigned long* pLengths = mysql_fetch_lengths(_pMysqlRes);
```

二进制 Blob 数据，取得 Blob 的函数实现如下：


```cpp
    long blobLength = pLengths[index];
    if (blobLength <= 0) {
        protoStr = "";
        return;
    }
    char* blobByte = new char[blobLength + 1];
    GetBlob(row, index, blobByte, blobLength);
    blobByte[blobLength] = '\0';
    protoStr = blobByte;
    delete[] blobByte;
}
int MysqlBase::GetBlob(MYSQL_ROW row, int index, char* buf, unsigned long size) const {
    unsigned int l = size > 0 ? size : 0;
    if (row[index] == nullptr) {
        LOG_ERROR("!!! Failed. MysqlConnector::GetBlob");
        return 0;
    }
    unsigned long* lengths = mysql_fetch_lengths(_pMysqlRes);
    if (lengths[index] < l)
        l = lengths[index];
    memcpy(buf, row[index], l);
    return l;
}
```

在获取 Blob 数据时，首先需要知道 Blob 的长度，调用 mysql_fetch_lengths 函数从数据集合中得到一个长度数组。

根据当前字段所在的 index 下标，在长度数组中找到这个字段的长度。为了读取二进制数据，需要创建一块内存，调用

memcpy 函数完全复制数据。取得数据之后，再传递到上层调用中。


## 7.8 数据表的创建与更新

前面几节详细地介绍了 MySQL 的一些 API 的使用，本节讨论一下数据库的创建与更新问题。一般来说，服务端每一

个版本的代码都对应一个相应的 SQL 文件。导入 SQL 文件到数据库之后，服务器才可以正常地使用数据库。但这项功能

要求在编译源代码的同时维护一系列 SQL 文件。长期来讲，这是一件非常令人恼火的事情，代码版本与 SQL 版本不一致

时就会出错。

鉴于此，我们将数据库的结构写入 C++的源代码中，并随着版本的推进进行自动更新。这样就可以把目光专注在源

代码上，不需要维护 SQL 文件了。图 7-3 展示了创建与更新数据库的流程。

**图 7-3 创建与更新数据库**

概述一下这个流程，首先连接数据库，查看数据库是否连接成功，如果失败，就查询出错的编号;如果编号为


```cpp
Error: 1049 SQLSTATE: 42000 (ER_BAD_DB_ERROR)
```

1049，就表示数据库不存在。官方对于 1049 的解释如下：

Message: Unknown database '%s'

如果数据库不存在，此时就需要创建一个数据库，并进行数据库的所有表格的创建，同时创建一个名为 Version 的

表，用来记录当前版本号，初始的版本号为 0，根据当前代码的版本号进行升级。

如果数据库连接成功，就查询数据库表 version 中的版本号，以对应当前代码的版本号进行升级。更新完成之后有

一个重要的操作，即将数据库中 version 表中的版本号定为当前代码的版本号。

下面是更新数据库的类 MysqlTableUpdate，在 07_01_db 目录的 dgmgr 工程的 mysql_table_update.cpp 文件中。其类


```cpp
class MysqlTableUpdate :public MysqlBase, public Singleton<MysqlTableUpdate> {
public:
    void Check();
private:
    bool CreateDatabaseIfNotExist();
    bool UpdateToVersion();            // 检查 DB 数据，更新到最新版本
    bool Update00();                        // 00 版本的数据修改
private:
    typedef std::function<bool(void)> OnUpdate;
    std::vector<OnUpdate> _update_func;
    int const _version = 0;
};
```

定义如下：

在 MysqlTableUpdate 类中存储了一个当前数据库代码的版本号，即 version，每一次变更数据库之后，这个变量就

要加 1。现在它是第 0 个版本号。


```cpp
int main(int argc, char* argv[]) {
    mysql_library_init(0, nullptr, nullptr);
    ...
    auto pTableUpdateObj = MysqlTableUpdate::Instance();
    pTableUpdateObj->Check();
    pTableUpdateObj->DestroyInstance();
    ...
    return 0;
}
```

MysqlTableUpdate 类是在 main.cpp 中被创建的。也就是说在 dbmgr 启动之初就对数据库进行了更新检查。

在启动线程之前，调用 MysqlTableUpdate::Check 检查当前数据库版本是否为最新版本。检查数据库函数代码如


```cpp
void MysqlTableUpdate::Check() {
    if (!ConnectInit())
        return;
    auto pYaml = Yaml::GetInstance();
    auto pDbMgrCfig = dynamic_cast<DBMgrConfig*>(pYaml->GetConfig (APP_DB_MGR));
    _pDbCfg = pDbMgrCfig->GetDBConfig(DBMgrConfig::DBTypeMysql);
    if (_pDbCfg == nullptr) {
        LOG_ERROR("Init failed. get mysql config is failed.");
        return;
    }
    if (mysql_real_connect(_pMysql, _pDbCfg->Ip.c_str(), _pDbCfg->User.c_str(), _pDbCfg->Password.c_str()
        mysql_select_db(_pMysql, _pDbCfg->DatabaseName.c_str());
    }
    int mysqlerrno = CheckMysqlError();
    if (mysqlerrno == ER_BAD_DB_ERROR) {
        // 1049: Unknown database 没有找到数据库，就新建一个
        if (!CreateDatabaseIfNotExist()) {
            Disconnect();
            return;
        }
        // 创建成功之后，选择到当前数据库
        mysql_select_db(_pMysql, _pDbCfg->DatabaseName.c_str());
        mysqlerrno = CheckMysqlError();
    }
    if (mysqlerrno > 0) {
        Disconnect();
        return;
    }
    // 检查版本，自动更新
    if (!UpdateToVersion()) {
        LOG_ERROR("!!!Failed. Mysql update. UpdateToVersion");
        return;
    }
```

下：


```cpp
}
```

...

下面我们详细分析这一段代码。


### 7.8.1 创建表

在检查数据库时，首先创建一个 MySQL 连接，此时并没有选择数据库，函数 mysql_real_connect 中关于数据库名的

参数值为 nullptr。连接成功之后，调用 mysql_select_db 函数试探数据库是否存在，如果返回错误码为


```cpp
bool MysqlTableUpdate::CreateDatabaseIfNotExist() {
    // 是否存在数据库，如果不存在就创建
    std::string querycmd = strutil::format("CREATE DATABASE IF NOT EXISTS %s;", _pDbCfg->DatabaseName.c_s
    my_ulonglong affected_rows;
    if (!Query(querycmd.c_str(), affected_rows)) {
        LOG_ERROR("!!! Failed. MysqlConnector::CreateDatabaseIfNotExist. cmd:" << querycmd.c_str());
        return false;
    }
    // 连接 DB 之后，选择指定的数据库
    if (mysql_select_db(_pMysql, _pDbCfg->DatabaseName.c_str()) != 0) {
        LOG_ERROR("!!! Failed. MysqlConnector::CreateDatabaseIfNotExist: mysql_select_db:" << LOG4CPLUS_S
        return false;
    }
    // 设置数据库的字符集，从 YAML 配置中读取需要的字符集
    if (mysql_set_character_set(_pMysql, _pDbCfg->CharacterSet.c_str()) != 0){
        LOG_ERROR("!!! Failed. MysqlConnector::CreateDatabaseIfNotExist: Could not set client connection
        return false;
    }
    // 设置数据库区分字母大小写，配置文件中为 utf8_general_ci
    querycmd = strutil::format("ALTER DATABASE CHARACTER SET %s COLLATE %s", _pDbCfg->CharacterSet.c_str(
    if (!Query(querycmd.c_str(), affected_rows)) {
        LOG_ERROR("!!! Failed. MysqlConnector::CreateDatabaseIfNotExist. cmd:" << LOG4CPLUS_STRING_TO_TST
        return false;
    }
    // 创建一个 version 表，使用了 InnoDB 方式
    std::string create_version =
        "CREATE TABLE IF NOT EXISTS `version` (" \
        "`version` int(11) NOT NULL," \
        "PRIMARY KEY (`version`)" \
        ") ENGINE=%s DEFAULT CHARSET=%s;";
    std::string cmd = strutil::format(create_version.c_str(), "InnoDB", _pDbCfg->CharacterSet.c_str());
    if (!Query(cmd.c_str(), affected_rows)) {
        LOG_ERROR("!!! Failed. MysqlConnector::CreateTable. " << LOG4CPLUS_STRING_TO_TSTRING(cmd));
        return false;
    }
    // 创建一个 player 表
    std::string create_player =
        "CREATE TABLE IF NOT EXISTS `player` (" \
        "`sn` bigint(20) NOT NULL," \
        "`name` char(32) NOT NULL," \
        "`account` char(64) NOT NULL," \
        "`base` blob," \
        "`item` blob,"  \
        "`misc` blob,"  \
        "`savetime` datetime default NULL," \
        "`createtime` datetime default NULL," \
        "PRIMARY KEY  (`sn`),"  \
        "UNIQUE KEY `NAME` (`name`),"  \
        "KEY `ACCOUNT` (`account`)"  \
        ") ENGINE=%s DEFAULT CHARSET=%s;";
    cmd = strutil::format(create_player.c_str(), "InnoDB", _pDbCfg->CharacterSet.c_str());
    if (!Query(cmd.c_str(), affected_rows)) {
        LOG_ERROR("!!! Failed. MysqlConnector::CreateTable" << LOG4CPLUS_STRING_TO_TSTRING(cmd));
        return false;
    }
    // 创建完成后，修改 version 表中的 version 字段，设为初始的 0 号版本
    cmd = "insert into `version` VALUES ('0')";
    if (!Query(cmd.c_str(), affected_rows)) {
        LOG_ERROR("!!! Failed. MysqlConnector::CreateTable." << LOG4CPLUS_STRING_TO_TSTRING(cmd));
        return false;
    }
    return true;
}
```

ER_BAD_DB_ERROR（1049），就认为可以创建一个数据库。创建数据库函数代码如下：

顾名思义，CreateDatabaseIfNotExist 函数是在数据库不存在时创建一个全新的供游戏使用的数据库。在上面的

代码中创建了两张表，两条 SQL 语句分别创建了 version 和 player 表。创建 player 表时，同时创建了几个索引，用于快

速查询。数据表创建完成之后，执行了一条 SQL 语句："insert into'version'VALUES（'0'）"，即当前版本为 0 号的初

始版本。

在创建 player 表时使用了一个关键字 InnoDB。InnoDB 和 MyISAM 是使用 MySQL 时常用的两个表类型。如果需要，那么

这里也可以替换成 MyISAM。


### 7.8.2 更新表

在整个开发或上线的过程中，数据表不可能是一成不变的。有需要时，需要对表结构进行更新。


```cpp
bool MysqlTableUpdate::UpdateToVersion() {
    my_ulonglong affected_rows;
    std::string sql = "select version from `version`";
    if (!Query(sql.c_str(), affected_rows))
        return false;
    MYSQL_ROW row = Fetch();
    if (row == nullptr)
        return false;
    int version = GetInt(row, 0);
    if (version == _version)
        return true;
    // 如果 DB 版本不匹配，就升级 DB
    for (int i = version + 1; i <= _version; i++) {
        if (_update_func[i] == nullptr)
            continue;
        if (!_update_func[i]()) {
            LOG_ERROR("UpdateToVersion failed!!!!!, version=" << i);
            return false;
        }
        LOG_INFO("update db to version:" << i);
        // 成功之后，更改 DB 的版本
        std::string cmd = strutil::format("update `version` set version = %d", i);
        if (!Query(cmd.c_str(), affected_rows)) {
            LOG_ERROR("UpdateToVersion failed!!!!!, change version failed. version=" << i);
            return false;
        }
    }
    return true;
}
```

创建表完成之后调用了 UpdateToVersionDB 进行升级，升级代码如下：

更新时首先查询当前数据库的版本号，再从注册的更新列表 update_func 中取得更新函数来更新每

一个版本。如同在 CreateDatabaseIfNotExist 中做的一样，最后一步将 version 表中的 version 字段修

改为当前的 DB 版本号。列表 update_func 的下标从 0 开始。在初始化时绑定好了 version 为 0 的函数为


```cpp
MysqlTableUpdate::MysqlTableUpdate() {
    // 注册更新函数，按下标执行，注意顺序
    _update_func.push_back(BindFunP0(this, &MysqlTableUpdate::Update00));
}
bool MysqlConnector::Update00( ) {
    return true;
}
```

Update00。

Update00 实现为空，我们没有进行任何操作。现在执行 dbmgr 工程来测试创建数据库的流程。执行

之前需要配置 dbmgr 工程数据，在 engine.yaml 文件中为 dbmgr 连接的 MySQL 配置 IP 和端口，如果你的

IP、端口与库中的配置不同，就需要修改。


### 7.8.3 测试更新与创建组件

第一次运行 dbmgr 工程，程序输出信息如下：


```cpp
[ERROR] - [CheckMysqlError]MysqlConnector::CheckError. mysql_errno=1049, mysql_error=Unknown database 'e_
```

[DEBUG] - Mysql update connect. 192.168.0.120:3306 starting...

[DEBUG] - Mysql. try create database:e_gamedata

[DEBUG] - Mysql Update successfully! addr:192.168.0.120:3306

首先连接数据库，反馈回 1049 编号的错误，这时试着创建一个 e_gamedata 数据库。用工具打开数据库，会发现已


```cpp
MariaDB [e_gamedata]> show tables;
```

经创建了两个表，其内部结构与我们期望的一致。

+----------------------+

| Tables_in_e_gamedata |

+----------------------+

| player               |

| version              |


```cpp
2 rows in set (0.00 sec)
MariaDB [e_gamedata]> show columns from player;
```

+----------------------+

+------------+------------+------+-----+---------+-------+

| Field      | Type       | Null | Key | Default | Extra |

+------------+------------+------+-----+---------+-------+

| sn          | bigint(20) | NO   | PRI | NULL    |       |

| name        | char(32)   | NO   | UNI | NULL    |       |

| account    | char(64)   | NO   | MUL | NULL    |       |

| base        | blob       | YES  |     | NULL    |       |

| item        | blob       | YES  |     | NULL    |       |

| misc        | blob       | YES  |     | NULL    |       |

| savetime   | datetime   | YES  |     | NULL    |       |

| createtime | datetime   | YES  |     | NULL    |       |


```cpp
8 rows in set (0.01 sec)
```

+------------+------------+------+-----+---------+-------+

再来测试一下更新效果，使用以下步骤：

• （1）在 mysql_table_update.h 文件中找到变量 version，在旧数据的基础上加 1，即 1。

• （2）增加版本 1 的更新函数 Update01。


```cpp
MysqlTableUpdate::MysqlTableUpdate() {
        // 注册更新函数，按下标执行，注意顺序
        _update_func.push_back(BindFunP0(this, &MysqlTableUpdate::Update00));
        _update_func.push_back(BindFunP0(this, &MysqlTableUpdate::Update01));
    }
```

• （3）修改 MysqlTableUpdate 类的构造函数，增加更新函数注册：


```cpp
bool MysqlTableUpdate::Update01() {
        std::string sql = "ALTER TABLE `player` ADD COLUMN `testa`  blob NULL AFTER `misc`;";
        my_ulonglong affected_rows;
        if (!Query(sql.c_str(), affected_rows)) {
            LOG_ERROR("!!! Failed. MysqlTableUpdate::Update01. ");
            return false;
        }
        return true;
}
```

• （4）实现 Update01 函数，代码如下：

在上面的代码中，在 player 表的列 misc 之后添加一个二进制、名为 testa 的列。更新后执行，其打印数据如下：

[DEBUG] - Mysql update connect. 192.168.0.120:3306 starting...

[INFO] - update db to version:1

[DEBUG] - Mysql Update successfully! addr:192.168.0.120:3306

因为已经有了数据库，没有出现 1049 编号的错误，所以不会走创建数据库的流程，而是直接连接到数据库上，并

进行对应版本号 1 的更新。打开数据库会发现有两个改变：第一个改变是 player 表中多了 testa 列;另一个改变是


```cpp
MariaDB [e_gamedata]> show columns from player;
```

version 表中的 version 列被更改为 1。

+------------+------------+------+-----+---------+-------+

| Field      | Type       | Null | Key | Default | Extra |

+------------+------------+------+-----+---------+-------+

| sn         | bigint(20) | NO   | PRI | NULL    |       |

| name       | char(32)   | NO   | UNI | NULL    |       |

| account    | char(64)   | NO   | MUL | NULL    |       |

| base       | blob       | YES  |     | NULL    |       |

| item       | blob       | YES  |     | NULL    |       |

| misc       | blob       | YES  |     | NULL    |       |

| testa      | blob       | YES  |     | NULL    |       |

| savetime   | datetime   | YES  |     | NULL    |       |

| createtime | datetime   | YES  |     | NULL    |       |


```cpp
9 rows in set (0.00 sec)
MariaDB [e_gamedata]> select * from version;
```

+------------+------------+------+-----+---------+-------+

+---------+

| version |

+---------+

|       1 |


```cpp
1 row in set (0.00 sec)
```

+---------+

当然，在真实的开发过程中，从一个版本到另一个版本，数据库的修改可能非常复杂，修改的可能不止一张表。


## 7.9 数据表中的数据结构与protobuf结构

现在，框架已经在数据库中建立了 player 表，本节要讨论的是 player 表与

protobuf 的关系。在 player 表中的字段 base 使用了二进制数据，在本书的案例

中，这些二进制数据都是 protobuf 结构的。

看到这里可能有些不习惯，大部分人都比较倾向于在数据库的表中创建给

定的列。例如，player 表中有等级 Level 列、经验 Exp 列，打开表格时，就可以

直观地看到这些数据，现在将这些数据放到一个二进制的结构中，看上去就不

直观了。

那么，为什么还要将这些数据变成不那么直观的二进制数据呢？

如果读者曾经有过做游戏的经验，那么一定有数据库升级的经历。游戏运

营之后，基于策划的要求新增了某些功能，需要将某些数据存储到数据库中，

于是创建了一个新的数据表，其中可能有 col1、col2、…、col5 等好几个存储

列。运营了一个月之后，新的要求来了，需要对这个功能进行修改，需要删除

col2、col3，增加 col6、col7。

7.8 节讲的更新数据库结构的操作可以帮上忙，编写一个新的更新函数。

但现在有一个更好的方案，不需要更改列就可以实现数据的删除与新增。

更新数据时不需要再编写更新函数，也就是使用 protobuf 定义的结构作为存储

数据结构。作为协议使用时，protobuf 方便的序列化特性被广泛使用。除此之

外，它还有一个非常给力的特性就是兼容性，这个特性用于存储时也非常给

力。

从整体效率上来看，使用二进制存取的方式要高于多列存取的方式。这是

由于二进制压缩了表的列数，只需要几个列就可以表示大量的数据。列的多少

在 MySQL 的存取上也会影响效率。下面通过例子来看如何将 protobuf 存到 MySQL

中。本节的源代码依然存放在 07_01_db 目录中。工程 libserver 中新建了一个


```protobuf
syntax = "proto3";
package Proto;
```

db.proto 文件，该文件中定义的结构是存储在数据库中的结构，定义如下：

...


```protobuf
enum Gender {
    none = 0;
    male = 1;
    female = 2;
}
message PlayerBase {
    Gender gender = 1;
    int32 level = 2;
}
message LastWorld {
    int32 world_id = 1;
    int64 world_sn = 2;
    Vector3 position = 3;
}
message PlayerMisc {
    LastWorld last_world = 1;     // 公共地图
    LastWorld last_dungeon = 2;   // 副本地图
    ...
}
message Player {
    uint64 sn = 1;
    string name = 2;
    PlayerBase base = 3;
    PlayerMisc misc = 4;
}
```

在上面的结构中，Proto::Player 就是最终的 Player 表。目前只用到了

sn、name、base 三个列，其他的列暂时无用。PlayerBase 结构就是 base 列的数

据结构，这个结构中有一个 int32 类型的 level 数据，还有一个枚举类型来表示

性别，以便区分显示模型。PlayerMisc 是 misc 列的数据结构，保存了位置数

据，last_world 表示公共地图的位置，last_dungeon 表示副本地图的位置。

**图 7-4 从整体上展示了数据库的结构。protobuf中的数据是一个可以嵌套的**

结构，也可以随时随意地增加与删除属性。在编码时，只有一个规则需要注

意，就是属性编号不能重用。假如之前的 4 号属性我们不再使用，可以将这个 4

号定义直接删除，这大大简化了我们的工作量，不再需要编写数据库更新函数

了。

**图 7-4 玩家数据结构**


## 7.1 0

角 色 查 询 与 创 建 流 程

数据库的操作是整个框架的基础，下面将之前的功能整合起来。在第 6 章的

例子中，机器人已经登录到了 login 进程中，梳理一下登录之后的流程：在进程

login 中验证完成之后，需要向 dgmgr 进程发起查询角色的协议，如果返回数据

没有角色，就由客户端发起创建角色的协议。整个流程如图 7-5 所示。

**图 7-5 创建、读取角色的流程**

对于客户端来说，需要发起的协议有两个：一个是账号验证协议;另一个

是创建角色协议。需要处理的协议是收到角色列表协议。

对于 login 工程来说，需要处理 3 个协议：验证账号、收到角色列返回数据

以及收到创建角色协议的处理。

对于 dbmgr 工程来说，需要处理两个协议：一个是查询角色协议;另一个是

创建角色协议。


## 7.1 0 . 1 为login进程与dbmgr进程创建连接类

在前面的案例中，协议总是从客户端（robots 进程）传来的，现在服务端是多进程了，验证账

号在 login 进程中，而创建角色在 dbmgr 进程中，这两个进程是如何传递消息的呢？

首先，login 和 dbmgr 都打开了监听端口，但 login 的监听端口是为了玩家连接，而 dbmgr 打开监

听端口则是为了其他进程方便连接到它。所以，在 login 工程的 main.cpp 文件中加入了一个新的


```cpp
int main(int argc, char* argv[]) {
    const APP_TYPE curAppType = APP_TYPE::APP_LOGIN;
    ServerApp app(curAppType, argc, argv);
    app.Initialize();
    ...
    auto pYaml = Yaml::GetInstance();
    auto pCommonConfig = pYaml->GetIPEndPoint(curAppType);
    pThreadMgr->CreateComponent<NetworkListen>(pCommonConfig->Ip, pCommonConfig->Port);
    pThreadMgr->CreateComponent<NetworkConnector>((int)APP_TYPE::APP_DB_MGR, 0);
    ...
}
```

NetworkConnector 组件。

监听组件 NetworkListen 传入了参数监听的 IP 与端口，而连接组件 NetworkConnector 传入的参

数为 AppType。在 NetworkConnector 初始化时，我们通过 AppType 从 YAML 配置文件中读出了需要连接

的 AppType 的 IP 与端口。

在上面的代码中，希望可以连接到 dbmgr 的端口。当 NetworkConnector 类从对象池中被唤醒之


```cpp
void NetworkConnector::AwakeFromPool(int appType, int appId) {
    auto pYaml = Yaml::GetInstance();
    auto pComponent = pYaml->GetIPEndPoint((APP_TYPE)appType, appId);
    if (pComponent == nullptr) {
        LOG_ERROR("can't find yaml config.");
        return;
    }
    Connect(pComponent->Ip, pComponent->Port);
    auto pNetworkLocator = ThreadMgr::GetInstance()->GetComponent <NetworkLocator>();
    pNetworkLocator->AddConnectorLocator(this, (APP_TYPE)appType, appId);
}
```

后，它做了如下操作：

在初始化的同时，将这个网络类放到了 NetworkLocator 组件中，方便发送数据时的定位。这

样，login 进程就与 dbmgr 进程有了一个网络连接。


## 7.1 0 . 2 找到指定的NetworkConnector实例

新流程需要向 dbmgr 发起一个查询请求，那么 login 是如何找到 dbmgr 的连接实例的呢？继续向下走这个流程，验证


```cpp
void Account::HandleAccountCheckToHttpRs(Packet* pPacket) {
    auto proto = pPacket->ParseToProto<Proto::AccountCheckToHttpRs>();
    auto pPlayer = _playerMgr.GetPlayer(proto.account());
    if (pPlayer == nullptr)   {
        std::cout << "can't find player. account:" << proto.account().c_str() << std::endl;
        return;
    }
    Proto::AccountCheckRs protoResult;
    protoResult.set_return_code(proto.return_code());
    SendPacket(Proto::MsgId::C2L_AccountCheckRs, pPlayer->GetSocket(), protoResult);
    // 验证成功，向 DB 发起查询
    if (proto.return_code() == Proto::AccountCheckReturnCode::ARC_OK) {
        Proto::QueryPlayerList protoQuery;
        protoQuery.set_account(pPlayer->GetAccount().c_str());
        SendPacket(Proto::MsgId::L2DB_QueryPlayerList, protoQuery, APP_DB_MGR);
    }
}
```

账号请求结束之后，在其处理函数中有如下实现：

函数 HandleAccountCheckToHttpRs 是 HTTP 请求的返回处理函数。它首先从返回协议中取出账号，以验证登录玩家

对象是否还存在，因为每一步都是异步操作，所以每一次消息到来时，玩家都有可能已经下线了。如果验证成功，就

向 dbmgr 发送 L2DB_QueryPlayerList 协议。组织协议看上去并不复杂，只传入了账号数据，最后调用了 SendPacket 函数


```cpp
void SendPacket(const Proto::MsgId msgId, const SOCKET socket, google::protobuf::Message& proto);
void SendPacket(const Proto::MsgId msgId, google::protobuf::Message& proto, APP_TYPE appType, int appId =
```

发送数据。SendPacket 函数的定义在 message_system.h 文件中，有两个定义，代码如下：

当需要向网络层发送消息时，现有两种发送方式，一种类似 Robot 类，创建一个 Socket，主动连接到服务端，这类

连接可以从 Player 类上拿到 Socket 值;另一种是服务端进程之间的数据发送，这时我们只需要向一个指定的 APP_TYPE


```cpp
void IMessageSystem::SendPacket(const Proto::MsgId msgId, google::protobuf::Message& proto, APP_TYPE appT
    auto packet = CreatePacket(msgId, 0);
    packet->SerializeToBuffer(proto);
    SendPacket(packet, appType, appId);
}
void IMessageSystem::SendPacket(Packet* packet, APP_TYPE appType, int appId){
    auto pNetworkLocator = ThreadMgr::GetInstance()->GetComponent <NetworkLocator>();
    auto pNetwork = pNetworkLocator->GetNetworkConnector(appType, appId);
    if (pNetwork != nullptr) {
        packet->SetSocket(pNetwork->GetSocket());
        pNetwork->SendPacket(packet);
        return;
    }
```

上发送数据即可。其实现如下：


```cpp
}
```

...

在上面的代码中，从主线程中取出 NetworkLocator 组件，从中取得了 dbmgr 的 INetwork 实例发送数据。初始化

NetworkConnector 时，已经向 NetworkLocator 组件注册过数据了，所以 NetworkLocator 可以找到连接到 dbmgr 的

NetworkConnector 实例，直接发送数据。


## 7.1 0 . 3 创建角色

对于数据库查询角色的功能，我们在阐述 MySQL 函数的时候已经讲解过

了，这里不再多说。玩家数据最终会传递到客户端，也就是我们的测试 robots

进程中，Robot 类收到一个空的玩家数据，这时它会发起一个创建角色的协

议，协议编号为 C2L_CreatePlayer。在这个创建角色协议里，客户端一般会带

上自己设定的参数，例如玩家名、玩家性别等。login 进程收到这个协议的处

理函数为 Account::HandleCreatePlayer，这个函数最终将数据传递到了

dbmgr 进程，使用的协议编号为 L2DB_CreatePlayer。这里我们不再详细讲述。


## 7.1 0 . 4 机器人登录创建角色测试

在 07_01_db 目录的工程中同时也准备了 robots 测试工程。现在需要启动两个服务

端进程 login 和 dbmgr。在启动之前确保 engine.yaml 文件中的数据库地址正确，不需要

手动操作数据库，程序会自动创建数据库，并更新数据表。如果先启动的是 login，就

会看到它一直不断地出现 re connect（重新连接）打印数据。因为它一直在试图连接

到 dbmgr 上，所以直到启动好 dbmgr 进程之后，login 的 re connect 行为才会停止。

启动 robots 之后，输入“login-a test”可以进行 test 登录，在 Robot 类中，我们

完成了自动登录和自动创建角色的功能。第一次登录，dbmgr 进程上会有以下打印消

息：

[INFO] - HandlePlayerCreate sn:55650694413156386 account:test name:test-2086

[DEBUG] - player list. account:test player list size:1 socket:700

为 test 账号创建了一个名为 test-2086 的角色，这个名字是随机取的。登录 MySQL


```cpp
MariaDB [e_gamedata]> select sn, name, account, base from player;
```

数据库，看看我们刚才新建的玩家数据：

+-------------------+-----------+---------+------+

| sn                   | name       | account | base |

+-------------------+-----------+---------+------+

| 55650694413156386 | test-2086 | test     | NULL |


```cpp
1 row in set (0.00 sec)
```

+-------------------+-----------+---------+------+

当然，如果登录已经创建过角色的账号，那么将不会再创建角色。关于 Robot 类的

代码实现细节可参考源代码，由于篇幅有限，这里就不再赘述。


## 7.1 1

总 结

本章介绍了对数据库的使用，数据库是游戏存储的基本组件。第 8 章将进

一步优化这个 ECS 框架。但不需要担心，在这些改造中，我们几乎不用变更这

些基于 Actor 原则的类，因为它们在某种程度上来说真的是与世隔绝，与外界

没有任何耦合。
