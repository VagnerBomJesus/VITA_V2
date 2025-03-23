#include <Wire.h>
#include <Servo.h>
#include "person_sensor.h"

// === Servo motors ===
Servo top_left_eyelid, bottom_left_eyelid;
Servo top_right_eyelid, bottom_right_eyelid;
Servo Yarm, Xarm; // Yarm: vertical; Xarm: horizontal (olhos esquerda/direita)

// === Parâmetros de deteção facial ===
const int32_t SAMPLE_DELAY_MS = 100;
const int MAX_FACES = PERSON_SENSOR_MAX_FACES_COUNT;
const int MIN_CONFIDENCE = 80;
const int POSITION_MARGIN = 10;
const unsigned long NO_FACE_TIMEOUT = 30000; // 30 segundos

// Intervalos considerados como “centralização”
const int BOX_LEFT_MIN = 100;
const int BOX_LEFT_MAX = 110;
const int BOX_RIGHT_MIN = 130;
const int BOX_RIGHT_MAX = 140;

// === Estado global ===
unsigned long lastFaceDetectedTime = 0;
bool facePresent = false;
int previousFaceCount = -1;
String previousState[MAX_FACES];
int lastCenterX[MAX_FACES];

// === Funções de movimento dos olhos ===
void closeEyes() {
  top_left_eyelid.write(135);
  bottom_left_eyelid.write(140);
  top_right_eyelid.write(90);
  bottom_right_eyelid.write(50);
}

void openEyes() {
  top_left_eyelid.write(70);
  bottom_left_eyelid.write(50);
  top_right_eyelid.write(160);
  bottom_right_eyelid.write(135);
}

void centerEyes() {
  Xarm.write(90);
  Yarm.write(120);
}

void moveEyesProportional(int centerX) {
  // Ajuste baseado na faixa real observada (100 a 150)
  int angle = map(centerX, 100, 150, 130, 50);
  angle = constrain(angle, 50, 130);
  Serial.print(" >> Ângulo dos olhos: ");
  Serial.println(angle);
  Xarm.write(angle);
}

// Mostrar visualmente a posição da face no terminal
void drawFacePosition(int centerX, int boxLeft, int boxRight) {
  const int width = 64;
  const int maxSensorX = 255;

  int posCenter = map(centerX, 0, maxSensorX, 0, width - 1);
  int posLeft = map(BOX_LEFT_MIN, 0, maxSensorX, 0, width - 1);
  int posRight = map(BOX_RIGHT_MAX, 0, maxSensorX, 0, width - 1);

  Serial.print("|");
  for (int i = 0; i < width; i++) {
    if (i == posCenter) Serial.print("📍");
    else if (i >= posLeft && i <= posRight) Serial.print("=");
    else Serial.print("-");
  }
  Serial.println("|");

  Serial.print("CentroX: ");
  Serial.print(centerX);
  Serial.print("  (entre ");
  Serial.print(boxLeft);
  Serial.print(" e ");
  Serial.print(boxRight);
  Serial.println(")");
}

// === Setup inicial ===
void setup() {
  Wire.begin();
  Serial.begin(57600);

  top_left_eyelid.attach(2);
  bottom_left_eyelid.attach(3);
  top_right_eyelid.attach(4);
  bottom_right_eyelid.attach(5);
  Yarm.attach(6);
  Xarm.attach(7);

  closeEyes();
  centerEyes();
  delay(1000);
}

// === Ciclo principal ===
void loop() {
  person_sensor_results_t results = {};
  bool faceDetectedNow = false;

  if (person_sensor_read(&results) && results.num_faces > 0) {
    for (int i = 0; i < results.num_faces; ++i) {
      const person_sensor_face_t* face = &results.faces[i];
      if (face->box_confidence < MIN_CONFIDENCE) continue;

      int centerX = (face->box_left + face->box_right) / 2;
      if (abs(centerX - lastCenterX[i]) < POSITION_MARGIN && previousState[i] != "") continue;
      lastCenterX[i] = centerX;

      String currentState = "Face #" + String(i) + ": Confiança = " + String(face->box_confidence);
      currentState += ", Caixa = (" + String(face->box_left) + ", " + String(face->box_top) + ")";
      currentState += " até (" + String(face->box_right) + ", " + String(face->box_bottom) + "), ";
      currentState += face->is_facing ? "a olhar para a câmara" : "não está a olhar para a câmara";

      // Verificar centralização
      bool isCentered =
        face->box_left >= BOX_LEFT_MIN && face->box_left <= BOX_LEFT_MAX &&
        face->box_right >= BOX_RIGHT_MIN && face->box_right <= BOX_RIGHT_MAX;

      if (currentState != previousState[i]) {
        Serial.println("********");
        Serial.println(currentState);
        drawFacePosition(centerX, face->box_left, face->box_right);

        if (isCentered) {
          Serial.println("✅ Rosto CENTRALIZADO");
          centerEyes();
        } else {
          moveEyesProportional(centerX);
        }

        previousState[i] = currentState;
      }

      // Abrir olhos apenas se centralizado
      if (!facePresent && isCentered) {
        openEyes();
        Serial.println(">>> Rosto centralizado — olhos abertos");
        facePresent = true;
      }

      faceDetectedNow = true;
    }
  }

  // Atualizar temporizador se algum rosto for detetado
  if (faceDetectedNow) {
    lastFaceDetectedTime = millis();
  }

  // Fechar olhos após tempo de ausência
  if (facePresent && millis() - lastFaceDetectedTime > NO_FACE_TIMEOUT) {
    closeEyes();
    Serial.println(">>> Ausência prolongada — olhos fechados");
    facePresent = false;
    previousFaceCount = 0;
    for (int i = 0; i < MAX_FACES; ++i) {
      previousState[i] = "";
      lastCenterX[i] = -1;
    }
  }

  delay(SAMPLE_DELAY_MS);
}
