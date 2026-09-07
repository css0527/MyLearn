#include "CameraWrapper.hpp"
#include <sstream>

namespace camera {

// ============================================
// 构造函数
// ============================================

CameraWrapper::CameraWrapper(int camera_id)
    : state_(CameraState::CLOSED)
    , camera_id_(camera_id)
    , camera_name_("Camera_" + std::to_string(camera_id))
    , frame_count_(0) {
    
    std::cout << "📷 [RAII] Opening camera: " << camera_id << std::endl;
    
    cap_.open(camera_id);
    if (cap_.isOpened()) {
        state_ = CameraState::OPENED;
        std::cout << "✅ Camera " << camera_id << " opened!" << std::endl;
    } else {
        state_ = CameraState::ERROR;
        throw std::runtime_error("Failed to open camera: " + std::to_string(camera_id));
    }
}

CameraWrapper::CameraWrapper(const std::string& video_path)
    : state_(CameraState::CLOSED)
    , camera_id_(-1)
    , camera_name_("Video_" + video_path)
    , frame_count_(0) {
    
    std::cout << "📹 [RAII] Opening video: " << video_path << std::endl;
    
    cap_.open(video_path);
    if (cap_.isOpened()) {
        state_ = CameraState::OPENED;
        std::cout << "✅ Video opened!" << std::endl;
    } else {
        state_ = CameraState::ERROR;
        throw std::runtime_error("Failed to open video: " + video_path);
    }
}

// ============================================
// 析构函数
// ============================================

CameraWrapper::~CameraWrapper() {
    std::cout << "🧹 [RAII] Destructor: " << camera_name_ << std::endl;
    release();
}

// ============================================
// 移动构造函数
// ============================================

CameraWrapper::CameraWrapper(CameraWrapper&& other) noexcept
    : cap_(std::move(other.cap_))
    , state_(other.state_)
    , camera_id_(other.camera_id_)
    , camera_name_(std::move(other.camera_name_))
    , frame_count_(other.frame_count_) {
    
    other.state_ = CameraState::CLOSED;
    other.camera_id_ = -1;
    other.frame_count_ = 0;
    std::cout << "🔀 [Move] Camera moved" << std::endl;
}

// ============================================
// 移动赋值
// ============================================

CameraWrapper& CameraWrapper::operator=(CameraWrapper&& other) noexcept {
    if (this != &other) {
        release();
        cap_ = std::move(other.cap_);
        state_ = other.state_;
        camera_id_ = other.camera_id_;
        camera_name_ = std::move(other.camera_name_);
        frame_count_ = other.frame_count_;
        
        other.state_ = CameraState::CLOSED;
        other.camera_id_ = -1;
        other.frame_count_ = 0;
        std::cout << "🔀 [Move Assign] Camera moved" << std::endl;
    }
    return *this;
}

// ============================================
// 公共接口
// ============================================

bool CameraWrapper::grab(cv::Mat& frame) {
    if (!cap_.isOpened()) {
        return false;
    }
    
    if (cap_.read(frame)) {
        frame_count_++;
        state_ = CameraState::STREAMING;
        return true;
    }
    
    state_ = CameraState::ERROR;
    return false;
}

std::string CameraWrapper::getInfo() const {
    std::stringstream ss;
    ss << "📷 Camera: " << camera_name_ << "\n";
    ss << "   State: " << static_cast<int>(state_) << "\n";
    ss << "   Frames: " << frame_count_ << "\n";
    ss << "   Width: " << cap_.get(cv::CAP_PROP_FRAME_WIDTH) << "\n";
    ss << "   Height: " << cap_.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    return ss.str();
}

// ============================================
// 私有方法
// ============================================

void CameraWrapper::release() {
    if (cap_.isOpened()) {
        std::cout << "📷 Releasing: " << camera_name_ 
                  << " (frames: " << frame_count_ << ")" << std::endl;
        cap_.release();
        state_ = CameraState::CLOSED;
    }
}

} // namespace camera
