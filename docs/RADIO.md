# Messages radio : observations et limites

La documentation originale du protocole reste dans `doc/intro.md` et appartient
au travail amont. Ce document précise les enseignements de cette installation.

- Le type/mode logiciel doit correspondre au module réel. Le nom d'une pièce
  n'est pas une identification électronique ; une lumière et un volet de la
  même pièce n'ont pas nécessairement les mêmes réponses ou le même trajet RF.
- `TX OK` indique un événement d'émission du pilote ; ce n'est ni une mesure
  de l'état de la charge ni une preuve suffisante de réception d'une réponse.
- Le firmware relève séparément une réception de payload de deux octets.
  Cette réception est utilisée comme indice de présence dans la transaction
  courante ; ce n'est pas une authentification cryptographique de l'émetteur.
- Une réponse reçue peut donner `UNDEFINED` au décodage. La disponibilité peut
  revenir sans publier un ON/OFF artificiel.
- Le libellé NO_RCPT ne justifie pas de supprimer toute interrogation : dans
  l'installation testée, la lumière configurée ainsi répondait aux demandes
  d'état. Cela ne démontre pas que tous les modules de ce mode répondent.
- Le décodage NO_RCPT est borné aux types déjà connus (0, 1, 0x1e, 0x2f).
  Les octets de commandes et les principes de décodage amont n'ont pas été
  remplacés par un nouveau protocole universel.
- Le pilote amont signale une différence de chronologie des réponses entre
  variateurs et interrupteurs ; ne pas assimiler une réponse précédant l'ordre
  à une mesure de l'état obtenu après l'ordre.
- Volets : OPEN/CLOSE/STOP utilisent les primitives radio existantes. Aucun
  retour fiable de position absolue n'a été établi par ces travaux.
- La dépendance à l'emplacement et l'amélioration après remplacement du module
  indiquent une marge de liaison initialement insuffisante. Les essais n'isolent
  pas émission, réception, gain d'antenne, interférences et alimentation.

Six échecs consécutifs retardent l'annonce d'une vraie panne et réduisent la
sensibilité aux pertes ponctuelles ; ce seuil n'améliore pas physiquement la radio.
Les interrogations continuent après Offline pour permettre une récupération.
