/**
   JellibiAGV 보드용 샘플
     D2 : RFID Reader control : SPI SS(Slave Select)
     D3 : Buzzer (Piezo Speaker)
     D4 : RFID Reader control : Module Reset
     D5 : Left side DC MOTOR : PWM  : 회전속도제어
     D6 : Right side DC MOTOR : PWM : 회전속도제어
     D7 : Left side DC MOTOR : GPIO : 회전방향제어
     D8 : Right side DC MOTOR : GPIO : 회전방향제어
     D9 : Servo 1  : PWM
     D10 : Servo 2 : PWM
     D11 : RFID Reader control : SPI MOSI
     D12 : RFID Reader control : SPI MISO
     D13 : RFID Reader control : SPI SCL
     A3 : 측면 사용자 버튼 : GPI
     A0 : 전면 센서보드 가운데 IR 센서 : ADC
     A1 : 전면 센서보드 왼쪽 IR 센서 : ADC
     A2 : 전면 센서보드 오른쪽 IR 센서 : ADC
     A6 : 바닥면 센서보드 왼쪽 IR 센서 : ADC
     A7 : 바닥면 센서보드 오른쪽 IR 센서 : ADC
*/
#include <Servo.h>

#include <deprecated.h>
#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <require_cpp11.h>
#include <SPI.h>



#define SERVO_DOWN  25//90-30   // Down 위치 (10 정도 더 아래)
#define SERVO_UP    160     // Up 위치 (떨림 방지)
#define SERVO_DEF   SERVO_DOWN   // 기본 위치

#define PIN_SERVO   9   // 리프터(서보)모터 핀 번호

#define BLACK 1
#define WHITE 0

#define WRP 6
#define WRD 8
#define WLP 5
#define WLD 7

#define IR_LEFT A6
#define IR_RIGHT A7

#define FORWARD 0
#define BACKWARD 1

#define BUZZER_PIN 3

#define RFID_SS_PIN     2 // SPI 통신용 SS(Slave Select)
#define RFID_RST_PIN    4 // RESET 핀

float Power1Ratio = 1.00;//0.95; // 왼쪽 모터 속도 보정비율 (최대= 1.0)
float Power2Ratio = 1.00;//1.00; // 오른쪽 모터 속도 보정비율 (최대= 1.0)

bool isLifterUp = false;

Servo servo;

MFRC522 mfrc522( RFID_SS_PIN, RFID_RST_PIN ); // 객체 인스턴스 선언

const char* testpath = "F*FF!-FLF!F-FTU!FFFF--FRFFLFFDLFFLF!FF*FFF!----FL"; // D -> 3 -> 서울 -> 3
const char* testpath2 = "U'--F'LFFF'RF'LF!F-FF'LFFDLFF!F*FFF!-FTU"; //3 -> 세종 -> 3
//const char* testpath2 = "U----FS";
const char* testpath3 = "--FFF-F'LF'RFFFDRFRF!FF*FFF!-FLU"; //3 -> 대전 -> 3
const char* testpath4 = "--FF'LF!FFFFF---F'LFDL!-FFRFLFF*FF-FTU"; //3 -> 인천 -> 3 
const char* testpath5 = "---FFFFF'RF'LFFDL!-FLF!F*FFFFFS"; //3 -> 인천 -> 3

void executePath(const char* path, int basePower) {
  int power = basePower;
  
  for (int i = 0; path[i] != '\0'; i++) {
    char cmd = path[i];
    Serial.print("▶ 명령: "); Serial.println(cmd);

    switch (cmd) {
      case 'F':
        moveIrForward(power);
        break;
      case 'B':
        moveIrBackward(power);
        break;
      case 'L':
        rotateLeft(80);
        break;
      case 'R':
        rotateRight(80);
        break;
      case 'T':
        rotate180(80);
        break;
      case 'U':
        lifterUp();
        break;
      case 'D':
        lifterDown();
        break;
      case 'S':
        Stop();
        delay(500);
        break;
      case 'W':
        waitCardTag();
        break;
      case '*':
        power = basePower + 50;  
        break;
      case '!':
        power = basePower;       // 다시 원래 속도로
        break;
      case '-':
        power = basePower - 25;
        break;
      case '`':
        Stop();
        delay(50);
        break;
      default:
        Serial.print("⚠️ 알 수 없는 명령어: ");
        Serial.println(cmd);
        break;
    }
  }
}


void setup(){
  Serial.begin(115200);

  SPI.begin(); // SPI 통신 초기화
  mfrc522.PCD_Init(); // MFRC522 초기화

 

  pinMode(WRP, OUTPUT);
  pinMode(WRD, OUTPUT);
  pinMode(WLP, OUTPUT);
  pinMode(WLD, OUTPUT);
  pinMode(A3, INPUT_PULLUP); // A3 핀을 풀업 저항으로 입력 모드 설정

/*

  waitCardTag();
  executePath(testpath, 120); 
  executePath(testpath2, 120);
  executePath(testpath3, 120);  
  executePath(testpath4, 120);
  executePath(testpath5, 120);

*/
  waitCardTag();
  lifterUp();
  delay(1000);
  lifterDown();

/*
  waitCardTag();
  rotateRight(80);
  waitCardTag();
  drive(FORWARD,120,FORWARD, 120);
  delay(1000);
*/


}

void loop(){
  //checkUserButtonLiftUpDown();
}


/*
void drive(int dir1, int power1, int dir2, int power2) {
  boolean dirHighLow1 = (dir1 == FORWARD) ? HIGH : LOW;
  boolean dirHighLow2 = (dir2 == FORWARD) ? LOW : HIGH;
  digitalWrite(WLD, dirHighLow1);
  analogWrite(WLP, power1);
  digitalWrite(WRD, dirHighLow2);
  analogWrite(WRP, power2);
}
*/

void  drive(int dir1, int power1, int dir2, int power2)
{
  boolean dirHighLow1, dirHighLow2;
  int     adjPower1, adjPower2;

  if(dir1 == FORWARD)  // 1번(왼쪽)모터 방향
    dirHighLow1 = HIGH;
  else // BACKWARD
    dirHighLow1 = LOW;

  adjPower1 = power1 * Power1Ratio; // 왼쪽모터 성능 조정
  
  if(dir2 == FORWARD)  // 2번(오른쪽)모터
    dirHighLow2 = LOW;
  else // BACKWARD
    dirHighLow2 = HIGH;

  adjPower2 = power2 * Power2Ratio; // 오른쪽모터 성능 조정

  digitalWrite(WLD, dirHighLow1); // 왼쪽모터 회전 방향
  analogWrite(WLP, adjPower1); // 조정된 왼쪽모터 속력

  digitalWrite(WRD, dirHighLow2); // 오른쪽모터 회전 방향
  analogWrite(WRP, adjPower2); // 조정된 오른쪽모터 속력
}



void moveIrForward(int power) { 
  int irVal[2];
  drive(FORWARD, power, FORWARD, power); 
  delay(10);

  while (true) {
    bw(irVal);

    if (irVal[0] == BLACK && irVal[1] == BLACK) {
      int delayVal = 85;
      if (isLifterUp) { delayVal = 60; }
      else { delayVal = 67; }

      buzzer(262, delayVal);

      for (int i = 0; i < 10; i++) {
        bw(irVal); // 센서값 갱신
        if (irVal[0] == WHITE && irVal[1] == WHITE) {
          break;
        } else {
          drive(FORWARD, power, FORWARD, power);
          delay(5);
        }
      }
      break;
    }

    if (irVal[0] == WHITE && irVal[1] == WHITE) {
      drive(FORWARD, power, FORWARD, power);
    } else if (irVal[0] == BLACK && irVal[1] == WHITE) {
      if (isLifterUp) {
        drive(FORWARD, power - 18, FORWARD, power); //리프트 올라갔을 때 라인 보정값 수정
      } else {
        drive(FORWARD, power - 17, FORWARD, power); //기존값
      }
    } else if (irVal[0] == WHITE && irVal[1] == BLACK) {
      if (isLifterUp) {
        drive(FORWARD, power, FORWARD, power - 18); //리프트 올라갔을 때 라인 보정값 수정
      } else {
        drive(FORWARD, power, FORWARD, power - 17); //기존값
      }
    }
  }

  Stop();
}

void moveIrBackward(int power) { drive(BACKWARD, power, BACKWARD, power); }

void rotateLeft(int power) {
  if (isLifterUp) {
    drive(BACKWARD, power, FORWARD, power);
    delay(506);  // 보정된 회전 시간
  } else {
    drive(BACKWARD, power, FORWARD, power);
    delay(496); //기존 회전 시간
  }
  Stop();
}

void rotateRight(int power) {
  if (isLifterUp) {
    drive(FORWARD, power, BACKWARD, power);
    delay(534);  // 회전 시간은 상황에 따라 조정
  } else {
    // 기본 우회전
    drive(FORWARD, power, BACKWARD, power);
    delay(528);
  }
  Stop();
}

void rotate180(int power) {
  if (isLifterUp) {
    drive(BACKWARD, power, FORWARD, power);
    delay(1030);  // 보정된 시간
  } else {
    drive(BACKWARD, power, FORWARD, power);
    delay(1001); //기존 시간
  }
  Stop();
}



void Stop() {
  analogWrite(WLP, 0);
  analogWrite(WRP, 0);
}



void bw(int result[]){
  int ir_left = analogRead(IR_LEFT);
  int ir_right = analogRead(IR_RIGHT);

  if(ir_left < 726){  result[0] = WHITE;  }
  else { result[0] = BLACK; }

  if(ir_right < 679){  result[1] = WHITE;  }
  else{ result[1] = BLACK; }
}

void buzzer(int val, int time){
  tone(BUZZER_PIN, val);
  delay(time);
  noTone(BUZZER_PIN);
}

// 여기부터 리프트 
void lifterUp()
{
  servo.attach(PIN_SERVO); // 서보 연결
  delay(10);

  for (int angle = SERVO_DOWN; angle <= SERVO_UP; angle += 4) {
    servo.write(angle);
    delay(15);  // 각도 변경 간의 지연 시간 (조절 가능)
  }

  delay(100);  // 마지막 도달 후 안정화 시간
  servo.detach(); // 서보 연결 해제
  delay(10);  

  isLifterUp = true;
}


void lifterDown()
{
  servo.attach( PIN_SERVO ); // 서보 연결
  delay( 10 );
  
  servo.write( SERVO_DOWN ); // 리프터 아래로 내림
  delay( 300 );

  servo.detach(); // 서보 연결 해제
  delay( 10 );  

  isLifterUp = false;
}

void waitCardTag() 
{
  // 카드가 태깅될 때까지 계속 대기
  while (!mfrc522.PICC_IsNewCardPresent()) {

    delay(500);
  }
  if( ! mfrc522.PICC_ReadCardSerial() )
  {
    delay( 500 );
    return;
  }
}

void checkUserButtonLiftUpDown() {
  if (digitalRead(A3) == LOW) { // 버튼이 눌렸을 때 (풀업저항)
    lifterUp();
  } else {
    lifterDown();
  }
}