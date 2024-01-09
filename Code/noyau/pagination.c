/*----------------------------------------------------------------------------*/
/*      Implantation des op�rations sp�cifiques au m�canisme de pagination.   */
/*                                                                            */
/*                                                       (C) Manu Chaput 2000 */
/*----------------------------------------------------------------------------*/
#include <manux/pagination.h>

#include <manux/config.h>     /* MANUX_TAILLE_PAGE */
#include <manux/memoire.h>

/*
 * R�pertoire de pagination global au syst�me. Le but est d'avoir une
 * premi�re configuration de la pagination, qui donne une vision "flat"
 * de la m�moire.
 */
PageDirectory repertoirePaginationSysteme;

void creerTablePaginationDirecte(PageDirectory * repertoirePagination,
				 int tailleMemoireEtendue)
/*
 * Initialisation d'une table de pagination "directe", c'est-�-dire
 * qui conserve les adresses physiques dans l'espace virtuel.
 */
{
   int         nombrePages;
   int         i;             /* Pour compter les pages initialis�es */
   Page      * pageCourante;  /* Pour parcourir les pages */
   PTE         PTECourante;   /* PDE de la page courante  */
   PDE         PDECourante;   /* PTE de la page courante  */
   int         indiceDirectory;     /* ... de la page courante  */
   int         indiceTable;         /* ... de la page courante  */
   PageTable   tableCourante;

   /* WARNING, valable pour la table syst�me uniquement, et encore ... */
   * repertoirePagination = (PageDirectory) 0x60000;

   /* La premi�re table est stoqu�e derri�re le r�pertoire */
   /* Attention � l'arithm�tique des pointeurs ! Pas de * sizeof(PDE) */
   tableCourante = *repertoirePagination; 

   /* WARNING, il faudrait peut-�tre passer l'adresse de fin */
   nombrePages = (tailleMemoireEtendue + 1024)/ 4;

   /* Construction des tables */
   pageCourante = (Page *)0;

   for (i = 1; i < nombrePages; i++) {

      /* Calcul des indices de la page courante */
      indiceTable = ((uint32_t)pageCourante >> 12) & 1023;
      indiceDirectory = ((uint32_t)pageCourante >> 22) & 1023;

      /* Construction d'une nouvelle PageTable */
      if (!indiceTable) {
         /* On met les tables les unes derri�re les autres */
         /* Attention � l'arithm�tique des pointeurs ! Pas de * sizeof(PDE) */
         tableCourante += 1024;
         PDECourante = ((uint32_t)tableCourante & 0xFFFFF000)
                       |0x003;
         (*repertoirePagination)[indiceDirectory] = PDECourante;
      }

      /* Calcul et enregistrement de la PTE */
      PTECourante = ((uint32_t)pageCourante & 0xFFFFF000)
                    |0x003;
      tableCourante[indiceTable] = PTECourante;

      /* On passe � la page suivante */
      /* Attention � l'arithm�tique des pointeurs ! */
      pageCourante += MANUX_TAILLE_PAGE / sizeof(void *);
   }
}

void creerTablePagination(PageDirectory * repertoirePagination)
/*
 * Initialisation d'une table de pagination pour une nouvelle t�che.
 * elle est cr��e dans la zone point�e par repertoirePagination et
 * englobe le nombre de pages r�serv�es au syst�me (avec un mapping
 * direct).
 */
{
   int         numeroPage;      /* Pour compter les pages initialis�es */
   Page      * pageCourante;    /* Pour parcourir les pages */
   PTE         PTECourante;     /* PDE de la page courante  */
   PDE         PDECourante;     /* PTE de la page courante  */
   int         indiceDirectory; /* ... de la page courante  */
   int         indiceTable;     /* ... de la page courante  */
   PageTable   tableCourante = 0;
   int         i;

   /* Le r�pertoire occupe une page */
   *repertoirePagination = allouerPage();

   /* On initialise le r�pertoire � z�ro */
   for (i = 0; i < 1024; i++) {
     (*repertoirePagination)[i] = (PDE) 0;
   }

   /* Construction des tables */
   pageCourante = (Page)0;

   /* On ins�re les premi�res pages */
   for (numeroPage = 1;
        numeroPage < nombrePagesSysteme;
        numeroPage++) {
      /* Calcul des indices de la page courante */
      indiceTable = ((uint32_t)pageCourante >> 12) & 1023;
      indiceDirectory = ((uint32_t)pageCourante >> 22) & 1023;

      /* Construction d'une nouvelle PageTable */
      if (!indiceTable) {
         /* On alloue une nouvelle page */
         tableCourante = allouerPage();
         for (i = 0; i < 1024; i++) {
            tableCourante[i] = (PTE)0;
	 }
         PDECourante = ((uint32_t)tableCourante & 0xFFFFF000)
                       |0x003;
         (*repertoirePagination)[indiceDirectory] = PDECourante;
      }

      /* Calcul et enregistrement de la PTE */
      PTECourante = ((uint32_t)pageCourante & 0xFFFFF000)
                    |0x003;
      tableCourante[indiceTable] = PTECourante;

      /* On passe � la page suivante */
      /* Attention � l'arithm�tique des pointeurs ! */
      pageCourante += MANUX_TAILLE_PAGE / sizeof(void *);
   }
}

int ajouterPage(PageDirectory * repertoirePagination,
                Page            adressePhysique,
                void          * adresseVirtuelle)
/*
 * Ajout d'une page dans l'espace d'adressage virtuel d'une
 * t�che � l'adresse voulue.
 */
{
   int       indiceDirectory; /* ... de la page */
   int       indiceTable;     /* ... de la page */
   PageTable tableCourante;

   /* Calcul des indices de la page courante */
   indiceTable = ((uint32_t)adresseVirtuelle >> 12) & 1023;
   indiceDirectory = ((uint32_t)adresseVirtuelle >> 22) & 1023;

   /* Cr�ation d'une nouvelle table si n�cessaire */
   if ((*repertoirePagination)[indiceDirectory] == 0) {
      (*repertoirePagination)[indiceDirectory] =
                            (((uint32_t)allouerPage()) & 0xFFFFF000)
                            | 0x003;
   }

   /* R�cup�ration de la table concern�e */
   tableCourante = (PageTable)((*repertoirePagination)[indiceDirectory]
                               & 0xFFFFF000);

   /* Placement de l'adresse dans la table */
   tableCourante[indiceTable] = ((uint32_t)adressePhysique & 0xFFFFF000)
                    |0x003;

   return 0 ; 
}

int initialiserPagination(int tailleMemoireEtendue)
{
   /* Initialisation de la table syst�me */
   creerTablePaginationDirecte(&repertoirePaginationSysteme,
			       tailleMemoireEtendue);

   /* Chargement en m�moire ... */
   __asm__ __volatile__ ("movl %0, %%cr3"
                         ::"a" ((char *)repertoirePaginationSysteme));

   /* ... puis validation du mode pagin� */
   __asm__ __volatile__ ("movl %%cr0, %%eax\n\t"
                         "orl $0x80000000, %%eax\n\t"
                         "movl %%eax, %%cr0\n\t"::);
   return 0;
}

