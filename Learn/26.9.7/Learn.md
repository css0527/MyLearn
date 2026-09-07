## 📚 第一部分：智能指针核心概念

### 1. 为什么需要智能指针？

**裸指针的问题**：
```cpp
// ❌ 裸指针 - 容易出错
Camera* cam = new Camera(0);
cam->grab(frame);
delete cam;  // 忘记 delete → 内存泄漏
// 或者异常发生时 delete 不会执行
```

**智能指针的好处**：
- ✅ 自动管理内存生命周期
- ✅ 异常安全
- ✅ 明确所有权语义
- ✅ 减少内存泄漏

---

### 2. `std::unique_ptr` - 独占所有权

```cpp
// ✅ unique_ptr - 独占所有权
std::unique_ptr<Camera> cam = std::make_unique<Camera>(0);
cam->grab(frame);
// 离开作用域自动 delete，无需手动释放
```

**特点**：
- 不能被拷贝（只能移动）
- 轻量级（没有额外开销）
- 默认使用 `delete` 释放
- 适合作为类成员或局部变量

---

### 3. `std::shared_ptr` - 共享所有权

```cpp
// ✅ shared_ptr - 共享所有权
std::shared_ptr<Camera> cam1 = std::make_shared<Camera>(0);
std::shared_ptr<Camera> cam2 = cam1;  // 引用计数 +1
// 两个指针共享同一个 Camera
// 最后一个指针销毁时才释放
```

**特点**：
- 可以被拷贝（引用计数）
- 有额外开销（引用计数 + 控制块）
- 适合多个对象共享资源
- 适合作为函数返回值

---

## 🔧 第二部分：改造 CameraWrapper 使用智能指针

### 1. 创建新项目

```bash
cd /home/ubuntu22/MyLearn/Learn/26.9.7/
mkdir -p smart_pointer_demo/{src,include,build}
cd smart_pointer_demo
```

---

### 2. 头文件 `include/CameraWrapper.hpp`（智能指针版本）

---

### 3. 实现文件 `src/CameraWrapper.cpp`

---

### 4. 主程序 `src/main.cpp` - 展示智能指针用法

---

### 5. CMakeLists.txt

---

## 🔨 编译和运行

```bash
cd /home/ubuntu22/MyLearn/Learn/26.9.7/smart_pointer_demo
mkdir -p build && cd build
cmake ..
make -j4
./main
```

---

## 📊 智能指针对比总结

| 特性 | unique_ptr | shared_ptr | weak_ptr |
|------|-----------|------------|----------|
| **所有权** | 独占 | 共享 | 弱引用 |
| **拷贝** | ❌ 禁止 | ✅ 允许 | ✅ 允许 |
| **移动** | ✅ | ✅ | ✅ |
| **引用计数** | ❌ 无 | ✅ 有 | ✅ 有（观察） |
| **开销** | 小 | 较大 | 较小 |
| **使用场景** | 局部变量、类成员 | 多线程共享、观察者 | 避免循环引用 |

---

## ✅ 代码 Review 

### Review 1: 使用 `make_unique` 代替 `new`

**Before (裸指针)**:
```cpp
CameraWrapper* cam = new CameraWrapper(0);
cv::Mat frame;
cam->grab(frame);
delete cam;  // 容易忘记
```

**After (智能指针)**:
```cpp
auto cam = std::make_unique<CameraWrapper>(0);
cv::Mat frame;
cam->grab(frame);
// 自动释放 ✅
```

### Review 2: 函数参数传递所有权

**Before (裸指针)**:
```cpp
void process(CameraWrapper* cam) {
    cam->grab(frame);
    delete cam;  // 谁负责删除？容易混乱
}
```

**After (unique_ptr)**:
```cpp
void process(std::unique_ptr<CameraWrapper> cam) {
    cam->grab(frame);
    // 所有权明确，函数结束时自动释放 ✅
}
// 调用: process(std::move(cam));
```

### Review 3: 多线程共享资源

**Before (裸指针)**:
```cpp
CameraWrapper* cam = new CameraWrapper(0);
std::thread t1([cam](){ cam->grab(frame); });
std::thread t2([cam](){ cam->grab(frame); });
// 谁负责删除？cam 可能被提前释放 ❌
```

**After (shared_ptr)**:
```cpp
auto cam = std::make_shared<CameraWrapper>(0);
std::thread t1([cam](){ cam->grab(frame); });  // 引用计数 +1
std::thread t2([cam](){ cam->grab(frame); });  // 引用计数 +1
// 所有线程结束时自动释放 ✅
```

### Review 4: 容器中使用

**Before (裸指针)**:
```cpp
std::vector<CameraWrapper*> cameras;
cameras.push_back(new CameraWrapper(0));
cameras.push_back(new CameraWrapper(1));
// 需要遍历 delete，容易泄漏 ❌
```

**After (shared_ptr)**:
```cpp
std::vector<CameraSharedPtr> cameras;
cameras.push_back(std::make_shared<CameraWrapper>(0));
cameras.push_back(std::make_shared<CameraWrapper>(1));
// 容器析构时自动释放 ✅
```


1. **优先使用 `unique_ptr`** - 独占所有权，零开销
2. **需要共享时使用 `shared_ptr`** - 多线程、观察者模式
3. **使用 `weak_ptr` 打破循环引用** - 避免内存泄漏
4. **使用 `make_unique` / `make_shared`** - 更安全、更高效
5. **`unique_ptr` 可以转换为 `shared_ptr`**（反之不行）