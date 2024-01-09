/*----------------------------------------------------------------------------*/
/*      Implantation des sous-programmes de gestion des appels syst�me.       */
/*                                                                            */
/*                                                  (C) Manu Chaput 2000-2021 */
/*----------------------------------------------------------------------------*/
#include <manux/appelsysteme.h>

#include <manux/errno.h>
#include <manux/printk.h>
#include <manux/debug.h>
#include <manux/scheduler.h>      // sys_basculerTache
#include <manux/memoire.h>        // AS_obtenirPages, � virer apr�s dispatch  
#include <manux/console.h>        // Console
#ifdef MANUX_TACHES
#   include <manux/tache.h>       // sysFork
#endif
#ifdef MANUX_TUBES
#   include <manux/tubes.h>       // sys_tube
#endif
#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
#   include <manux/atomique.h>    // Mutex
#endif

#include <manux/horloge.h>

void * vecteurAppelsSysteme[NB_MAX_APPELS_SYSTEME];

/**
 * @brief : Implantation de l'appel syst�me inutile (pour tests)
 */
int sys_dumbAS(ParametreAS as)
{
   printk("I am so useless, ...\n");

   attenteMilliSecondes(500);
   
   return 4832;
}

/**
 * @brief : Implantation de l'appel syst�me par d�faut
 */
int sys_erreurAS(ParametreAS as)
{
   paniqueNoyau("Appel systeme %d non implante !\n", as.eax);

   return 0;
}

int definirAppelSysteme(int num, void * appel)
{
   // C'est plus prudent
   assert(num < NB_MAX_APPELS_SYSTEME) ;

   // Attention, il faut tenir compte de la valeur de retour !!
   // Pour le moment, je ne suis pas tr�s clair l� dessus (voir par
   // exemple la fonction suivante) donc on maintient l'assert
   // pr�c�dente. 
   if ((num < 0) || (num >= NB_MAX_APPELS_SYSTEME)) {
      return EINVAL;
   } else {
      printk_debug(DBG_KERNEL_AS, "Appel sys %d loaded\n", num);
      vecteurAppelsSysteme[num] = appel;
      return ESUCCES;
   }
}

void initialiserAppelsSysteme()
{
   printk_debug(DBG_KERNEL_AS, "vecteur des AS = 0x%x\n", vecteurAppelsSysteme);

   for (int a = 0; a < NB_MAX_APPELS_SYSTEME; a++) {
      definirAppelSysteme(a, sys_erreurAS);
   }
   
   definirAppelSysteme(NBAS_DUMB, sys_dumbAS);
   
   /* Envoyer une cha�ne de caract�res sur la console */
   definirAppelSysteme(NBAS_ECRIRE_CONS, sys_ecrireConsole);

#ifdef MANUX_TACHES
   // Cr�ation d'une nouvelle t�che
   definirAppelSysteme(NBAS_CREER_TACHE, sys_creerTache);   

   // Invocation explicite de l'ordonnanceur
   definirAppelSysteme(NBAS_BASCULER_TACHE, sys_basculerTache);
#endif

#ifdef MANUX_TUBES
   // Cr�ation d'un tube de communiations entre t�ches
   definirAppelSysteme(NBAS_TUBE, sys_tube);
#endif

   definirAppelSysteme(NBAS_IDENTIFIANT_TACHE, sys_identifiantTache);

   /* Les suivants sont � revoir */
   //definirAppelSysteme(NBAS_CONSOLE,        AS_console);
   //definirAppelSysteme(NBAS_OBTENIR_PAGES,  AS_obtenirPages);
   //definirAppelSysteme(NBAS_FORK,           sysFork);

#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
   tacheDansLeNoyau= 0;
   initialiserExclusionMutuelle(&verrouGeneralDuNoyau);
#endif   
}

void entrerAppelSysteme(uint32_t num)
{
   printk_debug(DBG_KERNEL_AS, "Appel sys %d IN\n", num);

#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
   /**
    * Sp�cialement pour le TP sur le tube : on fait comme si le noyau
    * �tait r�entrant, mais juste pour cet AS
    */
#   ifdef MANUX_TUBE_REENTRANT
   if (num != NBAS_TUBE) {
#   endif
   entrerExclusionMutuelle(&verrouGeneralDuNoyau);
   assert(tacheDansLeNoyau == 0);
   tacheDansLeNoyau = tacheEnCours->numero;
#   ifdef MANUX_TUBE_REENTRANT
   }
#   endif
#endif

#ifdef MANUX_AS_AUDIT
   tacheEnCours->nbAppelsSystemeIn[num]++;
#endif   
}

void sortirAppelSysteme(uint32_t num)
{
   
   printk_debug(DBG_KERNEL_AS, "Appel sys %d OUT\n", num);

#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
   /**
    * Sp�cialement pour le TP sur le tube : on fait comme si le noyau
    * �tait r�entrant, mais juste pour cet AS
    */
#   ifdef MANUX_TUBE_REENTRANT
   if (num != NBAS_TUBE) {
#   endif
   tacheDansLeNoyau = 0;
   sortirExclusionMutuelle(&verrouGeneralDuNoyau);
#   ifdef MANUX_TUBE_REENTRANT
   }
#   endif
#endif

#ifdef MANUX_AS_AUDIT
   tacheEnCours->nbAppelsSystemeOut[num]++;
#endif   
}


