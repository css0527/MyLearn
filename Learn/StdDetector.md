## Path 
### 做了两层防护，git clone 到任何路径都无需改代码：

1. 编译时自动注入项目根目录（唯一需要关注的地方）
在 CMakeLists.txt:57-59 里加了一行，把 CMake 自己知道的源码目录 CMAKE_SOURCE_DIR 编译进程序：

target_compile_definitions(main PRIVATE STD_DETECTOR_ROOT="${CMAKE_SOURCE_DIR}")

这样 clone 到哪、在哪重新 cmake && make，路径就自动指向哪，一处都不用手动改。

2. 新增一个路径助手 tools/path.hpp

tools::projectRoot() — 返回项目根目录。优先用编译时注入的路径；如果它失效（比如把二进制拷到了别处），会从可执行文件位置（/proc/self/exe）向上查找包含 config/config.yaml 或 CMakeLists.txt 的目录，即你要求的「自己先查找项目所在路径」。
tools::projectPath("config/config.yaml") — 拼接相对路径。
tools::resolvePath(...) — 把字符串里的 ${PROJECT_ROOT} 占位符替换成实际根目录。
改动的文件
文件	                      改动
main.cpp:44	                 tools::projectPath("config/config.yaml")
main.cpp:80-81	             video_path 用 tools::resolvePath() 解析
detector/detector.cpp:16-17	 模型/label 路径用 projectPath()
config/config.yaml:76	     video_path: "${PROJECT_ROOT}/video/8radps.avi"
calibrate_handeye.cpp、
calibrate_robotworld_handeye.cpp、
capture.cpp	                 硬编码绝对路径改为相对 config/...（这些是独立标定工具，从项目根目录运行）

### 关于 config.yaml 里的数据路径
代码里的路径用 projectPath()，但 video_path 是 YAML 数据（不是代码），所以用了 ${PROJECT_ROOT} 占位符 + resolvePath() 解析。以后在 YAML 里写任何项目内路径都可以用这个占位符，运行时会自动替换。

## 整幅画面偏蓝
### 原因
hikrobot.cpp 里从来没设置过相机的 PixelFormat，所以相机一直用它的默认输出（海康彩色 USB 相机默认通常是 Bayer 原始数据）。代码拿到 Bayer 后用 OpenCV 自己做的去马赛克，但这里有两个问题叠加：

白平衡失效：BalanceWhiteAuto 是相机 ISP 才会执行的设置。原始 Bayer 数据里根本没有白平衡处理，所以软件解出来的图就是传感器原始色彩——在室内/LED 灯光下会明显偏蓝。
软件去马赛克还可能因为 Bayer 排列约定不一致导致 R/B 通道错位，进一步偏色。
改动
在 hikrobot.cpp:85-93 的相机初始化里，把像素格式强制成 BGR8，让相机 ISP 去完成去马赛克 + 白平衡，直接输出 BGR：

// 强制输出 BGR8：由相机 ISP 完成去马赛克与白平衡，避免拿到 Bayer 原始数据导致整幅画面偏蓝。
set_enum_value("PixelFormat", PixelType_Gvsp_BGR8_Packed);
代码里 PixelType_Gvsp_BGR8_Packed 的分支本来就只是直接包一层缓冲区，所以这条路径是现成的、也最省 CPU。

另外加了一行一次性日志，启动时会打印相机实际输出的像素类型，方便确认：

[info] Camera pixel type: 0x... (BGR8=0x...)
重新跑 ./main，看两件事：
画面颜色是否恢复正常（不再偏蓝）；
日志里 Camera pixel type 那一行——如果已经是 BGR8 的十六进制值，说明设置成功；如果它打印出来还是 Bayer 的某个值（比如 0x01080009 附近），说明这台相机的 PixelFormat 节点名/值不支持，需要改用 MV_CC_SetEnumValueByString 或换个格式（比如 RGB8）来设置。
如果还是偏蓝，继续定位（可能是需要设成 RGB8，或这台相机只支持 Bayer、需要加上正确的去马赛克模式映射）。


## 检测不到装甲板
### 角度需要相对于水平线进行归一化
 cv::Point2f diff = light1.center - light2.center;
  // 两个灯条中心连线相对水平线的夹角。水平时 atan2 约为 0 或 180 度，
  // 需归一化到 [0, 90]，否则水平装甲板会被误判成角度过大。
  float angle = std::abs(std::atan2(diff.y, diff.x)) * 180.0 / CV_PI;
  float tilt = std::min(angle, 180.0f - angle);
  if (tilt > params_.armor_params.max_angle) {
    return ArmorType::INVALID;
  }

## 调试
### main.cpp 加了 FPS 统计

  
## 延迟高（先检查相機是否插到USB3.0口！！！）
### 加入FPS 统计调试
主循环 main.cpp 每帧做的事，按开销排序大致是：

读帧 —— 相机模式下 camera->read() 会阻塞在队列 pop（队列大小是 1，见 hikrobot.cpp），实际帧率被「相机真实出帧率」和「主循环处理速度」两者里较慢的那个卡住。
传统视觉 —— 每帧都要 threshold + findContours（在 1280×1024 全图上），几 ms。
cv::imshow + cv::waitKey(1) —— GUI 渲染，窗口越大越慢。
之前日志里能反推出来的实测帧率：

场景	                    帧率
视频（未检测到装甲板，早退）	~130 FPS
相机（未检测到装甲板）	       ~10 FPS
视频能到 130 FPS，说明检测和渲染不是主要瓶颈；相机只有 ~10 FPS，问题在相机出帧/读帧这条链路，而不是 LeNet。

我加了 FPS 统计
在 main.cpp 里加了每秒打印一次：
...
  auto fps_start = std::chrono::steady_clock::now();
  int fps_frames = 0;
  double detect_ms = 0.0;
...
    auto detect_start = std::chrono::steady_clock::now();
    auto armors = detector.detect(bgr_img);
    detect_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - detect_start)
        .count();
...

    // 每秒统计一次 FPS 与单帧检测耗时
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - fps_start).count();
    if (elapsed >= 1.0) {
      tools::logger()->info(
        "FPS: {:.1f} | detect: {:.2f} ms/frame", (frame_count - fps_frames) / elapsed, detect_ms);
      fps_start = now;
      fps_frames = frame_count;
    }

bash:
FPS: xx.x | detect: xx.xx ms/frame

如果 detect 只有几 ms、但 FPS 很低 → 瓶颈在读帧/相机（去查 MV_CC_SetFrameRate(handle_, 150) 是否真的生效，或相机实际帧率）；
如果 detect 有几十 ms → 瓶颈在传统视觉（可以加 ROI、降分辨率、缩小 findContours 范围）。

### 实测数据
指标	值
FPS	   ~10.2
detect 耗时	3~5 ms/帧（平均 ~3ms）
关键结论：瓶颈不是 LeNet，也不是检测，是相机。

detect 只要 3ms → 单纯检测能跑到 300+ FPS，LeNet 分类器包含在里面，完全不是问题。
视频模式下整条循环能到 ~130 FPS，说明 imshow/waitKey 也不慢。
相机模式下只有 10 FPS，而 detect 只占 3ms → 剩下的 ~90ms 全卡在 camera->read() 阻塞等帧上，也就是相机实际只在 ~10 FPS 出帧。
原因与修复
相机代码里设帧率用的是 hikrobot.cpp 里的：

MV_CC_SetFrameRate(handle_, 150);   // 返回值被忽略
MV_CC_SetFrameRate 在海康 SDK 里是已废弃接口（在 MvObsoleteInterfaces.h 里），而且返回值没检查，很可能静默失败，相机就跑在默认的 ~10 FPS。

我改成现代写法（hikrobot.cpp）：
// 设置帧率（MV_CC_SetFrameRate 已废弃，需先使能 AcquisitionFrameRateEnable 再设值）
  set_bool_value("AcquisitionFrameRateEnable", true);
  set_float_value("AcquisitionFrameRate", 150.0);

并新增了 set_bool_value，所有设值都会在失败时打 warning（之前是静默忽略）。
void HikRobot::set_bool_value(const std::string & name, bool value)
{
  unsigned int ret;

  ret = MV_CC_SetBoolValue(handle_, name.c_str(), value);

  if (ret != MV_OK) {
    tools::logger()->warn("MV_CC_SetBoolValue(\"{}\", {}) failed: {:#x}", name, value, ret);
    return;
  }
}
【检查】：
FPS: 那一行有没有从 ~10 涨上去；
启动时有没有新的 warning，比如 MV_CC_SetBoolValue("AcquisitionFrameRateEnable", ...) failed —— 如果打印了这个，说明节点名不对。
如果 FPS 还是 ~10，那基本可以排除软件了，大概率是硬件带宽问题：这台 MV-CS016-10UC 在 1280×1024 全分辨率下要跑 150 FPS 需要 USB 3.0，如果插在 USB 2.0 口上带宽不够，就会被卡在 ~10 FPS。确认一下相机是不是插在 USB3 口（蓝色接口）上，或者把分辨率/帧率目标降下来（比如 90 FPS 或换 640×480 的 ROI）。

日志里偶尔出现 ID: not_armor、dist=22~28m 的帧，是分类器把某些干扰误判成装甲板了（数字分类置信度虚高），这是另一个独立的问题，和帧率无关，需要调分类置信度阈值或加尺寸过滤（可选）。


## 匹配问题
问题在于：现在的流程已经能「匹配两根灯条 → 组成装甲板」，但分类后的过滤只做了两件事——置信度低于阈值就删、装甲板类型与数字类别不匹配（如大装甲板识别成 2）就删。没有把「灯条之间没识别到数字」的情况删掉，所以两根灯条中间没有数字也会被框出来。

我在 number_classifier.cpp:145-148 的 eraseIgnoreClasses 里加了一条过滤：

// 灯条之间没有识别到数字（负样本 / 未识别），不构成有效装甲板，不框选
if (armor.name == ArmorName::not_armor) {
  return true;
}
这样逻辑就变成了「两根灯条 + 中间识别到数字」才会框成装甲板：

label.txt 里除了 1/2/3/4/5/sentry/outpost/base 外还有 negative（负样本 = 无数字）；
classify() 里 negative 和任何未识别类都会落到 ArmorName::not_armor；
现在 not_armor 会被直接移除，只保留真正识别到有效数字的装甲板。
main.cpp 里画框的那段（main.cpp:185-192）不用动，它画的就是 detect() 返回后剩下的装甲板。

另外顺带发现一个已有的小问题（这次没动）：number_classifier.cpp:150-155 的 ignore_classes_ 判断比较的是 ARMOR_NAMES[armor.name]（"one"/"two"/…），而默认忽略列表是 {"negative"}，两者永远对不上，等于这段是死代码。现在 negative 已通过 not_armor 被过滤掉，功能上不影响；如果以后想用 ignore_classes 忽略别的类别，这块需要改成比较原始类别名。

## 神经网络
神经网络目前只干了一件事：给两个灯条中间的数字分类。真正受曝光/增益/二值化影响的是「找灯条」这一步，而它还是传统 CV。

现在的流水线

原图 → 颜色差二值化(R-B) → 找轮廓 → 灯条匹配 → 【NN 只在这：数字分类】 → 输出装甲板
        ↑ 曝光/增益/threshold 都影响这一步
detector.cpp 里 preprocessImage 用的是 channels[2] - channels[0] 再做固定阈值 threshold=180 二值化，灯条全靠轮廓 + 几何约束捞出来。这部分对光照、曝光、增益非常敏感——换个场地、换个灯光，threshold 和曝光就得重调。NN（lenet.onnx）只是最后对已经框好的装甲板里的数字做分类，救不了前面。

想「换环境零调参」，有三条路
1. 端到端目标检测（最彻底，推荐方向）
用 YOLO 之类的检测网络直接从原图输出「装甲板框 + 数字」或者「灯条对 + 数字」，彻底去掉二值化和手工阈值。对光照/曝光鲁棒性最好，是 RoboMaster 主流成熟方案。

代价：要重新标注数据集（原图上标装甲板/灯条），训练+部署成本高，板卡上推理延迟和算力要求比 LeNet 大。
2. 保留传统灯条检测，但把敏感环节做成自适应的（改动小，见效快）

相机上开 自动曝光/自动增益（或写个简单的 AE 闭环，别用固定 exposure_ms=2 / gain=16）；
二值化用 Otsu 自适应阈值 或对亮度先归一化，替代固定 threshold=180；
灯条颜色用 HSV 通道判断，比 R-B 差值对光照更稳。
这样能大幅减少换环境时的手动调参，但做不到 100% 零调参。
3. 混合：NN 也参与灯条检测
用一个小网络直接检测「灯条 ROI」，传统方法只做几何匹配和 PnP。比端到端省算力，比纯传统鲁棒。

如果目标是「换场地不用每次去现场调曝光和阈值」，先做第 2 条（自动曝光 + Otsu + HSV），半天就能改完，效果立刻有。等有标注数据了再往第 1 条端到端 YOLO 迁移——那才是真正意义上的「环境无关」。

tiny_resnet.onnx 现在没有任何代码引用它，之前「神经网络优化」尝试留下的，不用可以清理。

## 第二条
改了什么
1. 自动曝光/增益（相机侧）

hikrobot.cpp：auto_exposure=true 时用相机的 ExposureAuto=Continuous + GainAuto=Continuous，不再手动设 ExposureTime/Gain；false 时回退到原来的手动值。
camera.cpp、hikrobot.hpp：透传 auto_exposure 开关。
2. HSV 颜色分割（检测侧，替代 R-B 差值 + 固定阈值）

detector.cpp 的 preprocessImage：默认走 HSV inRange，红/蓝分开取色调段（红色 H 跨 0/180 两端，蓝色 100~130），用饱和度/亮度下限滤掉灰白背景和暗噪点。这样 threshold: 100 和 R-B 差值对光照的敏感问题就没了。
保留颜色差路径作为回退（use_hsv: false 时用）。
3. Otsu 自适应阈值

use_otsu: true 时：HSV 路径用 Otsu 在 V 通道自动找亮度阈值（并保留下限 hsv_v_min）；颜色差路径用 Otsu 替代固定 threshold。默认关，因为自动曝光 + HSV 通常已经够了。
4. 配置（config.yaml）新增，都有默认值、换环境一般不用动：

auto_exposure: true
use_hsv: true
use_otsu: false
hsv_s_min: 60
hsv_v_min: 60
效果与使用
换环境后大概率零调参：自动曝光负责把整体亮度拉到合理区间，HSV 负责稳定地抠出敌方颜色的灯条。如果遇到极暗/极亮场地灯条抠不出或噪点太多，才需要动 hsv_s_min/hsv_v_min 这两个（或者开 use_otsu），颜色本身（色调段）不用动。

两个提醒
当前 config.yaml 里 threshold 已被改成 100 了——现在默认走 HSV，这个值不再起作用，仅 use_hsv: false 时才用到。
自动曝光会让画面亮度逐帧微调，可能带来轻微闪烁，但 HSV 对亮度变化不敏感，灯条检测本身不受影响。如果后面觉得曝光在目标上乱跳影响 PnP，可以再改回手动（auto_exposure: false）并配合 Otsu。
可选：在 main.cpp 里加一个 h 键显示 HSV 掩码（现在只有 b 键看二值图），方便现场验证 HSV 抠得准不准

## 自适应方案的典型代价：
自适应方法更鲁棒，但在固定场景里往往不如手调参数精准。手调是对着当前场地一点点试出来的最优解，自动曝光/HSV 是「够用但不极致」。

先定位一下是「漏检」还是「误检」——这决定了往哪调。

最可能的元凶：自动曝光过曝
红色 LED 很亮。开自动曝光后，相机会把画面平均亮度拉到「正常」，结果灯条被过曝——红通道爆到 255 时 G、B 也一起涨，灯条从纯红变成偏白的粉红，饱和度 S 掉下来，然后就被 hsv_s_min: 60 给滤掉了。

而原来的 R - B > 100 只要求「红远大于蓝」，即使过曝一点 R 仍远大于 B，照样能抓到。所以对 LED 灯条来说，颜色差法其实比 HSV 的高饱和度门槛更抗过曝。

先做
把 config.yaml 里 auto_exposure 改回 false（保留调好的 exposure_ms: 2 / gain: 16），use_hsv 保持 true。
跑起来按 b 看二值图——现在 main.cpp 里的 b 键显示的就是 HSV 掩码（binary_img_ 已经是 inRange 的结果）。
看灯条在掩码里是白的（抠出来了）还是黑的（被滤掉了）。
如果 auto_exposure: false 后立马准了 → 就是自动曝光过曝的锅，保留手动曝光即可。
如果灯条是黑的 → 把 hsv_s_min / hsv_v_min 往下调（比如 40/40）再试。
如果灯条是白的但周围噪点很多 → 往上调，或把 use_otsu 开 true。
（兼顾「准」和「少调」）
既然手调已经准了，最省事的组合是：


auto_exposure: false    # 保留调好的曝光/增益（这是准的关键）
use_hsv: false          # 回到验证过的颜色差法
use_otsu: true          # 只把 threshold 从手调改成 Otsu 自适应
这样曝光不动（保持准）、颜色差法不动（保持准），只把最容易因环境变化的 threshold 交给 Otsu，换环境时理论上只需要让 Otsu 自己找阈值，不用手动改那个数字。等到了一个新环境发现还不行，再临时开 auto_exposure: true 过渡。

## 这个项目用了什么实现，效果如何？
`main.cpp` 主流程，混合「传统视觉 + 神经网络」：

1. **灯条提取** — 默认 HSV 颜色分割（`inRange` 按敌方颜色取红/蓝色调段，滤灰白背景与暗噪点），对光照/曝光变化更鲁棒；可回退到通道差二值化（红 R−B / 蓝 B−R，绕开红/蓝 LED 在灰度图上亮度低的问题）。配合相机自动曝光/增益 + Otsu 自适应阈值，实现换环境免调参
2. **灯条检测** — `findContours` + `minAreaRect` + 几何约束（宽长比、角度、颜色差）
3. **装甲板匹配** — 两根灯条按长度比 / 中心距 / 倾角配对，区分大小装甲板
4. **非极大值抑制（NMS）** — 一根灯条只属于一个装甲板，按置信度排序，抑制相邻装甲板灯条错配
5. **数字识别** — 透视变换拉正 + Otsu 二值化 + LeNet 分类 1~5 / 哨兵 / 前哨站 / 基地 / 负样本；只有「两根灯条 + 中间识别到有效数字」才保留为装甲板（负样本 / 未识别 / 类型不符均被过滤，不框选）
6. **PCA 角点矫正** — 对灯条 ROI 做 PCA 求对称轴，沿轴按亮度梯度精修角点
7. **PnP 位姿** — `solvePnPGeneric(IPPE)` 解出目标中心三维坐标与朝向；对 IPPE 的两个等价解按「法线朝向相机 + 上一帧 yaw 最近」消歧，消除 yaw/pitch/roll 符号跳变
8. **EKF 跟踪** — 6 维恒速模型（`[x, y, z, vx, vy, vz]`，相机系），球坐标测量 `[yaw, pitch, distance]`，估计目标位置与速度
9. **弹道补偿** — 理想模型（仅重力下坠），迭代解算飞行时间与瞄准角，输出命中所需的 `(yaw, pitch)`
## 效果

### 实测（海康相机，约 1m）

- **检测稳定**：连续识别无丢帧、无误检，置信度 1.00
- **实时性**：稳态约 80 FPS，单帧检测 3~5ms（首秒 0.8 FPS 为相机采集线程预热）
- **距离稳定**：`dist` 稳定在 0.74~0.85m，tvec 解算可靠

### 自适应 vs 手调

- **手调参数**（固定曝光/增益 + 通道差阈值）在单一固定场地最准，上表「实测」即手调结果
- **自适应模式**（`auto_exposure` + `use_hsv` + `use_otsu`）换环境免调曝光/二值化，但固定场景下精度略低于手调——追求「够用、省事」时开启，追求极致精度时回退手调

### 已修复的问题

| 问题 | 处理 |
|------|------|
| PnP 二义性导致 yaw/pitch/roll 正负跳变 | `solvePnPGeneric` 取双解 + 法线朝向 / 时间一致性消歧 |
| 相邻装甲板灯条跨框错配 | 灯条唯一性 NMS + 收紧 `max_armor_ratio` / `min_confidence` |
| 多个装甲板只框出一个 | 循环绘制所有装甲板 |
| 预测点反向 / 过远 | 废弃退化的卡尔曼，换 EKF 跟踪 + 弹道补偿 |

### 已知局限

- **尚未闭环**：算出的 aim 角只显示 / 画图，`SerialBoard::send` 未接入 main，未真正控制云台
- **弹道仅重力**：无空气阻力（resistance / rk4 / ceres 未做），远距离下坠与减速偏差大
- **枪口 = 相机光心假设**：`t_camera2gimbal` / 枪-相机安装偏移未补偿
- **相机系跟踪**：云台转动未用 IMU 补偿，恒速假设在云台运动时会退化
- 其它：`pnp_optimizer.cpp`（三分法位姿优化）为死代码；`use_roi` 未在预处理中使用；`detailed_test` 编译产物与 `.avi` 被提交进 git
