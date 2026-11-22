#include "ui.h"
#include "dialogs.h"
#include "legacy_list_data.h"
#include "location_map.h"

enum {
    ID_LIST = 1000, ID_GROUP, ID_APPLY, ID_RESET, ID_ENABLED, ID_FIELD_FIRST,
    ID_NEW = 1011, ID_DELETE, ID_NEW_GROUP, ID_RENAME_GROUP, ID_DELETE_GROUP, ID_IMPORT, ID_EXPORT,
    ID_GROUP_ENABLE, ID_GROUP_DISABLE, ID_CHEAT_COPY,
    ID_GARAGE_SLOT, ID_GARAGE_VEHICLE, ID_GARAGE_COLORS, ID_GARAGE_MODS, ID_GARAGE_CLEAR_MODS,
    ID_GARAGE_READ_PREVIEW, ID_GARAGE_WRITE_PREVIEW, ID_GARAGE_PROTECTION,
    ID_SHORTCUT_CATEGORY = 1040, ID_SHORTCUT_COMMAND, ID_SHORTCUT_KEY, ID_SHORTCUT_CTRL, ID_SHORTCUT_ALT,
    ID_SHORTCUT_INTERVAL, ID_SHORTCUT_ORIGINAL, ID_SHORTCUT_FEEDBACK, ID_SHORTCUT_VERSION,
    ID_SHORTCUT_PARAMETER_HELP, ID_SHORTCUT_TEST, ID_SHORTCUT_PARAMETER_PICK, ID_SHORTCUT_FOLDER_MOVE,
    ID_FEEDBACK = 1089, ID_MAP, ID_ZOOM, ID_CENTER, ID_MARKERS, ID_MARKER_SIZE,
    ID_SHOW_ON_MAP, ID_READ_MAP, ID_TELEPORT, ID_READ_PLAYER
};

typedef struct {
    LPCWSTR region;
    int first, count;
} GarageRegion;

typedef struct {
    HWND list, group, countLabel, feedback, field[6], enabled;
    HWND category, command, key, ctrl, alt, parameterHint;
    HWND garageSlots, garageModel, garageColors, garageMods, garageFlags[4];
    WCHAR folders[MAX_GROUPS][FIELD_LENGTH];
    int sourceRows[MAX_ROWS], count, folderCount, garageSlot, commandCategory;
    BOOL changing;
} ListState;

static UiPage *listPages[4];
static ListState listStates[4];
static int collectionRevision[3];
static int commandRevision;
static WCHAR previewPosition[4][FIELD_LENGTH] = { L"2495.33", L"-1687.60", L"13.52", L"90" };

static const GarageRegion garageRegions[] = {
    { L"洛圣都", 0, 5 }, { L"圣辉洛", 13, 4 }, { L"拉斯文加斯", 5, 4 }, { L"骨郡", 9, 4 }
};
static const LPCWSTR garageNames[] = {
    L"约翰逊家", L"科罗娜", L"圣玛丽亚海滩", L"穆赫兰", L"帕洛米诺溪", L"刺松",
    L"白木庄园", L"红沙西区", L"岩岸西区", L"迪利莫", L"卡森堡", L"翠绿草原",
    L"翠绿草原机库", L"卡尔顿高地", L"天堂区", L"多尔蒂", L"哈什伯里"
};
static const LPCWSTR parameterHints[] = {
    L"护甲值 0–1000", L"生命值 0–1000", L"通缉等级 0–6",
    L"武器参数（沿用原配置格式）", L"武器熟练度参数（沿用原配置格式）",
    L"车辆防护（爆炸;碰撞;子弹;火焰）", L"车门 0＝锁定，1＝解锁",
    L"天气编号", L"引擎健康 0–4000（千分值）", L"车辆重量 1–4000（0.1 吨）",
    L"车身颜色（主色;0;副色;0）", L"方向与启动速度（沿用原配置格式）",
    L"载具型号编号", L"速度或旋转（千分值，允许负值）",
    L"任务计时参数", L"击杀计数", L"0＝关闭，1＝开启", L"无需参数", L"位置标记编号",
    L"转向角度"
};
static const struct { int code; LPCWSTR name; } specialKeys[] = {
    { VK_BACK, L"BACKSPACE" }, { VK_TAB, L"TAB" }, { VK_RETURN, L"ENTER" }, { VK_SHIFT, L"SHIFT" },
    { VK_CONTROL, L"CTRL" }, { VK_MENU, L"ALT" }, { VK_PAUSE, L"PAUSE" }, { VK_ESCAPE, L"ESC" },
    { VK_SPACE, L"SPACE" }, { VK_PRIOR, L"PAGE UP" }, { VK_NEXT, L"PAGE DOWN" }, { VK_END, L"END" },
    { VK_HOME, L"HOME" }, { VK_LEFT, L"LEFT" }, { VK_UP, L"UP" }, { VK_RIGHT, L"RIGHT" },
    { VK_DOWN, L"DOWN" }, { VK_INSERT, L"INSERT" }, { VK_DELETE, L"DELETE" }, { VK_MULTIPLY, L"NUM *" },
    { VK_ADD, L"NUM +" }, { VK_SUBTRACT, L"NUM -" }, { VK_DECIMAL, L"NUM COMMA" },
    { VK_DIVIDE, L"NUM /" }, { VK_OEM_PLUS, L"+" }, { VK_OEM_MINUS, L"-" },
    { VK_OEM_COMMA, L"," }, { VK_OEM_PERIOD, L"." }
};

static ListState *StateFor(UiPage *page)
{
    return &listStates[page->index - PAGE_GARAGES];
}

static void Feedback(ListState *state, LPCWSTR text)
{
    SetWindowTextW(state->feedback, text);
}

static HWND Label(UiPage *page, LPCWSTR text, int x, int y, int width)
{
    return UiControl(page, WC_STATICW, text, SS_LEFT | SS_NOPREFIX, 0, 0, x, y, width, 20);
}

static HWND FeedbackLabel(UiPage *page, int x, int y, int width)
{
    return UiControl(page, WC_STATICW, L"", SS_LEFT | SS_NOPREFIX, 0,
                     ID_FEEDBACK, x, y, width, 23);
}

static HWND Button(UiPage *page, LPCWSTR text, int id, int x, int y, int width)
{
    return UiControl(page, WC_BUTTONW, text, BS_PUSHBUTTON | WS_TABSTOP, 0, id, x, y, width, 25);
}

static HWND Check(UiPage *page, LPCWSTR text, int id, int x, int y, int width)
{
    return UiControl(page, WC_BUTTONW, text, BS_AUTOCHECKBOX | WS_TABSTOP, 0, id, x, y, width, 24);
}

static HWND Combo(UiPage *page, int id, int x, int y, int width)
{
    return UiControl(page, WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                     0, id, x, y, width, 255);
}

static LRESULT AddCombo(HWND combo, LPCWSTR text, LPARAM value)
{
    LRESULT item = SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)text);
    if (item >= 0)
        SendMessageW(combo, CB_SETITEMDATA, item, value);
    return item;
}

static void ChooseComboItem(HWND combo, int value)
{
    LRESULT item, count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
    for (item = 0; item < count; ++item)
        if (SendMessageW(combo, CB_GETITEMDATA, item, 0) == value)
            break;
    SendMessageW(combo, CB_SETCURSEL, item < count ? item : (WPARAM)-1, 0);
}

static int ComboValue(HWND combo)
{
    LRESULT item = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    return item < 0 ? -1 : (int)SendMessageW(combo, CB_GETITEMDATA, item, 0);
}

static void SetChecked(HWND check, BOOL value)
{
    SendMessageW(check, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
}

static BOOL Checked(HWND check)
{
    return SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static HWND Edit(UiPage *page, LPCWSTR caption, int field, int x, int y, int width)
{
    HWND edit;
    Label(page, caption, x, y, width);
    edit = UiControl(page, WC_EDITW, L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
                     ID_FIELD_FIRST + field, x, y + 17, width, 23);
    SendMessageW(edit, EM_SETLIMITTEXT, FIELD_LENGTH - 1, 0);
    StateFor(page)->field[field] = edit;
    return edit;
}

static int SelectedSource(const ListState *state)
{
    int selected = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
    return selected >= 0 && selected < state->count ? state->sourceRows[selected] : -1;
}

static void SelectVisible(ListState *state, int selected)
{
    ListView_SetItemState(state->list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    if (selected >= 0 && selected < state->count) {
        ListView_SetItemState(state->list, selected, LVIS_SELECTED | LVIS_FOCUSED,
                             LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(state->list, selected, FALSE);
    }
}

static void SelectLocationFromMap(int row)
{
    SelectVisible(StateFor(listPages[PAGE_LOCATIONS - PAGE_GARAGES]), row);
}

static LPCWSTR DisplayFolder(LPCWSTR path, int page)
{
    LPCWSTR root = rootFolders[page - PAGE_CHEATS];
    size_t len = wcslen(root);
    if (_wcsnicmp(path, root, len) == 0) {
        if (!path[len])
            return L"";
        if (path[len] == L'\\')
            return path + len + 1;
    }
    return path;
}

static void FullFolder(WCHAR *output, LPCWSTR path, int page)
{
    LPCWSTR root = rootFolders[page - PAGE_CHEATS];
    if (!*path)
        CopyField(output, root);
    else
        StringCchPrintfW(output, FIELD_LENGTH, L"%ls\\%ls", root, DisplayFolder(path, page));
}

static BOOL IsUnderFolder(LPCWSTR value, LPCWSTR folder)
{
    size_t length = wcslen(folder);
    return _wcsnicmp(value, folder, length) == 0 && (!value[length] || value[length] == L'\\');
}

static void CurrentFolder(UiPage *page, WCHAR *name)
{
    ListState *state = StateFor(page);
    int index = ComboValue(state->group);
    name[0] = L'\0';
    if (index > 0 && index <= state->folderCount)
        CopyField(name, state->folders[index - 1]);
}

static int EnsureFolder(ListState *state, LPCWSTR path)
{
    int a;
    for (a = 0; a < state->folderCount; ++a)
        if (_wcsicmp(state->folders[a], path) == 0)
            return a;
    if (state->folderCount == MAX_GROUPS)
        return -1;
    CopyField(state->folders[state->folderCount], path);
    return state->folderCount++;
}

static void RefreshFolders(UiPage *page, LPCWSTR selection)
{
    ListState *state = StateFor(page);
    const RecordSet *set = RecordsFor(page->index);
    int index, selected = 0;
    WCHAR path[FIELD_LENGTH], *separator;

    for (index = 0; index < set->count; ++index) {
        CopyField(path, set->rows[index].field[1]);
        while ((separator = wcsrchr(path, L'\\')) != NULL) {
            EnsureFolder(state, path);
            *separator = L'\0';
        }
    }
    SendMessageW(state->group, CB_RESETCONTENT, 0, 0);
    AddCombo(state->group, L"所有分组", 0);
    for (index = 0; index < state->folderCount; ++index) {
        AddCombo(state->group, DisplayFolder(state->folders[index], page->index), index + 1);
        if (selection && _wcsicmp(state->folders[index], selection) == 0)
            selected = index + 1;
    }
    ChooseComboItem(state->group, selected);
}

static void DisplaySelection(UiPage *page);

static void RefreshRows(UiPage *page, int keep)
{
    ListState *state = StateFor(page);
    const RecordSet *set;
    WCHAR folder[FIELD_LENGTH], count[72];
    int index, selected = 0, region;
    state->changing = TRUE;
    state->count = 0;
    if (page->index == PAGE_GARAGES) {
        region = max(0, ComboValue(state->group));
        for (index = 0; index < garageRegions[region].count; ++index)
            state->sourceRows[state->count++] = garageRegions[region].first + index;
        StringCchPrintfW(count, ARRAYSIZE(count), L"%d 座车库 / 共 %d 座", state->count, GARAGE_COUNT);
    } else {
        set = RecordsFor(page->index);
        CurrentFolder(page, folder);
        for (index = 0; index < set->count; ++index) {
            if (!folder[0] || IsUnderFolder(set->rows[index].field[1], folder)) {
                if (index == keep)
                    selected = state->count;
                state->sourceRows[state->count++] = index;
            }
        }
        StringCchPrintfW(count, ARRAYSIZE(count), L"%d 条 / 共 %d 条", state->count, set->count);
    }
    ListView_SetItemCount(state->list, state->count);
    SelectVisible(state, selected);
    if (state->count)
        ListView_RedrawItems(state->list, 0, state->count - 1);
    SetWindowTextW(state->countLabel, count);
    state->changing = FALSE;
    if (page->index == PAGE_LOCATIONS) {
        mapView.visible = state->sourceRows;
        mapView.visibleCount = state->count;
        mapView.dirty = TRUE;
        InvalidateRect(mapView.window, NULL, FALSE);
    }
    DisplaySelection(page);
}

static LPCWSTR KeyName(int key, WCHAR name[32])
{
    unsigned index;
    for (index = 0; index < ARRAYSIZE(specialKeys); ++index)
        if (specialKeys[index].code == key)
            return specialKeys[index].name;
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
        name[0] = (WCHAR)key;
        name[1] = L'\0';
    } else if (key >= VK_F1 && key <= VK_F24) {
        StringCchPrintfW(name, 32, L"F%d", key - VK_F1 + 1);
    } else if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
        StringCchPrintfW(name, 32, L"NUM %d", key - VK_NUMPAD0);
    } else {
        StringCchPrintfW(name, 32, L"VK %d", key);
    }
    return name;
}

static void BuildKeyCombo(HWND combo)
{
    int key;
    unsigned i;
    WCHAR name[32];
    for (i = 0; i < ARRAYSIZE(specialKeys); ++i)
        AddCombo(combo, specialKeys[i].name, specialKeys[i].code);
    for (key = '0'; key <= '9'; ++key)
        AddCombo(combo, KeyName(key, name), key);
    for (key = 'A'; key <= 'Z'; ++key)
        AddCombo(combo, KeyName(key, name), key);
    for (key = VK_F1; key <= VK_F24; ++key)
        AddCombo(combo, KeyName(key, name), key);
    for (key = VK_NUMPAD0; key <= VK_NUMPAD9; ++key)
        AddCombo(combo, KeyName(key, name), key);
}

static void PrepareCommandCombo(ListState *state, int category)
{
    const RecordSet *set;
    int i, revision = 0;
    if (category == 1 || category == 2)
        revision = collectionRevision[category - 1];
    if (state->commandCategory == category && commandRevision == revision)
        return;
    state->commandCategory = category;
    commandRevision = revision;
    SendMessageW(state->command, WM_SETREDRAW, FALSE, 0);
    SendMessageW(state->command, CB_RESETCONTENT, 0, 0);
    if (category == 0) {
        for (i = 0; i < (int)ARRAYSIZE(commandSpecs); ++i)
            AddCombo(state->command, UiTranslate(commandSpecs[i].caption), commandSpecs[i].id);
    } else if (category == 1 || category == 2) {
        set = RecordsFor(category == 1 ? PAGE_CHEATS : PAGE_LOCATIONS);
        for (i = 0; i < set->count; ++i)
            AddCombo(state->command, set->rows[i].field[0], i);
    }
    SendMessageW(state->command, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(state->command, NULL, FALSE);
}

static int ShortcutParameterPage(ListState *state)
{
    const CommandSpec *command = FindCommand(ComboValue(state->command));
    return ComboValue(state->category) == 0 && command ? command->parameterPage : 17;
}

static void UpdateParameterHint(ListState *state)
{
    int parameter = ShortcutParameterPage(state);
    BOOL use = parameter != 17;
    SetWindowTextW(state->parameterHint,
        parameter >= 0 && parameter < (int)ARRAYSIZE(parameterHints) ? parameterHints[parameter] : L"附加参数");
    EnableWindow(state->field[3], use);
    EnableWindow(state->field[4], use);
    EnableWindow(GetDlgItem(GetParent(state->command), ID_SHORTCUT_PARAMETER_PICK), parameter == 10 || parameter == 12);
}

static void LoadShortcutEditor(ListState *state, const Record *entry)
{
    const RecordSet *targets;
    int i, key, command = -1, category = entry ? entry->category : 0;
    ChooseComboItem(state->category, category);
    PrepareCommandCombo(state, category);
    if (entry) {
        if (category == 0)
            command = _wtoi(entry->command);
        else if (category == 1 || category == 2) {
            targets = RecordsFor(category == 1 ? PAGE_CHEATS : PAGE_LOCATIONS);
            for (i = 0; i < targets->count; ++i)
                if (_wcsicmp(targets->rows[i].uid, entry->command) == 0)
                    command = i;
        }
    }
    ChooseComboItem(state->command, command < 0 && !entry ? 0 : command);
    key = entry ? entry->key : 'A';
    ChooseComboItem(state->key, key);
    if (key > 0 && ComboValue(state->key) < 0) {
        WCHAR name[32];
        AddCombo(state->key, KeyName(key, name), key);
        ChooseComboItem(state->key, key);
    }
    SetChecked(state->ctrl, entry && entry->modifier == VK_CONTROL);
    SetChecked(state->alt, entry && entry->modifier == VK_MENU);
    SetChecked(state->enabled, entry && entry->enabled);
    UpdateParameterHint(state);
}

static void GarageStatus(UiPage *page)
{
    const ListState *state = StateFor(page);
    int garage = SelectedSource(state), i;
    const GarageSlot *slot;
    LPCWSTR name;
    WCHAR text[160];
    HWND slots = state->garageSlots;
    if (garage < 0)
        return;
    SendMessageW(slots, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < SLOT_COUNT; ++i) {
        slot = &garageData[garage].slots[i];
        name = slot->field[1] ? UiVehicleName((int)slot->field[1]) : L"空车位";
        StringCchPrintfW(text, ARRAYSIZE(text), L"车位 %d  ·  %ls", i + 1, name);
        AddCombo(slots, text, i);
    }
    ChooseComboItem(slots, state->garageSlot);
    SetChecked(state->enabled, garageData[garage].locked);
    slot = &garageData[garage].slots[state->garageSlot];
    name = slot->field[1] ? UiVehicleName((int)slot->field[1]) : L"空车位";
    SetWindowTextW(state->garageModel, name);
    StringCchPrintfW(text, ARRAYSIZE(text), L"主色 #%lu   /   副色 #%lu", slot->field[6], slot->field[7]);
    SetWindowTextW(state->garageColors, text);
    for (i = 0; i < 4; ++i) {
        SetChecked(state->garageFlags[i], slot->field[2 + i] != 0);
        EnableWindow(state->garageFlags[i], slot->field[1] != 0);
    }
    for (i = 9; i < 24; ++i)
        if (slot->field[i] && slot->field[i] != 0xFFFF)
            break;
    SetWindowTextW(state->garageMods, i < 24 || (slot->field[30] != 0 && slot->field[30] != 0xFF)
        ? L"改装状态：已改装" : L"改装状态：原厂");
    EnableWindow(GetDlgItem(page->window, ID_GARAGE_COLORS), slot->field[1] != 0);
    EnableWindow(GetDlgItem(page->window, ID_GARAGE_MODS), slot->field[1] != 0);
    EnableWindow(GetDlgItem(page->window, ID_GARAGE_CLEAR_MODS), slot->field[1] != 0);
}

static void DisplaySelection(UiPage *page)
{
    ListState *state = StateFor(page);
    const Record *entry = NULL;
    int index, selected = SelectedSource(state);
    if (state->changing)
        return;
    if (page->index == PAGE_GARAGES) {
        GarageStatus(page);
        return;
    }
    if (selected >= 0)
        entry = &RecordsFor(page->index)->rows[selected];
    for (index = 0; index < (int)ARRAYSIZE(state->field); ++index) {
        if (state->field[index]) {
            LPCWSTR text = entry ? entry->field[index] : L"";
            if (index == 1 && entry)
                text = DisplayFolder(text, page->index);
            SetWindowTextW(state->field[index], text);
        }
    }
    if (page->index == PAGE_SHORTCUTS)
        LoadShortcutEditor(state, entry);
    if (page->index == PAGE_LOCATIONS)
        InvalidateRect(mapView.window, NULL, FALSE);
    EnableWindow(GetDlgItem(page->window, ID_APPLY), selected >= 0);
    EnableWindow(GetDlgItem(page->window, ID_DELETE), selected >= 0);
}

static void SharedRecordButtons(UiPage *page, int x, int y, int buttonWidth)
{
    int col = buttonWidth + 5;
    Button(page, L"应用修改", ID_APPLY, x, y, buttonWidth);
    Button(page, L"新增记录", ID_NEW, x + col, y, buttonWidth);
    Button(page, L"删除记录", ID_DELETE, x + col * 2, y, buttonWidth);
    Button(page, L"新建分组", ID_NEW_GROUP, x, y + 30, buttonWidth);
    Button(page, L"重命名组", ID_RENAME_GROUP, x + col, y + 30, buttonWidth);
    Button(page, L"删除分组", ID_DELETE_GROUP, x + col * 2, y + 30, buttonWidth);
    Button(page, L"读取配置", ID_IMPORT, x, y + 60, buttonWidth);
    Button(page, L"保存配置", ID_EXPORT, x + col, y + 60, buttonWidth);
    Button(page, L"还原输入", ID_RESET, x + col * 2, y + 60, buttonWidth);
}

static void BuildLocations(UiPage *page)
{
    ListState *state = StateFor(page);
    WCHAR text[20];
    unsigned level;
    state->group = Combo(page, ID_GROUP, 660, 8, 246);
    Label(page, L"分组", 605, 12, 47);
    state->countLabel = Label(page, L"", 605, 35, 301);

    ZeroMemory(&mapView, sizeof(mapView));
    mapView.font = page->font;
    mapView.zoomIndex = 2;
    mapView.markerSize = 4;
    mapView.showMarkers = TRUE;
    mapView.dirty = TRUE;
    mapView.hasPlayer = ReadPoint(previewPosition[0], previewPosition[1], &mapView.player);
    RegisterLocationMap(page);
    Label(page, L"缩放", 12, 13, 43);
    mapView.zoom = Combo(page, ID_ZOOM, 57, 8, 81);
    for (level = 0; level < ARRAYSIZE(mapZoomLevels); ++level) {
        StringCchPrintfW(text, ARRAYSIZE(text), L"%d%%", mapZoomLevels[level]);
        AddCombo(mapView.zoom, text, level);
    }
    ChooseComboItem(mapView.zoom, mapView.zoomIndex);
    Button(page, L"居中地图", ID_CENTER, 152, 8, 81);
    SetChecked(Check(page, L"显示地点标记", ID_MARKERS, 250, 8, 119), TRUE);
    Label(page, L"标记大小", 380, 13, 72);
    {
        const int sizes[] = { 4, 8, 12, 16 };
        HWND combo = Combo(page, ID_MARKER_SIZE, 456, 8, 87);
        for (level = 0; level < ARRAYSIZE(sizes); ++level) {
            StringCchPrintfW(text, ARRAYSIZE(text), L"%d × %d", sizes[level], sizes[level]);
            AddCombo(combo, text, sizes[level]);
        }
        ChooseComboItem(combo, mapView.markerSize);
    }
    Label(page, L"单击选点 · 双击标记选择地点 · 拖动平移 · 滚轮缩放", 12, 38, 575);
    mapView.window = UiControl(page, mapClass, L"传送地点地图", WS_TABSTOP, 0, ID_MAP, 12, 62, 575, 423);
    mapView.label = Label(page, L"", 12, 492, 575);

    Edit(page, L"地点名称", 0, 605, 220, 301);
    Edit(page, L"地点分组（可编辑路径）", 1, 605, 262, 301);
    Edit(page, L"X 坐标", 2, 605, 304, 68);
    Edit(page, L"Y 坐标", 3, 682, 304, 69);
    Edit(page, L"Z 高度", 4, 761, 304, 68);
    Edit(page, L"朝向", 5, 838, 304, 68);
    Button(page, L"在地图上显示", ID_SHOW_ON_MAP, 605, 348, 147);
    Button(page, L"从地图读取 XY", ID_READ_MAP, 759, 348, 147);
    Button(page, L"传送预览", ID_TELEPORT, 605, 378, 147);
    Button(page, L"读取预览位置", ID_READ_PLAYER, 759, 378, 147);
    SharedRecordButtons(page, 605, 408, 97);
    state->feedback = FeedbackLabel(page, 605, 502, 301);
}

static void BuildCheats(UiPage *page)
{
    ListState *state = StateFor(page);
    Label(page, L"分组", 12, 13, 42);
    state->group = Combo(page, ID_GROUP, 62, 8, 275);
    state->countLabel = Label(page, L"", 350, 13, 140);
    Edit(page, L"秘籍名称", 0, 529, 53, 366);
    Edit(page, L"分组（可编辑路径）", 1, 529, 110, 366);
    Edit(page, L"秘籍代码（可组合多个连续代码）", 2, 529, 167, 366);
    Button(page, L"复制秘籍代码", ID_CHEAT_COPY, 529, 217, 145);
    SharedRecordButtons(page, 529, 261, 118);
    state->feedback = FeedbackLabel(page, 529, 370, 366);
}

static void BuildShortcuts(UiPage *page)
{
    ListState *state = StateFor(page);
    static const LPCWSTR versions[] = { L"Steam", L"DVD 1.0", L"DVD 1.01", L"DVD 1.1" };
    unsigned index;
    WCHAR interval[24];
    HWND setting;
    Label(page, L"分组", 12, 13, 42);
    state->group = Combo(page, ID_GROUP, 62, 8, 274);
    state->countLabel = Label(page, L"", 350, 13, 146);

    Edit(page, L"快捷键名称", 0, 516, 47, 385);
    Edit(page, L"分组（可编辑路径）", 1, 516, 88, 385);
    Label(page, L"命令来源", 516, 129, 118);
    Label(page, L"命令 / 秘籍 / 传送地点", 651, 129, 250);
    state->category = Combo(page, ID_SHORTCUT_CATEGORY, 516, 147, 128);
    AddCombo(state->category, L"控制中心命令", 0);
    AddCombo(state->category, L"游戏秘籍", 1);
    AddCombo(state->category, L"传送地点", 2);
    state->command = Combo(page, ID_SHORTCUT_COMMAND, 651, 147, 250);
    state->commandCategory = -1;

    Label(page, L"绑定按键", 516, 181, 120);
    state->key = Combo(page, ID_SHORTCUT_KEY, 516, 198, 123);
    BuildKeyCombo(state->key);
    state->ctrl = Check(page, L"Ctrl", ID_SHORTCUT_CTRL, 649, 198, 52);
    state->alt = Check(page, L"Alt", ID_SHORTCUT_ALT, 705, 198, 52);
    state->enabled = Check(page, L"启用快捷键", ID_ENABLED, 763, 198, 138);
    Edit(page, L"参数值", 3, 516, 229, 192);
    Edit(page, L"参数说明", 4, 717, 229, 184);
    state->parameterHint = Label(page, L"", 516, 274, 263);
    Button(page, L"选择参数", ID_SHORTCUT_PARAMETER_PICK, 791, 273, 110);
    SharedRecordButtons(page, 516, 302, 125);
    Button(page, L"启用全组", ID_GROUP_ENABLE, 516, 396, 125);
    Button(page, L"停用全组", ID_GROUP_DISABLE, 646, 396, 125);
    Button(page, L"预览执行信息", ID_SHORTCUT_TEST, 776, 396, 125);
    state->feedback = FeedbackLabel(page, 516, 429, 385);

    Label(page, L"检测间隔", 15, 464, 76);
    setting = Combo(page, ID_SHORTCUT_INTERVAL, 95, 460, 100);
    for (index = 10; index <= 500; index += 10) {
        StringCchPrintfW(interval, ARRAYSIZE(interval), L"%u 毫秒", index);
        AddCombo(setting, interval, index);
    }
    ChooseComboItem(setting, 100);
    Label(page, L"游戏版本", 244, 464, 73);
    setting = Combo(page, ID_SHORTCUT_VERSION, 323, 460, 168);
    for (index = 0; index < ARRAYSIZE(versions); ++index)
        AddCombo(setting, versions[index], index);
    ChooseComboItem(setting, 0);
    SetChecked(Check(page, L"游戏画面反馈", ID_SHORTCUT_FEEDBACK, 15, 492, 145), TRUE);
    SetChecked(Check(page, L"使用原版 SCM", ID_SHORTCUT_ORIGINAL, 174, 492, 155), TRUE);
}

static void BuildGarages(UiPage *page)
{
    ListState *state = StateFor(page);
    static const LPCWSTR flags[] = { L"防爆", L"防碰撞", L"防弹", L"防火" };
    WCHAR *defaults;
    unsigned index;

    defaults = ResourceText(DATA_GARAGES);
    if (!LoadGarageText(defaults, garageData))
        page->failed = TRUE;
    free(defaults);
    memcpy(mockGarages, garageData, sizeof(mockGarages));

    Label(page, L"地区", 12, 13, 45);
    state->group = Combo(page, ID_GROUP, 62, 8, 230);
    for (index = 0; index < ARRAYSIZE(garageRegions); ++index)
        AddCombo(state->group, garageRegions[index].region, index);
    ChooseComboItem(state->group, 0);
    state->countLabel = Label(page, L"", 324, 13, 180);
    Button(page, L"读取车库配置", ID_IMPORT, 12, 413, 119);
    Button(page, L"保存车库配置", ID_EXPORT, 137, 413, 119);
    Button(page, L"读取预览车库", ID_GARAGE_READ_PREVIEW, 262, 413, 119);
    Button(page, L"写入预览车库", ID_GARAGE_WRITE_PREVIEW, 387, 413, 115);

    Label(page, L"车位（每座车库四个）", 529, 55, 340);
    state->garageSlots = Combo(page, ID_GARAGE_SLOT, 529, 73, 366);
    state->garageModel = Label(page, L"", 529, 112, 169);
    Button(page, L"选择或移除载具", ID_GARAGE_VEHICLE, 718, 107, 177);
    state->garageColors = Label(page, L"", 529, 145, 169);
    Button(page, L"选择车身主色 / 副色", ID_GARAGE_COLORS, 718, 140, 177);
    for (index = 0; index < ARRAYSIZE(flags); ++index)
        state->garageFlags[index] = Check(page, flags[index], ID_GARAGE_PROTECTION + index,
                                         529 + (int)index * 88, 177, 82);
    state->garageMods = Label(page, L"", 529, 223, 142);
    Button(page, L"编辑改装", ID_GARAGE_MODS, 684, 218, 102);
    Button(page, L"清除改装", ID_GARAGE_CLEAR_MODS, 793, 218, 102);
    state->enabled = Check(page, L"锁定此车库的停放车辆及设置", ID_ENABLED, 529, 257, 366);
    state->feedback = FeedbackLabel(page, 529, 304, 366);
}

static void CreateList(UiPage *page)
{
    static const LPCWSTR headings[][4] = {
        { L"车库", L"停放载具", L"数量", L"锁定" },
        { L"秘籍名称", L"分组", L"代码", NULL },
        { L"传送地点", L"分组", NULL, NULL },
        { L"快捷键名称", L"按键", L"状态", NULL }
    };
    static const int widths[][4] = {
        { 174, 161, 61, 65 }, { 224, 122, 116, 0 }, { 178, 103, 0, 0 },
        { 274, 125, 62, 0 }
    };
    int panel = page->index - PAGE_GARAGES;
    ListState *state = StateFor(page);
    LVCOLUMNW column = { 0 };
    int index, x = 12, y = 44, width = 490, height = 462;
    if (page->index == PAGE_LOCATIONS) {
        x = 605; y = 56; width = 301; height = 154;
    } else if (page->index == PAGE_GARAGES) {
        height = 351;
    } else if (page->index == PAGE_SHORTCUTS) {
        height = 401;
    }
    state->list = UiControl(page, WC_LISTVIEWW, L"",
                            LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
                            WS_EX_CLIENTEDGE, ID_LIST, x, y, width, height);
    SendMessageW(state->list, CCM_SETUNICODEFORMAT, TRUE, 0);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT);
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    for (index = 0; index < 4 && headings[panel][index]; ++index) {
        column.pszText = (LPWSTR)headings[panel][index];
        column.cx = UiScale(page, widths[panel][index]);
        column.iSubItem = index;
        SendMessageW(state->list, LVM_INSERTCOLUMNW, index, (LPARAM)&column);
    }
}

void BuildListPage(UiPage *page)
{
    WCHAR *source;
    listPages[page->index - PAGE_GARAGES] = page;
    CreateList(page);
    if (page->index == PAGE_GARAGES) {
        BuildGarages(page);
    } else {
        source = ResourceText(dataResources[page->index - PAGE_CHEATS]);
        if (!LoadRecords(page->index, source, TRUE))
            page->failed = TRUE;
        free(source);
        if (page->index == PAGE_CHEATS)
            BuildCheats(page);
        else if (page->index == PAGE_LOCATIONS)
            BuildLocations(page);
        else
            BuildShortcuts(page);
        RefreshFolders(page, NULL);
    }
    RefreshRows(page, -1);
}

static void GarageText(int source, int column, WCHAR *text, size_t capacity)
{
    const Garage *garage = &garageData[source];
    int count = 0, first = 0, i;
    switch (column) {
    case 0:
        StringCchCopyW(text, capacity, garageNames[source]);
        break;
    case 1:
    case 2:
        for (i = 0; i < SLOT_COUNT; ++i) {
            if (garage->slots[i].field[1]) {
                if (!first)
                    first = (int)garage->slots[i].field[1];
                ++count;
            }
        }
        if (column == 1)
            StringCchCopyW(text, capacity, first ? UiVehicleName(first) : L"空");
        else
            StringCchPrintfW(text, capacity, L"%d / 4", count);
        break;
    case 3:
        StringCchCopyW(text, capacity, garage->locked ? L"已锁定" : L"未锁定");
        break;
    }
}

static void RecordText(UiPage *page, int source, int column, WCHAR *text, size_t capacity)
{
    const Record *entry = &RecordsFor(page->index)->rows[source];
    LPCWSTR value = L"";
    if (column == 0)
        value = entry->field[0];
    else if (page->index == PAGE_SHORTCUTS) {
        if (column == 1) {
            LPCWSTR modifier = entry->modifier == VK_CONTROL ? L"Ctrl+" : entry->modifier == VK_MENU ? L"Alt+" : L"";
            StringCchPrintfW(text, capacity, L"%ls%ls", modifier, entry->field[2]);
            return;
        }
        if (column == 2)
            value = entry->enabled ? L"启用" : L"停用";
    } else if (column == 1)
        value = DisplayFolder(entry->field[1], page->index);
    else if (column == 2)
        value = entry->field[2];
    StringCchCopyW(text, capacity, value);
}

static void ListContextMenu(UiPage *page)
{
    HMENU menu = CreatePopupMenu();
    POINT cursor;
    int command;
    if (!menu)
        return;
    AppendMenuW(menu, MF_STRING, ID_APPLY, L"应用详情修改");
    AppendMenuW(menu, MF_STRING, ID_NEW, L"新增记录");
    AppendMenuW(menu, MF_STRING, ID_DELETE, L"删除所选记录");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_SHORTCUT_FOLDER_MOVE, L"移动到详情中的分组");
    if (page->index == PAGE_LOCATIONS)
        AppendMenuW(menu, MF_STRING, ID_SHOW_ON_MAP, L"在地图上显示");
    if (page->index == PAGE_SHORTCUTS) {
        AppendMenuW(menu, MF_STRING, ID_GROUP_ENABLE, L"启用当前分组");
        AppendMenuW(menu, MF_STRING, ID_GROUP_DISABLE, L"停用当前分组");
    }
    GetCursorPos(&cursor);
    command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0,
                             page->window, NULL);
    DestroyMenu(menu);
    if (command)
        ListPageCommand(page, command, BN_CLICKED, GetDlgItem(page->window, command));
}

LRESULT ListPageNotify(UiPage *page, NMHDR *notification)
{
    ListState *state = StateFor(page);
    NMLVDISPINFOW *display;
    int row;
    if (notification->hwndFrom != state->list)
        return 0;
    switch (notification->code) {
    case LVN_GETDISPINFOW:
        display = (NMLVDISPINFOW *)notification;
        row = display->item.iItem;
        if (!(display->item.mask & LVIF_TEXT) || !display->item.pszText || display->item.cchTextMax <= 0
            || row < 0 || row >= state->count)
            break;
        display->item.pszText[0] = L'\0';
        if (page->index == PAGE_GARAGES)
            GarageText(state->sourceRows[row], display->item.iSubItem,
                       display->item.pszText, display->item.cchTextMax);
        else
            RecordText(page, state->sourceRows[row], display->item.iSubItem,
                       display->item.pszText, display->item.cchTextMax);
        break;
    case LVN_ITEMCHANGED:
        if ((((NMLISTVIEW *)notification)->uOldState ^ ((NMLISTVIEW *)notification)->uNewState) & LVIS_SELECTED)
            DisplaySelection(page);
        break;
    case LVN_ODSTATECHANGED:
        DisplaySelection(page);
        break;
    case NM_RCLICK:
        if (page->index != PAGE_GARAGES)
            ListContextMenu(page);
        break;
    case NM_DBLCLK:
        if (page->index == PAGE_LOCATIONS)
            ListPageCommand(page, ID_SHOW_ON_MAP, BN_CLICKED, GetDlgItem(page->window, ID_SHOW_ON_MAP));
        break;
    }
    return 0;
}

static BOOL CopyCheat(UiPage *page)
{
    WCHAR code[FIELD_LENGTH];
    HGLOBAL data;
    LPVOID text;
    size_t bytes;
    HWND owner = GetAncestor(page->window, GA_ROOT);
    GetWindowTextW(StateFor(page)->field[2], code, ARRAYSIZE(code));
    if (!code[0] || !OpenClipboard(owner))
        return FALSE;
    bytes = (wcslen(code) + 1) * sizeof(WCHAR);
    data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (data) {
        text = GlobalLock(data);
        if (text) {
            memcpy(text, code, bytes);
            GlobalUnlock(data);
            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data)) {
                CloseClipboard();
                return TRUE;
            }
        }
        GlobalFree(data);
    }
    CloseClipboard();
    return FALSE;
}

static BOOL CaptureLocationInputs(ListState *state, WCHAR position[4][FIELD_LENGTH], POINT *point)
{
    int index;
    double value;
    for (index = 0; index < 4; ++index) {
        GetWindowTextW(state->field[index + 2], position[index], FIELD_LENGTH);
        if (!ReadNumber(position[index], &value))
            return FALSE;
    }
    return ReadPoint(position[0], position[1], point);
}

static void SetPointInputs(ListState *state, POINT coordinate)
{
    WCHAR text[24];
    StringCchPrintfW(text, ARRAYSIZE(text), L"%ld", coordinate.x);
    SetWindowTextW(state->field[2], text);
    StringCchPrintfW(text, ARRAYSIZE(text), L"%ld", coordinate.y);
    SetWindowTextW(state->field[3], text);
}

static BOOL HandleLocation(UiPage *page, int id, int code, HWND control)
{
    ListState *state = StateFor(page);
    POINT coordinate = { 0, 0 };
    WCHAR positions[4][FIELD_LENGTH];
    int i;
    if (code == CBN_SELCHANGE && id == ID_ZOOM) {
        SetMapZoom(ComboValue(control), NULL);
    } else if (code == CBN_SELCHANGE && id == ID_MARKER_SIZE) {
        mapView.markerSize = ComboValue(control);
        mapView.dirty = TRUE;
        InvalidateRect(mapView.window, NULL, FALSE);
    } else if (code != BN_CLICKED) {
        return FALSE;
    } else if (id == ID_CENTER) {
        CenterLocationMap(coordinate);
    } else if (id == ID_MARKERS) {
        mapView.showMarkers = Checked(control);
        mapView.dirty = TRUE;
        InvalidateRect(mapView.window, NULL, FALSE);
    } else if (id == ID_SHOW_ON_MAP) {
        GetWindowTextW(state->field[2], positions[0], FIELD_LENGTH);
        GetWindowTextW(state->field[3], positions[1], FIELD_LENGTH);
        if (ReadPoint(positions[0], positions[1], &coordinate)) {
            SetManualPoint(coordinate, TRUE);
            Feedback(state, L"已在地图上显示输入坐标。");
        } else {
            Feedback(state, L"请输入有效的 X、Y 坐标。");
        }
    } else if (id == ID_READ_MAP) {
        if (mapView.hasManual) {
            SetPointInputs(state, mapView.manual);
            Feedback(state, L"已读取地图上的 X 和 Y 坐标。");
        } else {
            Feedback(state, L"请先在地图中选择坐标。");
        }
    } else if (id == ID_TELEPORT) {
        if (CaptureLocationInputs(state, positions, &coordinate)) {
            memcpy(previewPosition, positions, sizeof(previewPosition));
            mapView.player = coordinate;
            mapView.hasPlayer = TRUE;
            CenterLocationMap(coordinate);
            Feedback(state, L"已更新红色预览位置。");
        } else {
            Feedback(state, L"请填写有效的坐标和朝向。");
        }
    } else if (id == ID_READ_PLAYER) {
        for (i = 0; i < 4; ++i)
            SetWindowTextW(state->field[i + 2], previewPosition[i]);
        Feedback(state, L"已读取预览位置和朝向。");
    } else {
        return FALSE;
    }
    return TRUE;
}

static void BrowseConfiguration(UiPage *page, BOOL save)
{
    ListState *state = StateFor(page);
    WCHAR path[MAX_PATH], *data;
    BOOL success;
    if (!ChooseConfigFile(page, save, path))
        return;
    if (save) {
        success = SaveConfiguration(page->index, path);
        Feedback(state, success ? L"配置已保存。" : L"无法保存所选配置文件。");
    } else {
        data = ReadDataFile(path);
        success = page->index == PAGE_GARAGES ? LoadGarageText(data, garageData)
                                              : LoadRecords(page->index, data, FALSE);
        free(data);
        if (success) {
            if (page->index != PAGE_GARAGES) {
                ++collectionRevision[page->index - PAGE_CHEATS];
                state->folderCount = 0;
                RefreshFolders(page, NULL);
            }
            RefreshRows(page, -1);
        }
        Feedback(state, success ? L"已读取所选配置。" : L"配置文件无法读取，现有内容已保留。");
    }
}

static void ResetMods(GarageSlot *slot)
{
    int index;
    for (index = 9; index < 24; ++index)
        slot->field[index] = 0xFFFF;
    for (index = 24; index < GARAGE_VALUE_COUNT; ++index)
        slot->field[index] = 0;
    slot->field[30] = 0xFF;
}

static BOOL HandleGarage(UiPage *page, int id, int code, HWND control)
{
    ListState *state = StateFor(page);
    int index, model, previous, primary, secondary, mods[UI_VEHICLE_MOD_SLOTS] = { 0 };
    int garage = SelectedSource(state);
    WCHAR name[FIELD_LENGTH];
    GarageSlot *slot;
    HWND owner = GetAncestor(page->window, GA_ROOT);
    if (garage < 0)
        return FALSE;
    slot = &garageData[garage].slots[state->garageSlot];
    model = (int)slot->field[1];
    primary = (int)slot->field[6];
    secondary = (int)slot->field[7];
    if (code == CBN_SELCHANGE && id == ID_GARAGE_SLOT) {
        state->garageSlot = max(0, ComboValue(control));
    } else if (code != BN_CLICKED) {
        return FALSE;
    } else if (id == ID_GARAGE_VEHICLE) {
        previous = model;
        if (UiChooseVehicle(owner, &model, name, ARRAYSIZE(name))) {
            slot->field[1] = model;
            if (model != previous) {
                if (UiVehicleDefaultColors(model, &primary, &secondary)) {
                    slot->field[6] = primary;
                    slot->field[7] = secondary;
                }
                ResetMods(slot);
            }
        }
    } else if (id == ID_GARAGE_COLORS) {
        if (UiChooseVehicleColors(owner, &primary, &secondary)) {
            slot->field[6] = primary;
            slot->field[7] = secondary;
        }
    } else if (id == ID_GARAGE_MODS) {
        if (slot->field[30] < 4)
            mods[0] = (int)slot->field[30] + 1;
        for (index = 0; index < 15; ++index)
            if (slot->field[index + 9] != 0xFFFF)
                mods[index + 1] = (int)slot->field[index + 9];
        if (UiEditVehicleMods(owner, model, mods)) {
            slot->field[30] = mods[0] ? mods[0] - 1 : 0xFF;
            for (index = 0; index < 15; ++index)
                slot->field[index + 9] = mods[index + 1] ? (DWORD)mods[index + 1] : 0xFFFF;
        }
    } else if (id == ID_GARAGE_CLEAR_MODS) {
        ResetMods(slot);
    } else if (id == ID_ENABLED) {
        garageData[garage].locked = Checked(control);
    } else if (id >= ID_GARAGE_PROTECTION && id < ID_GARAGE_PROTECTION + 4) {
        slot->field[id - ID_GARAGE_PROTECTION + 2] = Checked(control);
    } else if (id == ID_GARAGE_READ_PREVIEW) {
        memcpy(garageData, mockGarages, sizeof(garageData));
        Feedback(state, L"已从离线预览读取 17 座车库。");
    } else if (id == ID_GARAGE_WRITE_PREVIEW) {
        memcpy(mockGarages, garageData, sizeof(mockGarages));
        Feedback(state, L"已将 17 座车库写入离线预览。");
    } else {
        return FALSE;
    }
    GarageStatus(page);
    if (state->count)
        ListView_RedrawItems(state->list, 0, state->count - 1);
    return TRUE;
}

static BOOL RecordFromInputs(UiPage *page, Record *row)
{
    ListState *state = StateFor(page);
    WCHAR folder[FIELD_LENGTH], name[32];
    const RecordSet *targets;
    double coordinate;
    int i, selected;
    for (i = 0; i < (int)ARRAYSIZE(state->field); ++i) {
        if (state->field[i]) {
            GetWindowTextW(state->field[i], row->field[i], FIELD_LENGTH);
            if (wcschr(row->field[i], L'|')) {
                Feedback(state, L"记录字段不能包含竖线。");
                return FALSE;
            }
        }
    }
    if (!row->field[0][0]) {
        Feedback(state, L"请输入记录名称。");
        SetFocus(state->field[0]);
        return FALSE;
    }
    CopyField(folder, row->field[1]);
    FullFolder(row->field[1], folder, page->index);
    if (page->index == PAGE_CHEATS && !row->field[2][0]) {
        Feedback(state, L"请输入秘籍代码。");
        return FALSE;
    }
    if (page->index == PAGE_LOCATIONS) {
        RecordPoint(row);
        if (!row->validPoint || !ReadNumber(row->field[4], &coordinate) || !ReadNumber(row->field[5], &coordinate)) {
            Feedback(state, L"请填写有效的坐标和朝向。");
            return FALSE;
        }
    } else if (page->index == PAGE_SHORTCUTS) {
        row->key = ComboValue(state->key);
        row->category = ComboValue(state->category);
        row->modifier = Checked(state->ctrl) ? VK_CONTROL : Checked(state->alt) ? VK_MENU : 0;
        row->parameterPage = ShortcutParameterPage(state);
        row->enabled = Checked(state->enabled);
        selected = ComboValue(state->command);
        if (row->key <= 0 || selected < 0 || row->category < 0 || row->category > 2) {
            Feedback(state, L"请选择命令或地点，并绑定一个按键。");
            return FALSE;
        }
        if (row->category == 0) {
            StringCchPrintfW(row->command, FIELD_LENGTH, L"%d", selected);
        } else {
            targets = RecordsFor(row->category == 1 ? PAGE_CHEATS : PAGE_LOCATIONS);
            if (selected >= targets->count)
                return FALSE;
            CopyField(row->command, targets->rows[selected].uid);
        }
        CopyField(row->field[2], KeyName(row->key, name));
        if (row->parameterPage == 17)
            CopyField(row->field[3], L"0");
    }
    return TRUE;
}

static void ChangeFolder(UiPage *page, int action)
{
    ListState *state = StateFor(page);
    RecordSet *set = RecordsFor(page->index);
    WCHAR current[FIELD_LENGTH], requested[FIELD_LENGTH], replacement[FIELD_LENGTH], converted[FIELD_LENGTH];
    WCHAR *last;
    size_t length;
    int index, folderIndex, retained, modified = 0;
    CurrentFolder(page, current);
    GetWindowTextW(state->field[1], requested, FIELD_LENGTH);
    if (action != ID_DELETE_GROUP &&
        (!requested[0] || wcschr(requested, L'|') || wcslen(requested) >= FIELD_LENGTH - 22)) {
        Feedback(state, L"请在分组输入框填写目标分组路径。");
        SetFocus(state->field[1]);
        return;
    }
    if (action == ID_NEW_GROUP) {
        FullFolder(replacement, requested, page->index);
        if (EnsureFolder(state, replacement) < 0) {
            Feedback(state, L"分组列表已满。");
            return;
        }
        RefreshFolders(page, replacement);
        RefreshRows(page, -1);
        SetWindowTextW(state->field[1], DisplayFolder(replacement, page->index));
        Feedback(state, L"已建立分组，可在此分组中新增记录。");
        return;
    }
    if (!current[0]) {
        Feedback(state, L"请先从上方选择一个分组。");
        return;
    }
    if (action == ID_DELETE_GROUP) {
        if (MessageBoxW(GetAncestor(page->window, GA_ROOT), L"删除分组并将记录移至上一级？",
                        L"删除分组", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
        CopyField(replacement, current);
        last = wcsrchr(replacement, L'\\');
        if (last)
            *last = L'\0';
    } else {
        FullFolder(replacement, requested, page->index);
        if (IsUnderFolder(replacement, current)) {
            Feedback(state, L"请选择该分组之外的目标路径。");
            return;
        }
    }

    length = wcslen(current);
    for (index = 0; index < set->count; ++index) {
        if (IsUnderFolder(set->rows[index].field[1], current)) {
            if (FAILED(StringCchPrintfW(converted, ARRAYSIZE(converted), L"%ls%ls",
                         replacement, set->rows[index].field[1] + length))) {
                Feedback(state, L"目标分组路径过长。");
                return;
            }
        }
    }
    for (index = 0; index < set->count; ++index) {
        if (IsUnderFolder(set->rows[index].field[1], current)) {
            StringCchPrintfW(converted, ARRAYSIZE(converted), L"%ls%ls", replacement,
                             set->rows[index].field[1] + length);
            CopyField(set->rows[index].field[1], converted);
            ++modified;
        }
    }
    for (folderIndex = 0, retained = 0; folderIndex < state->folderCount; ++folderIndex) {
        if (!IsUnderFolder(state->folders[folderIndex], current)) {
            if (retained != folderIndex)
                CopyField(state->folders[retained], state->folders[folderIndex]);
            ++retained;
        }
    }
    state->folderCount = retained;
    if (action == ID_RENAME_GROUP)
        EnsureFolder(state, replacement);
    if (modified)
        ++collectionRevision[page->index - PAGE_CHEATS];
    RefreshFolders(page, replacement);
    RefreshRows(page, -1);
    Feedback(state, action == ID_DELETE_GROUP ? L"已删除分组；记录移动到了上一级。" : L"已更新分组路径。");
}

static void EditRecord(UiPage *page, int action)
{
    ListState *state = StateFor(page);
    RecordSet *set = RecordsFor(page->index);
    int index = action == ID_NEW ? set->count : SelectedSource(state);
    Record entry = { 0 };
    WCHAR selectedFolder[FIELD_LENGTH], newGroup[FIELD_LENGTH];
    if (index < 0) {
        Feedback(state, L"请先在列表中选择一条记录。");
        return;
    }
    if (index == MAX_ROWS) {
        Feedback(state, L"记录列表已满。");
        return;
    }
    CurrentFolder(page, selectedFolder);
    if (action == ID_DELETE) {
        if (MessageBoxW(GetAncestor(page->window, GA_ROOT), L"删除当前选中的记录？", L"删除记录",
                        MB_YESNO | MB_ICONQUESTION) != IDYES)
            return;
        memmove(&set->rows[index], &set->rows[index + 1], (set->count - index - 1) * sizeof(entry));
        --set->count;
        index = min(index, set->count - 1);
    } else {
        if (action != ID_NEW)
            entry = set->rows[index];
        if (action == ID_SHORTCUT_FOLDER_MOVE) {
            GetWindowTextW(state->field[1], newGroup, ARRAYSIZE(newGroup));
            if (wcschr(newGroup, L'|')) {
                Feedback(state, L"分组路径不能包含竖线。");
                return;
            }
            FullFolder(entry.field[1], newGroup, page->index);
            CopyField(selectedFolder, entry.field[1]);
        } else if (!RecordFromInputs(page, &entry)) {
            return;
        }
        if (action == ID_NEW) {
            if (!CreateUid(entry.uid)) {
                Feedback(state, L"无法为这条记录生成标识。");
                return;
            }
            ++set->count;
        }
        set->rows[index] = entry;
        if (selectedFolder[0] && !IsUnderFolder(entry.field[1], selectedFolder))
            CopyField(selectedFolder, entry.field[1]);
    }
    ++collectionRevision[page->index - PAGE_CHEATS];
    RefreshFolders(page, selectedFolder);
    RefreshRows(page, index);
    Feedback(state, action == ID_DELETE ? L"记录已删除。" : action == ID_NEW ? L"记录已新增。" : L"修改已应用。");
}

static void ToggleShortcutGroup(UiPage *page, BOOL enabled)
{
    ListState *state = StateFor(page);
    RecordSet *set = RecordsFor(page->index);
    int source = SelectedSource(state), index;
    for (index = 0; index < state->count; ++index)
        set->rows[state->sourceRows[index]].enabled = enabled;
    RefreshRows(page, source);
    Feedback(state, enabled ? L"已启用当前视图中的全部快捷键。" : L"已停用当前视图中的全部快捷键。");
}

static void ShortcutParameters(UiPage *page)
{
    ListState *state = StateFor(page);
    HWND owner = GetAncestor(page->window, GA_ROOT);
    WCHAR input[FIELD_LENGTH], name[FIELD_LENGTH];
    int parameter = ShortcutParameterPage(state), primary = 0, secondary = 0, model = 411;
    GetWindowTextW(state->field[3], input, FIELD_LENGTH);
    if (parameter == 10) {
        WCHAR *colors[4];
        if (SplitFields(input, L';', colors, 4)) {
            primary = _wtoi(colors[0]);
            secondary = _wtoi(colors[2]);
        }
        if (UiChooseVehicleColors(owner, &primary, &secondary)) {
            StringCchPrintfW(input, ARRAYSIZE(input), L"%d;0;%d;0", primary, secondary);
            SetWindowTextW(state->field[3], input);
        }
    } else if (parameter == 12) {
        if (*input)
            model = _wtoi(input);
        if (UiChooseVehicle(owner, &model, name, ARRAYSIZE(name))) {
            StringCchPrintfW(input, ARRAYSIZE(input), L"%d", model);
            SetWindowTextW(state->field[3], input);
            SetWindowTextW(state->field[4], name);
        }
    }
}

static void PreviewShortcut(UiPage *page)
{
    const ListState *state = StateFor(page);
    WCHAR command[FIELD_LENGTH], key[32], message[FIELD_LENGTH + 48], binding[32];
    int selected = (int)SendMessageW(state->command, CB_GETCURSEL, 0, 0);
    if (selected < 0 || ComboValue(state->key) < 0
        || SendMessageW(state->command, CB_GETLBTEXTLEN, selected, 0) >= ARRAYSIZE(command)) {
        Feedback(StateFor(page), L"请先选择命令与按键。");
        return;
    }
    SendMessageW(state->command, CB_GETLBTEXT, selected, (LPARAM)command);
    StringCchCopyW(binding, ARRAYSIZE(binding), KeyName(ComboValue(state->key), key));
    StringCchPrintfW(message, ARRAYSIZE(message), L"%ls%ls → %ls",
        Checked(state->ctrl) ? L"Ctrl+" : Checked(state->alt) ? L"Alt+" : L"", binding, command);
    UiSetStatus(page, message);
    Feedback(StateFor(page), L"已在状态栏显示快捷键预览。");
}

static BOOL HandleShortcut(UiPage *page, int id, int code)
{
    ListState *state = StateFor(page);
    if (code == CBN_SELCHANGE && id == ID_SHORTCUT_CATEGORY) {
        PrepareCommandCombo(state, ComboValue(state->category));
        SendMessageW(state->command, CB_SETCURSEL, 0, 0);
        SetWindowTextW(state->field[3], L"");
        SetWindowTextW(state->field[4], L"");
        UpdateParameterHint(state);
    } else if (code == CBN_SELCHANGE && id == ID_SHORTCUT_COMMAND) {
        SetWindowTextW(state->field[3], L"");
        SetWindowTextW(state->field[4], L"");
        UpdateParameterHint(state);
    } else if (code != BN_CLICKED) {
        return FALSE;
    } else if (id == ID_SHORTCUT_ALT || id == ID_SHORTCUT_CTRL) {
        if (id == ID_SHORTCUT_CTRL && Checked(state->ctrl))
            SetChecked(state->alt, FALSE);
        if (id == ID_SHORTCUT_ALT && Checked(state->alt))
            SetChecked(state->ctrl, FALSE);
    } else if (id == ID_GROUP_ENABLE || id == ID_GROUP_DISABLE) {
        ToggleShortcutGroup(page, id == ID_GROUP_ENABLE);
    } else if (id == ID_SHORTCUT_TEST) {
        PreviewShortcut(page);
    } else if (id == ID_SHORTCUT_PARAMETER_PICK) {
        ShortcutParameters(page);
    } else {
        return FALSE;
    }
    return TRUE;
}

BOOL ListPageCommand(UiPage *page, int id, int code, HWND control)
{
    ListState *state = StateFor(page);
    if (id == ID_GROUP && code == CBN_SELCHANGE) {
        state->garageSlot = 0;
        RefreshRows(page, -1);
        Feedback(state, L"");
        return TRUE;
    }
    if (code == BN_CLICKED && (id == ID_IMPORT || id == ID_EXPORT)) {
        BrowseConfiguration(page, id == ID_EXPORT);
        return TRUE;
    }
    if (page->index == PAGE_GARAGES)
        return HandleGarage(page, id, code, control);
    if (page->index == PAGE_LOCATIONS && HandleLocation(page, id, code, control))
        return TRUE;
    if (page->index == PAGE_SHORTCUTS && HandleShortcut(page, id, code))
        return TRUE;
    if (code != BN_CLICKED)
        return FALSE;
    switch (id) {
    case ID_RESET:
        DisplaySelection(page);
        Feedback(state, L"已还原输入。");
        return TRUE;
    case ID_APPLY:
    case ID_NEW:
    case ID_DELETE:
    case ID_SHORTCUT_FOLDER_MOVE:
        EditRecord(page, id);
        return TRUE;
    case ID_NEW_GROUP:
    case ID_RENAME_GROUP:
    case ID_DELETE_GROUP:
        ChangeFolder(page, id);
        return TRUE;
    case ID_CHEAT_COPY:
        Feedback(state, CopyCheat(page) ? L"已复制秘籍代码。" : L"没有可复制的代码，或剪贴板正忙。");
        return TRUE;
    default:
        return FALSE;
    }
}
