/*----------------------------------------------------------------------------*/
/*      Un exemple pitoyable de d�but de noyau.                               */
/*                                                                            */
/*                                                  (C) Manu Chaput 2000-2023 */
/*----------------------------------------------------------------------------*/
#include <manux/config.h>
#include <manux/console.h>
#include <manux/printk.h>

void startManuX()
{
  // Initialisation de la console noyau
   consoleInitialisation();

   // Un petit message
   printk("Printk dit bonjour !\n");
  
}   /* startManuX */


