# Yokis Hack — volets Home Assistant et stabilité radio

Version dérivée de [nmaupu/yokis-hack](https://github.com/nmaupu/yokis-hack),
fondée sur **v1.8.7**, conservant la licence Apache 2.0 et les crédits d'origine.
Le projet original fournit déjà le protocole, l'appairage, les commandes radio
et des primitives pour les volets. Cette version complète notamment leur
intégration **MQTT / Home Assistant** ; elle ne revendique pas l'invention du protocole Yokis.

## Ce que cette version apporte

- Volets découverts comme `cover`, commandes `OPEN`, `CLOSE`, `STOP`.
- Arrêt immédiatement après une commande de mouvement, sans le filtrage
  temporel appliqué aux autres commandes MQTT.
- Suppression de l'ancienne découverte MQTT `light` pour un volet.
- Gestion des abonnements MQTT : déduplication, remise à zéro des pointeurs,
  vérification des topics et des tailles des messages.
- Disponibilité : six interrogations consécutives sans réponse avant `Offline`,
  compteur initialisé et saturé, annonce sur transition et récupération sur RX.
- Interrogation maintenue pour les modules hors ligne et pour le mode `NO_RCPT`.
- Présence radio distinguée du décodage ON/OFF : une réponse non décodée peut
  rétablir la disponibilité sans inventer un état de lampe.
- Commande `rfPower low|high|max` ; MAX au démarrage, changements à l'exécution
  non persistants. L'identité du client MQTT reste stable par passerelle.

**Limite volets :** absence de mesure fiable de position. L'état de mouvement/
ouverture affiché par Home Assistant est optimiste ; aucun pourcentage mesuré
n'est promis. Ces commandes ne constituent pas une fonction de sécurité.

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
