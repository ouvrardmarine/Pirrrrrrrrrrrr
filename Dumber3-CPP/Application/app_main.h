/*
 * app_main.h
 *
 *  Created on: 11 avr. 2026
 *      Author: leole
 */

#ifndef APPLICATION_APP_MAIN_H_
#define APPLICATION_APP_MAIN_H_

/* * Ce bloc est CRUCIAL. Il dit au compilateur :
 * "Si tu es en train de compiler du C++, traite ces fonctions
 * avec les règles de nommage du C".
 */
#ifdef __cplusplus
extern "C" {
#endif

/* * Déclare ici les fonctions que tu veux appeler depuis le main.c.
 * On utilise généralement deux fonctions : une pour l'init, une pour la boucle.
 */

void APPLICATION_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_APP_MAIN_H_ */
