# lanefollow_sim

ROS 2 기반의 카메라 영상을 활용한 **차선 인식 및 추종 시뮬레이션** 패키지입니다.  
카메라로부터 압축 이미지를 구독하고, OpenCV를 이용해 좌우 차선을 실시간으로 검출한 뒤, Proportional 제어를 통해 Dynamixel 모터의 속도 명령을 생성합니다.

---

## 📺 시연 영상

| RAPAI5 시점 | 사람 시점 |
|:-----------:|:---------:|
| [YouTube Shorts](https://youtube.com/shorts/Y-UWfVWqSPo?si=fzs4Lk2MH_RkBDJC) | [YouTube](https://youtu.be/6H_-eKHCJzQ) |

---

## 📦 패키지 정보

| 항목 | 내용 |
|------|------|
| 패키지명 | `lanefollow_sim` |
| 버전 | 0.0.0 |
| 빌드 시스템 | `ament_cmake` (C++14) |
| ROS 버전 | ROS 2 |
| 유지관리자 | weejea13@naver.com |

---

## 🗂 파일 구조

```
lanefollow_sim/
├── CMakeLists.txt          # CMake 빌드 설정
├── package.xml             # 패키지 메타데이터 및 의존성
└── src/
    ├── sub.cpp             # 메인 노드: 이미지 구독 → 차선 인식 → 속도 퍼블리시
    ├── object.cpp          # 차선 객체 검출 및 에러 계산 구현
    ├── object.hpp          # object.cpp 헤더
    ├── dxl.cpp             # Dynamixel 모터 제어 구현 (참조용)
    └── dxl.hpp             # Dynamixel SDK 래퍼 클래스 헤더
```

---

## ⚙️ 동작 원리

### 전체 처리 흐름

```
카메라 (image/compressed)
        │
        ▼
  [sub.cpp - sub_video 노드]
        │
        ├─ 1. 이미지 디코딩 (CompressedImage → cv::Mat)
        ├─ 2. ROI 추출 (하단 1/4 영역)
        ├─ 3. 그레이스케일 변환 + 밝기 정규화 (목표 밝기 90)
        ├─ 4. 이진화 (threshold: 150)
        ├─ 5. 차선 객체 검출 (findObjects)
        ├─ 6. 에러 계산 (TwoLineError)
        └─ 7. P 제어 → 좌/우 모터 속도 계산
                │
                ▼
        [topic_dxlpub - geometry_msgs/Vector3]
          x: 좌측 모터 속도 (RPM)
          y: 우측 모터 속도 (RPM)
```

### 차선 검출 알고리즘 (`object.cpp`)

1. **커넥티드 컴포넌트 분석** (`cv::connectedComponentsWithStats`)으로 이진화 영상에서 흰색 덩어리(블랍) 검출
2. **노이즈 제거**: 면적 100픽셀 이하 객체 무시
3. **좌/우 차선 추적**: 이전 프레임에서 기억한 위치(`left_pt`, `right_pt`)로부터 **150픽셀 이내**의 가장 가까운 객체를 각각 좌선·우선으로 선택
4. 해당 프레임에서 차선을 찾지 못하면 **마지막으로 알려진 위치를 유지** (Lost 대응)

### 제어 로직 (P 제어)

```
에러 = 영상 중심 X - (좌측 차선 X + 우측 차선 X) / 2

좌측 모터 속도 = 100 - k × error
우측 모터 속도 = -(100 + k × error)

k = 0.18  (비례 게인)
```

> 에러가 양수 → 차량이 우측으로 치우침 → 좌측 속도 감소, 우측 속도 증가 → 좌회전

### 키보드 제어

| 키 | 동작 |
|----|------|
| `s` | 주행 모드 활성화 |
| `q` | 정지 (속도 0 발행) |

---

## 🔌 Dynamixel 지원 모터 (`dxl.hpp` / `dxl.cpp`)

`dxl.hpp`에 정의된 `DXL_MODEL` 매크로를 변경하여 지원 모터를 선택합니다.

| 매크로 | 모터 모델 | 프로토콜 | 보드레이트 | 속도 단위 | 속도 범위 |
|--------|-----------|----------|-----------|-----------|-----------|
| `MX12W` (기본값) | MX-12W | 1.0 | 2,000,000 bps | 0.916 RPM/unit | ±470 RPM |
| `XC430W150` | XC430-W150 | 2.0 | 4,000,000 bps | 0.229 RPM/unit | ±105 RPM |
| `XL430W250` | XL430-W250 | 2.0 | 4,000,000 bps | 0.229 RPM/unit | ±60 RPM |

모터 연결 포트 기본값: `/dev/ttyUSB0`

---

## 📡 ROS 2 인터페이스

### 구독 (Subscribe)

| 토픽 | 메시지 타입 | 설명 |
|------|------------|------|
| `image/compressed` | `sensor_msgs/msg/CompressedImage` | 카메라 압축 이미지 입력 |

> 영상 파일을 사용하려면 `sub.cpp` 내 `pub_video` 토픽으로 변경 가능 (주석 참조)

### 발행 (Publish)

| 토픽 | 메시지 타입 | 설명 |
|------|------------|------|
| `topic_dxlpub` | `geometry_msgs/msg/Vector3` | 모터 속도 명령 |

- `x`: 좌측 모터 목표 속도 (RPM)
- `y`: 우측 모터 목표 속도 (RPM)

### QoS 설정

- `KeepLast(10)` / **TCP** (신뢰성 보장) 모드 사용
- UDP(best_effort) 모드로 전환하려면 `sub.cpp` 내 주석 해제

---

## 🛠 의존성

```xml
<depend>rclcpp</depend>
<depend>sensor_msgs</depend>
<depend>cv_bridge</depend>
<depend>OpenCV</depend>
<depend>geometry_msgs</depend>  <!-- CMakeLists.txt에 추가됨 -->
```

---

## 🚀 빌드 및 실행

### 빌드

```bash
cd ~/ros2_ws
colcon build --packages-select lanefollow_sim
source install/setup.bash
```

### 실행

```bash
# 노드 실행
ros2 run lanefollow_sim sub
```

### 카메라 토픽 확인

```bash
# 카메라 토픽이 올바르게 발행되는지 확인
ros2 topic list
ros2 topic echo /image/compressed
```

---

## 🖥 실행 화면

노드가 실행되면 두 개의 OpenCV 창이 표시됩니다.

| 창 이름 | 내용 |
|---------|------|
| `Video Subscriber` | 원본 컬러 프레임 |
| `Binary` | ROI 이진화 결과 + 검출된 차선 박스/포인트 시각화 |

터미널에는 매 프레임마다 아래와 같은 로그가 출력됩니다.

```
[INFO] error: 12 / Time:8.34 / Lvel:97.84 / Rvel:-102.16
```

---

## 📝 참고 사항

- `dxl.cpp` / `dxl.hpp`는 실제 Dynamixel 하드웨어 제어용 코드이며, 현재 빌드 타겟(`sub`)에는 직접 링크되지 않습니다. 하드웨어 연동 시 별도 실행 파일을 추가하거나 통합이 필요합니다.
- 기본 모터 모델은 `MX12W`로 설정되어 있으며, `dxl.hpp`의 `DXL_MODEL` 정의를 변경하여 다른 모터로 전환 가능합니다.
- ROI는 프레임 하단 1/4 영역을 사용합니다. 카메라 해상도나 설치 높이에 따라 `sub.cpp`의 ROI 설정 조정이 필요할 수 있습니다.
