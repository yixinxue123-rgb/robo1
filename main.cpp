#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>

// -------- 参数配置 --------
struct Config {
    // 颜色阈值 (HSV)
    // 蓝色灯条 - 针对 RoboMaster 蓝色装甲板优化
    cv::Scalar blue_low  = {100, 100, 100};
    cv::Scalar blue_high = {124, 255, 255};
    
    // 红色灯条（红色在HSV中跨0度，需要两段）
    cv::Scalar red_low1  = {0,   100, 100};
    cv::Scalar red_high1 = {10,  255, 255};
    cv::Scalar red_low2  = {156, 100, 100};
    cv::Scalar red_high2 = {180, 255, 255};

    // 灯条筛选 - RoboMaster 标准装甲板尺寸约为 125mm x 55mm
    float min_area        = 10.f;     // 最小面积
    float max_area        = 5000.f;   // 最大面积
    float min_ratio       = 1.5f;     // 最小长宽比 (高/宽)
    float max_ratio       = 20.f;     // 最大长宽比
    float max_angle       = 30.f;     // 灯条允许的最大倾斜角 (度)

    // 灯条配对 - 更严格的条件
    float max_angle_diff  = 15.f;     // 两灯条角度差 (度)
    float max_height_diff = 0.4f;     // 两灯条高度差比例
    float min_x_dist      = 0.5f;     // 最小水平距离 / 平均灯条高度
    float max_x_dist      = 4.0f;     // 最大水平距离 / 平均灯条高度
    float max_y_diff      = 2.0f;     // 最大垂直偏移 / 平均灯条高度
    
    // 装甲板比例限制 (宽/高)
    float min_armor_ratio = 1.2f;     // 最小装甲板宽高比
    float max_armor_ratio = 4.5f;     // 最大装甲板宽高比
};

// -------- 灯条结构 --------
struct LightBar {
    cv::RotatedRect rect;
    cv::Point2f top, bottom, center;
    float angle;   // 与竖直方向的夹角
    float length;  // 长轴长度
    float width;   // 短轴宽度
    float area;
};

// -------- 装甲板结构 --------
struct ArmorResult {
    LightBar left, right;
    cv::RotatedRect armor_rect;
    cv::Point2f center;
    float confidence;  // 置信度评分
    
    // 计算置信度
    void calcConfidence() {
        // 基于多个因素评分
        float angle_score = 1.0f - std::abs(left.angle - right.angle) / 30.0f;
        float height_score = 1.0f - std::abs(left.length - right.length) / std::max(left.length, right.length);
        float y_score = 1.0f - std::abs(left.center.y - right.center.y) / std::max(left.length, right.length);
        
        confidence = (angle_score + height_score + y_score) / 3.0f;
        confidence = std::max(0.0f, std::min(1.0f, confidence));
    }
};

// -------- 将RotatedRect归一化为"竖直"方向 --------
LightBar toLightBar(const cv::RotatedRect& r) {
    LightBar lb;
    lb.rect = r;
    lb.center = r.center;
    lb.area = r.size.area();

    // OpenCV RotatedRect angle: [-90, 0)
    float w = r.size.width, h = r.size.height;
    float angle = r.angle;
    
    // 让长轴始终对应"高度"
    if (w > h) {
        std::swap(w, h);
        angle += 90.f;
    }
    
    lb.length = h;
    lb.width = w;
    lb.angle = angle;

    // 计算顶点（上下端点）
    float rad = angle * CV_PI / 180.f;
    cv::Point2f dir(std::cos(rad), std::sin(rad));
    lb.top    = lb.center - dir * (h / 2.f);
    lb.bottom = lb.center + dir * (h / 2.f);
    
    // 保证top在上方（y更小）
    if (lb.top.y > lb.bottom.y) 
        std::swap(lb.top, lb.bottom);

    return lb;
}

// -------- 颜色提取 --------
cv::Mat extractColor(const cv::Mat& hsv, bool detect_blue, const Config& cfg) {
    cv::Mat mask;
    if (detect_blue) {
        cv::inRange(hsv, cfg.blue_low, cfg.blue_high, mask);
    } else {
        cv::Mat mask1, mask2;
        cv::inRange(hsv, cfg.red_low1, cfg.red_high1, mask1);
        cv::inRange(hsv, cfg.red_low2, cfg.red_high2, mask2);
        mask = mask1 | mask2;
    }
    // 形态学处理，填充空洞
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, {3, 3});
    cv::dilate(mask, mask, kernel, {-1,-1}, 2);
    cv::erode (mask, mask, kernel, {-1,-1}, 1);
    return mask;
}

// -------- 提取灯条 --------
std::vector<LightBar> findLightBars(const cv::Mat& mask, const Config& cfg) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LightBar> bars;
    for (auto& c : contours) {
        float area = cv::contourArea(c);
        if (area < cfg.min_area || area > cfg.max_area) continue;
        if (c.size() < 5) continue;

        cv::RotatedRect r = cv::fitEllipse(c);
        LightBar lb = toLightBar(r);

        // 长宽比筛选
        // 直接用minRect长宽比
        float minW = std::min(r.size.width, r.size.height);
        float maxH = std::max(r.size.width, r.size.height);
        float aspect = maxH / std::max(minW, 1.f);
        if (aspect < cfg.min_ratio || aspect > cfg.max_ratio) continue;

        // 倾斜角筛选：灯条应接近竖直，angle接近90度
        float tilt = std::abs(lb.angle - 90.f);
        if (tilt > cfg.max_angle) continue;

        bars.push_back(lb);
    }
    return bars;
}

// -------- 灯条配对 --------
std::vector<ArmorResult> matchLightBars(const std::vector<LightBar>& bars, const Config& cfg) {
    std::vector<ArmorResult> armors;

    for (size_t i = 0; i < bars.size(); ++i) {
        for (size_t j = i + 1; j < bars.size(); ++j) {
            const LightBar& a = bars[i];
            const LightBar& b = bars[j];

            // 1. 角度差
            float angle_diff = std::abs(a.angle - b.angle);
            if (angle_diff > cfg.max_angle_diff) continue;

            // 2. 高度差比例
            float avg_len   = (a.length + b.length) / 2.f;
            float len_diff  = std::abs(a.length - b.length) / avg_len;
            if (len_diff > cfg.max_height_diff) continue;

            // 3. 水平距离
            float dx        = std::abs(a.center.x - b.center.x);
            float x_ratio   = dx / avg_len;
            if (x_ratio < cfg.min_x_dist || x_ratio > cfg.max_x_dist) continue;

            // 4. 确认左右
            const LightBar& left  = (a.center.x < b.center.x) ? a : b;
            const LightBar& right = (a.center.x < b.center.x) ? b : a;

            // 构造装甲板外接矩形（四顶点）
            std::vector<cv::Point2f> pts = {left.top, left.bottom, right.bottom, right.top};
            cv::RotatedRect armor_rect = cv::minAreaRect(pts);

            armors.push_back({left, right, armor_rect});
        }
    }
    return armors;
}

// -------- 主函数 --------
int main(int argc, char** argv) {
    std::string src = (argc > 1) ? argv[1] : "armor.jpg";
    cv::Mat img = cv::imread(src);
    if (img.empty()) {
        std::cerr << "Cannot open image: " << src << std::endl;
        return -1;
    }

    Config cfg;
    bool detect_blue = true; // true=识别蓝色灯条, false=红色

    cv::Mat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

    // 1. 颜色提取
    cv::Mat mask = extractColor(hsv, detect_blue, cfg);

    // 2. 灯条检测
    std::vector<LightBar> bars = findLightBars(mask, cfg);

    // 3. 灯条配对
    std::vector<ArmorResult> armors = matchLightBars(bars, cfg);

    // -------- 可视化 --------
    cv::Mat vis = img.clone();

    // 画所有灯条（绿色）
    for (auto& lb : bars) {
        cv::Point2f pts[4];
        lb.rect.points(pts);
        for (int k = 0; k < 4; k++)
            cv::line(vis, pts[k], pts[(k+1)%4], {0, 255, 0}, 1);
        cv::circle(vis, lb.center, 3, {0,255,0}, -1);
    }

    // 画装甲板（红色框 + 角点）
    for (auto& ar : armors) {
        cv::Point2f pts[4];
        ar.armor_rect.points(pts);
        for (int k = 0; k < 4; k++)
            cv::line(vis, pts[k], pts[(k+1)%4], {0, 0, 255}, 2);

        // 画灯条端点
        cv::circle(vis, ar.left.top,    4, {255, 0, 0}, -1);
        cv::circle(vis, ar.left.bottom, 4, {255, 0, 0}, -1);
        cv::circle(vis, ar.right.top,   4, {255, 0, 0}, -1);
        cv::circle(vis, ar.right.bottom,4, {255, 0, 0}, -1);

        cv::putText(vis, "Armor", ar.armor_rect.center,
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, {0,0,255}, 2);
    }

    std::cout << "Light bars: " << bars.size()
              << "  Armors: "   << armors.size() << std::endl;

    // 保存结果图片
    cv::imwrite("result.jpg", vis);
    cv::imwrite("mask.jpg", mask);
    std::cout << "Results saved to result.jpg and mask.jpg" << std::endl;
    
    // 如果需要显示窗口，取消注释下面两行
    // cv::imshow("mask", mask);
    // cv::imshow("result", vis);
    // cv::waitKey(0);
    return 0;
}
