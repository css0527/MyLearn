/**
 * @file CameraWrapper.hpp
 * @brief RAII 风格的相机包装器，自动管理相机资源
 * 
 * 学习目标：
 * 1. RAII 资源管理
 * 2. 构造函数/析构函数
 * 3. 移动语义
 * 4. 异常安全
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>

namespace camera {

/**
 * @brief 相机状态枚举
 */
enum class CameraState {
    CLOSED,      // 未打开
    OPENED,      // 已打开
    STREAMING,   // 正在流式传输
    ERROR        // 错误状态
};

/**
 * @brief CameraWrapper - RAII 风格的相机管理类
 * 
 * 特点：
 * - 构造函数自动打开相机
 * - 析构函数自动释放相机
 * - 禁止拷贝，支持移动
 * - 线程安全
 */
class CameraWrapper {
public:
    /**
     * @brief 构造函数 - RAII 获取资源
     * @param camera_id 相机ID（0, 1, 2... 或视频文件路径）
     * @param api_preference 相机API偏好（cv::CAP_ANY, cv::CAP_V4L2 等）
     */
    explicit CameraWrapper(int camera_id = 0, int api_preference = cv::CAP_ANY);
    
    /**
     * @brief 构造函数 - 从视频文件打开
     * @param video_path 视频文件路径
     */
    explicit CameraWrapper(const std::string& video_path);
    
    /**
     * @brief 析构函数 - RAII 释放资源
     * 
     * 自动调用 release() 释放相机资源
     * 即使发生异常也会被调用（栈展开）
     */
    ~CameraWrapper();
    
    // ============================================
    // 禁止拷贝（复制）语义
    // ============================================
    CameraWrapper(const CameraWrapper&) = delete;
    CameraWrapper& operator=(const CameraWrapper&) = delete;
    
    // ============================================
    // 支持移动语义（高效转移所有权）
    // ============================================
    CameraWrapper(CameraWrapper&& other) noexcept;
    CameraWrapper& operator=(CameraWrapper&& other) noexcept;
    
    // ============================================
    // 公共接口
    // ============================================
    
    /**
     * @brief 捕获一帧图像
     * @param frame 输出参数，存储捕获的图像
     * @return true 成功，false 失败
     */
    bool grab(cv::Mat& frame);
    
    /**
     * @brief 获取相机状态
     */
    CameraState getState() const { return state_; }
    
    /**
     * @brief 获取相机名称
     */
    std::string getCameraName() const { return camera_name_; }
    
    /**
     * @brief 获取相机参数
     */
    double getProperty(int prop_id) const;
    
    /**
     * @brief 设置相机参数
     */
    bool setProperty(int prop_id, double value);
    
    /**
     * @brief 检查相机是否已打开
     */
    bool isOpened() const { return state_ != CameraState::CLOSED; }
    
    /**
     * @brief 获取相机信息（格式化字符串）
     */
    std::string getInfo() const;

private:
    /**
     * @brief 初始化相机
     */
    void initialize();
    
    /**
     * @brief 释放相机资源
     */
    void release();
    
    /**
     * @brief 更新状态
     */
    void updateState();
    
    // ============================================
    // 成员变量
    // ============================================
    
    cv::VideoCapture cap_;           // OpenCV 视频捕获对象
    CameraState state_;              // 当前状态
    std::string camera_name_;        // 相机名称
    mutable std::mutex mutex_;       // 互斥锁（线程安全）
    
    // 统计信息
    size_t frame_count_;             // 捕获帧数
    double total_grab_time_;         // 总捕获时间
};

/**
 * @brief 智能指针别名（推荐使用）
 */
using CameraWrapperPtr = std::shared_ptr<CameraWrapper>;
using CameraWrapperUniquePtr = std::unique_ptr<CameraWrapper>;

} // namespace camera
