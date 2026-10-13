#include <Arduino.h>
#include <TM1637Display.h>

/* ==========================================================================
 * DỰ ÁN: ĐIỀU KHIỂN HỆ THỐNG ĐÈN GIAO THÔNG NGÃ TƯ THÔNG MINH
 * Vi điều khiển: Arduino Uno
 * 
 * PHÂN CÔNG CHÂN (PIN MAPPING):
 * - Trục Bắc - Nam (NS):
 *     Đèn: Đỏ (Pin 13), Vàng (Pin 12), Xanh (Pin 11)
 *     Màn hình TM1637 Bắc: CLK (Pin 3), DIO (Pin 2)
 *     Màn hình TM1637 Nam: CLK (Pin 5), DIO (Pin 4)
 * - Trục Đông - Tây (EW):
 *     Đèn: Đỏ (Pin 10), Vàng (Pin 9), Xanh (Pin 8)
 *     Màn hình TM1637 Tây: CLK (Pin 7), DIO (Pin 6)
 *     Màn hình TM1637 Đông: CLK (Pin A1), DIO (Pin A0)
 * - Nút bấm (Push Buttons - INPUT_PULLUP, nối đất GND khi bấm):
 *     Pin A2: Nút NGƯỜI ĐI BỘ (Pedestrian)
 *     Pin A3: Nút CHẾ ĐỘ KHẨN CẤP (Emergency - Toggle)
 *     Pin A4: Nút TĂNG THỜI GIAN ĐÈN XANH BẮC-NAM (+5s)
 *     Pin A5: Nút GIẢM THỜI GIAN ĐÈN XANH BẮC-NAM (-5s)
 *     Pin 0:  Nút TĂNG THỜI GIAN ĐÈN XANH ĐÔNG-TÂY (+5s)
 *     Pin 1:  Nút GIẢM THỜI GIAN ĐÈN XANH ĐÔNG-TÂY (-5s)
 * ========================================================================== */

// --- Cấu hình chân Đèn LED ---
const int red1 = 13, yellow1 = 12, green1 = 11; // Trục NS
const int red2 = 10, yellow2 = 9, green2 = 8;   // Trục EW

// --- Cấu hình chân Nút bấm ---
const int PIN_BTN_PED    = A2; // Nút Người đi bộ
const int PIN_BTN_EMG    = A3; // Nút Khẩn cấp
const int PIN_BTN_INC_NS = A4; // Nút Tăng thời gian Xanh NS (+5s)
const int PIN_BTN_DEC_NS = A5; // Nút Giảm thời gian Xanh NS (-5s)
const int PIN_BTN_INC_EW = 0;  // Nút Tăng thời gian Xanh EW (+5s)
const int PIN_BTN_DEC_EW = 1;  // Nút Giảm thời gian Xanh EW (-5s)

// --- Khởi tạo 4 màn hình hiển thị TM1637 ---
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

// Thời gian cài đặt cho chu kỳ tiếp theo và thời gian đang chạy hiện tại
int settingGreenNS = DEFAULT_GREEN_NS;
int activeGreenNS  = DEFAULT_GREEN_NS;

int settingGreenEW = DEFAULT_GREEN_EW;
int activeGreenEW  = DEFAULT_GREEN_EW;

// --- Định nghĩa các trạng thái hệ thống (Finite State Machine) ---
enum SystemMode {
  MODE_NORMAL,         // Chu kỳ bình thường
  MODE_PED_TRANSITION, // Chuyển tiếp đèn vàng trước khi nhường đường cho người đi bộ
  MODE_PED_CROSSING,   // Đang trong 10s ưu tiên người đi bộ
  MODE_EMERGENCY       // Chế độ khẩn cấp: nhấp nháy đèn vàng tất cả các hướng
};

enum TrafficPhase {
  PHASE_NS_GREEN,  // NS Xanh | EW Đỏ
  PHASE_NS_YELLOW, // NS Vàng | EW Đỏ
  PHASE_EW_GREEN,  // NS Đỏ | EW Xanh
  PHASE_EW_YELLOW  // NS Đỏ | EW Vàng
};

SystemMode currentMode = MODE_NORMAL;
TrafficPhase currentPhase = PHASE_NS_GREEN;
TrafficPhase resumePhase = PHASE_EW_GREEN; // Pha sẽ chạy tiếp sau khi người đi bộ qua đường

// Biến đếm thời gian pha
int timerNS = 0;
int timerEW = 0;
int timerPed = 0;
int timerTransition = 0;

// Quản lý nhịp thời gian không chặn (non-blocking timing)
unsigned long lastTickMillis = 0;
unsigned long lastBlinkMillis = 0;
bool emergencyBlinkState = false;

// Ký hiệu hiển thị gạch ngang (DASH: --) trên màn hình TM1637 khi khẩn cấp
const uint8_t DASH_SEGMENTS[] = { SEG_G, SEG_G };
const uint8_t BLANK_SEGMENTS[] = { 0x00, 0x00 };

// Xóa 2 chữ số hiển thị
void clearDisplay(TM1637Display &disp) {
  disp.setSegments(BLANK_SEGMENTS, 2, 2);
}

// --- Cấu trúc Debounce Nút bấm ---
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

// Hàm kiểm tra nút bấm có được nhấn xuống hay không (bắt sườn âm Falling Edge)
bool isButtonPressed(ButtonDebounce &btn) {
  int reading = digitalRead(btn.pin);
  bool pressed = false;

  if (reading != btn.lastReading) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_DELAY_MS) {
    if (reading != btn.stableState) {
      btn.stableState = reading;
      // Nhấn xuống (LOW do dùng INPUT_PULLUP)
      if (btn.stableState == LOW) {
        pressed = true;
      }
    }
  }

  btn.lastReading = reading;
  return pressed;
}

// Điều khiển bật tắt các cụm đèn LED
void setLight(bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  digitalWrite(red1, r1); digitalWrite(yellow1, y1); digitalWrite(green1, g1);
  digitalWrite(red2, r2); digitalWrite(yellow2, y2); digitalWrite(green2, g2);
}

// Cập nhật số đếm ngược lên các màn hình TM1637
void updateDisplays(int valNS, int valEW) {
  if (valNS >= 0) {
    displayN.showNumberDec(valNS, true, 2, 2);
    displayS.showNumberDec(valNS, true, 2, 2);
  } else {
    clearDisplay(displayN);
    clearDisplay(displayS);
  }

  if (valEW >= 0) {
    displayW.showNumberDec(valEW, true, 2, 2);
    displayE.showNumberDec(valEW, true, 2, 2);
  } else {
    clearDisplay(displayW);
    clearDisplay(displayE);
  }
}

// Hiển thị ký hiệu khẩn cấp trên các màn hình TM1637
void showEmergencyDisplay(bool on) {
  if (on) {
    displayN.setSegments(DASH_SEGMENTS, 2, 2);
    displayS.setSegments(DASH_SEGMENTS, 2, 2);
    displayW.setSegments(DASH_SEGMENTS, 2, 2);
    displayE.setSegments(DASH_SEGMENTS, 2, 2);
  } else {
    clearDisplay(displayN);
    clearDisplay(displayS);
    clearDisplay(displayW);
    clearDisplay(displayE);
  }
}

// Khởi động pha đèn trong chu kỳ bình thường
void startPhase(TrafficPhase phase) {
  currentPhase = phase;
  switch (phase) {
    case PHASE_NS_GREEN:
      activeGreenNS = settingGreenNS; // Áp dụng thời gian cài đặt mới của NS!
      timerNS = activeGreenNS;
      timerEW = activeGreenNS + YELLOW_TIME; // Đỏ EW = Xanh NS + Vàng NS
      setLight(LOW, LOW, HIGH, HIGH, LOW, LOW); // NS Xanh, EW Đỏ
      break;

    case PHASE_NS_YELLOW:
      timerNS = YELLOW_TIME;
      // timerEW giữ nguyên để đếm nốt 3s cuối của đèn đỏ
      setLight(LOW, HIGH, LOW, HIGH, LOW, LOW); // NS Vàng, EW Đỏ
      break;

    case PHASE_EW_GREEN:
      activeGreenEW = settingGreenEW; // Áp dụng thời gian cài đặt mới của EW!
      timerEW = activeGreenEW;
      timerNS = activeGreenEW + YELLOW_TIME; // Đỏ NS = Xanh EW + Vàng EW
      setLight(HIGH, LOW, LOW, LOW, LOW, HIGH); // NS Đỏ, EW Xanh
      break;

    case PHASE_EW_YELLOW:
      timerEW = YELLOW_TIME;
      // timerNS giữ nguyên để đếm nốt 3s cuối của đèn đỏ
      setLight(HIGH, LOW, LOW, LOW, HIGH, LOW); // NS Đỏ, EW Vàng
      break;
  }
  updateDisplays(timerNS, timerEW);
}

// Kích hoạt chế độ Người đi bộ
void triggerPedestrianMode() {
  if (currentMode == MODE_EMERGENCY || currentMode == MODE_PED_CROSSING) {
    return; // Đang khẩn cấp hoặc đang ưu tiên đi bộ thì không kích hoạt lại
  }

  if (currentMode == MODE_NORMAL) {
    if (currentPhase == PHASE_NS_GREEN) {
      // Trục NS đang xanh -> chuyển sang Vàng 3s ngay, sau đó nhường đường
      currentMode = MODE_PED_TRANSITION;
      timerTransition = YELLOW_TIME;
      resumePhase = PHASE_EW_GREEN; // Xong đi bộ thì đến lượt EW xanh
      setLight(LOW, HIGH, LOW, HIGH, LOW, LOW);
      updateDisplays(timerTransition, timerTransition);
    } else if (currentPhase == PHASE_EW_GREEN) {
      // Trục EW đang xanh -> chuyển sang Vàng 3s ngay, sau đó nhường đường
      currentMode = MODE_PED_TRANSITION;
      timerTransition = YELLOW_TIME;
      resumePhase = PHASE_NS_GREEN; // Xong đi bộ thì đến lượt NS xanh
      setLight(HIGH, LOW, LOW, LOW, HIGH, LOW);
      updateDisplays(timerTransition, timerTransition);
    } else {
      // Đang ở pha Vàng sẵn -> vào thẳng chế độ người đi bộ
      currentMode = MODE_PED_CROSSING;
      timerPed = PEDESTRIAN_TIME;
      setLight(HIGH, LOW, LOW, HIGH, LOW, LOW); // Cả 2 trục ĐỎ
      updateDisplays(timerPed, timerPed);
    }
  }
}

// Kích hoạt / Tắt Chế độ khẩn cấp (Toggle)
void toggleEmergencyMode() {
  if (currentMode != MODE_EMERGENCY) {
    currentMode = MODE_EMERGENCY;
    lastBlinkMillis = millis();
    emergencyBlinkState = true;
    // Bật tất cả đèn Vàng, tắt Đỏ và Xanh
    setLight(LOW, HIGH, LOW, LOW, HIGH, LOW);
    showEmergencyDisplay(true);
  } else {
    currentMode = MODE_NORMAL;
    startPhase(PHASE_NS_GREEN);
  }
}

// Tăng thời gian đèn Xanh NS thêm 5s cho chu kỳ tới
void increaseGreenTimeNS() {
  if (settingGreenNS + 5 <= MAX_GREEN_TIME) {
    settingGreenNS += 5;
  }
}

// Giảm thời gian đèn Xanh NS bớt 5s cho chu kỳ tới
void decreaseGreenTimeNS() {
  if (settingGreenNS - 5 >= MIN_GREEN_TIME) {
    settingGreenNS -= 5;
  }
}

// Tăng thời gian đèn Xanh EW thêm 5s cho chu kỳ tới
void increaseGreenTimeEW() {
  if (settingGreenEW + 5 <= MAX_GREEN_TIME) {
    settingGreenEW += 5;
  }
}

// Giảm thời gian đèn Xanh EW bớt 5s cho chu kỳ tới
void decreaseGreenTimeEW() {
  if (settingGreenEW - 5 >= MIN_GREEN_TIME) {
    settingGreenEW -= 5;
  }
}

// Quản lý kiểm tra tất cả các nút bấm liên tục (Polling không chặn)
void handleButtons() {
  if (isButtonPressed(btnPed)) {
    triggerPedestrianMode();
  }
  if (isButtonPressed(btnEmg)) {
    toggleEmergencyMode();
  }
  if (isButtonPressed(btnIncNS)) {
    increaseGreenTimeNS();
  }
  if (isButtonPressed(btnDecNS)) {
    decreaseGreenTimeNS();
  }
  if (isButtonPressed(btnIncEW)) {
    increaseGreenTimeEW();
  }
  if (isButtonPressed(btnDecEW)) {
    decreaseGreenTimeEW();
  }
}

void setup() {
  // Khởi tạo các chân LED
  pinMode(red1, OUTPUT); pinMode(yellow1, OUTPUT); pinMode(green1, OUTPUT);
  pinMode(red2, OUTPUT); pinMode(yellow2, OUTPUT); pinMode(green2, OUTPUT);

  // Khởi tạo các chân Nút bấm với điện trở kéo lên nội (Pull-up)
  pinMode(PIN_BTN_PED,    INPUT_PULLUP);
  pinMode(PIN_BTN_EMG,    INPUT_PULLUP);
  pinMode(PIN_BTN_INC_NS, INPUT_PULLUP);
  pinMode(PIN_BTN_DEC_NS, INPUT_PULLUP);
  pinMode(PIN_BTN_INC_EW, INPUT_PULLUP);
  pinMode(PIN_BTN_DEC_EW, INPUT_PULLUP);

  // Khởi tạo độ sáng và BẬT màn hình TM1637 (0x0f: Bit 3 = 1 bật hiển thị, Bit 0-2 = 7 độ sáng tối đa)
  displayN.setBrightness(0x0f);
  displayS.setBrightness(0x0f);
  displayW.setBrightness(0x0f);
  displayE.setBrightness(0x0f);

  // Bắt đầu chu kỳ pha đầu tiên
  startPhase(PHASE_NS_GREEN);
  lastTickMillis = millis();
}

void loop() {
  // 1. Luôn kiểm tra nút bấm liên tục (Non-blocking polling có debounce)
  handleButtons();

  unsigned long currentMillis = millis();

  // 2. Xử lý chế độ KHẨN CẤP (nhấp nháy đèn Vàng mỗi 500ms)
  if (currentMode == MODE_EMERGENCY) {
    if (currentMillis - lastBlinkMillis >= 500) {
      lastBlinkMillis = currentMillis;
      emergencyBlinkState = !emergencyBlinkState;
      if (emergencyBlinkState) {
        setLight(LOW, HIGH, LOW, LOW, HIGH, LOW); // Bật tất cả đèn Vàng
        showEmergencyDisplay(true);
      } else {
        setLight(LOW, LOW, LOW, LOW, LOW, LOW);   // Tắt tất cả các đèn
        showEmergencyDisplay(false);
      }
    }
    return; // Khi khẩn cấp, bỏ qua bộ đếm chu kỳ bình thường
  }

  // 3. Xử lý bộ đếm thời gian 1 giây cho các chế độ khác
  if (currentMillis - lastTickMillis >= 1000) {
    lastTickMillis += 1000;

    // --- Chế độ Chuyển tiếp trước khi người đi bộ qua đường (Vàng 3s) ---
    if (currentMode == MODE_PED_TRANSITION) {
      timerTransition--;
      updateDisplays(timerTransition, timerTransition);
      if (timerTransition <= 0) {
        // Hết 3s Vàng -> Cả 2 trục chuyển sang ĐỎ để người đi bộ qua đường trong 10s
        currentMode = MODE_PED_CROSSING;
        timerPed = PEDESTRIAN_TIME;
        setLight(HIGH, LOW, LOW, HIGH, LOW, LOW); // Cả 2 trục ĐỎ
        updateDisplays(timerPed, timerPed);
      }
    }
    // --- Chế độ Người đi bộ đang qua đường (10s) ---
    else if (currentMode == MODE_PED_CROSSING) {
      timerPed--;
      updateDisplays(timerPed, timerPed);
      if (timerPed <= 0) {
        // Hết 10s người đi bộ -> Trả lại chu kỳ bình thường
        currentMode = MODE_NORMAL;
        startPhase(resumePhase);
      }
    }
    // --- Chế độ Chu kỳ bình thường ---
    else if (currentMode == MODE_NORMAL) {
      timerNS--;
      timerEW--;

      updateDisplays(timerNS, timerEW);

      // Kiểm tra chuyển pha
      switch (currentPhase) {
        case PHASE_NS_GREEN:
          if (timerNS <= 0) {
            startPhase(PHASE_NS_YELLOW);
          }
          break;

        case PHASE_NS_YELLOW:
          if (timerNS <= 0) {
            startPhase(PHASE_EW_GREEN);
          }
          break;

        case PHASE_EW_GREEN:
          if (timerEW <= 0) {
            startPhase(PHASE_EW_YELLOW);
          }
          break;

        case PHASE_EW_YELLOW:
          if (timerEW <= 0) {
            startPhase(PHASE_NS_GREEN);
          }
          break;
      }
    }
  }
}