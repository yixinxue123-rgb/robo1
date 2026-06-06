# RoboMaster 装甲板识别系统

基于 OpenCV 的 RoboMaster 机器人装甲板自动识别程序，采用经典计算机视觉算法实现灯条检测与装甲板配对。

## 项目结构

```
armor_detector/
├── main.cpp                    # 主程序（基础版）
├── main_robomaster.cpp         # 优化版程序（带置信度评分）
├── CMakeLists.txt              # CMake 构建配置
├── DESIGN.md                   # 详细设计文档
├── INSTALL.md                  # 环境安装指南
├── README.md                   # 本文件
├── 快速运行.bat                # 一键运行脚本
└── build/                      # 编译输出目录
    ├── armor_detector.exe      # 基础版可执行文件
    ├── armor_robomaster.exe    # 优化版可执行文件
    ├── result.jpg              # 检测结果图（运行后生成）
    └── mask.jpg                # 颜色分割图（运行后生成）
```

## 代码结构

### 核心数据结构

```cpp
// 配置参数
struct Config {
    cv::Scalar blue_low, blue_high;    // 蓝色 HSV 阈值
    cv::Scalar red_low1, red_high1;    // 红色 HSV 阈值（第一段）
    cv::Scalar red_low2, red_high2;    // 红色 HSV 阈值（第二段）
    float min_area, max_area;          // 灯条面积范围
    float min_ratio, max_ratio;        // 灯条长宽比范围
    float max_angle;                   // 灯条最大倾斜角
    float max_angle_diff;              // 配对角度差阈值
    float max_height_diff;             // 配对高度差阈值
    float min_x_dist, max_x_dist;      // 配对水平距离范围
};

// 灯条结构
struct LightBar {
    cv::RotatedRect rect;              // 旋转矩形
    cv::Point2f top, bottom, center;   // 上端点、下端点、中心点
    float angle;                       // 与竖直方向的夹角
    float length, width;               // 长度、宽度
    float area;                        // 面积
};

// 装甲板结构
struct ArmorResult {
    LightBar left, right;              // 左右灯条
    cv::RotatedRect armor_rect;        // 装甲板外接矩形
    cv::Point2f center;                // 中心点
    float confidence;                  // 置信度（仅优化版）
};
```

### 核心函数

```cpp
// 1. 颜色分割
cv::Mat extractColor(const cv::Mat& hsv, bool detect_blue, const Config& cfg);

// 2. 灯条提取
std::vector<LightBar> findLightBars(const cv::Mat& mask, const Config& cfg);

// 3. 灯条配对
std::vector<ArmorResult> matchLightBars(const std::vector<LightBar>& bars, const Config& cfg);

// 4. 辅助函数
LightBar toLightBar(const cv::RotatedRect& r);  // 旋转矩形转灯条
```

### 算法流程

```
原始图像
    ↓
BGR → HSV 转换
    ↓
颜色阈值分割 (cv::inRange)
    ↓
形态学处理 (膨胀/腐蚀)
    ↓
轮廓提取 (cv::findContours)
    ↓
椭圆拟合 (cv::fitEllipse)
    ↓
灯条筛选 (面积/长宽比/角度)
    ↓
灯条配对 (角度差/高度差/距离)
    ↓
装甲板输出 (位置/类型/置信度)
```

## 快速开始

### 方法一：使用批处理脚本（最简单）

双击 `快速运行.bat` 即可运行程序。

### 方法二：命令行运行

```cmd
# 1. 设置环境变量
set PATH=C:\msys64\mingw64\bin;%PATH%

# 2. 运行基础版
armor_detector\build\armor_detector.exe 图片.jpg

# 3. 运行优化版（带置信度评分）
armor_detector\build\armor_robomaster.exe 图片.jpg
```

### 方法三：VSCode 运行

1. 打开 `main.cpp` 或 `main_robomaster.cpp`
2. 按 `F5` 启动调试
3. 查看生成的 `result.jpg` 和 `mask.jpg`

## 编译说明

### 使用 g++ 直接编译

```cmd
# 基础版
g++ -std=c++17 -O2 main.cpp -o build/armor_detector.exe ^
    -IC:/msys64/mingw64/include/opencv4 ^
    -LC:/msys64/mingw64/lib ^
    -lopencv_core -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc

# 优化版
g++ -std=c++17 -O2 main_robomaster.cpp -o build/armor_robomaster.exe ^
    -IC:/msys64/mingw64/include/opencv4 ^
    -LC:/msys64/mingw64/lib ^
    -lopencv_core -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc
```

### 使用 CMake 编译

```cmd
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

## 参数配置

### HSV 颜色阈值

编辑 `main.cpp` 中的 `Config` 结构：

```cpp
struct Config {
    // 蓝色：H[100-130], S[80-255], V[80-255]
    cv::Scalar blue_low  = {100, 80, 80};
    cv::Scalar blue_high = {130, 255, 255};
    
    // 红色：H[0-10]∪[170-180], S[80-255], V[80-255]
    cv::Scalar red_low1  = {0,   80, 80};
    cv::Scalar red_high1 = {10,  255, 255};
    cv::Scalar red_low2  = {170, 80, 80};
    cv::Scalar red_high2 = {180, 255, 255};
};
```

### 灯条筛选参数

```cpp
float min_area = 10.f;          // 最小面积
float max_area = 10000.f;       // 最大面积
float min_ratio = 1.5f;         // 最小长宽比
float max_ratio = 25.f;         // 最大长宽比
float max_angle = 35.f;         // 最大倾斜角（度）
```

### 装甲板配对参数

```cpp
float max_angle_diff = 20.f;    // 两灯条最大角度差（度）
float max_height_diff = 0.5f;   // 两灯条最大高度差比例
float min_x_dist = 0.3f;        // 最小水平距离比例
float max_x_dist = 5.0f;        // 最大水平距离比例
float max_y_diff = 3.0f;        // 最大垂直偏移比例
```

## 输出说明

### 控制台输出

**基础版：**
```
Light bars: 16  Armors: 3
Results saved to result.jpg and mask.jpg
```

**优化版：**
```
======== RoboMaster Armor Detector ========
Image: test.png
Light bars detected: 16
Armors detected: 3
Processing time: 4.5 ms

Detailed Results:
  Armor #1: Small armor, Confidence: 95%, Center: (320, 240)
  Armor #2: Large armor, Confidence: 87%, Center: (450, 280)
  Armor #3: Small armor, Confidence: 82%, Center: (580, 300)
===========================================
```

### 生成的图像文件

1. **result.jpg** - 检测结果可视化
   - 🟢 绿色框：检测到的灯条
   - 🔴 红色框：识别出的装甲板
   - 🔵 蓝色点：灯条端点
   - 🟡 黄色文字：装甲板信息（编号、类型、置信度）

2. **mask.jpg** - 颜色分割结果
   - ⚪ 白色区域：检测到的目标颜色
   - ⚫ 黑色区域：背景

## 版本对比

| 特性 | 基础版 (main.cpp) | 优化版 (main_robomaster.cpp) |
|------|------------------|----------------------------|
| 灯条检测 | ✅ | ✅ |
| 装甲板配对 | ✅ | ✅ |
| 置信度评分 | ❌ | ✅ (0-100%) |
| 装甲板分类 | ❌ | ✅ (大/小) |
| 处理时间统计 | ❌ | ✅ |
| 详细结果输出 | ❌ | ✅ |
| HSV 阈值 | 宽松 | 严格 |
| 适用场景 | 测试调试 | 比赛实战 |

## 性能指标

- **处理速度**：3-5 ms (640×480 图像)
- **理论帧率**：200+ FPS
- **检测准确率**：取决于参数调优和环境条件
- **内存占用**：< 50 MB

## 技术栈

- **语言**：C++ 17
- **图像库**：OpenCV 4.13.0
- **编译器**：GCC 16.1.0 (MinGW-w64)
- **构建工具**：CMake 3.10+ / g++ 直接编译
- **平台**：Windows 10/11 (可移植到 Linux/macOS)

## 核心算法

### 1. 颜色分割 (HSV)

- 将 BGR 图像转换到 HSV 颜色空间
- 使用 `cv::inRange()` 进行阈值分割
- 形态学处理（闭运算+开运算）去噪

### 2. 灯条提取

- 轮廓检测：`cv::findContours()`
- 椭圆拟合：`cv::fitEllipse()`
- 特征筛选：面积、长宽比、倾斜角

### 3. 装甲板配对

- 双重循环遍历所有灯条对
- 几何约束：角度一致性、高度相似性、距离合理性
- 置信度评分（优化版）：综合多项指标

<img width="1124" height="960" alt="微信图片_20260606205020_734_8" src="https://github.com/user-attachments/assets/0799ae7c-34a4-43cc-b6fc-37b11037e149" />


## 常见问题

**Q: 检测不到装甲板？**  
A: 调整 HSV 颜色阈值，可能光照导致颜色偏移

**Q: 误检太多？**  
A: 提高灯条筛选条件（长宽比、面积）

**Q: 程序无法运行？**  
A: 检查 PATH 是否包含 `C:\msys64\mingw64\bin`

**Q: 编译失败？**  
A: 参考 `INSTALL.md` 安装 OpenCV 和编译环境

## 相关文档

- [DESIGN.md](DESIGN.md) - 详细设计文档
- [INSTALL.md](INSTALL.md) - 环境安装指南
- [CMakeLists.txt](CMakeLists.txt) - CMake 构建配置

## 开发计划

- [ ] 添加深度学习检测（YOLO）
- [ ] 实现多目标跟踪（卡尔曼滤波）
- [ ] 3D 位姿估计（PnP 算法）
- [ ] 自适应参数调整
- [ ] GPU 加速优化

## 许可证

本项目仅供学习和研究使用。

## 作者

yixin xue

---

**最后更新**：2026-06-03
