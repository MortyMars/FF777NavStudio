# FF777NavStudio - Prise en main rapide<br /><br />


## Présentation générale de l’interface :


<br /><br />
<figure>
    <img src="qrc:/Interface.jpg" width="1000" title="Interface.jpg" alt="Interface.jpg" />
</figure>
<br />

#### L’interface de FF777NavStudio présente 4 grandes parties distinctes :

- Dans la partie supérieure de l’écran se trouve la zone des onglets permettant de sélectionner l’une des 15 structures de données sur laquelle on souhaite travailler.
- Dans toute la partie gauche de l’écran se trouve la vue de la ‘Base de données’ du projet actuellement chargé. Les données sont représentées en ‘clair’ dans leur acception habituellement reconnue en aéronautique par la norme ARINC 424.
- En haut à droite de l’écran, on trouve la zone d’édition des données, adaptée à l’onglet ‘structure’ sélectionné
- Sous la zone d’édition, se trouve la zone d’aperçu de l’enregistrement sélectionné dans la zone de base de données et mis en forme conformément à ce qu’attend le FF 777.

#### En complément de ces zones, on trouve :

- Dans la partie gauche du bandeau un bouton ‘*Enregistrer*’, toujours accessible afin d’enregistrer facilement et rapidement les modifications faites. L’enregistrement se fait au niveau de la base de données, c’est à dire qu’il fixe les données dans l’état où on les voit dans la vue ‘*Base de données*’
- Dans la partie droite de ce même bandeau, un convertisseur bidirectionnel pieds↔ mètres et miles nautiques ↔ mètres, pouvant rendre quelques petits services pendant la saisie et l’édition des données.
<br /><br /><br />
## Menus de l’application

L’unique menu nécessitant véritablement une présentation est le menu ‘Fichier’ qui apparait ainsi :


<br /><br />
<figure>
  <img src="qrc:/MenuFichier.jpg" width="1000" title="MenuFichier.jpg" alt="MenuFichier.jpg" />
</figure>
<br />

#### Le menu ‘Fichier’ se décompose en 3 groupes :<br />


***PROJETS :*** 
- ‘*Nouveau projet*’ : 
Créé un nouveau projet correspondant à un nouvel aéroport. On part d’une ‘feuille’ totalement blanche, ce qui va nécessiter un travail conséquent de saisie, en sachant que cette phase aura forcément été précédée d’une phase d’étude : renseigner l’application suppose que l’on sache par avance ce que l’on décrit.
- ‘*Ouvrir un projet*’ : 
Ouvre un projet existant dans la base de données, en vue de visualiser, éditer, compléter les données existante. En phase de création d’un projet, rappeler le projet permet de reprendre là où on s’était arrêté.
- ‘*Aéroport existant -\> Projet*’ : 
Permet de sélectionner un aéroport existant dans la base mondiale (une fois décodée) pour en extraire les données de navigation. Les données peuvent alors être modifiées ou complétée avant d’être réinsérées.<br />


***BASE de DONNÉES :*** 
- ‘*Enregister*’ : 
Même fonctionnalité que le bouton situé dans l’interface, càd enregistrer les données de la base de données
- ‘*Recharger le fichier mondial ’nav1.txt*’ : 
Permet de relire le fichier mondial afin de réindexer à la suite les données de la base de données afin d’éviter les écrasements d’index et donc de données.
- ‘*Exporter les fichiers .txt*’ : 
Permet de recréer un jeu de données du projet chargé et éventuellement modifié. La création de ce jeu de fichier est nécessaire pour compléter le fichier mondial.<br />


***SUITE à MàJ des AIRACS :*** 
- ‘*Décoder nav1.db -\> nav1.txt*’ : 
Permet de décoder le fichier ‘nav1.db’ fourni spécifiquement pour le FF777v2 avec les mises à jour des AIRACS. Le décodage produit le fichier ‘nav1.txt’ dans un format texte parfaitement visible.
- ‘*Compléter nav1.txt et réencoder nav1.db*’ : 
Complète le fichier nav1.txt avec les fichiers texte extraits par le menu  ‘\_Exporter les fichiers .txt\*\_, et le réencode au format .db pour qu’il  soit lisible dans le FF777.
