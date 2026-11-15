//Définition de la classe Web pour le serveur web en mode AP

#include "web.h"

static TelemetrieMoteur g_telemetrie = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
static portMUX_TYPE g_telemetrieMux = portMUX_INITIALIZER_UNLOCKED;

void setTelemetrieMoteur(const TelemetrieMoteur& data) {
    portENTER_CRITICAL(&g_telemetrieMux);
    g_telemetrie = data;
    portEXIT_CRITICAL(&g_telemetrieMux);
}

TelemetrieMoteur getTelemetrieMoteur() {
    TelemetrieMoteur copie;
    portENTER_CRITICAL(&g_telemetrieMux);
    copie = g_telemetrie;
    portEXIT_CRITICAL(&g_telemetrieMux);
    return copie;
}

#pragma region Tâche du Serveur Web en mode Point Accès (AP) | sur core 0

Web::Web() : server(80) {}

void Web::tacheServeurWebStatic(void* parameter)
{
  Web* instance = static_cast<Web *>(parameter);
  if (instance)
  {
    instance->tacheServeurWeb();
  }
  vTaskDelete(NULL);
}

void Web::tacheServeurWeb() // tâche du serveur web (instance)
{
  // Setup() de la tâche
  WiFi.softAP(this->_ssid, this->_password);
  Logger::Log("Nom du réseau: " + String(this->_ssid));
  Logger::Log("Mot de passe du réseau: " + String(this->_password));
  Logger::Log("Adresse IP de l'AP: " + WiFi.softAPIP().toString());

  server.on("/", [this]() {
    File file = LITTLEFS.open("/index.html", "r");
    if (!file) {
      this->server.send(500, "text/plain", "Erreur: Fichier index.html absent");
      return;
    }
    this->server.streamFile(file, "text/html");
    file.close();
  });

  server.on("/dvbFavicon.png", [this]() {
    File file = LITTLEFS.open("/dvbFavicon.png", "r");
    if (!file) {
      this->server.send(500, "image/png", "Erreur: Fichier dvbFavicon.png absent");
      return;
    }
    this->server.streamFile(file, "image/png");
    file.close();
  });

  // Endpoint API pour fournir les données des moteurs et de la charge en temps réel
  server.on("/data", [this]() {
    TelemetrieMoteur t = getTelemetrieMoteur();
    char jsonResponse[160];
    snprintf(jsonResponse, sizeof(jsonResponse),
      "{\"tempMotor\":%.1f,\"battery\":%.1f,\"angle\":%.1f,\"rpm\":%.0f,\"current\":%.2f,\"chargeKg\":%.2f}",
      t.tempMotor, t.inpVoltage, t.angle, t.rpm, t.current, t.chargeKg
    );
    this->server.send(200, "application/json", jsonResponse);
  });

  server.begin();
  Logger::Log("Site Internet démarré");

  // Loop() de la tâche
  for (;;)
  {
    server.handleClient();

    delay(2); // permet d'éviter le watchdog timer reset | en gros de faire crash l'ESP32
  }
}

#pragma endregion