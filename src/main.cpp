#include <Arduino.h>
#include <TM1637Display.h>

/* ==========================================================================
 * CẤU HÌNH CHÂN (DỰA THEO DIAGRAM.JSON)
 * ========================================================================== */

// --- Cấu hình chân Đèn LED ---
const int red1 = 13, yellow1 = 12, green1 = 11; // Trục NS (Bắc - Nam)
const int red2 = 10, yellow2 = 9, green2 = 8;   // Trục EW (Đông - Tây)

// --- Cấu hình chân Nút bấm ---
const int PIN_BTN_PED    = A2; // Cả 4 nút Người đi bộ đều nối chung vào A2
const int PIN_BTN_EMG    = A3; // Nút Khẩn cấp
const int PIN_BTN_INC_NS = A4; // Nút Tăng thời gian Xanh NS (+5s)
const int PIN_BTN_DEC_NS = A5; // Nút Giảm thời gian Xanh NS (-5s)
const int PIN_BTN_INC_EW = 0;  // Nút Tăng thời gian Xanh EW (+5s)
const int PIN_BTN_DEC_EW = 1;  // Nút Giảm thời gian Xanh EW (-5s)

// --- Khởi tạo 4 màn hình hiển thị TM1637 (CLK, DIO) ---
TM1637Display displayN(3, 2);
TM1637Display displayS(5, 4);
TM1637Display displayW(7, 6);
TM1637Display displayE(A1, A0);

// --- Cấu hình tham số thời gian mặc định (giây) ---
const int DEFAULT_GREEN_NS = 30;
const int DEFAULT_GREEN_EW = 30;
const int YELLOW_TIME = 3;
const int PEDESTRIAN_TIME = 10;
const int MIN_GREEN_TIME = 10;
const int MAX_GREEN_TIME = 60;

int settingGreenNS = DEFAULT_GREEN_NS;
int activeGreenNS  = DEFAULT_GREEN_NS;

int settingGreenEW = DEFAULT_GREEN_EW;
int activeGreenEW  = DEFAULT_GREEN_EW;

// --- Định nghĩa các trạng thái hệ thống ---
enum SystemMode {
  MODE_NORMAL, 
  MODE_PED_TRANSITION, 
  MODE_PED_CROSSING, 
  MODE_EMERGENCY 
};

enum TrafficPhase {
  PHASE_NS_GREEN, 
  PHASE_NS_YELLOW, 
  PHASE_EW_GREEN, 
  PHASE_EW_YELLOW 
};

SystemMode currentMode = MODE_NORMAL;
TrafficPhase currentPhase = PHASE_NS_GREEN;
TrafficPhase resumePhase = PHASE_EW_GREEN; 

int timerNS = 0;
int timerEW = 0;
int timerPed = 0;
int timerTransition = 0;

unsigned long lastTickMillis = 0;
unsigned long lastBlinkMillis = 0;
bool emergencyBlinkState = false;

const uint8_t DASH_SEGMENTS[] = { SEG_G, SEG_G, SEG_G, SEG_G }; // Ký hiệu ----
const uint8_t BLANK_SEGMENTS[] = { 0x00, 0x00, 0x00, 0x00 }; // Ký hiệu rỗng để tắt màn hình

// Xóa màn hình bằng cách truyền mảng rỗng
void clearDisplays() {
  displayN.setSegments(BLANK_SEGMENTS);
  displayS.setSegments(BLANK_SEGMENTS);
  displayW.setSegments(BLANK_SEGMENTS);
  displayE.setSegments(BLANK_SEGMENTS);
}

// Cấu trúc chống nhiễu phím (Debounce)
struct ButtonDebounce {
  int pin;
  int lastReading;
  int stableState;
  unsigned long lastDebounceTime;
};

ButtonDebounce btnPed   = { PIN_BTN_PED,    HIGH, HIGH, 0 };
ButtonDebounce btnEmg   = { PIN_BTN_EMG,    HIGH, HIGH, 0 };
ButtonDebounce btnIncNS = { PIN_BTN_INC_NS, HIGH, HIGH, 0 };
ButtonDebounce btnDecNS = { PIN_BTN_DEC_NS, HIGH, HIGH, 0 };
ButtonDebounce btnIncEW = { PIN_BTN_INC_EW, HIGH, HIGH, 0 };
ButtonDebounce btnDecEW = { PIN_BTN_DEC_EW, HIGH, HIGH, 0 };

const unsigned long DEBOUNCE_DELAY_MS = 50;

bool isButtonPressed(ButtonDebounce &btn) {
  int reading = digitalRead(btn.pin);
  bool pressed = false;

  if (reading != btn.lastReading) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != btn.stableState) {
      btn.stableState = reading;
      if (btn.stableState == LOW) {
        pressed = true;
      }
    }
  }
  btn.lastReading = reading;
  return pressed;
}

void setLight(bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  digitalWrite(red1, r1); digitalWrite(yellow1, y1); digitalWrite(green1, g1);
  digitalWrite(red2, r2); digitalWrite(yellow2, y2); digitalWrite(green2, g2);
}
// Thêm hàm trợ giúp này để tự mã hóa 4 ô LED
void displayTime(TM1637Display &display, int timeVal)

{
  if (timeVal < 0)
  {
    display.setSegments(BLANK_SEGMENTS);
    return;
  }
  uint8_t segs[4];
  segs[0] = 0x00; // Tắt hoàn toàn ô LED 1 (hàng nghìn)
  segs[1] = 0x00; // Tắt hoàn toàn ô LED 2 (hàng trăm)
  
  if (timeVal > 99) timeVal = 99; // Giới hạn đồng hồ ở 2 con số
  
  // Tự mã hóa và hiển thị ở 2 ô LED cuối cùng
  segs[2] = display.encodeDigit(timeVal / 10); // Lấy số hàng chục
  segs[3] = display.encodeDigit(timeVal % 10); // Lấy số hàng đơn vị
  
  display.setSegments(segs); // Gửi trực tiếp xuống phần cứng
}

// Hàm updateDisplays mới gọi đến hàm trợ giúp ở trên
void updateDisplays(int valNS, int valEW) {
  displayTime(displayN, valNS);
  displayTime(displayS, valNS);
  displayTime(displayW, valEW);
  displayTime(displayE, valEW);
}
void showEmergencyDisplay(bool on) {
  if (on) {
    displayN.setSegments(DASH_SEGMENTS);
    displayS.setSegments(DASH_SEGMENTS);
    displayW.setSegments(DASH_SEGMENTS);
    displayE.setSegments(DASH_SEGMENTS);
  } else {
    clearDisplays();
  }
}

void startPhase(TrafficPhase phase) {
  currentPhase = phase;
  switch (phase) {
    case PHASE_NS_GREEN:
      activeGreenNS = settingGreenNS;
      timerNS = activeGreenNS;
      timerEW = activeGreenNS + YELLOW_TIME; 
      setLight(LOW, LOW, HIGH, HIGH, LOW, LOW); 
      break;

    case PHASE_NS_YELLOW:
      timerNS = YELLOW_TIME;
      setLight(LOW, HIGH, LOW, HIGH, LOW, LOW); 
      break;

    case PHASE_EW_GREEN:
      activeGreenEW = settingGreenEW;
      timerEW = activeGreenEW;
      timerNS = activeGreenEW + YELLOW_TIME; 
      setLight(HIGH, LOW, LOW, LOW, LOW, HIGH); 
      break;

    case PHASE_EW_YELLOW:
      timerEW = YELLOW_TIME;
      setLight(HIGH, LOW, LOW, LOW, HIGH, LOW);
      break;
  }
  updateDisplays(timerNS, timerEW);
}

void triggerPedestrianMode() {
  if (currentMode == MODE_EMERGENCY || currentMode == MODE_PED_CROSSING) return;

  if (currentMode == MODE_NORMAL) {
    if (currentPhase == PHASE_NS_GREEN) {
      currentMode = MODE_PED_TRANSITION;
      timerTransition = 5;
      resumePhase = PHASE_EW_GREEN; 
      setLight(LOW, HIGH, LOW, HIGH, LOW, LOW);
      updateDisplays(timerTransition, timerTransition);
    } else if (currentPhase == PHASE_EW_GREEN) {
      currentMode = MODE_PED_TRANSITION;
      timerTransition = 5;
      resumePhase = PHASE_NS_GREEN; 
      setLight(HIGH, LOW, LOW, LOW, HIGH, LOW);
      updateDisplays(timerTransition, timerTransition);
    } else {
      currentMode = MODE_PED_CROSSING;
      timerPed = PEDESTRIAN_TIME;
      setLight(HIGH, LOW, LOW, HIGH, LOW, LOW); 
      updateDisplays(timerPed, timerPed);
    }
  }
}

void toggleEmergencyMode() {
  if (currentMode != MODE_EMERGENCY) {
    currentMode = MODE_EMERGENCY;
    lastBlinkMillis = millis();
    emergencyBlinkState = true;
    setLight(LOW, HIGH, LOW, LOW, HIGH, LOW);
    showEmergencyDisplay(true);
  } else {
    currentMode = MODE_NORMAL;
    startPhase(PHASE_NS_GREEN);
  }
}

void increaseGreenTimeNS() { if (settingGreenNS + 5 <= MAX_GREEN_TIME) settingGreenNS += 5; }
void decreaseGreenTimeNS() { if (settingGreenNS - 5 >= MIN_GREEN_TIME) settingGreenNS -= 5; }
void increaseGreenTimeEW() { if (settingGreenEW + 5 <= MAX_GREEN_TIME) settingGreenEW += 5; }
void decreaseGreenTimeEW() { if (settingGreenEW - 5 >= MIN_GREEN_TIME) settingGreenEW -= 5; }

void handleButtons() {
  if (isButtonPressed(btnPed)) triggerPedestrianMode();
  if (isButtonPressed(btnEmg)) toggleEmergencyMode();
  if (isButtonPressed(btnIncNS)) increaseGreenTimeNS();
  if (isButtonPressed(btnDecNS)) decreaseGreenTimeNS();
  if (isButtonPressed(btnIncEW)) increaseGreenTimeEW();
  if (isButtonPressed(btnDecEW)) decreaseGreenTimeEW();
}

void setup() {
  pinMode(red1, OUTPUT); pinMode(yellow1, OUTPUT); pinMode(green1, OUTPUT);
  pinMode(red2, OUTPUT); pinMode(yellow2, OUTPUT); pinMode(green2, OUTPUT);

  pinMode(PIN_BTN_PED,    INPUT_PULLUP);
  pinMode(PIN_BTN_EMG,    INPUT_PULLUP);
  pinMode(PIN_BTN_INC_NS, INPUT_PULLUP);
  pinMode(PIN_BTN_DEC_NS, INPUT_PULLUP);
  pinMode(PIN_BTN_INC_EW, INPUT_PULLUP);
  pinMode(PIN_BTN_DEC_EW, INPUT_PULLUP);

  // Khởi tạo mức sáng tối đa và BẬT màn hình (tham số true cực kỳ quan trọng)
  displayN.setBrightness(7);
  displayS.setBrightness(7);
  displayW.setBrightness(7);
  displayE.setBrightness(7);
  
  // TEST: Hiển thị số 8888 ngay khi vừa bật nguồn
  displayN.showNumberDec(8888);
  displayS.showNumberDec(8888);
  displayW.showNumberDec(8888);
  displayE.showNumberDec(8888);
  
  // Dừng hệ thống 2 giây để bạn kiểm tra xem màn hình có sáng số 8888 không
  delay(2000); 

  clearDisplays();
  startPhase(PHASE_NS_GREEN);
  lastTickMillis = millis();
}

void loop() {
  handleButtons();
  unsigned long currentMillis = millis();

  if (currentMode == MODE_EMERGENCY) {
    if (currentMillis - lastBlinkMillis >= 500) {
      lastBlinkMillis = currentMillis;
      emergencyBlinkState = !emergencyBlinkState;
      if (emergencyBlinkState) {
        setLight(LOW, HIGH, LOW, LOW, HIGH, LOW); 
        showEmergencyDisplay(true);
      } else {
        setLight(LOW, LOW, LOW, LOW, LOW, LOW);  
        showEmergencyDisplay(false);
      }
    }
    return; 
  }

  if (currentMillis - lastTickMillis >= 1000) {
    lastTickMillis += 1000;

    if (currentMode == MODE_PED_TRANSITION) {
      timerTransition--;
      updateDisplays(timerTransition, timerTransition);
      if (timerTransition <= 0) {
        currentMode = MODE_PED_CROSSING;
        timerPed = PEDESTRIAN_TIME;
        setLight(HIGH, LOW, LOW, HIGH, LOW, LOW); 
        updateDisplays(timerPed, timerPed);
      }
    }
    else if (currentMode == MODE_PED_CROSSING) {
      timerPed--;
      updateDisplays(timerPed, timerPed);
      if (timerPed <= 0) {
        currentMode = MODE_NORMAL;
        startPhase(resumePhase);
      }
    }
    else if (currentMode == MODE_NORMAL) {
      timerNS--;
      timerEW--;
      updateDisplays(timerNS, timerEW);

      switch (currentPhase) {
        case PHASE_NS_GREEN:  if (timerNS <= 0) startPhase(PHASE_NS_YELLOW); break;
        case PHASE_NS_YELLOW: if (timerNS <= 0) startPhase(PHASE_EW_GREEN);  break;
        case PHASE_EW_GREEN:  if (timerEW <= 0) startPhase(PHASE_EW_YELLOW); break;
        case PHASE_EW_YELLOW: if (timerEW <= 0) startPhase(PHASE_NS_GREEN);  break;
      }
    }
  }
}