/**
 *     Un exemple pitoyable de d�but de noyau.                               
 *                                                                            
 *                                                     (C) Manu Chaput 2000-2023
 */
#include <manux/config.h>
#include <manux/bootloader.h>
#include <manux/errno.h>
#include <manux/console.h>
#include <manux/segment.h>
#include <manux/memoire.h>
#include <manux/kmalloc.h>
#include <manux/printk.h>
#include <manux/stdlib.h>  // rand

/**
 * @brief Une fonction qu iinvoque kmalloc et kfree
 */
#define NB_ELEMENTS 200
#define NB_APPELS   10000

void testerKmalloc()
{
   void * elements[NB_ELEMENTS] = {NULL,};
   int n, e;

   printk(PRINTK_DEBUGAGE "Avant :\n");
   for (e = 0; e<NB_ELEMENTS; e++){
      elements[e] = NULL;
   }
   kmallocAfficherStatistiques("");
   for (n = 0; n < NB_APPELS; n++) {
      e = rand() % NB_ELEMENTS;
      if (elements[e] == NULL) {
         elements[e] = kmalloc(rand() % 1024);
      } else {
         kfree(elements[e]);
         elements[e] = NULL;
      }
   }
   printk(PRINTK_DEBUGAGE "Apres :\n");
   kmallocAfficherStatistiques("");
}

void startManuX()
{
   // R�cup�ration des informations depuis le bootloader
   bootloaderLireInfo();
   
   // Initialisation de la console noyau
   consoleInitialisation();

   initialiserMemoire(infoSysteme.memoireDeBase,
		      infoSysteme.memoireEtendue);

   /* Initilisation des descripteurs de segments */
   //   initialiserGDT();

   // Initialsation du syst�me kmalloc
   kmallocInitialisation();
   
   testerKmalloc();
}   /* startManuX */


