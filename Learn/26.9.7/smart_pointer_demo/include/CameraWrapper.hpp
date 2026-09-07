/**
 * @file CameraWrapper.hpp
 * @brief 使用智能指针管理相机资源
 * 
 * 学习目标：
 * 1. 理解 unique_ptr 和 shared_ptr 的区别
 * 2. 在实战中使用智能指针
 * 3. 掌握所有权转移
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <iostream>
#include <memory>
#include <string>

namespace camera {

/**
 * @brief 相机状态
 */
enum class CameraState {
    CLOSED,
    OPENED,
    STREAMING,
    ERROR
};

/**
 * @brief CameraWrapper - 使用 RAII 管理相机
 * 
 * 注意：这个类本身使用 RAII，但我们可以用智能指针来管理它
 */
class CameraWrapper {
public:
    // ============================================
    // 构造函数（RAII 获取资源）
    // ============================================
    explicit CameraWrapper(int camera_id = 0);
    explicit CameraWrapper(const std::string& video_path);
    
    // ============================================
    // 析构函数（RAII 释放资源）
    // ============================================
    ~CameraWrapper();
    
    // ============================================
    // 禁止拷贝，支持移动
    // ============================================
    CameraWrapper(const CameraWrapper&) = delete;
    CameraWrapper& operator=(const CameraWrapper&) = delete;
    CameraWrapper(CameraWrapper&& other) noexcept;
    CameraWrapper& operator=(CameraWrapper&& other) noexcept;
    
    // ============================================
    // 公共接口
    // ============================================
    bool grab(cv::Mat& frame);
    bool isOpened() const { return state_ != CameraState::CLOSED; }
    CameraState getState() const { return state_; }
    std::string getInfo() const;
    
    // 获取相机ID
    int getCameraId() const { return camera_id_; }

private:
    void release();
    
    cv::VideoCapture cap_;
    CameraState state_;
    int camera_id_;
    std::string camera_name_;
    size_t frame_count_;
};

// ============================================
// 智能指针类型别名（推荐使用）
// ============================================

// 独占所有权 - 适用于单例、局部变量、类成员
using CameraPtr = std::unique_ptr<CameraWrapper>;

// 共享所有权 - 适用于多线程共享、回调、观察者模式
using CameraSharedPtr = std::shared_ptr<CameraWrapper>;

// 弱引用 - 避免循环引用
using CameraWeakPtr = std::weak_ptr<CameraWrapper>;

// 工厂函数（使用 make_unique/make_shared）
template<typename... Args>
CameraPtr make_camera(Args&&... args) {
    return std::make_unique<CameraWrapper>(std::forward<Args>(args)...);
}

template<typename... Args>
CameraSharedPtr make_shared_camera(Args&&... args) {
    return std::make_shared<CameraWrapper>(std::forward<Args>(args)...);
}

} // namespace camera
