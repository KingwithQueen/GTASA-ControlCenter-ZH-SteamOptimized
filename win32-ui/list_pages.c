#include "ui.h"

#include <stdlib.h>
#include <strsafe.h>

enum {
    PAGE_GARAGES = 2,
    PAGE_CHEATS,
    PAGE_LOCATIONS,
    PAGE_SHORTCUTS,
    ID_LIST = 1000,
    ID_REGION,
    ID_APPLY,
    ID_RESET,
    ID_TOGGLE,
    ID_FIELD_FIRST,
    FIELD_COUNT = 6,
    FIELD_LENGTH = 72,
    GARAGE_COUNT = 17,
    CHEAT_COUNT = 130,
    LOCATION_COUNT = 503,
    SHORTCUT_COUNT = 69,
    MAP_LIMIT = 3000
};

typedef struct {
    WCHAR fields[FIELD_COUNT][FIELD_LENGTH];
    BOOL checked;
} SampleRow;

typedef struct {
    LPCWSTR heading;
    LPCWSTR tabNote;
    LPCWSTR fields[FIELD_COUNT];
    LPCWSTR columns[4];
    LPCWSTR toggle;
    const int columnWidths[4];
    int fieldCount;
    int columnCount;
    int rowCount;
    SampleRow *rows;
} ListPageSpec;

typedef struct {
    HWND list;
    HWND region;
    HWND fields[FIELD_COUNT];
    HWND toggle;
    HWND countLabel;
    HWND feedback;
    HWND map;
} ListPageState;

typedef struct {
    LPCWSTR name;
    int first;
    int count;
} GarageRegion;

typedef struct {
    POINT coordinate;
    BOOL valid;
} MapPoint;

static const GarageRegion garageRegions[] = {
    { L"洛圣都", 0, 5 },
    { L"圣辉洛", 5, 4 },
    { L"拉斯文加斯", 9, 4 },
    { L"骨郡", 13, 4 }
};

static const LPCWSTR garageNames[] = {
    L"约翰逊家", L"科罗娜", L"圣玛丽亚海滩", L"穆赫兰", L"帕洛米诺溪",
    L"卡尔顿高地", L"天堂区", L"多尔蒂", L"哈什伯里",
    L"刺松", L"白木庄园", L"红沙西区", L"岩岸西区",
    L"迪利莫", L"卡森堡", L"翠绿草原", L"翠绿草原机库"
};

static const LPCWSTR garageVehicles[] = {
    L"无", L"公路摩托车", L"皮卡", L"跑车", L"低底盘车", L"直升机"
};

static const LPCWSTR cheatExamples[][3] = {
    { L"补满生命和护甲", L"游戏玩法", L"HESOYAM" },
    { L"降低通缉等级", L"通缉等级", L"ASNAEB" },
    { L"所有汽车配备氮气", L"载具", L"SPEEDFREAK" },
    { L"船只飞行", L"载具", L"FLYINGFISH" },
    { L"晴朗天气", L"天气", L"PLEASANTLYWARM" },
    { L"加快时钟", L"时间", L"YSOHNUL" }
};

static const LPCWSTR locationExamples[][4] = {
    { L"甘顿（约翰逊家）", L"洛圣都", L"2495.33", L"-1687.60" },
    { L"圣玛丽亚海滩", L"洛圣都", L"374.81", L"-2025.18" },
    { L"多尔蒂（车库）", L"圣辉洛", L"-2027.52", L"156.31" },
    { L"天堂区", L"圣辉洛", L"-2695.38", L"818.77" },
    { L"红沙西区", L"拉斯文加斯", L"1664.21", L"2252.22" },
    { L"翠绿草原", L"骨郡", L"414.44", L"2533.44" }
};

typedef struct {
    LPCWSTR name;
    LPCWSTR category;
    int count;
} LocationGroup;

static const LocationGroup locationGroups[] = {
    { L"鲜花", L"道具", 40 },
    { L"涂鸦", L"收集品", 100 },
    { L"牡蛎", L"收集品", 50 },
    { L"马蹄铁", L"收集品", 50 },
    { L"快照", L"活动", 50 },
    { L"独特跳跃", L"活动", 70 },
    { L"传送点", L"地点", 137 }
};

static const LPCWSTR shortcutExamples[][3] = {
    { L"开启飞行辅助", L"驾驶辅助", L"1" },
    { L"关闭飞行辅助", L"驾驶辅助", L"2" },
    { L"解锁车门", L"驾驶辅助", L"Alt+Enter" },
    { L"设置车辆重量", L"驾驶辅助", L"Alt+F11" },
    { L"冻结车辆移动", L"驾驶辅助", L"End" },
    { L"传送至多尔蒂车库", L"地点", L"Ctrl+S" }
};

static SampleRow garageRows[GARAGE_COUNT];
static SampleRow cheatRows[CHEAT_COUNT];
static SampleRow locationRows[LOCATION_COUNT];
static SampleRow shortcutRows[SHORTCUT_COUNT];
static ListPageState states[4];
static MapPoint locationPoints[LOCATION_COUNT];
static const WCHAR mapClass[] = L"GTASAControlCenterCoordinatePlot";

static const ListPageSpec specs[] = {
    {
        L"车库样例", L"地区",
        { L"车库名称", L"停放载具", L"车位" },
        { L"车库名称", L"停放载具", L"车位", L"锁定" },
        L"锁定车库", { 228, 186, 70, 84 },
        3, 4, GARAGE_COUNT, garageRows
    },
    {
        L"秘籍样例", L"选择一条秘籍查看或编辑内容",
        { L"秘籍名称", L"分组", L"秘籍代码" },
        { L"秘籍名称", L"分组", L"秘籍代码" },
        NULL, { 236, 145, 176, 0 },
        3, 3, CHEAT_COUNT, cheatRows
    },
    {
        L"地点样例", L"选择地点查看或编辑坐标",
        { L"地点名称", L"分类", L"X 坐标", L"Y 坐标", L"Z 坐标", L"朝向" },
        { L"地点名称", L"分类", L"X 坐标", L"Y 坐标" },
        NULL, { 226, 120, 108, 109 },
        6, 4, LOCATION_COUNT, locationRows
    },
    {
        L"快捷键样例", L"选择快捷键查看或编辑绑定",
        { L"指令名称", L"分组", L"按键组合" },
        { L"指令名称", L"分组", L"按键组合", L"状态" },
        L"启用快捷键", { 207, 121, 137, 95 },
        3, 4, SHORTCUT_COUNT, shortcutRows
    }
};

static const ListPageSpec *GetSpec(const UiPage *page)
{
    return &specs[page->index - PAGE_GARAGES];
}

static ListPageState *GetState(const UiPage *page)
{
    return &states[page->index - PAGE_GARAGES];
}

static void SetField(SampleRow *row, int field, LPCWSTR value)
{
    StringCchCopyW(row->fields[field], FIELD_LENGTH, value);
}

static void SetExample(SampleRow *row, const LPCWSTR *values, int count)
{
    int field;

    for (field = 0; field < count; ++field) {
        SetField(row, field, values[field]);
    }
}

static void FillGarageRows(void)
{
    int i;

    for (i = 0; i < GARAGE_COUNT; ++i) {
        SetField(&garageRows[i], 0, garageNames[i]);
        SetField(&garageRows[i], 1, garageVehicles[i % ARRAYSIZE(garageVehicles)]);
        StringCchPrintfW(garageRows[i].fields[2], FIELD_LENGTH, L"%d", 1 + i % 4);
        garageRows[i].checked = (i % 4 == 1);
    }
}

static void FillCheatRows(void)
{
    static const LPCWSTR groups[] = {
        L"游戏玩法", L"载具", L"天气", L"武器", L"时间", L"通缉等级"
    };
    int row;
    LPCWSTR group;

    for (row = 0; row < (int)ARRAYSIZE(cheatExamples); ++row) {
        SetExample(&cheatRows[row], cheatExamples[row], 3);
    }
    for (; row < CHEAT_COUNT; ++row) {
        group = groups[(row / 12) % ARRAYSIZE(groups)];
        StringCchPrintfW(cheatRows[row].fields[0], FIELD_LENGTH,
                        L"%ls样例 %03d", group, row + 1);
        SetField(&cheatRows[row], 1, group);
        StringCchPrintfW(cheatRows[row].fields[2], FIELD_LENGTH, L"DEMO%03d", row + 1);
    }
}

static BOOL ReadCoordinate(LPCWSTR text, LONG *coordinate)
{
    double value;
    LPWSTR end;

    value = wcstod(text, &end);
    if (end == text || *end || !(value >= -MAP_LIMIT && value <= MAP_LIMIT)) {
        return FALSE;
    }
    *coordinate = (LONG)value;
    return TRUE;
}

static void CacheMapPoint(int index)
{
    MapPoint *point;
    const SampleRow *row;

    point = &locationPoints[index];
    row = &locationRows[index];
    point->valid = ReadCoordinate(row->fields[2], &point->coordinate.x) &&
                   ReadCoordinate(row->fields[3], &point->coordinate.y);
}

static void FillLocationRows(void)
{
    int row;
    int group;
    int number;
    SampleRow *entry;

    for (row = 0; row < (int)ARRAYSIZE(locationExamples); ++row) {
        SetExample(&locationRows[row], locationExamples[row], 4);
    }
    for (group = 0; group < (int)ARRAYSIZE(locationGroups); ++group) {
        for (number = 1; number <= locationGroups[group].count; ++number) {
            entry = &locationRows[row];
            StringCchPrintfW(entry->fields[0], FIELD_LENGTH, L"%ls %03d",
                            locationGroups[group].name, number);
            SetField(entry, 1, locationGroups[group].category);
            StringCchPrintfW(entry->fields[2], FIELD_LENGTH, L"%d.%02d",
                            (row * row * 73 + row * 197) % 5900 - 2950, row * 3 % 100);
            StringCchPrintfW(entry->fields[3], FIELD_LENGTH, L"%d.%02d",
                            (row * row * 31 + row * 139) % 5800 - 2900, row * 7 % 100);
            ++row;
        }
    }
    for (row = 0; row < LOCATION_COUNT; ++row) {
        StringCchPrintfW(locationRows[row].fields[4], FIELD_LENGTH, L"%d.%02d",
                        12 + row % 94, row * 9 % 100);
        StringCchPrintfW(locationRows[row].fields[5], FIELD_LENGTH, L"%d", row * 23 % 360);
        CacheMapPoint(row);
    }
}

static void FillShortcutRows(void)
{
    static const LPCWSTR groupNames[] = {
        L"载具控制", L"驾驶辅助", L"玩家状态", L"地点", L"其他"
    };
    int row;
    LPCWSTR group;

    for (row = 0; row < (int)ARRAYSIZE(shortcutExamples); ++row) {
        SetExample(&shortcutRows[row], shortcutExamples[row], 3);
    }
    for (; row < SHORTCUT_COUNT; ++row) {
        group = groupNames[(row / 13) % ARRAYSIZE(groupNames)];
        StringCchPrintfW(shortcutRows[row].fields[0], FIELD_LENGTH,
                        L"%ls样例 %02d", group, row + 1);
        SetField(&shortcutRows[row], 1, group);
        StringCchPrintfW(shortcutRows[row].fields[2], FIELD_LENGTH,
                        L"Ctrl+%c", 'A' + row % 26);
    }
    for (row = 0; row < SHORTCUT_COUNT - 4; ++row) {
        shortcutRows[row].checked = TRUE;
    }
}

static const GarageRegion *GetGarageRegion(const ListPageState *state)
{
    LRESULT region;

    region = SendMessageW(state->region, CB_GETCURSEL, 0, 0);
    if (region < 0 || region >= (LRESULT)ARRAYSIZE(garageRegions)) {
        region = 0;
    }
    return &garageRegions[region];
}

static SampleRow *GetRow(UiPage *page, int listIndex)
{
    const ListPageSpec *spec;
    const GarageRegion *region;

    spec = GetSpec(page);
    if (listIndex < 0) {
        return NULL;
    }
    if (page->index == PAGE_GARAGES) {
        region = GetGarageRegion(GetState(page));
        if (listIndex >= region->count) {
            return NULL;
        }
        listIndex += region->first;
    }
    if (listIndex >= spec->rowCount) {
        return NULL;
    }
    return &spec->rows[listIndex];
}

static POINT PlotPosition(const RECT *plot, const POINT *coordinate)
{
    POINT pixel;

    pixel.x = plot->left + MulDiv(coordinate->x + MAP_LIMIT, plot->right - plot->left, 2 * MAP_LIMIT);
    pixel.y = plot->top + MulDiv(MAP_LIMIT - coordinate->y, plot->bottom - plot->top, 2 * MAP_LIMIT);
    return pixel;
}

static void DrawMap(HDC dc, const RECT *client)
{
    RECT plot, mark;
    HBRUSH gridBrush, pointBrush, selectionBrush;
    POINT point;
    HWND list;
    int x, y, row, selected;

    FillRect(dc, client, GetSysColorBrush(COLOR_WINDOW));
    FrameRect(dc, client, GetSysColorBrush(COLOR_3DSHADOW));
    plot = *client;
    InflateRect(&plot, -8, -8);
    if (IsRectEmpty(&plot)) {
        return;
    }
    gridBrush = GetSysColorBrush(COLOR_3DFACE);
    for (row = 0; row <= 6; ++row) {
        x = plot.left + MulDiv(row, plot.right - plot.left, 6);
        y = plot.top + MulDiv(row, plot.bottom - plot.top, 6);
        SetRect(&mark, x, plot.top, x + 1, plot.bottom + 1);
        FillRect(dc, &mark, gridBrush);
        SetRect(&mark, plot.left, y, plot.right + 1, y + 1);
        FillRect(dc, &mark, gridBrush);
    }

    pointBrush = CreateSolidBrush(RGB(107, 138, 159));
    if (pointBrush) {
        for (row = 0; row < LOCATION_COUNT; ++row) {
            if (locationPoints[row].valid) {
                point = PlotPosition(&plot, &locationPoints[row].coordinate);
                SetRect(&mark, point.x - 1, point.y - 1, point.x + 1, point.y + 1);
                FillRect(dc, &mark, pointBrush);
            }
        }
        DeleteObject(pointBrush);
    }

    list = states[PAGE_LOCATIONS - PAGE_GARAGES].list;
    selected = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if (selected >= 0 && selected < LOCATION_COUNT && locationPoints[selected].valid) {
        selectionBrush = GetSysColorBrush(COLOR_HIGHLIGHT);
        point = PlotPosition(&plot, &locationPoints[selected].coordinate);
        SetRect(&mark, point.x - 5, point.y - 5, point.x + 6, point.y + 6);
        FrameRect(dc, &mark, selectionBrush);
        InflateRect(&mark, -3, -3);
        FillRect(dc, &mark, selectionBrush);
    }
}

static LRESULT CALLBACK MapProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT paint;
    RECT client;
    HDC display, buffer;
    HBITMAP bitmap;
    HGDIOBJ oldBitmap;

    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        display = BeginPaint(window, &paint);
        GetClientRect(window, &client);
        buffer = CreateCompatibleDC(display);
        bitmap = buffer ? CreateCompatibleBitmap(display, client.right, client.bottom) : NULL;
        if (bitmap) {
            oldBitmap = SelectObject(buffer, bitmap);
            DrawMap(buffer, &client);
            BitBlt(display, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, oldBitmap);
            DeleteObject(bitmap);
        } else {
            DrawMap(display, &client);
        }
        if (buffer) {
            DeleteDC(buffer);
        }
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void CreateMap(UiPage *page)
{
    WNDCLASSW wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = MapProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = mapClass;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        page->failed = TRUE;
        return;
    }
    UiControl(page, WC_STATICW, L"坐标预览", SS_LEFT, 0, 0, 628, 237, 261, 21);
    GetState(page)->map = UiControl(page, mapClass, L"", 0, 0, 0, 628, 260, 261, 140);
}

static void LoadSelection(UiPage *page)
{
    const ListPageSpec *spec;
    ListPageState *state;
    SampleRow *row;
    int selected;
    int i;

    spec = GetSpec(page);
    state = GetState(page);
    selected = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
    row = GetRow(page, selected);

    for (i = 0; i < spec->fieldCount; ++i) {
        SetWindowTextW(state->fields[i], row ? row->fields[i] : L"");
        EnableWindow(state->fields[i], row != NULL);
    }
    if (state->toggle) {
        SendMessageW(state->toggle, BM_SETCHECK,
                     row && row->checked ? BST_CHECKED : BST_UNCHECKED, 0);
        EnableWindow(state->toggle, row != NULL);
    }
    EnableWindow(GetDlgItem(page->window, ID_APPLY), row != NULL);
    EnableWindow(GetDlgItem(page->window, ID_RESET), row != NULL);
    SetWindowTextW(state->feedback, L"");
    if (state->map) {
        InvalidateRect(state->map, NULL, FALSE);
    }
}

static void SetListCount(UiPage *page)
{
    ListPageState *state;
    const ListPageSpec *spec;
    WCHAR countLabel[64];
    int visible;

    state = GetState(page);
    spec = GetSpec(page);
    visible = spec->rowCount;
    if (page->index == PAGE_GARAGES) {
        visible = GetGarageRegion(state)->count;
        StringCchPrintfW(countLabel, ARRAYSIZE(countLabel),
                        L"本地区 %d 座车库 · 共 %d 座", visible, spec->rowCount);
    } else {
        StringCchPrintfW(countLabel, ARRAYSIZE(countLabel), L"%d 条样例", visible);
    }

    ListView_SetItemCount(state->list, visible);
    ListView_SetItemState(state->list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(state->list, 0, LVIS_SELECTED | LVIS_FOCUSED,
                         LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(state->list, 0, FALSE);
    ListView_RedrawItems(state->list, 0, visible - 1);
    SetWindowTextW(state->countLabel, countLabel);
    LoadSelection(page);
}

static void CreateFields(UiPage *page)
{
    const ListPageSpec *spec;
    ListPageState *state;
    HWND group;
    int i;
    int x, y, width;

    spec = GetSpec(page);
    state = GetState(page);
    group = UiControl(page, WC_BUTTONW, spec->heading, BS_GROUPBOX | WS_CLIPSIBLINGS,
                      0, 0, 611, 36, 297, 478);
    if (group) {
        SetWindowPos(group, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    for (i = 0; i < spec->fieldCount; ++i) {
        if (page->index == PAGE_LOCATIONS) {
            x = i < 2 ? 628 : 628 + i % 2 * 135;
            y = i < 2 ? 62 + i * 43 : 150 + (i - 2) / 2 * 43;
            width = i < 2 ? 261 : 126;
        } else {
            x = 628;
            y = 62 + i * 55;
            width = 261;
        }
        UiControl(page, WC_STATICW, spec->fields[i], SS_LEFT, 0, 0, x, y, width, 19);
        state->fields[i] = UiControl(page, WC_EDITW, L"", ES_AUTOHSCROLL | WS_TABSTOP,
                                     WS_EX_CLIENTEDGE, ID_FIELD_FIRST + i,
                                     x, y + 18, width, 24);
        SendMessageW(state->fields[i], EM_SETLIMITTEXT, FIELD_LENGTH - 1, 0);
    }
    if (page->index == PAGE_LOCATIONS) {
        CreateMap(page);
    }
    if (spec->toggle) {
        state->toggle = UiControl(page, WC_BUTTONW, spec->toggle,
                                  BS_AUTOCHECKBOX | WS_TABSTOP, 0, ID_TOGGLE,
                                  628, 62 + spec->fieldCount * 55, 261, 24);
    }
    UiControl(page, WC_BUTTONW, L"保存样例修改", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_APPLY,
              628, 416, 127, 29);
    UiControl(page, WC_BUTTONW, L"还原输入", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_RESET,
              762, 416, 127, 29);
    state->feedback = UiControl(page, WC_STATICW, L"", SS_LEFT, 0, 0, 628, 458, 261, 35);
}

void BuildListPage(UiPage *page)
{
    const ListPageSpec *spec;
    ListPageState *state;
    LVCOLUMNW column;
    DWORD extendedStyle;
    int i;

    spec = GetSpec(page);
    state = GetState(page);
    ZeroMemory(state, sizeof(*state));

    if (page->index == PAGE_GARAGES) {
        FillGarageRows();
        UiControl(page, WC_STATICW, spec->tabNote, SS_LEFT, 0, 0, 12, 10, 45, 21);
        state->region = UiControl(page, WC_COMBOBOXW, L"",
                                  CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, ID_REGION,
                                  62, 7, 160, 125);
        for (i = 0; i < (int)ARRAYSIZE(garageRegions); ++i) {
            SendMessageW(state->region, CB_ADDSTRING, 0, (LPARAM)garageRegions[i].name);
        }
        SendMessageW(state->region, CB_SETCURSEL, 0, 0);
    } else {
        switch (page->index) {
        case PAGE_CHEATS:
            FillCheatRows();
            break;
        case PAGE_LOCATIONS:
            FillLocationRows();
            break;
        case PAGE_SHORTCUTS:
            FillShortcutRows();
            break;
        }
        UiControl(page, WC_STATICW, spec->tabNote, SS_LEFT, 0, 0, 12, 10, 350, 21);
    }

    state->countLabel = UiControl(page, WC_STATICW, L"", SS_RIGHT, 0, 0, 395, 10, 200, 21);
    state->list = UiControl(page, WC_LISTVIEWW, L"",
                            LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL |
                            LVS_SHOWSELALWAYS | WS_TABSTOP,
                            WS_EX_CLIENTEDGE, ID_LIST, 12, 36, 585, 478);
    SendMessageW(state->list, CCM_SETUNICODEFORMAT, TRUE, 0);
    extendedStyle = LVS_EX_FULLROWSELECT;
#ifdef LVS_EX_DOUBLEBUFFER
    extendedStyle |= LVS_EX_DOUBLEBUFFER;
#endif
    ListView_SetExtendedListViewStyle(state->list, extendedStyle);

    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    for (i = 0; i < spec->columnCount; ++i) {
        column.pszText = (LPWSTR)spec->columns[i];
        column.cx = UiScale(page, spec->columnWidths[i]);
        column.iSubItem = i;
        SendMessageW(state->list, LVM_INSERTCOLUMNW, i, (LPARAM)&column);
    }
    CreateFields(page);
    SetListCount(page);
}

LRESULT ListPageNotify(UiPage *page, NMHDR *notification)
{
    const ListPageSpec *spec;
    ListPageState *state;
    const SampleRow *row;
    NMLVDISPINFOW *display;
    LPCWSTR value;

    if (page->index < PAGE_GARAGES || page->index > PAGE_SHORTCUTS) {
        return 0;
    }
    spec = GetSpec(page);
    state = GetState(page);
    if (notification->hwndFrom != state->list) {
        return 0;
    }

    switch (notification->code) {
    case LVN_GETDISPINFOW:
        display = (NMLVDISPINFOW *)notification;
        if (!(display->item.mask & LVIF_TEXT) || !display->item.pszText ||
            display->item.cchTextMax <= 0) {
            break;
        }
        row = GetRow(page, display->item.iItem);
        value = L"";
        if (row && display->item.iSubItem >= 0 && display->item.iSubItem < spec->columnCount) {
            if (spec->toggle && display->item.iSubItem == spec->columnCount - 1) {
                if (page->index == PAGE_GARAGES) {
                    value = row->checked ? L"已锁定" : L"未锁定";
                } else {
                    value = row->checked ? L"启用" : L"停用";
                }
            } else {
                value = row->fields[display->item.iSubItem];
            }
        }
        StringCchCopyW(display->item.pszText, (size_t)display->item.cchTextMax, value);
        break;
    case LVN_ITEMCHANGED:
        if ((((NMLISTVIEW *)notification)->uOldState ^ ((NMLISTVIEW *)notification)->uNewState)
            & LVIS_SELECTED) {
            LoadSelection(page);
        }
        break;
    case LVN_ODSTATECHANGED:
        LoadSelection(page);
        break;
    }
    return 0;
}

BOOL ListPageCommand(UiPage *page, int id, int code, HWND control)
{
    const ListPageSpec *spec;
    ListPageState *state;
    SampleRow *row;
    int selected;
    int i;

    if (page->index < PAGE_GARAGES || page->index > PAGE_SHORTCUTS) {
        return FALSE;
    }
    spec = GetSpec(page);
    state = GetState(page);
    if (page->index == PAGE_GARAGES && id == ID_REGION && code == CBN_SELCHANGE &&
        control == state->region) {
        SetListCount(page);
        return TRUE;
    }
    if (code != BN_CLICKED || !control) {
        return FALSE;
    }
    if (id == ID_RESET) {
        LoadSelection(page);
        SetWindowTextW(state->feedback, L"已还原为当前记录的值。");
        return TRUE;
    }
    if (id != ID_APPLY) {
        return FALSE;
    }

    selected = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
    row = GetRow(page, selected);
    if (!row) {
        return TRUE;
    }
    if (GetWindowTextLengthW(state->fields[0]) == 0) {
        SetWindowTextW(state->feedback, L"请输入名称。");
        SetFocus(state->fields[0]);
        return TRUE;
    }
    for (i = 0; i < spec->fieldCount; ++i) {
        GetWindowTextW(state->fields[i], row->fields[i], FIELD_LENGTH);
    }
    if (state->toggle) {
        row->checked = SendMessageW(state->toggle, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    if (state->map) {
        CacheMapPoint(selected);
        InvalidateRect(state->map, NULL, FALSE);
    }
    ListView_RedrawItems(state->list, selected, selected);
    SetWindowTextW(state->feedback, L"已保存到本次会话。");
    return TRUE;
}
