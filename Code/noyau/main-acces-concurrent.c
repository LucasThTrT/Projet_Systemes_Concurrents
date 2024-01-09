/**
 * @file main-acces-concurrent.c
 * @brief Un exemple pitoyable de d�but de noyau.
 *
 *                                                     (C) Manu Chaput 2000-2023
 */
#include <manux/config.h>
#include <manux/errno.h>
#include <manux/debug.h>      // A virer j'esp�re !
#include <manux/console.h>
#include <manux/kmalloc.h>    // Pour l'initialisation
#include <manux/printk.h>
#include <manux/memoire.h>
#include <manux/journal.h>    // initialiserJournal()
#include <manux/fichier.h>
#include <manux/bootloader.h>
#include <manux/clavier.h>
#include <manux/pagination.h>

/**
 * Configuration de la console
 */
INoeud  iNoeudConsole;  // Le INoeud qui d�crit la console

extern void init(); // Faire un init.h

#ifdef MANUX_VIRTIO_CONSOLE
INoeud iNoeudVirtioConsole;
Fichier fichierVirtioConsole;
#endif

void startManuX()
{
   // R�cup�ration des informations depuis le bootloader
   bootloaderLireInfo();

   bootloaderInitialiser();

   initialiserMemoire(infoSysteme.memoireDeBase,
		      infoSysteme.memoireEtendue);
   
   // Initialisation de la console noyau
   consoleInitialisationINoeud(&iNoeudConsole);

   /* Initilisation des descripteurs de segments */
   printk_debug(DBG_KERNEL_START, "Initialisation de la GDT ...\n");
   initialiserGDT();

   /* Initialisation de la table des interruptions */
   initialiserIDT();

   // Initialisation de l'allocateur g�n�raliste
   kmallocInitialisation();
   
   // Le clavier nous permettra de basculer entre consoles
   initialiserClavier();

   // Initialisation du journal
   printk_debug(DBG_KERNEL_START, "Initialisation du journal ...\n");
   journalInitialiser();

   // On va utiliser des appels syst�mes
   printk_debug(DBG_KERNEL_START, "Initialisation des AS ...\n");
   initialiserAppelsSysteme();

   // Le clavier va nous servir � basculer entre consoles
   printk_debug(DBG_KERNEL_START, "Initialisation du clavier ...\n");
   initialiserClavier();

   // On va utiliser des tubes, donc le syst�me de fichiers
   printk_debug(DBG_KERNEL_START, "Initialisation des fichiers ...\n");
   sfInitialiser();

   // Initialisation de la gestion des processus
   printk_debug(DBG_KERNEL_START, "Initialisation du scheduler ...\n");
   initialiserScheduler();

   // On a besoin de l'horloge pour l'ordonnanceur
   printk_debug(DBG_KERNEL_START, "Initialisation de l'horloge...\n");
   initialiserHorloge();

   printk_debug(DBG_KERNEL_START, "On passe en usr ...\n");

   // Avant de passer la main � init, on rel�che le verrou global. On
   // n'a donc plus acc�s au noyau, il faudra passer par des appels
   // syst�mes
   
#if defined(MANUX_TACHES) && !defined(MANUX_REENTRANT)
   tacheDansLeNoyau = 0;
   sortirExclusionMutuelle(&verrouGeneralDuNoyau);
#endif

   init();
}   /* _startManuX */


