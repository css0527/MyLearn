下面按「整体方案 → 实现流程 → 推障机构精妙性 → 视觉算法创新 → 导航避障创新 → 工程化与答辩要点」完整梳理。

---

## 一、项目总体方案

这是一个 **ROS 2 Humble + Gazebo Classic 11 的无人车灾害救援仿真闭环**，对应《B09 仿真技术赛道-无人车仿真组》赛题：在一座 3 m × 3 m 的厂区中，无人车需依次完成——

1. **走廊巡检**：识别 5 类彩色灾害标识（FIRE / CHEMICAL / RADIATION / ELECTRICAL / BIOHAZARD）；
2. **入口定位**：导航至核心区 0.5 m 宽的狭窄入口；
3. **物理推障**：推动堵在入口的 30 cm 可动障碍物；
4. **核心区标定**：识别 4 个 AprilTag 36h11 标识并换算到 `map` 坐标系发布 Marker；
5. **自主返航**：回到起点，任务状态机收敛到 `complete`。

系统分层架构：

| 层 | 组件 | 关键文件 |
|---|---|---|
| 仿真层 | Gazebo 世界、物理参数、机器人 SDF/URDF | [rescue_world.world](src/avs_rescue_sim/worlds/rescue_world.world)、[model.sdf](src/avs_rescue_sim/models/rescue_rover/model.sdf)、[rescue_rover.urdf](src/avs_rescue_sim/urdf/rescue_rover.urdf) |
| 感知层 | 视觉识别 + AprilTag 定位 | [vision_node.py](src/avs_rescue_sim/avs_rescue_sim/vision_node.py) |
| 决策层 | YAML 驱动的任务状态机 | [mission_node.py](src/avs_rescue_sim/avs_rescue_sim/mission_node.py) |
| 导航层 | Nav2（AMCL + NavFn A* + DWB） | [nav_params.yaml](src/avs_rescue_sim/config/nav_params.yaml) |
| 配置层 | 全部参数外置 YAML | [config/](src/avs_rescue_sim/config/) |
| 工具层 | 确定性纹理/地图生成 | [generate_assets.py](src/avs_rescue_sim/tools/generate_assets.py) |

---

## 二、实现流程（数据流闭环）

### 1. 环境与物理建模

世界在 [rescue_world.world](src/avs_rescue_sim/worlds/rescue_world.world) 中严格按图纸搭建：四周围墙（3.1 m 边）、核心区西墙 + 北侧两段墙，在 `x=1.10~1.25` 处留出 **0.5 m 入口**。物理引擎用 ODE，显式设置了 `max_step_size=0.002`、`iters=80`、接触约束 `cfm/erp/contact_max_correcting_vel/contact_surface_layer`（[第 4-7 行](src/avs_rescue_sim/worlds/rescue_world.world#L4-L7)）——这一步是推障**不穿模、不抖动**的物理基础。

关键的可动障碍物 `push_barrier`（[第 28-34 行](src/avs_rescue_sim/worlds/rescue_world.world#L28-L34)）：
- `static=false`，质量 2 kg，非静态刚体；
- 配置了 `velocity_decay`（线性 0.10 / 角向 0.20）模拟地面阻尼，防止被推后无限滑动；
- 接触参数 `kp=1000000 / kd=10 / min_depth=0.001 / max_vel=0.1`，保证推撞时既不过度穿透又能及时响应。

### 2. 机器人模型与传感器

[rescue_rover/model.sdf](src/avs_rescue_sim/models/rescue_rover/model.sdf) 是差速底盘：双主动轮 + 后置球形万向轮（`slip1=slip2=1` 模拟自由转向）。三个传感器全部通过 Gazebo ROS 插件接入：
- **激光雷达**：360 点、8 m 量程、15 Hz、0.003 m 高斯噪声（[第 54 行](src/avs_rescue_sim/models/rescue_rover/model.sdf#L54)）；
- **RGB 相机**：640×480、15 Hz，发布 `camera_info` 供 PnP 使用（[第 62 行](src/avs_rescue_sim/models/rescue_rover/model.sdf#L62)）；
- **差速驱动插件**：发布 `/odom` 和 `odom→base_link` TF（[第 66-69 行](src/avs_rescue_sim/models/rescue_rover/model.sdf#L66-L69)）。

### 3. 视觉感知流程（vision_node）

[vision_node.py](src/avs_rescue_sim/avs_rescue_sim/vision_node.py) 收到图像后，主回调里做两条独立管线（`image_callback` → `detect_hazards` + `detect_tags`）：

**灾害分类管线**（`detect_hazards`，[第 119-143 行](src/avs_rescue_sim/avs_rescue_sim/vision_node.py#L119-L143)）：
```
BGR → HSV → inRange(五组阈值) → 底部 14% 掩膜剔除车体
    → 5×5 开运算去噪 + 闭运算填充 → 找最大连通域 → 面积阈值 900px
    → 首次命中即发布 JSON 事件 + 终端打印 [CORRIDOR n/5]
```

**AprilTag 标定管线**（`detect_tags`，[第 145-188 行](src/avs_rescue_sim/avs_rescue_sim/vision_node.py#L145-L188)）：
```
灰度图 → ArucoDetector(36h11, 亚像素角点细化) 
    → solvePnP(IPPE_SQUARE) 求相机系位姿
    → TF 查询 map←camera 变换 → 相机坐标旋转平移到 map
    → 指数平滑(α=0.35) → 发布彩色立方体 + 文字 Marker
```

两条管线最终都汇入同一个事件话题 `/hazards/detections`（JSON：`kind=corridor_hazard|core_hazard`），供任务状态机订阅。

### 4. 任务状态机（mission_node）—— 全流程编排

[mission_node.py](src/avs_rescue_sim/avs_rescue_sim/mission_node.py) 的核心是一个 **50 ms tick 的有限状态机**，步骤表完全由 YAML 驱动：

```
step_names:    [corridor_fire, …, corridor_biohazard, barrier_stage, clear_barrier, 
                core_tag_0..3, exit_core, return_start]
step_types:    [corridor×5, navigate, push, core×4, navigate, return]
step_expected: [FIRE…BIOHAZARD, '', '', '0'..'3', '', '']
```

状态流转（`tick`，[第 104-126 行](src/avs_rescue_sim/avs_rescue_sim/mission_node.py#L104-L126)）：

1. **startup**：等 7 s 让仿真和 Nav2 就绪，并轮询 `navigate_to_pose` Action 服务可用；
2. 每个 corridor/core 步骤：`send_navigation_goal` 发 `NavigateToPose` → 到达后进入 **observing**，等待视觉确认对应 `expected` 标签；超时（5 s）自动放行，保证流程不卡死；
3. **push** 步骤：不经过 Nav2，直接走闭环推障（见下节）；
4. 所有步骤走完后 `advance_step` 统计漏检项并 `report('complete', …)` 收尾。

每一步状态都通过 `/mission/status` 以 JSON 发布，并同步发布一条 `LINE_STRIP` 路线 Marker（`publish_route`）在 RViz 里可视化规划路线。

### 5. 导航与返航

[Nav2](src/avs_rescue_sim/config/nav_params.yaml) 采用 **AMCL（似然场模型，2000 粒子）** 做定位、**NavFn A\* 全局规划器**（`use_astar=true`）、**DWB 局部规划器**（7 个 critics）、激光代价地图 + 恢复行为。返航就是状态机最后给 `return_start` 目标（起点 0.50, 0.35, yaw=1.5708），无需额外逻辑。

---

## 三、推障机构的精妙性

这是本方案最出彩的机械设计。车体前端不是一块平推板，而是一个 **「黄色横梁 + 两侧导流翼」的 V 形被动推板**：

- [rescue_rover.urdf](src/avs_rescue_sim/urdf/rescue_rover.urdf) 第 19-20 行定义了 `pusher_link`（横梁 0.16×0.36×0.10，前置 0.29 m）；
- [model.sdf](src/avs_rescue_sim/models/rescue_rover/model.sdf) 第 39-48 行展开为三个碰撞体：
  - **中央横梁** `crossbar`（0.14×0.34×0.10，摩擦 μ=1.3）；
  - **左翼/右翼** 各一块 0.18×0.055×0.10 的板，绕竖轴偏转 **±0.45 rad（约 ±26°）**。

**精妙之处在于三点**：

1. **正撞 → 自动居中**：当障碍物与推板不是正对、而是一侧先接触时，V 形斜翼会把横向接触力分解成「向前的推力 + 指向中心线的侧向力」，把障碍物自然「导流」到横梁中央，避免单侧受力导致障碍物旋转、卡在 0.5 m 窄入口的门框上。这是纯被动结构（fixed joint，无电机、无控制），靠几何形状就实现了主动对中——**用零成本机械结构换掉了复杂的主动对中算法**。

2. **推面摩擦专门增强**：推板横梁摩擦 μ 设为 1.3（车体 0.7、障碍物 0.35），保证推动时是「推走」而不是「滑脱」。

3. **配合闭环推障控制**（[mission_node.py](src/avs_rescue_sim/avs_rescue_sim/mission_node.py) 第 200-227 行）：进入 `push` 后读取里程计记录起点，以 `push_speed=0.22 m/s` 持续发 `cmd_vel`，用 `math.hypot` 实时累计位移，达到 `push_distance=0.78 m` 立即停车，另有 `push_timeout=8 s` 兜底。**关键设计是「基于里程计距离的闭环」，而非固定 sleep 时间**——这直接避免了固定延时方案的两种经典翻车：障碍物太硬导致 `sleep` 期间车顶在障碍物上打滑（过推/偏航），或障碍物太轻导致 `sleep` 没结束障碍物已飞远（穿模）。推完还有 1 s `push_settling` 让物理稳定再进入下一步。

> 一句话概括：**几何（V 形翼）负责「推得正」，物理（接触参数）负责「推得稳」，里程计闭环（距离判据）负责「推得准」**。

---

## 四、视觉算法的创新点

1. **「识别」与「标定」统一成一个 ROS 事件流**：`/hazards/detections` 同时承载 `corridor_hazard`（类别标签）和 `core_hazard`（AprilTag id + map 坐标），终端打印、RViz Marker、任务状态机确认**共享同一份检测结果**——单点检测、多点消费，避免了「分类节点」和「标定节点」各自维护一套状态导致的时序不一致。这在答辩时是一个很能体现系统设计功力的点。

2. **HSV 去噪 + 最大连通域筛选**（[第 121-131 行](src/avs_rescue_sim/avs_rescue_sim/vision_node.py#L121-L131)）：开运算去孤立噪点、闭运算填内部孔洞，再取 `max(contours, key=area)` 只保留最大连通域，从根本上抑制小色块误报；再叠加 **底部 14% 掩膜**（`mask[int(h*0.86):,:]=0`）主动剔除相机拍到车体/推板（黄色）造成的自污染——这是个很容易被忽略但实际很关键的细节。

3. **AprilTag 亚像素角点 + IPPE_SQUARE**（[第 66-67、162-164 行](src/avs_rescue_sim/avs_rescue_sim/vision_node.py#L162-L164)）：`CORNER_REFINE_SUBPIX` 把角点精度推到亚像素级；`SOLVEPNP_IPPE_SQUARE` 利用方形标签几何约束求位姿，比通用 `ITERATIVE` 更稳、更快，还能天然剔除退化解（`tvec[2]<=0` 时跳过）。

4. **指数平滑抑制 RViz 乱飘**（[第 172-179 行](src/avs_rescue_sim/avs_rescue_sim/vision_node.py#L172-L179)）：每个 tag 的 map 坐标做 `stable = α·map + (1-α)·prev`（α=0.35）一阶低通，让 Marker 在单帧 PnP/TF 抖动下依然稳定落点，同时保留首次检测日志（`[CORE] AprilTag n …`）用于验收。

5. **不依赖 cv_bridge 的轻量解码**：`decode_image`（[第 90-102 行](src/avs_rescue_sim/avs_rescue_sim/vision_node.py#L90-L102)）直接用 `np.frombuffer` 处理 `step` 行跨距，并手写四元数向量旋转（`rotate_vector`，[第 18-22 行](src/avs_rescue_sim/avs_rescue_sim/vision_node.py#L18-L22)）替代 tf 变换库——减少依赖、可控性强。

---

## 五、导航避障算法的创新点

1. **针对窄入口/可动障碍物定制参数**：全局用 `NavfnPlanner(use_astar=true, allow_unknown=false)`，局部 `DWB` 配齐 7 个 critics（`RotateToGoal / Oscillation / BaseObstacle / GoalAlign / PathAlign / PathDist / GoalDist`），目标容差收到 `xy=0.08 m / yaw=0.12 rad`（[nav_params.yaml](src/avs_rescue_sim/config/nav_params.yaml) 第 142-146 行），保证能精准对准 0.5 m 入口。`Oscillation` critic 专门抑制窄口反复打摆。

2. **「推障」与「避障」分离，各司其职**：这是方案里最聪明的导航设计。可动障碍物在激光里是「障碍物」，若让 Nav2 去绕它会被挡在入口外、且它不在静态地图里导致规划失败。所以状态机对 `push` 步骤**主动绕过 Nav2**，直接以开环 `cmd_vel` 直线推进，把「该推的时候推」和「该绕的时候绕」解耦：Nav2 只负责「该绕的静态墙」，推障模块只负责「该推的障碍物」。代价地图里 `voxel_layer`（局部）用 3D 体素 + `max_obstacle_height=2.0` 过滤地面噪声，`robot_radius=0.19 + inflation_radius=0.25` 留出安全包络。

3. **定位精度兜底**：AMCL 用 `likelihood_field` 似然场模型（比 beam model 对遮挡/动态障碍更鲁棒）、2000 粒子、`pf_err=0.05 / pf_z=0.99` 维持收敛，配合 `initial_pose` 预置保证起点已知——在障碍物被推走后激光地图动态变化时，似然场模型能更快重新收敛。

---

## 六、工程化亮点（答辩加分项）

- **全参数外置 YAML**：路线点、推障阈值、HSV 范围、导航参数全部在 `config/` 下，改现场无需改源码（README 与 [mission_params.yaml](src/avs_rescue_sim/config/mission_params.yaml) 明确说明）。
- **确定性可复现**：[generate_assets.py](src/avs_rescue_sim/tools/generate_assets.py) 用 OpenCV 程序化生成 5 张灾害纹理、4 张 AprilTag、以及 `rescue_map.pgm`（用 `world_to_pixel` 把墙体坐标精确画进栅格图），全程无手工位图，保证「地图—世界—识别阈值」三方严格对齐。
- **一键启动 + 独立建图**：[start.sh](start.sh) 一键 `colcon build + launch`；[start_mapping.sh](start_mapping.sh) + [slam.yaml](src/avs_rescue_sim/config/slam.yaml) 用 slam_toolbox 在线建图，[save_map.sh](save_map.sh) 落盘——形成「演示导航用预置图 / 现场可重扫」的双通道。
- **鲁棒的状态机**：导航失败自动重试、观察超时自动放行、推障超时兜底，任何单步失败都不会让整个演示卡死。

---

## 七、一句话总结（可直接用于答辩开场）

> 本方案以 **V 形被动导流推板 + 里程计闭环推障** 解决窄入口可动障碍物的「推得正、推得稳、推得准」，以 **HSV 色彩分割 + AprilTag PnP/TF 标定统一成单一事件流** 解决「识别与定位一致性」，以 **Nav2 全局导航与专用推障模块解耦** 解决「该绕则绕、该推则推」，全流程由 YAML 驱动、一键启动、确定性复现。

把这份内容整理成 **答辩 PPT 逐页大纲**，或画一张**系统架构图 / 状态机时序图**（Mermaid）
