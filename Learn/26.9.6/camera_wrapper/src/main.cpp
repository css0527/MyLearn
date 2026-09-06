/**
 * @file main.cpp
 * @brief 演示 CameraWrapper 的使用
 * 
 * 学习目标：
 * 1. RAII 自动资源管理
 * 2. 异常安全
 * 3. 移动语义
 * 4. 智能指针使用
 */

#include "CameraWrapper.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <memory>

using namespace camera;

/**
 * @brief 演示 1：基本 RAII 用法
 */
void demo_basic_usage() {
    std::cout << "\n========== Demo 1: Basic RAII ==========\n" << std::endl;
    
    try {
        // RAII：构造函数打开相机，析构函数自动关闭
        CameraWrapper camera(0);
        
        cv::Mat frame;
        for (int i = 0; i < 10; i++) {
            if (camera.grab(frame)) {
                cv::imshow("Camera", frame);
                cv::waitKey(30);
            }
        }
        
        // 相机在离开作用域时自动释放
        std::cout << "\n⏳ Leaving scope, destructor will be called..." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Demo 1 failed: " << e.what() << std::endl;
    }
    
    // 这里相机已经自动释放了！
}

/**
 * @brief 演示 2：使用智能指针管理
 */
void demo_smart_pointer() {
    std::cout << "\n========== Demo 2: Smart Pointer ==========\n" << std::endl;
    
    try {
        // 使用 unique_ptr 管理相机
        auto camera = std::make_unique<CameraWrapper>(0);
        
        cv::Mat frame;
        for (int i = 0; i < 5; i++) {
            if (camera->grab(frame)) {
                cv::imshow("Camera (unique_ptr)", frame);
                cv::waitKey(30);
            }
        }
        
        // unique_ptr 会自动调用析构函数
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Demo 2 failed: " << e.what() << std::endl;
    }
}

/**
 * @brief 演示 3：移动语义
 */
void demo_move_semantics() {
    std::cout << "\n========== Demo 3: Move Semantics ==========\n" << std::endl;
    
    try {
        // 创建相机
        CameraWrapper camera1(0);
        std::cout << "📷 camera1 created" << std::endl;
        
        // 移动构造 - 所有权转移
        CameraWrapper camera2 = std::move(camera1);
        std::cout << "🔀 camera2 move-constructed from camera1" << std::endl;
        
        cv::Mat frame;
        if (camera2.grab(frame)) {
            cv::imshow("Camera (moved)", frame);
            cv::waitKey(1000);
        }
        
        // camera1 现在无效，camera2 拥有资源
        // 只有 camera2 会释放资源
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Demo 3 failed: " << e.what() << std::endl;
    }
}

/**
 * @brief 演示 4：异常安全
 */
void demo_exception_safety() {
    std::cout << "\n========== Demo 4: Exception Safety ==========\n" << std::endl;
    
    try {
        // 故意使用无效的相机ID来触发异常
        std::cout << "⚠️ Trying to open invalid camera (id=99)..." << std::endl;
        CameraWrapper camera(99);  // 会抛出异常
        
        // 这行代码不会执行
        cv::Mat frame;
        camera.grab(frame);
        
    } catch (const std::exception& e) {
        std::cerr << "✅ Exception caught: " << e.what() << std::endl;
        std::cout << "✅ Camera resources were automatically cleaned up!" << std::endl;
    }
}

/**
 * @brief 演示 5：从视频文件读取（模拟相机）
 */
void demo_video_file() {
    std::cout << "\n========== Demo 5: Video File ==========\n" << std::endl;
    
    // 尝试打开视频文件
    // 如果没有视频文件，会抛出异常
    try {
        CameraWrapper video("../test.mp4");  // 或者使用绝对路径
        
        cv::Mat frame;
        for (int i = 0; i < 10; i++) {
            if (video.grab(frame)) {
                cv::imshow("Video", frame);
                cv::waitKey(30);
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "ℹ️ Video file not found (this is expected if no test.mp4)" << std::endl;
        std::cout << "   " << e.what() << std::endl;
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char** argv) {
    std::cout << "=========================================" << std::endl;
    std::cout << "   CameraWrapper RAII Demo" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // 运行所有演示
    // 注释掉不想要的演示
    
    // demo_basic_usage();
    // demo_smart_pointer();
    // demo_move_semantics();
    // demo_exception_safety();
    // demo_video_file();
    
    // 运行一个简单的演示
    std::cout << "\n📷 Running simple camera demo..." << std::endl;
    std::cout << "Press 'q' to quit, any other key to continue" << std::endl;
    
    try {
        CameraWrapper camera(0);
        
        // 设置相机参数（可选）
        camera.setProperty(cv::CAP_PROP_FRAME_WIDTH, 640);
        camera.setProperty(cv::CAP_PROP_FRAME_HEIGHT, 480);
        
        std::cout << camera.getInfo() << std::endl;
        
        cv::Mat frame;
        while (true) {
            if (camera.grab(frame)) {
                cv::imshow("Camera RAII Demo", frame);
                
                char key = cv::waitKey(1) & 0xFF;
                if (key == 'q' || key == 'Q') {
                    break;
                }
            } else {
                std::cout << "⚠️ Failed to grab frame" << std::endl;
                break;
            }
        }
        
        // 相机自动释放！
        std::cout << "\n✅ Demo completed, camera will be auto-released..." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        std::cout << "\n💡 No camera found. Try running with a video file instead." << std::endl;
    }
    
    std::cout << "\n=========================================" << std::endl;
    std::cout << "   Program finished" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    return 0;
}
