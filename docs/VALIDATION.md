# Validation

## Vérifications historiques du 26 octobre 2026

- Compilation PlatformIO d1_mini réussie avant installation.
- Image installée et identité/version relues sur le Wemos.
- Configuration des 14 modules conservée lors de l'installation.
- Trois demandes d'état à la lumière NO_RCPT testée ont retourné OFF.
- Tests C++ : seuil six, saturation, récupération sur RX non décodé sans
  publication ON/OFF, découverte MQTT et interrogation des cinq modes.

## Tests fournis

`tests/stability_policy_test.cpp` teste la politique utilisée par le firmware.
`tests/firmware_paths_test.cpp` est un instantané généré des fonctions du candidat
avec matériel simulé : il faut le régénérer/mettre à jour si ces fonctions changent.
Ces tests ne remplacent ni une compilation ESP8266 ni un essai radio physique.

Exemple avec un compilateur C++11 :

```sh
c++ -std=c++11 -Iinclude tests/stability_policy_test.cpp -o policy-test
./policy-test
c++ -std=c++11 -Iinclude tests/firmware_paths_test.cpp -o firmware-paths-test
./firmware-paths-test
```

## Retour du 14 novembre 2026

L'utilisateur rapporte un fonctionnement parfait et des commandes instantanées
avec le HALJIA PA/LNA et son antenne externe. Il ne s'agit pas d'une campagne
instrumentée : aucun taux de pertes, temps de réponse quantifié ou journal
continu démontrant l'absence de coupure sur toute la période n'est fourni.

Les cibles ESP32/Arduino, d'autres régulateurs et références de modules,
les très grandes installations et toutes les variantes Yokis ne sont pas
qualifiés par cette installation. Aucun essai physique supplémentaire n'est
déclenché par la préparation de la publication.

## Vérification du dossier public, 14 novembre 2026

- 49 fichiers de code comparés à la source finale : identiques après retrait
  des seuls commentaires de provenance ajoutés pour la publication.
- Licence Apache 2.0 conservée à l'identique.
- Deux suites C++ compilées et exécutées : PASS.
- Recompilation ESP8266 d1_mini dans un environnement PlatformIO neuf : SUCCESS.
- Aucun accès matériel, flash ou commande Home Assistant lors de ces vérifications.
- Archive publique vérifiée : pas de dump flash, binaire compilé ou identifiant
  connu de l'installation ; documentation et sources uniquement.
