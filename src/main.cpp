#include <Arduino.h>
#include <TM1637Display.h>

const int red1 = 13, yellow1 = 12, green1 = 11;
const int red2 = 10, yellow2 = 9, green2 = 8;

TM1637Display displayNS(3, 2);
TM1637Display displayEW(5, 4);

void setup() {
  pinMode(red1, OUTPUT); pinMode(yellow1, OUTPUT); pinMode(green1, OUTPUT);
  pinMode(red2, OUTPUT); pinMode(yellow2, OUTPUT); pinMode(green2, OUTPUT);

  // 0x0f là mã lệnh ép buộc màn hình TM1637 phải BẬT và SÁNG TỐI ĐA
  displayNS.setBrightness(0x0f);
  displayEW.setBrightness(0x0f);
}

void setLight(bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  digitalWrite(red1, r1); digitalWrite(yellow1, y1); digitalWrite(green1, g1);
  digitalWrite(red2, r2); digitalWrite(yellow2, y2); digitalWrite(green2, g2);
}

void runPhase(int timeNS, int timeEW, bool r1, bool y1, bool g1, bool r2, bool y2, bool g2) {
  setLight(r1, y1, g1, r2, y2, g2);

  int timerNS = timeNS;
  int timerEW = timeEW;
  int maxTime = max(timeNS, timeEW);

  for (int i = 0; i < maxTime; i++) {
    // Nếu vẫn còn thời gian thì đếm ngược (hiển thị 05, 04..), nếu hết thì giữ ở mức 00
    if (timerNS >= 0) displayNS.showNumberDec(timerNS, true, 2, 2);
    else displayNS.showNumberDec(0, true, 2, 2);

    if (timerEW >= 0) displayEW.showNumberDec(timerEW, true, 2, 2);
    else displayEW.showNumberDec(0, true, 2, 2);

    delay(1000);
    timerNS--;
    timerEW--;
  }
}

void loop() {
  runPhase(5, 7, LOW, LOW, HIGH, HIGH, LOW, LOW);
  runPhase(2, 2, LOW, HIGH, LOW, HIGH, LOW, LOW);
  runPhase(7, 5, HIGH, LOW, LOW, LOW, LOW, HIGH);
  runPhase(2, 2, HIGH, LOW, LOW, LOW, HIGH, LOW);
}