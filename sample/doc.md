# Hướng dẫn kết nối MQTT cho thiết bị

* **Broker:** MQTT server (103.57.220.146 - port **1884**)
* **Username:** `countingsystem` (cố định cho tất cả thiết bị)
* **Password:** `KeyLogin` (mỗi thiết bị có một mã riêng, lưu trong bảng `Devices`)
* **ClientId:** được sinh ngẫu nhiên bởi hệ thống.

---

## Cấu trúc topic

Thiết bị gửi dữ liệu theo dạng:

```
devices/{KeyLogin}/{Action}
```

Thiết bị nhận dữ liệu theo dạng:

```
devices/response/{KeyLogin}/{Action}
```

* `KeyLogin`: mã định danh thiết bị (password).
* `Action`: tên hành động, thuộc enum `MqttDeviceAction` (ví dụ: `Transaction`, `Customer`).

Ví dụ:

```
devices/abc123/transaction
devices/abc123/customer
```

---

## Payload

Tất cả payload gửi lên server theo JSON UTF-8.

Ví dụ `Transaction`:

```json
{
  "transactionId": "tx-1",
  "weightKg": 123.45
}
```

Ví dụ `Customer`:

```json
{
  "customerId": "cust-001",
  "name": "Nguyen Van A"
}
```

---

## Trạng thái (LWT)

* Khi kết nối: publish `{ "status": "online" }` vào `devices/{KeyLogin}/status` với `retain=true`.
* LWT: `{ "status": "offline" }` để broker tự gửi khi thiết bị mất kết nối.

---

## Tham số khuyến nghị

* **QoS:** 1 cho dữ liệu quan trọng.
* **Keepalive:** 60 giây.
* **Clean Session:** true nếu chỉ publish, false nếu còn subscribe.

---

## Ví dụ Python (paho-mqtt)

```python
import paho.mqtt.client as mqtt, json, datetime

HOST, PORT = 'mqtt.example.com', 1884
USERNAME, KEYLOGIN = 'countingsystem', 'abc123'

client = mqtt.Client(client_id="device-123")
client.username_pw_set(USERNAME, KEYLOGIN)

client.connect(HOST, PORT)

payload = {
  "transactionId": "tx-1",
  "weightKg": 12.34
}
client.publish(f"devices/{KEYLOGIN}/Transaction", json.dumps(payload), qos=1)
```

---

## Các lệnh chi tiết
1. Lấy thông tin phiên bản
* Thiết bị chủ động gửi yêu cầu lấy phiên bản thông qua topic: Action = version-info
```
devices/abc123/version-info
```
* Thiết bị đăng ký Topic nhận phiên bản ứng dụng: Action = version-info
```
devices/response/abc123/version-info
```
Nội dung trong Topic có dạng:

```json
{
  "VersionName":"tên test",
  "VersionNumber":"0.22",
  "ReleaseDate":"2025-12-02T16:17:55.6841598",
  "LinkDownload":"https://localhost:7085/Uploads/VersionDevice/Identity-master_20251202163746_9e66ca7f-c8d2-4bc2-b94f-6bfc96fd673f.zip"
}
```


2. Gửi lượt đếm bao
* Thiết bị chủ động gửi thông tin lượt đếm bao thông qua topic: Action = transaction
```
devices/abc123/version-info
```

Nội dung trong Topic có dạng:

```json
{
  "Name":"",
  "OrderCode":"",
  "ProductGroup":"",
  "ProductCode":"",
  "CustomerName":"",
  "CustomerPhone":"",
  "StartTime":"",
  "SetMode":"",
  "Location":"",
}
```