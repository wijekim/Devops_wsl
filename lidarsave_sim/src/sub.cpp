#include "rclcpp/rclcpp.hpp"
#include "opencv2/opencv.hpp"
#include "geometry_msgs/msg/vector3.hpp"
#include <chrono>
#include <math.h>

#define WIDTH 500
#define HEIGHT 500
#define MAXRANGE 2.0
#define SCALE ((WIDTH/2.0)/MAXRANGE)
#define RAD2DEG(x) ((x)*180./M_PI)

using namespace std::chrono;

cv::VideoWriter video_writer;
rclcpp::Publisher<geometry_msgs::msg::Vector3>::SharedPtr dxl_pub;

struct DetectionResult {
    int error;
    float left_dist, left_angle;
    float right_dist, right_angle;
    bool left_found, right_found;
};

DetectionResult processImageToAlgorithm(cv::Mat frame) {
    int center_x = WIDTH / 2;
    int center_y = HEIGHT / 2;
    
    cv::Mat hsv, mask;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::Scalar lower_red1(0, 100, 100), upper_red1(10, 255, 255);
    cv::Scalar lower_red2(160, 100, 100), upper_red2(179, 255, 255);
    cv::Mat mask1, mask2;
    cv::inRange(hsv, lower_red1, upper_red1, mask1);
    cv::inRange(hsv, lower_red2, upper_red2, mask2);
    mask = mask1 | mask2;

    DetectionResult res;
    res.left_dist = 1.0; res.right_dist = 1.0;
    res.left_found = false; res.right_found = false;

    for (int y = 0; y < mask.rows; y++) {
        for (int x = 0; x < mask.cols; x++) {
            if (mask.at<uchar>(y, x) > 0) {
                float dx = (x - center_x) / SCALE;
                float dy = (center_y - y) / SCALE;
                float r = sqrt(dx*dx + dy*dy);
                float angle = atan2(dx, dy);

                if (angle > M_PI/2.0 && angle <= M_PI) {
                    if (r < res.left_dist) {
                        res.left_dist = r; res.left_angle = angle; res.left_found = true;
                    }
                } else if (angle >= -M_PI && angle < -M_PI/2.0) {
                    if (r < res.right_dist) {
                        res.right_dist = r; res.right_angle = angle; res.right_found = true;
                    }
                }
            }
        }
    }

    float target_angle = 0.0;
    const float MAX_VIEW = M_PI / 2.0;
    if (res.left_found && res.right_found) target_angle = (res.left_angle + res.right_angle) / 2.0;
    else if (res.left_found) target_angle = (res.left_angle + (-MAX_VIEW)) / 2.0;
    else if (res.right_found) target_angle = (res.right_angle + MAX_VIEW) / 2.0;

    res.error = (int)RAD2DEG(target_angle);
    return res;
}

void processFrame(cv::Mat frame) {
    // 알고리즘 실행
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(WIDTH, HEIGHT));
    cv::Mat display_img = resized.clone();

    DetectionResult res = processImageToAlgorithm(resized);

    // 제어값 계산 및 출력
    float k = 0.5;
    int speed = 50;
    float Lvel = (float)(speed - k * res.error);
    float Rvel = (float)-(speed + k * res.error);
    printf("error: %d / Lvel: %.2f / Rvel: %.2f\n", res.error, Lvel, Rvel);

    // ROS2 퍼블리시 (노드 있을 때만)
    if (dxl_pub) {
        geometry_msgs::msg::Vector3 vel;
        vel.x = Lvel;
        vel.y = Rvel;
        dxl_pub->publish(vel);
    }

    // 시각화
    int center_px = WIDTH / 2;
    int center_py = HEIGHT / 2;
    int rect_size = 10;

    if(res.left_found) {
        int px = center_px + (int)(res.left_dist * sin(res.left_angle) * SCALE);
        int py = center_py - (int)(res.left_dist * cos(res.left_angle) * SCALE);
        int edge_x = (px < center_px) ? px + rect_size : px - rect_size;
        int edge_y = (py < center_py) ? py + rect_size : py - rect_size;
        cv::line(display_img, cv::Point(center_px, center_py), cv::Point(edge_x, edge_y), cv::Scalar(0, 255, 0), 1);
        cv::rectangle(display_img, cv::Point(px-rect_size, py-rect_size), cv::Point(px+rect_size, py+rect_size), cv::Scalar(255, 0, 0), 2);
        cv::circle(display_img, cv::Point(px, py), 2, cv::Scalar(255, 0, 0), -1);
    }

    if(res.right_found) {
        int px = center_px + (int)(res.right_dist * sin(res.right_angle) * SCALE);
        int py = center_py - (int)(res.right_dist * cos(res.right_angle) * SCALE);
        int edge_x = (px < center_px) ? px + rect_size : px - rect_size;
        int edge_y = (py < center_py) ? py + rect_size : py - rect_size;
        cv::line(display_img, cv::Point(center_px, center_py), cv::Point(edge_x, edge_y), cv::Scalar(0, 0, 255), 1);
        cv::rectangle(display_img, cv::Point(px-rect_size, py-rect_size), cv::Point(px+rect_size, py+rect_size), cv::Scalar(255, 0, 0), 2);
        cv::circle(display_img, cv::Point(px, py), 2, cv::Scalar(255, 0, 0), -1);
    }

    float line_angle = (float)res.error * M_PI / 180.0;
    cv::line(display_img, cv::Point(center_px, center_py),
             cv::Point(center_px + 50*sin(line_angle), center_py - 50*cos(line_angle)),
             cv::Scalar(255, 255, 0), 3);

    cv::imshow("Video_Replay", display_img);

    // 결과 영상 저장
    if (video_writer.isOpened()) {
        video_writer.write(display_img);
    }
}

int main(int argc, char **argv) {
    // ✅ 입력 영상 경로 (여기를 본인 파일로 변경)
    std::string input_path = "/home/linux/ros2_ws/lidar_output.mp4";
    std::string output_path = "/home/linux/ros2_ws/sim_result_processed.avi";

    // 영상 열기
    cv::VideoCapture cap(input_path);
    if (!cap.isOpened()) {
        printf("영상 파일 열기 실패: %s\n", input_path.c_str());
        return -1;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    printf("영상 열기 성공! FPS: %.1f\n", fps);

    // 결과 영상 저장 설정
    video_writer.open(output_path,
                      cv::VideoWriter::fourcc('M','J','P','G'),
                      fps > 0 ? fps : 10.0,
                      cv::Size(WIDTH, HEIGHT));

    if (!video_writer.isOpened()) {
        printf("결과 영상 저장 파일 열기 실패!\n");
    } else {
        printf("결과 저장 경로: %s\n", output_path.c_str());
    }

    // ROS2 (퍼블리시 필요시 주석 해제)
    // rclcpp::init(argc, argv);
    // auto node = rclcpp::Node::make_shared("video_replay_node");
    // dxl_pub = node->create_publisher<geometry_msgs::msg::Vector3>("topic_dxlpub", 10);

    cv::Mat frame;
    int frame_count = 0;

    while (cap.read(frame)) {
        if (frame.empty()) break;

        processFrame(frame);
        frame_count++;

        // ✅ 스페이스: 일시정지 / q: 종료
        int key = cv::waitKey(30);
        if (key == 'q' || key == 27) {
            printf("사용자 종료\n");
            break;
        } else if (key == ' ') {
            printf("일시정지 - 아무 키나 누르면 재개\n");
            cv::waitKey(0);
        }
    }

    printf("처리 완료! 총 %d 프레임\n", frame_count);
    cap.release();
    video_writer.release();
    cv::destroyAllWindows();

    // rclcpp::shutdown();
    return 0;
}