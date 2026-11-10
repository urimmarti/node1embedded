#ifndef MENU_H
#define MENU_H

#include <stdint.h>

// Definiciones de bits para NAV (según la tabla del comando 0x04)[cite: 2]
#define NAV_UP    (1 << 4)
#define NAV_DOWN  (1 << 3)
#define NAV_BTN   (1 << 0)

// Bits para el byte RIGHT (para detectar SR6/R6)[cite: 2]
#define BTN_R6    (1 << 5)

void menu_init(void);
uint8_t menu_read_nav_raw(void);
void menu_update_display(void);

// Función centralizada que maneja la lógica de pantallas según el input
void handle_nav_and_game(uint8_t nav_val, uint8_t goal_changed, uint8_t current_goals);

// Control de estados y best score
int8_t is_in_game(void);
int8_t get_best_score(void);

#endif