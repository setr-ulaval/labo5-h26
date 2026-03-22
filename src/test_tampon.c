#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "tamponCirculaire.h"

static struct requete creerRequete(const char* data) {
    struct requete r;
    r.tempsReception = get_time();
    r.taille = strlen(data);
    r.data = strdup(data);
    return r;
}

static void test_init() {
    assert(initTamponCirculaire(5) == 0);
    assert(longueurFile() == 0);
    printf("  [OK] test_init\n");
}

static void test_inserer_et_consommer_un() {
    initTamponCirculaire(5);
    struct requete r = creerRequete("abc");
    assert(insererDonnee(&r) == 0);
    assert(longueurFile() == 1);

    struct requete out;
    assert(consommerDonnee(&out) == 1);
    assert(out.taille == 3);
    assert(memcmp(out.data, "abc", 3) == 0);
    assert(longueurFile() == 0);
    free(out.data);
    printf("  [OK] test_inserer_et_consommer_un\n");
}

static void test_consommer_vide() {
    initTamponCirculaire(5);
    struct requete out;
    assert(consommerDonnee(&out) == 0);
    printf("  [OK] test_consommer_vide\n");
}

static void test_fifo_order() {
    initTamponCirculaire(5);
    struct requete r1 = creerRequete("first");
    struct requete r2 = creerRequete("second");
    insererDonnee(&r1);
    insererDonnee(&r2);
    assert(longueurFile() == 2);

    struct requete out;
    consommerDonnee(&out);
    assert(memcmp(out.data, "first", 5) == 0);
    free(out.data);

    consommerDonnee(&out);
    assert(memcmp(out.data, "second", 6) == 0);
    free(out.data);
    printf("  [OK] test_fifo_order\n");
}

static void test_wrap_around() {
    initTamponCirculaire(3);
    struct requete r, out;

    r = creerRequete("A"); insererDonnee(&r);
    r = creerRequete("B"); insererDonnee(&r);
    r = creerRequete("C"); insererDonnee(&r);
    assert(longueurFile() == 3);

    consommerDonnee(&out);
    assert(memcmp(out.data, "A", 1) == 0);
    free(out.data);

    r = creerRequete("D"); insererDonnee(&r);
    assert(longueurFile() == 3);

    consommerDonnee(&out); assert(memcmp(out.data, "B", 1) == 0); free(out.data);
    consommerDonnee(&out); assert(memcmp(out.data, "C", 1) == 0); free(out.data);
    consommerDonnee(&out); assert(memcmp(out.data, "D", 1) == 0); free(out.data);
    printf("  [OK] test_wrap_around\n");
}

static void test_overflow_ecrase_ancien() {
    initTamponCirculaire(2);
    struct requete r, out;

    r = creerRequete("X"); insererDonnee(&r);
    r = creerRequete("Y"); insererDonnee(&r);
    r = creerRequete("Z"); insererDonnee(&r);
    assert(longueurFile() == 2);

    consommerDonnee(&out); assert(memcmp(out.data, "Y", 1) == 0); free(out.data);
    consommerDonnee(&out); assert(memcmp(out.data, "Z", 1) == 0); free(out.data);
    printf("  [OK] test_overflow_ecrase_ancien\n");
}

int main() {
    printf("=== Tests tampon circulaire ===\n");
    test_init();
    test_inserer_et_consommer_un();
    test_consommer_vide();
    test_fifo_order();
    test_wrap_around();
    test_overflow_ecrase_ancien();
    printf("\nTous les tests passent!\n");
    return 0;
}
