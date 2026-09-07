/**
 * @file main.cpp
 * @brief 演示 unique_ptr vs shared_ptr 的区别
 */

#include "CameraWrapper.hpp"
#include <iostream>
#include <memory>
#include <vector>

using namespace camera;

// ============================================
// Demo 1: unique_ptr - 独占所有权
// ============================================

void demo_unique_ptr() {
    std::cout << "\n========== DEMO 1: unique_ptr (独占所有权) ==========\n" << std::endl;
    
    // ✅ 创建 unique_ptr（推荐使用 make_unique）
    auto cam1 = std::make_unique<CameraWrapper>(0);
    std::cout << "✅ cam1 created (owns the camera)" << std::endl;
    
    // ❌ 不能拷贝
    // auto cam2 = cam1;  // 编译错误！
    
    // ✅ 可以移动（所有权转移）
    auto cam2 = std::move(cam1);
    std::cout << "✅ cam2 now owns the camera (cam1 is null)" << std::endl;
    
    // ❌ cam1 现在无效
    // if (cam1) { ... }  // cam1 是 nullptr
    
    // ✅ cam2 可以使用
    cv::Mat frame;
    if (cam2->grab(frame)) {
        std::cout << "✅ Captured frame with cam2" << std::endl;
        cv::imshow("Demo 1", frame);
        cv::waitKey(1000);
    }
    
    // cam2 离开作用域自动释放
    std::cout << "⏳ Leaving scope, cam2 will be destroyed..." << std::endl;
}

// ============================================
// Demo 2: shared_ptr - 共享所有权
// ============================================

void demo_shared_ptr() {
    std::cout << "\n========== DEMO 2: shared_ptr (共享所有权) ==========\n" << std::endl;
    
    // ✅ 创建 shared_ptr（推荐使用 make_shared）
    auto cam1 = std::make_shared<CameraWrapper>(0);
    std::cout << "✅ cam1 created (ref_count: " << cam1.use_count() << ")" << std::endl;
    
    // ✅ 可以拷贝（引用计数 +1）
    auto cam2 = cam1;
    std::cout << "✅ cam2 copied from cam1 (ref_count: " << cam1.use_count() << ")" << std::endl;
    
    auto cam3 = cam1;
    std::cout << "✅ cam3 copied from cam1 (ref_count: " << cam1.use_count() << ")" << std::endl;
    
    // 所有 shared_ptr 共享同一个相机
    cv::Mat frame;
    if (cam3->grab(frame)) {
        std::cout << "✅ Captured frame with cam3" << std::endl;
        cv::imshow("Demo 2", frame);
        cv::waitKey(1000);
    }
    
    // 引用计数减少
    cam2.reset();  // cam2 释放，引用计数 -1
    std::cout << "✅ cam2 reset (ref_count: " << cam1.use_count() << ")" << std::endl;
    
    cam3.reset();  // cam3 释放，引用计数 -1
    std::cout << "✅ cam3 reset (ref_count: " << cam1.use_count() << ")" << std::endl;
    
    // 最后一个 shared_ptr 离开作用域时释放资源
    std::cout << "⏳ Leaving scope, cam1 will be destroyed..." << std::endl;
}

// ============================================
// Demo 3: unique_ptr 作为函数参数
// ============================================

void processCamera(std::unique_ptr<CameraWrapper> cam) {
    std::cout << "📷 Processing camera in function..." << std::endl;
    cv::Mat frame;
    if (cam->grab(frame)) {
        std::cout << "✅ Processed frame" << std::endl;
    }
    // cam 在函数结束时自动释放
}

void demo_unique_ptr_as_parameter() {
    std::cout << "\n========== DEMO 3: unique_ptr 作为函数参数 ==========\n" << std::endl;
    
    auto cam = std::make_unique<CameraWrapper>(0);
    std::cout << "✅ cam created" << std::endl;
    
    // ✅ 传递所有权到函数
    processCamera(std::move(cam));
    
    // ❌ cam 现在无效（所有权已转移）
    if (!cam) {
        std::cout << "✅ cam is now null (ownership transferred)" << std::endl;
    }
}

// ============================================
// Demo 4: shared_ptr 作为函数参数
// ============================================

void processCameraShared(CameraSharedPtr cam) {
    std::cout << "📷 Processing shared camera (ref_count: " 
              << cam.use_count() << ")" << std::endl;
    cv::Mat frame;
    if (cam->grab(frame)) {
        std::cout << "✅ Processed frame" << std::endl;
    }
    // 引用计数在函数返回时 -1，但不会释放（因为外面还有引用）
}

void demo_shared_ptr_as_parameter() {
    std::cout << "\n========== DEMO 4: shared_ptr 作为函数参数 ==========\n" << std::endl;
    
    auto cam = std::make_shared<CameraWrapper>(0);
    std::cout << "✅ cam created (ref_count: " << cam.use_count() << ")" << std::endl;
    
    processCameraShared(cam);  // 拷贝 shared_ptr，引用计数 +1
    std::cout << "✅ After function (ref_count: " << cam.use_count() << ")" << std::endl;
}

// ============================================
// Demo 5: 容器中使用智能指针
// ============================================

void demo_container_with_smart_pointers() {
    std::cout << "\n========== DEMO 5: 容器 + 智能指针 ==========\n" << std::endl;
    
    std::vector<CameraSharedPtr> cameras;
    
    // 创建多个相机
    for (int i = 0; i < 3; i++) {
        auto cam = std::make_shared<CameraWrapper>(i);
        cameras.push_back(cam);
        std::cout << "✅ Camera " << i << " added (ref_count: " 
                  << cam.use_count() << ")" << std::endl;
    }
    
    // 使用所有相机
    cv::Mat frame;
    for (size_t i = 0; i < cameras.size(); i++) {
        if (cameras[i]->grab(frame)) {
            std::cout << "✅ Camera " << i << " captured frame" << std::endl;
        }
    }
    
    // 容器清空时自动释放
    std::cout << "⏳ Clearing vector..." << std::endl;
    cameras.clear();
}

// ============================================
// Demo 6: weak_ptr 避免循环引用
// ============================================

struct Node {
    std::shared_ptr<Node> next;
    CameraWeakPtr camera;  // weak_ptr 避免循环引用
    
    Node(CameraSharedPtr cam) : camera(cam) {
        std::cout << "🔄 Node created" << std::endl;
    }
    ~Node() {
        std::cout << "🧹 Node destroyed" << std::endl;
    }
};

void demo_weak_ptr() {
    std::cout << "\n========== DEMO 6: weak_ptr 避免循环引用 ==========\n" << std::endl;
    
    auto cam = std::make_shared<CameraWrapper>(0);
    
    // 创建节点，使用 weak_ptr
    auto node1 = std::make_shared<Node>(cam);
    auto node2 = std::make_shared<Node>(cam);
    node1->next = node2;
    node2->next = node1;  // 循环引用，但因为使用 weak_ptr 不会泄漏
    
    // 使用 weak_ptr
    if (auto locked_cam = node1->camera.lock()) {
        cv::Mat frame;
        if (locked_cam->grab(frame)) {
            std::cout << "✅ Captured frame from weak_ptr" << std::endl;
            cv::imshow("Demo 6", frame);
            cv::waitKey(1000);
        }
    }
    
    std::cout << "⏳ Nodes will be destroyed without memory leak" << std::endl;
}

// ============================================
// Demo 7: 对比裸指针 vs 智能指针
// ============================================

void demo_bare_vs_smart() {
    std::cout << "\n========== DEMO 7: 裸指针 vs 智能指针 ==========\n" << std::endl;
    
    // ❌ 裸指针 - 需要手动管理
    std::cout << "🔴 裸指针:" << std::endl;
    CameraWrapper* raw_cam = nullptr;
    try {
        raw_cam = new CameraWrapper(0);
        cv::Mat frame;
        raw_cam->grab(frame);
        delete raw_cam;  // 必须手动删除
        raw_cam = nullptr;
    } catch (...) {
        // 如果异常发生，delete 不会执行 → 内存泄漏
        if (raw_cam) delete raw_cam;
    }
    
    // ✅ 智能指针 - 自动管理
    std::cout << "\n🟢 智能指针:" << std::endl;
    try {
        auto smart_cam = std::make_unique<CameraWrapper>(0);
        cv::Mat frame;
        smart_cam->grab(frame);
        // 无需手动 delete，异常安全
    } catch (...) {
        // 智能指针会自动释放资源
    }
    
    std::cout << "\n✅ 智能指针更安全!" << std::endl;
}

// ============================================
// 主函数
// ============================================

int main(int argc, char** argv) {
    std::cout << "=========================================" << std::endl;
    std::cout << "  智能指针学习 - unique_ptr vs shared_ptr" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // 运行所有演示
    // 注释掉不需要的演示
    
    // demo_unique_ptr();
    // demo_shared_ptr();
    // demo_unique_ptr_as_parameter();
    // demo_shared_ptr_as_parameter();
    // demo_container_with_smart_pointers();
    // demo_weak_ptr();
    // demo_bare_vs_smart();
    
    // 运行一个综合演示
    std::cout << "\n🚀 运行综合演示..." << std::endl;
    std::cout << "按 'q' 退出, 按任意键继续" << std::endl;
    
    try {
        // 使用 unique_ptr（独占）
        std::cout << "\n📸 使用 unique_ptr:" << std::endl;
        auto cam = std::make_unique<CameraWrapper>(0);
        std::cout << cam->getInfo() << std::endl;
        
        cv::Mat frame;
        for (int i = 0; i < 5; i++) {
            if (cam->grab(frame)) {
                cv::imshow("Camera (unique_ptr)", frame);
                if (cv::waitKey(30) == 'q') break;
            }
        }
        
        // 转移所有权到函数
        // processCamera(std::move(cam));  // 如果要测试，取消注释
        
        std::cout << "\n✅ 程序结束，所有资源自动释放" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
    }
    
    return 0;
}
