#include "main.h"

#pragma region Initialisation des instances globales
Web web;
Pression PressionBrasDroitDroit = Pression(PIN_PRESSION_BRAS_DROIT_DROIT);
Pression PressionBrasDroitGauche = Pression(PIN_PRESSION_BRAS_DROIT_GAUCHE);
Pression PressionBrasDroitHaut = Pression(PIN_PRESSION_BRAS_DROIT_HAUT);
Pression PressionBrasDroitBas = Pression(PIN_PRESSION_BRAS_DROIT_BAS);
MoteurBrushless MoteurCoudeDroit = MoteurBrushless(can_id_moteur_coude_droit);
MoteurBrushless MoteurCoudeGauche = MoteurBrushless(can_id_moteur_coude_gauche);
MoteurBrushless MoteurEpauleDroit = MoteurBrushless(can_id_moteur_epaule_droit);
MoteurBrushless MoteurEpauleGauche = MoteurBrushless(can_id_moteur_epaule_gauche);
#pragma endregion

bool modeLibre = true;

void setup()
{
  Serial.begin(115200); // Démarre la communication série pour le debug
  Serial.println("------------------------------");
  Serial.println("Démarrage du système...");
  
  #pragma region lancement du système de fichiers LittleFS
    if(!LITTLEFS.begin()){
      Logger::Log("Erreur lors de l'initialisation de LittleFS");
      return;
    }
    Logger::Log("LittleFS initialisé avec succès");
  #pragma endregion

  #pragma region lancement du site
  // Execute la tache sur le core 0
  xTaskCreatePinnedToCore(
    Web::tacheServeurWebStatic, // fonction (static wrapper)
    "ServeurWeb",    // nom
    10000,           // taille de la pile (bytes)
    &web,            // paramètres: pointer to the global instance
    1,               // priorité
    NULL,            // task handle (not used)
    0                // core
  );
  Logger::Log("Tâche Web lancée sur Core 0.");
  #pragma endregion
}

void loop()
{

if (!MoteurCoudeDroit.Actualiser()) { //Lance la fonction actualiser(), qui actualise les informations du vesc, qui stoppe le moteur et retourne false si il y a un problème, et qui retourne true si il n'y a pas de problème
    delay(20);
    return;
  }

 bool correctionCD = MoteurCoudeDroit.AngleVerification(false);

  if(!correctionCD) { //ne s' active que si le moteur n'a pas été corrigé par la fonction AngleVerification() (donc était déjà dans les limites)
    if(modeLibre) {
      MoteurCoudeDroit.FollowMvt();
    } 
    else {
      if(PressionBrasDroitDroit.capteurDetecte()) {
        MoteurCoudeDroit.Turn(true);
      } else if(PressionBrasDroitGauche.capteurDetecte()) {
        MoteurCoudeDroit.Turn(false);
      } else {
        MoteurCoudeDroit.Stop();
      }
    }
  }

  Logger::Log(String(PressionBrasDroitDroit.capteurDetecte()));  
  Logger::Log(String(PressionBrasDroitGauche.capteurDetecte()));  
  Logger::Log(String(PressionBrasDroitHaut.capteurDetecte()));  
  Logger::Log(String(PressionBrasDroitBas.capteurDetecte()));


  // Mise à jour de la télémétrie pour le serveur Web (Core 0)
  TelemetrieMoteur telemetry;
  telemetry.tempMotor = MoteurCoudeDroit.GetTempMotor();
  telemetry.inpVoltage = MoteurCoudeDroit.GetVoltage();
  telemetry.angle = MoteurCoudeDroit.GetPosition();
  telemetry.rpm = MoteurCoudeDroit.GetRpm();
  telemetry.current = MoteurCoudeDroit.GetCurrent();
  telemetry.chargeKg = MoteurCoudeDroit.EstimerChargeKg(0.35f, 0.1f); // levier de 35 cm et kt approx
  setTelemetrieMoteur(telemetry);

  delay(20);

}
