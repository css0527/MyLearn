/**
 * @file CameraWrapper.cpp
 * @brief CameraWrapper 类的实现
 */

#include "CameraWrapper.hpp"
#include <chrono>
#include <sstream>

namespace camera {

// ============================================
// 构造函数 - RAII 获取资源
// ============================================

CameraWrapper::CameraWrapper(int camera_id, int api_preference)
    : state_(CameraState::CLOSED)
    , camera_name_("Camera_" + std::to_string(camera_id))
    , frame_count_(0)
    , total_grab_time_(0.0) {
    
    std::cout << "📷 CameraWrapper: Opening camera " << camera_id << std::endl;
    
    try {
        // 打开相机（资源获取）
        cap_.open(camera_id, api_preference);
        
        if (cap_.isOpened()) {
            state_ = CameraState::OPENED;
            std::cout << "✅ Camera " << camera_id << " opened successfully!" << std::endl;
            
            // 打印相机信息
            std::cout << getInfo() << std::endl;
        } else {
            state_ = CameraState::ERROR;
            std::cerr << "❌ Failed to open camera " << camera_id << std::endl;
            throw std::runtime_error("Failed to open camera: " + std::to_string(camera_id));
        }
        
    } catch (const std::exception& e) {
        state_ = CameraState::ERROR;
        std::cerr << "❌ Camera initialization error: " << e.what() << std::endl;
        throw;  // 重新抛出异常
    }
}

CameraWrapper::CameraWrapper(const std::string& video_path)
    : state_(CameraState::CLOSED)
    , camera_name_("Video_" + video_path)
    , frame_count_(0)
    , total_grab_time_(0.0) {
    
    std::cout << "📹 CameraWrapper: Opening video " << video_path << std::endl;
    
    try {
        cap_.open(video_path);
        
        if (cap_.isOpened()) {
            state_ = CameraState::OPENED;
            std::cout << "✅ Video opened successfully!" << std::endl;
        } else {
            state_ = CameraState::ERROR;
            throw std::runtime_error("Failed to open video: " + video_path);
        }
        
    } catch (const std::exception& e) {
        state_ = CameraState::ERROR;
        std::cerr << "❌ Video initialization error: " << e.what() << std::endl;
        throw;
    }
}

// ============================================
// 析构函数 - RAII 释放资源
// ============================================

CameraWrapper::~CameraWrapper() {
    std::cout << "🧹 CameraWrapper: Destructor called for " << camera_name_ << std::endl;
    release();  // 自动释放资源
}

// ============================================
// 移动构造函数
// ============================================

CameraWrapper::CameraWrapper(CameraWrapper&& other) noexcept
    : cap_(std::move(other.cap_))
    , state_(other.state_)
    , camera_name_(std::move(other.camera_name_))
    , frame_count_(other.frame_count_)
    , total_grab_time_(other.total_grab_time_) {
    
    // 将源对象置为无效状态
    other.state_ = CameraState::CLOSED;
    other.frame_count_ = 0;
    other.total_grab_time_ = 0.0;
    
    std::cout << "🔀 CameraWrapper: Move constructor" << std::endl;
}

// ============================================
// 移动赋值运算符
// ============================================

CameraWrapper& CameraWrapper::operator=(CameraWrapper&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        release();
        
        // 转移资源
        cap_ = std::move(other.cap_);
        state_ = other.state_;
        camera_name_ = std::move(other.camera_name_);
        frame_count_ = other.frame_count_;
        total_grab_time_ = other.total_grab_time_;
        
        // 将源对象置为无效状态
        other.state_ = CameraState::CLOSED;
        other.frame_count_ = 0;
        other.total_grab_time_ = 0.0;
        
        std::cout << "🔀 CameraWrapper: Move assignment" << std::endl;
    }
    return *this;
}

// ============================================
// 公共接口实现
// ============================================

bool CameraWrapper::grab(cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!cap_.isOpened()) {
        std::cerr << "⚠️ Camera not opened!" << std::endl;
        return false;
    }
    
    try {
        // 测量捕获时间
        auto start = std::chrono::high_resolution_clock::now();
        
        // 捕获帧
        bool success = cap_.read(frame);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        if (success) {
            frame_count_++;
            total_grab_time_ += duration;
            state_ = CameraState::STREAMING;
        } else {
            state_ = CameraState::ERROR;
            std::cerr << "⚠️ Failed to grab frame!" << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Grab error: " << e.what() << std::endl;
        state_ = CameraState::ERROR;
        return false;
    }
}

double CameraWrapper::getProperty(int prop_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cap_.get(prop_id);
}

bool CameraWrapper::setProperty(int prop_id, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    return cap_.set(prop_id, value);
}

std::string CameraWrapper::getInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    ss << "📷 Camera Info:\n";
    ss << "   Name: " << camera_name_ << "\n";
    ss << "   State: " << static_cast<int>(state_) << "\n";
    ss << "   Width: " << cap_.get(cv::CAP_PROP_FRAME_WIDTH) << "\n";
    ss << "   Height: " << cap_.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    ss << "   FPS: " << cap_.get(cv::CAP_PROP_FPS) << "\n";
    ss << "   Total Frames: " << cap_.get(cv::CAP_PROP_FRAME_COUNT) << "\n";
    ss << "   Captured: " << frame_count_ << " frames\n";
    if (frame_count_ > 0) {
        ss << "   Avg grab time: " << total_grab_time_ / frame_count_ << " ms\n";
    }
    return ss.str();
}

// ============================================
// 私有方法
// ============================================

void CameraWrapper::initialize() {
    // 已经在构造函数中完成
}

void CameraWrapper::release() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (cap_.isOpened()) {
        std::cout << "📷 Releasing camera: " << camera_name_ << std::endl;
        
        // 统计信息
        if (frame_count_ > 0) {
            std::cout << "   📊 Captured " << frame_count_ << " frames" << std::endl;
            std::cout << "   ⏱️  Avg grab time: " << total_grab_time_ / frame_count_ << " ms" << std::endl;
        }
        
        // 释放相机资源（OpenCV 会自动处理）
        cap_.release();
        state_ = CameraState::CLOSED;
        
        std::cout << "✅ Camera released: " << camera_name_ << std::endl;
    }
}

void CameraWrapper::updateState() {
    if (cap_.isOpened()) {
        state_ = CameraState::OPENED;
    } else {
        state_ = CameraState::CLOSED;
    }
}

} // namespace camera
