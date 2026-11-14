# Changements et provenance

## Base publique

nmaupu/yokis-hack v1.8.7, commit
`c2aeb5644adc2fcc2d351787946454bf46bfda1f`, licence Apache 2.0.
Les fonctions radio de volets, le protocole et le décodage initiaux préexistent.

## Première évolution : volets dans Home Assistant

Huit modifications historiques ont été reconstruites depuis le script archivé :
découverte `cover`, topic de découverte distinct, nettoyage de l'ancien `light`,
abonnement COVER, traitement OPEN/CLOSE/STOP, exemption temporelle pour STOP,
déduplication des abonnements et remise à zéro des pointeurs de topics MQTT.
La reconstruction historique n'est pas présentée comme une copie binaire
vérifiée de chaque ancien firmware installé.

## Stabilisation, 26 octobre 2026

- Stabilite1 : seuil à six échecs, compteur saturé, annonces sur transition,
  récupération et validation des messages MQTT, puissance radio configurable.
- Stabilite2/3 : MAX par défaut. Ces étapes intermédiaires excluaient à tort
  certains NO_RCPT du polling ; elles sont remplacées par Stabilite4.
- Stabilite4-SDB : interrogation de tous les modes ; fenêtre de réponse complète
  de 500 ms pour les requêtes d'état NO_RCPT ; distinction réception/décodage ;
  disponibilité propre au module ; état non optimiste en mode NO_RCPT ;
  compteur d'échecs explicitement initialisé à zéro.

La source finale est celle de `1.8.7-volets-stabilite4-sdb`, compilée et installée
le 26 octobre 2026. Les commentaires de provenance ajoutés pour cette publication
ne changent pas les instructions exécutées.

## Matériel et retour terrain

Remplacement radio signalé le 29 octobre 2026. Montage final précisé le
14 novembre : HALJIA PA/LNA, antenne externe, 3,3 V Wemos, 47 µF/35 V,
boîtier de dérivation. L'utilisateur rapporte alors un fonctionnement parfait
et instantané. L'amélioration principale est attribuée au remplacement radio ;
les modifications logicielles restent distinctes de ce constat matériel.
