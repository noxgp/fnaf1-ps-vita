#ifndef FNAF_MENU_UI_H
#define FNAF_MENU_UI_H
enum { MENU_TITLE, MENU_NOT_READY, MENU_OPTIONS, MENU_UNLOCKS };
typedef struct { int panel, selection, option, music; } MenuUI;
void menu_ui_reset(MenuUI *ui);
void menu_ui_move(MenuUI *ui, int direction);
void menu_ui_activate(MenuUI *ui);
void menu_ui_back(MenuUI *ui);
int menu_ui_object(const MenuUI *ui);
#endif
