/**
 * @file sf/tube.c
 * @brief Une implantation des tubes de communication
 *
 * Un tube va être mis en oeuvre par un buffer mémoire dans lequel
 * seront écrites/lues les données.
 *
 * Attention, pour le moment, aucune distinction n'est faite entre les
 * extrémités du tube : ce qui est écrit (sur t[0] ou t[1]) est lu
 * (sur t[0] ou t[1]).
 */
#include <manux/tubes.h>
#include <manux/debug.h>
#include <manux/tache.h>    // tacheAjouterFichiers
#include <manux/scheduler.h>// tacheEnCours
#include <manux/fichier.h>
#include <manux/errno.h>    // ESUCCES
#include <manux/memoire.h>  // NULL
#include <manux/kmalloc.h>  // NULL
#include <manux/string.h>   // memcpy
#include <manux/atomique.h> // exclusions mutuelles

MethodesFichier tubeMethodesFichier;

/**
 * @brief Capacité mémoire d'un tube, en nombre de pages
 */
#define MANUX_TUBE_NB_PAGES 1

#define MANUX_TUBE_CAPACITE (MANUX_TAILLE_PAGE * MANUX_TUBE_NB_PAGES)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/**
 * @brief : Définition d'un tube
 */
typedef struct _tube {
   uint8_t * donnees;   //< Pointeur sur la zone de données
   int taille;          //< Nombre d'octets présents dans le tube
   int indiceProchain ; //< Position de la prochaine insertion

   int nbEcrivains;
   int nbLecteurs;

   // Varriable utilisé par le moniteur
   ExclusionMutuelle  em; //< Exclusion mutuelle pour l'accès au tube

   Condition  pasPlein;   //< Condition  pasPlein état "tube non plein"
   Condition  pasVide;    //< Condition  pasVide état "tube non vide"
   
  
} Tube;

/**
  * @brief Ouverture d'un tube en tant que Fichier 
  */       

int tubeOuvrir(INoeud * iNoeud, Fichier * f, uint16_t fanions, uint16_t mode)
{
   Tube * tube = (Tube *) f->iNoeud->prive;

   if (fanions & O_RDONLY) {
      tube->nbLecteurs++;
   }
   if (fanions & O_WRONLY) {
      tube->nbEcrivains++;
   }

   f->prive = NULL;

   return ESUCCES;
}

/**
  * @brief Fermeture d'un tube en tant que Fichier 
  */
int tubeFermer(Fichier * f)
{
   printk_debug(DBG_KERNEL_TUBE, "in\n");
   Tube * tube = (Tube *) f->iNoeud->prive;
   
   if (f->fanions & O_RDONLY) {
      tube->nbLecteurs--;
   }
   if (f->fanions & O_WRONLY) {
      tube->nbEcrivains--;
   }

   return ESUCCES;
}

/** ##################### BLOQUANT ##################### **/
/* @brief Écriture dans un fichier
 * // CAS BLOQUANT :
 * // Il faut avoir écrit nbOctets dans le tube avant que l'activité se termine
 * // Si on ne peut pas écrire tout de suite, on attend que la place se libère
 */
/** size_t tubeEcrire(Fichier * f, void * buffer, size_t nbOctets)
{
   Tube * tube;
   int n = 0;
   int nbOctetsEcrits = 0; // Le nombre d'octets déja écrits
  // Peut-on décemment écrire dans le tube ?
   if ((f == NULL) || (f->iNoeud == NULL) || (f->iNoeud->prive == NULL)) {
      return -EINVAL; // Erreur : mauvais paramètres
   }
   tube = f->iNoeud->prive;
 
   // Execution de la section critique donc on ne peut pas être interrompu
   // Car modification de la ressource partagée !!

   // Création du verrou par l'exclusion mutuelle
   // printk("EXCLUSION MUTUELLE ECRITURE -> IN ??\n");
   exclusionMutuelleEntrer(&(tube->em));
   // printk("EXCLUSION MUTUELLE ECRITURE -> OK !!\n");
  
   printk_debug(DBG_KERNEL_TUBE, "in\n");
   
   // On fait une boucle, car il est possible que l'on doive écrire en
   // deux fois si on est proche de la fin du tableau qui contient les
   // données.



   // Ici on ne s'occupe pas du problème d'ordonnancement
   // Mais celui ci semble être résolu par l'exclusion mutuelle et sa liste d'attente en FIFO...
   // Si l'activité se met en attente par faute de place dans le tube :
   // elle sera réveillée en premier avec le signal de la condition pasVide


   //CAS BLOQUANT :
   
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
            conditionDiffuser(&(tube->pasVide)); // on signale a TOUT le monde le nouvel état !
            conditionAttendre(&(tube->pasPlein), &(tube->em));
         }
            // Taille suffisante pour ecrire dans le tube
            // On n'écrit ni plus que ce qui est demandé, ni plus que ce
            // qu'on peut
            n = MIN(nbOctets - nbOctetsEcrits, MANUX_TUBE_CAPACITE - tube->taille);

            // On ne doit pas aller écrire au delà du buffer
            n = MIN(n, (MANUX_TUBE_CAPACITE - tube->indiceProchain));

            // On peut donc copier n octets dans le buffer à partir de la
            // position courante, sans risque de déborder
            memcpy(tube->donnees + tube->indiceProchain, buffer, n);

            tube->indiceProchain = (tube->indiceProchain + n) % MANUX_TUBE_CAPACITE;
            tube->taille += n;
            
            buffer += n;

            nbOctetsEcrits += n;
            
      } while (n > 0 || nbOctetsEcrits < nbOctets); 
      // CONDITION DE SORTIE BLOQUANTE !!
      // On ne sort que si on a écrit tout ce qu'on voulait écrire !

      // Je sors enfin de mon exclusion Mutuelle car toute mon information a bien était écrite dans le tube
      conditionSignaler(&(tube->pasVide));         // On signale que le tube n'est plus vide a la première activité en attente
      exclusionMutuelleSortir(&(tube->em));        // On rend la main en enlevant le verrou

   return nbOctetsEcrits;  // {nbOctetsEcrits == nbOctets} == TRUE -> INVARIANT EN ETAT BLOQUANT POUR LE RETOUR


// problème peut-être ici car un autre processus peut prendre la main avant
// et afficher son nbOctetsEcrits en premier.... 
// Pas grave ici ce n'est pas un problème d'ordonnancement, tant que l'écriture et la lecture sont cohérente
// ce n'est pas un problème d'afficher en premier le résultat d'un processus éxecuter en 1 + Xème place
// SURTOUT EN ETAT BLOQUANT !! 
// (où on connait le nombre qui va être écrit dans le tube si pas de gros problème d'éxecution)
} 
**/


/** ############# NON BLOQUANT ############# **/
/* @brief Écriture dans un fichier
 * // CAS NON BLOQUANT :
 * // Il faut avoir écrit au minimum un élément dans le tube avant que l'activité se termine
 * // Si on ne peut pas écrire tout de suite, on attend que la place se libère
 * // Si on a déjà écrit un élément mais qu'on ne peut pas écrire tout de suite, on sort quand même !
 */
size_t tubeEcrire(Fichier * f, void * buffer, size_t nbOctets)
{
   Tube * tube;
   int n = 0;
   int nbOctetsEcrits = 0; // Le nombre d'octets déja écrits
  // Peut-on décemment écrire dans le tube ?
   if ((f == NULL) || (f->iNoeud == NULL) || (f->iNoeud->prive == NULL)) {
      return -EINVAL; // Erreur : mauvais paramètres
   }
   tube = f->iNoeud->prive;
 
   // Execution de la section critique donc on ne peut pas être interrompu
   // Car modification de la ressource partagée !!

   // Création du verrou par l'exclusion mutuelle
   // printk("EXCLUSION MUTUELLE ECRITURE -> IN ??\n");
   exclusionMutuelleEntrer(&(tube->em));
   // printk("EXCLUSION MUTUELLE ECRITURE -> OK !!\n");
  
   printk_debug(DBG_KERNEL_TUBE, "in\n");
   
   // On fait une boucle, car il est possible que l'on doive écrire en
   // deux fois si on est proche de la fin du tableau qui contient les
   // données.


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
            // Taille suffisante pour ecrire dans le tube
            // On n'écrit ni plus que ce qui est demandé, ni plus que ce
            // qu'on peut
            n = MIN(nbOctets - nbOctetsEcrits, MANUX_TUBE_CAPACITE - tube->taille);

            // On ne doit pas aller écrire au delà du buffer
            n = MIN(n, (MANUX_TUBE_CAPACITE - tube->indiceProchain));

            // On peut donc copier n octets dans le buffer à partir de la
            // position courante, sans risque de déborder
            memcpy(tube->donnees + tube->indiceProchain, buffer, n);

            tube->indiceProchain = (tube->indiceProchain + n) % MANUX_TUBE_CAPACITE;
            tube->taille += n;
            
            buffer += n;

            nbOctetsEcrits += n;
            
      } while (n > 0); 
      // CONDITION DE SORTIE NON BLOQUANTE !!
      // On sort si on ne peut plus écrire dans le tube ou que l'on a écrit tout ce qu'on voulait écrire
      // => n == 0

      // Je sors enfin de mon exclusion Mutuelle car toute mon information a bien était écrite dans le tube
      // printk("J V Diffuser");
      conditionDiffuser(&(tube->pasVide));         // On signale que le tube n'est plus vide à tout le monde
      // printk("G Diffuse");
      exclusionMutuelleSortir(&(tube->em));        // On rend la main en enlevant le verrou

   return nbOctetsEcrits;  // {nbOctetsEcrits > 0} == TRUE -> INVARIANT EN ETAT NON BLOQUANT POUR LE RETOUR


// problème peut-être ici car un autre processus peut prendre la main avant
// et afficher son nbOctetsEcrits en premier.... 
// Pas grave ici ce n'est pas un problème d'ordonnancement, tant que l'écriture et la lecture sont cohérente
// ce n'est pas un problème d'afficher en premier le résultat d'un processus éxecuter en 1 + Xème place
// SURTOUT EN ETAT BLOQUANT !! 
// (où on connait le nombre qui va être écrit dans le tube si pas de gros problème d'éxecution)
} 




/** ##################### NON BLOQUANT ##################### **/
/* @brief Lecture dans un fichier
 * // CAS NON BLOQUANT :
 * // Il faut avoir lu au minimum 1 élément dans le tube avant que l'activité se termine
 * // Si on ne peut pas lire tout de suite, on attend que la place se libère
 */
size_t tubeLire(Fichier * f, void * buffer, size_t nbOctets)
{ 
   Tube * tube;
   int n = 0;
   int nbOctetsLus = 0;
   int indicePremier;
   printk_debug(DBG_KERNEL_TUBE, "in\n");
   // printk("lect %d\n", nbOctets);

   // Peut-on décemment lire dans le tube ? (note : les deux premières
   // conditions sont assurées par l'appelant (fichierLire) a priori
   if ((f == NULL) || (f->iNoeud == NULL) || (f->iNoeud->prive == NULL)) {
      return -EINVAL;
   }
   tube = f->iNoeud->prive;
   // On se place dans l'exclusionMutuelle afin de lire les octets à la suite
   // printk("EXCLUSION MUTUELLE LECTURE -> IN ??\n");
   exclusionMutuelleEntrer(&(tube->em));
   // printk("EXCLUSION MUTUELLE LECTURE -> OK !!\n");



   // CAS NON BLOQUANT 

   // L'activité va essayer de lire le maximum d'octets qu'elle peut dans le tube
   // (avec un maximum de nbOctets demandé)
   // Si elle ne peut pas lire tout de suite, elle va attendre que le tube ne soit plus vide
   // Sinon l'activité ne servirai a rien...

   // Dès que c'est son tour elle va lire dans le tube
   // Même si NbOctetsLus < nbOctets demandé elle se termine !

   // On regarde si dans le tube il y a au moins un élément qu'on peut lire
   while ((tube->taille) <= 0){
      // printk("je suis en attente!!");
      conditionAttendre(&(tube->pasVide), &(tube->em));
   }

   // On peut alors rentrer dans la boucle de lecture
   
   do { 
      // Alors on peut lire le tube car il ya a bien au moins un élément existant dans le tube 
            // A partir de quel octet peut-on lire ?
            indicePremier = (tube->indiceProchain + MANUX_TUBE_CAPACITE - tube->taille)
         %MANUX_TUBE_CAPACITE;

            // On ne lit ni plus que ce qui est demandé, ni plus que ce
            // qu'on a
            n = MIN(nbOctets - nbOctetsLus, tube->taille);
            
            // On ne doit pas aller lire au delà du buffer
            n = MIN(n, (MANUX_TUBE_CAPACITE - indicePremier));

            printk_debug(DBG_KERNEL_TUBE,"Je vais lire %d\n", n);
            
            // On peut donc copier n octets dans le buffer à partir de la
            // position courante, sans risque de déborder
            memcpy(buffer, tube->donnees + indicePremier, n);

            indicePremier = (indicePremier + n) % MANUX_TUBE_CAPACITE;
            tube->taille -= n;
            
            buffer += n;

            nbOctetsLus += n;
        
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


// Est-ce que l'état non bloquant oblige quand même l'activité à lire
// à lire au moins un octet ? ou peut ne rien lire du tout ?
// Ici j'ai choisi de lire au moins un octet, mais je ne sais pas si c'est pertinent.
}



/** ##################### BLOQUANT ##################### **/
/* @brief Lecture dans un fichier
 * // CAS BLOQUANT :
 * // Il faut avoir lu nbOctets dans le tube avant que l'activité se termine
 * // Si on ne peut pas lire tout de suite, on attend que la place se libère
 */
/**size_t tubeLire(Fichier * f, void * buffer, size_t nbOctets)
{ 
   Tube * tube;
   int n = 0;
   int nbOctetsLus = 0;
   int indicePremier;
   printk_debug(DBG_KERNEL_TUBE, "in\n");

   // Peut-on décemment lire dans le tube ? (note : les deux premières
   // conditions sont assurées par l'appelant (fichierLire) a priori
   if ((f == NULL) || (f->iNoeud == NULL) || (f->iNoeud->prive == NULL)) {
      return -EINVAL;
   }
   tube = f->iNoeud->prive;
   // On se place dans l'exclusionMutuelle afin de lire les octets à la suite
   //printk("EXCLUSION MUTUELLE LECTURE -> IN ??\n");
   exclusionMutuelleEntrer(&(tube->em));
   //printk("EXCLUSION MUTUELLE LECTURE -> OK !!\n");



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
         // Comme on vient de vérifier un état on le signal a tout le monde BROADCAST
         // conditionDiffuser(&(tube->pasPlein)); // (ne change rien)
         conditionAttendre(&(tube->pasVide), &(tube->em));
      }

      // Alors on peut lire le tube car il ya a bien au moins un élément existant dans le tube 
            // A partir de quel octet peut-on lire ?
            indicePremier = (tube->indiceProchain + MANUX_TUBE_CAPACITE - tube->taille)
         %MANUX_TUBE_CAPACITE;

            // On ne lit ni plus que ce qui est demandé, ni plus que ce
            // qu'on a
            n = MIN(nbOctets - nbOctetsLus, tube->taille);
            
            // On ne doit pas aller lire au delà du buffer
            n = MIN(n, (MANUX_TUBE_CAPACITE - indicePremier));

            printk_debug(DBG_KERNEL_TUBE,"Je vais lire %d\n", n);
            
            // On peut donc copier n octets dans le buffer à partir de la
            // position courante, sans risque de déborder
            memcpy(buffer, tube->donnees + indicePremier, n);

            indicePremier = (indicePremier + n) % MANUX_TUBE_CAPACITE;
            tube->taille -= n;
            
            buffer += n;

            nbOctetsLus += n;
        
   } while (nbOctetsLus < nbOctets);
   // CONDITION DE SORTIE BLOQUANTE !!
   // On ne sort que si on a lu tout ce qu'on voulait lire !

   // fin de l'utilisation de l'exclusionMutuelle on peut redonner la main
   printk_debug(DBG_KERNEL_TUBE, "out\n");
   //conditionDiffuser(&(tube->pasPlein));
   conditionSignaler(&(tube->pasPlein));
   exclusionMutuelleSortir(&(tube->em));

   return nbOctetsLus;  // {nbOctetsLus == nbOctets} == TRUE -> INVARIANT EN ETAT BLOQUANT
}**/





/**
 * @brief Déclaration des méthodes permettant de traiter un tube
 * comme un fichier
 */
MethodesFichier tubeMethodesFichier = {
   .ouvrir = tubeOuvrir,
   .fermer = tubeFermer,
   .ecrire = tubeEcrire,
   .lire = tubeLire
};

#ifdef MANUX_APPELS_SYSTEME
/**
 * @brief Implantation de l'appel système tube()
 *
 * On va créer un iNoeud décrivant le tube (une structure en mémoire)
 * puis deux descripteurs de fichiers, l'un pour écrire, l'autre pour
 * lire. 
 */
int sys_tube(ParametreAS as, int * fds)
{
   INoeud  * iNoeud;
   Fichier * fichiers[2];
   Tube    * tube;

   printk_debug(DBG_KERNEL_TUBE, "Creation d'un tube (lire = 0x%x) ...\n", tubeLire);

   // Création de la structure
   tube = kmalloc(sizeof(Tube));
   if (tube == NULL) {
      return ENOMEM;
   }
     
   // Alocation de la mémoire tampon du tube
   if ((tube->donnees = allouerPage()) == NULL) {
      return ENOMEM;
   }

   // Initialisation des compteurs
   tube->taille = 0;
   tube->indiceProchain = 0;

   // Création de l'iNoeud qui décrit le tube dans le système
   iNoeud = iNoeudCreer(tube, &tubeMethodesFichier);

   // Création du fichier de sortie du tube (celui où on va lire)
   fichiers[0] = fichierCreer(iNoeud, O_RDONLY, 0);

   // Création du fichier d'entrée du tube (celui où on va écrire)
   fichiers[1] = fichierCreer(iNoeud, O_WRONLY, 0);

   // On ajoute les fichiers à la tâche
   if (tacheAjouterFichiers(tacheEnCours, 2, fichiers, fds) != 2 ) {
      return ENOMEM;
   }

   // Création de l'exclusion mutuelle
   exclusionMutuelleInitialiser(&(tube->em));

   // Création des conditions
   // La liste n'est pas pleine donc écriture possible
   conditionInitialiser(&(tube->pasPlein));
   conditionInitialiser(&(tube->pasVide));

   printk_debug(DBG_KERNEL_TUBE, "Tube cree entre %d et %d\n", fds[0], fds[1]);

   // Si on est encore là, c'est que tout s'est déroulé comme prévu !
   return ESUCCES;
}
#endif
