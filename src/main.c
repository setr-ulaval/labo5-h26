/******************************************************************************
 * Laboratoire 5
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier principal
 ******************************************************************************/

#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "emulateurClavier.h"
#include "tamponCirculaire.h"
#include "utils.h"

static void destroyPipe(struct pipeHandle *pipe)
{
	if (pipe->fd >= 0) {
		close(pipe->fd);
		pipe->fd = -1;
	}

	if (pipe->createdByUs) {
		unlink(pipe->path);
		pipe->createdByUs = 0;
	}
}

static int initPipe(struct pipeHandle *pipe, const char *path)
{
	pipe->fd = -1;
	pipe->path = path;
	pipe->createdByUs = 0;

	if (mkfifo(path, 0666) == 0) {
		pipe->createdByUs = 1;
	} else if (errno != EEXIST) {
		perror("Named pipe creation failed.");
		return EXIT_FAILURE;
	}

	pipe->fd = open(path, O_RDONLY);
	if (pipe->fd < 0) {
		perror("Cannot open named pipe to read.");
		destroyPipe(pipe);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int initBarrier(pthread_barrier_t *barrier)
{
	pthread_barrierattr_t barrierAttr = {0};

	if (pthread_barrierattr_init(&barrierAttr) != 0) {
		perror("Barrier attr initialization failed.");
		return EXIT_FAILURE;
	}

	if (pthread_barrierattr_setpshared(&barrierAttr, PTHREAD_PROCESS_PRIVATE) != 0) {
		perror("Barrier attr setpshared failed.");
		pthread_barrierattr_destroy(&barrierAttr);
		return EXIT_FAILURE;
	}

	if (pthread_barrier_init(barrier, &barrierAttr, 2) != 0) {
		perror("Barrier initialization failed.");
		pthread_barrierattr_destroy(&barrierAttr);
		return EXIT_FAILURE;
	}

	pthread_barrierattr_destroy(&barrierAttr);
	return EXIT_SUCCESS;
}

static void *threadFonctionClavier(void *args)
{
	// Implementez ici votre fonction de thread pour l'ecriture sur le bus USB
	// La premiere des choses est de recuperer les arguments (deja fait pour vous)
	struct infoThreadClavier *infos = (struct infoThreadClavier *)args;

	FILE *fd = infos->pointeurClavier;
	unsigned int time = infos->tempsTraitementParCaractereMicroSecondes;

	pthread_barrier_wait(infos->barriere);

	// Vous devez ensuite attendre sur la barriere passee dans les arguments
	// pour etre certain de commencer au meme moment que le thread lecteur

	// Finalement, ecrivez dans cette boucle la logique du thread, qui doit:
	// 1) Tenter d'obtenir une requete depuis le tampon circulaire avec
	// consommerDonnee() 2) S'il n'y en a pas, attendre un cours laps de temps
	// (par exemple usleep(500)) 3) S'il y en a une, appeler ecrireCaracteres avec
	// les informations requises 4) Liberer la memoire du champ data de la requete
	// avec la fonction free(), puisque
	//      la requete est maintenant terminee

	struct requete req;
	while (1) {
		if (consommerDonnee(&req) <= 0) {
			usleep(500);
			continue;
		}

		ecrireCaracteres(fd, req.data, req.taille, time); // perror already displays everything, no need to do something in case of error
		free(req.data);
	}
	return NULL;
}

int updateMessage(struct requete *req, char *new, size_t len)
{
	if (req->data == NULL) {
		req->data = strndup(new, len);
		if (req->data == NULL) {
			perror("Failed to allocate memory for message.");
			req->taille = 0;
			return -1;
		}
		req->taille = len;
	} else {
		size_t newLength = (req->taille + len) * sizeof(char);
		char *newMessage = realloc(req->data, newLength + 1);
		if (newMessage == NULL) {
			perror("Failed to allocate memory for message.");
			free(req->data);
			req->data = NULL;
			req->taille = 0;
			return -1;
		}
		req->data = newMessage;
		strncat(req->data, new, len);
		req->taille = newLength;
	}
	return 0;
}

static void *threadFonctionLecture(void *args)
{

	// Implementez ici votre fonction de thread pour la lecture sur le named pipe
	// La premiere des choses est de recuperer les arguments (deja fait pour vous)
	struct infoThreadLecture *infos = (struct infoThreadLecture *)args;

	const int pipeFd = infos->pipeFd;
	pthread_barrier_t *barrier = infos->barriere;

	// Ces champs vous seront utiles pour l'appel a select()
	fd_set setFd;
	int nfds = infos->pipeFd + 1;

	pthread_barrier_wait(barrier);

	// Vous devez ensuite attendre sur la barriere passee dans les arguments
	// pour etre certain de commencer au meme moment que le thread lecteur

	// Finalement, ecrivez dans cette boucle la logique du thread, qui doit:
	// 1) Remplir setFd en utilisant FD_ZERO et FD_SET correctement, pour faire en
	// sorte
	//      d'attendre sur infos->pipeFd
	// 2) Appeler select(), sans timeout, avec setFd comme argument de lecture (on
	// veut bien
	//      lire sur le pipe)
	// 3) Lire les valeurs sur le named pipe
	// 4) Si une de ses valeurs est le caracteres ASCII EOT (0x4), alors c'est la
	// fin d'un
	//      message. Vous creez alors une nouvelle requete et utilisez
	//      insererDonnee() pour l'inserer dans le tampon circulaire. Notez que le
	//      caractere EOT ne doit PAS se retrouver dans le champ data de la
	//      requete! N'oubliez pas egalement de donner la bonne valeur aux champs
	//      taille et tempsReception.

	char buffer[256] = {0};
	struct requete req = {0};
	while (1) {
		FD_ZERO(&setFd);
		FD_SET(pipeFd, &setFd);
		int res = select(nfds, &setFd, NULL, NULL, NULL);
		if (res < 0) {
			perror("Select failed.");
			continue;
		}
		if (res == 0) {
			continue;
		}

		res = read(pipeFd, buffer, sizeof(buffer) - 1);
		if (res < 0) {
			perror("Read from pipe failed.");
			continue;
		}
		if (res == 0) {
			usleep(5000);
			continue;
		}
		buffer[res] = '\0';

		char *token = memchr(buffer, 0x4, res);
		while (token != NULL) { // 0x4 found, end of message. We either queue it to previous unfinished message, or create a new one if there is no unfinished message
			size_t messagePartLength = token - buffer;

			if (req.data == NULL) {
				req.tempsReception = get_time();
			}

			if (updateMessage(&req, buffer, messagePartLength) != 0)
				break;

			insererDonnee(&req);

			req.data = NULL;
			req.taille = 0;

			res = res - messagePartLength - 1;
			memmove(buffer, token + 1, res);
			token = memchr(buffer, 0x4, res);
		}
		if (res > 0) { // no EOT found, we have an unfinished message part that we need to save for the next read
			if (req.data == NULL) {
				req.tempsReception = get_time();
			}

			updateMessage(&req, buffer, res);
		}
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc < 4) {
		printf("Pas assez d'arguments! Attendu : ./emulateurClavier cheminPipe "
		       "tempsAttenteParPaquet tailleTamponCirculaire\n");
		return EXIT_FAILURE;
	}

	const char *namedPipePath = argv[1];
	const int waitTime = atoi(argv[2]);
	const int bufferSize = atoi(argv[3]);

	mlockall(MCL_CURRENT | MCL_FUTURE);

	// A ce stade, vous pouvez consider que:
	// argv[1] contient un chemin valide vers un named pipe
	// argv[2] contient un entier valide (que vous pouvez convertir avec atoi())
	// representant le nombre de microsecondes a
	//      attendre a chaque envoi de paquet
	// argv[3] contient un entier valide (que vous pouvez convertir avec atoi())
	// contenant la taille voulue pour le tampon
	//      circulaire

	// Vous avez plusieurs taches d'initialisation a faire :
	//
	// 1) Ouvrir le named pipe

	struct pipeHandle pipe;
	if (initPipe(&pipe, namedPipePath) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	// 2) Declarer et initialiser la barriere

	pthread_barrier_t barrier = {0};
	if (initBarrier(&barrier) != EXIT_SUCCESS) {
		destroyPipe(&pipe);
		return EXIT_FAILURE;
	}

	// 3) Initialiser le tampon circulaire avec la bonne taille

	int tamponRes = initTamponCirculaire(bufferSize);

	if (tamponRes != EXIT_SUCCESS) {
		perror("Circular buffer initialization failed.");
		pthread_barrier_destroy(&barrier);
		destroyPipe(&pipe);
		return EXIT_FAILURE;
	}

	// 4) Creer et lancer les threads clavier et lecteur, en leur passant les bons
	// arguments dans leur struct de configuration respective

	FILE *keyboardFd = initClavier();
	if (keyboardFd == NULL) {
		perror("Keyboard initialization failed.");
		pthread_barrier_destroy(&barrier);
		destroyPipe(&pipe);
		return EXIT_FAILURE;
	}

	pthread_t threadClavier;
	struct infoThreadClavier infoClavier = {.pointeurClavier = keyboardFd, .barriere = &barrier, .tempsTraitementParCaractereMicroSecondes = waitTime};

	if (pthread_create(&threadClavier, NULL, threadFonctionClavier, &infoClavier) != 0) {
		perror("Failed to create keyboard thread.");
		fclose(keyboardFd);
		pthread_barrier_destroy(&barrier);
		destroyPipe(&pipe);
		return EXIT_FAILURE;
	}

	pthread_t threadLecture;
	struct infoThreadLecture infoLecture = {.pipeFd = pipe.fd, .barriere = &barrier};

	if (pthread_create(&threadLecture, NULL, threadFonctionLecture, &infoLecture) != 0) {
		perror("Failed to create reader thread.");
		pthread_cancel(threadClavier);
		pthread_join(threadClavier, NULL);
		fclose(keyboardFd);
		pthread_barrier_destroy(&barrier);
		destroyPipe(&pipe);
		return EXIT_FAILURE;
	}

	// La boucle de traitement est deja implementee pour vous. Toutefois, si vous
	// voulez eviter l'affichage des statistiques (qui efface le terminal a chaque
	// fois), vous pouvez commenter la ligne afficherStats().
	struct statistiques stats;
	double tempsDebut = get_time();
	while (1) {
		// Affichage des statistiques toutes les 2 secondes
		calculeStats(&stats);
		afficherStats((unsigned int)(round(get_time() - tempsDebut)), &stats);
		resetStats();
		usleep(2e6);
	}

	return EXIT_SUCCESS;
}
