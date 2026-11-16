// Fichier pour implémenter les fonctions de la classe MoteurBrushless déclarée dans moteur.h, pour pouvoir controller et avoir un retour d'information sur les moteurs brushless.
// Pour empécher l'encodeur de donner des fausses valeurs dangereuses, on peut créer une fonction qui vérifie qu'à chaque boucle, l'angle du moteur de varie que d'une quatité maximale définie par rapport à la dernière boucle (si l'encodeur bouge mais n'est pas débranché, il va d'un coup donner une valeur d'angle possible mais incohérente par rapport à la valeur précédente)
#include "moteur.h"

MoteurBrushless::MoteurBrushless(int can_id) : _can_id(can_id) { // Initialisation du VESC et de la communication série
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); // UART VESC (baudrate par défaut VESC)
    _vesc.setSerialPort(&Serial2); // port de communication des VESC
    _vesc.setDebugPort(&Serial); // port de debug des VESC
    Logger::Log("Communication série pour le VESC " + String(_can_id) + " démarrée sur RXD2: " + String(RXD2) + " et TXD2: " + String(TXD2));
}

void MoteurBrushless::Turn(bool senshoraire) { // Tourne le moteur dans le sens horaire ou antihoraire selon la position actuelle du moteur et les limites définies
    _vesc.getVescValues();
    if (senshoraire) {
        _vesc.setRPM(_RPM_AVANT_moteur, _can_id);
    } else if(MoteurBrushless::Position() < degmax_moteur) {
        _vesc.setRPM(_RPM_ARRIERE_moteur, _can_id);
    }
    Logger::Log(String(_vesc.data.pidPos));
}

void MoteurBrushless::Stop() { // Arrête le moteur et affiche la position actuelle du moteur
    _vesc.setRPM(0, _can_id);
    Logger::Log("Arrêt du moteur. Position: " + String(_vesc.data.pidPos));
}

int MoteurBrushless::Position() { // Retourne la position actuelle du moteur en degrés
    return _vesc.data.pidPos;
}

void MoteurBrushless::SetPosition(float deg) { // Régler la position du moteur à une valeur spécifique en degrés
    return setPosition(deg, 0);
}

void MoteurBrushless::setPosition(float pos, uint8_t canId) { // Régler la position du moteur à une valeur spécifique en degrés, avec un ID CAN pour les commandes relayées
    int32_t index = 0;
    int payloadSize = (canId == 0 ? 5 : 7); // Taille de la charge utile : 5 octets pour une commande directe, 7 octets pour une commande relayée via CAN
    uint8_t payload[payloadSize];

    if (canId != 0) {
        payload[index++] = COMM_FORWARD_CAN;  // Demande à relayer via CAN
        payload[index++] = canId;             // ID du VESC destinataire sur le bus CAN
    }

    payload[index++] = COMM_SET_POS;          // Commande pour régler la position
    buffer_append_int32(payload, (int32_t)(pos * 1000000.0), &index); // Position encodée

    _vesc.packSendPayload(payload, payloadSize);    // Envoie via UART
}

void MoteurBrushless::PrintData() { // Affiche les données actuelles du moteur (position, RPM, température, etc.)
    Logger::Log("Position actuelle du moteur: " + String(_vesc.data.pidPos));
    Logger::Log("RPM actuel du moteur: " + String(_vesc.data.rpm));
    Logger::Log("Température du moteur: " + String(_vesc.data.tempMotor));
    _vesc.printVescValues();
}

void MoteurBrushless::FollowMvt() { // Fonction pour faire suivre le mouvement du bras par le moteur  en utilisant les données de position et d'effort du VESC

    float effort = _vesc.data.avgMotorCurrent; 
    float angle = _vesc.data.pidPos;
        
    if (abs(effort) > ZONE_MORTE) {

        angle += (effort * FACTEUR_ASSISTANCE);
        SetPosition(angle);
        Logger::Log("Effort: "+String(effort)+" | Cible: "+String(angle));
    } else {
        Logger::Log("Effort: "+String(effort)+" | Zone Morte");
    }
  

}

float MoteurBrushless::EstimerChargeKg(float brasDeLevierMetres, float kt) const {
    // Couple moteur Tau = Kt * I (en N.m)
    // Force F = Tau / L (en N)
    // Masse = F / g (en kg), avec g ≈ 9.81 m/s²
    if (brasDeLevierMetres <= 0.001f) return 0.0f;
    float courant = fabsf(_vesc.data.avgMotorCurrent);
    float couple = courant * kt; // N.m
    float chargeKg = couple / (brasDeLevierMetres * 9.81f); //Pour l'instant mis en cte, a changer si nécéssaire
    return chargeKg;
}


bool MoteurBrushless::AngleVerification(bool inverseSymetrie) {
    int posActuelle = Position();

    if (posActuelle < degmin_moteur) {// Si on est sous le min, on veut tourner pour augmenter l'angle
        if(inverseSymetrie == false){
            Turn(false); 
        } 
        else {
            Turn(true);
        }
        return true;
    } 
    else if (posActuelle > degmax_moteur) {// Si on est au-dessus du max, on veut tourner pour diminuer l'angle
        if(inverseSymetrie == false){
            Turn(true);
        } 
        else {
            Turn(false);
        }
        return true;
    } 
    return false;
    
}

bool MoteurBrushless::Actualiser() {

    if (!_vesc.getVescValues()) { // plus de communication avec le VESC, on arrête le moteur et on affiche un message d'erreur
        Logger::Log("Perte de communication VESC (ID: " + String(_can_id) + ")");
        Stop();
        return false; 
    }
    if (_vesc.data.error != 0) { //panne active détectée, on arrête le moteur et on affiche un message d'erreur
        Logger::Log("Défaut matériel VESC/Encodeur, code = " + String(_vesc.data.error));
        Stop();
        return false; // Signale un défaut critique au main
    }
    return true; // Aucun problème détecté
}