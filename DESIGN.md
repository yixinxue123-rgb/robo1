# RoboMaster 装甲板识别程序设计思路

## 一、项目背景

RoboMaster 机器人大赛要求机器人能够自动识别敌方装甲板并进行打击。装甲板由两个竖直的发光灯条组成，根据队伍颜色分为红色和蓝色。本程序实现了基于计算机视觉的装甲板自动识别算法。

## 二、总体设计

### 2.1 技术路线

采用经典的**图像处理 + 特征匹配**方案：

```
原始图像 → 颜色分割 → 轮廓提取 → 灯条筛选 → 灯条配对 → 装甲板识别
```

### 2.2 核心技术

- **OpenCV 4.13** - 计算机视觉库
- **C++ 17** - 编程语言
- **HSV 颜色空间** - 颜色分割
- **形态学处理** - 图像降噪
- **椭圆拟合** - 灯条提取
- **几何约束** - 装甲板配对

## 三、算法流程详解

### 3.1 颜色分割（Color Segmentation）

**目的**：从图像中提取特定颜色（红色或蓝色）的区域

**方法**：
1. 将 BGR 图像转换到 HSV 颜色空间
2. 使用 `cv::inRange()` 进行颜色阈值分割
3. HSV 阈值设定：
   - **蓝色**：H[100-130], S[80-255], V[80-255]
   - **红色**：H[0-10]∪[170-180], S[80-255], V[80-255]

**核心代码**：
```cpp
cv::Mat extractColor(const cv::Mat& img, bool detect_blue, const Config& cfg) {
    cv::Mat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    
    cv::Mat mask;
    if (detect_blue) {
        cv::inRange(hsv, cfg.blue_low, cfg.blue_high, mask);
    } else {
        // 红色跨越 0 度，需要两段
        cv::Mat mask1, mask2;
        cv::inRange(hsv, cfg.red_low1, cfg.red_high1, mask1);
        cv::inRange(hsv, cfg.red_low2, cfg.red_high2, mask2);
        mask = mask1 | mask2;
    }
    return mask;
}
```

**优点**：
- HSV 对光照变化不敏感
- 颜色阈值易于调整
- 计算效率高

### 3.2 形态学处理（Morphological Operations）

**目的**：去除噪点，填充空洞，改善分割效果

**方法**：
1. **闭运算（Close）**：先膨胀后腐蚀，填充小孔洞
2. **开运算（Open）**：先腐蚀后膨胀，去除小噪点

```cpp
cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, {5, 5});
cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);  // 填充
cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);   // 去噪
```

**效果**：
- 消除椒盐噪声
- 连接断裂的灯条
- 使轮廓更平滑

### 3.3 灯条提取（Light Bar Detection）

**目的**：从二值图像中提取符合灯条特征的轮廓

**步骤**：

#### 3.3.1 轮廓检测
```cpp
std::vector<std::vector<cv::Point>> contours;
cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
```

#### 3.3.2 椭圆拟合
对每个轮廓使用 `cv::fitEllipse()` 拟合旋转矩形

#### 3.3.3 灯条筛选条件

| 条件 | 阈值 | 说明 |
|------|------|------|
| 面积 | [10, 10000] | 过滤过小/过大的区域 |
| 长宽比 | [1.5, 25] | 灯条是细长形状 |
| 倾斜角 | ≤ 40° | 灯条应接近竖直 |

```cpp
float aspect = lb.length / lb.width;
if (aspect < cfg.min_ratio || aspect > cfg.max_ratio) continue;

float tilt = std::abs(lb.angle - 90.f);  // 90°为竖直
if (tilt > cfg.max_angle) continue;
```

**数据结构**：
```cpp
struct LightBar {
    cv::RotatedRect rect;    // 旋转矩形
    cv::Point2f top, bottom; // 上下端点
    cv::Point2f center;      // 中心点
    float angle;             // 角度
    float length, width;     // 长度、宽度
    float area;              // 面积
};
```

### 3.4 灯条配对（Light Bar Matching）

**目的**：将两个灯条配对组成装甲板

**配对约束条件**：

#### 3.4.1 角度一致性
```cpp
float angle_diff = std::abs(left.angle - right.angle);
if (angle_diff > 25°) reject;  // 两灯条应平行
```

#### 3.4.2 高度相似性
```cpp
float height_diff = |left.length - right.length| / avg_length;
if (height_diff > 0.6) reject;  // 高度差不超过60%
```

#### 3.4.3 水平距离合理性
```cpp
float x_ratio = horizontal_distance / avg_length;
if (x_ratio < 0.2 || x_ratio > 6.0) reject;  // 距离合理范围
```

#### 3.4.4 垂直偏移限制
```cpp
float y_ratio = vertical_offset / avg_length;
if (y_ratio > 4.0) reject;  // 中心点应在同一水平线
```

#### 3.4.5 装甲板比例检查
```cpp
float armor_ratio = width / height;
if (armor_ratio < 0.8 || armor_ratio > 6.0) reject;
```

**配对算法**：
```cpp
for (i = 0; i < bars.size(); i++) {
    for (j = i+1; j < bars.size(); j++) {
        if (满足所有约束条件) {
            创建装甲板对象
            计算置信度
            加入结果列表
        }
    }
}
```

**时间复杂度**：O(n²)，n 为灯条数量（通常 n < 20）

### 3.5 置信度评分（Confidence Scoring）

**目的**：评估装甲板识别的可靠性

**评分因子**：

1. **角度一致性**：`1.0 - angle_diff / max_angle_diff`
2. **高度一致性**：`1.0 - height_diff / max_height_diff`
3. **垂直对齐度**：`1.0 - y_diff / max_y_diff`

```cpp
float confidence = (angle_score + height_score + y_score) / 3.0;
```

**用途**：
- 多目标场景下选择最优目标
- 过滤低质量检测结果
- 提供给后续跟踪模块参考

### 3.6 装甲板分类（Armor Classification）

**分类依据**：装甲板宽高比

```cpp
armor.type = (armor_ratio > 3.0) ? LARGE : SMALL;
```

- **小装甲板**：比例约 2:1，常见于步兵、英雄机器人
- **大装甲板**：比例约 4:1，常见于哨兵、基地

## 四、参数配置策略

### 4.1 参数设计原则

1. **宽松的颜色阈值**：适应不同光照条件
2. **严格的形状约束**：减少误检
3. **灵活的配对条件**：适应装甲板倾斜、远近变化

### 4.2 关键参数调优

| 参数类别 | 关键参数 | 调优建议 |
|---------|---------|---------|
| 颜色分割 | HSV范围 | 在实际场地采样调整 |
| 灯条筛选 | 长宽比 | 根据相机距离调整 |
| 灯条配对 | 距离范围 | 根据相机视场角调整 |

## 五、性能优化

### 5.1 算法优化

1. **早期拒绝策略**：不满足条件立即跳过
2. **结果排序**：按置信度降序，优先处理高质量目标
3. **ROI 处理**：仅处理感兴趣区域（可选）

### 5.2 计算性能

- **处理时间**：~4-5ms（640x480 图像）
- **帧率**：>200 FPS（理论值）
- **实时性**：满足 RoboMaster 比赛要求

```cpp
auto start = cv::getTickCount();
// ... 处理流程 ...
auto end = cv::getTickCount();
double time_ms = (end - start) * 1000.0 / cv::getTickFrequency();
```

## 六、数据结构设计

### 6.1 配置结构
```cpp
struct Config {
    // HSV 颜色阈值
    cv::Scalar blue_low, blue_high;
    cv::Scalar red_low1, red_high1, red_low2, red_high2;
    
    // 灯条筛选参数
    float min_area, max_area;
    float min_ratio, max_ratio;
    float max_angle;
    
    // 装甲板配对参数
    float max_angle_diff, max_height_diff;
    float min_x_dist, max_x_dist, max_y_diff;
    float min_armor_ratio, max_armor_ratio;
};
```

### 6.2 装甲板结构
```cpp
struct Armor {
    LightBar left, right;        // 左右灯条
    cv::RotatedRect armor_rect;  // 装甲板外接矩形
    cv::Point2f center;          // 中心点
    float confidence;            // 置信度 [0, 1]
    int type;                    // 0=小装甲板, 1=大装甲板
};
```

## 七、可视化与调试

### 7.1 可视化输出

1. **mask.jpg**：颜色分割结果（二值图）
2. **result.jpg**：检测结果标注图
   - 绿色框：所有检测到的灯条
   - 红色框：配对成功的装甲板
   - 蓝色点：灯条端点
   - 黄色文字：装甲板信息（编号、类型、置信度）

### 7.2 调试信息

```
======== RoboMaster Armor Detector ========
Image: test.png
Light bars detected: 5
Armors detected: 2
Processing time: 4.6 ms

Detailed Results:
  Armor #1: Small armor, Confidence: 95%, Center: (320, 240)
  Armor #2: Large armor, Confidence: 87%, Center: (450, 280)
===========================================
```

## 八、系统架构

```
┌─────────────────────────────────────────┐
│           图像输入 (Camera/File)         │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│         预处理 (BGR → HSV)               │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│      颜色分割 (Color Segmentation)       │
│   - HSV阈值分割                          │
│   - 形态学处理                           │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│      轮廓提取 (Contour Detection)        │
│   - findContours                         │
│   - 椭圆拟合                             │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│      灯条筛选 (Light Bar Filtering)      │
│   - 面积过滤                             │
│   - 长宽比过滤                           │
│   - 角度过滤                             │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│      灯条配对 (Light Bar Matching)       │
│   - 角度一致性                           │
│   - 高度相似性                           │
│   - 距离合理性                           │
│   - 置信度评分                           │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│      结果输出 (Result Output)            │
│   - 装甲板位置                           │
│   - 装甲板类型                           │
│   - 置信度评分                           │
│   - 可视化结果                           │
└─────────────────────────────────────────┘
```

## 九、关键技术点

### 9.1 HSV vs RGB

**为什么选择 HSV？**
- **H (色调)**：表示颜色类型，对光照不敏感
- **S (饱和度)**：表示颜色纯度
- **V (明度)**：表示亮度

RGB 受光照影响大，同一物体在不同光照下 RGB 值变化明显。

### 9.2 椭圆拟合 vs 最小外接矩形

```cpp
cv::fitEllipse()     // 更贴合灯条实际形状
cv::minAreaRect()    // 可能包含多余区域
```

灯条发光后边缘模糊，椭圆拟合效果更好。

### 9.3 角度归一化

```cpp
// OpenCV RotatedRect angle 范围 [-90, 0)
// 归一化为 [0, 180)，竖直方向为 90°
if (width > height) {
    swap(width, height);
    angle += 90;
}
```

## 十、扩展方向

### 10.1 功能扩展

1. **多目标跟踪**：卡尔曼滤波 + 匈牙利算法
2. **深度学习**：YOLO/SSD 端到端检测
3. **3D 定位**：PnP 算法估计装甲板空间位置
4. **自适应阈值**：根据环境自动调整参数

### 10.2 性能优化

1. **GPU 加速**：OpenCV CUDA 模块
2. **多线程处理**：并行处理多个候选区域
3. **模型压缩**：量化、剪枝（深度学习方案）

### 10.3 鲁棒性提升

1. **光照自适应**：直方图均衡化
2. **运动模糊处理**：去模糊算法
3. **遮挡处理**：部分遮挡下的识别

## 十一、总结

本程序采用传统计算机视觉方法实现 RoboMaster 装甲板识别，具有以下特点：

✅ **高效**：处理时间 < 5ms，满足实时性要求  
✅ **准确**：多重约束条件，误检率低  
✅ **鲁棒**：适应不同光照、距离、角度  
✅ **可扩展**：模块化设计，易于优化改进  

**适用场景**：
- RoboMaster 比赛自动识别
- 机器人视觉系统开发
- 计算机视觉教学演示

**局限性**：
- 对光照条件有一定要求
- 复杂背景下可能误检
- 无法处理严重遮挡情况

**改进建议**：
- 结合深度学习提升鲁棒性
- 添加时序信息（跟踪）
- 引入深度相机获取3D信息
