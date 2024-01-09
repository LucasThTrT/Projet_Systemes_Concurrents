/*----------------------------------------------------------------------------*/
/*      Un exemple pitoyable de d�but de noyau.                               */
/*                                                                            */
/*                                                  (C) Manu Chaput 2000-2023 */
/*----------------------------------------------------------------------------*/
#include <manux/config.h>
#include <manux/memoire.h>
#include <manux/console.h>
#include <manux/printk.h>
#include <manux/debug.h>
#include <manux/bootloader.h>

#define NB_PAGES_ALLOC 128

void startManuX()
{
   void * p;

   // R�cup�ration des informations depuis le bootloader
   bootloaderLireInfo();
   
   // Initialisation de la console noyau
   consoleInitialisation();

   // Initialisation des informations depuis le bootloader
   bootloaderInitialiser();

   // Affichage de la m�moire disponible 
   printk("Memoire : %d + %d Ko\n",
	  infoSysteme.memoireDeBase,
	  infoSysteme.memoireEtendue);

   /* Initialisation de la gestion m�moire */
   initialiserMemoire(infoSysteme.memoireDeBase,
		      infoSysteme.memoireEtendue);

   printk("Allocation des pages :\n");
   for (int n = 0; n < NB_PAGES_ALLOC; n++) {
      p = allouerPage();
      printk("0x%x ", p);
   }
   printk("\nThe end\n");
}   /* _start */

