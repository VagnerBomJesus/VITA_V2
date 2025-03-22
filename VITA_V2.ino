#include <Wire.h>
#include "person_sensor.h"

const int32_t SAMPLE_DELAY_MS = 100;
const int MAX_ROSTOS = PERSON_SENSOR_MAX_FACES_COUNT;

const int TOLERANCIA_AUSENCIA = 5;
const int CONFIANCA_MINIMA = 80;
const int MARGEM_MUDANCA_POSICAO = 10;

// Faixa para considerar como rosto centralizado
const int CAIXA_LEFT_MIN = 100;
const int CAIXA_LEFT_MAX = 110;
const int CAIXA_RIGHT_MIN = 130;
const int CAIXA_RIGHT_MAX = 140;

String estadoAnterior[MAX_ROSTOS];
int ultimoCentroX[MAX_ROSTOS];
bool rostoPresente = false;
int contadorAusencia = 0;
int numRostosAnterior = -1;

// ----- Função para visualizar no Serial a posição do rosto -----
void mostrarVisualizacaoRosto(int centroX, int boxLeft, int boxRight) {
  const int largura = 64; // largura do gráfico
  const int maxSensorX = 255;

  int posCentro = map(centroX, 0, maxSensorX, 0, largura - 1);
  int posCentroMin = map(CAIXA_LEFT_MIN, 0, maxSensorX, 0, largura - 1);
  int posCentroMax = map(CAIXA_RIGHT_MAX, 0, maxSensorX, 0, largura - 1);

  Serial.print("|");
  for (int i = 0; i < largura; i++) {
    if (i == posCentro) {
      Serial.print("📍"); // posição do rosto
    } else if (i >= posCentroMin && i <= posCentroMax) {
      Serial.print("="); // faixa central
    } else {
      Serial.print("-");
    }
  }
  Serial.println("|");

  Serial.print("CentroX: ");
  Serial.print(centroX);
  Serial.print("  (entre ");
  Serial.print(boxLeft);
  Serial.print(" e ");
  Serial.print(boxRight);
  Serial.println(")");
}
// ---------------------------------------------------------------

void setup() {
  Wire.begin();
  Serial.begin(57600);
}

void loop() {
  person_sensor_results_t results = {};

  if (!person_sensor_read(&results)) {
    delay(SAMPLE_DELAY_MS);
    return;
  }

  if (results.num_faces > 0) {
    contadorAusencia = 0;

    if (results.num_faces != numRostosAnterior) {
      Serial.println("********");
      Serial.print(results.num_faces);
      Serial.println(" rosto(s) detectado(s)");
      numRostosAnterior = results.num_faces;
    }

    for (int i = 0; i < results.num_faces; ++i) {
      const person_sensor_face_t* face = &results.faces[i];

      if (face->box_confidence < CONFIANCA_MINIMA) {
        continue;
      }

      int centroX = (face->box_left + face->box_right) / 2;

      if (abs(centroX - ultimoCentroX[i]) < MARGEM_MUDANCA_POSICAO &&
          estadoAnterior[i] != "") {
        continue;
      }

      ultimoCentroX[i] = centroX;

      String estadoAtual = "Rosto #" + String(i) + ": Confiança = " + String(face->box_confidence);
      estadoAtual += ", Caixa = (" + String(face->box_left) + ", " + String(face->box_top) + ")";
      estadoAtual += " até (" + String(face->box_right) + ", " + String(face->box_bottom) + "), ";
      estadoAtual += face->is_facing ? "olhando para a câmera, " : "não está olhando para a câmera, ";

      // Critério de centralização com base na faixa precisa
      bool estaCentralizado =
        face->box_left >= CAIXA_LEFT_MIN && face->box_left <= CAIXA_LEFT_MAX &&
        face->box_right >= CAIXA_RIGHT_MIN && face->box_right <= CAIXA_RIGHT_MAX;

      if (estaCentralizado) {
        estadoAtual += "rosto CENTRALIZADO";
      } else if (centroX < 128) {
        estadoAtual += "rosto mais à DIREITA ";
      } else {
        estadoAtual += "rosto mais à ESQUERDA";
      }

      if (estadoAtual != estadoAnterior[i]) {
        Serial.println("********");
        Serial.println(estadoAtual);
        estadoAnterior[i] = estadoAtual;

        // Desenhar visualização após impressão do novo estado
        mostrarVisualizacaoRosto(centroX, face->box_left, face->box_right);
      }

      rostoPresente = true;
    }

  } else {
    contadorAusencia++;

    if (contadorAusencia >= TOLERANCIA_AUSENCIA && rostoPresente) {
      Serial.println("********");
      Serial.println("Nenhum rosto detectado");
      rostoPresente = false;
      numRostosAnterior = 0;

      for (int i = 0; i < MAX_ROSTOS; ++i) {
        estadoAnterior[i] = "";
        ultimoCentroX[i] = -1;
      }
    }
  }

  delay(SAMPLE_DELAY_MS);
}
