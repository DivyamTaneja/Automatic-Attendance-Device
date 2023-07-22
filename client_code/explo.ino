#include "esp_camera.h"
#include "camera_pins.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "config.h"
#include "WiFi.h"

WiFiClient client; 
File root; 

struct wifi find_wifi(){
  struct wifi cred;
  File file = root.openNextFile();
  String ssid = ""; String password = ""; String type = "";
  if(!file) cred.result = "scanning_completed";
  else{
    String file_name = file.name();
    if(file_name[0]!='w') cred.result = "wifi_cred_not_found";
    else {
      bool cond_1 = false; bool cond_2 = false;
      while(file.available()){
        byte c = file.read();
        if(c==0x0D && cond_1==false) cond_1 = true;
        else if(c==0x0D && cond_1==true) cond_2 = true;
        else if(cond_1==false && cond_2==false && c!=0x0A && c!=0x0D) ssid += char(c);
        else if(cond_1==true && cond_2==false && c!=0x0A && c!=0x0D) password += char(c);
        else if(cond_1==true && cond_2==true && c!=0x0A && c!=0x0D) type += char(c);}
      cred.result = "wifi_cred_found";}
  }
  cred.ssid = ssid;
  cred.password = password;
  cred.type = type;
  return cred;
}

struct server find_server(){
  struct server cred;
  File file = root.openNextFile();
  String id = ""; String path = ""; String port = "";
  if(!file) cred.result = "scanning_completed";
  else{
    String file_name = file.name();
    if(file_name[0]!='s') cred.result = "server_cred_not_found";
    else {
      bool cond_1 = false; bool cond_2 = false;
      while(file.available()){
        byte c = file.read();
        if(c==0x0D && cond_1==false) cond_1 = true;
        else if(c==0x0D && cond_1==true) cond_2 = true;
        else if(cond_1==false && cond_2==false && c!=0x0A && c!=0x0D) id += char(c);
        else if(cond_1==true && cond_2==false && c!=0x0A && c!=0x0D) path += char(c);
        else if(cond_1==true && cond_2==true && c!=0x0A && c!=0x0D) port += char(c);}
      cred.result = "server_cred_found";}
  }
  cred.id = id;
  cred.path = path;
  cred.port = port;
  return cred;
}

void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);

  SD_MMC.begin();
  root = SD_MMC.open("/"); root.openNextFile();
  while(true){
    wifi_cred = find_wifi();
    if(wifi_cred.result=="scanning_completed") break;
    else if(wifi_cred.result=="wifi_cred_found") break;
  }
  root = SD_MMC.open("/"); root.openNextFile();
  while(true){
    server_cred = find_server();
    if(server_cred.result=="scanning_completed") break;
    else if(server_cred.result=="server_cred_found") break;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_cred.ssid.c_str(), wifi_cred.password.c_str());  

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_240X240;
  config.jpeg_quality = 6;
  config.fb_count = 2;
  esp_err_t err = esp_camera_init(&config);
  if(err != ESP_OK) {delay(1000); ESP.restart();}
  
  pinMode(2, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(12, OUTPUT);
}

void loop(){
  String pred = send_img();
  Serial.println(pred);
  if(pred!=String("None")) {
    digitalWrite(2, HIGH); 
    delay(1000); 
    digitalWrite(2, LOW);
  }
}

void connect_server() {
  if(!client.connect(server_cred.id.c_str(), server_cred.port.toInt())) {
    if(WiFi.status()!=WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.reconnect();
      while(WiFi.status()!=WL_CONNECTED) delay(100);
    }
    delay(100);
    connect_server();
  }
}

String send_img(){
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  digitalWrite(12, HIGH);
  connect_server();
  digitalWrite(12, LOW);
  String head = "--ESP32\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--ESP32--\r\n";
  uint16_t imageLen = fb->len;
  uint16_t extraLen = head.length() + tail.length();
  uint16_t totalLen = imageLen + extraLen;
  client.println("POST " + server_cred.path + " HTTP/1.1");
  client.println("Host: " + server_cred.id);
  client.println("Content-Length: " + String(totalLen));
  client.println("Content-Type: multipart/form-data; boundary=ESP32");
  client.println();
  client.print(head);
  uint8_t *fbBuf = fb->buf;
  size_t fbLen = fb->len;
  for (size_t n=0; n<fbLen; n=n+1024){
    if(n+1024<fbLen){
      client.write(fbBuf, 1024);
      fbBuf += 1024;}
    else if(fbLen%1024>0){
      size_t remainder = fbLen%1024;
      client.write(fbBuf, remainder);}}   
  client.print(tail);
  esp_camera_fb_return(fb);
  String pred = client.readStringUntil('\r');
  pred.remove(0, 11);
  return pred;
}
