/*----------------------------------------------------------------------------*/
/*      Implantation des fonctions de base du scheduler.                      */
/*                                                                            */
/* A voir : le param�tre "nouvelleConsole" n'a pas de sens s'il n'y a pas de  */
/* consoles virtuelles, � supprimer dans ce cas ? Ca rendra le code moins     */
/* lisible.                                                                   */
/*                                                  (C) Manu Chaput 2000-2023 */
/*----------------------------------------------------------------------------*/
#include <manux/scheduler.h>

#define DEBUG_MANUX_SCHEDULER

#include <manux/errno.h>
#include <manux/console.h>
#include <manux/io.h>
#include <manux/memoire.h>       /* NULL, allouerPage */
#include <manux/atomique.h>      /* Pour le verrou sur le scheduler */
#include <manux/printk.h>        /* printk() */
#include <manux/debug.h>         /* debug() paniqueNoyau() */
#include <manux/interruptions.h> /* nbTopHorloge */
#include <manux/i386.h>          /* ltr */
#include <manux/appelsysteme.h>  /* console() */
#include <manux/temps.h>         /* secondesDansTemps */
#ifdef MANUX_KMALLOC_STAT
#   include <manux/kmalloc.h>    // kmallocAfficherStatistiques
#endif
#ifdef MANUX_VIRTIO_CONSOLE
#   include <manux/virtio-console.h> // A virer
#endif
#ifdef MANUX_VIRTIO_NET
#   include <manux/virtio-net.h> // A virer
#endif

extern TacheID numeroProchaineTache ;

/*
 * Une variable globale permettant d'identifier la tache du scheduler
 */
Tache * tacheScheduler;

Temps dateDernierOrdonnancement;  // En nbTopHorloge
Atomique schedulerEnCours = 0;

/*
 * Ce qui suit n'est pas tr�s joli, mais �a ne devrait pas rester.
 */
booleen basculeConsoleDemandee = FALSE;

booleen basculerTacheDemande = TRUE; // WARNING � virer ? C'est pour
				     // faire du "pas � pas"

/*
 * Le scheduler est-il en cours d'ex�cution ?
 */
Atomique verrouScheduler;

/**
 * @brief Identifiant de la t�che actuellement en cours d'ex�cution
 * dans le noyau (0 si aucune)
 *
 * C'est une fa�on d'assurer le fonctionnement d'un noyau non
 * r�entrant.
 */
#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
ExclusionMutuelle verrouGeneralDuNoyau;
TacheID tacheDansLeNoyau = 0;
#endif

/**
 * @brief : La liste des t�ches pr�tes sur le syst�me
 *
 * Ce sont les t�ches pr�tes � �tre ex�cut�es. N'y figurent donc pas
 * la t�che en cours d'ex�cution ni les t�ches en attente sur un outil
 * de synchronisation.
 */
ListeTache listeTachesPretes;

/**
 * @brief : La liste de toutes les t�ches existant sur le syst�me.
 */
ListeTache listeToutesLesTaches;

/**
 * @brief : La liste des t�ches achev�es.
 */
ListeTache listeTachesTerminees;

/**
 * @brief : La t�che en cours
 */
Tache * tacheEnCours = NULL;

/**
 * @brief Le coeur de l'ordonnanceur.
 *
 * C'est cette fonction qui d�termine la prochaine t�che � ex�cuter.
 */
void ordonnanceur()
{
   Tache * tachePrecedente = tacheEnCours;

   assert(tacheEnCours != NULL);

   printk_debug(DBG_KERNEL_ORDON, "in (de tache %d)\n", tachePrecedente->numero);
   
#ifdef MANUX_CONSOLES_VIRTUELLES
   // Basculement entre les consoles virtuelles WARNING pourquoi ici
   // !? A faire faire par dummyTask
   if (basculeConsoleDemandee) {
      basculeConsoleDemandee = FALSE;
      basculerVersConsoleSuivante();
   }
#endif

   // Attention, si la t�che en cours n'est pas pr�emptible
   if (tacheEnCours->nonPreemptible) {
      return;
   }
   
   // (1) On s'occupe de la t�che en cours
  
   // On cumule le temps d'�cution dont elle vient de profiter
   tacheEnCours->tempsExecution += (nbTopHorloge - dateDernierOrdonnancement);

   // Si on n'est pas l� spontan�ment, on se consid�re en cours, mais
   // c'est fini pour le moment !
   if (tacheEnCours->etat == Tache_En_Cours) {
      tacheEnCours->etat = Tache_Prete;
   
      insererCelluleTache(&listeTachesPretes,
                          tacheEnCours,
                          (CelluleTache*)tacheEnCours+sizeof(Tache));
   }

   // Dans les autres cas, la t�che est sens�e �tre dans une file
   // correspondant � son �tat
   
   // (2) on cherche la t�che suivante
   // On prend la premi�re t�che pr�te, il y en a au moins une : la dummy 
   do {
      tacheEnCours = extraireTache(&listeTachesPretes);
   } while (tacheEnCours->etat != Tache_Prete); 

   tacheEnCours->etat = Tache_En_Cours;
 
   /* On note la date pour pouvoir mesurer le temps dont elle va profiter */
   dateDernierOrdonnancement = nbTopHorloge;

   if (tacheEnCours != tachePrecedente){
      printk_debug(DBG_KERNEL_ORDON, "On passe a la tache %d de TSS 0x%x \n",
	     tacheEnCours->numero,
	     tacheEnCours->indiceTSSDescriptor);

      /* Une activation de plus pour elle */
      tacheEnCours->nbActivations++;

      printk_debug(DBG_KERNEL_ORDON, "out (vers tache %d)\n", tacheEnCours->numero);
      basculerVersTache(tacheEnCours);
   }
   printk_debug(DBG_KERNEL_ORDON, "back (vers tache %d)\n", tacheEnCours->numero);
}

void afficherEtatUneTache(Tache * tache)
{
  printk(" [  %d]  %s   %4d  %2d:%2d  0x%x   0x%x  0x%x\n",
       tache->numero,
         (tache->etat == Tache_En_Cours)?"c":(((tache->etat == Tache_Prete)?"p":((tache->etat == Tache_Terminee)?"t":"b"))),
          tache->nbActivations,
	  totalMinutesDansTemps(tache->tempsExecution),
	  secondesDansTemps(tache->tempsExecution),
          tache,
#ifdef MANUX_TACHE_CONSOLE
          tache->console,
#else
          0x00, // WARNING : bof
#endif
          tache->ldt);
}

#ifdef MANUX_AS_AUDIT
/**
 * @brief Affichage sur la console des AS de chaque t�che
 */
void appelsSystemeAfficher()
{
   CelluleTache * celluleTache;

   printk("\nTache  | Appels Systeme (num:in/out)\n");
   printk("-------+----------------------------------------\n");
   for (celluleTache = listeToutesLesTaches.tete;
      celluleTache != NULL;
      celluleTache = celluleTache->suivant){
      printk("%3d    | ", celluleTache->tache->numero);

      for (int i=0; i < NB_MAX_APPELS_SYSTEME; i++) {
         if (celluleTache->tache->nbAppelsSystemeIn[i]) {
	    printk("%d:%d/%d ", i,
		   celluleTache->tache->nbAppelsSystemeIn[i],
		   celluleTache->tache->nbAppelsSystemeOut[i]);
         }
      }
      printk("\n");
   }
}
#endif

#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
/**
 * @brief Etat du verrou g�n�ral
 */
void afficherEtatMutex()
{
   printk("\n-- Tache dans le noyau : %d \n-- Taches en attente : ", tacheDansLeNoyau);
   for (CelluleTache * celluleTache = verrouGeneralDuNoyau.tachesEnAttente.tete;
        celluleTache != NULL;
	celluleTache = celluleTache->suivant){
      printk("%d ", celluleTache->tache->numero);
   }
   printk("\n-- %d ent / %d sor\n", verrouGeneralDuNoyau.nbEntrees, verrouGeneralDuNoyau.nbSorties);
}
#endif

/**
 * @brief Affichage des t�ches
 */
void afficherEtatTaches()
{
   CelluleTache * celluleTache;

   printk("\n ------------------------<SCHEDULER t = %d:%d (%d)>----------------------------\n",
	  totalMinutesDansTemps(nbTopHorloge),
	  secondesDansTemps(nbTopHorloge),
	  nbTopHorloge);

   printk("\n Num prochaine tache : %d\n", numeroProchaineTache);
   printk(" [num] et   nbAc  tpsEx    tache   console       ldt\n");
   for (celluleTache = listeToutesLesTaches.tete;
      celluleTache != NULL;
      celluleTache = celluleTache->suivant){
        afficherEtatUneTache(celluleTache->tache);
   }
   printk("\n------------------------------------------------------------------------------\n");
}

/**
 * @brief Affichage des IT re�ues
 *
 * Le but est de pr�senter un �cran synth�tique avec le nombre
 * d'occurences de chacune des interruptions.
 */
void interruptionAfficher()
{
   int i;

   printk("----[ Exceptions ]--------------------------\n");
   for (i = 0; i < MANUX_NB_EXCEPTIONS ; i ++) {
      if (nbItRecues[i]) {
         printk(" [ %3x : %5d ]", i, nbItRecues[i]); 
      }
   }
   printk("\n");
   
   printk("----[ IRQ ]---------------------------------\n");
   for (i = MANUX_NB_EXCEPTIONS; i < MANUX_NB_EXCEPTIONS + MANUX_NB_IRQ ; i ++) {
      if (nbItRecues[i]) {
         printk(" [ %3x : %5d ]", i, nbItRecues[i]); 
      }
   }
   printk("\n");
   
   printk("----[ Interruptions ]-----------------------\n");
   for (i = MANUX_NB_EXCEPTIONS + MANUX_NB_IRQ; i < MANUX_NB_INTERRUPTIONS ; i ++) {
      if (nbItRecues[i]) {
         printk(" [ %3x : %5d ]", i, nbItRecues[i]); 
      }
   }
   printk("\n");
}

#ifdef MANUX_CLAVIER_CONSOLE
/**
 * @brief Gestion du clavier pour la dummy
 */
void dummyTraiterClavier()
{
   Console * cons = tacheEnCours->console;
   char c[1] ;
   int i;
   
   while (cons->nbCarAttente) {
      c[0] = 0;
      consoleLire(cons, c, 1);
      switch (c[0]) {
#ifdef MANUX_AS_AUDIT
         case 'a' :
            appelsSystemeAfficher();
         break;
#endif
         case 'c' :
	    for (i = 0; i < 24; i++)
	       printk("\n");
         break;
         case 'h' :
	   printk("c(lear screen)\nh(elp)\np(rocessus)\nm(emoire)\ni(nterruptions)\ns(ynchronisation)\n");
	 break;
         case 'i' :
            interruptionAfficher();
	 break;
         case 'p' :
            afficherEtatTaches();
	 break;
#if defined(MANUX_ATOMIQUE_AUDIT)
         case 's' :
            exclusionsMutuellesAfficherEtat();
            conditionsAfficherEtat();
         break;
#endif
#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
         case 'x' :
            afficherEtatMutex();
	 break;
#endif
         case 'm' :
#ifdef MANUX_KMALLOC_STAT
            kmallocAfficherStatistiques("");
#else
            printk(" Memoire : %d / %d pages allouees\n",
	    nombrePagesAllouees(), nombrePagesTotal());
#endif
	 break;
         default :
	   //            printk("Unknown [0x%x] pressed\n", c[0]);
         break;
      }
   }
}
#endif // MANUX_CLAVIER_CONSOLE

/**
 * Le corps d'une t�che � ex�cuter lorsqu'on n'a que �a � faire, ...
 */
void aDummyKernelTask()
{
   while(1) {
#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
      // Cette t�che passe sa vie dans le noyau, elle doit donc
      // acqu�rir le verrou si le noyau n'est pas r�entrant.
      entrerExclusionMutuelle(&verrouGeneralDuNoyau);
      assert(tacheDansLeNoyau == 0);
      tacheDansLeNoyau = tacheEnCours->numero;
#endif

      printk_debug(DBG_KERNEL_ORDON, "aDummyKernelTask running\n");

#ifdef MANUX_CLAVIER_CONSOLE
      dummyTraiterClavier();
#endif
#ifdef MANUX_VIRTIO_NET
      virtioReseauPoll(); // WARNING � virer !!!
#endif
#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
      // Cette t�che passe sa vie dans le noyau, elle doit donc
      // rendre le verrou si le noyau n'est pas r�entrant.
      tacheDansLeNoyau = 0;
      sortirExclusionMutuelle(&verrouGeneralDuNoyau);
      ordonnanceur();
#endif
   }
}

/**
 * @brief Ajout d'une t�che dans l'ordonnanceur
 *
 * On se contente de l'ins�rer dans la liste des t�ches de l'ordonnanceur. 
 */
void ordonnanceurAddTache(Tache * tache)
{
   insererCelluleTache(&listeTachesPretes,
		       tache,
		       (CelluleTache*)tache+sizeof(Tache));
}

/**
 * @brief Iniitalisation de l'ordonnanceur
 */
void initialiserScheduler()
{
   Tache * t0, *t1;

   dateDernierOrdonnancement = nbTopHorloge;

   // Initialisation de la liste (vide) des t�ches en cours
   initialiserListeTache(&listeTachesPretes);

   // Initialisation de la liste (vide) de toutes les t�ches
   initialiserListeTache(&listeToutesLesTaches);

   // Initialisation de la liste (vide) des t�ches termin�es
   initialiserListeTache(&listeTachesTerminees);

   // Cr�ation d'une t�che pour le fil actuel (premier num�ro)
   t0 = tacheCreer(NULL);
   if (t0 == NULL) {
      paniqueNoyau("impossible de creer la premiere tache !\n");
   }
#ifdef MANUX_TACHE_CONSOLE   
   tacheSetConsole(t0, consoleNoyau());
#endif
   /* Cas particulier de la premi�re t�che : */
   /*   . et on la d�clare comme en cours.   */
   tacheEnCours = t0;
   t0->etat = Tache_En_Cours;
   
   /*   . on charge son task register ;      */
   ltr(t0->indiceTSSDescriptor);
			
   printk_debug(DBG_KERNEL_ORDON, "creons la dummy\n");

   // Initialisation de la tache "aDummyKernelTask" (num�ro 1)
   t1 = tacheCreer(aDummyKernelTask);

   printk_debug(DBG_KERNEL_ORDON, "la dummy est la ...\n");
   
   if (t1 == NULL) {
      paniqueNoyau("impossible de creer la seconde tache !\n");
   }
#ifdef MANUX_TACHE_CONSOLE   
   tacheSetConsole(t1, consoleNoyau());
#endif

   // Avant de permettre � une deuxi�me t�che d'entrer en concurrence,
   // il faut s'assurer qu'on a la main sur le noyau.
#if !defined(MANUX_REENTRANT)
   printk_debug(DBG_KERNEL_ORDON, "on verouille le verrou\n");
   
   entrerExclusionMutuelle(&verrouGeneralDuNoyau);
   assert(tacheDansLeNoyau == 0);
   tacheDansLeNoyau = tacheEnCours->numero;
#endif

   printk_debug(DBG_KERNEL_ORDON, "on ajoute la t1 dans l'ordo\n");
   
   // A partir de maintenant, nous ne sommes plus seuls
   ordonnanceurAddTache(t1);

   printk_debug(DBG_KERNEL_ORDON, "scheduler is done\n");
}

#ifdef SUPPRIME
/*
 * Insertion d'une nouvelle t�che dans l'ordonnanceur. La valeur
 * retourn�e est l'id de la t�che ou un code d'erreur.
 * WARNING a virer d�s que la pr�c�dente est OK
 */
TacheID ordonnancerTache(CorpsTache corpsTache, console * cons)
{
   Tache   * tache;

#ifdef MANUX_TACHE_CONSOLE   
   Console * cons;
#   ifdef MANUX_CONSOLES_VIRTUELLES
   if (nouvelleConsole) {
      cons = creerConsoleVirtuelle();
   } else {
      if (tacheEnCours != NULL) {
         cons = tacheEnCours->console;
      } else { // Pour la premi�re a priori
         cons = NULL;//consoleNoyau();
      }
   }   
#   endif // MANUX_CONSOLES_VIRTUELLES
#endif // MANUX_TACHE_CONSOLE
   
   /* Cr�ation de la tache */
#ifdef MANUX_TACHE_CONSOLE
   tache = creerTache(corpsTache, cons);
#else
   tache = creerTache(corpsTache);
#endif
   if (tache == NULL) {
      return -ENOMEM;
   }

   /* On ins�re la nouvelle t�che � la fin de la liste */
   if (corpsTache) {
      insererCelluleTache(&listeTachesPretes,
                          tache,
                          (CelluleTache*)tache+sizeof(Tache));
      printk_debug(DBG_KERNEL_TACHE, "Tache inseree\n");

   } else {
     //printk("99999\n");
      /* Cas particulier de la premi�re t�che : */
      /*   . on charge son task register ;      */
      ltr(tache->indiceTSSDescriptor);
      /*   . et on la d�clare comme en cours.   */
      tacheEnCours = tache;
   }
   //printk("00000\n");
   
   return tache->numero;
}
#endif


#ifdef MANUX_APPELS_SYSTEME

/**
 * @brief Implantation de l'AS d'obtention de l'identifiant
 */
int sys_identifiantTache()
{
   return (int)tacheEnCours->numero;
}

#ifdef MANUX_TACHE_CONSOLE
/**
 * @brief l'AS permettant d'obtenir la Console de la t�che
 *
 * Le nom de cette fonction est � changer
 */
uint32_t AS_console()
{
   if (schedulerEnCours) {
      return (uint32_t)tacheScheduler->console;
   } else if (tacheEnCours) {
      return (uint32_t)tacheEnCours->console;
   } else {
      return (uint32_t) NULL;
   }
}
#endif // MANUX_TACHE_CONSOLE

/**
 * @brief Implantation de l'appel syst�me d'invocation de l'ordonnanceur
 */
int sys_basculerTache(ParametreAS as)
{
   assert(tacheEnCours != NULL);
   ordonnanceur();

   return 0;
}

/**
 * @brief Implantation de l'appel syst�me de cr�ation d'une nouvelle
 * t�che
 * @param as les param�trers d'un appel syst�me
 * @param corpsTache un pointeur vers la fonction a ex�cuter
 * @param shareConsole pour partager la console de la t�che en cours
 * (ou en cr��er une nouvelle sinon)
 */
TacheID sys_creerTache(ParametreAS as, CorpsTache corpsTache, booleen shareConsole)
{
   Tache   * tache;
#ifdef MANUX_TACHE_CONSOLE
   Console * console;
#endif
   
   assert(tacheEnCours != NULL);

   printk_debug(DBG_KERNEL_ORDON, "corpsTache = 0x%x, share=%d\n", corpsTache, shareConsole);

   // Cr�ation de la t�che
   tache = tacheCreer(corpsTache);
   if (tache == NULL) {
      return -ENOENT;
   }

#ifdef MANUX_TACHE_CONSOLE
   // Affectation de la console
#   ifdef MANUX_CONSOLES_VIRTUELLES    // WARNING : MANUX_TACHE_CONSOLE plut�t ?
   if (shareConsole) {
      console = tacheEnCours->console;
   } else {
      console = creerConsoleVirtuelle();
   }
#else
   console = consoleNoyau();
#   endif // MANUX_CONSOLES_VIRTUELLES
   tacheSetConsole(tache, console);
#endif

   ordonnanceurAddTache(tache);
   
   return tache->numero;
}

#endif // MANUX_APPELS_SYSTEME
