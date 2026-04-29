## TOPIC

devices/{KeyLogin}/Transaction

## Cấu trúc dữ liệu trả về (Response)

ESP32 sẽ trả về một đối tượng JSON có cấu trúc như sau:

```json
{
  "orderCode": "ORD001",
  "productGroup": "Bao",
  "productCode": "BAO001",
  "target":123,
  "count":123,
  "customerName": "Khách hàng A",
  "customerPhone": "123456789",
  "startTime": "14:30 - 15/12/2024",
  "setMode": "output",
  "location": "Khu vực A",
  "conveyor": "Băng tải 01",
  "sensorTimeMs": 2450
}
```

## Mô tả chi tiết các trường dữ liệu

| Tên trường | Kiểu dữ liệu | Mô tả | Ví dụ |
|------------|---------------|-------|--------|
| `orderCode` | String | Mã đơn hàng | `"ORD001"` |
| `productGroup` | String | Nhóm sản phẩm | `"Bao"` |
| `productCode` | String | Mã sản phẩm | `"BAO001"` |
| `target` | INT | Kế hoạch đếm của đơn hàng | `123` |
| `count` | INT | Số lượng đếm của sản phẩm | `123` |
| `customerName` | String | Tên khách hàng: | `"Khách hàng A"` |
| `customerPhone` | String | Số điện thoại: | `"123456789"` |
| `startTime` | String | Thời gian bắt đầu đếm| `"14:30 - 15/12/2024"` |
| `setMode` | String | Loại hình  | `"output/input(Nhập hoặc xuất)"` |
| `location` | String | Địa điểm | `"Hà Nội"` |
| `conveyor` | String | Số hiệu băng tải | `"BT-001"` |
| `sensorTimeMs` | INT | Thời gian sensor HIGH đo được gần nhất, đơn vị ms | `2450` |
