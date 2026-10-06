#include <Arduino.h>
#include <TM1637Display.h>

const int red1 = 13, yellow1 = 12, green1 = 11;
const int red2 = 10, yellow2 = 9, green2 = 8;

TM1637Display displayN(3, 2);
TM1637Display displayS(5, 4);
TM1637Display displayW(7, 6);
TM1637Display displayE(A1, A0);

void setup() {
  pinMode(red1, OUTPUT); pinMode(yellow1, OUTPUT); pinMode(green1, OUTPUT);
  pinMode(red2, OUTPUT); pinMode(yellow2, OUTPUT); pinMode(green2, OUTPUT);

  displayN.setBrightness(0x0f);
  displayS.setBrightness(0x0f);
  displayW.setBrightness(0x0f);
  displayE.setBrightness(0x0f);
}

void setLight(bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  digitalWrite(red1, r1); digitalWrite(yellow1, y1); digitalWrite(green1, g1);
  digitalWrite(red2, r2); digitalWrite(yellow2, y2); digitalWrite(green2, g2);
}

// Tham số duration quyết định đèn sáng bao lâu. Tham số &timerNS và &timerEW giữ số đếm hiện tại.
void runPhase(int duration, int &timerNS, int &timerEW, bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  setLight(r1, y1, g1, r2, y2, g2);

  for (int i = 0; i < duration; i++) {
    // Hiển thị cho trục Bắc - Nam
    if (timerNS > 0) {
      displayN.showNumberDec(timerNS, true, 2, 2);
      displayS.showNumberDec(timerNS, true, 2, 2);
    }
    
    // Hiển thị cho trục Đông - Tây
    if (timerEW > 0) {
      displayW.showNumberDec(timerEW, true, 2, 2);
      displayE.showNumberDec(timerEW, true, 2, 2);
    }

    delay(1000);
    timerNS--;
    timerEW--;
  }
}

void loop() {
  int tNS, tEW;

  // Pha 1: Trục NS Xanh (chạy 30s) | Trục EW Đỏ (bắt đầu đếm từ 33s)
  tNS = 30; tEW = 33;
  runPhase(30, tNS, tEW, LOW, LOW, HIGH, HIGH, LOW, LOW);

  // Pha 2: Trục NS chuyển Vàng (chạy 3s) | Trục EW tiếp tục Đỏ (đang đếm nốt 3s cuối)
  tNS = 3; 
  runPhase(3, tNS, tEW, LOW, HIGH, LOW, HIGH, LOW, LOW);

  // Pha 3: Trục NS chuyển Đỏ (bắt đầu đếm từ 33s) | Trục EW chuyển Xanh (chạy 30s)
  tNS = 33; tEW = 30;
  runPhase(30, tNS, tEW, HIGH, LOW, LOW, LOW, LOW, HIGH);

  // Pha 4: Trục NS tiếp tục Đỏ (đang đếm nốt 3s cuối) | Trục EW chuyển Vàng (chạy 3s)
  tEW = 3;
  runPhase(3, tNS, tEW, HIGH, LOW, LOW, LOW, HIGH, LOW);
}