/*----------------------------------------------------------------------------*/
/*      Implantation des op�rations de manipulation des s�maphores.           */
/*                                                                            */
/*                                                       (C) Manu Chaput 2000 */
/*----------------------------------------------------------------------------*/
#include <manux/semaphore.h>

#include <manux/scheduler.h> /* Pour r�ordonner lors de l'attente */

void semInit(Semaphore * sem, int val)
{
   sem->valeur = val;
   initialiserListeTache(&sem->tachesEnAttente);
}

void semObtenir(Semaphore * sem)
{
   /* Tant que l'on n'a pas la main sur le s�maphore ou qu'aucune
    * ressource n'est disponible, on rend la main */

   /* OK, on est les seuls ici, on peut donc modifier en toute tranquilit� */
}

void semRelacher(Semaphore * sem)
{
   
}
