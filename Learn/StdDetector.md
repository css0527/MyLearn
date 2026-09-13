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




