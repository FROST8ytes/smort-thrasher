#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "mbedtls/base64.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// specific sensor ID
//#define SENSOR_ID 1
int SENSOR_ID = random(2,6);

// OLED display dimensions
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 

// I2C address of the display
#define SCREEN_ADDRESS 0x3C

// OLED reset pin (set to -1 if not used)
#define OLED_RESET    -1

// Create the display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//pins for ultrasonic sensor
const int trigPin = 12; //trigger
const int echoPin = 13; //echo

//define sound speed in cm/uS
#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

//initialize variables
long duration;
float distanceCm;
float distanceInch;
float maxDistanceCm = 13;
float currentPercentage;

// WiFi credentials
const char* ssid = "depressingFutures";
const char* password = "12345678";
const char* serverUrl = "http://45.118.132.167/record";

// Camera pins for ESP32-CAM
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_LED_PIN 4  

// Set brightness level (0 to 255 for 8-bit resolution)
uint32_t maxBrightness = 39; 

void setup() {

  pinMode(FLASH_LED_PIN, OUTPUT);
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);

  // Initialize I2C communication (ESP32 default I2C pins: SDA=21, SCL=22)
  Wire.begin(2, 14);
  
  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Loop forever if initialization fails
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
  }

  // Clear the display buffer
  display.clearDisplay();

  // Set text parameters
  display.setTextSize(2);             
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(32, 16);            

  // Configure camera
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
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 20;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", err);
      return;
  }

}


void loop() {

    while (1==1) {
      // ultrasonic sensor reading and calculation
      digitalWrite(trigPin, LOW);
      delay(2);
      digitalWrite(trigPin, HIGH);
      delay(10);
      digitalWrite(trigPin, LOW);

      // Reads the echoPin, returns the sound wave travel time in microseconds
      duration = pulseIn(echoPin, HIGH);

      // Calculate the distance
      distanceCm = duration * SOUND_SPEED/2;
      Serial.println(String(distanceCm));

      // Convert to inches
      distanceInch = distanceCm * CM_TO_INCH;

      currentPercentage = ((maxDistanceCm-distanceCm) / maxDistanceCm) * 100;

      if (currentPercentage < 100 || currentPercentage >= 0){
        // Print text to the buffer
        display.clearDisplay();
        display.println(String(F("ID: 1 \n  ")) + String(currentPercentage) + "%");
        display.display();
        break;
      }


    }
    analogWrite(FLASH_LED_PIN, maxBrightness);
    delay(100);
    bool conversionDone = false;
    char* b64_str = NULL;
    
    // Loop until we get a valid frame and complete the conversion
    while (!conversionDone) {
      // Try to capture a frame from the camera
      camera_fb_t* fb = esp_camera_fb_get();
      if (fb) {
        // Determine required buffer length for Base64 conversion
        size_t b64_len = 0;
        mbedtls_base64_encode(NULL, 0, &b64_len, fb->buf, fb->len);
        
        // Allocate memory for Base64 string
        b64_str = (char*)malloc(b64_len);
        if (b64_str != NULL) {
          // Perform the Base64 encoding
          if (mbedtls_base64_encode((unsigned char*)b64_str, b64_len, &b64_len, fb->buf, fb->len) == 0) {
            // Conversion succeeded, exit loop
            conversionDone = true;
          }
          else {
            // Conversion failed; free allocated memory and try again
            free(b64_str);
            b64_str = NULL;
          }
        }
        // Return the frame buffer back to the driver
        esp_camera_fb_return(fb);
      }
      // Small delay before retrying
      delay(10);
    }
    delay(100);

    // Once conversion is done, turn off the flash
    analogWrite(FLASH_LED_PIN, maxBrightness-maxBrightness);

    int counter = 0;
    // Send HTTP POST request
    while (WiFi.status() == WL_CONNECTED && counter < 5) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");
        
        String payload = "{\"sensor_ID\":" + String(SENSOR_ID) + ", \"trash_level\":" + String(currentPercentage) + ", \"image_base64\":\"" + String(b64_str) + "\"}";
        Serial.println(payload);
        int httpResponseCode = http.POST(payload);
        
        if (httpResponseCode > 0) {
            Serial.println("Image sent successfully");
            Serial.println(httpResponseCode);
            break;
        } else {
            Serial.print("Error sending image: ");
            Serial.println(httpResponseCode);
            counter++;
            delay(1000);
        }
        
        http.end();
    }

    // Free memory
    free(b64_str);
    delay(5000); // Wait 5 seconds before next capture

    // Clear the display buffer
    display.clearDisplay();
}