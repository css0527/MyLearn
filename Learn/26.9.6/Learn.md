## 📚 什么是 RAII？

**RAII 核心思想**：
- 资源获取在构造函数中完成
- 资源释放在析构函数中完成
- 对象生命周期结束自动释放资源
- 异常安全，不会泄漏资源

**常见资源类型**：
- 内存（堆内存）
- 文件句柄
- 网络连接
- 相机设备
- 互斥锁
- OpenCV 的 Mat（已经实现了 RAII）

---

## 📁 创建项目结构

```bash
cd /home/ubuntu22/MyLearn/Learn/26.9.6/
mkdir -p camera_wrapper/{src,include,build}
cd camera_wrapper

touch src/main.cpp
touch include/CameraWrapper.hpp
touch src/CameraWrapper.cpp
touch CMakeLists.txt
```

---

## 📝 编写 CameraWrapper 类

### 1. 头文件 `include/CameraWrapper.hpp`

### 2. 实现文件 `src/CameraWrapper.cpp`

### 3. 主程序 `src/main.cpp`

## 📝 编写 CMakeLists.txt


## 🔨 编译和运行

```bash
cd /home/ubuntu22/MyLearn/Learn/26.9.6/camera_wrapper
mkdir -p build
cd build

# 配置
cmake ..

# 编译
make -j4

# 运行（如果有相机）
./main
```

---

## 📊 预期输出

```
=========================================
   CameraWrapper RAII Demo
=========================================

📷 Running simple camera demo...
Press 'q' to quit, any other key to continue

📷 CameraWrapper: Opening camera 0
✅ Camera 0 opened successfully!
📷 Camera Info:
   Name: Camera_0
   State: 1
   Width: 640
   Height: 480
   FPS: 30
   Total Frames: -1
   Captured: 0 frames

[Camera window opens...]

✅ Demo completed, camera will be auto-released...
🧹 CameraWrapper: Destructor called for Camera_0
📷 Releasing camera: Camera_0
   📊 Captured 50 frames
   ⏱️  Avg grab time: 8.5 ms
✅ Camera released: Camera_0
```

---

## 🎯 关键学习点

### 1. RAII 核心
```cpp
// 构造函数获取资源
CameraWrapper camera(0);  // 自动打开相机

// 析构函数释放资源
// 离开作用域时自动调用 ~CameraWrapper()
// 自动调用 release() 释放相机
```

### 2. 禁止拷贝，支持移动
```cpp
// 禁止拷贝（避免双重释放）
CameraWrapper c1(0);
CameraWrapper c2 = c1;  // ❌ 编译错误

// 支持移动（高效转移所有权）
CameraWrapper c3 = std::move(c1);  // ✅ 所有权转移
```

### 3. 异常安全
```cpp
try {
    CameraWrapper camera(99);  // 无效相机ID
    // 抛出异常，但之前分配的资源会自动清理
} catch (...) {
    // 相机资源已经被释放
}
```

---

## 🔧 在 ROS2 项目中使用

```cpp
// 在 ROS2 Node 中使用
class MyNode : public rclcpp::Node {
public:
    MyNode() : Node("camera_node") {
        // RAII：在构造函数中打开相机
        camera_ = std::make_unique<CameraWrapper>(0);
    }
    
private:
    std::unique_ptr<CameraWrapper> camera_;  // 自动管理生命周期
};
