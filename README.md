# Yokis Hack pour Home Assistant

Une passerelle ESP8266 et nRF24L01+ pour piloter les éclairages et volets radio Yokis depuis Home Assistant, via MQTT.

Cette version complète l’intégration des volets et améliore le suivi de disponibilité des modules. Elle s’appuie sur [Yokis Hack, développé par nmaupu et ses contributeurs](https://github.com/nmaupu/yokis-hack), avec une documentation du matériel, du câblage et des essais réalisés.

## Fonctionnalités

| Fonction | Prise en charge |
|---|---|
| Volets | Découverte automatique dans Home Assistant et commandes d’ouverture, de fermeture et d’arrêt |
| Éclairages | Fonctions de commande et de variation héritées du projet original, selon le module Yokis |
| Disponibilité | Détection après six échecs consécutifs et reprise automatique du suivi dès réception d’une réponse |
| Puissance radio | Réglage `low`, `high` ou `max` depuis la console ; MAX au démarrage |

**Position des volets :** le firmware ne mesure pas leur position. Home Assistant affiche un état estimé à partir des commandes envoyées ; aucun pourcentage d’ouverture réel n’est disponible.

## Évolutions de cette version

- **Intégration MQTT des volets** : entités `cover`, commandes `OPEN`, `CLOSE` et `STOP`, arrêt immédiat après une commande de mouvement et remplacement de l’ancienne découverte en tant que lumière.
- **Suivi des modules** : interrogations maintenues après une perte de communication, y compris en mode `NO_RCPT`. Une réponse radio peut rétablir la disponibilité même lorsque l’état ON/OFF ne peut pas être décodé.
- **Traitement MQTT** : validation des messages, gestion des abonnements et identifiant de connexion stable pour chaque passerelle.
- **Documentation radio et matérielle** : montage testé avec antenne externe, câblage complet et limites observées selon les modules.

Le détail des modifications et leur historique sont disponibles dans le [journal des changements](CHANGELOG.md). Le firmware est fondé sur **Yokis Hack v1.8.7**, sous licence **Apache 2.0**.

## Montage effectivement testé

| Élément | Matériel utilisé |
|---|---|
| Microcontrôleur | Wemos D1 Mini V4 ESP8266, GoTronic code 39132 |
| Radio | HALJIA nRF24L01+ PA/LNA, connecteur et antenne externe |
| Découplage | Condensateur radial 47 µF / 35 V, GoTronic code 22708 |
| Alimentation radio | Sortie 3,3 V du Wemos, masse commune ; aucun régulateur séparé |
| Boîtier | Boîtier de dérivation électrique adapté ; aucun modèle 3D fourni |

Le condensateur est raccordé en parallèle près de la radio : **+ sur VCC 3,3 V,
− sur GND**. Sa tension nominale de 35 V ne signifie pas que la radio accepte 35 V.
La radio ne doit pas être alimentée en 5 V. Vérifier les inscriptions des broches
du module réel : ne pas déduire leur ordre de sa seule orientation sur une photo.

| Broche radio | Broche Wemos |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CE | D2 / GPIO4 |
| CSN | D8 / GPIO15 |
| SCK | D5 / GPIO14 |
| MOSI | D7 / GPIO13 |
| MISO | D6 / GPIO12 |
| IRQ | D1 / GPIO5 — indispensable |

**Retour de l'installation, communiqué le 14 novembre 2026 :** après remplacement
du module à antenne PCB par le HALJIA PA/LNA avec antenne externe, toutes les
commandes sont rapportées instantanées et le fonctionnement satisfaisant.
Dans cette installation, ce remplacement a été nécessaire pour obtenir le résultat
attendu. Ce retour ne mesure ni un taux de pertes ni une portée et ne permet pas
d'isoler le gain de l'antenne de celui de l'électronique PA/LNA.
Le « 1100 m » commercial n'est pas une portée validée par ce projet.

L'alimentation par le Wemos est le montage observé, pas une garantie universelle
pour tous les régulateurs, clones et câbles. Les modules amplifiés peuvent demander
environ 115 mA en pointe selon leur modèle : consulter la
[documentation RF24](https://nrf24.github.io/RF24/md_COMMON__ISSUES.html).
MAX règle l'émission ; la puissance rayonnée réelle dépend de la radio et de
l'antenne. Les valeurs dBm affichées par ce firmware correspondent à la puce nue,
pas à une mesure en sortie du PA/LNA. Vérifier les limites applicables au montage.

## Compiler

Version de firmware conservée : `1.8.7-volets-stabilite4-sdb`.
La compilation ciblée ici est **d1_mini / ESP8266** ; les autres cibles héritées
ne sont pas qualifiées par cette publication.

```sh
python -m pip install platformio==6.1.19
python build_test_firmware.py
```

Sous Windows, utiliser un chemin court si une dépendance dépasse la limite de longueur des chemins.

Ce script compile uniquement, avec Wi-Fi/MQTT activés mais sans identifiants
intégrés. Résultat : `.pio/build/d1_mini/firmware.bin`. Il n'accède à aucun Wemos
et n'envoie aucune commande aux équipements. Paramètres de carte et dépendances
dans `platformio.ini` ; les versions non épinglées restent une limite de reproductibilité.

## Installation et configuration

Avant une mise à jour, conserver **privément** `dConfigFS` et une sauvegarde
adaptée au firmware existant. Ne pas effacer toute la flash : elle contient
la configuration, le Wi-Fi et MQTT. Le choix du port et de l'image doit être vérifié
avant toute installation ; aucun script de flash automatique n'est fourni ici.

L'installation initiale, le Wi-Fi et l'appairage reposent sur le projet d'origine :
voir [README amont conservé](README.upstream.md). Les anciennes instructions
de migration/effacement y sont historiques et ne doivent pas être appliquées
automatiquement à une installation existante.

Console série : 115200 bauds. Une fois le Wi-Fi configuré, la console Telnet est
disponible sur l'adresse locale du Wemos. Utiliser uniquement sur un réseau de confiance.

Commandes de lecture : `help`, `dConfig`, `dConfigFS`, `status NOM`, `rfPower`.
`poll` et `debug` sont des bascules, pas des lectures. Les commandes d'appairage,
`copy`, `delete`, `clear`, `save`, `dRestore`, `reload` modifient la configuration :
suivre leur aide et sauvegarder avant utilisation. `mqttDiag` / `wifiDiag` peuvent
exposer des paramètres privés : ne pas publier leurs sorties.

Configuration MQTT en console, avec ses propres paramètres :
`mqttConfig HOTE PORT UTILISATEUR MOT_DE_PASSE`.
Home Assistant utilise son intégration MQTT avec découverte activée.

Exemple de commande de volet déjà appairé et configuré en mode volet :
topic `volet_demo/cmnd/COVER`, payload `OPEN`, `CLOSE` ou `STOP`, **non retained**.
La découverte se fait sur `homeassistant/cover/volet_demo/config`.
Les noms sont des exemples, pas des appareils livrés dans le firmware.

## Documentation et validation

- [Historique des changements](CHANGELOG.md)
- [Observations sur les messages et les modules](docs/RADIO.md)
- [Tests, portée et limites de la validation](docs/VALIDATION.md)
- [Diff des sources du firmware contre v1.8.7](docs/firmware-changes-vs-v1.8.7.patch)
- [Analyse radio originale de nmaupu](doc/intro.md)

Les sauvegardes complètes de flash, identifiants, configurations d'appairage et
captures d'écran de comptes personnels sont exclus. Aucun binaire n'est inclus
dans ce dossier source. Aucun fonctionnement récent n'a été mesuré en direct
pour cette préparation : le retour terrain ci-dessus est celui de l'utilisateur.
