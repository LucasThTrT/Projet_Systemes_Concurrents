# PROJET SYSTEMES CONCURRENTS

- auteur: THIETART Lucas
- date : Janvier 2024

### travail fait sur ./sf/tubes.c & ./usr/init-acces-concurrent.c

# 1. Implantation

## 1.1 Lecteur Non Bloquant et écrivain Bloquant

### 1.1.1 Introduction

Dans un premier temps, j’ai implanté sans me poser de questions particulières sur la perfor-
mance ou autre les 2 fonctions tubeLire et tubeEcrire dans "tubes.c".

Ainsi, naturellement, j’ai mis tubeEcrire en activité BLOQUANTE et tubeLire en activité NON
BLOQUANTE.

Pour cela j’ai tout d’abord, dans les 2 fonctions, repéré les sections critiques donc quand il y a
une modification de la ressource partagée. Dans notre cas, c’est lors de la lecture et de l’écriture
au sein du tube. Je place donc l’entrée et la sortie de l’exclusion mutuelle autour du bloc atomique.

Ensuite je repère les conditions d’états :
- pasVide : il y a de l’information dans le tube -> On peut lire une partie dans le tube.
- pasPlein : il y a de la place dans le tube -> on peut encore écrire une partie dans le tube.


Enfin, je détermine les conditions de fin d’exécution des 2 activités :

Pour tubeEcrire (BLOQUANT) :

    On va finir l’activité uniquement si tous les octets donnés à écrire sont écrits dans le tube.
    S’il n’y a plus assez de place dans le tube pour écrire, on se met dans l’état bloquant d’attente
    de libération du tube. 
    Lorsque la libération de la ressource est effectuée, c’est l’activité qui s’était
    bloquée qui reprend la main, et donc le verrou (normalement, l’ordonnancement est fait par la
    liste FIFO d’attente).

    On va ainsi placer notre condition d'attente dans cette boucle pour controler si on a bien les ressources suffisantes.
    ET ON DIFFUSE CETTE DECOUVERTE EN BROADCAST afin qu'un lecteur en attente prenne la main et vide le tube. Cela va donc dans certain cas limiter l'entrée d'un ecrivain dans l'exclusion et s'apercevoir qu'il ne peut pas s'éxécuter. (Optimisation du temps d'exécution global) 


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   
   // Il faut avoir écrit nbOctets dans le tube avant que l'activité se termine
   // Si on ne peut pas écrire tout de suite, on attend que la place se libère
   do{
         // On regarde si il y a de l'espace disponible pour écrire dans le tube
         // boucle while :
         // On vérifie même si la condition est vraie, car il est possible que l'on soit réveillé
         // par un signal et que la condition ne soit plus vraie (on se rendort donc)
         // car une autre activité a pu écrire entre temps dans le tube

         while (tube->taille == MANUX_TUBE_CAPACITE){
            // On se met dans l'etat d'attente et on attend le signal de la taille disponible MINIMAL 
            // pour ecrire soit > 0
            conditionDiffuser(&(tube->pasVide)); // on signale a TOUT le monde le nouvel état pour attebdre le moins possible !
            conditionAttendre(&(tube->pasPlein), &(tube->em));
         }

...

    } while (n > 0 || nbOctetsEcrits < nbOctets); 
        // CONDITION DE SORTIE BLOQUANTE !!
        // On ne sort que si on a écrit tout ce qu'on voulait écrire !

        // Je sors enfin de mon exclusion Mutuelle car toute mon information a bien était écrite dans le tube
        conditionSignaler(&(tube->pasVide));         // On signale que le tube n'est plus vide a la première activité en attente
        exclusionMutuelleSortir(&(tube->em));        // On rend la main en enlevant le verrou

    return nbOctetsEcrits;  // {nbOctetsEcrits == nbOctets} == TRUE -> INVARIANT EN ETAT BLOQUANT POUR LE RETOUR

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


Pour tubeLire (NON BLOQUANT) :

    On va lancer l’activité s’il y a au moins un octet à lire dans le tube. Ensuite, on ne se préoccupe
    pas de savoir si on a pu lire la taille voulue et on rend la main !

    On a donc la vérification de la variable d'état en dehors de la boucle de lecture. 
    De plus, comme cette activité ne peut pas se bloquer; on signal uniquement en fin d'activité le nouvel état.


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// Dès que c'est son tour elle va lire dans le tube
   // Même si NbOctetsLus < nbOctets demandé elle se termine !

   // On regarde si dans le tube il y a au moins un élément qu'on peut lire
   while ((tube->taille) == 0){
      conditionAttendre(&(tube->pasVide), &(tube->em));
   }

   // On peut alors rentrer dans la boucle de lecture
   
   do { 
     
...
        
   } while (n > 0);
   //while ((tube->taille > 0) && (n > 0)); 
   // (tube->taille > 0 est une condition déjà comprise dans n > 0) 

   // CONDITION DE SORTIE NON BLOQUANTE !!
   // On sort que si on a lu tout ce qu'on voulait lire ou que le tube est vide !


   // fin de l'utilisation de l'exclusionMutuelle on peut redonner la main
   printk_debug(DBG_KERNEL_TUBE, "out\n");
   //conditionDiffuser(&(tube->pasPlein));
   conditionSignaler(&(tube->pasPlein));
   exclusionMutuelleSortir(&(tube->em));

   return nbOctetsLus;  // {nbOctetsLus > 0} == TRUE -> INVARIANT EN ETAT NON BLOQUANT

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%



### 1.1.2 Résultats

L’exécution avec cette première implantation se déroule parfaitement dans le cas simple de
2 lecteurs / 2 écrivains avec plusieurs tailles différentes de buffer et/ou de tailles d’informations
écrites dans le tube.

On peut observer le bon fonctionnement des écrivains et des lecteurs !

— Avec l’écriture ici des 2 bytes (taille du texte) < 4ko du buffer, il y a une incrémentation de
l’écriture totale = +2 bytes = +20.

— Avec la lecture de l’intégralité du Buffer lecteur, il y a une incrémentation de la lecture totale
= 16 - 1.


## 1.2 Lecteur Bloquant et Ecrivain Non Bloquant

### 1.2.1 Introduction

Dans un deuxième temps, j'ai donc fait les 2 autres implantations des fonctions possible.
Pour cela je suis donc parti des fonctions précédentes où les sections critiques et donc les verrous restent identiques.
J'adapte alors uniquement les conditions d'attentes et de sortie de l'exclusion mutuelle.

Ecrivain (NON BLOQUANT): 

    En mode non bloquant, il n'y a pas d'attente d'écriture maximal mais uniquement l'écriture minimal d'un élément.
    Ainsi on fixe notre condition d'attente en dehors de la boucle d'écriture dans le tube.
    Alors si (n > 0) FALSE cela exprime le cas où on a écrit tous les éléments demandés et / ou le cas où il n'y a plus de place dans le tube pour écrire a mon instant t !


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 //CAS NON BLOQUANT :
   
   // Il faut avoir écrit au moins 1 élément dans le tube avant que l'activité se termine
   // Si on ne peut pas écrire tout de suite, on attend que la place se libère

   // On regarde si il y a de l'espace disponible pour écrire dans le tube
   // boucle while :
   // On vérifie même si la condition est vraie, car il est possible que l'on soit réveillé
   // par un signal et que la condition ne soit plus vraie (on se rendort donc)
   // car une autre activité a pu écrire entre temps dans le tube
   while (tube->taille == MANUX_TUBE_CAPACITE){
      // On se met dans l'etat d'attente et on attend le signal de la taille disponible MINIMAL 
      // pour ecrire au moins 1 élément
      conditionAttendre(&(tube->pasPlein), &(tube->em));
   }

   // Taille suffisante pour ecrire dans le tube

   do{
           
...

      } while (n > 0); 
      // CONDITION DE SORTIE NON BLOQUANTE !!
      // On sort si on ne peut plus écrire dans le tube ou que l'on a écrit tout ce qu'on voulait écrire
      // => n == 0

      // Je sors enfin de mon exclusion Mutuelle car toute mon information a bien était écrite dans le tube
      conditionSignaler(&(tube->pasVide));         // On signale que le tube n'est plus vide a la première activité en attente
      exclusionMutuelleSortir(&(tube->em));        // On rend la main en enlevant le verrou

   return nbOctetsEcrits;  // {nbOctetsEcrits > 0} == TRUE -> INVARIANT EN ETAT NON BLOQUANT POUR LE RETOUR

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


Lecteur (BLOQUANT):

    En mode bloquant, on va finir notre activité si et seulement si on a lu dans le tube tous les élements demandés.
    On va donc placer notre vérification d'état à l'intérieur de cette boucle de lecture dans le tube.
    Alors la sortie de cette boucle (n > 0) FALSE et donc de l'exclusion mutuelle se passe uniquement si on a tout lu car n égale à zéro ne peut maintenant plus se produire si il y a manque de ressource dans le tube, contrer par cette vérification et mise en attente.


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 // CAS BLOQUANT 

   // L'activité doit lire tous les nbOctets dans le tube
   // Si elle ne peut pas lire tout de suite, elle va attendre que le tube ne soit plus vide
   // Si elle a déjà lu une partie mais qu'elle ne peut pas lire tout de suite, elle attend quand même
   // Jusqu'à ce qu'elle ait lu tout ce qu'elle voulait lire
   // Puis elle se termine
   
   
   do { 
      
         // On regarde si dans le tube il y a au moins un élément qu'on peut lire
         // sinon on se met en attente
      while ((tube->taille) == 0){
         conditionDiffuser(&(tube->pasPlein)); // Comme on vient de vérifier un état on le signal a tout le monde BROADCAST
         conditionAttendre(&(tube->pasVide), &(tube->em));
      }

      // Alors on peut lire le tube car il ya a bien au moins un élément existant dans le tube 
       
...
        
   } while (nbOctetsLus < nbOctets);
   // CONDITION DE SORTIE BLOQUANTE !!
   // On ne sort que si on a lu tout ce qu'on voulait lire !

   // fin de l'utilisation de l'exclusionMutuelle on peut redonner la main
   printk_debug(DBG_KERNEL_TUBE, "out\n");
   //conditionDiffuser(&(tube->pasPlein));
   conditionSignaler(&(tube->pasPlein));
   exclusionMutuelleSortir(&(tube->em));

   return nbOctetsLus;  // {nbOctetsLus == nbOctets} == TRUE -> INVARIANT EN ETAT BLOQUANT

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


### 1.2.2 Résultats 

L'exécution dans ce cas simple de n lecteurs et n écrivains est correcte. on a bien les incrémentations de lectures et d'écritures a chaque fois.


# 2. Cas de la famine pour les états bloquants

Pour faire apparaitre une famine, cet à dire l'exécution uniquement d'un lecteur dans notre cas (alors que les autres lecteurs demandent aussi les ressources).
On augmente grandement la taille de notre BUFFER de lecture et on diminue la taille d'écriture des écrivains afin d'avoir beaucoup de blocage et d'attente chez notre lecteur.

Tout d'abord, cela m'a permis de me rendre compte d'une erreur dans la signalisation de mes états dans mes activités bloquantes

J'ai modifié mais 2 fonctions bloquantes et tout fonctionne. On peut maintenant s'appercevoir de la famine.
Pour l'observer assez facilement, je regarde les états de variables d'exclusions mutuelles. C'est ici que jai alors une erreur que je n'arrive pas a trouver !


# 3. Erreur

J'observe malheureusement une erreur que je n'arrive pas a fixer.
Lorsque j'augmente trop la taille de mon BUFFER lecture (de l'odre de 1000 octets), je ne vais pouvoir exécuter qu'une seule fois tous les lecteurs puis ion va remplir le tube jusqu'à sa taille max 4096 par exemple


Ensuite mes lecteurs se mettent en état d'attente et n'en sortent pas. 
Or si je rentre en attente dans lecteur je signal immédiatement à un ecrivain qu'il peut prendre la main 
(VOIR Image/Erreur_3 (-- Erreur pour Buffer = 6000 octets) ou make run avec un grand buffer)

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    do { 
      
         // On regarde si dans le tube il y a au moins un élément qu'on peut lire
         // sinon on se met en attente
      while ((tube->taille) == 0){
         conditionDiffuser(&(tube->pasPlein)); // Comme on vient de vérifier un état on le signal a tout le monde BROADCAST
         conditionAttendre(&(tube->pasVide), &(tube->em));
    } 

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


Donc j'ai ensuite regardé les possibles erreurs de non libération au niveau de mon écrivain non bloquant
Mais par définition, l'écrivain va bien redonner la main au lecteur en attente sans se bloquer. 
Le signal est juste et les conditions d'écriture semblent aussi être correctes....

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

     } while (n > 0); 
      // CONDITION DE SORTIE NON BLOQUANTE !!
      // On sort si on ne peut plus écrire dans le tube ou que l'on a écrit tout ce qu'on voulait écrire
      // => n == 0

      // Je sors enfin de mon exclusion Mutuelle car toute mon information a bien était écrite dans le tube
      conditionSignaler(&(tube->pasVide));         // On signale que le tube n'est plus vide a la première activité en attente
      exclusionMutuelleSortir(&(tube->em));        // On rend la main en enlevant le verrou

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

Donc cette erreur correspond a ma voix d'amélioration dans ce projet.

Finalement ce n'est pas de ma faute ! Mais un problème dans le printf qui ne peut pas afficher la totalité de mon BUFFER.

On peut reprendre après modification du code "./usr/init-access-concurrent.c" notre observation de la famine chez le lecteur comme prévu. Pour supprimer cette famine on pourrai alors instaurer un temps d'attente de la part de notre lecteur mais cela ne semble pas être optimale... Comme l'idée du début de ce rapport, on pourrai utiliser la liste des activités trentrant en attente et utiliser une politique FIFO pour alterner toutes les activités !


# Conclusion

Dans ce projet, j'ai pu ettre en place les activités tubeLire et tubeEcrire en version bloquante et non bloquante.
Cela fonctionne maintenant pour toutes tailles de buffer et avec un nombre important de lecteur et/ou d'écrivain dans un même tube. 

Pour optimiser encore plus cette implémentation, il serait peut être interressant de regarder comment régler la famine (par une file d'attente FIFO par exemple).

FIN



