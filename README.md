# pack_and_write_send
26/5/2020 pack_and_write_send  version นี้  read ram send MQTT
### ทดสอบ Task send realtime  read RAM send ถ้า Database server พร้อม  คือ ส่ง 1 กลับ

### หลังจาก Fumction นี้  ChkDBReadram_send() publish ไปตรวจสอบ  Function นี้ถูกเรียกใช้งาน

### จาก taskChkDB_Send  run ทุกๆ 500 ms เมื่อ Database server พร้อม ส่ง ข้อมูลที่อ่าน จาก Ram

## Function writeToram(); เรียก  readRam(String sdatamqtt, int write_addr)

### *** ต่อไปที่จะทำคือ   เมื่อเก็บข้อมูลใน Ram จนเต็ม 32KB จะย้ายไปใน SD card /history จะมี Task อ่าน SDcard  แล้วส่ง 
