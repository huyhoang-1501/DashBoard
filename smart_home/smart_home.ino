// WiFi
#include <WiFi.h>
// WiFi Credentials
#define WIFI_SSID "Phong Tro Tang 3.2"
#define WIFI_PASSWORD "99999999"

// Firebase
#include <FirebaseESP32.h>
#define FIREBASE_HOST "https://smart-home-bebd9-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "1952SmaIJ5MPkWxK7Bqxejx6oi4SlrpoCuU6lrzs" // Lấy từ Firebase Console > Project Settings > Service Accounts > Database Secrets

// DHT11
#include <DHT.h>
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Cảm biến khí (MQ-2) mô phỏng CO2
#define GAS_SENSOR_PIN 36
#define CO2_THRESHOLD 2000 // Ngưỡng CO2 (ppm)

// LED mô phỏng thiết bị
#define LED_DEN_1 26   
#define LED_QUAT_1 25    
#define LED_LOA_1 15 
#define LED_DEN_2 14   
#define LED_QUAT_2 27    
#define LED_LOA_2 33 
#define LED_DEN_3 32   
#define LED_QUAT_3 23    
#define LED_LOA_3 22 
#define LED_DEN_4 21   
#define LED_QUAT_4 19    
#define LED_LOA_4 5  

// Firebase Objects
FirebaseData fbdo;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;
FirebaseJson json; // Thêm FirebaseJson để xử lý dữ liệu JSON

// Thời gian xử lý không chặn
unsigned long previousMillis = 0;
const long interval = 2000; // Gửi dữ liệu mỗi 2 giây

// Danh sách phong
const char* phongs[] = {"phong1", "phong2", "phong3", "phong4"};
const int numPhongs = 4;

void setup() {
    Serial.begin(9600);
    Serial.println(F("Khởi động DHT11, MQ-2 và Firebase cho 4 phòng (1 bộ cảm biến, LED mô phỏng thiết bị)!"));

    // Thiết lập chân LED
    int ledPins[] = {LED_DEN_1, LED_QUAT_1, LED_LOA_1, LED_DEN_2, LED_QUAT_2, LED_LOA_2,
                     LED_DEN_3, LED_QUAT_3, LED_LOA_3, LED_DEN_4, LED_QUAT_4, LED_LOA_4};
    for (int i = 0; i < 12; i++) {
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], LOW);
    }

    // Kết nối WiFi
    connectWiFi();

    // Khởi động DHT11
    dht.begin();

    // Khởi tạo MQ-2
    pinMode(GAS_SENSOR_PIN, INPUT);

    // Cấu hình Firebase
    firebaseConfig.host = FIREBASE_HOST;
    firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&firebaseConfig, &firebaseAuth);
    Firebase.reconnectWiFi(true);

    if (Firebase.ready()) {
        Serial.println("Đã kết nối Firebase!");
        for (int i = 0; i < numPhongs; i++) {
            Firebase.setInt(fbdo, String("/") + phongs[i] + "/status/connection", 1);
        }
    } else {
        Serial.println("Không thể kết nối Firebase!");
    }

    // Khởi tạo trạng thái
    initializeFirebaseStates();
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // Kiểm tra WiFi
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi ngắt kết nối, thử lại...");
            connectWiFi();
            for (int i = 0; i < numPhongs; i++) {
                Firebase.setInt(fbdo, String("/") + phongs[i] + "/status/connection", WiFi.status() == WL_CONNECTED ? 1 : 0);
            }
        }

        if (Firebase.ready()) {
            for (int i = 0; i < numPhongs; i++) {
                Firebase.setInt(fbdo, String("/") + phongs[i] + "/status/connection", 1);
            }

            // Gửi dữ liệu cảm biến
            sendSensorData();

            // Điều khiển LED
            controlLEDs();
        } else {
            for (int i = 0; i < numPhongs; i++) {
                Firebase.setInt(fbdo, String("/") + phongs[i] + "/status/connection", 0);
            }
        }
    }
}

void connectWiFi() {
    Serial.print("Đang kết nối WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retryCount = 0, maxRetries = 20;
    while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
        Serial.print(".");
        delay(500);
        retryCount++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nĐã kết nối WiFi! IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nKhông thể kết nối WiFi!");
    }
}

void initializeFirebaseStates() {
    for (int i = 0; i < numPhongs; i++) {
        String basePath = "/" + String(phongs[i]);
        Firebase.setFloat(fbdo, basePath + "/temp", 0.0);
        Firebase.setFloat(fbdo, basePath + "/humi", 0.0);
        Firebase.setFloat(fbdo, basePath + "/co2", 0.0);
        Firebase.setInt(fbdo, basePath + "/den", 0);
        Firebase.setInt(fbdo, basePath + "/quat", 0);
        Firebase.setInt(fbdo, basePath + "/loa", 0);
        Firebase.setInt(fbdo, basePath + "/status/connection", 0);

        // Sử dụng FirebaseJson để gửi totalTime (bỏ lastResetTimestamp)
        json.clear();
        json.set("den", 0);
        json.set("quat", 0);
        json.set("loa", 0);
        Firebase.setJSON(fbdo, basePath + "/totalTime", json);

        Serial.println("Khởi tạo trạng thái Firebase cho " + String(phongs[i]) + " hoàn tất!");
    }
}

void sendSensorData() {
    // Đọc cảm biến DHT11
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    // Đọc cảm biến MQ-2
    int mq2Value = analogRead(GAS_SENSOR_PIN);
    float co2PPM = map(mq2Value, 0, 4095, 0, 3000); // Mô phỏng CO2

    // Gửi dữ liệu chung cho tất cả phòng
    for (int i = 0; i < numPhongs; i++) {
        String basePath = "/" + String(phongs[i]);
        if (!isnan(humidity) && !isnan(temperature)) {
            Firebase.setFloat(fbdo, basePath + "/temp", temperature);
            Firebase.setFloat(fbdo, basePath + "/humi", humidity);
            Serial.println(String(phongs[i]) + " - Nhiệt độ: " + String(temperature) + "°C, Độ ẩm: " + String(humidity) + "%");
        } else {
            Firebase.setString(fbdo, basePath + "/sensor_status", "DHT11 Error");
            Serial.println(String(phongs[i]) + " - Lỗi đọc DHT11!");
        }
        Firebase.setFloat(fbdo, basePath + "/co2", co2PPM);
        if (co2PPM > CO2_THRESHOLD) {
            // Sử dụng FirebaseJson để gửi log (bỏ timestamp)
            json.clear();
            json.set("message", "Nồng độ CO2 quá cao: " + String(co2PPM) + " ppm");
            Firebase.setJSON(fbdo, basePath + "/logs/" + String(millis()), json);
        }
        Serial.println(String(phongs[i]) + " - CO2: " + String(co2PPM) + " ppm");
    }
}

void controlLEDs() {
    // Phòng 1
    String basePath = "/" + String(phongs[0]);
    if (Firebase.getInt(fbdo, basePath + "/den")) {
        int state = fbdo.intData();
        digitalWrite(LED_DEN_1, state ? HIGH : LOW);
        Serial.println(String(phongs[0]) + " - LED ĐÈN: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[0]) + " - Lỗi đọc trạng thái LED ĐÈN!");
    }
    if (Firebase.getInt(fbdo, basePath + "/quat")) {
        int state = fbdo.intData();
        digitalWrite(LED_QUAT_1, state ? HIGH : LOW);
        Serial.println(String(phongs[0]) + " - LED QUẠT: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[0]) + " - Lỗi đọc trạng thái LED QUẠT!");
    }
    if (Firebase.getInt(fbdo, basePath + "/loa")) {
        int state = fbdo.intData();
        digitalWrite(LED_LOA_1, state ? HIGH : LOW);
        Serial.println(String(phongs[0]) + " - LED LOA: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[0]) + " - Lỗi đọc trạng thái LED LOA!");
    }

    // Phòng 2
    basePath = "/" + String(phongs[1]);
    if (Firebase.getInt(fbdo, basePath + "/den")) {
        int state = fbdo.intData();
        digitalWrite(LED_DEN_2, state ? HIGH : LOW);
        Serial.println(String(phongs[1]) + " - LED ĐÈN: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[1]) + " - Lỗi đọc trạng thái LED ĐÈN!");
    }
    if (Firebase.getInt(fbdo, basePath + "/quat")) {
        int state = fbdo.intData();
        digitalWrite(LED_QUAT_2, state ? HIGH : LOW);
        Serial.println(String(phongs[1]) + " - LED QUẠT: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[1]) + " - Lỗi đọc trạng thái LED QUẠT!");
    }
    if (Firebase.getInt(fbdo, basePath + "/loa")) {
        int state = fbdo.intData();
        digitalWrite(LED_LOA_2, state ? HIGH : LOW);
        Serial.println(String(phongs[1]) + " - LED LOA: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[1]) + " - Lỗi đọc trạng thái LED LOA!");
    }

    // Phòng 3
    basePath = "/" + String(phongs[2]);
    if (Firebase.getInt(fbdo, basePath + "/den")) {
        int state = fbdo.intData();
        digitalWrite(LED_DEN_3, state ? HIGH : LOW);
        Serial.println(String(phongs[2]) + " - LED ĐÈN: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[2]) + " - Lỗi đọc trạng thái LED ĐÈN!");
    }
    if (Firebase.getInt(fbdo, basePath + "/quat")) {
        int state = fbdo.intData();
        digitalWrite(LED_QUAT_3, state ? HIGH : LOW);
        Serial.println(String(phongs[2]) + " - LED QUẠT: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[2]) + " - Lỗi đọc trạng thái LED QUẠT!");
    }
    if (Firebase.getInt(fbdo, basePath + "/loa")) {
        int state = fbdo.intData();
        digitalWrite(LED_LOA_3, state ? HIGH : LOW);
        Serial.println(String(phongs[2]) + " - LED LOA: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[2]) + " - Lỗi đọc trạng thái LED LOA!");
    }

    // Phòng 4
    basePath = "/" + String(phongs[3]);
    if (Firebase.getInt(fbdo, basePath + "/den")) {
        int state = fbdo.intData();
        digitalWrite(LED_DEN_4, state ? HIGH : LOW);
        Serial.println(String(phongs[3]) + " - LED ĐÈN: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[3]) + " - Lỗi đọc trạng thái LED ĐÈN!");
    }
    if (Firebase.getInt(fbdo, basePath + "/quat")) {
        int state = fbdo.intData();
        digitalWrite(LED_QUAT_4, state ? HIGH : LOW);
        Serial.println(String(phongs[3]) + " - LED QUẠT: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[3]) + " - Lỗi đọc trạng thái LED QUẠT!");
    }
    if (Firebase.getInt(fbdo, basePath + "/loa")) {
        int state = fbdo.intData();
        digitalWrite(LED_LOA_4, state ? HIGH : LOW);
        Serial.println(String(phongs[3]) + " - LED LOA: " + String(state ? "Bật" : "Tắt"));
    } else {
        Serial.println(String(phongs[3]) + " - Lỗi đọc trạng thái LED LOA!");
    }
}