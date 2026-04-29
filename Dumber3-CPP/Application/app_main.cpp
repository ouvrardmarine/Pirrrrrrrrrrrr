/*
 * app_main.cpp
 *
 *  Created on: 11 avr. 2026
 *      Author: leole
 */

#include "app_main.h"  // Crée ce petit header aussi
#include "main.h"      // Pour accéder au HAL

// Ici tu peux utiliser tes classes C++
#include "application.h"

extern "C" void APPLICATION_Start(void) {
    // Ce code est écrit en C++ mais appelable par le main.c
    //Application app;
    //app.Init();

	/*
	 * Appel du singleton pour creer l'objet application
	 * ATTENTION: L'application est créée sur la stack, de facon permanente (static).
	 * Prevoir assez de stack
	 */
	Application::Instance().Init();
}
