# robo1
识别装甲板
#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <iomanip>

// RoboMaster 装甲板识别程序 - 优化版 v2.0

struct Config {
    // 颜色阈值 (HSV) - 放宽阈值以适应更多场景
    cv::Scalar blue_low  = {90, 50, 50};     // 降低饱和度和明度下限
    cv::Scalar blue_high = {130, 255, 255};
    
    cv::Scalar red_low1  = {0,   50, 50};    // 降低饱和度和明度下限
    cv::Scalar red_high1 = {10,  255, 255};
    cv::Scalar red_low2  = {170, 50, 50};
    cv::Scalar red_high2 = {180, 255, 255};

    // 灯条筛选参数 - 更宽松的范围
    float min_area = 10.f;
    float max_area = 10000.f;              // 增大最大面积
    float min_ratio = 1.2f;                // 降低最小长宽比
    float max_ratio = 30.f;                // 增大最大长宽比
    float max_angle = 40.f;                // 增大允许的倾斜角

    // 装甲板配对参数 - 更灵活的匹配
    float max_angle_diff = 25.f;           // 增大角度差容忍
    float max_height_diff = 0.6f;          // 增大高度差容忍
    float min_x_dist = 0.2f;               // 降低最小距离
    float max_x_dist = 6.0f;               // 增大最大距离
    float max_y_diff = 4.0f;               // 增大垂直偏移容忍
    
    float min_armor_ratio = 0.8f;          // 降低最小比例
    float max_armor_ratio = 6.0f;          // 增大最大比例
};

struct LightBar {
    cv::RotatedRect rect;
    cv::Point2f top, bottom, center;
    float angle, length, width, area;
};

struct Armor {
    LightBar left, right;
    cv::RotatedRect armor_rect;
    cv::Point2f center;
    float confidence;
    int type; // 0=小装甲板, 1=大装甲板
};

LightBar toLightBar(const cv::RotatedRect& r) {
    LightBar lb;
    lb.rect = r;
    lb.center = r.center;
    lb.area = r.size.area();

    float w = r.size.width, h = r.size.height;
    float angle = r.angle;
    
    if (w > h) {
        std::swap(w, h);
        angle += 90.f;
    }
    
    lb.length = h;
    lb.width = w;
    lb.angle = angle;

    float rad = angle * CV_PI / 180.f;
    cv::Point2f dir(std::cos(rad), std::sin(rad));
    lb.top = lb.center - dir * (h / 2.f);
    lb.bottom = lb.center + dir * (h / 2.f);
    
    if (lb.top.y > lb.bottom.y) 
        std::swap(lb.top, lb.bottom);

    return lb;
}

cv::Mat extractColor(const cv::Mat& img, bool detect_blue, const Config& cfg) {
    cv::Mat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    
    cv::Mat mask;
    if (detect_blue) {
        cv::inRange(hsv, cfg.blue_low, cfg.blue_high, mask);
    } else {
        cv::Mat mask1, mask2;
        cv::inRange(hsv, cfg.red_low1, cfg.red_high1, mask1);
        cv::inRange(hsv, cfg.red_low2, cfg.red_high2, mask2);
        mask = mask1 | mask2;
    }
    
    // 优化的形态学处理
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, {3, 3});
    cv::dilate(mask, mask, kernel, cv::Point(-1,-1), 2);  // 膨胀2次
    cv::erode(mask, mask, kernel, cv::Point(-1,-1), 1);   // 腐蚀1次
    
    return mask;
}

std::vector<LightBar> findLightBars(const cv::Mat& mask, const Config& cfg) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LightBar> bars;
    for (const auto& c : contours) {
        if (c.size() < 5) continue;
        
        float area = cv::contourArea(c);
        if (area < cfg.min_area || area > cfg.max_area) continue;

        cv::RotatedRect r = cv::fitEllipse(c);
        LightBar lb = toLightBar(r);

        float aspect = lb.length / std::max(lb.width, 0.1f);
        if (aspect < cfg.min_ratio || aspect > cfg.max_ratio) continue;

        float tilt = std::abs(lb.angle - 90.f);
        if (tilt > cfg.max_angle) continue;

        bars.push_back(lb);
    }
    return bars;
}

std::vector<Armor> matchLightBars(const std::vector<LightBar>& bars, const Config& cfg) {
    std::vector<Armor> armors;

    for (size_t i = 0; i < bars.size(); ++i) {
        for (size_t j = i + 1; j < bars.size(); ++j) {
            const LightBar& a = bars[i];
            const LightBar& b = bars[j];

            // 1. 角度差
            float angle_diff = std::abs(a.angle - b.angle);
            if (angle_diff > cfg.max_angle_diff) continue;

            // 2. 高度差
            float avg_len = (a.length + b.length) / 2.f;
            float len_diff = std::abs(a.length - b.length) / avg_len;
            if (len_diff > cfg.max_height_diff) continue;

            // 3. 水平距离
            float dx = std::abs(a.center.x - b.center.x);
            float x_ratio = dx / avg_len;
            if (x_ratio < cfg.min_x_dist || x_ratio > cfg.max_x_dist) continue;

            // 4. 垂直偏移
            float dy = std::abs(a.center.y - b.center.y);
            float y_ratio = dy / avg_len;
            if (y_ratio > cfg.max_y_diff) continue;

            // 5. 装甲板比例
            float armor_width = dx;
            float armor_height = avg_len;
            float armor_ratio = armor_width / armor_height;
            if (armor_ratio < cfg.min_armor_ratio || armor_ratio > cfg.max_armor_ratio) continue;

            // 构造装甲板
            const LightBar& left = (a.center.x < b.center.x) ? a : b;
            const LightBar& right = (a.center.x < b.center.x) ? b : a;

            std::vector<cv::Point2f> pts = {left.top, left.bottom, right.bottom, right.top};
            cv::RotatedRect armor_rect = cv::minAreaRect(pts);

            Armor armor;
            armor.left = left;
            armor.right = right;
            armor.armor_rect = armor_rect;
            armor.center = armor_rect.center;
            
            // 计算置信度
            float angle_score = 1.0f - angle_diff / cfg.max_angle_diff;
            float height_score = 1.0f - len_diff / cfg.max_height_diff;
            float y_score = 1.0f - y_ratio / cfg.max_y_diff;
            armor.confidence = (angle_score + height_score + y_score) / 3.0f;
            
            // 判断装甲板类型（大/小）
            armor.type = (armor_ratio > 3.0f) ? 1 : 0;

            armors.push_back(armor);
        }
    }
    
    // 按置信度排序
    std::sort(armors.begin(), armors.end(), 
              [](const Armor& a, const Armor& b) { return a.confidence > b.confidence; });
    
    return armors;
}

int main(int argc, char** argv) {
    std::string src = (argc > 1) ? argv[1] : "armor.jpg";
    cv::Mat img = cv::imread(src);
    if (img.empty()) {
        std::cerr << "Error: Cannot open image: " << src << std::endl;
        return -1;
    }

    Config cfg;
    bool detect_blue = true; // true=蓝色, false=红色
    
    auto start = cv::getTickCount();

    // 1. 颜色提取
    cv::Mat mask = extractColor(img, detect_blue, cfg);

    // 2. 灯条检测
    std::vector<LightBar> bars = findLightBars(mask, cfg);

    // 3. 灯条配对
    std::vector<Armor> armors = matchLightBars(bars, cfg);

    auto end = cv::getTickCount();
    double time_ms = (end - start) * 1000.0 / cv::getTickFrequency();

    // 可视化
    cv::Mat vis = img.clone();

    // 画灯条（绿色）
    for (const auto& lb : bars) {
        cv::Point2f pts[4];
        lb.rect.points(pts);
        for (int k = 0; k < 4; k++)
            cv::line(vis, pts[k], pts[(k+1)%4], {0, 255, 0}, 2);
    }

    // 画装甲板（红色框 + 信息）
    for (size_t i = 0; i < armors.size(); ++i) {
        const auto& ar = armors[i];
        
        cv::Point2f pts[4];
        ar.armor_rect.points(pts);
        for (int k = 0; k < 4; k++)
            cv::line(vis, pts[k], pts[(k+1)%4], {0, 0, 255}, 3);

        // 画灯条端点
        cv::circle(vis, ar.left.top, 5, {255, 0, 0}, -1);
        cv::circle(vis, ar.left.bottom, 5, {255, 0, 0}, -1);
        cv::circle(vis, ar.right.top, 5, {255, 0, 0}, -1);
        cv::circle(vis, ar.right.bottom, 5, {255, 0, 0}, -1);

        // 显示信息
        std::string info = "Armor #" + std::to_string(i+1) + 
                          (ar.type == 1 ? " [L]" : " [S]") +
                          " " + std::to_string(int(ar.confidence * 100)) + "%";
        cv::putText(vis, info, cv::Point(ar.center.x - 60, ar.center.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 255, 255}, 2);
    }

    // 显示统计信息
    std::cout << "======== RoboMaster Armor Detector (优化版) ========" << std::endl;
    std::cout << "Image: " << src << " (" << img.cols << "x" << img.rows << ")" << std::endl;
    std::cout << "Light bars detected: " << bars.size() << std::endl;
    std::cout << "Armors detected: " << armors.size() << std::endl;
    std::cout << "Processing time: " << std::fixed << std::setprecision(2) << time_ms << " ms" << std::endl;
    std::cout << "FPS: " << std::fixed << std::setprecision(1) << (1000.0 / time_ms) << std::endl;
    
    if (!armors.empty()) {
        std::cout << "\n装甲板检测详情:" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        for (size_t i = 0; i < armors.size(); ++i) {
            const auto& ar = armors[i];
            std::cout << "  [" << (i+1) << "] ";
            std::cout << (ar.type == 1 ? "大装甲板" : "小装甲板");
            std::cout << " | 置信度: " << std::fixed << std::setprecision(1) << (ar.confidence * 100) << "%";
            std::cout << " | 中心: (" << std::setprecision(0) << ar.center.x << ", " << ar.center.y << ")";
            std::cout << " | 宽度: " << std::setprecision(1) << ar.armor_rect.size.width << "px";
            std::cout << std::endl;
        }
        std::cout << std::string(50, '-') << std::endl;
    } else {
        std::cout << "\n⚠ 未检测到装甲板" << std::endl;
        std::cout << "建议: 检查图片中是否有明显的蓝色或红色灯条" << std::endl;
    }
    std::cout << "====================================================" << std::endl;

    // 保存结果
    cv::imwrite("result.jpg", vis);
    cv::imwrite("mask.jpg", mask);
    std::cout << "\nResults saved to result.jpg and mask.jpg" << std::endl;
    
    return 0;
}
