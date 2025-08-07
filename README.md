# B+ 树索引库

## 新闻
我们计划用C语言重新编写该索引库，以获得更高的运行效率。
在新的版本中，我们还将引入
- 基于空闲列表的动态空间管理机制，不再需要定期维护
- 更丰富的服务
- 更完善的接口设计
- 更详细的文档
新版本将存于同目录下仓库`db-c`中。
新版本发布后，本仓库将删除，但现版本源码依旧可在同根目录下仓库`oop-cpp`中找到，该仓库的项目调用了现版本索引库。
本新闻编写与2025年8月8日。

## 概述
该项目实现了一个基于 B+ 树的索引库，支持高效的键值存储和检索操作。该库提供了以下核心功能：
- 键值对的持久化存储
- 精确匹配查询（点查询）
- 范围查询（区间查询）
- 动态的树结构调整（自动分裂和合并）
- 基于文件的磁盘存储
- 支持大规模数据集（节点阶数 = 254）

## 文件结构
该库会创建并管理两个文件：

1. **`<文件名>.idx`** - 存储 B+ 树索引结构
   - 文件头（根节点偏移量、树高度、节点总数）
   - 固定大小的树节点（每个节点 4KB）
   
2. **`<文件名>.dat`** - 存储实际数据值
   - 带长度前缀的可变长记录
   - 以 null 结尾的字符串数据

## 类文档

### `bptree::bpnode` - B+ 树节点结构
```cpp
class bptree::bpnode {
public:
    uint8_t node_type;    // 节点类型: 0x01 = 分支节点, 0x02 = 叶节点
    uint8_t key_cnt;      // 当前节点包含的键数量
    uint64_t keys[ORDER]; // 键数组 (大小 = ORDER)
    uint64_t children[ORDER]; // 子节点指针/数据偏移量
    uint64_t next_leaf;   // 指向下一个叶节点的指针
};
```

### `bptree` - B+ 树主类

#### 构造函数
```cpp
bptree(const std::string& filename);
```
初始化 B+ 树并关联数据文件
- 自动创建 `.idx` 索引文件和 `.dat` 数据文件
- 若文件已存在则加载现有数据

#### 数据操作接口
```cpp
// 插入键值对
bool insert(uint64_t key, std::string data);

// 精确查询
std::string find(uint64_t key);

// 范围查询 [left, right)
std::vector<std::string> find_range(uint64_t left, uint64_t right);

// 删除指定键
bool erase(uint64_t key);

// 更新指定键的值
bool update(uint64_t key, std::string data);
```

#### 内部机制
```cpp
// 分裂子节点
void split_ith_child(uint64_t offset, int i);

// 合并子节点
void merge_child(uint64_t offset, int i);

// 节点分配
uint64_t alloc_index_node(bpnode node); // 分配索引节点
uint64_t alloc_data(const char* data, uint64_t size); // 分配数据空间
```

## 使用示例
```cpp
// 创建B+树实例
bptree index("mydatabase");

// 插入数据
index.insert(1001, "张三");
index.insert(1002, "李四");
index.insert(1003, "王五");

// 查询数据
std::cout << index.find(1002); // 输出: 李四

// 范围查询
auto results = index.find_range(1001, 1003);
// 结果: ["张三", "李四"]

// 更新数据
index.update(1002, "李四新信息");

// 删除数据
index.erase(1003);
```

## 技术细节
1. **节点结构**：
   - 固定节点大小 4KB (0x1000 字节)
   - 最大阶数 ORDER = 254
   - 分支节点存储子树指针
   - 叶节点包含数据偏移量和指向下一叶节点的指针

2. **动态调整**：
   - 插入时自动分裂满节点
   - 删除时自动合并/重分布节点
   - 根节点满时自动增加树高度

3. **数据存储**：
   - 数据文件存储格式：[数据长度][数据内容][\0]
   - 更新时自动处理空间不足的情况

4. **持久化**：
   - 析构时自动更新索引文件头
   - 文件操作后立即刷新缓冲区
   - 支持异常恢复（通过文件检查）

## 注意事项
1. 键类型固定为 `uint64_t`
2. 值类型为 `std::string`，支持二进制数据
3. 范围查询区间为左闭右开 [left, right)
4. 不支持并发访问（单线程设计）
5. 数据更新时若空间不足会自动删除后重新插入
