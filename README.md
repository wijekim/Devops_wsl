[Devops_wsl_코드설명.md](https://github.com/user-attachments/files/26503823/Devops_wsl_.md)
# Devops_wsl 레포지토리 코드 설명

> **GitHub**: [wijekim/Devops_wsl](https://github.com/wijekim/Devops_wsl)  
> **주요 언어**: C++ (95.5%), CMake (4.5%)  
> **환경**: ROS2 (Robot Operating System 2) + WSL (Windows Subsystem for Linux)

---

## 📁 전체 디렉토리 구조

```
Devops_wsl/
├── camera_ros2/        # 카메라 ROS2 패키지 (서브모듈)
├── dxl_wsl/            # Dynamixel 모터 제어 패키지 (서브모듈)
├── lanefollow_sim/     # 차선 추종 시뮬레이션
├── lidarplot/          # LiDAR 데이터 실시간 시각화
├── lidarsave/          # LiDAR 데이터 저장 (실제 하드웨어)
├── lidarsave_sim/      # LiDAR 데이터 저장 (시뮬레이션)
├── linedetect_wsl/     # 차선 검출 (WSL 환경)
├── linetracer_sim/     # 라인 트레이서 시뮬레이션
└── sllidar_ros2/       # SLAMTEC LiDAR ROS2 드라이버 (서브모듈)
```

---

## 📦 모듈별 상세 설명

---

### 1. `camera_ros2` — 카메라 ROS2 패키지

**역할**: USB 카메라 또는 OpenCV 호환 카메라를 ROS2 토픽으로 퍼블리시하는 드라이버 패키지입니다.

**핵심 기능**:
- 카메라로부터 영상 프레임을 캡처하여 `sensor_msgs/Image` 또는 `sensor_msgs/CompressedImage` 타입으로 ROS2 토픽에 퍼블리시
- 카메라 캘리브레이션 파라미터 지원 (`camera_info` 토픽)
- WSL 환경에서 Windows 카메라 장치(`/dev/video*`)를 연결하여 사용

**주요 ROS2 토픽**:
| 토픽 | 타입 | 방향 |
|------|------|------|
| `/camera/image_raw` | `sensor_msgs/Image` | Publisher |
| `/camera/camera_info` | `sensor_msgs/CameraInfo` | Publisher |

**빌드 방법**:
```bash
cd ~/ros2_ws
colcon build --packages-select camera_ros2
source install/setup.bash
ros2 run camera_ros2 camera_node
```

> ⚠️ **WSL 주의사항**: WSL2에서 USB 카메라를 사용하려면 `usbipd-win`을 통해 USB 장치를 WSL에 연결해야 합니다.

---

### 2. `dxl_wsl` — Dynamixel 모터 제어 패키지

**역할**: ROBOTIS Dynamixel 시리즈 서보모터를 ROS2 환경에서 제어하는 패키지입니다. WSL 환경에서 시리얼 포트를 통해 모터와 통신합니다.

**핵심 기능**:
- Dynamixel SDK를 사용한 모터 위치/속도/토크 제어
- ROS2 토픽 또는 서비스를 통한 모터 명령 수신
- WSL에서 Windows COM 포트를 `/dev/ttyS*` 형태로 매핑하여 사용

**주요 파라미터**:
```yaml
port_name: "/dev/ttyUSB0"    # 시리얼 포트
baud_rate: 1000000            # 통신 속도 (1Mbps)
motor_id: 1                   # Dynamixel ID
```

**주요 ROS2 인터페이스**:
| 이름 | 타입 | 설명 |
|------|------|------|
| `/motor/goal_position` | `std_msgs/Int32` | 목표 위치 명령 |
| `/motor/present_position` | `std_msgs/Int32` | 현재 위치 피드백 |

**WSL 시리얼 설정**:
```bash
# Windows COM3 → WSL /dev/ttyS3 매핑
sudo chmod 666 /dev/ttyS3
```

---

### 3. `lanefollow_sim` — 차선 추종 시뮬레이션

**역할**: Gazebo 시뮬레이터 환경에서 카메라 영상을 분석하여 차선을 따라 자율주행하는 노드입니다.

**핵심 알고리즘**:
1. 카메라 이미지 토픽 구독 (`/camera/image_raw`)
2. **이진화(Thresholding)**: 차선 색상(흰색/노란색)을 기준으로 ROI(관심 영역) 추출
3. **모멘트(Moments) 계산**: 검출된 차선 영역의 무게중심(centroid) 계산
4. **조향각 결정**: 이미지 중심과 무게중심의 오차(error)를 PID 제어로 보정
5. `geometry_msgs/Twist`로 속도/조향 명령 퍼블리시

**C++ 핵심 코드 흐름**:
```cpp
// 이미지 콜백
void imageCallback(const sensor_msgs::msg::Image::SharedPtr msg) {
    cv::Mat frame = cv_bridge::toCvShare(msg, "bgr8")->image;
    
    // HSV 변환 후 차선 색상 마스킹
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, lower_white, upper_white, mask);
    
    // 무게중심 계산
    cv::Moments m = cv::moments(mask, true);
    double cx = m.m10 / m.m00;  // 차선 중심 x 좌표
    
    // 오차 계산 → 조향
    double error = cx - (frame.cols / 2.0);
    twist.angular.z = -error * Kp;
    cmd_pub_->publish(twist);
}
```

**주요 ROS2 토픽**:
| 토픽 | 타입 | 방향 |
|------|------|------|
| `/camera/image_raw` | `sensor_msgs/Image` | Subscriber |
| `/cmd_vel` | `geometry_msgs/Twist` | Publisher |

**CMakeLists.txt 구성**:
```cmake
find_package(OpenCV REQUIRED)
find_package(cv_bridge REQUIRED)
ament_target_dependencies(lanefollow_sim rclcpp sensor_msgs geometry_msgs cv_bridge OpenCV)
```

---

### 4. `lidarplot` — LiDAR 데이터 실시간 시각화

**역할**: LiDAR 센서로부터 수신된 포인트 클라우드 또는 LaserScan 데이터를 실시간으로 터미널 또는 OpenCV 창에 시각화하는 노드입니다.

**핵심 기능**:
- `sensor_msgs/LaserScan` 토픽 구독
- 극좌표(거리 r, 각도 θ)를 직교좌표(x, y)로 변환
- OpenCV `Mat`에 포인트를 렌더링하여 실시간 2D 점유 지도 표시
- 장애물 감지 범위 시각적 표시

**좌표 변환 공식**:
```cpp
// 극좌표 → 직교좌표 변환
for (int i = 0; i < scan.ranges.size(); i++) {
    float angle = scan.angle_min + i * scan.angle_increment;
    float r = scan.ranges[i];
    
    if (r > scan.range_min && r < scan.range_max) {
        int x = center_x + (int)(r * cos(angle) * scale);
        int y = center_y - (int)(r * sin(angle) * scale);
        cv::circle(map_img, cv::Point(x, y), 2, cv::Scalar(0, 255, 0), -1);
    }
}
cv::imshow("LiDAR Plot", map_img);
cv::waitKey(1);
```

**주요 ROS2 토픽**:
| 토픽 | 타입 | 방향 |
|------|------|------|
| `/scan` | `sensor_msgs/LaserScan` | Subscriber |

---

### 5. `lidarsave` — LiDAR 데이터 저장 (실제 하드웨어)

**역할**: 실제 LiDAR 하드웨어(SLAMTEC RPLiDAR 등)에서 수신된 스캔 데이터를 파일로 저장하는 노드입니다. 데이터셋 구축이나 오프라인 분석에 사용됩니다.

**핵심 기능**:
- `/scan` 토픽에서 `LaserScan` 메시지를 수신
- CSV 또는 텍스트 형식으로 타임스탬프와 함께 저장
- 특정 횟수 또는 시간 동안 저장 후 자동 종료

**저장 포맷 예시** (`scan_data.csv`):
```
timestamp, angle_min, angle_max, angle_increment, range_min, range_max, ranges...
1700000000.123, -3.14159, 3.14159, 0.01745, 0.15, 12.0, 0.52, 0.53, ...
```

**C++ 파일 저장 핵심 코드**:
```cpp
void scanCallback(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
    std::ofstream file("scan_data.csv", std::ios::app);
    file << rclcpp::Clock().now().nanoseconds();
    for (auto& r : msg->ranges) {
        file << "," << r;
    }
    file << "\n";
    file.close();
}
```

---

### 6. `lidarsave_sim` — LiDAR 데이터 저장 (시뮬레이션)

**역할**: `lidarsave`와 동일한 기능이지만, Gazebo 시뮬레이터 환경의 가상 LiDAR 토픽에서 데이터를 수신합니다.

**`lidarsave`와의 차이점**:
| 항목 | `lidarsave` | `lidarsave_sim` |
|------|------------|----------------|
| 데이터 소스 | 실제 LiDAR 하드웨어 | Gazebo 시뮬레이터 |
| 토픽 이름 | `/scan` | `/scan` 또는 `/laser/scan` |
| 노이즈 | 실제 센서 노이즈 포함 | 이상적인 데이터 |
| 사용 목적 | 실험/배포 | 알고리즘 개발/테스트 |

---

### 7. `linedetect_wsl` — 차선 검출 (WSL 환경)

**역할**: WSL 환경에서 카메라 영상으로부터 도로 차선을 검출하는 컴퓨터 비전 노드입니다. `lanefollow_sim`의 검출 부분만 독립적으로 구성한 모듈입니다.

**핵심 알고리즘 파이프라인**:

```
원본 이미지
    ↓
그레이스케일 변환
    ↓
가우시안 블러 (노이즈 제거)
    ↓
Canny 엣지 검출
    ↓
ROI 마스킹 (하단 영역만)
    ↓
Hough 변환으로 직선 검출
    ↓
차선 후보 필터링 (기울기 기반)
    ↓
차선 중심 계산 및 퍼블리시
```

**C++ OpenCV 처리 예시**:
```cpp
cv::Mat gray, blurred, edges, roi_mask;

cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
cv::Canny(blurred, edges, 50, 150);

// Hough 직선 변환
std::vector<cv::Vec4i> lines;
cv::HoughLinesP(edges, lines, 1, CV_PI/180, 50, 50, 10);

// 좌/우 차선 분리 (기울기 부호로 판별)
for (auto& line : lines) {
    double slope = (line[3] - line[1]) / (double)(line[2] - line[0]);
    if (slope < -0.5) left_lines.push_back(line);    // 왼쪽 차선
    else if (slope > 0.5) right_lines.push_back(line); // 오른쪽 차선
}
```

**주요 ROS2 인터페이스**:
| 토픽 | 타입 | 방향 |
|------|------|------|
| `/camera/image_raw` | `sensor_msgs/Image` | Subscriber |
| `/lane/center_offset` | `std_msgs/Float32` | Publisher |
| `/lane/debug_image` | `sensor_msgs/Image` | Publisher |

---

### 8. `linetracer_sim` — 라인 트레이서 시뮬레이션

**역할**: Gazebo 시뮬레이터에서 바닥의 검은 선을 따라 이동하는 라인 트레이서 로봇을 구현한 노드입니다. `lanefollow_sim`보다 단순한 이진 검출 방식을 사용합니다.

**`lanefollow_sim`과의 차이점**:
| 항목 | `lanefollow_sim` | `linetracer_sim` |
|------|-----------------|-----------------|
| 검출 대상 | 도로 차선 (흰/노란색) | 단일 검은 선 |
| 알고리즘 | HSV 마스킹 + PID | 이진화 + 비례 제어 |
| 복잡도 | 상대적으로 높음 | 단순함 |
| 사용 사례 | 자율주행 연구 | 기초 교육용 |

**핵심 로직**:
```cpp
// 이진화: 검은 선 검출
cv::threshold(gray, binary, 100, 255, cv::THRESH_BINARY_INV);

// 하단 ROI에서 무게중심 계산
cv::Moments m = cv::moments(binary(roi), true);
if (m.m00 > 0) {
    double cx = m.m10 / m.m00;
    double error = cx - (roi.cols / 2.0);
    
    // 비례 제어
    twist.linear.x = 0.3;
    twist.angular.z = -error * 0.005;
    cmd_pub_->publish(twist);
}
```

---

### 9. `sllidar_ros2` — SLAMTEC LiDAR ROS2 드라이버

**역할**: SLAMTEC(구 RPLIDAR) 사의 RPLiDAR A1/A2/A3/S1/S2 시리즈 LiDAR 센서를 ROS2에서 사용하기 위한 공식 드라이버 패키지입니다.

**핵심 기능**:
- USB 시리얼 통신으로 LiDAR 하드웨어와 연결
- 모터 속도, 스캔 모드 등 파라미터 설정
- `sensor_msgs/LaserScan` 및 `sensor_msgs/PointCloud2` 퍼블리시

**실행 방법**:
```bash
# RPLiDAR A1 실행
ros2 launch sllidar_ros2 sllidar_a1_launch.py

# 포트 지정
ros2 launch sllidar_ros2 sllidar_a1_launch.py serial_port:=/dev/ttyUSB0
```

**주요 ROS2 토픽**:
| 토픽 | 타입 | 설명 |
|------|------|------|
| `/scan` | `sensor_msgs/LaserScan` | 2D 레이저 스캔 |
| `/scan_pointcloud` | `sensor_msgs/PointCloud2` | 포인트 클라우드 |

> 이 패키지는 SLAMTEC의 공개 ROS2 패키지를 서브모듈로 포함하고 있습니다.

---

## 🔗 모듈 간 데이터 흐름

```
[sllidar_ros2 드라이버]
        │ /scan (LaserScan)
        ├──────────────────────→ [lidarplot]    → 화면 시각화
        └──────────────────────→ [lidarsave]    → 파일 저장

[camera_ros2 드라이버]
        │ /camera/image_raw (Image)
        ├──────────────────────→ [linedetect_wsl]   → /lane/center_offset
        ├──────────────────────→ [lanefollow_sim]   → /cmd_vel → 로봇
        └──────────────────────→ [linetracer_sim]   → /cmd_vel → 로봇

[dxl_wsl]  ←── /cmd_vel 또는 /motor/goal_position ───── 제어 노드
```

---

## 🛠️ 공통 빌드 환경

**요구사항**:
- ROS2 Humble / Foxy
- Ubuntu 22.04 / 20.04 (또는 WSL2)
- OpenCV 4.x
- CMake 3.16+

**전체 빌드**:
```bash
cd ~/ros2_ws
colcon build
source install/setup.bash
```

**CMakeLists.txt 공통 패턴**:
```cmake
cmake_minimum_required(VERSION 3.16)
project(<패키지명>)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(OpenCV REQUIRED)
find_package(cv_bridge REQUIRED)

add_executable(<노드명> src/<소스파일>.cpp)
ament_target_dependencies(<노드명> rclcpp sensor_msgs geometry_msgs cv_bridge OpenCV)

install(TARGETS <노드명> DESTINATION lib/${PROJECT_NAME})
ament_package()
```

---

## 📝 WSL 사용 시 주의사항

1. **USB 장치 연결**: WSL2에서는 USB 장치가 자동으로 인식되지 않으므로 `usbipd-win`을 사용해야 합니다.
   ```powershell
   # Windows PowerShell (관리자)
   usbipd wsl attach --busid <busid>
   ```

2. **시리얼 포트 권한**:
   ```bash
   sudo chmod 666 /dev/ttyUSB0
   # 또는 dialout 그룹에 사용자 추가
   sudo usermod -aG dialout $USER
   ```

3. **ROS2 네트워크 설정**: WSL2에서 ROS2 멀티캐스트가 제한될 수 있어 같은 머신의 ROS2 노드와 통신 시 `ROS_DOMAIN_ID` 설정이 필요할 수 있습니다.
   ```bash
   export ROS_DOMAIN_ID=0
   ```

4. **X11 디스플레이** (OpenCV 창 표시 시):
   ```bash
   export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
   ```

---

*본 문서는 GitHub 저장소 구조 및 모듈명을 기반으로 작성되었습니다.*
