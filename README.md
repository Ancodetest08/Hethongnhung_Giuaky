# **Traffic Light Mini System**

## Thông tin dự án

**Môn học:** Hệ thống nhúng  
**Giảng viên hướng dẫn:** ThS. Hồ Viết Việt  
**Nhóm:** A2D  
**Thành viên:**
- Nguyễn Ngọc Đức
- Trương Văn An
- Lê Thị Ánh Trinh

## 1. Mô tả

Hệ thống đèn giao thông thông minh được xây dựng trên nền tảng ESP32 với FreeRTOS, có khả năng hoạt động trong 3 chế độ:
- **Chế độ bình thường:** Đèn giao thông tự động chuyển đổi theo chu kỳ (Đỏ → Vàng → Xanh)
- **Chế độ khẩn cấp:** Đèn đỏ nhấp nháy (kích hoạt 15 giây)
- **Chế độ ban đêm:** Đèn vàng nhấp nháy (kích hoạt 20 giây)

## 2. Tính năng

### Chế độ hoạt động
1. **Chế độ giao thông bình thường**
   - Đèn đỏ: 5 giây
   - Đèn vàng: 2 giây (chuyển tiếp)
   - Đèn xanh: 5 giây
   - Chu kỳ: Đỏ → Vàng → Xanh → Vàng → Đỏ
2. **Chế độ khẩn cấp (Emergency Mode)**
   - Đèn đỏ nhấp nháy với tần số 1Hz
   - Tự động tắt sau 15 giây
   - Có thể tắt thủ công bằng nút bấm
3. **Chế độ ban đêm (Night Mode)**
   - Đèn vàng nhấp nháy với tần số 1Hz
   - Tự động tắt sau 20 giây
   - Có thể tắt thủ công bằng nút bấm

### Ưu tiên chế độ
Emergency Mode > Night Mode > Traffic Light Mode

## 3. Phần cứng

### Linh kiện
- **Vi điều khiển:** ESP32
- **LED:** 3 LED (Đỏ, Vàng, Xanh)
- **Nút bấm:** 2 nút bấm (Emergency, Night Mode)
- **Điện trở:** Điện trở pull-up cho nút bấm

### Sơ đồ kết nối
| Thành phần     | GPIO Pin |
|----------------|----------|
| LED Đỏ         | GPIO 3   |
| LED Vàng       | GPIO 2   |
| LED Xanh       | GPIO 1   |
| Nút Emergency  | GPIO 5   |
| Nút Night Mode | GPIO 6   |

## 4. Kiến trúc phần mềm

### FreeRTOS Tasks
| Task             | Priority      | Mô tả                                   |
|------------------|---------------|-----------------------------------------|
| EmergencyTask    | 5 (cao nhất)  | Xử lý chế độ khẩn cấp                   |
| NightModeTask    | 4             | Xử lý chế độ ban đêm                    |
| ButtonTask       | 3             | Đọc và xử lý debounce nút bấm           |
| TrafficLightTask | 2             | Điều khiển đèn giao thông bình thường   |
| MonitorTask      | 1 (thấp nhất) | Giám sát và ghi log trạng thái hệ thống |

### Cơ chế đồng bộ
- **Mutex:** Bảo vệ các biến chia sẻ (flags, countdown)
- **Queue:** Truyền sự kiện giữa các task
  - `eventQueueEmergency`: Queue cho sự kiện Emergency
  - `eventQueueNight`: Queue cho sự kiện Night Mode

### Debouncing
Thời gian debounce: 50ms cho cả hai nút bấm

## 5. Cài đặt và chạy

### Yêu cầu
- ESP-IDF v4.4 trở lên
- Python 3.7+
- ESP32C3 development board

### Các bước thực hiện
1. **Clone repository**
```bash
git clone https://github.com/ancodetest08/Hethongnhung_Giuaky.git
cd Hethongnhung_Giuaky
```
2. **Build project**
```bash
idf.py build
```
3. **Flash vào ESP32**
```bash
idf.py flash
```
4. **Xem log**
```bash
idf.py monitor
```

## 6. Log Output

Hệ thống hiển thị thông tin trạng thái theo thời gian thực:
```
Traffic: 🔴 Red (5 s) | 🚨 Emergency: 0 (0 s) | 🌃 Night: 0 (0 s)
Traffic: 🔴 Red (4 s) | 🚨 Emergency: 0 (0 s) | 🌃 Night: 0 (0 s)
Traffic: 🟡 Yellow (2 s) | 🚨 Emergency: 0 (0 s) | 🌃 Night: 0 (0 s)
Traffic: 🟢 Green (5 s) | 🚨 Emergency: 0 (0 s) | 🌃 Night: 0 (0 s)
```

## 7. Hướng dẫn sử dụng

1. **Khởi động hệ thống:** Hệ thống tự động chạy ở chế độ giao thông bình thường
2. **Kích hoạt Emergency Mode:** Nhấn nút Emergency (GPIO 5)
3. **Kích hoạt Night Mode:** Nhấn nút Night Mode (GPIO 6)
4. **Tắt chế độ đặc biệt:** Nhấn lại nút tương ứng hoặc chờ tự động tắt

## 8. Chi tiết kỹ thuật

### State Machine
```
Normal Traffic:
RED (5s) → YELLOW (2s) → GREEN (5s) → YELLOW (2s) → RED (5s) → ...

Emergency: Overrides all states
Night Mode: Overrides Traffic Light only
```

### Thread Safety
- Tất cả các biến chia sẻ được bảo vệ bởi mutex
- Event-driven architecture với FreeRTOS queues
- Không có busy-waiting, sử dụng vTaskDelay hiệu quả

## 9. Tài liệu tham khảo

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)

## 10. Acknowledgments

- ThS. Hồ Viết Việt - Giảng viên hướng dẫn
- Khoa Điện tử - Viễn thông
- Trường Đại học Bách Khoa Đà Nẵng
