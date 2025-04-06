# JsonParser

学习记录
链接:[【C++项目实战】实现一个JSON解析器](https://www.bilibili.com/video/BV1pa4y1g7v6/?spm_id_from=333.337.search-card.all.click&vd_source=452811d53d64d58829c7c9b100c1115c)
代码包含完整的注释
# 笔记

---
## 特性
### 直接列表初始化
> C++11 引入了 `{}` 初始化，目的是提供一种统一的初始化方式，==适用于所有类型==
```cpp
int a = 5;     // 传统初始化（拷贝初始化）
int b(5);      // 直接初始化（函数式初始化）
int c{5};      // 直接列表初始化（C++11 推荐方式）
```

`{}` 初始化会检查 **窄化转换**（如 `double → int` 丢失精度、`long → int` 溢出等），并在编译时报错，避免潜在问题。例如：
```cpp
int x{5.0};    // 错误：从 double 到 int 是窄化转换
int y(5.0);    // 警告（可能丢失精度），但仍允许编译
```

**显式构造**临时对象
```cpp
int a = 'A';          // 隐式转换（ASCII → int）
int b = int{'A'};     // 显式构造临时 int（更清晰）
```

⚠️注意：如果属性是私有的，必须提供公有的初始化构造函数才能使用列表初始化

### if语句初始化
```cpp
if (auto num = try_parse_num<int>(str); num.has_value()) {

    // 这种写法是依次判定的意思：首先执行赋值，然后判断 has_value()。
}
```


---
## 函数

### 全局函数
####  std::from_chars()
> std::from_chars 是 C++17 引入的一个**==超轻量级==字符串解析函数**，作用是：
> **把字符串直接转成数字，效率高，不分配内存，不抛异常。**
> **头文件:**`#include <charconv>`

```cpp
auto res = std::from_chars(sv.begin(), sv.end(), value);
    if (res.ec == std::errc()) {
        // 转换成功
    } else {
        // 转换失败
    }
```
- 返回值
	返回一个 std::from_chars_result，包含两个成员
	
| **返回值** |                          |
| :-----: | :----------------------: |
|   ptr   |       指向未被处理的下一个字符       |
|   ec    | 错误码（std::errc 枚举），判断是否成功 |


### 成员函数
#### try_emplace
> try_emplace 是 C++17 中引入的 std::map 和 std::unordered_map 的成员函数，用来**高效地插入键值对**，如果键==不存在时才插入==。

**🟢 特点：**
1. **只有 key 不存在时才插入**。
2. **避免不必要的构造**（尤其是值对象构造开销大时）
3. 比 insert 更高效（特别是右值或复杂值时）

#### insert_or_assign
> insert_or_assign 是 C++17 引入的，用于 std::map 和 std::unordered_map，作用是：
>==如果 key 存在，更新值==；否则插入新键值对。

  会进行构造
  
| **插入方式**                     | **支持 move 吗** | **用法**                |
| ---------------------------- | ------------- | --------------------- |
| emplace(k, std::move(v))     | ✅ 是           | 推荐方式，完美转发             |
| try_emplace(k, std::move(v)) | ✅ 是           | key 不存在才 move，避免浪费    |
| insert({k, std::move(v)})    | ✅ 是           | 但临时 pair 还是构造了 → 有点浪费 |
|insert_or_assign(k, std::move(v))|✅ 是|key 存在就移动赋值|


---

## 类
### std::optional
> std::optional 是一个模板类，表示一个可能包含值的对象。
> 一般用来表示一个==可能为空==的值，可以隐式转换为bool
   **头文件**：`#include <optional>`


• **检查值**：
```cpp
if (opt1.has_value()) {
    // 执行逻辑
}
```
- **获取值**
```cpp
int value = opt2.value();                   // 获取值，如果为空会抛出异常
//也可以用*或者->
```

- std::nullopt
	显式无值

### std::variant
> 功能上variant类似于c的==枚举==，但内存上更类似于结构体
   **头文件**：`<variant>`

- index()
```cpp
  std::variant<std::string, int> data;
  data = "reisen";
  // index()函数指向最近存储的类型在variant定义的位置(默认从0开始)
  std::cout << data.index() << std::endl;
```
- **获取值**
	- std::get/std::get_if
```cpp
 // 报错，因为访问的对象是int，而选择的访问方式为string
 // std::cout << std::get<std::string>(data) << std::endl;
 if (auto value: string * = std::get_if<std::string>(v: &data)) {
	// get_if像名字一样，可以判断接受的数据类型
	std::string &v = *value;
	std::cout << v << std::endl;
 } else {
	std::cout << "value type get wrong\n";
 }
```

- std::holds_alternative()
	 std::holds_alternative 是 C++17 中引入的一个模板函数，用于检查 std::variant 中是否包含指定类型的值。它的主要功能是**返回一个布尔值，指示 variant 当前==是否存储了所提供的类型==。**

- std::monostate
	std::monostate 是 C++17 中为 std::variant 设计的一个**占位类型**，表示“什么都没有”。它常用于 std::variant 的第一个模板参数，让 variant 有一个“空”的初始状态。


**union效率更高，但是variant类型安全，不会导致未定义行为**

### string_view
> std::string_view 是 C++17 引入的一个非常轻量的==字符串“视图”==，和它的名字一样，它 **不拥有数据本身**，只是一个对已有字符序列的 **只读视图**,多用于传参

`string_view 本质上就是一个 { **const char* ptr, size_t length** }，不会拷贝字符串内容，也不会管理内存。`

```cpp
std::string str = "hello";
std::string_view sv = str;

std::cout << sv << std::endl; // 输出 hello
std::cout<<sv.substr(0,3)<<end::endl;//输出hel

```

⚠️注意：
```cpp
//错误的，string是一个临时对象，导致sv变为悬空指针
std::string_view sv = std::string("hello");
```

