// TYPES.H
// DÉFINIT LES TYPES PARTICULIERS UTILISÉS PAR L'APPLICATION

#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

// Définition de types de variables personnalisées permettant la simplification du code

// Attention à la juste définition des types des variables car à l'éxécution, entre écriture
// au format texte et réencodage en binaire, certaines variables se sont trouvées frappées de
// débordement et ne correspondaient plus à leur valeur initiale
// (cf. commentaires dans FileConverteur.cpp)


    typedef std::int8_t     sint8;      // valeurs possibles [-128 , 127]
    typedef std::uint8_t    uint8;      // valeurs possibles [   0 , 255]

    typedef std::int16_t    sint16;     // valeurs possibles [-32 768 , 32 767]
    typedef std::uint16_t   uint16;     // valeurs possibles [      0 , 65 535]

    typedef std::int32_t    sint32;     // valeurs possibles [-2 147 483 647 , 2 147 483 647]
    typedef std::uint32_t   uint32;     // valeurs possibles [             0 , 4 294 967 295]

    typedef std::int64_t    sint64;     // valeurs possibles // pas de problème de débordement
    typedef std::uint64_t   uint64;     // valeurs possibles // pas de problème de débordement



#endif // TYPES_H
