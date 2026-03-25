/******************************************************************************
 * Laboratoire 5
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2026
 * Marc-André Gardner
 *
 * Fichier implémentant les fonctions de l'emulateur de clavier
 ******************************************************************************/

#include "emulateurClavier.h"
#include "utils.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

FILE *initClavier(void)
{
#ifdef SIMULATION_MODE
	FILE *f = fopen("/tmp/hidg0_sim.bin", "wb");
#else
	FILE *f = fopen(FICHIER_CLAVIER_VIRTUEL, "wb");
#endif
	setbuf(f, NULL); // On desactive le buffering pour eviter tout delai
	return f;
}

char ASCItoHID(char c)
{
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 4;
	}
	if (c >= '1' && c <= '9') {
		return c - '1' + 30;
	}
	switch (c) {
		case ',':
			return 54;
		case '.':
			return 55;
		case ' ':
			return 44;
		case '\n':
			return 40;
		case '0':
			return 39;
	}
	assert(false);
	return -1;
}

int queuePacket(FILE *target, const char *packet, bool empty, int waitTime)
{
	size_t res = fwrite(packet, LONGUEUR_USB_PAQUET, 1, target);

	if (res != 1) {
		perror("Failed to write packet.");
		return -1;
	}

	if (!empty) {
		res = fwrite(EMPTY_PACKET, LONGUEUR_USB_PAQUET, 1, target);

		if (res != 1) {
			perror("Failed to write empty packet.");
			return -1;
		}
	}
	usleep(waitTime);
	return 0;
}

void getCharInfo(char c, struct characterInformations *info)
{
	char lower;
	if (isupper(c)) {
		info->leftShift = true;
		lower = tolower(c);
	} else {
		info->leftShift = false;
		lower = c;
	}

	info->value = ASCItoHID(lower);
}

void initPacketInformations(struct packetInformations *info)
{
	info->initialized = false;
	info->index = 0;
	info->leftShift = false;
}

bool containsInvalidCharacters(char const *caracteres, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)caracteres[i];
		if (!isalnum(c) && c != '\n' && c != ' ' && c != ',' && c != '.')
			return true;
	}
	return false;
}

int ecrireCaracteres(FILE *periphClavier, const char *caracteres, size_t len, unsigned int tempsTraitementParPaquetMicroSecondes)
{
	if (containsInvalidCharacters(caracteres, len)) {
		perror("Invalid characters found.");
		return -1;
	}

	char buffer[LONGUEUR_USB_PAQUET] = {0};
	size_t i = 0;
	struct packetInformations packetInformations = {0};
	struct characterInformations characterInformations = {0};

	initPacketInformations(&packetInformations);

	while (i < len) {
		getCharInfo(caracteres[i], &characterInformations);

		if (packetInformations.initialized == false) {
			memset(buffer, '\0', LONGUEUR_USB_PAQUET);
			buffer[0] = characterInformations.leftShift ? 2 : 0;
			packetInformations.leftShift = characterInformations.leftShift;
			packetInformations.initialized = true;
			packetInformations.index = 0;
		}

		if (packetInformations.leftShift != characterInformations.leftShift) { // we queue current packet and reset
			int res = queuePacket(periphClavier, buffer, 0, tempsTraitementParPaquetMicroSecondes);

			if (res < 0)
				return -1;

			initPacketInformations(&packetInformations);
			continue;
		}

		buffer[2 + packetInformations.index] = characterInformations.value;
		packetInformations.index++;
		i++;

		if (packetInformations.index >= 6) // we have maximum of 6 characters per packet, index starts at 0 (so max is 5)
		{
			int res = queuePacket(periphClavier, buffer, 0, tempsTraitementParPaquetMicroSecondes);

			if (res < 0)
				return -1;

			initPacketInformations(&packetInformations);
		}
	}

	if (packetInformations.initialized) {
		int res = queuePacket(periphClavier, buffer, false, tempsTraitementParPaquetMicroSecondes);

		if (res < 0)
			return -1;
	}
	return (int)len;
}
