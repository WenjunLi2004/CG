# Final 机械臂实验室说明

## 构建与运行
- 依赖：GLFW、GLAD、GLM（与前序实验一致，vcpkg 可自动找到）。
- 构建：  
  ```bash
  cd Final
  cmake -B build
  cmake --build build
  ./build/FinalArmLab
  ```
- 资源与着色器已在 `CMakeLists.txt` 中自动复制到构建目录。

## 键鼠交互
- `1 / 2` 基座旋转；`3 / 4` 下臂俯仰；`5 / 6` 上臂俯仰；`7 / 8` 手爪开合；`R` 关节归位。
- 相机（沿用前序实验）：`U/I/O`（+Shift 反向）绕行/仰角/距离；`SPACE` 重置相机；`H` 查看帮助；`ESC/Q` 退出。

## 场景要点
- 封闭实验室：金属格栅地面、混凝土墙，聚光灯位于 `(0, 2.5, 0)` 指向 `(0, -1, 0)`，内外切角 15°/20°，具备衰减与阴影贴图（1024²，3x3 PCF）。
- 机械臂四层层级：基座 → 下臂 → 上臂 → 手爪（左右爪）。变换链路在 `gatherDrawItems()` 中逐层构建，展示父子矩阵传递。
- 纹理资源：`assets/textures/metal_grate.ppm`（地面）、`assets/textures/concrete_wall.ppm`（墙体）；可替换为更高分辨率贴图，保持同名即可。
