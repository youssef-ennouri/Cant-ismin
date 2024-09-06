#include "pse.h"

#define NB_WORKERS 100

int creerCompte(int canal);
int afficherplat(int canal);
int chercherWorkerLibre(void);
void *threadWorker(void *arg);
void sessionClient(int canal);
void creerCohorteWorkers(void);
void sessionFournisseur(int canal);
void lockMutexCanal(int numWorker);
void unlockMutexCanal(int numWorker);
int ajouterPlat(int canal, char *identifiant);
int retirerPlat(int canal, char *identifiant);
int verifierIdentifiantClient(char *identifiant);
int affichercommande(int canal, char *identifiant);
int verifierIdentifiantFournisseur(char *identifiant);
int affichermodifications(int canal, char *identifiant);
int enregistrerPlatChoisi(char *identifiant, char *plat);
int gestionstock(int canal, char *identifiant, char *numeroplat);
int gestionreponse(int canal, char *identifiant, char *reponsefinale);
int enregistrermodifications(char *identifiant, char *plat, char *stock);
int gestionreponse_fournisseur(int canal, char *reponsefinale, char *identifiant);
int gestionstock_fournisseur(int canal, char *numeroplat, char *augmentation, char *identifiant);

DataSpec dataSpec[NB_WORKERS];
sem_t semWorkersLibres;
pthread_mutex_t mutexCanal[NB_WORKERS];

int main(int argc, char *argv[]) {
    short port;
    int ecoute, canal, ret;
    struct sockaddr_in adrEcoute, adrClient;
    unsigned int lgAdrClient;
    int numWorkerLibre;

    // Création des threads workers
    creerCohorteWorkers();

    // Initialisation du sémaphore pour les workers libres
    ret = sem_init(&semWorkersLibres, 0, NB_WORKERS);
    if (ret == -1)
        erreur_IO("init sem workers libres");

    // Vérification du nombre d'arguments
    if (argc != 2)
        erreur("usage: %s port\n", argv[0]);

    // Conversion du port en entier
    port = (short)atoi(argv[1]);

    // Création de la socket d'écoute
    printf("******************************************\nServeur: Création d'une socket\n");
    ecoute = socket(AF_INET, SOCK_STREAM, 0);
    if (ecoute < 0)
        erreur_IO("socket");

    // Configuration de l'adresse de la socket
    adrEcoute.sin_family = AF_INET;
    adrEcoute.sin_addr.s_addr = INADDR_ANY;
    adrEcoute.sin_port = htons(port);
    printf("Serveur: Liaison de la socket\n******************************************\n");

    // Liaison de la socket
    ret = bind(ecoute, (struct sockaddr *)&adrEcoute, sizeof(adrEcoute));
    if (ret < 0)
        erreur_IO("bind");

    // Mise en écoute de la socket
    ret = listen(ecoute, 5);
    if (ret < 0)
        erreur_IO("listen");

    // Boucle principale pour accepter les connexions entrantes
    while (VRAI) {
        canal = accept(ecoute, (struct sockaddr *)&adrClient, &lgAdrClient);
        if (canal < 0)
            erreur_IO("accept");

        // Attente d'un worker libre
        ret = sem_wait(&semWorkersLibres);
        if (ret == -1)
            erreur_IO("wait sem workers libres");
        
        // Recherche d'un worker libre
        numWorkerLibre = chercherWorkerLibre();

        // Affectation du canal au worker libre et réveil du worker
        dataSpec[numWorkerLibre].canal = canal;
        sem_post(&dataSpec[numWorkerLibre].sem);
        if (ret == -1)
            erreur_IO("post sem worker");
    }

    // Fermeture de la socket d'écoute
    if (close(ecoute) == -1)
        erreur_IO("fermeture ecoute");

    exit(EXIT_SUCCESS);
}

void creerCohorteWorkers(void) {
    int i, ret;

    // Initialisation des structures de données et création des threads workers
    for (i = 0; i < NB_WORKERS; i++) {
        dataSpec[i].canal = -1;
        dataSpec[i].tid = i;
        ret = sem_init(&dataSpec[i].sem, 0, 0);
        if (ret == -1)
            erreur_IO("init sem worker");

        ret = pthread_create(&dataSpec[i].id, NULL, threadWorker, &dataSpec[i]);
        if (ret != 0)
            erreur_IO("creation thread");

        ret = pthread_mutex_init(&mutexCanal[i], NULL);
        if (ret != 0)
            erreur_IO("init mutex canal");
    }
}

// Retourne le numéro du worker libre trouvé ou -1 si aucun worker libre
int chercherWorkerLibre(void) {
    int numWorkerLibre = -1, i = 0, canal;

    // Boucle pour rechercher un worker libre
    while (numWorkerLibre < 0 && i < NB_WORKERS) {
        lockMutexCanal(i);
        canal = dataSpec[i].canal;
        unlockMutexCanal(i);

        if (canal == -1)
            numWorkerLibre = i;
        else
            i++;
    }

    return numWorkerLibre;
}

void *threadWorker(void *arg) {
    DataSpec *dataTh = (DataSpec *)arg;
    int ret;

    // Boucle principale du thread worker
    while (VRAI) {
        ret = sem_wait(&dataTh->sem); // Attente du réveil par le thread principal
        if (ret == -1)
            erreur_IO("wait sem worker");

        // Envoi du message de bienvenue
        char welcomeMessage[] = "\n############################################\n Bienvenue à Cant'Ismin !\n 1 - Continuer en tant que Client\n 2 - Continuer en tant que Fournisseur\n###########################################\n";
        if (write(dataTh->canal, welcomeMessage, sizeof(welcomeMessage)) < 0)
            erreur_IO("write");

        // Lecture de la réponse du client
        char ligne[LIGNE_MAX];
        int lgLue = lireLigne(dataTh->canal, ligne);
        if (lgLue < 0)
            erreur_IO("lireLigne");

        ligne[lgLue - 1] = '\0';

        // Gestion des choix du client
        if (strcmp(ligne, "1") == 0) {
            sessionClient(dataTh->canal);
        } else if (strcmp(ligne, "2") == 0) {
            sessionFournisseur(dataTh->canal);
        } else {
            char invalidChoiceMessage[] = "Choix invalide. Veuillez entrer 1 pour se connecter en tant que client ou 2 pour se connecter en tant que fournisseur.\n";
            if (write(dataTh->canal, invalidChoiceMessage, strlen(invalidChoiceMessage)) < 0)
                erreur_IO("write");
        }

        // Réinitialisation du canal pour le worker
        lockMutexCanal(dataTh->tid);
        dataTh->canal = -1;
        unlockMutexCanal(dataTh->tid);

        // Incrémentation du nombre de workers libres
        ret = sem_post(&semWorkersLibres);
        if (ret == -1)
            erreur_IO("post sem workers libres");
    }

    pthread_exit(NULL);
}

void sessionClient(int canal) {
    char ligne[LIGNE_MAX];
    int lgLue;
    char identifiant[LIGNE_MAX];
    int identifiantValide = 0;
    char numero_plat[LIGNE_MAX];
    char reponsefinale[LIGNE_MAX];

    printf("\033[1;32m" "\n******************************************\nLancement d'une Session Client\n******************************************\n" "\033[0m");

    // Boucle de session pour le client
    while (1) {
        char welcomeMessage[] = "#######################################\n 1 - Se connecter\n 2 - Créer un compte\n#######################################\n";
        if (write(canal, welcomeMessage, sizeof(welcomeMessage)) < 0)
            erreur_IO("write");

        if (write(canal, "\n\n", 2) < 0)
            erreur_IO("write");

        lgLue = lireLigne(canal, ligne);
        if (lgLue < 0)
            erreur_IO("lireLigne");

        ligne[lgLue - 1] = '\0';

        if (strcmp(ligne, "1") == 0 || strcmp(ligne, "2") == 0) {
            if (strcmp(ligne, "2") == 0) {
                if (!creerCompte(canal)) {
                    char erreurMessage[] = "Erreur lors de la création du compte. Veuillez réessayer.\n";
                    if (write(canal, erreurMessage, strlen(erreurMessage)) < 0)
                        erreur_IO("write");
                    pthread_exit(NULL);
                }
            }
            break;
        } else {
            char invalidChoiceMessage[] = "##############################################################################\nChoix invalide.\nVeuillez entrer 1 pour se connecter ou 2 pour créer un compte.\n##############################################################################";
            if (write(canal, invalidChoiceMessage, strlen(invalidChoiceMessage)) < 0) {
                erreur_IO("write");
            }
            if (write(canal, "\n\n", 2) < 0) {
                erreur_IO("write");
            }
        }
    }

    char demandeIdentifiant[] = "###########################################\nVeuillez entrer votre identifiant Client :\n###########################################";
    if (write(canal, demandeIdentifiant, strlen(demandeIdentifiant)) < 0)
        erreur_IO("write");

    if (write(canal, "\n\n", 2) < 0)
        erreur_IO("write");

    // Vérification de l'identifiant client
    while (!identifiantValide) {
        lgLue = lireLigne(canal, identifiant);
        if (lgLue < 0)
            erreur_IO("lireLigne");

        identifiant[lgLue - 1] = '\0';

        if (verifierIdentifiantClient(identifiant)) {
            identifiantValide = 1;
            char messageconf[] = "\n######################################################################\nVous êtes connectés en tant que Client\n";
            if (write(canal, messageconf, strlen(messageconf)) < 0) {
                erreur_IO("write");
            }

            printf("\033[1;32m" "\n******************************************\nLe Client : %s s'est connecté\n******************************************\n" "\033[0m", identifiant);

            // Création du fichier de commande pour le client
            char nomFichier[LIGNE_MAX*2];
            snprintf(nomFichier, sizeof(nomFichier), "Projet/commande_%s.txt", identifiant);
            FILE *fichierCommande = fopen(nomFichier, "w");
            if (fichierCommande == NULL) {
                perror("Erreur lors de la création du fichier de commande");
                pthread_exit(NULL);
            }
            fclose(fichierCommande);

            char messageChoix[] = "Entrez le numéro du plat que vous voulez : \n######################################################################\n";
            if (write(canal, messageChoix, strlen(messageChoix)) < 0) {
                erreur_IO("write");
            }

            if (afficherplat(canal) < 0) {
                erreur_IO("afficherplat");
            }

            lgLue = lireLigne(canal, numero_plat);
            if (lgLue < 0) {
                erreur_IO("lireLigne");
            }

            numero_plat[lgLue - 1] = '\0';
            if (gestionstock(canal, identifiant, numero_plat) == 0) {
                printf("Erreur dans la gestion du stock.\n");
            }

            // Boucle pour la commande d'autres plats
            while (1) {
                char autreChoseMessage[] = "###############################################\nVoulez-vous commander un autre plat ? (oui/non)\n###############################################\n";
                if (write(canal, autreChoseMessage, strlen(autreChoseMessage)) < 0)
                    erreur_IO("write");
                if (write(canal, "\n\n", 2) < 0) {
                    erreur_IO("write");
                }
                lgLue = lireLigne(canal, reponsefinale);
                if (lgLue < 0) {
                    erreur_IO("lireLigne");
                }
                reponsefinale[lgLue - 1] = '\0';

                if (gestionreponse(canal, identifiant, reponsefinale) == 0) {
                    break;
                }
            }

        } else {
            char messageErreur[] = "####################################################\nIdentifiant incorrect. Veuillez réessayer : \n####################################################\n\n";
            if (write(canal, messageErreur, strlen(messageErreur)) < 0)
                erreur_IO("write");
        }
    }

    // Boucle pour gérer la fin de la session client
    while (1) {
        lgLue = lireLigne(canal, ligne);
        if (lgLue < 0)
            erreur_IO("lireLigne");
        else if (lgLue == 0) {
            printf("Client déconnecté de manière inattendue\n");
            break;
        }

        ligne[lgLue - 1] = '\0';

        printf("Serveur: Réception de %d octets : \"%s\"\n", lgLue, ligne);

        if (strcmp(ligne, "fin") == 0) {
            printf("Serveur: fin client\n");
            break;
        }
    }

    // Fermeture du canal de communication
    if (close(canal) == -1)
        erreur_IO("fermeture canal");

    pthread_exit(NULL);
}

void sessionFournisseur(int canal) {
    char ligne[LIGNE_MAX];
    int lgLue;
    char identifiant[LIGNE_MAX];
    int identifiantValide = 0;
    char numero_plat[LIGNE_MAX];
    char reponsefinale[LIGNE_MAX];
    char augmentation[LIGNE_MAX];

    printf("\033[1;32m" "\n******************************************\nLancement d'une Session Fournisseur\n******************************************\n" "\033[0m");

    // Boucle de session pour le fournisseur
    while (1) {
        char demandeIdentifiant[] = "###################################################\nVeuillez entrer votre identifiant Fournisseur :\n###################################################";
        if (write(canal, demandeIdentifiant, strlen(demandeIdentifiant)) < 0)
            erreur_IO("write");

        if (write(canal, "\n\n", 2) < 0)
            erreur_IO("write");

        // Vérification de l'identifiant fournisseur
        while (!identifiantValide) {
            lgLue = lireLigne(canal, identifiant);
            if (lgLue < 0)
                erreur_IO("lireLigne");

            identifiant[lgLue - 1] = '\0';

            if (verifierIdentifiantFournisseur(identifiant)) {
                identifiantValide = 1;
                printf("\033[1;32m" "\n******************************************\nLe Fournisseur : %s s'est connecté\n******************************************\n" "\033[0m", identifiant);

                char messageconf[] = "############################################\nVous êtes connectés en tant que Fournisseur\n";
                if (write(canal, messageconf, strlen(messageconf)) < 0)
                    erreur_IO("write");

                // Création du fichier de modifications pour le fournisseur
                char nomFichier[LIGNE_MAX*2];
                snprintf(nomFichier, sizeof(nomFichier), "Projet/modifications_%s.txt", identifiant);
                FILE *fichierCommande = fopen(nomFichier, "w");
                if (fichierCommande == NULL) {
                    perror("Erreur lors de la création du fichier de commande");
                    pthread_exit(NULL);
                }
                fclose(fichierCommande);

                char messageChoix[] = "1 - Modifier le stock d'un plat\n2 - Ajouter un plat\n3 - Retirer un plat\n############################################\n";
                if (write(canal, messageChoix, strlen(messageChoix)) < 0)
                    erreur_IO("write");

                if (write(canal, "\n\n", 2) < 0)
                    erreur_IO("write");

                // Boucle pour gérer les actions du fournisseur
                while (1) {
                    lgLue = lireLigne(canal, ligne);
                    if (lgLue < 0)
                        erreur_IO("lireLigne");

                    ligne[lgLue - 1] = '\0';

                    if (strcmp(ligne, "1") == 0) {
                        char messageChoix[] = "##################################################################\nEntrez le numéro du plat dont vous voulez modifier le stock :\n##################################################################\n";
                        if (write(canal, messageChoix, strlen(messageChoix)) < 0)
                            erreur_IO("write");

                        if (afficherplat(canal) < 0)
                            erreur_IO("afficherplat");

                        lgLue = lireLigne(canal, numero_plat);
                        if (lgLue < 0)
                            erreur_IO("lireLigne");
                        numero_plat[lgLue - 1] = '\0';

                        char messageAugment[] = "###########################################\nQuelle quantité voulez-vous ajouter :\n###########################################\n";
                        if (write(canal, messageAugment, strlen(messageAugment)) < 0)
                            erreur_IO("write");

                        if (write(canal, "\n\n", 2) < 0)
                            erreur_IO("write");

                        lgLue = lireLigne(canal, augmentation);
                        if (lgLue < 0)
                            erreur_IO("lireLigne");
                        augmentation[lgLue - 1] = '\0';

                        if (gestionstock_fournisseur(canal, numero_plat, augmentation, identifiant) == 0)
                            printf("Erreur dans la gestion du stock\n");

                        // Boucle pour modifier d'autres stocks
                        while (1) {
                            char autreChoseMessage[] = "############################################################\nVoulez-vous modifier le stock d'un autre plat ? (oui/non)\n############################################################\n";
                            if (write(canal, autreChoseMessage, strlen(autreChoseMessage)) < 0)
                                erreur_IO("write");

                            if (write(canal, "\n\n", 2) < 0)
                                erreur_IO("write");

                            lgLue = lireLigne(canal, reponsefinale);
                            if (lgLue < 0)
                                erreur_IO("lireLigne");

                            reponsefinale[lgLue - 1] = '\0';

                            if (gestionreponse_fournisseur(canal, reponsefinale, identifiant) == 0)
                                break;
                        }
                    } else if (strcmp(ligne, "2") == 0) {
                        if (ajouterPlat(canal, identifiant) == 0)
                            printf("Erreur lors de l'ajout d'un plat\n");
                    } else if (strcmp(ligne, "3") == 0) {
                        if (retirerPlat(canal, identifiant) == 0)
                            printf("Erreur lors du retrait d'un plat\n");
                    } else {
                        char invalidChoiceMessage[] = "Choix invalide.\n";
                        if (write(canal, invalidChoiceMessage, strlen(invalidChoiceMessage)) < 0)
                            erreur_IO("write");
                        if (write(canal, "\n\n", 2) < 0)
                            erreur_IO("write");
                    }
                }
            } else {
                char messageErreur[] = "########################################\nIdentifiant incorrect. Veuillez réessayer :\n########################################";
                if (write(canal, messageErreur, strlen(messageErreur)) < 0)
                    erreur_IO("write");
            }

            if (write(canal, "\n\n", 2) < 0) {
                erreur_IO("write");
            }
        }

        // Boucle pour gérer la fin de la session fournisseur
        while (1) {
            lgLue = lireLigne(canal, ligne);
            if (lgLue < 0)
                erreur_IO("lireLigne");
            else if (lgLue == 0) {
                printf("Fournisseur déconnecté de manière inattendue\n");
                break;
            }

            ligne[lgLue - 1] = '\0';

            if (strcmp(ligne, "fin") == 0) {
                printf("Serveur: fin fournisseur\n");
                break;
            }
        }

        // Fermeture du canal de communication
        if (close(canal) == -1)
            erreur_IO("fermeture canal");

        pthread_exit(NULL);
    }
}

int verifierIdentifiantClient(char *identifiant) {
    // Vérification de l'identifiant client dans le fichier
    FILE *fichier = fopen("Projet/identifiant_client.txt", "r");
    if (fichier == NULL) {
        perror("Erreur lors de l'ouverture du fichier identifiant_client.txt");
        return 0;
    }
    char ligne[LIGNE_MAX];

    // Lecture du fichier ligne par ligne
    while (fgets(ligne, sizeof(ligne), fichier) != NULL) {
        ligne[strcspn(ligne, "\n")] = '\0';
        if (strcmp(ligne, identifiant) == 0) {
            fclose(fichier);
            return 1;
        }
    }

    fclose(fichier);
    return 0;
}

void lockMutexCanal(int numWorker) {
    int ret;

    // Verrouillage du mutex du canal du worker
    ret = pthread_mutex_lock(&mutexCanal[numWorker]);
    if (ret != 0)
        erreur_IO("lock mutex canal");
}
void unlockMutexCanal(int numWorker) {
    int ret;

    // Déverrouillage du mutex du canal du worker
    ret = pthread_mutex_unlock(&mutexCanal[numWorker]);
    if (ret != 0)
        erreur_IO("unlock mutex canal");
}

// Vérifie si l'identifiant du fournisseur existe
int verifierIdentifiantFournisseur(char *identifiant) {
    FILE *fichier = fopen("Projet/identifiant_fournisseur.txt", "r");
    if (fichier == NULL) {
        perror("Erreur lors de l'ouverture du fichier identifiant_fournisseur.txt");
        return 0;
    }
    char ligne[LIGNE_MAX];

    while (fgets(ligne, sizeof(ligne), fichier) != NULL) { // On parcourt le fichier
        ligne[strcspn(ligne, "\n")] = '\0';
        if (strcmp(ligne, identifiant) == 0) { // On s'arrête quand on trouve l'identifiant
            fclose(fichier);
            return 1;
        }
    }

    fclose(fichier);
    return 0;
}

// Affiche la liste des plats disponibles
int afficherplat(int canal) {
    FILE *fichier = fopen("Projet/nourriture.txt", "r");
    if (fichier == NULL) {
        perror("Erreur lors de l'ouverture du fichier nourriture.txt");
        return 0;
    }

    char nom[LIGNE_MAX];
    int id;
    int stock;
    char message[LIGNE_MAX * 4];

    while (fscanf(fichier, "%s %d %d", nom, &id, &stock) != EOF) { // On parcourt le fichier
        int n = snprintf(message, sizeof(message), "\nPlat Numéro %d : %s (stock restant : %d)\n", id, nom, stock);
        if (n < 0) {
            erreur_IO("snprintf");
            return 0;
        }

        if (write(canal, message, n) < 0) { // On affiche tous les plats un par un
            erreur_IO("write");
            return 0;
        }
    }

    if (write(canal, "\n\n", 2) < 0) {
        erreur_IO("write");
        return 0;
    }

    fclose(fichier);
    return 0;
}

// Gère le stock après une commande client
int gestionstock(int canal, char *identifiant, char *numeroplat) {
    FILE *fichier = fopen("Projet/nourriture.txt", "r+");
    if (fichier == NULL) {
        perror("Erreur lors de l'ouverture du fichier nourriture.txt");
        return 0;
    }

    char nom[LIGNE_MAX];
    int id, stock;
    long pos;
    int found = 0;
    char buffer[LIGNE_MAX];
    int n;
    char message[LIGNE_MAX * 4];

    while (fgets(buffer, sizeof(buffer), fichier) != NULL) { // On parcourt le fichier
        pos = ftell(fichier);
        sscanf(buffer, "%s %d %d", nom, &id, &stock);

        if (atoi(numeroplat) == id) { // On s'arrête quand on trouve le bon id
            if (stock > 0) { // Vérifie que le stock est > 0
                printf("\033[34m" "\n******************************************\nNouvelle Commande du Client : %s\n******************************************\n" "\033[0m", identifiant);
                stock--; // Diminue le stock de 1 car plat choisi
                found = 1;
                fseek(fichier, pos - strlen(buffer), SEEK_SET);
                fprintf(fichier, "%s %d %d\n", nom, id, stock);
                fflush(fichier);

                n = snprintf(message, sizeof(message), "#################################################\nVous avez choisi le plat %s. Stock restant : %d\n#################################################\n", nom, stock);
                if (n < 0) {
                    erreur_IO("snprintf");
                    fclose(fichier);
                    return 0;
                }

                if (!enregistrerPlatChoisi(identifiant, nom)) {
                    erreur_IO("enregistrerPlatChoisi");
                }

                if (write(canal, message, n) < 0) {
                    erreur_IO("write");
                    fclose(fichier);
                    return 0;
                }

                break;
            } else {
                // Message d'erreur pour une rupture de stock
                char noStockMessage[] = "Désolé, ce plat est en rupture de stock.\n";
                if (write(canal, noStockMessage, strlen(noStockMessage)) < 0) {
                    erreur_IO("write");
                    fclose(fichier);
                    return 0;
                }
                break;
            }
        }
    }

    if (!found) {
        // Message d'erreur pour un plat non trouvé
        char notFoundMessage[] = "Plat non trouvé.\n";
        if (write(canal, notFoundMessage, strlen(notFoundMessage)) < 0) {
            erreur_IO("write");
            fclose(fichier);
            return 0;
        }
    }

    fclose(fichier);
    return found ? 1 : 0;
}

// Gère le stock après une modification fournisseur
int gestionstock_fournisseur(int canal, char *numeroplat, char *augmentation, char *identifiant) {
    FILE *fichier = fopen("Projet/nourriture.txt", "r+");
    if (fichier == NULL) {
        perror("Erreur lors de l'ouverture du fichier nourriture.txt");
        return 0;
    }

    char nom[LIGNE_MAX];
    int id, stock;
    long pos;
    int found = 0;
    char buffer[LIGNE_MAX];
    int n;
    char message[LIGNE_MAX * 4];

    while (fgets(buffer, sizeof(buffer), fichier) != NULL) {
        pos = ftell(fichier);
        sscanf(buffer, "%s %d %d", nom, &id, &stock);
        if (atoi(numeroplat) == id) {
            printf("\033[34m" "\n******************************************\nNouvelle Modification du Fournisseur : %s\n******************************************\n" "\033[0m", identifiant);
            stock += atoi(augmentation);  // Ajoute la quantité spécifiée au stock
            found = 1;
            fseek(fichier, pos - strlen(buffer), SEEK_SET);
            fprintf(fichier, "%s %d %d\n", nom, id, stock);
            fflush(fichier);

            n = snprintf(message, sizeof(message), "Vous avez choisi le plat %s. Nouveau Stock : %d\n", nom, stock);
            if (n < 0) {
                erreur_IO("snprintf");
                fclose(fichier);
                return 0;
            }

            if (!enregistrermodifications(identifiant, nom, augmentation)) {
                erreur_IO("enregistrermodifications");
            }

            if (write(canal, message, n) < 0) {
                erreur_IO("write");
                fclose(fichier);
                return 0;
            }
            break;
        }
    }

    if (!found) {
        char notFoundMessage[] = "Plat non trouvé.\n";
        if (write(canal, notFoundMessage, strlen(notFoundMessage)) < 0) {
            erreur_IO("write");
            fclose(fichier);
            return 0;
        }
    }

    fclose(fichier);
    return found ? 1 : 0;
}

// Gère la réponse du client
int gestionreponse(int canal, char *identifiant, char *reponsefinale) {
    if (strcmp(reponsefinale, "non") == 0) {
        printf("\033[31m" "\n******************************************\nFin de session Client : %s\n******************************************\n" "\033[0m", identifiant);
        affichercommande(canal, identifiant);
        char messageFermeture[] = "Merci beaucoup pour votre commande !\n####################################\n";
        if (write(canal, messageFermeture, strlen(messageFermeture)) < 0)
            erreur_IO("write");
        
        char Fermeture[] = "***";
        if (write(canal, Fermeture, strlen(Fermeture)) < 0)
            erreur_IO("write");
        
        if (shutdown(canal, SHUT_WR) < 0)
            erreur_IO("shutdown");

        if (close(canal) == -1)
            erreur_IO("fermeture canal");

        pthread_exit(NULL);
        return 0;

    } else if (strcmp(reponsefinale, "oui") == 0) {
        char messageChoix[] = "##################################################################\nEntrez le numéro du plat que vous voulez :\n##################################################################\n";
        if (write(canal, messageChoix, strlen(messageChoix)) < 0)
            erreur_IO("write");

        if (afficherplat(canal) < 0)
            erreur_IO("afficherplat");

        char numero_plat[LIGNE_MAX];
        int lgLue = lireLigne(canal, numero_plat);
        if (lgLue < 0)
            erreur_IO("lireLigne");

        numero_plat[lgLue - 1] = '\0';

        if (gestionstock(canal, identifiant, numero_plat) == 0)
            printf("Erreur dans la gestion du stock.\n");
        return 1;
    } else {
        char messageErreur[] = "##########################################################\nRéponse non reconnue. Veuillez répondre par 'oui' ou 'non'.\n##########################################################\n";
        if (write(canal, messageErreur, strlen(messageErreur)) < 0)
            erreur_IO("write");
        if (write(canal, "\n\n", 2) < 0)
            erreur_IO("write");
        return 1;
    }
}

// Gère la réponse du fournisseur
int gestionreponse_fournisseur(int canal, char *reponsefinale, char *identifiant) {
    char augmentation[LIGNE_MAX];
    if (strcmp(reponsefinale, "non") == 0) {
        printf("\033[31m" "\n******************************************\nFin de session Fournisseur : %s\n******************************************\n" "\033[0m", identifiant);
        affichermodifications(canal, identifiant);
        char messageFermeture[] = "Merci beaucoup pour vos modifications !\n####################################\n";
        if (write(canal, messageFermeture, strlen(messageFermeture)) < 0)
            erreur_IO("write");
        
        char Fermeture[] = "***";
        if (write(canal, Fermeture, strlen(Fermeture)) < 0)
            erreur_IO("write");
        
        if (shutdown(canal, SHUT_WR) < 0)
            erreur_IO("shutdown");

        if (close(canal) == -1)
            erreur_IO("fermeture canal");

        pthread_exit(NULL);
        return 0;

    } else if (strcmp(reponsefinale, "oui") == 0) {
        char messageChoix[] = "##################################################################\nEntrez le numéro du plat dont vous voulez modifier le plat :\n##################################################################\n";
        if (write(canal, messageChoix, strlen(messageChoix)) < 0)
            erreur_IO("write");

        if (afficherplat(canal) < 0)
            erreur_IO("afficherplat");

        char numero_plat[LIGNE_MAX];
        int lgLue = lireLigne(canal, numero_plat);
        if (lgLue < 0)
            erreur_IO("lireLigne");

        numero_plat[lgLue - 1] = '\0';
        
        char messageAugment[] = "###########################################\nQuelle quantité voulez-vous ajouter :\n###########################################\n";
        if (write(canal, messageAugment, strlen(messageAugment)) < 0)
                erreur_IO("write");

        if (write(canal, "\n\n", 2) < 0)
                erreur_IO("write");

        lgLue = lireLigne(canal, augmentation);
        if (lgLue < 0)
                 erreur_IO("lireLigne");
        augmentation[lgLue - 1] = '\0';

        if (gestionstock_fournisseur(canal, numero_plat, augmentation, identifiant) == 0)
            printf("Erreur dans la gestion du stock.\n");
        return 1;
    } else {
        char messageErreur[] = "##########################################################\nRéponse non reconnue. Veuillez répondre par 'oui' ou 'non'.\n##########################################################\n";
        if (write(canal, messageErreur, strlen(messageErreur)) < 0)
            erreur_IO("write");
        if (write(canal, "\n\n", 2) < 0)
            erreur_IO("write");
        return 1;
    }
}

// Crée un nouveau compte client
int creerCompte(int canal) {
    char identifiant[LIGNE_MAX];
    int lgLue;

    char demandeIdentifiant[] = "#################################################\nVeuillez entrer votre nouvel identifiant :\n#################################################";
    if (write(canal, demandeIdentifiant, strlen(demandeIdentifiant)) < 0)
        erreur_IO("write");

    if (write(canal, "\n\n", 2) < 0)
        erreur_IO("write");

    lgLue = lireLigne(canal, identifiant);
    if (lgLue < 0)
        erreur_IO("lireLigne");

    identifiant[lgLue - 1] = '\0';

    FILE *fichier = fopen("Projet/identifiant_client.txt", "a");
    if (fichier == NULL) {
        perror("Erreur lors de l'ouverture du fichier identifiant_client.txt");
        return 0;
    }

    // Ajoute le nouvel identifiant à la liste des identifiants clients
    fprintf(fichier, "\n%s", identifiant);
    fclose(fichier);

    char confirmation[] = "\n###########################################\nCompte créé avec succès.\n###########################################\n";
    if (write(canal, confirmation, strlen(confirmation)) < 0)
        erreur_IO("write");
    printf("\n******************************************\nUn nouveau compte Client a été crée\nBienvenue %s\n******************************************\n", identifiant);

    return 1;
}

// Enregistre le plat choisi par le client
int enregistrerPlatChoisi(char *identifiant, char *plat) {
    char nomFichier[LIGNE_MAX * 2];
    // Construire le nom de fichier basé sur l'identifiant du client
    snprintf(nomFichier, sizeof(nomFichier), "Projet/commande_%s.txt", identifiant);
    FILE *fichierCommande = fopen(nomFichier, "a");
    if (fichierCommande == NULL) {
        perror("Erreur lors de l'ouverture du fichier de commande");
        return 0;
    }

    // Ajoute le plat choisi au fichier de commande du client
    fprintf(fichierCommande, "%s\n", plat);
    fclose(fichierCommande);
    return 1;
}

// Enregistre les modifications apportées par le fournisseur
int enregistrermodifications(char *identifiant, char *plat, char *stock) {
    char nomFichier[LIGNE_MAX * 2];
    // Construire le nom de fichier basé sur l'identifiant du fournisseur
    snprintf(nomFichier, sizeof(nomFichier), "Projet/modifications_%s.txt", identifiant);
    FILE *fichierCommande = fopen(nomFichier, "a");
    if (fichierCommande == NULL) {
        perror("Erreur lors de l'ouverture du fichier de commande");
        return 0;
    }

    // Ajoute les modifications au fichier de modifications du fournisseur
    fprintf(fichierCommande, "%s %s\n", plat, stock);
    fclose(fichierCommande);
    return 1;
}

// Affiche les commandes d'un client spécifique
int affichercommande(int canal, char *identifiant) {
    char nomFichier[LIGNE_MAX * 2];
    // Construire le nom de fichier basé sur l'identifiant du client
    snprintf(nomFichier, sizeof(nomFichier), "Projet/commande_%s.txt", identifiant);
    FILE *fichierCommande = fopen(nomFichier, "r");
    if (fichierCommande == NULL) {
        // Gestion de l'erreur si le fichier ne peut pas être ouvert
        perror("Erreur lors de l'ouverture du fichier commande.txt");
        return 0;
    }

    // Envoyer un récapitulatif des commandes au client
    char Commande[] = "####################################\nVotre Récapitulatif :\n";
    if (write(canal, Commande, strlen(Commande)) < 0)
        erreur_IO("write");

    char nom[LIGNE_MAX];
    char message[LIGNE_MAX * 4];

    // Lire chaque plat commandé depuis le fichier et l'envoyer au client
    while (fscanf(fichierCommande, "%s", nom) != EOF) {
        int n = snprintf(message, sizeof(message), "Plat : %s\n", nom);
        if (n < 0) {
            erreur_IO("snprintf");
            return 0;
        }

        if (write(canal, message, n) < 0) {
            erreur_IO("write");
            return 0;
        }
    }

    fclose(fichierCommande);
    return 0;
}

// Affiche les modifications effectuées par un fournisseur spécifique
int affichermodifications(int canal, char *identifiant) {
    char nomFichier[LIGNE_MAX * 2];
    // Construire le nom de fichier basé sur l'identifiant du fournisseur
    snprintf(nomFichier, sizeof(nomFichier), "Projet/modifications_%s.txt", identifiant);
    
    FILE *fichierCommande = fopen(nomFichier, "r");
    if (fichierCommande == NULL) {
        // Gestion de l'erreur si le fichier ne peut pas être ouvert
        perror("Erreur lors de l'ouverture du fichier de modifications");
        return 0;
    }

    // Envoyer un récapitulatif des modifications au fournisseur
    char Commande[] = "####################################\nVotre Récapitulatif :\n";
    if (write(canal, Commande, strlen(Commande)) < 0) {
        erreur_IO("write");
    }

    char nom[LIGNE_MAX];
    int stock;
    char message[LIGNE_MAX * 4];

    // Lire chaque modification depuis le fichier et l'envoyer au fournisseur
    while (fscanf(fichierCommande, "%s %d", nom, &stock) != EOF) {
        int n = snprintf(message, sizeof(message), "Vous avez modifié le Plat : %s de : %d\n", nom, stock);
        if (n < 0) {
            erreur_IO("snprintf");
            fclose(fichierCommande);
            return 0;
        }

        if (write(canal, message, n) < 0) {
            erreur_IO("write");
            fclose(fichierCommande);
            return 0;
        }
    }

    fclose(fichierCommande);
    return 1;
}

// Ajoute un nouveau plat au fichier des plats
int ajouterPlat(int canal, char* identifiant) {
    char nomPlat[LIGNE_MAX];
    char stockInitial[LIGNE_MAX];
    FILE *fichier = fopen("Projet/nourriture.txt", "a+");
    if (fichier == NULL) {
        // Gestion de l'erreur si le fichier ne peut pas être ouvert
        perror("Erreur lors de l'ouverture du fichier nourriture.txt");
        return 0;
    }

    // Demander le nom du plat au fournisseur
    char demandeNom[] = "#########################\nEntrez le nom du plat :\n#########################\n";
    if (write(canal, demandeNom, strlen(demandeNom)) < 0)
        erreur_IO("write");
    
    if (write(canal, "\n\n", 2) < 0)
        erreur_IO("write");

    int lgLue = lireLigne(canal, nomPlat);
    if (lgLue < 0)
        erreur_IO("lireLigne");
    nomPlat[lgLue - 1] = '\0';

    // Demander le stock initial du plat
    char demandeStock[] = "#########################\nEntrez le stock initial :\n#########################\n";
    if (write(canal, demandeStock, strlen(demandeStock)) < 0)
        erreur_IO("write");
    
    if (write(canal, "\n\n", 2) < 0)
        erreur_IO("write");

    lgLue = lireLigne(canal, stockInitial);
    if (lgLue < 0)
        erreur_IO("lireLigne");
    stockInitial[lgLue - 1] = '\0';

    // Calculer le nouvel ID du plat
    int dernierId = 0;
    char ligne[LIGNE_MAX];
    while (fgets(ligne, sizeof(ligne), fichier) != NULL) {
        int id;
        sscanf(ligne, "%*s %d %*d", &id);
        if (id > dernierId)
            dernierId = id;
    }
    int nouvelId = dernierId + 1;

    // Ajouter le nouveau plat au fichier
    fprintf(fichier, "%s %d %s\n", nomPlat, nouvelId, stockInitial);
    fflush(fichier);
    fclose(fichier);

    // Confirmation de l'ajout du plat
    printf("\033[34m" "\n******************************************\nNouvelle Ajout du Fournisseur : %s\n******************************************\n" "\033[0m", identifiant);
    char confirmationMessage[LIGNE_MAX*3];
    snprintf(confirmationMessage, sizeof(confirmationMessage), "\n####################################\nPlat ajouté : %s (ID : %d, Stock : %s)\n", nomPlat, nouvelId, stockInitial);
    if (write(canal, confirmationMessage, strlen(confirmationMessage)) < 0)
        erreur_IO("write");
    
    // Boucle de questions ajouter plat
    while (1) {
        char welcomeMessage[] = "#######################################\n Voulez-vous ajouter un autre plat ? (oui/non) \n#######################################\n";
        if (write(canal, welcomeMessage, sizeof(welcomeMessage)) < 0)
            erreur_IO("write");

        if (write(canal, "\n\n", 2) < 0)
            erreur_IO("write");

        lgLue = lireLigne(canal, ligne);
        if (lgLue < 0)
            erreur_IO("lireLigne");

        ligne[lgLue - 1] = '\0';

        if (strcmp(ligne, "oui") == 0 || strcmp(ligne, "non") == 0) {
            if(strcmp(ligne, "non") == 0){
                break;
            }
            else{
                ajouterPlat(canal,identifiant);
            }
        }
         else {
            char erreurMessage[] = "Erreur lors du choix, rentrez oui ou non.\n";
            if (write(canal, erreurMessage, strlen(erreurMessage)) < 0)
                erreur_IO("write");
        }
    }
    
    // Fin de session fournisseur
    printf("\033[31m" "\n******************************************\nFin de session Fournisseur : %s\n******************************************\n" "\033[0m", identifiant);
    char messageFermeture[] = "Merci beaucoup pour vos ajouts !\n####################################\n";
    if (write(canal, messageFermeture, strlen(messageFermeture)) < 0)
        erreur_IO("write");
    
    char Fermeture[] = "***";
    if (write(canal, Fermeture, strlen(Fermeture)) < 0)
        erreur_IO("write");
    
    if (shutdown(canal, SHUT_WR) < 0)
        erreur_IO("shutdown");

    if (close(canal) == -1)
        erreur_IO("fermeture canal");

    pthread_exit(NULL);

    return 1;
}

// Retire un plat existant du fichier des plats
int retirerPlat(int canal, char *identifiant) {
    char numeroPlat[LIGNE_MAX];
    FILE *fichier = fopen("Projet/nourriture.txt", "r");
    FILE *tempFichier = fopen("Projet/nourriture_temp.txt", "w");
    if (fichier == NULL || tempFichier == NULL) {
        // Gestion de l'erreur si les fichiers ne peuvent pas être ouverts
        perror("Erreur lors de l'ouverture des fichiers");
        return 0;
    }

    // Demander le numéro du plat à retirer
    char demandeNumero[] = "#############################\nEntrez le numéro du plat à retirer :\n#############################\n";
    if (write(canal, demandeNumero, strlen(demandeNumero)) < 0)
        erreur_IO("write");

    // Afficher les plats disponibles pour aider le fournisseur à choisir
    if (afficherplat(canal) < 0)
        erreur_IO("afficherplat");

    int lgLue = lireLigne(canal, numeroPlat);
    if (lgLue < 0)
        erreur_IO("lireLigne");
    numeroPlat[lgLue - 1] = '\0';

    // Copier toutes les lignes sauf celle à supprimer dans un fichier temporaire
    char ligne[LIGNE_MAX];
    int id;
    while (fgets(ligne, sizeof(ligne), fichier) != NULL) {
        sscanf(ligne, "%*s %d %*d", &id);
        if (id != atoi(numeroPlat)) {
            fputs(ligne, tempFichier);
        }
    }

    fclose(fichier);
    fclose(tempFichier);

    // Remplacer l'ancien fichier par le nouveau fichier temporaire
    remove("Projet/nourriture.txt");
    rename("Projet/nourriture_temp.txt", "Projet/nourriture.txt");

    // Confirmation du retrait du plat
    printf("\033[34m" "\n******************************************\nNouveau Retrait du Fournisseur : %s\n******************************************\n" "\033[0m", identifiant);

    char confirmationMessage[LIGNE_MAX*2];
    snprintf(confirmationMessage, sizeof(confirmationMessage), "\n####################################\nPlat avec ID %s retiré.\n", numeroPlat);
    if (write(canal, confirmationMessage, strlen(confirmationMessage)) < 0)
        erreur_IO("write");

    // Boucle de questions retirer plat
    while (1) {
        char welcomeMessage[] = "#######################################\n Voulez-vous retirez un autre plat ? (oui/non) \n#######################################\n";
        if (write(canal, welcomeMessage, sizeof(welcomeMessage)) < 0)
            erreur_IO("write");

        if (write(canal, "\n\n", 2) < 0)
            erreur_IO("write");

        lgLue = lireLigne(canal, ligne);
        if (lgLue < 0)
            erreur_IO("lireLigne");

        ligne[lgLue - 1] = '\0';

        if (strcmp(ligne, "oui") == 0 || strcmp(ligne, "non") == 0) {
            if(strcmp(ligne, "non") == 0){
                break;
            }
            else{
                retirerPlat(canal,identifiant);
            }
        }
         else {
            char erreurMessage[] = "Erreur lors du choix, rentrez oui ou non.\n";
            if (write(canal, erreurMessage, strlen(erreurMessage)) < 0)
                erreur_IO("write");
        }
    }
    
    // Fin de session fournisseur
    printf("\033[31m" "\n******************************************\nFin de session Fournisseur : %s\n******************************************\n" "\033[0m", identifiant);
    char messageFermeture[] = "Merci beaucoup pour vos ajouts !\n####################################\n";
    if (write(canal, messageFermeture, strlen(messageFermeture)) < 0)
        erreur_IO("write");
    
    char Fermeture[] = "***";
    if (write(canal, Fermeture, strlen(Fermeture)) < 0)
        erreur_IO("write");
    
    if (shutdown(canal, SHUT_WR) < 0)
        erreur_IO("shutdown");

    if (close(canal) == -1)
        erreur_IO("fermeture canal");

    pthread_exit(NULL);

    return 1;
}
