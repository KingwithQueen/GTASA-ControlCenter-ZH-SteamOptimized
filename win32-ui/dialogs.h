#ifndef GTASA_UI_DIALOGS_H
#define GTASA_UI_DIALOGS_H

#include "ui.h"

enum {
    UI_VEHICLE_COLORS = 127,
    UI_VEHICLE_MOD_SLOTS = 17,
    UI_WEAPON_SKILLS = 10
};

LPCWSTR UiVehicleName(int modelId);
COLORREF UiVehicleColor(int index);
BOOL UiVehicleDefaultColors(int modelId, int *primary, int *secondary);
BOOL UiChooseVehicle(HWND owner, int *modelId, WCHAR *name, int nameCapacity);
BOOL UiChooseVehicleColors(HWND owner, int *primary, int *secondary);
BOOL UiPickVehicleColors(HWND owner, int *primary, int *secondary, BOOL chooseSecondary);
BOOL UiEditVehicleMods(HWND owner, int modelId, int slots[UI_VEHICLE_MOD_SLOTS]);
BOOL UiEditWeaponProficiency(HWND owner, int levels[UI_WEAPON_SKILLS]);

#endif
