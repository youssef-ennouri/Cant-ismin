#include "pse.h"

int main(int argc, char *argv[]) { 
    int sock, ret; 
    struct sockaddr_in *adrServ;
    int fin = FAUX;
    char ligne[LIGNE_MAX]; 
    char buffer[LIGNE_MAX];

    if (argc != 3) // Vérification du nombre d'arguments en ligne de commande
        erreur("usage: %s machine port\n", argv[0]); 

    printf("******************************************\nClient : Connexion au Serveur\n"); 

    sock = socket(AF_INET, SOCK_STREAM, 0); // Création d'un socket TCP
    if (sock < 0) 
        erreur_IO("socket"); 

    adrServ = resolv(argv[1], argv[2]); // Adresse IP et du port du serveur à partir des arguments de la ligne de commande
    if (adrServ == NULL) 
        erreur("adresse %s port %s inconnus\n", argv[1], argv[2]); 

    printf("Client : Adresse %s et Port %hu\n",
            stringIP(ntohl(adrServ->sin_addr.s_addr)),
            ntohs(adrServ->sin_port)); // Affichage de l'adresse IP et du port du serveur

    printf("Client : Connectée au Serveur\n******************************************\n");

    ret = connect(sock, (struct sockaddr *)adrServ, sizeof(struct sockaddr_in)); // Tentative de connexion au serveur
    if (ret < 0) 
        erreur_IO("connect"); 

    // Affichage du message de bienvenue reçu du serveur
    while ((ret = read(sock, buffer, sizeof(buffer) - 1)) > 0) { // Lecture des données reçues du serveur
        buffer[ret] = '\0';
        printf("%s", buffer); // Affichage des données lues
        if (strchr(buffer, '\n') != NULL) break; 
    }
    if (ret < 0) 
        erreur_IO("read");

    // Boucle principale pour envoyer des réponses au serveur et recevoir des réponses
    while (!fin) {
        printf("\033[31m" "\nRéponse > " "\033[0m"); // Affichage de l'invite de réponse
        if (fgets(ligne, LIGNE_MAX, stdin) == NULL) // Lecture de la réponse de l'utilisateur
            erreur("saisie fin de fichier\n");

        if (write(sock, ligne, strlen(ligne)) < 0) // Envoi de la réponse au serveur
            erreur_IO("write");

        printf("\033[34m" "\n****************** Réponse envoyée ******************\n\n" "\033[0m");
        // Lecture de la réponse du serveur
        // char *reponse = malloc(LIGNE_MAX); 
        // int reponse_longueur = 0; 
        while ((ret = read(sock, buffer, sizeof(buffer) - 1)) > 0) { // Lecture des données reçues du serveur
            buffer[ret] = '\0'; 
            printf("%s", buffer); // Affichage des données lues
            // strncat(reponse, buffer, ret); 
            // reponse_longueur += ret;
            if (buffer[ret - 1] == '\n' && buffer[ret - 2] == '\n') { // Si deux caractères de nouvelle ligne sont consécutifs
                break; // Sortie de la boucle
            }
            if (strstr(buffer, "***") != NULL) { // Vérification si la réponse contient "***"
                fin = VRAI; // Définition de fin à VRAI pour terminer l'exécution
                break; 
            }
        }
        free(reponse); 
        if (ret < 0) 
            erreur_IO("read");
    }

    if (close(sock) == -1) // Fermeture du socket
        erreur_IO("close socket");

    exit(EXIT_SUCCESS);
}