# Ghi chú về MQTT Broker nội bộ

Tài liệu này được giữ lại để tránh nhầm lẫn với các phiên bản cũ của dự án.

## Trạng thái hiện tại

- Giao tiếp nội bộ giữa `ESP32 <-> web UI` đã chuyển sang **WebSocket trực tiếp**.
- Web interface kết nối thẳng tới endpoint realtime của ESP32:
  - Port: `81`
  - Path: `/ws`
- **Không còn cần Mosquitto hoặc MQTT broker nội bộ** để xem số đếm realtime, heartbeat, trạng thái cảm biến hoặc gửi lệnh Start/Pause/Reset từ giao diện web.

## Phần MQTT còn lại

- `MQTT2` vẫn được giữ nguyên để gửi báo cáo hoàn thành đơn hàng ra server ngoài.
- Luồng này dùng các trường cấu hình:
  - `mqtt2Server`
  - `mqtt2Port`
  - `mqtt2Username`
  - `mqtt2Password`

## Khi nào mới cần MQTT broker?

Chỉ cần nếu bạn chủ động khôi phục lại kiến trúc cũ hoặc tích hợp thêm một luồng MQTT mới ngoài phạm vi hiện tại.
