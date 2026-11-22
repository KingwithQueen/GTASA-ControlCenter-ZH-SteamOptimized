#include "ui.h"

enum {
    GROUP_WIDTH = 442,
    LEFT_COLUMN = 12,
    RIGHT_COLUMN = 466,
    ROW_HEIGHT = 26,
    STAT_FIRST_ID = 400,
    STAT_LAST_ID = 499,
    ACTION_ID = 500
};

typedef struct StatSpec {
    LPCWSTR name;
    int maximum;
    int initial;
} StatSpec;

static const StatSpec vehicleStats[] = {
    { L"车辆耐久", 1000, 1000 }, { L"车辆重量（吨）", 100, 50 },
    { L"车辆速度（%）", 300, 100 }, { L"X 轴速度（%）", 200, 0 },
    { L"Y 轴速度（%）", 200, 0 }, { L"Z 轴速度（%）", 200, 0 },
    { L"X 轴旋转（%）", 200, 0 }, { L"Y 轴旋转（%）", 200, 0 },
    { L"Z 轴旋转（%）", 200, 0 }, { L"车辆朝向（度）", 359, 0 }
};
static const StatSpec gameStats[] = {
    { L"游戏速度（%）", 200, 100 }, { L"时间流速（%）", 200, 100 },
    { L"小时", 23, 12 }, { L"分钟", 59, 0 },
    { L"任务显示计时", 600, 120 }, { L"已击杀罪犯", 999, 0 }
};
static const StatSpec playerStats[] = {
    { L"生命值", 100, 100 }, { L"护甲值", 100, 50 },
    { L"通缉等级", 6, 0 }, { L"金钱", 9999999, 5000 },
    { L"耐力", 1000, 400 }, { L"肌肉", 1000, 250 },
    { L"脂肪", 1000, 100 }, { L"性感度", 1000, 300 },
    { L"威望", 1000, 300 }, { L"肺活量", 1000, 400 }
};
static const StatSpec weaponStats[] = {
    { L"弹匣容量", 999, 30 }, { L"携带弹药", 99999, 120 },
    { L"武器熟练度", 1000, 300 }, { L"驾驶技能", 1000, 400 },
    { L"摩托车技能", 1000, 400 }, { L"飞行技能", 1000, 400 },
    { L"自行车技能", 1000, 400 }, { L"玩家朝向（度）", 359, 0 }
};
static const StatSpec relationshipStats[] = {
    { L"丹妮丝", 100, 50 }, { L"海伦娜", 100, 32 },
    { L"米歇尔", 100, 42 }, { L"凯蒂", 100, 20 },
    { L"芭芭拉", 100, 30 }, { L"米莉", 100, 25 }
};

static const LPCWSTR vehicleFlags[] = {
    L"防弹", L"防火", L"防爆", L"防碰撞", L"防近战", L"全伤害防护",
    L"锁定车门", L"冻结车辆", L"完美操控", L"保持引擎运转", L"固定主色", L"固定副色"
};
static const LPCWSTR gameFlags[] = {
    L"全车氮气", L"汽车飞行", L"船只飞行", L"减少交通流量", L"自行车大跳",
    L"冻结任务计时", L"冻结时钟", L"锁定天气", L"一击必杀", L"游戏画面反馈"
};
static const LPCWSTR playerFlags[] = {
    L"防弹", L"防火", L"防爆", L"防碰撞", L"防近战", L"全伤害防护",
    L"不会饥饿", L"无限氧气", L"无限奔跑", L"免疫通缉", L"一击必杀", L"Hot Coffee（脚本）"
};
static const LPCWSTR vehicleModels[] = { L"Elegy", L"Sultan", L"Infernus", L"Hydra", L"Sanchez", L"Jetmax" };
static const LPCWSTR colors[] = { L"黑色", L"白色", L"红色", L"蓝色", L"绿色", L"银色" };
static const LPCWSTR weather[] = { L"晴朗", L"多云", L"下雨", L"雾天", L"沙尘暴", L"雷雨" };
static const LPCWSTR weapons[] = { L"手枪", L"沙漠之鹰", L"战斗霰弹枪", L"冲锋枪", L"AK-47", L"狙击步枪" };

static void Label(UiPage *page, LPCWSTR text, int x, int y, int width)
{
    UiControl(page, WC_STATICW, text, SS_CENTERIMAGE, 0, 0, x, y, width, 22);
}

static void Group(UiPage *page, LPCWSTR title, int x, int y, int height)
{
    HWND group = UiControl(page, WC_BUTTONW, title, BS_GROUPBOX | WS_CLIPSIBLINGS,
                           0, 0, x, y, GROUP_WIDTH, height);
    if (group)
        SetWindowPos(group, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void SetStatText(HWND edit, UINT value)
{
    WCHAR text[16];
    wsprintfW(text, L"%u", value);
    SetWindowTextW(edit, text);
}

static void Stats(UiPage *page, LPCWSTR title, int x, int y,
                  const StatSpec *spec, int count, int firstRow)
{
    int row;
    Group(page, title, x, y, 24 + count * ROW_HEIGHT + 6);
    for (row = 0; row < count; ++row) {
        int top = y + 20 + row * ROW_HEIGHT;
        int sliderId = STAT_FIRST_ID + (row + firstRow) * 4;
        HWND slider, edit;

        Label(page, spec[row].name, x + 12, top, 110);
        slider = UiControl(page, TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
                           0, sliderId, x + 119, top, 138, 22);
        SendMessageW(slider, TBM_SETRANGEMAX, FALSE, spec[row].maximum);
        SendMessageW(slider, TBM_SETPOS, TRUE, spec[row].initial);
        edit = UiControl(page, WC_EDITW, L"", WS_TABSTOP | ES_CENTER | ES_NUMBER | ES_AUTOHSCROLL,
                         WS_EX_CLIENTEDGE, sliderId + 1, x + 264, top, 56, 22);
        SendMessageW(edit, EM_SETLIMITTEXT, 7, 0);
        SetStatText(edit, (UINT)spec[row].initial);
        UiControl(page, WC_BUTTONW, L"最大", WS_TABSTOP | BS_PUSHBUTTON, 0,
                  sliderId + 2, x + 327, top, 49, 22);
        UiControl(page, WC_BUTTONW, L"锁定", WS_TABSTOP | BS_AUTOCHECKBOX, 0,
                  sliderId + 3, x + 385, top, 49, 22);
    }
}

static void Flags(UiPage *page, LPCWSTR title, int x, int y,
                  const LPCWSTR *flags, int count, int columns)
{
    int index;
    int countRows = (count + columns - 1) / columns;
    int columnWidth = (GROUP_WIDTH - 24) / columns;

    Group(page, title, x, y, 22 + 24 * countRows + 5);
    for (index = 0; index < count; ++index)
        UiControl(page, WC_BUTTONW, flags[index], WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0,
                  x + 12 + index % columns * columnWidth, y + 20 + index / columns * 24,
                  columnWidth - 4, 22);
}

static void Choice(UiPage *page, LPCWSTR label, const LPCWSTR *options,
                   int count, int x, int y)
{
    int index;
    HWND combo;

    Label(page, label, x, y, 76);
    combo = UiControl(page, WC_COMBOBOXW, L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                      0, 0, x + 76, y, 125, 192);
    for (index = 0; index < count; ++index)
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)options[index]);
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

static void Action(UiPage *page, LPCWSTR label, int x, int y, int width)
{
    UiControl(page, WC_BUTTONW, label, WS_TABSTOP | BS_PUSHBUTTON, 0,
              ACTION_ID, x, y, width, 25);
}

static void BuildVehiclePage(UiPage *page)
{
    Stats(page, L"载具状态与运动", LEFT_COLUMN, 8, vehicleStats, ARRAYSIZE(vehicleStats), 0);
    Stats(page, L"时间与任务", RIGHT_COLUMN, 8, gameStats, ARRAYSIZE(gameStats), 10);
    Flags(page, L"载具保护与操控", LEFT_COLUMN, 306, vehicleFlags, ARRAYSIZE(vehicleFlags), 3);
    Flags(page, L"世界选项", RIGHT_COLUMN, 202, gameFlags, ARRAYSIZE(gameFlags), 2);

    Group(page, L"载具选择", LEFT_COLUMN, 438, 84);
    Choice(page, L"载具型号", vehicleModels, ARRAYSIZE(vehicleModels), 24, 460);
    Label(page, L"车牌", 245, 460, 43);
    UiControl(page, WC_EDITW, L"SAN ANDR", WS_TABSTOP | ES_AUTOHSCROLL,
              WS_EX_CLIENTEDGE, 0, 288, 460, 148, 22);
    Action(page, L"生成载具", 24, 489, 96);
    Action(page, L"翻转载具", 127, 489, 96);
    Action(page, L"修复载具", 230, 489, 96);
    Action(page, L"更新车牌", 333, 489, 103);

    Group(page, L"天气与车身颜色", RIGHT_COLUMN, 356, 117);
    Choice(page, L"当前天气", weather, ARRAYSIZE(weather), 478, 379);
    Choice(page, L"车身主色", colors, ARRAYSIZE(colors), 478, 410);
    Choice(page, L"车身副色", colors, ARRAYSIZE(colors), 692, 410);
    Action(page, L"应用天气", 692, 378, 108);
    Action(page, L"应用车身颜色", 478, 439, 132);
}

static void BuildPlayerPage(UiPage *page)
{
    Stats(page, L"玩家属性", LEFT_COLUMN, 8, playerStats, ARRAYSIZE(playerStats), 0);
    Stats(page, L"弹药与熟练度", RIGHT_COLUMN, 8, weaponStats, ARRAYSIZE(weaponStats), 10);
    Stats(page, L"女友进度", LEFT_COLUMN, 306, relationshipStats, ARRAYSIZE(relationshipStats), 18);
    Flags(page, L"玩家保护与开关", RIGHT_COLUMN, 254, playerFlags, ARRAYSIZE(playerFlags), 2);

    Group(page, L"武器选择", RIGHT_COLUMN, 432, 90);
    Choice(page, L"当前武器", weapons, ARRAYSIZE(weapons), 478, 458);
    Action(page, L"武器熟练度", 692, 456, 108);
    Action(page, L"补满弹药", 478, 489, 94);
    Action(page, L"清除通缉", 579, 489, 94);
    Action(page, L"同步女友进度", 680, 489, 120);
}

void BuildDensePage(UiPage *page)
{
    if (page->index == 0)
        BuildVehiclePage(page);
    else
        BuildPlayerPage(page);
}

void DensePageScroll(HWND slider)
{
    int id = GetDlgCtrlID(slider);
    HWND edit;

    if (id < STAT_FIRST_ID || id > STAT_LAST_ID)
        return;
    edit = GetDlgItem(GetParent(slider), id + 1);
    SetStatText(edit, (UINT)SendMessageW(slider, TBM_GETPOS, 0, 0));
}

BOOL DensePageCommand(UiPage *page, int id, int code, HWND control)
{
    HWND slider;
    UINT value, maximum;
    int field;

    if (id == ACTION_ID && code == BN_CLICKED) {
        WCHAR title[48], status[80];
        GetWindowTextW(control, title, ARRAYSIZE(title));
        wsprintfW(status, L"离线原型 · 已试用「%s」按钮", title);
        UiSetStatus(page, status);
        return TRUE;
    }
    if (id < STAT_FIRST_ID || id > STAT_LAST_ID)
        return FALSE;
    field = (id - STAT_FIRST_ID) % 4;
    if (field == 1 && code == EN_KILLFOCUS) {
        slider = GetDlgItem(page->window, id - 1);
        maximum = (UINT)SendMessageW(slider, TBM_GETRANGEMAX, 0, 0);
        value = GetDlgItemInt(page->window, id, NULL, FALSE);
        SendMessageW(slider, TBM_SETPOS, TRUE, value > maximum ? maximum : value);
        DensePageScroll(slider);
        return TRUE;
    }
    if (field == 2 && code == BN_CLICKED) {
        slider = GetDlgItem(page->window, id - 2);
        SendMessageW(slider, TBM_SETPOS, TRUE, SendMessageW(slider, TBM_GETRANGEMAX, 0, 0));
        DensePageScroll(slider);
        return TRUE;
    }
    return FALSE;
}
