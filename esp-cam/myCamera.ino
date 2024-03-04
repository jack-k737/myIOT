#include "WiFi.h"
#include "esp_camera.h"
#include "Arduino.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Arduino_JSON.h>

#include <PubSubClient.h>
#include "b64.h"

//esp32-cam针脚定义 拍照相关
constexpr int kCameraPin_PWDN   =  32;
constexpr int kCameraPin_RESET  =  -1;  // NC
constexpr int kCameraPin_XCLK   =   0;
constexpr int kCameraPin_SIOD   =  26;
constexpr int kCameraPin_SIOC   =  27;
constexpr int kCameraPin_Y9     =  35;
constexpr int kCameraPin_Y8     =  34;
constexpr int kCameraPin_Y7     =  39;
constexpr int kCameraPin_Y6     =  36;
constexpr int kCameraPin_Y5     =  21;
constexpr int kCameraPin_Y4     =  19;
constexpr int kCameraPin_Y3     =  18;
constexpr int kCameraPin_Y2     =   5;
constexpr int kCameraPin_VSYNC  =  25;
constexpr int kCameraPin_HREF   =  23;
constexpr int kCameraPin_PCLK   =  22;

//LED灯的管脚
constexpr int LED = 4;

// 此处需替换连接的无线网络为自己使用的无线网络
const char* ssid = "ESP";
const char* password = "12345678";

// 此处需替换MQTT服务端为自己使用的服务端
const char* mqtt_server = "raspberrypi";
const int mqtt_port = 1883;
const char* mqtt_user = "xiejun";
const char* mqtt_password = "xiej17708490986";

//设置主题,图片信息的主题和下达指令的主题
const char* mqtt_TopicName = "/devices/esp32/camera";
const char* mqtt_TopicCmd = "/devices/esp32/cmd";

//设置开发板类型
#define CAMERA_MODEL_AI_THINKER


bool Camera_on = true;


//mqtt客户端接收到消息后，通过client.loop调用此函数
void callback(char* topic, byte* payload, unsigned int length) {

  JSONVar myObject = JSON.parse((char*)payload);
    if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }
  if (myObject.hasOwnProperty("Led_cmd")) {
    switch((int)myObject["Led_cmd"])
    {
      case 0:
        digitalWrite(LED, LOW);
        break;
      case 1:
        digitalWrite(LED, HIGH);
        break;
      default:
        break;
    }
  }
  if (myObject.hasOwnProperty("Camera_cmd")) {
    switch((int)myObject["Camera_cmd"])
    {
      case 0:
        Camera_on = false;
        break;
      case 1:
        Camera_on = true;
        break;
      default:
        break;
    }
  }
}

//定义mqtt客户端
WiFiClient mqttClient;
PubSubClient client(mqtt_server, mqtt_port, callback, mqttClient);


//初始化相机
void setup_camera() {
  camera_config_t config;
  config.pin_pwdn     = kCameraPin_PWDN;
  config.pin_reset    = kCameraPin_RESET;
  config.pin_xclk     = kCameraPin_XCLK;
  config.pin_sscb_sda = kCameraPin_SIOD;
  config.pin_sscb_scl = kCameraPin_SIOC;
  config.pin_d7       = kCameraPin_Y9;
  config.pin_d6       = kCameraPin_Y8;
  config.pin_d5       = kCameraPin_Y7;
  config.pin_d4       = kCameraPin_Y6;
  config.pin_d3       = kCameraPin_Y5;
  config.pin_d2       = kCameraPin_Y4;
  config.pin_d1       = kCameraPin_Y3;
  config.pin_d0       = kCameraPin_Y2;
  config.pin_vsync    = kCameraPin_VSYNC;
  config.pin_href     = kCameraPin_HREF;
  config.pin_pclk     = kCameraPin_PCLK;
  config.xclk_freq_hz = 20000000;
  config.ledc_timer   = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 31;
  config.fb_count     = 1;

  esp_camera_init(&config);
}


//通过mqtt发送图片
String msg;
void getimg(){//拍照分段发送到mqtt
    if(!Camera_on)return;
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb){
        //Serial.printf("width: %d, height: %d, buf: 0x%x, len: %d\n", fb->width, fb->height, fb->buf, fb->len);
        char *msg_buffer = b64_encode(fb->buf, fb->len);
        msg = msg_buffer;
        if (msg.length() > 0){
            client.beginPublish(mqtt_TopicName, msg.length(), 0);
            client.print(msg);
            client.endPublish();
            msg = "";
        }
        free(msg_buffer);
        esp_camera_fb_return(fb);
    }
}

//连接WiFi
void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());
}

//MQTT重连
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //通过写寄存器，消除电压检测，防止开发板重启
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  setup_camera(); //设置相机
  setup_wifi();		//连接WIFI
  if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("mqtt connected");
  }
  client.subscribe(mqtt_TopicCmd);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  getimg();
  client.loop();    //如果有订阅消息，需要通过此函数，来处理消息，并调用自定义的回调函数callback。
  //delay(10);
}
