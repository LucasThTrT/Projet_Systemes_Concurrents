/*----------------------------------------------------------------------------*/
/*      Implantation des sous-programmes de gestion des listes de taches.     */
/*                                                                            */
/*                                                       (C) Manu Chaput 2000 */
/*----------------------------------------------------------------------------*/
#include <manux/listetaches.h>
#include <manux/tache.h>
#include <manux/debug.h>         /* assert */
#include <manux/memoire.h>       /* NULL */

void initialiserListeTache(ListeTache * listeTache)
{
   listeTache->tete = NULL;
   listeTache->queue = NULL;
}

void insererCelluleTache(ListeTache   * listeTaches,
			 Tache        * tache,
                         CelluleTache * celluleTache)
{
   assert(listeTaches != NULL);
   assert(tache != NULL);
   assert(celluleTache != NULL);

   celluleTache->tache = tache;

   celluleTache->suivant = NULL;

   /* L'�l�ment se retrouve apr�s l'actuel dernier */
   if (listeTaches->queue != NULL) {
     listeTaches->queue->suivant = celluleTache;
   }

   /* L'�l�ment ins�r� se retrouve � la fin */
   listeTaches->queue = celluleTache;

   /* Si la liste �tait vide, il se retrouve aussi en t�te */
   if (listeTaches->tete == NULL) {
      listeTaches->tete = celluleTache;
   }

   assert(listeTaches->tete != NULL);
   assert(listeTaches->queue->tache == tache);
}

Tache * extraireTache(ListeTache * listeTaches)
{
   CelluleTache * celluleTache;

   celluleTache = listeTaches->tete;
  
   /* Si c'est la derni�re */
   if (celluleTache == listeTaches->queue) {
      listeTaches->queue = NULL;
   }

   /* On sort la cellule de la liste et on la renvoie */
   if (celluleTache != NULL) {
      listeTaches->tete = celluleTache->suivant;
      return celluleTache->tache;
   }

   /* Si on est encore l�, c'est que la liste est vide */
   return NULL;
}

