#include "menu_ui.h"
void menu_ui_reset(MenuUI *ui) { ui->panel = MENU_TITLE; ui->selection = 0; ui->option = 0; ui->music = 1; }
void menu_ui_move(MenuUI *ui, int direction) {
    if (ui->panel == MENU_TITLE) ui->selection = (ui->selection + (direction < 0 ? 3 : 1)) % 4;
    if (ui->panel == MENU_OPTIONS) ui->option = 1 - ui->option;
}
void menu_ui_activate(MenuUI *ui) {
    if (ui->panel == MENU_TITLE) {
        ui->panel = ui->selection == 2 ? MENU_OPTIONS : ui->selection == 3 ? MENU_UNLOCKS : MENU_NOT_READY;
        ui->option = 0;
    } else if (ui->panel == MENU_OPTIONS) {
        if (ui->option == 0) ui->music = !ui->music;
        else ui->panel = MENU_TITLE;
    } else ui->panel = MENU_TITLE;
}
void menu_ui_back(MenuUI *ui) { ui->panel = MENU_TITLE; }
int menu_ui_object(const MenuUI *ui) { const int handles[] = {12,13,34,33}; return handles[ui->selection & 3]; }
