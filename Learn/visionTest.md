> 版本：v1.0 | 基于C++/OpenCV
---

## 一、基础必答题

### 1. Git规范题

**问题：** 为什么比赛紧急修复bug时禁止使用 `git push --force`？

**答案：**
- `git push --force` 会覆盖远程仓库的历史记录
- 紧急修复时可能有多个成员同时在不同分支修复不同bug
- 强制推送会导致队友基于旧提交的修复被永久丢失

**正确做法：**
```bash
# 方案1：先同步再推送
git pull --rebase
git push

# 方案2：使用更安全的强制推送
git push --force-with-lease
```

---

### 2. 相机掉帧排查（C++特有）

**问题：** 工业相机在现场从150fps掉到30fps，如何排查？

**排查流程（按顺序）：**

| 层级 | 检查项 | 具体操作 |
|:---|:---|:---|
| 硬件链路 | USB/网线连接 | 检查接口松动，更换线缆 |
| 带宽资源 | USB总线竞争 | 拔掉其他USB设备 |
| 系统负载 | CPU占用 | 检查是否有进程占满CPU |
| 相机驱动 | 驱动稳定性 | 重启相机SDK，检查固件 |
| 代码逻辑 | 内存问题 | 见下方C++排查点 |

**C++特有排查点：**

```cpp
// 错误1：循环内动态分配内存
while (running) {
    std::vector<Point> points;  // 每帧重新分配
    // 正确：声明在外，每帧clear()
}

// 错误2：深拷贝大图像
cv::Mat process(cv::Mat input) {
    cv::Mat output = input.clone();  // 昂贵深拷贝
    // 正确：cv::Mat output = input;  // 浅拷贝
}

// 错误3：智能指针循环引用
struct Node {
    std::shared_ptr<Node> next;  // 错误：应使用weak_ptr
};
```

**现场排查命令：**
```bash
valgrind --leak-check=full ./robot_vision          # 内存泄漏
perf stat -e cache-misses ./robot_vision           # CPU cache miss
```

---

### 3. 坐标变换与弹道补偿

**问题：** 为什么视觉解算的坐标传给电控后子弹总是打偏？

**完整变换链路：**
```
相机坐标系 → 云台坐标系（外参） → 枪口坐标系 → 世界坐标系（弹道补偿） → 电控
```

**C++实现：**

```cpp
struct Vector3d { double x, y, z; };

class CoordinateTransformer {
private:
    cv::Mat R_cam2gimbal;      // 3x3旋转矩阵
    cv::Mat T_cam2gimbal;      // 3x1平移向量
    double bullet_speed = 30.0; // 子弹速度 m/s
    double gravity = 9.8;       // 重力加速度
    
public:
    void computeAngles(const Vector3d& target_in_cam, 
                       double& out_yaw, 
                       double& out_pitch) {
        // 1. 相机→云台坐标系
        cv::Mat P_cam = (cv::Mat_<double>(3,1) << target_in_cam.x, 
                         target_in_cam.y, target_in_cam.z);
        cv::Mat P_gimbal = R_cam2gimbal * P_cam + T_cam2gimbal;
        
        double x = P_gimbal.at<double>(0);
        double y = P_gimbal.at<double>(1);
        double z = P_gimbal.at<double>(2);
        
        // 2. 计算飞行时间
        double distance = std::sqrt(x*x + y*y + z*z);
        double flight_time = distance / bullet_speed;
        
        // 3. 重力下坠补偿
        double drop_compensation = 0.5 * gravity * flight_time * flight_time;
        
        // 4. 计算最终角度
        out_yaw = std::atan2(x, z);
        out_pitch = std::atan2(y + drop_compensation, z);
    }
};
```

---

## 二、核心拉分题

### 1. 装甲板识别（无颜色方案）

**问题：** 灯光杂乱导致颜色阈值失效，如何不依赖深度学习进行识别？

**C++实现：**

```cpp
class ArmorDetector {
private:
    const float MIN_ASPECT_RATIO = 3.0f;
    const float MAX_ASPECT_RATIO = 8.0f;
    const float MIN_AREA = 100.0f;
    const float MAX_AREA = 2000.0f;
    
public:
    std::vector<ArmorPlate> detect(const cv::Mat& gray_img) {
        cv::Mat edges;
        cv::Canny(gray_img, edges, 50, 150);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL, 
                         cv::CHAIN_APPROX_SIMPLE);
        
        std::vector<LightBar> light_bars;
        for (const auto& contour : contours) {
            cv::RotatedRect rect = cv::minAreaRect(contour);
            float aspect = std::max(rect.size.width, rect.size.height) / 
                           std::min(rect.size.width, rect.size.height);
            float area = rect.size.area();
            
            if (aspect >= MIN_ASPECT_RATIO && aspect <= MAX_ASPECT_RATIO &&
                area >= MIN_AREA && area <= MAX_AREA) {
                light_bars.push_back({rect});
            }
        }
        
        return pairLightBars(light_bars);
    }
};
```

**核心思路：** 放弃颜色，改用**几何特征**（长宽比、面积、平行度、高度相似性）。

---

### 2. 能量机关预测公式

**问题：** 已知子弹飞行时间100ms，目标转速10rpm，求预测角度偏移。

**C++实现：**

```cpp
class EnergyPredictor {
private:
    double bullet_speed = 30.0;   // m/s
    double rpm = 10.0;            // 转/分钟
    
public:
    double predictYawOffset(const cv::Point3d& current_pos,
                            const cv::Point3d& center) {
        // 1. 角速度 rad/s
        double angular_velocity = rpm * 2 * M_PI / 60.0;
        
        // 2. 飞行时间
        double distance = cv::norm(current_pos - center);
        double flight_time = distance / bullet_speed;
        
        // 3. 预测角度偏移
        return angular_velocity * flight_time;  // ≈ 0.1047 rad ≈ 6°
    }
    
    cv::Point3d predictPosition(const cv::Point3d& pos, 
                                 const cv::Point3d& center,
                                 double theta) {
        cv::Point3d relative = {pos.x - center.x, 
                                pos.y - center.y, 
                                pos.z - center.z};
        double cos_t = std::cos(theta);
        double sin_t = std::sin(theta);
        
        return {
            relative.x * cos_t - relative.z * sin_t + center.x,
            relative.y + center.y,
            relative.x * sin_t + relative.z * cos_t + center.z
        };
    }
};
```

---

### 3. 性能优化（不改变算法逻辑）

**C++优化方法表：**

| 方法 | 代码示例 | 提升幅度 |
|:---|:---|:---|
| 预分配内存 | `vec.reserve(1000);` | 避免多次reallocation |
| 传递引用 | `void f(const cv::Mat& img)` | 避免深拷贝 |
| ROI跟踪 | 只处理上一帧目标周围区域 | 60-80% |
| 图像降采样 | `cv::resize(img, small, cv::Size(640,480));` | 50-70% |
| 提前转灰度 | 相机直接输出灰度图 | 15-25% |
| 编译优化 | `-O3 -march=native -mtune=native` | 30-50% |
| OpenCV UMat | `cv::UMat uimg = img.getUMat();` | GPU加速 |
| 移动语义 | `return std::move(local_mat);` | 依赖RVO |

**内存布局优化：**
```cpp
// 优化前：内存不连续
struct BadPoint { float x; float y; bool valid; float z; };  // 16字节但有空洞

// 优化后：cache友好
struct GoodPoint { float x, y, z; bool valid; char pad[3]; };  // 16字节对齐
static_assert(sizeof(GoodPoint) == 16);
```

---

## 三、极限压轴题

### 1. 系统解耦（通信协议变更）

**问题：** 电控突然改变通信协议，代码需要修改几个文件？

**正确答案：只修改1个文件（通信驱动层）**

**C++架构设计：**

```cpp
// 1. 抽象接口（不修改）
class ICommunication {
public:
    virtual ~ICommunication() = default;
    virtual void send(float yaw, float pitch, float distance) = 0;
};

// 2. 具体实现（唯一需要修改的文件）
class McuCommunication : public ICommunication {
public:
    void send(float yaw, float pitch, float distance) override {
        // 新协议：打包成16位掩码
        uint16_t mask = 0;
        mask |= (static_cast<uint16_t>(yaw * 100) & 0x3FF);
        mask |= (static_cast<uint16_t>(pitch * 100) & 0x3F) << 10;
        write(serial_fd, &mask, sizeof(mask));
    }
};

// 3. 视觉主流程（完全不修改）
class VisionNode {
    std::unique_ptr<ICommunication> comm;  // 依赖抽象
public:
    VisionNode(std::unique_ptr<ICommunication> c) : comm(std::move(c)) {}
    void process() { comm->send(yaw, pitch, dist); }
};
```

---

### 2. 鲁棒性与异常处理

**问题：** 相机过曝/欠曝3秒，如何保证不发送错误数据？

**状态机设计：**

```cpp
enum class VisionState {
    TRACKING,    // 正常跟踪
    DEGRADED,    // 降级（发送上次目标+置信度0.5）
    BLIND_FIRE,  // 盲射（惯性推算+置信度0.3）
    RESET        // 重置（发送无效标志）
};

class RobustVisionSystem {
private:
    VisionState state = VisionState::TRACKING;
    int consecutive_fail = 0;
    OutputData last_valid;
    
public:
    OutputData update(const cv::Mat& frame) {
        auto result = detect(frame);
        
        if (result.confidence > 0.6) {
            consecutive_fail = 0;
            last_valid = result;
            state = VisionState::TRACKING;
            return result;
        }
        
        consecutive_fail++;
        
        if (consecutive_fail < 3) {
            state = VisionState::DEGRADED;
            return {last_valid, 0.5};
        } else if (consecutive_fail < 10) {
            state = VisionState::BLIND_FIRE;
            return {predictByInertia(), 0.3};
        } else {
            state = VisionState::RESET;
            return {.is_valid = false};  // 发送无效标志
        }
    }
};
```

---

### 3. 自动计算算法延迟

**问题：** 如何自动计算算法的平均识别延迟？

**C++实现：**

```cpp
class LatencyProfiler {
public:
    void profile(const std::string& video_path,
                 std::function<cv::Point3d(const cv::Mat&)> algo) {
        cv::VideoCapture cap(video_path);
        std::vector<double> latencies;
        
        cv::Mat frame;
        while (cap.read(frame)) {
            auto start = std::chrono::high_resolution_clock::now();
            algo(frame);
            auto end = std::chrono::high_resolution_clock::now();
            
            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            latencies.push_back(ms);
        }
        
        // 统计
        std::sort(latencies.begin(), latencies.end());
        printf("Mean: %.2f ms | P99: %.2f ms | Max: %.2f ms\n",
               std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size(),
               latencies[latencies.size() * 99 / 100],
               latencies.back());
    }
};
```

---

## 四、C++特有加分项

| 考察点 | 问题 | 优秀答案 |
|:---|:---|:---|
| 智能指针 | 多模块共享图像用什么？ | `std::shared_ptr<cv::Mat>` |
| 移动语义 | 如何避免函数返回大图像的开销？ | 依赖RVO，或用输出参数 |
| 模板 | 写通用坐标系转换器？ | `template<typename T> Point3<T> transform(...)` |
| 内存池 | 频繁分配小对象导致碎片？ | 实现 `ObjectPool<T>` |
| 多线程 | 识别和发送线程如何共享数据？ | `std::mutex` + `std::atomic` 或无锁队列 |

---

## 五、水平判定标准

| 等级 | 标准 |
|:---|:---|
| **不合格** | 基础题错2道以上；不懂`const`、引用、指针区别 |
| **执行层** | 基础题全对，能用OpenCV完成识别，代码有内存隐患 |
| **核心研发** | 基础+核心全对，懂RAII、智能指针、移动语义，有异常处理 |
| **下任组长** | 全部答出，能设计模块化架构，会写单元测试和性能profiling |

---

1. 将本文档分发给队员作为复习材料
2. 考核时可从中选取3-5题，结合实操（给一段视频让成员写代码处理）
3. 答辩环节追问"为什么这样设计"而非"代码怎么写"
