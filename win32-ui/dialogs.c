#include "dialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum {
    MODEL_FIRST = 400,
    MODEL_LAST = 611,
    VEHICLE_NAME_LENGTH = 64,
    MOD_PARTS = 192,
    INVALID_COLOR = -1
};

typedef struct ResourceReader {
    const char *next;
    const char *end;
} ResourceReader;

typedef struct Vehicle {
    WCHAR name[VEHICLE_NAME_LENGTH];
    WCHAR type[24];
    WCHAR detail[590];
    unsigned categories;
    int rating;
    int primaryColor;
    int secondaryColor;
    BOOL parkable;
    BOOL favorite;
    char mods[320];
} Vehicle;

typedef struct Color {
    COLORREF rgb;
    WCHAR name[68];
    BOOL available;
} Color;

typedef struct ModPart {
    int id;
    int group;
    WCHAR name[68];
} ModPart;

typedef struct DialogState DialogState;
typedef BOOL (*DialogHandler)(DialogState *, UINT, WPARAM, LPARAM);

struct DialogState {
    HWND dialog;
    HFONT font;
    int dpi;
    int width;
    int height;
    DialogHandler handle;
};

typedef union DialogTemplate {
    DWORD alignment;
    struct {
        DLGTEMPLATE header;
        WORD menu;
        WORD windowClass;
        WCHAR title[80];
    } content;
} DialogTemplate;

static Vehicle catalog[MODEL_LAST - MODEL_FIRST + 1];
static Color palette[UI_VEHICLE_COLORS];
static ModPart modParts[MOD_PARTS];
static int modCount;
static BOOL loadedCatalog;
static BOOL loadedCategories;
static BOOL loadedMods;

static const LPCWSTR categoryLabels[] = {
    L"全部", L"收藏", L"跑车", L"越野", L"低底盘车", L"敞篷车",
    L"其他汽车", L"工业车辆", L"公务车辆", L"直升机", L"飞机",
    L"摩托车和自行车", L"船舶", L"特殊车辆"
};

static BOOL OpenReader(ResourceReader *reader, int resourceId)
{
    DWORD size = 0;
    reader->next = UiResourceBytes(resourceId, &size);
    reader->end = reader->next ? reader->next + size : NULL;
    return reader->next != NULL && size != 0;
}

static BOOL ReadLine(ResourceReader *reader, char *line, size_t capacity)
{
    size_t length = 0;
    if (reader->next == reader->end)
        return FALSE;
    while (reader->next != reader->end && *reader->next != '\n') {
        if (*reader->next != '\r' && length + 1 < capacity)
            line[length++] = *reader->next;
        ++reader->next;
    }
    if (reader->next != reader->end)
        ++reader->next;
    line[length] = '\0';
    return TRUE;
}

static char *Trim(char *text)
{
    char *end;
    while (*text == ' ' || *text == '\t')
        ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t'))
        --end;
    *end = '\0';
    return text;
}

static int SplitFields(char *text, char separator, char **fields, int capacity)
{
    int count = 0;
    while (count < capacity) {
        char *next = strchr(text, separator);
        if (next)
            *next = '\0';
        fields[count++] = Trim(text);
        if (!next)
            break;
        text = next + 1;
    }
    return count;
}

static void Widen(WCHAR *output, int capacity, const char *input)
{
    int length = MultiByteToWideChar(1252, 0, input, -1, NULL, 0);
    if (length <= 0) {
        output[0] = L'\0';
    } else if (length > capacity) {
        MultiByteToWideChar(1252, 0, input, capacity - 1, output, capacity - 1);
        output[capacity - 1] = L'\0';
    } else {
        MultiByteToWideChar(1252, 0, input, -1, output, capacity);
    }
}

static Vehicle *FindVehicle(int modelId)
{
    if (modelId < MODEL_FIRST || modelId > MODEL_LAST || !catalog[modelId - MODEL_FIRST].name[0])
        return NULL;
    return &catalog[modelId - MODEL_FIRST];
}

static void ReadVehicle(char *line)
{
    char *fields[11];
    int count = SplitFields(line, ',', fields, ARRAYSIZE(fields));
    int model = atoi(fields[0]);
    Vehicle *vehicle;
    if (count < 10 || model < MODEL_FIRST || model > MODEL_LAST)
        return;
    vehicle = &catalog[model - MODEL_FIRST];
    Widen(vehicle->name, ARRAYSIZE(vehicle->name), fields[1]);
    Widen(vehicle->type, ARRAYSIZE(vehicle->type), fields[2]);
    vehicle->parkable = atoi(fields[7]) != 0;
    vehicle->secondaryColor = atoi(fields[8]);
    vehicle->primaryColor = atoi(fields[9]);
    if (count == 11)
        strcpy_s(vehicle->mods, sizeof(vehicle->mods), fields[10]);
}

static void ReadColor(char *line)
{
    int blue, green, red;
    long colorId;
    char *name, *tab;
    char *hash = strchr(line, '#');
    if (!hash || sscanf_s(line, "%d,%d,%d", &red, &green, &blue) != 3)
        return;
    colorId = strtol(hash + 1, &name, 10);
    if (colorId < 0 || colorId >= UI_VEHICLE_COLORS)
        return;
    name = Trim(name);
    tab = strchr(name, '\t');
    if (tab)
        *tab = '\0';
    palette[colorId].rgb = RGB(red, green, blue);
    Widen(palette[colorId].name, ARRAYSIZE(palette[colorId].name), Trim(name));
    palette[colorId].available = TRUE;
}

static void LoadCatalog(void)
{
    ResourceReader reader;
    char line[768];
    BOOL vehicles = FALSE, colors = FALSE;

    if (loadedCatalog)
        return;
    loadedCatalog = TRUE;
    if (!OpenReader(&reader, ID_DATA_VEHICLES))
        return;
    while (ReadLine(&reader, line, sizeof(line))) {
        if (strcmp(line, "GTASACars") == 0)
            vehicles = TRUE;
        else if (strcmp(line, "GTASAColors") == 0)
            colors = TRUE;
        else if (strncmp(line, "END_", 4) == 0)
            vehicles = colors = FALSE;
        else if (line[0] >= '0' && line[0] <= '9') {
            if (vehicles)
                ReadVehicle(line);
            else if (colors)
                ReadColor(line);
        }
    }
}

static void LoadCategories(void)
{
    ResourceReader reader;
    char line[768], *fields[5];
    BOOL rows = FALSE;
    int category;
    Vehicle *vehicle;

    LoadCatalog();
    if (loadedCategories)
        return;
    loadedCategories = TRUE;
    if (!OpenReader(&reader, ID_DATA_VEHICLE_CATEGORIES))
        return;
    while (ReadLine(&reader, line, sizeof(line))) {
        if (strcmp(line, "GTASACarPictures") == 0)
            rows = TRUE;
        else if (strcmp(line, "END_GTASACarPictures") == 0)
            break;
        else if (rows && SplitFields(line, '|', fields, ARRAYSIZE(fields)) == ARRAYSIZE(fields)) {
            vehicle = FindVehicle(atoi(fields[0]));
            category = atoi(fields[1]);
            if (!vehicle || category < 0 || category > 12)
                continue;
            if (category == 0)
                vehicle->favorite = TRUE;
            else
                vehicle->categories |= 1u << category;
            if (!vehicle->detail[0]) {
                vehicle->rating = atoi(fields[3]);
                Widen(vehicle->detail, ARRAYSIZE(vehicle->detail), fields[4]);
            }
        }
    }
}

LPCWSTR UiVehicleName(int modelId)
{
    Vehicle *vehicle;
    LoadCatalog();
    vehicle = FindVehicle(modelId);
    if (vehicle)
        return UiTranslate(vehicle->name);
    return modelId == 0 ? L"空车位" : L"未知载具";
}

COLORREF UiVehicleColor(int index)
{
    LoadCatalog();
    if (index >= 0 && index < UI_VEHICLE_COLORS && palette[index].available)
        return palette[index].rgb;
    return GetSysColor(COLOR_BTNFACE);
}

BOOL UiVehicleDefaultColors(int modelId, int *primary, int *secondary)
{
    Vehicle *vehicle;
    LoadCatalog();
    vehicle = FindVehicle(modelId);
    if (!vehicle)
        return FALSE;
    *primary = vehicle->primaryColor;
    *secondary = vehicle->secondaryColor;
    return TRUE;
}

static int DialogScale(const DialogState *state, int value)
{
    return MulDiv(value, state->dpi, 96);
}

static HWND Control(DialogState *state, LPCWSTR className, LPCWSTR text, DWORD style,
                    DWORD exStyle, int id, int x, int y, int width, int height)
{
    HWND result = CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
                                  DialogScale(state, x), DialogScale(state, y),
                                  DialogScale(state, width), DialogScale(state, height),
                                  state->dialog, (HMENU)(INT_PTR)id,
                                  GetModuleHandleW(NULL), NULL);
    SendMessageW(result, WM_SETFONT, (WPARAM)state->font, FALSE);
    return result;
}

static void AddConfirmButtons(DialogState *state)
{
    Control(state, WC_BUTTONW, L"确定", WS_TABSTOP | BS_DEFPUSHBUTTON, 0, IDOK,
            state->width - 184, state->height - 42, 76, 27);
    Control(state, WC_BUTTONW, L"取消", WS_TABSTOP | BS_PUSHBUTTON, 0, IDCANCEL,
            state->width - 96, state->height - 42, 76, 27);
    SendMessageW(state->dialog, DM_SETDEFID, IDOK, 0);
}

static void PositionDialog(DialogState *state)
{
    HWND owner = GetWindow(state->dialog, GW_OWNER);
    HMONITOR monitor = MonitorFromWindow(owner ? owner : state->dialog, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = { 0 };
    RECT window, parent, workArea;
    HDC screen = GetDC(state->dialog);
    int x, y, width, height;

    state->dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen)
        ReleaseDC(state->dialog, screen);
    state->font = CreateFontW(-MulDiv(9, state->dpi, 72), 0, 0, 0, FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Tahoma");
    SetRect(&window, 0, 0, DialogScale(state, state->width), DialogScale(state, state->height));
    AdjustWindowRectEx(&window, (DWORD)GetWindowLongW(state->dialog, GWL_STYLE), FALSE,
                       (DWORD)GetWindowLongW(state->dialog, GWL_EXSTYLE));
    width = window.right - window.left;
    height = window.bottom - window.top;
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfoW(monitor, &monitorInfo))
        workArea = monitorInfo.rcWork;
    else
        SetRect(&workArea, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    if (!owner || !GetWindowRect(owner, &parent))
        parent = workArea;
    x = (parent.left + parent.right - width) / 2;
    y = (parent.top + parent.bottom - height) / 2;
    if (x + width > workArea.right)
        x = workArea.right - width;
    if (y + height > workArea.bottom)
        y = workArea.bottom - height;
    SetWindowPos(state->dialog, NULL, max(workArea.left, x), max(workArea.top, y),
                 width, height, SWP_NOACTIVATE | SWP_NOZORDER);
}

static INT_PTR CALLBACK DialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    DialogState *state;
    if (message == WM_INITDIALOG) {
        state = (DialogState *)lParam;
        state->dialog = window;
        SetWindowLongPtrW(window, DWLP_USER, (LONG_PTR)state);
        PositionDialog(state);
        state->handle(state, message, wParam, lParam);
        return TRUE;
    }
    state = (DialogState *)GetWindowLongPtrW(window, DWLP_USER);
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wParam) == IDCANCEL)) {
        EndDialog(window, IDCANCEL);
        return TRUE;
    }
    if (state && state->handle(state, message, wParam, lParam))
        return TRUE;
    if (message == WM_DESTROY && state && state->font)
        DeleteObject(state->font);
    return FALSE;
}

static BOOL ShowDialog(HWND owner, LPCWSTR title, DialogState *state)
{
    DialogTemplate contents = { 0 };
    contents.content.header.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    contents.content.header.cx = 120;
    contents.content.header.cy = 100;
    lstrcpynW(contents.content.title, title, ARRAYSIZE(contents.content.title));
    return DialogBoxIndirectParamW(GetModuleHandleW(NULL), &contents.content.header,
                                  GetAncestor(owner, GA_ROOT), DialogProc, (LPARAM)state) == IDOK;
}

enum {
    VEHICLE_GROUP = 100,
    VEHICLE_QUERY,
    VEHICLE_LIST,
    VEHICLE_DETAIL,
    VEHICLE_FAVORITE,
    VEHICLE_RATING,
    PRIMARY_TARGET = 200,
    SECONDARY_TARGET,
    PRIMARY_PREVIEW,
    SECONDARY_PREVIEW,
    COLOR_DETAIL,
    SWATCH_FIRST = 300,
    MODS_LIST = 500,
    MODS_PAINT,
    MODS_CLEAR,
    WEAPON_SLIDER_FIRST = 600,
    WEAPON_VALUE_FIRST = 620,
    WEAPON_MAX = 640
};

typedef struct VehicleDialog {
    DialogState base;
    HWND category;
    HWND query;
    HWND list;
    HWND detail;
    HWND rating;
    HWND favorite;
    int model;
} VehicleDialog;

static void AddColumn(DialogState *state, HWND list, int index, LPCWSTR label, int width)
{
    LVCOLUMNW column = { 0 };
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.cx = DialogScale(state, width);
    column.iSubItem = index;
    column.pszText = (LPWSTR)label;
    SendMessageW(list, LVM_INSERTCOLUMNW, index, (LPARAM)&column);
}

static BOOL ContainsText(LPCWSTR text, LPCWSTR pattern)
{
    size_t length = wcslen(pattern);
    if (length == 0)
        return TRUE;
    for (; *text; ++text) {
        if (_wcsnicmp(text, pattern, length) == 0)
            return TRUE;
    }
    return FALSE;
}

static int SelectedVehicle(const VehicleDialog *state)
{
    LVITEMW item = { 0 };
    item.mask = LVIF_PARAM;
    item.iItem = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
    if (item.iItem == -1 || !SendMessageW(state->list, LVM_GETITEMW, 0, (LPARAM)&item))
        return -1;
    return (int)item.lParam;
}

static int PrimaryCategory(const Vehicle *vehicle)
{
    int category;
    if (!vehicle)
        return 0;
    for (category = 1; category <= 12; ++category) {
        if (vehicle->categories & (1u << category))
            return category + 1;
    }
    if (wcscmp(vehicle->type, L"plane") == 0)
        return 10;
    if (wcscmp(vehicle->type, L"heli") == 0)
        return 9;
    if (wcscmp(vehicle->type, L"boat") == 0)
        return 12;
    if (wcscmp(vehicle->type, L"bike") == 0 || wcscmp(vehicle->type, L"bicycle") == 0)
        return 11;
    return wcscmp(vehicle->type, L"car") == 0 ? 6 : 13;
}

static BOOL MatchesCategory(const Vehicle *vehicle, int category)
{
    if (category <= 0)
        return TRUE;
    if (!vehicle)
        return FALSE;
    if (category == 1)
        return vehicle->favorite;
    return vehicle->categories & (1u << (category - 1)) ||
           PrimaryCategory(vehicle) == category;
}

static void UpdateVehicleDetail(VehicleDialog *state)
{
    int selected = SelectedVehicle(state);
    Vehicle *vehicle = FindVehicle(selected);
    WCHAR heading[124];
    WCHAR detail[610];

    EnableWindow(GetDlgItem(state->base.dialog, IDOK), selected >= 0);
    EnableWindow(state->favorite, vehicle != NULL);
    SetWindowTextW(state->favorite, vehicle && vehicle->favorite ? L"移出收藏" : L"加入收藏");
    if (vehicle) {
        swprintf_s(heading, ARRAYSIZE(heading), L"%s  ·  %s  ·  %s",
                   UiVehicleName(selected), categoryLabels[PrimaryCategory(vehicle)],
                   vehicle->parkable ? L"可停放" : L"原版未标记为可停放");
        if (vehicle->rating >= 1 && vehicle->rating <= 5)
            swprintf_s(detail, ARRAYSIZE(detail), L"评级 %d / 5\r\n\r\n%s",
                       vehicle->rating, UiTranslate(vehicle->detail));
        else
            lstrcpynW(detail, UiTranslate(vehicle->detail), ARRAYSIZE(detail));
    } else {
        lstrcpynW(heading, selected == 0 ? L"空车位" : L"", ARRAYSIZE(heading));
        detail[0] = L'\0';
    }
    SetWindowTextW(state->rating, heading);
    SetWindowTextW(state->detail, detail);
}

static void RefreshVehicleList(VehicleDialog *state, int model)
{
    static const WCHAR blank[] = L"";
    WCHAR query[VEHICLE_NAME_LENGTH], id[14];
    LVITEMW item = { 0 };
    LRESULT categoryValue = SendMessageW(state->category, CB_GETCURSEL, 0, 0);
    int category = categoryValue == CB_ERR ? 0 : (int)categoryValue;
    int key, row, selectedRow = -1;
    Vehicle *vehicle;

    GetWindowTextW(state->query, query, ARRAYSIZE(query));
    SendMessageW(state->list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(state->list);
    for (key = 0; key <= MODEL_LAST; key = key == 0 ? MODEL_FIRST : key + 1) {
        vehicle = FindVehicle(key);
        if ((key != 0 && !vehicle) || !MatchesCategory(vehicle, category))
            continue;
        swprintf_s(id, ARRAYSIZE(id), L"%d", key);
        if (!ContainsText(UiVehicleName(key), query) && !ContainsText(id, query))
            continue;
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(state->list);
        item.iSubItem = 0;
        item.pszText = (LPWSTR)UiVehicleName(key);
        item.lParam = key;
        row = (int)SendMessageW(state->list, LVM_INSERTITEMW, 0, (LPARAM)&item);
        if (row == -1)
            continue;
        item.iSubItem = 1;
        item.pszText = key == 0 ? (LPWSTR)blank : id;
        SendMessageW(state->list, LVM_SETITEMTEXTW, row, (LPARAM)&item);
        item.iSubItem = 2;
        item.pszText = vehicle ? (LPWSTR)categoryLabels[PrimaryCategory(vehicle)] : (LPWSTR)blank;
        SendMessageW(state->list, LVM_SETITEMTEXTW, row, (LPARAM)&item);
        if (key == model)
            selectedRow = row;
    }
    if (selectedRow == -1 && ListView_GetItemCount(state->list) > 0)
        selectedRow = 0;
    if (selectedRow >= 0) {
        ListView_SetItemState(state->list, selectedRow, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(state->list, selectedRow, FALSE);
    }
    SendMessageW(state->list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(state->list, NULL, TRUE);
    UpdateVehicleDetail(state);
}

static void CreateVehicleDialog(VehicleDialog *state)
{
    DialogState *base = &state->base;
    int index;
    Control(base, WC_STATICW, L"分类", SS_CENTERIMAGE, 0, 0, 16, 16, 36, 24);
    state->category = Control(base, WC_COMBOBOXW, L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                              0, VEHICLE_GROUP, 58, 16, 124, 230);
    for (index = 0; index < ARRAYSIZE(categoryLabels); ++index)
        SendMessageW(state->category, CB_ADDSTRING, 0, (LPARAM)categoryLabels[index]);
    SendMessageW(state->category, CB_SETCURSEL, 0, 0);
    Control(base, WC_STATICW, L"搜索车名或编号", SS_CENTERIMAGE, 0, 0, 205, 16, 93, 24);
    state->query = Control(base, WC_EDITW, L"", ES_AUTOHSCROLL | WS_TABSTOP,
                           WS_EX_CLIENTEDGE, VEHICLE_QUERY, 306, 16, 248, 24);
    state->list = Control(base, WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                          LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, VEHICLE_LIST, 16, 55, 538, 262);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    AddColumn(base, state->list, 0, L"载具名称", 283);
    AddColumn(base, state->list, 1, L"编号", 66);
    AddColumn(base, state->list, 2, L"分类", 149);
    state->rating = Control(base, WC_STATICW, L"", SS_CENTERIMAGE, 0,
                            VEHICLE_RATING, 16, 325, 538, 24);
    state->detail = Control(base, WC_EDITW, L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                            WS_VSCROLL, WS_EX_CLIENTEDGE, VEHICLE_DETAIL, 16, 352, 538, 60);
    state->favorite = Control(base, WC_BUTTONW, L"加入收藏", WS_TABSTOP | BS_PUSHBUTTON, 0,
                              VEHICLE_FAVORITE, 16, base->height - 42, 112, 27);
    AddConfirmButtons(base);
    RefreshVehicleList(state, state->model);
}

static BOOL HandleVehicleDialog(DialogState *base, UINT message, WPARAM wParam, LPARAM lParam)
{
    VehicleDialog *state = (VehicleDialog *)base;
    Vehicle *vehicle;
    int selected;
    if (message == WM_INITDIALOG) {
        CreateVehicleDialog(state);
        return TRUE;
    }
    if (message == WM_NOTIFY && ((NMHDR *)lParam)->hwndFrom == state->list) {
        NMHDR *notify = (NMHDR *)lParam;
        if (notify->code == LVN_ITEMCHANGED &&
            (((NMLISTVIEW *)notify)->uOldState ^ ((NMLISTVIEW *)notify)->uNewState) & LVIS_SELECTED)
            UpdateVehicleDetail(state);
        else if (notify->code == NM_DBLCLK && SelectedVehicle(state) >= 0)
            SendMessageW(base->dialog, WM_COMMAND, IDOK, 0);
        return TRUE;
    }
    if (message != WM_COMMAND)
        return FALSE;
    selected = SelectedVehicle(state);
    switch (LOWORD(wParam)) {
    case IDOK:
        if (selected >= 0) {
            state->model = selected;
            EndDialog(base->dialog, IDOK);
        }
        return TRUE;
    case VEHICLE_FAVORITE:
        vehicle = FindVehicle(selected);
        if (vehicle) {
            vehicle->favorite = !vehicle->favorite;
            if (SendMessageW(state->category, CB_GETCURSEL, 0, 0) == 1)
                RefreshVehicleList(state, selected);
            else
                UpdateVehicleDetail(state);
        }
        return TRUE;
    case VEHICLE_GROUP:
        if (HIWORD(wParam) == CBN_SELCHANGE)
            RefreshVehicleList(state, selected);
        return TRUE;
    case VEHICLE_QUERY:
        if (HIWORD(wParam) == EN_CHANGE)
            RefreshVehicleList(state, selected);
        return TRUE;
    }
    return FALSE;
}

BOOL UiChooseVehicle(HWND owner, int *modelId, WCHAR *name, int nameCapacity)
{
    VehicleDialog state = { 0 };
    LoadCategories();
    state.base.handle = HandleVehicleDialog;
    state.base.width = 570;
    state.base.height = 465;
    state.model = *modelId;
    if (!ShowDialog(owner, L"控制中心 · 选择载具", &state.base))
        return FALSE;
    *modelId = state.model;
    if (name && nameCapacity > 0)
        lstrcpynW(name, UiVehicleName(state.model), nameCapacity);
    return TRUE;
}

typedef struct ColorDialog {
    DialogState base;
    int colors[2];
    int target;
} ColorDialog;

static int ValidColor(int color)
{
    return color >= 0 && color < UI_VEHICLE_COLORS && palette[color].available ? color : 0;
}

static void RefreshColors(ColorDialog *state, int formerColor)
{
    HWND dialog = state->base.dialog;
    int index = state->colors[state->target];
    WCHAR information[136];
    COLORREF rgb = palette[index].rgb;
    CheckRadioButton(dialog, PRIMARY_TARGET, SECONDARY_TARGET, PRIMARY_TARGET + state->target);
    swprintf_s(information, ARRAYSIZE(information), L"%s %03d  %s  ·  RGB %u, %u, %u",
               state->target ? L"副色" : L"主色", index, UiTranslate(palette[index].name),
               (unsigned)GetRValue(rgb), (unsigned)GetGValue(rgb), (unsigned)GetBValue(rgb));
    SetDlgItemTextW(dialog, COLOR_DETAIL, information);
    InvalidateRect(GetDlgItem(dialog, PRIMARY_PREVIEW), NULL, FALSE);
    InvalidateRect(GetDlgItem(dialog, SECONDARY_PREVIEW), NULL, FALSE);
    if (formerColor >= 0)
        InvalidateRect(GetDlgItem(dialog, SWATCH_FIRST + formerColor), NULL, FALSE);
    InvalidateRect(GetDlgItem(dialog, SWATCH_FIRST + index), NULL, FALSE);
}

static void CreateColorDialog(ColorDialog *state)
{
    DialogState *base = &state->base;
    int index;
    WCHAR number[12];
    Control(base, WC_BUTTONW, L"设置主色", WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON,
            0, PRIMARY_TARGET, 16, 14, 82, 26);
    Control(base, WC_BUTTONW, L"设置副色", WS_TABSTOP | BS_AUTORADIOBUTTON,
            0, SECONDARY_TARGET, 177, 14, 82, 26);
    Control(base, WC_STATICW, L"", WS_GROUP | SS_OWNERDRAW,
            0, PRIMARY_PREVIEW, 105, 14, 48, 26);
    Control(base, WC_STATICW, L"", SS_OWNERDRAW,
            0, SECONDARY_PREVIEW, 266, 14, 48, 26);
    Control(base, WC_STATICW, L"", SS_CENTERIMAGE,
            0, COLOR_DETAIL, 16, 47, 560, 23);
    for (index = 0; index < UI_VEHICLE_COLORS; ++index) {
        if (!palette[index].available)
            continue;
        swprintf_s(number, ARRAYSIZE(number), L"%03d", index);
        Control(base, WC_BUTTONW, number, WS_TABSTOP | BS_OWNERDRAW,
                0, SWATCH_FIRST + index, 16 + index % 16 * 35,
                80 + index / 16 * 30, 33, 28);
    }
    AddConfirmButtons(base);
    RefreshColors(state, INVALID_COLOR);
}

static BOOL PaintColor(const ColorDialog *state, const DRAWITEMSTRUCT *draw)
{
    RECT box = draw->rcItem;
    int index, brightness, saved;
    BOOL active;
    COLORREF rgb;
    HBRUSH fill;
    WCHAR number[12];

    if (draw->CtlID == PRIMARY_PREVIEW || draw->CtlID == SECONDARY_PREVIEW) {
        index = state->colors[draw->CtlID == SECONDARY_PREVIEW];
        active = FALSE;
    } else if (draw->CtlID >= SWATCH_FIRST && draw->CtlID < SWATCH_FIRST + UI_VEHICLE_COLORS) {
        index = (int)draw->CtlID - SWATCH_FIRST;
        active = index == state->colors[state->target];
    } else {
        return FALSE;
    }
    rgb = palette[index].rgb;
    fill = CreateSolidBrush(rgb);
    FillRect(draw->hDC, &box, fill ? fill : GetSysColorBrush(COLOR_BTNFACE));
    if (fill)
        DeleteObject(fill);
    FrameRect(draw->hDC, &box, GetSysColorBrush(active ? COLOR_HIGHLIGHT : COLOR_3DSHADOW));
    if (active) {
        InflateRect(&box, -1, -1);
        FrameRect(draw->hDC, &box, GetSysColorBrush(COLOR_HIGHLIGHT));
    }
    brightness = GetRValue(rgb) * 299 + GetGValue(rgb) * 587 + GetBValue(rgb) * 114;
    saved = SaveDC(draw->hDC);
    SetTextColor(draw->hDC, brightness > 146000 ? RGB(0, 0, 0) : RGB(255, 255, 255));
    SetBkMode(draw->hDC, TRANSPARENT);
    if (state->base.font)
        SelectObject(draw->hDC, state->base.font);
    swprintf_s(number, ARRAYSIZE(number), L"%03d", index);
    DrawTextW(draw->hDC, number, -1, &box, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    if (draw->itemState & ODS_FOCUS) {
        InflateRect(&box, -2, -2);
        DrawFocusRect(draw->hDC, &box);
    }
    if (saved)
        RestoreDC(draw->hDC, saved);
    return TRUE;
}

static BOOL HandleColorDialog(DialogState *base, UINT message, WPARAM wParam, LPARAM lParam)
{
    ColorDialog *state = (ColorDialog *)base;
    int command, prior;
    if (message == WM_INITDIALOG) {
        CreateColorDialog(state);
        return TRUE;
    }
    if (message == WM_DRAWITEM)
        return PaintColor(state, (const DRAWITEMSTRUCT *)lParam);
    if (message != WM_COMMAND)
        return FALSE;
    command = LOWORD(wParam);
    if (command == IDOK) {
        EndDialog(base->dialog, IDOK);
        return TRUE;
    }
    prior = state->colors[state->target];
    if (command == PRIMARY_TARGET || command == SECONDARY_TARGET) {
        state->target = command == SECONDARY_TARGET;
    } else if (command >= SWATCH_FIRST && command < SWATCH_FIRST + UI_VEHICLE_COLORS &&
               HIWORD(wParam) == BN_CLICKED) {
        state->colors[state->target] = command - SWATCH_FIRST;
    } else {
        return FALSE;
    }
    RefreshColors(state, prior);
    return TRUE;
}

BOOL UiPickVehicleColors(HWND owner, int *primary, int *secondary, BOOL chooseSecondary)
{
    ColorDialog state = { 0 };
    LoadCatalog();
    state.base.handle = HandleColorDialog;
    state.base.width = 592;
    state.base.height = 370;
    state.colors[0] = ValidColor(*primary);
    state.colors[1] = ValidColor(*secondary);
    state.target = chooseSecondary ? 1 : 0;
    if (!ShowDialog(owner, L"车辆颜色", &state.base))
        return FALSE;
    *primary = state.colors[0];
    *secondary = state.colors[1];
    return TRUE;
}

BOOL UiChooseVehicleColors(HWND owner, int *primary, int *secondary)
{
    return UiPickVehicleColors(owner, primary, secondary, FALSE);
}

typedef struct WeaponDialog {
    DialogState base;
    int levels[UI_WEAPON_SKILLS];
} WeaponDialog;

static const LPCWSTR weaponLabels[UI_WEAPON_SKILLS] = {
    L"手枪", L"消音手枪", L"沙漠之鹰", L"霰弹枪", L"短管霰弹枪",
    L"战斗霰弹枪", L"微型冲锋枪", L"冲锋枪", L"AK-47", L"M4"
};

static void ShowWeaponLevel(WeaponDialog *state, int index)
{
    WCHAR level[16];
    swprintf_s(level, ARRAYSIZE(level), L"%d.%d%%",
               state->levels[index] / 10, state->levels[index] % 10);
    SetDlgItemTextW(state->base.dialog, WEAPON_VALUE_FIRST + index, level);
}

static void CreateWeaponDialog(WeaponDialog *state)
{
    DialogState *base = &state->base;
    HWND slider;
    int index, y;
    for (index = 0; index < UI_WEAPON_SKILLS; ++index) {
        y = 14 + 31 * index;
        Control(base, WC_STATICW, weaponLabels[index], SS_CENTERIMAGE,
                0, 0, 16, y, 114, 25);
        slider = Control(base, TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_NOTICKS,
                         0, WEAPON_SLIDER_FIRST + index, 134, y, 255, 25);
        SendMessageW(slider, TBM_SETRANGEMAX, FALSE, 1000);
        SendMessageW(slider, TBM_SETPOS, TRUE, state->levels[index]);
        Control(base, WC_STATICW, L"", SS_CENTERIMAGE | SS_RIGHT,
                0, WEAPON_VALUE_FIRST + index, 402, y, 57, 25);
        ShowWeaponLevel(state, index);
    }
    Control(base, WC_BUTTONW, L"全部最大", WS_TABSTOP | BS_PUSHBUTTON,
            0, WEAPON_MAX, 16, base->height - 42, 93, 27);
    AddConfirmButtons(base);
}

static BOOL HandleWeaponDialog(DialogState *base, UINT message, WPARAM wParam, LPARAM lParam)
{
    WeaponDialog *state = (WeaponDialog *)base;
    HWND slider;
    int index;
    if (message == WM_INITDIALOG) {
        CreateWeaponDialog(state);
        return TRUE;
    }
    if (message == WM_HSCROLL && lParam) {
        slider = (HWND)lParam;
        index = GetDlgCtrlID(slider) - WEAPON_SLIDER_FIRST;
        if (index >= 0 && index < UI_WEAPON_SKILLS) {
            state->levels[index] = (int)SendMessageW(slider, TBM_GETPOS, 0, 0);
            ShowWeaponLevel(state, index);
            return TRUE;
        }
    } else if (message == WM_COMMAND && LOWORD(wParam) == WEAPON_MAX) {
        for (index = 0; index < UI_WEAPON_SKILLS; ++index) {
            state->levels[index] = 1000;
            SendDlgItemMessageW(base->dialog, WEAPON_SLIDER_FIRST + index, TBM_SETPOS, TRUE, 1000);
            ShowWeaponLevel(state, index);
        }
        return TRUE;
    } else if (message == WM_COMMAND && LOWORD(wParam) == IDOK) {
        EndDialog(base->dialog, IDOK);
        return TRUE;
    }
    return FALSE;
}

BOOL UiEditWeaponProficiency(HWND owner, int levels[UI_WEAPON_SKILLS])
{
    WeaponDialog state = { 0 };
    int index;
    state.base.handle = HandleWeaponDialog;
    state.base.width = 480;
    state.base.height = 369;
    for (index = 0; index < UI_WEAPON_SKILLS; ++index)
        state.levels[index] = max(0, min(1000, levels[index]));
    if (!ShowDialog(owner, L"武器熟练度", &state.base))
        return FALSE;
    CopyMemory(levels, state.levels, sizeof(state.levels));
    return TRUE;
}

typedef struct ModDialog {
    DialogState base;
    const Vehicle *vehicle;
    HWND list;
    HWND paint;
    int values[UI_VEHICLE_MOD_SLOTS];
    BOOL updating;
} ModDialog;

enum ModGroup {
    MOD_NITRO,
    MOD_STEREO,
    MOD_HYDRAULICS,
    MOD_WHEELS,
    MOD_SPOILER,
    MOD_EXHAUST,
    MOD_SIDE,
    MOD_HOOD_VENT,
    MOD_HOOD_SCOOP,
    MOD_ROOF,
    MOD_FOG_LIGHT,
    MOD_FRONT_BUMPER,
    MOD_REAR_BUMPER,
    MOD_FRONT_GUARD,
    MOD_REAR_GUARD,
    MOD_GRILLE,
    MOD_BODY_BAR,
    MOD_BODY_LIGHT,
    MOD_OTHER
};

static const LPCWSTR modGroups[] = {
    L"氮气", L"低音增强", L"液压悬挂", L"轮毂", L"尾翼", L"排气", L"侧裙",
    L"引擎盖通风口", L"引擎盖进气口", L"车顶", L"雾灯", L"前保险杠",
    L"后保险杠", L"前护杠", L"后护杠", L"进气格栅", L"车身饰条", L"车身灯", L"其他"
};

static int ModPartGroup(LPCWSTR name)
{
    static const struct {
        LPCWSTR word;
        int group;
    } keywords[] = {
        { L"Nitro", MOD_NITRO }, { L"Stereo", MOD_STEREO }, { L"Hydraulic", MOD_HYDRAULICS },
        { L"Side", MOD_SIDE }, { L"Wheels", MOD_WHEELS }, { L"Spoiler", MOD_SPOILER },
        { L"Exhaust", MOD_EXHAUST }, { L"Hood Vent", MOD_HOOD_VENT },
        { L"Hood Scoop", MOD_HOOD_SCOOP }, { L"Roof", MOD_ROOF }, { L"Fog", MOD_FOG_LIGHT },
        { L"Front Bumper", MOD_FRONT_BUMPER }, { L"Rear Bumper", MOD_REAR_BUMPER },
        { L"Front Bull", MOD_FRONT_GUARD }, { L"Rear Bull", MOD_REAR_GUARD },
        { L"Grill", MOD_GRILLE }, { L"Bars", MOD_BODY_BAR }, { L"Lights", MOD_BODY_LIGHT }
    };
    int index;
    for (index = 0; index < ARRAYSIZE(keywords); ++index) {
        if (ContainsText(name, keywords[index].word))
            return keywords[index].group;
    }
    return MOD_OTHER;
}

static void LoadMods(void)
{
    ResourceReader reader;
    char line[500], code[6], *field, *name, *end;
    int id;

    if (loadedMods)
        return;
    loadedMods = TRUE;
    if (!OpenReader(&reader, ID_DATA_VEHICLE_MODS))
        return;
    while (modCount < MOD_PARTS && ReadLine(&reader, line, sizeof(line))) {
        if (strncmp(line, "<tr><td>", 8) != 0)
            continue;
        strncpy_s(code, sizeof(code), line + 8, 3);
        id = (int)strtol(code, &end, 16);
        if (end - code != 3 || id < 1000)
            continue;
        field = strstr(line + 8, "</td><td>");
        if (!field)
            continue;
        field = strstr(field + 9, "</td><td>");
        if (!field)
            continue;
        name = field + 9;
        end = strstr(name, "</td>");
        if (!end)
            continue;
        *end = '\0';
        modParts[modCount].id = id;
        Widen(modParts[modCount].name, ARRAYSIZE(modParts[modCount].name), name);
        modParts[modCount].group = ModPartGroup(modParts[modCount].name);
        ++modCount;
    }
}

static BOOL ModAllowed(const char *list, int partId)
{
    char *next;
    while (*list) {
        if (strtol(list, &next, 16) == partId)
            return TRUE;
        if (!*next)
            break;
        list = next + 1;
    }
    return FALSE;
}

static BOOL HasMod(const ModDialog *state, int id)
{
    int index;
    for (index = 1; index < UI_VEHICLE_MOD_SLOTS; ++index) {
        if (state->values[index] == id)
            return TRUE;
    }
    return FALSE;
}

static const ModPart *PartAt(const ModDialog *state, int row)
{
    LVITEMW item = { 0 };
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (row < 0 || !SendMessageW(state->list, LVM_GETITEMW, 0, (LPARAM)&item))
        return NULL;
    return (const ModPart *)item.lParam;
}

static void FillModList(ModDialog *state)
{
    LVITEMW item = { 0 };
    WCHAR id[12];
    int index, row;
    const ModPart *part;
    state->updating = TRUE;
    for (index = 0; index < modCount; ++index) {
        part = &modParts[index];
        if (!ModAllowed(state->vehicle->mods, part->id))
            continue;
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(state->list);
        item.iSubItem = 0;
        item.pszText = (LPWSTR)UiTranslate(part->name);
        item.lParam = (LPARAM)part;
        row = (int)SendMessageW(state->list, LVM_INSERTITEMW, 0, (LPARAM)&item);
        if (row == -1)
            continue;
        item.iSubItem = 1;
        item.pszText = (LPWSTR)modGroups[part->group];
        SendMessageW(state->list, LVM_SETITEMTEXTW, row, (LPARAM)&item);
        item.iSubItem = 2;
        swprintf_s(id, ARRAYSIZE(id), L"%03X", (unsigned)part->id);
        item.pszText = id;
        SendMessageW(state->list, LVM_SETITEMTEXTW, row, (LPARAM)&item);
        ListView_SetCheckState(state->list, row, HasMod(state, part->id));
    }
    state->updating = FALSE;
}

static void CreateModDialog(ModDialog *state)
{
    static const LPCWSTR paintLabels[] = {
        L"原厂涂装", L"喷漆 1", L"喷漆 2", L"喷漆 3", L"喷漆 4（自定义载具）"
    };
    DialogState *base = &state->base;
    int index, painting = state->values[0];
    Control(base, WC_STATICW, UiTranslate(state->vehicle->name), SS_CENTERIMAGE,
            0, 0, 16, 15, 216, 24);
    Control(base, WC_STATICW, L"车身喷漆", SS_CENTERIMAGE, 0, 0, 248, 15, 66, 24);
    state->paint = Control(base, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP,
                           0, MODS_PAINT, 322, 15, 187, 132);
    for (index = 0; index < ARRAYSIZE(paintLabels); ++index)
        SendMessageW(state->paint, CB_ADDSTRING, 0, (LPARAM)paintLabels[index]);
    SendMessageW(state->paint, CB_SETCURSEL,
                 painting >= 0 && painting < ARRAYSIZE(paintLabels) ? painting : 0, 0);
    state->list = Control(base, WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                          LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, MODS_LIST, 16, 54, 493, 316);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES |
                                     LVS_EX_DOUBLEBUFFER);
    AddColumn(base, state->list, 0, L"可用配件", 260);
    AddColumn(base, state->list, 1, L"部位", 137);
    AddColumn(base, state->list, 2, L"编号", 62);
    FillModList(state);
    Control(base, WC_BUTTONW, L"清除改装", WS_TABSTOP | BS_PUSHBUTTON, 0,
            MODS_CLEAR, 16, base->height - 42, 108, 27);
    AddConfirmButtons(base);
}

static void SelectOnlyInGroup(ModDialog *state, int selected)
{
    const ModPart *current = PartAt(state, selected), *other;
    int row, count = ListView_GetItemCount(state->list);
    if (!current || current->group == MOD_OTHER)
        return;
    state->updating = TRUE;
    for (row = 0; row < count; ++row) {
        if (row == selected || !ListView_GetCheckState(state->list, row))
            continue;
        other = PartAt(state, row);
        if (other && other->group == current->group)
            ListView_SetCheckState(state->list, row, FALSE);
    }
    state->updating = FALSE;
}

static BOOL HandleModDialog(DialogState *base, UINT message, WPARAM wParam, LPARAM lParam)
{
    ModDialog *state = (ModDialog *)base;
    int row, count, index;
    const ModPart *part;
    NMLISTVIEW *change;
    if (message == WM_INITDIALOG) {
        CreateModDialog(state);
        return TRUE;
    }
    if (message == WM_NOTIFY && !state->updating) {
        change = (NMLISTVIEW *)lParam;
        if (change->hdr.hwndFrom == state->list && change->hdr.code == LVN_ITEMCHANGED &&
            ((change->uOldState ^ change->uNewState) & LVIS_STATEIMAGEMASK) != 0 &&
            (change->uNewState & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2)) {
            SelectOnlyInGroup(state, change->iItem);
            return TRUE;
        }
    }
    if (message != WM_COMMAND)
        return FALSE;
    count = ListView_GetItemCount(state->list);
    if (LOWORD(wParam) == MODS_CLEAR) {
        SendMessageW(state->paint, CB_SETCURSEL, 0, 0);
        for (row = 0; row < count; ++row)
            ListView_SetCheckState(state->list, row, FALSE);
        return TRUE;
    }
    if (LOWORD(wParam) == IDOK) {
        ZeroMemory(state->values, sizeof(state->values));
        state->values[0] = (int)SendMessageW(state->paint, CB_GETCURSEL, 0, 0);
        index = 1;
        for (row = 0; row < count && index < UI_VEHICLE_MOD_SLOTS; ++row) {
            if (!ListView_GetCheckState(state->list, row))
                continue;
            part = PartAt(state, row);
            if (part)
                state->values[index++] = part->id;
        }
        EndDialog(base->dialog, IDOK);
        return TRUE;
    }
    return FALSE;
}

BOOL UiEditVehicleMods(HWND owner, int modelId, int slots[UI_VEHICLE_MOD_SLOTS])
{
    ModDialog state = { 0 };
    LoadCatalog();
    LoadMods();
    state.vehicle = FindVehicle(modelId);
    if (!state.vehicle || !state.vehicle->mods[0] || modCount == 0) {
        MessageBoxW(GetAncestor(owner, GA_ROOT), L"此载具没有可用的改装部件。",
                    L"载具改装", MB_OK | MB_ICONINFORMATION);
        return FALSE;
    }
    state.base.handle = HandleModDialog;
    state.base.width = 525;
    state.base.height = 424;
    CopyMemory(state.values, slots, sizeof(state.values));
    if (!ShowDialog(owner, L"载具改装", &state.base))
        return FALSE;
    CopyMemory(slots, state.values, sizeof(state.values));
    return TRUE;
}
