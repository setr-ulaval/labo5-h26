/******************************************************************************
 * Laboratoire 5
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier implémentant les fonctions de gestion du tampon circulaire
 ******************************************************************************/

#include "tamponCirculaire.h"
#include "utils.h"
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

// Plusieurs variables globales statiques (pour qu'elles ne soient accessible
// que dans les fonctions de ce fichier) sont declarees ici. Elle servent a
// conserver l'etat du tampon circulaire ainsi qu'a mesurer certains elements
// utiles au calcul des statistiques. Vous etes libres d'en creer d'autres si
// vous en voyez le besoin.

// Pointe vers la memoire allouee pour le tampon circulaire
static char *memoire;

// Taile du tampon circulaire (en nombre d'elements de type struct requete)
static size_t memoireTaille;

// Positions de lecture et d'ecriture, et longueur actuelle du tampon circulaire
static unsigned int posLecture, posEcriture, longueurCourante;

// Mutex permettant de proteger les acces au tampon circulaire
// N'oubliez pas que _deux_ threads vont tenter de faire des operations en
// parallele!
pthread_mutex_t mutexTampon;

// Pour les statistiques
static unsigned int nombreRequetesRecues, nombreRequetesTraitees, nombreRequetesPerdues;

// On calcule les statistiques par période de N secondes.
// Le tempsDebutPeriode permet de se rappeler du temps de debut de la periode où
// les statistiques sont mesurées La variable sommeTempsAttente contient la
// somme de toutes les periodes d'attente pour les requetes (vous pourrez donc
// calculer la moyenne du temps d'attente en utilisant les autres variables sur
// le nombre de requetes).
static double tempsDebutPeriode, sommeTempsAttente;

static const unsigned long REQUETE_SIZE = sizeof(struct requete);

int initTamponCirculaire(size_t taille)
{
	memoireTaille = taille;
	posLecture = 0;
	posEcriture = 0;
	longueurCourante = 0;

	pthread_mutexattr_t mutexAttr = {0};
	pthread_mutexattr_init(&mutexAttr);

	if (pthread_mutex_init(&mutexTampon, &mutexAttr) != 0) {
		perror("Failed to initialize mutex");
		pthread_mutexattr_destroy(&mutexAttr);
		return EXIT_FAILURE;
	}
	pthread_mutexattr_destroy(&mutexAttr);

	memoire = calloc(taille, REQUETE_SIZE);

	if (memoire == NULL) {
		perror("Failed to allocate memory for circular buffer.");
		return EXIT_FAILURE;
	}

	resetStats();
	return EXIT_SUCCESS;
}

void resetStats(void)
{
	pthread_mutex_lock(&mutexTampon);
	nombreRequetesRecues = 0;
	nombreRequetesPerdues = 0;
	nombreRequetesTraitees = 0;
	sommeTempsAttente = 0;
	tempsDebutPeriode = get_time();
	pthread_mutex_unlock(&mutexTampon);
}

void calculeStats(struct statistiques *stats)
{
	pthread_mutex_lock(&mutexTampon);

	const unsigned int tmpNombreRequetesRecues = nombreRequetesRecues;
	const unsigned int tmpNombreRequetesTraitees = nombreRequetesTraitees;
	const unsigned int tmpNombreRequetesPerdues = nombreRequetesPerdues;
	const unsigned int tmpLongueurCourante = longueurCourante;
	const double tmpSommeTempsAttente = sommeTempsAttente;
	const double tmpTempsDebutPeriode = tempsDebutPeriode;

	pthread_mutex_unlock(&mutexTampon);

	double deltaT = get_time() - tmpTempsDebutPeriode;

	stats->lambda = (deltaT > 0) ? tmpNombreRequetesRecues / deltaT : 0;
	stats->mu = (deltaT > 0) ? tmpNombreRequetesTraitees / deltaT : 0;
	stats->rho = (stats->mu > 0) ? stats->lambda / stats->mu : 0;

	stats->nombreRequetesEnAttente = tmpLongueurCourante;
	stats->nombreRequetesTraitees = tmpNombreRequetesTraitees;
	stats->nombreRequetesPerdues = tmpNombreRequetesPerdues;

	stats->tempsTraitementMoyen = (tmpNombreRequetesTraitees > 0) ? tmpSommeTempsAttente / tmpNombreRequetesTraitees : 0;
}

int insererDonnee(struct requete *req)
{
	pthread_mutex_lock(&mutexTampon);
	if (longueurCourante == memoireTaille) {
		struct requete *oldestReq = (struct requete *)(memoire + (posLecture * REQUETE_SIZE));
		free(oldestReq->data);

		posLecture = (posLecture + 1) % memoireTaille;
		nombreRequetesPerdues++;
		longueurCourante--;
	}

	memcpy(memoire + (posEcriture * REQUETE_SIZE), req, REQUETE_SIZE);
	posEcriture = (posEcriture + 1) % memoireTaille;
	longueurCourante++;
	nombreRequetesRecues++;
	pthread_mutex_unlock(&mutexTampon);

	return 0;
}

int consommerDonnee(struct requete *req)
{
	pthread_mutex_lock(&mutexTampon);

	if (longueurCourante == 0) {
		pthread_mutex_unlock(&mutexTampon);
		return 0;
	}

	memcpy(req, memoire + (posLecture * REQUETE_SIZE), REQUETE_SIZE);
	sommeTempsAttente += get_time() - req->tempsReception;
	posLecture = (posLecture + 1) % memoireTaille;
	longueurCourante--;
	nombreRequetesTraitees++;
	pthread_mutex_unlock(&mutexTampon);

	return 1;
}

unsigned int longueurFile(void)
{
	pthread_mutex_lock(&mutexTampon);
	unsigned int size = longueurCourante;
	pthread_mutex_unlock(&mutexTampon);

	return size;
}
