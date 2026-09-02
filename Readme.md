# FF777 NavStudio
***FF777NavStudio*** est une application de mise en forme et d'édition de données de navigation pour le FlightFactor B777v2 (FF777) sous X-Plane 12.

## Ce que l'application n'est PAS
- Elle n'est **pas** un planificateur de vol ni un outil de navigation en temps réel.
- Elle n'est **pas** davantage un éditeur de scènes (paysages, objets 3D, ou textures).
- Elle ne perturbe **pas** le comportement de l'avion, mais modifie certains fichiers que celui-ci lit lors de son chargement.

## Ce qu'elle permet
L'application est conçue pour permettre au FF777 d'utiliser les procédures d'approche créées pour un aéroport fictif ou -WIP- ajoutées à un aéroport existant.

Ainsi, ***au titre d'opérations élémentaires sur les données de navigation***, l'application permet de :
- Créer, éditer et supprimer les données d'un aéroport personnalisé :
  - Points, waypoints, aéroports, pistes, navaids,
  - Séquences de legs, legs,
  - Approches, transitions d'approche,
  - Procédures SID / STAR et leurs transitions,
  - Transitions de pistes SID / STAR.
- Ajouter des données de navigation (mêmes entités que ci-dessus) à un aéroport existant (WIP), 
- Stocker les projets (càd les données des différents aéroports édités) dans une base SQLite locale.

Et ***lors de l'intégration initiale des données d'un aéroport fictif***, ou ***Après une mise à jour du fichier 'nav1.db'*** -à la sortie d'un nouveau cycle AIRAC par exemple- l'application a pour objectf de :
- Décoder la base 'nav1.db' (fichier mondial) en un fichier texte éditable 'nav1.txt',
- Aligner les index d'un 'projet' (aéroport fictif) sur ceux du 'nav1.txt', pour assurer une continuité des enregistrements.
- Enrichir le fichier texte, avec les données des procédures du projet, 
- Ré-encoder la version complétée du 'nav1.txt' pour en faire un nouveau 'nav1.db' interprétable par le FF777.

## Cadre du fonctionnement
***FF777NavStudio*** permet la construction syntaxique de procédures SID, STAR et APPROACH utilisables dans les modes d'approche RNAV (RNP) et ILS par le FlightFactor B777 sous X-Plane 12.
<br>Cette cible de niche répond à un besoin spécifique dû au format propriétaire adopté par FF pour les données de navigation exploitées par cet avion.</br>
En effet, s'il est réalisable avec un peu de persévérence de créer des procédures pour un avion adoptant les données de navigation du standard X-Plane 12, il était impossible de faire la même chose pour le FF777, compte tenu du format propriétaire codé du fichier 'nav1.db'.
<br>C'est désormais possible avec la présente application.</br>
      
***FF777NavStudio*** n'est pas un logiciel permettant de construire ad-nihilo des procédures d'approche, c'est une forme de masque de saisie assistée, facilitant la construction des fichiers listés ci-dessus sur la base de ***procédures que vous aurez préalablement imaginées et que vous êtes en capacité -au moment de la saisie- de décrire fonctionnellement, géographiquement et géométriquement : origine, points de passage, altitude, vitesse, pente ...***  
<br>Cette phase de conception à réaliser en amont de la saisie dans l'application (1), nécessite une base élémentaire de connaissances aéronautiques garantissant -le réalisme et la cohérence des procédures imaginées, -et la capacité qu'auront les dispositifs de pilotage automatique à les comprendre et l'appareil à les exécuter (2).</br>

En résumé, l'application ne génère pas elle-même des procédures toutes faites ; elle ne fait que formater et codifier les éléments que vous lui donnez en clair. 
<br>Sa finalité n'est donc pas de réaliser le travail de conception de procédures, mais de vous permettre d'établir des fichiers de procédures structurés et pleinement reconnus par le FF777 sous X-Plane 12.</br>  

## Quelques considérations aéronautiques

*(Retour sur les renvois du § précédent)*

(1) ***Little Navmap*** pourra s'avérer une aide précieuse dans le cadre de ces préparatifs car, bien qu'il ne s'agisse pas d'une de ses fonctionnalités natives, il permet de dessiner des segments de vol en vue de dessus, au besoin en s'appuyant sur la copie de procédures existantes. 
Quant à la composante verticale de l'approche, indispensable pour en faire un véritable tracé en 3D, elle pourra être calculée via un tableur en recherchant des altitudes successives garantissant une pente de descente en tous points acceptable.

(2) Pour démystifier la complexité apparente de ce point en simplifiant à l'extrême le sujet, on peut se dire qu'une procédure d'approche -pour prendre cet exemple- n'a besoin que de connaitre quelle succession de points géographiques vous **décidez** de franchir pour finalement parvenir à vous aligner sur la piste d'atterrissage. 
Cela dit, les trajets que vous choisirez devront pouvoir être 'avalés' par un avion long courrier tel que le FF777 : on ne choisit pas le même itinéraire lorsqu'on se déplace en petite citadine ou en semi-remorque. 
Le seul vrai risque que vous courez à réaliser des approches mal conçues, est que l'avion se trouvera dans l'incapacité matérielle de les suivre (distance de décélération insuffisante, virage exagérément serré,...). 
Dans ce domaine l'existant est le meilleur exemple à suivre.


## Gestion des projets
Par 'projet' il faut comprendre le périmètre d'un aéroport fictif dont on souhaite ajouter les procédures de navigation à la base de données utilisée par le FF777.
La gestion des projets consiste à :
- Créer autant de nouveaux projets que nécessaire
- Ouvrir / Renommer / Supprimer un projet existant
- Enregistrer la saisie en cours dans la base de données locale
- Indexer les enregistrements d'un projet sur le fichier mondial
- plus généralement, Sauvegarder les données des procédures d'un aéroport fictif


## Édition des entités
Le FF777 exploite (pour ce qui nous concerne ici) 15 types de structures de données différentes pour décrire des procédures de navigation compréhensibles par lui.
Ces 15 types de structures sont mises en forme dans 15 fichiers textes differents, qui distinguent les entités suivantes : 
<pre style="font-size: 11px;">
- POINT                           Points de cheminement en 2D longitude /latitude
- WAYPOINT                        Points ci-dessus recevant la qualif. de Waypoint
- AIRPORT                         Déclaration de l'aéroport
- RUNWAY                          Définition des pistes
- NAVAID                          Installations au sol d'aide à la nav. (ILS, ...)
- LEG                             Définition des segments élémentaires
- LEGSEQUENCE                     Regroupement de Legs définissant un cheminement
- APPROACH                        Procédures d'Approche
- APPROACHTRANSITION              Transitions vers une Approche
- PROCEDURE SID                   Procédures SID
- PROCEDURE STAR                  Procédures STAR
- PROCEDURETRANSITION SID         Transition au-delà d'une SID
- PROCEDURETRANSITION STAR        Transition vers une STAR
- RUNWAYPROCEDURETRANSITION SID   Transition Runway -> SID
- RUNWAYPROCEDURETRANSITION STAR  Transition STAR -> Runway
</pre>

## Compilation, Installation
Les informations concernant la compilation et l'installation sont contenues dans le fichier 'Config.md'.

## État du projet / Limites connues
- La pertinence de l'application est directement dépendante du format des données exploitées par le FF777.
- Son utilisation se cantonne au seul avion FF777. Elle pourra cependant être testée avec les données du FF A320 Ultimate qui semble présenter la même particularité de format.
- Elle ne garantit aucune compatibilité avec des versions futures de X-Plane ou du FF777.
- Les fichiers créés ou modifiés par l'application ne sont utilisables que dans le cadre de la simulation de vol et ne sauraient être utilisés en totalité ou partiellement pour une quelconque finalité liée au domaine du vol réel.
