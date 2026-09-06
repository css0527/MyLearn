/**
 * OpenCV 基础示例
 * 读取图片并显示
 */

#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>

int main(int argc, char** argv) {
    // 1. 检查命令行参数
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <image_path>" << std::endl;
        return -1;
    }

    // 2. 读取图片
    cv::Mat image = cv::imread(argv[1]);
    if (image.empty()) {
        std::cout << "Could not read the image: " << argv[1] << std::endl;
        return -1;
    }

    // 3. 显示图片信息
    std::cout << "Image loaded successfully!" << std::endl;
    std::cout << "Width: " << image.cols << std::endl;
    std::cout << "Height: " << image.rows << std::endl;
    std::cout << "Channels: " << image.channels() << std::endl;

    // 4. 显示图片
    cv::imshow("Hello OpenCV", image);
    cv::waitKey(0);

    // 5. 保存图片副本
    cv::Mat gray_image;
    cv::cvtColor(image, gray_image, cv::COLOR_BGR2GRAY);
    cv::imwrite("gray_image.jpg", gray_image);
    std::cout << "Gray image saved as gray_image.jpg" << std::endl;

    return 0;
}
