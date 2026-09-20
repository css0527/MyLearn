
## “起点卡死”和“核心区入口过窄”
### 需要修改的参数清单
（局部代价地图2处 + 全局代价地图2处）
参数位置   参数名   当前值   建议修改值   修改目的
local_costmap > ros__parameters   robot_radius   0.19   0.16   匹配真实车宽0.32m，解决起点碰撞判定

local_costmap > inflation_layer   inflation_radius   0.25   0.20   缩小膨胀圈，确保0.5m入口可通过

global_costmap > ros__parameters   robot_radius   0.19   0.16   保持全局规划与局部控制一致

global_costmap > inflation_layer   inflation_radius   0.25   0.20   避免全局A*在起点/窄口直接规划失败

⚠️ 关键提示： local_costmap 和 global_costmap 中的这两组参数必须同步修改。如果只改局部不改全局，会出现“全局规划器认为能过、局部控制器认为不能过”的矛盾，导致车辆在窄口前反复震荡或急停。

### 为什么要这样改？

问题一：起点卡死 (No valid trajectories)
现状分析： 车辆中心在 y=0.35，南墙内沿在 y=0.05，实际净空仅 0.30m。但当前配置 robot_radius: 0.19 意味着Nav2认为车体直径是 0.38m。
后果： 车辆一启动，代价地图就判定“机器人本体已与墙壁重叠”，DWB的 BaseObstacle 评分器对所有419条轨迹都打出碰撞惩罚，导致 No valid trajectories。
修改依据： 真实车宽0.32m → 半径应为 0.16m。此时所需净空为 0.32m，小于实际净空0.30m+安全余量，车辆不再被误判为“出生即碰撞”。

问题二：核心区0.5m入口无法通过
现状分析： 在Nav2膨胀模型中，通道可通行的最小宽度 ≈ 2 × robot_radius + 2 × (inflation_radius - robot_radius) × decay_factor。简化估算下，当前配置要求的最小通道宽度约为 0.76m（远超0.5m）。
修改依据： 将 robot_radius 降至0.16、inflation_radius 降至0.20后，理论最小通行宽度降至约 0.48~0.50m，刚好满足0.5m入口的通行需求，同时保留了约4cm的安全膨胀余量。

### 参数含义详解
参数   含义   对导航的影响
robot_radius   机器人圆形足迹的半径（单位：米）。Nav2用此值将机器人近似为一个圆，用于碰撞检测。   直接决定“车有多大”。设大了→窄路过不去、起点误碰撞；设小了→真实车体可能刮擦障碍物。必须≤真实车体最大外接圆半径。

inflation_radius   障碍物膨胀层的半径（单位：米）。从障碍物边界向外扩展的距离，在此范围内代价值从253衰减到0。   决定“离障碍物多远开始减速/避让”。设大了→安全但通行能力差；设小了→通行能力强但碰撞风险高。必须≥robot_radius，否则机器人会直接撞上障碍物。

cost_scaling_factor   膨胀层代价值的衰减速率因子（当前值3.0）。公式：cost = 253 × e^(-factor × (dist - robot_radius))   值越大→代价衰减越快，机器人更“大胆”地靠近障碍物；值越小→衰减越慢，机器人更“保守”。本次未建议修改，因为3.0已是较合理的默认值，优先调半径即可解决问题。

### 补充检查项
验证真实车体尺寸： 请务必实测车辆最外侧突出物（如雷达支架、防撞条）的最大外接圆直径。若实测大于0.32m，则 robot_radius 应按实测值设定，此时0.5m入口可能需要物理拓宽或接受“不可通行”。
观察膨胀层可视化： 修改后在RViz中订阅 /global_costmap/costmap 和 /local_costmap/costmap 话题，确认：
    起点位置车辆footprint不再与墙体红色区域重叠
    0.5m入口处膨胀后的灰色/绿色区域留有连续的可通行白色通道


## 已应用的改动

| 文件 | 改动 | 效果 |
|---|---|---|
| [nav_params.yaml](src/avs_rescue_sim/config/nav_params.yaml) | robot_radius 0.12、inflation 0.20（上一轮） | 车能规划了（碰撞报错已消失） |
| [model.sdf](src/avs_rescue_sim/models/rescue_rover/model.sdf) | 车体 0.32→0.24、轮距 0.364→0.24、推板横梁 x 0.285→0.22 / 翼 x 0.34→0.26、推板宽 0.34→0.32 | 物理车变窄变短 |
| [rescue_rover.urdf](src/avs_rescue_sim/urdf/rescue_rover.urdf) | 同步车体/轮/推板尺寸 | TF 与碰撞体一致，RViz 不错位 |

改完后的关键数字：

- **宽度**：轮子外缘 0.409 m → **0.285 m**，推板 0.34 → **0.32 m**（入口 0.5 m 单边余量从 4.5 cm 提升到 **9 cm**）；
- **推板前伸**：0.43 m → **0.35 m**（车在 `corridor_fire` 转朝西看标识时，推板尖端离西墙从「顶死」变成 **8 cm** 净空）。

推板的 V 形结构和推障功能没动，只是整体后移 0.06 m、收窄 0.02 m。

## 下一步

直接 `./start.sh`（脚本开头会自动 `colcon build`，把新的 SDF/URDF 拷进 install 目录）。这次重点看三件事：

1. 车是否从起点**向北开出去**（不再原地卡）；
2. 到 `(0.48, 0.85)` 后能否**转到位**，终端出现 `[CORRIDOR 1/5] detected FIRE`；
3. 之后一路巡检五个标识、推障、进核心区。

## 还挂在待办上的一个隐患

之前提过的**静态地图入口只有 ~0.40 m**（[generate_assets.py](src/avs_rescue_sim/tools/generate_assets.py) 里 `cv2.line` 圆头端点各伸 0.05 m 导致）。现在车变窄了（0.32 m），`4×0.12=0.48` 依然大于 0.40，所以**推完障碍进核心区那一步**可能还是会被静态地图卡一下，要靠激光 clearing 自愈。稳妥起见把 `make_map()` 改成 `cv2.rectangle` 精确画墙、把入口还原成真 0.5 m



##
- [rescue_rover.urdf (line 67)](/home/c/AVS/avs_ws/src/avs_rescue_sim/urdf/rescue_rover.urdf:67) 补齐底盘、轮子、尾轮和 V 形推板的 collision，推板外形与 Gazebo SDF 一致。
- [nav_params.yaml (line 202)](/home/c/AVS/avs_ws/src/avs_rescue_sim/config/nav_params.yaml:202) 将 robot_radius: 0.12 改为包含推板的六边形 footprint，Nav2 不再只按小圆形车体规划。
- [mission_node.py (line 232)](/home/c/AVS/avs_ws/src/avs_rescue_sim/avs_rescue_sim/mission_node.py:232) 将误导性的任务点直连线改为青色航点圆点；RViz 里绿色 /plan 是唯一的实时避障路线。
- RViz 显示名称改为 Mission Waypoints，README 已说明两者区别。
已通过 check_urdf、YAML/Python 校验、包构建和启动参数检查。重新执行 ./start.sh 即可看到更新。

README.md+2-0
src/avs_rescue_sim/avs_rescue_sim/mission_node.pysrc/avs_rescue_sim/avs_rescue_sim/mission_node.py+8-2
src/avs_rescue_sim/config/nav_params.yamlsrc/avs_rescue_sim/config/nav_params.yaml+6-2
src/avs_rescue_sim/rviz/rescue.rvizsrc/avs_rescue_sim/rviz/rescue.rviz+1-1
src/avs_rescue_sim/urdf/rescue_rover.urdfsrc/avs_rescue_sim/urdf/rescue_rover.urdf+116-11


