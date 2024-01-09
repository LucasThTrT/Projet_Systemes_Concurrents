/**
 * @file usr/init.c
 * @brief L'init du monde utilisateur
 * 
 * Attention, ici les include sont dans usr/include !!!
 */
#include <manux/types.h>
#include <stdio.h>
#include <manux/errno.h>
#include <unistd.h>   // creerNouvelleTache
#include <manux/string.h>

/**
 * @brief Taille du buffer utilisé en lecture
 */ 
/**#define TAILLE_BUFFER 16**/
# define TAILLE_BUFFER 1000

/**
 * @brief Nombres de lecteurs et écrivains
 */ 
#define NB_LECTEURS  2
#define NB_ECRIVAINS 2

int fd[2];  // Le tube

void lecteur()
{
   int r, c=0;
   char b[TAILLE_BUFFER];
   
   printf("[%d] Je suis un lecteur !\n", identifiantTache());
   
   do {
      printf("[%d] lecture %d va lire %d \n", identifiantTache(), r);
      r = lire(fd[0], b, TAILLE_BUFFER - 1);
      printf("[%d] lecture %d || lu avant %d \n", identifiantTache(), r, c);

      if (r > 0) {
         b[100] = 0;
         printf(b);
         c += r;
      } else if (r < 0){
	printf("[%d] Erreur lecture\n", identifiantTache());
      }
   } while (r > 0);

   printf("\n[%d] Fini ... En tout, j'ai lu %d !\n", identifiantTache(), c);
}

void ecrivain()
{
   int r, c  = 0;

   // Génération Lorem Ipsum de 2bytes
   char *b = "Lorem ipsum egestas.";
   
  //// Génération Lorem Ipsum de 2ko
  //char *b = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
  //  "Proin varius enim vestibulum, porttitor est non, viverra ex. "
  //  "Donec elementum erat sed nisi tempor congue. Maecenas ac congue tellus, "
  //  "sit amet tincidunt nisi. Fusce vitae quam at ligula vehicula tincidunt "
  //  "in ac sapien. Etiam mattis sem lacus, vel ultricies metus aliquet a. "
  //  "Nullam ac ante eu neque euismod fermentum. Proin lacinia vel nunc ac "
  //  "vestibulum. Aenean tincidunt eros nec consequat feugiat. Fusce condimentum "
  //  "tristique massa, ac condimentum risus sollicitudin non. Fusce in egestas "
  //  "ligula, a porta leo. Cras eros enim, convallis quis pellentesque nec, "
  //  "ultrices eu erat. Praesent porta augue sed tortor pharetra bibendum. "
  //  "Fusce placerat odio enim, sit amet fringilla augue semper at. Sed luctus "
  //  "volutpat quam, non tristique turpis tristique ut.Integer pretium enim sed "
  //  "varius placerat. Etiam cursus, dui eget hendrerit bibendum, lacus tortor "
  //  "auctor odio, ac interdum tellus neque vel nibh. Nullam fermentum massa sed "
  //  "tristique pretium. Donec efficitur ligula eros, sed congue lacus feugiat id. "
  //  "Proin faucibus commodo fringilla. In viverra id ligula non dictum. Integer "
  //  "varius id metus in commodo. In hac habitasse platea dictumst. Phasellus a "
  //  "lacinia magna. Nunc id condimentum lectus, eu sodales ipsum. Vestibulum "
  //  "dignissim interdum arcu a lobortis. Nunc nec hendrerit purus. Aenean at "
  //  "pretium sapien. Nam faucibus blandit efficitur. Vivamus vehicula posuere "
  //  "leo eu dictum. Donec convallis velit consectetur aliquet mattis. Integer "
  //  "nisl enim, viverra sed vehicula et, eleifend quis lorem. Aliquam erat ipsum, "
  //  "volutpat vel mi ut, efficitur vestibulum turpis. Ut accumsan turpis ac ante "
  //  "vestibulum vestibulum. Etiam dignissim eu lorem id consectetur. Ut fringilla "
  //  "eget nulla vitae commodo. Nunc et tristique libero, a rhoncus odio. Pellentesque "
  //  "sed placerat sapien. Nulla tincidunt aliquam leo non egestas. Fusce non est "
  //  "aliquam, hendrerit dolor nec, mollis augue. Integer ut purus tincidunt, ultrices "
  //  "velit quis, viverra libero. Nunc nec.";

   printf("[%d] Je suis un ecrivain !\n", identifiantTache());

   do {
      printf("[%d] Je vais ecrire %d || ecrit avant %d\n", identifiantTache(), strlen(b),c);
      r = ecrire(fd[1], b, strlen(b));
      printf("[%d] Voila j'ai ecrit %d\n", identifiantTache(), r);

      if (r >= 0) {
         c += r;
      } else {
         printf("[%d] Erreur ecriture\n", identifiantTache());
      }
   } while (r > 0);

   printf("[%d] En tout, j'ai ecrit %d !\n", identifiantTache(), c);
}

void init()
{
  int r, l, e;

   printf("Bonjour le mode utilisateur !\n");

   r = tube(fd);

   if ( r != ESUCCES) {
      printf("r = %d : casse la pipe !?\n", r);
   } else {
      for (e = 0; e < NB_ECRIVAINS; e++) {
         r = creerNouvelleTache(ecrivain, FALSE);
      }
      for (l = 0; l < NB_LECTEURS; l++) {
         r = creerNouvelleTache(lecteur, FALSE);
      }
   }

   printf("Voila voila !\n");
   while(1){};
}
