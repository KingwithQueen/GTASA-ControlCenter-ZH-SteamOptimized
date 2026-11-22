#include "ui.h"
#include "dialogs.h"
#include "trainer.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>

enum {
    SECTION_COUNT = 3, SECTION_TOP = 41,
    LEFT_COLUMN = 12, RIGHT_COLUMN = 466, GROUP_WIDTH = 442,
    ID_SECTION_FIRST = 7100, ID_PANE_FIRST = 7110,
    ID_COLOR_PRIMARY = 7120, ID_COLOR_SECONDARY, ID_COLOR_LOCK, ID_PRIMARY_LOCK, ID_SECONDARY_LOCK,
    ID_PLAYER_DAMAGE_FIRE = 7130, ID_CHEAT_FIRE, ID_CHEAT_FIRE_LOCK,
    ID_PLAYER_DAMAGE_LOCK, ID_PLAYER_DAMAGE_EXPLOSION,
    ID_PLAYER_DAMAGE_COLLISION, ID_PLAYER_DAMAGE_BULLET,
    ID_DONT_BURN = 7140, ID_DONT_EXPLODE,
    ID_VEHICLE_DAMAGE_LOCK, ID_VEHICLE_DAMAGE_EXPLOSION, ID_VEHICLE_DAMAGE_COLLISION,
    ID_VEHICLE_DAMAGE_BULLET, ID_VEHICLE_DAMAGE_FIRE, ID_VEHICLE_WHEELS, ID_VEHICLE_DOORS,
    ID_VEHICLE_DOOR_LOCK,
    ID_MODEL = 7150, ID_CURRENT_MODEL, ID_PICK_MODEL, ID_SPAWN_MODEL,
    ID_PLATE, ID_APPLY_PLATE, ID_PLATE_LOCK, ID_SPAWNER_AUTO, ID_SPAWNER_READY, ID_CHECK_SPAWNER,
    ID_VEHICLE_ALARM, ID_VEHICLE_RESTART, ID_VEHICLE_REMOTE,
    ID_WEATHER = 7170, ID_APPLY_WEATHER, ID_WEATHER_LOCK, ID_CLOCK_LABEL,
    ID_APPLY_MONEY, ID_CLEAR_CHEATS, ID_RESYNC, ID_SYNC_RELATIONSHIP, ID_CHEAT_STATUS, ID_WORLD_DAY,
    ID_GAME_FEEDBACK, ID_GAME_VERSION, ID_RELATIONSHIP_DATE, ID_PREPARE_DATE,
    ID_STOP_VEHICLE_SPEED = 7200, ID_STOP_VEHICLE_SPIN,
    ID_FREEZE_VEHICLE, ID_FLIP_VEHICLE, ID_REPAIR_VEHICLE,
    ID_STOP_PED_SPEED, ID_VEHICLE_DIRECTION, ID_VEHICLE_DIRECTION_APPLY,
    ID_VEHICLE_KICK_DIRECTION, ID_VEHICLE_KICK_SPEED, ID_VEHICLE_KICK_APPLY,
    ID_PED_DIRECTION, ID_PED_DIRECTION_APPLY, ID_PED_KICK_DIRECTION, ID_PED_KICK_SPEED, ID_PED_KICK_APPLY,
    ID_CLOCK_STOP, ID_CLOCK_REAL, ID_CLOCK_NORMAL, ID_GAME_SPEED_NORMAL,
    ID_WEAPON_LEVELS = 7250, ID_WEAPON_AVERAGE, ID_AMMO_REFILL, ID_BRASS_KNUCKLES,
    ID_LOADED_LOCK, ID_CURRENT_SLOT, ID_CURRENT_LOADED, ID_CURRENT_TOTAL,
    ID_CURRENT_WEAPON, ID_READ_WEAPONS,
    ID_CHEAT_FIRST = 7300, ID_SAFE_CHEATS = 7364, ID_COFFEE,
    ID_STAT_FIRST = 7400, ID_WEAPON_FIRST = 7600,
    WEAPON_SLOT_COUNT = 11, SLOT_UNARMED = 0, SLOT_DETONATOR = WEAPON_SLOT_COUNT + 1,
    CHEAT_COUNT = 22,
    FIELD_ROW_HEIGHT = 27
};

typedef struct StatSpec {
    LPCWSTR name;
    int minimum, maximum, initial, actionValue;
    LPCWSTR action;
} StatSpec;

enum {
    VEH_HEALTH, VEH_WEIGHT, VEH_FLIGHT, VEH_SPEED_X, VEH_SPEED_Y, VEH_SPEED_Z,
    VEH_SPIN_X, VEH_SPIN_Y, VEH_SPIN_Z, WORLD_TIME, WORLD_CLOCK_SPEED, WORLD_GAME_SPEED,
    WORLD_MONEY, WORLD_MISSION_TIMER, WORLD_CRIMINALS, RELATIONSHIP_FIRST,
    PED_ARMOR = 0, PED_HEALTH, PED_FAT, PED_STAMINA, PED_MUSCLE, PED_LUNG,
    PED_GAMBLING, PED_CAR, PED_MOTORBIKE, PED_BICYCLE, PED_FLIGHT_SKILL,
    PED_SPEED_X, PED_SPEED_Y, PED_SPEED_Z, PED_FLIGHT_ASSIST, PED_WANTED,
    PED_RESPECT, PED_APPEAL,
    STAT_SLOT_COUNT = 24
};

typedef struct WeaponSpec {
    LPCWSTR category;
    LPCWSTR options[9];
    int gameIds[9];
    BOOL hasAmmo;
} WeaponSpec;

typedef struct WeaponSetting {
    int type, total, loaded;
    unsigned editRevision, submittedRevision;
    BOOL dirty, applying;
} WeaponSetting;

typedef struct DenseState {
    HWND pane[SECTION_COUNT];
    int activePane;
    int colors[2], model, money, weather, gameDay;
    int heading, cheatsUsed, loadedLockSlot;
    int currentWeaponSlot, requestedWeaponSlot, unarmedType;
    ULONGLONG weaponReadsPending, weaponReadAgain;
    BOOL weaponRendering;
    BOOL upsideDown, spawnerReady, preventExplosion, liveFailure, liveSession;
    WCHAR plate[9];
    int proficiency[10], relationship[6];
    WeaponSetting weapons[WEAPON_SLOT_COUNT];
} DenseState;

typedef struct GameBinding {
    GameField field;
    int index;
} GameBinding;

typedef struct FlagBinding {
    int id;
    GameField field;
    int index;
} FlagBinding;

static DenseState states[2];
static const WCHAR sectionClass[] = L"GTASAControlCenterDenseSection";

static const StatSpec vehicleStats[] = {
    { L"引擎耐久（%）", 0, 400, 100, 100, L"修复" },
    { L"车辆重量（吨）", 1, 4000, 500, 500, L"50 吨" },
    { L"载具飞行辅助", 0, 1000, 0, 0, L"复位" }
};
static const StatSpec vehicleMotion[] = {
    { L"X 轴速度（%）", -400, 400, 0, 0, L"停止" },
    { L"Y 轴速度（%）", -400, 400, 0, 0, L"停止" },
    { L"Z 轴速度（%）", -400, 400, 0, 0, L"停止" },
    { L"X 轴旋转（%）", -400, 400, 0, 0, L"停止" },
    { L"Y 轴旋转（%）", -400, 400, 0, 0, L"停止" },
    { L"Z 轴旋转（%）", -400, 400, 0, 0, L"停止" }
};
static const StatSpec worldStats[] = {
    { L"游戏时间（分钟）", 0, 1439, 720, 720, L"中午" },
    { L"时钟流速（%）", 0, 1000, 100, 100, L"正常" },
    { L"游戏速度（%）", 10, 1000, 100, 100, L"正常" },
    { L"金钱", 0, 9999999, 5000, 9999999, L"最大" },
    { L"任务显示计时", 0, 9999, 120, 0, L"清零" },
    { L"义警累计击杀", 0, INT_MAX, 0, 0, L"清零" }
};
static const StatSpec relationshipStats[] = {
    { L"丹妮丝", 0, 100, 50, 100, L"最大" },
    { L"米歇尔", 0, 100, 42, 100, L"最大" },
    { L"海伦娜", 0, 100, 32, 100, L"最大" },
    { L"凯蒂", 0, 100, 20, 100, L"最大" },
    { L"芭芭拉", 0, 100, 30, 100, L"最大" },
    { L"米莉", 0, 100, 25, 100, L"最大" }
};
static const StatSpec playerStats[] = {
    { L"护甲值", 0, 1000, 50, 1000, L"最大" },
    { L"生命值", 0, 1000, 100, 1000, L"最大" },
    { L"脂肪", 0, 1000, 100, 1000, L"最大" },
    { L"耐力", 0, 1000, 400, 1000, L"最大" },
    { L"肌肉", 0, 1000, 250, 1000, L"最大" },
    { L"肺活量", 0, 1000, 400, 1000, L"最大" },
    { L"赌博技能", 0, 1000, 100, 1000, L"最大" },
    { L"驾驶技能", 0, 1000, 400, 1000, L"最大" },
    { L"摩托车技能", 0, 1000, 400, 1000, L"最大" },
    { L"自行车技能", 0, 1000, 400, 1000, L"最大" },
    { L"飞行技能", 0, 1000, 400, 1000, L"最大" }
};
static const StatSpec playerMotion[] = {
    { L"X 轴速度（%）", -400, 400, 0, 0, L"停止" },
    { L"Y 轴速度（%）", -400, 400, 0, 0, L"停止" },
    { L"Z 轴速度（%）", -400, 400, 0, 0, L"停止" },
    { L"主角飞行辅助", -100, 100, 0, 0, L"复位" }
};
static const StatSpec otherPlayerStats[] = {
    { L"通缉等级", 0, 6, 0, 0, L"清除" },
    { L"总威望", 0, 1000, 300, 1000, L"最大" },
    { L"总性感度", 0, 1000, 300, 1000, L"最大" }
};
static LPCWSTR const cheatNames[] = {
    L"永不通缉", L"不会饥饿", L"无限生命", L"无限氧气", L"无限弹药", L"坦克模式",
    L"超级重拳", L"超级跳跃", L"最高威望", L"最高性感度", L"街上全是跑车", L"街上全是廉价车",
    L"无限奔跑", L"主角防火（秘籍）", L"完美操控", L"减少交通流量", L"自行车大跳",
    L"全车氮气", L"船只飞行", L"汽车飞行", L"一击必杀", L"冻结任务计时"
};
static LPCWSTR const weatherNames[] = {
    L"蓝天、局部多云 1", L"蓝天、局部多云 2", L"蓝天、局部多云 3", L"蓝天、局部多云 4",
    L"蓝天、局部多云 5", L"蓝天、局部多云 6", L"蓝天、局部多云 7", L"蓝天、局部多云 8",
    L"暴风雨", L"多云、有雾", L"晴朗蓝天", L"酷热（洛圣都热浪）", L"昏暗、灰白、薄雾 1",
    L"昏暗、灰白、薄雾 2", L"昏暗、灰白、薄雾 3", L"昏暗、灰白、薄雾 4", L"昏暗、多云、有雨",
    L"酷热 1", L"酷热 2", L"沙尘暴", L"偏绿、有雾", L"深紫昏暗", L"深绿昏暗",
    L"浅橙天空 1", L"浅橙天空 2", L"浅橙天空 3", L"浅橙天空 4", L"清新蓝天 1",
    L"清新蓝天 2", L"清新蓝天 3", L"暗青阴天 1", L"暗青阴天 2", L"暗青阴天 3",
    L"暗棕阴天", L"蓝紫天空", L"灰棕天空", L"橙色亮雾 1", L"橙色亮雾 2",
    L"橙色亮雾 3", L"极亮天空", L"蓝紫多云 1", L"蓝紫多云 2", L"蓝紫多云 3",
    L"昏暗毒云", L"黑白天空", L"黑紫天空"
};
static LPCWSTR const compass[] = {
    L"北（0°）", L"东北（45°）", L"东（90°）", L"东南（135°）",
    L"南（180°）", L"西南（225°）", L"西（270°）", L"西北（315°）"
};
static const WeaponSpec weaponSpecs[WEAPON_SLOT_COUNT] = {
    { L"近战", { L"高尔夫球杆", L"警棍", L"匕首", L"棒球棍", L"铲子", L"武士刀", L"台球杆", L"电锯" },
      { 2, 3, 4, 5, 6, 8, 7, 9 }, FALSE },
    { L"手枪", { L"手枪", L"消音手枪", L"沙漠之鹰" }, { 22, 23, 24 }, TRUE },
    { L"霰弹枪", { L"霰弹枪", L"短管霰弹枪", L"战斗霰弹枪" }, { 25, 26, 27 }, TRUE },
    { L"冲锋枪", { L"Micro Uzi", L"MP5", L"Tec-9" }, { 28, 29, 32 }, TRUE },
    { L"突击步枪", { L"AK-47", L"M4" }, { 30, 31 }, TRUE },
    { L"步枪", { L"乡村步枪", L"狙击步枪" }, { 33, 34 }, TRUE },
    { L"重型武器", { L"火箭筒", L"热追踪火箭筒", L"火焰喷射器", L"加特林机枪" }, { 35, 36, 37, 38 }, TRUE },
    { L"投掷武器", { L"手榴弹", L"催泪弹", L"燃烧瓶", L"遥控炸药" }, { 16, 17, 18, 39 }, TRUE },
    { L"特殊道具", { L"喷漆罐", L"灭火器", L"照相机" }, { 41, 42, 43 }, TRUE },
    { L"礼物", { L"鲜花", L"成人道具 1", L"成人道具 2", L"成人道具 3", L"成人道具 4", L"手杖" },
      { 14, 10, 11, 12, 13, 15 }, FALSE },
    { L"特殊装备", { L"夜视仪", L"热成像仪", L"降落伞" }, { 44, 45, 46 }, FALSE }
};

static const GameBinding gameStats[2][STAT_SLOT_COUNT] = {
    {
        { GAME_VEHICLE_ENGINE, 0 }, { GAME_VEHICLE_WEIGHT, 0 }, { GAME_VEHICLE_FLIGHT_ASSIST, 0 },
        { GAME_VEHICLE_SPEED, GAME_AXIS_X }, { GAME_VEHICLE_SPEED, GAME_AXIS_Y }, { GAME_VEHICLE_SPEED, GAME_AXIS_Z },
        { GAME_VEHICLE_SPIN, GAME_AXIS_X }, { GAME_VEHICLE_SPIN, GAME_AXIS_Y }, { GAME_VEHICLE_SPIN, GAME_AXIS_Z },
        { GAME_CLOCK_MINUTES, 0 }, { GAME_CLOCK_SPEED, 0 }, { GAME_WORLD_SPEED, 0 },
        { GAME_PLAYER_MONEY, 0 }, { GAME_MISSION_TIMER, 0 }, { GAME_CRIMINALS, 0 },
        { GAME_RELATIONSHIP, 0 }, { GAME_RELATIONSHIP, 1 }, { GAME_RELATIONSHIP, 2 },
        { GAME_RELATIONSHIP, 3 }, { GAME_RELATIONSHIP, 4 }, { GAME_RELATIONSHIP, 5 }
    },
    {
        { GAME_PLAYER_ARMOR, 0 }, { GAME_PLAYER_HEALTH, 0 }, { GAME_PLAYER_FAT, 0 },
        { GAME_PLAYER_STAMINA, 0 }, { GAME_PLAYER_MUSCLE, 0 }, { GAME_PLAYER_LUNG, 0 },
        { GAME_PLAYER_GAMBLING, 0 }, { GAME_PLAYER_DRIVING, 0 }, { GAME_PLAYER_BIKING, 0 },
        { GAME_PLAYER_CYCLING, 0 }, { GAME_PLAYER_FLYING, 0 },
        { GAME_PLAYER_SPEED, GAME_AXIS_X }, { GAME_PLAYER_SPEED, GAME_AXIS_Y }, { GAME_PLAYER_SPEED, GAME_AXIS_Z },
        { GAME_PLAYER_FLIGHT_ASSIST, 0 }, { GAME_PLAYER_WANTED, 0 }, { GAME_PLAYER_RESPECT, 0 }, { GAME_PLAYER_APPEAL, 0 }
    }
};

static const FlagBinding vehicleFlags[] = {
    { ID_VEHICLE_DAMAGE_EXPLOSION, GAME_VEHICLE_PROTECTION, GAME_PROOF_EXPLOSION },
    { ID_VEHICLE_DAMAGE_COLLISION, GAME_VEHICLE_PROTECTION, GAME_PROOF_COLLISION },
    { ID_VEHICLE_DAMAGE_BULLET, GAME_VEHICLE_PROTECTION, GAME_PROOF_BULLET },
    { ID_VEHICLE_DAMAGE_FIRE, GAME_VEHICLE_PROTECTION, GAME_PROOF_FIRE },
    { ID_VEHICLE_WHEELS, GAME_VEHICLE_WHEELS, 0 }, { ID_VEHICLE_ALARM, GAME_VEHICLE_AUTO_ALARM, 0 },
    { ID_VEHICLE_RESTART, GAME_VEHICLE_AUTO_RESTART, 0 }, { ID_VEHICLE_REMOTE, GAME_VEHICLE_RC, 0 },
    { ID_DONT_BURN, GAME_VEHICLE_DONT_BURN, 0 }, { ID_DONT_EXPLODE, GAME_VEHICLE_DONT_EXPLODE, 0 }
};

static const FlagBinding playerFlags[] = {
    { ID_PLAYER_DAMAGE_EXPLOSION, GAME_PLAYER_PROTECTION, GAME_PROOF_EXPLOSION },
    { ID_PLAYER_DAMAGE_COLLISION, GAME_PLAYER_PROTECTION, GAME_PROOF_COLLISION },
    { ID_PLAYER_DAMAGE_BULLET, GAME_PLAYER_PROTECTION, GAME_PROOF_BULLET },
    { ID_PLAYER_DAMAGE_FIRE, GAME_PLAYER_PROTECTION, GAME_PROOF_FIRE }
};

static HWND Label(UiPage *view, LPCWSTR text, int id, int x, int y, int width)
{
    return UiControl(view, WC_STATICW, text, SS_CENTERIMAGE, 0, id, x, y, width, 23);
}

static void Group(UiPage *view, LPCWSTR title, int x, int y, int width, int height)
{
    HWND group = UiControl(view, WC_BUTTONW, title, BS_GROUPBOX | WS_CLIPSIBLINGS,
                           0, 0, x, y, width, height);
    if (group)
        SetWindowPos(group, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static HWND Check(UiPage *view, LPCWSTR label, int id, int x, int y, int width)
{
    return UiControl(view, WC_BUTTONW, label, WS_TABSTOP | BS_AUTOCHECKBOX, 0, id, x, y, width, 23);
}

static HWND Button(UiPage *view, LPCWSTR text, int id, int x, int y, int width)
{
    return UiControl(view, WC_BUTTONW, text, WS_TABSTOP | BS_PUSHBUTTON, 0, id, x, y, width, 24);
}

static HWND Combo(UiPage *view, int id, int x, int y, int width)
{
    return UiControl(view, WC_COMBOBOXW, L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                     0, id, x, y, width, 222);
}

static void FillCombo(HWND combo, LPCWSTR const *list, int count)
{
    int option;
    for (option = 0; option < count; ++option)
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)list[option]);
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

static void SetNumber(HWND edit, int value)
{
    WCHAR text[16];
    wsprintfW(text, L"%d", value);
    SetWindowTextW(edit, text);
}

static int ReadNumber(HWND edit, int fallback, int minimum, int maximum)
{
    WCHAR text[32], *end;
    long value;

    if (!GetWindowTextW(edit, text, ARRAYSIZE(text)))
        return fallback;
    value = wcstol(text, &end, 10);
    if (end == text || *end)
        return fallback;
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return (int)value;
}

static HWND Number(UiPage *view, int id, int x, int y, int width, int value, BOOL allowOtherCharacters)
{
    HWND edit = UiControl(view, WC_EDITW, L"", WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL |
                          (allowOtherCharacters ? 0 : ES_NUMBER), WS_EX_CLIENTEDGE, id, x, y, width, 22);
    SendMessageW(edit, EM_SETLIMITTEXT, 8, 0);
    SetNumber(edit, value);
    return edit;
}

static void SetChecked(HWND window, BOOL checked)
{
    SendMessageW(window, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

static BOOL Checked(HWND window)
{
    return SendMessageW(window, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static const StatSpec *GetStatSpec(int page, int slot)
{
    if (page == 0) {
        if (slot < VEH_SPEED_X)
            return &vehicleStats[slot];
        if (slot < WORLD_TIME)
            return &vehicleMotion[slot - VEH_SPEED_X];
        if (slot < RELATIONSHIP_FIRST)
            return &worldStats[slot - WORLD_TIME];
        if (slot < RELATIONSHIP_FIRST + (int)ARRAYSIZE(relationshipStats))
            return &relationshipStats[slot - RELATIONSHIP_FIRST];
    } else {
        if (slot < PED_SPEED_X)
            return &playerStats[slot];
        if (slot < PED_WANTED)
            return &playerMotion[slot - PED_SPEED_X];
        if (slot < PED_WANTED + (int)ARRAYSIZE(otherPlayerStats))
            return &otherPlayerStats[slot - PED_WANTED];
    }
    return NULL;
}

static int StatId(int slot)
{
    return ID_STAT_FIRST + slot * 4;
}

static BOOL IsWeightStat(HWND pane, int slot)
{
    return slot == VEH_WEIGHT && GetWindowLongPtrW(pane, GWLP_USERDATA) == (LONG_PTR)&states[0];
}

static void WriteStatText(HWND pane, int slot, HWND edit, int value)
{
    if (IsWeightStat(pane, slot)) {
        WCHAR text[16];
        wsprintfW(text, L"%d.%d", value / 10, value % 10);
        SetWindowTextW(edit, text);
    } else {
        SetNumber(edit, value);
    }
}

static void Stats(UiPage *view, LPCWSTR title, int x, int y, int firstSlot, int count)
{
    int index;
    Group(view, title, x, y, GROUP_WIDTH, 26 + count * FIELD_ROW_HEIGHT);
    for (index = 0; index < count; ++index) {
        int top = y + 20 + index * FIELD_ROW_HEIGHT;
        int slot = firstSlot + index;
        int id = StatId(slot);
        const StatSpec *spec = GetStatSpec(view->index, slot);
        BOOL wideValue = spec->maximum > 99999999;
        HWND slider, edit;

        Label(view, spec->name, 0, x + 12, top, 118);
        slider = UiControl(view, TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_NOTICKS,
                           0, id, x + 128, top, wideValue ? 94 : 125, 22);
        SendMessageW(slider, TBM_SETRANGEMIN, FALSE, spec->minimum);
        SendMessageW(slider, TBM_SETRANGEMAX, FALSE, spec->maximum);
        SendMessageW(slider, TBM_SETPOS, TRUE, spec->initial);
        edit = Number(view, id + 1, x + (wideValue ? 229 : 260), top, wideValue ? 90 : 59, spec->initial,
                      spec->minimum < 0 || IsWeightStat(view->window, slot));
        if (wideValue)
            SendMessageW(edit, EM_SETLIMITTEXT, 10, 0);
        WriteStatText(view->window, slot, edit, spec->initial);
        Button(view, spec->action, id + 2, x + 326, top, 53);
        Check(view, L"锁定", id + 3, x + 386, top, 51);
    }
}

static void UpdateClockLabel(HWND pane, int value)
{
    static LPCWSTR const weekdays[] = {
        L"星期日", L"星期一", L"星期二", L"星期三", L"星期四", L"星期五", L"星期六"
    };
    HWND label = GetDlgItem(pane, ID_CLOCK_LABEL);
    WCHAR text[48];
    if (label) {
        int day = ReadNumber(GetDlgItem(pane, ID_WORLD_DAY), 0, 0, 99999);
        wsprintfW(text, L"当前游戏时间 %02d:%02d · %s", value / 60, value % 60, weekdays[(day + 3) % 7]);
        SetWindowTextW(label, text);
    }
}

static int ReadStat(HWND pane, int slot, const StatSpec *spec)
{
    HWND edit = GetDlgItem(pane, StatId(slot) + 1);
    int current = (int)SendDlgItemMessageW(pane, StatId(slot), TBM_GETPOS, 0, 0);
    if (IsWeightStat(pane, slot)) {
        WCHAR text[16], *end;
        double value;
        if (!GetWindowTextW(edit, text, ARRAYSIZE(text)))
            return current;
        value = wcstod(text, &end);
        if (end == text || *end || value != value)
            return current;
        if (value <= 0.1)
            return spec->minimum;
        if (value >= 400.0)
            return spec->maximum;
        return (int)(value * 10.0 + 0.5);
    }
    return ReadNumber(edit, current, spec->minimum, spec->maximum);
}

static void SetStat(HWND pane, int slot, int value)
{
    HWND slider = GetDlgItem(pane, StatId(slot));
    SendMessageW(slider, TBM_SETPOS, TRUE, value);
    DensePageScroll(slider);
}

static HWND StatPane(int page, int slot)
{
    int section = page == 1 || slot < VEH_SPEED_X ? 0 : slot < WORLD_TIME ? 1 : 2;
    return states[page].pane[section];
}

static double LiveStatValue(int page, int slot, int value)
{
    return page == 0 && slot == VEH_WEIGHT ? value / 10.0 : value;
}

static GameResult QueueStat(UiPage *page, HWND pane, int slot, int value, int lock)
{
    const GameBinding *binding = &gameStats[page->index][slot];
    BOOL locked = lock >= 0 ? lock != 0 : Checked(GetDlgItem(pane, StatId(slot) + 3));
    double liveValue = LiveStatValue(page->index, slot, value);

    if (binding->field == GAME_RELATIONSHIP)
        return GameSetGirlfriend(binding->index, value, lock == 0 ? 0 : locked ? 1 : 2);
    if (lock >= 0 || locked)
        return GameLockField(binding->field, binding->index, liveValue, locked);
    return GameSetField(binding->field, binding->index, liveValue);
}

static BOOL DisplayGameResult(UiPage *page, GameResult result)
{
    if (result == GAME_OFFLINE)
        return FALSE;
    states[page->index].liveFailure = result != GAME_OK && result != GAME_QUEUED;
    UiSetStatus(page, GameResultText(result));
    return TRUE;
}

static BOOL SetLiveField(UiPage *page, GameField field, int index, double value)
{
    return GameIsConnected() && DisplayGameResult(page, GameSetField(field, index, value));
}

static GameResult MergeGameResult(GameResult current, GameResult next)
{
    if (current != GAME_OK && current != GAME_QUEUED)
        return current;
    return next == GAME_OK ? current : next;
}

static void ReadLiveField(GameField field, int index)
{
    if (GameSupportsField(field, index, FALSE))
        GameReadField(field, index);
}

static void CommitLiveScroll(HWND pane, HWND slider, UINT action)
{
    UiPage *page = (UiPage *)GetWindowLongPtrW(GetParent(pane), GWLP_USERDATA);
    int id = slider ? GetDlgCtrlID(slider) : 0;
    int slot, value;

    if (!page || action != TB_ENDTRACK || id < ID_STAT_FIRST || id >= ID_STAT_FIRST + STAT_SLOT_COUNT * 4 ||
        (id - ID_STAT_FIRST) % 4 != 0 || !GameIsConnected())
        return;
    slot = (id - ID_STAT_FIRST) / 4;
    value = (int)SendMessageW(slider, TBM_GETPOS, 0, 0);
    DisplayGameResult(page, QueueStat(page, pane, slot, value, -1));
}

static void DirectionPanel(UiPage *view, int x, int y, BOOL vehicle)
{
    int heading = vehicle ? ID_VEHICLE_DIRECTION : ID_PED_DIRECTION;
    int kickHeading = vehicle ? ID_VEHICLE_KICK_DIRECTION : ID_PED_KICK_DIRECTION;
    Group(view, vehicle ? L"载具朝向和起步" : L"主角朝向和起步", x, y, GROUP_WIDTH, 113);
    Label(view, L"设定朝向", 0, x + 12, y + 23, 86);
    FillCombo(Combo(view, heading, x + 100, y + 23, 154), compass, ARRAYSIZE(compass));
    Button(view, L"应用朝向", heading + 1, x + 269, y + 22, 153);
    Label(view, L"起步方向", 0, x + 12, y + 62, 86);
    FillCombo(Combo(view, kickHeading, x + 100, y + 62, 130), compass, ARRAYSIZE(compass));
    Label(view, L"速度（%）", 0, x + 238, y + 62, 63);
    Number(view, kickHeading + 1, x + 304, y + 62, 48, 35, FALSE);
    Button(view, L"起步", kickHeading + 2, x + 360, y + 61, 62);
}

static void BuildVehicleControls(UiPage *view)
{
    HWND model, edit;
    int modelId;
    Group(view, L"载具选择、生成和车牌", LEFT_COLUMN, 4, GROUP_WIDTH, 196);
    Label(view, L"当前载具：Elegy", ID_CURRENT_MODEL, 24, 25, 409);
    Label(view, L"选择载具", 0, 24, 56, 78);
    model = Combo(view, ID_MODEL, 105, 56, 193);
    for (modelId = 400; modelId <= 611; ++modelId) {
        LPCWSTR name = UiVehicleName(modelId);
        if (name && *name && wcscmp(name, L"未知载具") != 0) {
            LRESULT index = SendMessageW(model, CB_ADDSTRING, 0, (LPARAM)name);
            if (index >= 0) {
                SendMessageW(model, CB_SETITEMDATA, index, modelId);
                if (modelId == 562)
                    SendMessageW(model, CB_SETCURSEL, index, 0);
            }
        }
    }
    Button(view, L"选择载具…", ID_PICK_MODEL, 313, 55, 123);
    Button(view, L"生成所选载具", ID_SPAWN_MODEL, 24, 91, 156);
    Check(view, L"自动准备生成器", ID_SPAWNER_AUTO, 193, 91, 140);
    SetChecked(GetDlgItem(view->window, ID_SPAWNER_AUTO), TRUE);
    Button(view, L"准备生成器", ID_CHECK_SPAWNER, 328, 91, 108);
    Label(view, L"生成器状态：就绪（离线）", ID_SPAWNER_READY, 24, 119, 412);
    Label(view, L"设置车牌", 0, 24, 157, 77);
    edit = UiControl(view, WC_EDITW, L"GTASA CC", WS_TABSTOP | ES_UPPERCASE | ES_AUTOHSCROLL,
                     WS_EX_CLIENTEDGE, ID_PLATE, 105, 156, 123, 22);
    SendMessageW(edit, EM_SETLIMITTEXT, 8, 0);
    Button(view, L"应用车牌", ID_APPLY_PLATE, 239, 155, 103);
    Check(view, L"锁定车牌", ID_PLATE_LOCK, 354, 156, 86);

    Stats(view, L"引擎、重量和飞行", LEFT_COLUMN, 208, VEH_HEALTH, ARRAYSIZE(vehicleStats));
    Group(view, L"自动维护", LEFT_COLUMN, 325, GROUP_WIDTH, 101);
    Check(view, L"自动关闭防盗警报", ID_VEHICLE_ALARM, 24, 346, 181);
    Check(view, L"熄火后自动重启", ID_VEHICLE_RESTART, 218, 346, 196);
    Check(view, L"燃烧时自动修复", ID_DONT_BURN, 24, 372, 181);
    Check(view, L"锁定燃烧计时防止爆炸", ID_DONT_EXPLODE, 218, 372, 218);
    Check(view, L"同时控制遥控载具", ID_VEHICLE_REMOTE, 24, 398, 205);

    Group(view, L"车身颜色", RIGHT_COLUMN, 4, GROUP_WIDTH, 146);
    UiControl(view, WC_BUTTONW, L"车身主色", WS_TABSTOP | BS_OWNERDRAW, 0,
              ID_COLOR_PRIMARY, 478, 31, 188, 33);
    UiControl(view, WC_BUTTONW, L"车身副色", WS_TABSTOP | BS_OWNERDRAW, 0,
              ID_COLOR_SECONDARY, 695, 31, 188, 33);
    Check(view, L"锁定当前主色", ID_PRIMARY_LOCK, 478, 75, 188);
    Check(view, L"锁定当前副色", ID_SECONDARY_LOCK, 695, 75, 188);
    Check(view, L"在换车后继续应用主色与副色", ID_COLOR_LOCK, 478, 105, 333);

    Group(view, L"载具防护和门锁", RIGHT_COLUMN, 160, GROUP_WIDTH, 180);
    Check(view, L"锁定载具伤害防护", ID_VEHICLE_DAMAGE_LOCK, 478, 182, 265);
    Check(view, L"载具防爆", ID_VEHICLE_DAMAGE_EXPLOSION, 478, 209, 101);
    Check(view, L"载具防撞", ID_VEHICLE_DAMAGE_COLLISION, 584, 209, 101);
    Check(view, L"载具防弹", ID_VEHICLE_DAMAGE_BULLET, 690, 209, 101);
    Check(view, L"载具防火", ID_VEHICLE_DAMAGE_FIRE, 795, 209, 101);
    Check(view, L"保护车辆及拖车车轮", ID_VEHICLE_WHEELS, 478, 240, 285);
    Label(view, L"车门状态", 0, 478, 270, 76);
    {
        static LPCWSTR const doorStates[] = { L"解锁", L"上锁" };
        FillCombo(Combo(view, ID_VEHICLE_DOORS, 556, 270, 133), doorStates, ARRAYSIZE(doorStates));
    }
    Check(view, L"锁定车门状态", ID_VEHICLE_DOOR_LOCK, 705, 269, 175);
    Button(view, L"修复载具", ID_REPAIR_VEHICLE, 478, 303, 108);
}

static void BuildVehicleMotion(UiPage *view)
{
    Stats(view, L"载具线速度", LEFT_COLUMN, 4, VEH_SPEED_X, 3);
    Stats(view, L"载具旋转速度", LEFT_COLUMN, 122, VEH_SPIN_X, 3);
    Group(view, L"快捷操作", LEFT_COLUMN, 244, GROUP_WIDTH, 99);
    Button(view, L"停止所有线速度", ID_STOP_VEHICLE_SPEED, 24, 268, 184);
    Button(view, L"停止所有旋转", ID_STOP_VEHICLE_SPIN, 221, 268, 184);
    Button(view, L"冻结载具", ID_FREEZE_VEHICLE, 24, 302, 184);
    Button(view, L"翻转载具", ID_FLIP_VEHICLE, 221, 302, 184);
    DirectionPanel(view, RIGHT_COLUMN, 4, TRUE);
}

static void BuildWorld(UiPage *view)
{
    HWND date;
    int index;

    Stats(view, L"时间、游戏速度和统计", LEFT_COLUMN, 4, WORLD_TIME, ARRAYSIZE(worldStats));
    for (index = 0; index < 4; ++index)
        EnableWindow(GetDlgItem(view->window, StatId(WORLD_MISSION_TIMER) + index), FALSE);
    SetDlgItemTextW(view->window, StatId(WORLD_MISSION_TIMER) + 1, L"—");
    Label(view, L"任务计时读写待验证，可在「游戏开关」冻结计时。", 0, 24, 192, 416);
    Label(view, L"当前游戏时间 12:00 · 星期三", ID_CLOCK_LABEL, 24, 219, 252);
    Label(view, L"游戏天数", 0, 286, 219, 66);
    Number(view, ID_WORLD_DAY, 359, 219, 77, 0, FALSE);
    Button(view, L"停止时钟", ID_CLOCK_STOP, 24, 251, 99);
    Button(view, L"现实时间", ID_CLOCK_REAL, 128, 251, 99);
    Button(view, L"正常时钟", ID_CLOCK_NORMAL, 232, 251, 99);
    Button(view, L"正常游戏速度", ID_GAME_SPEED_NORMAL, 337, 251, 105);

    Group(view, L"天气和全局操作", LEFT_COLUMN, 286, GROUP_WIDTH, 181);
    Label(view, L"选择天气", 0, 24, 310, 72);
    FillCombo(Combo(view, ID_WEATHER, 103, 310, 224), weatherNames, ARRAYSIZE(weatherNames));
    Button(view, L"应用天气", ID_APPLY_WEATHER, 338, 309, 97);
    Check(view, L"锁定当前天气", ID_WEATHER_LOCK, 24, 341, 175);
    Check(view, L"游戏画面反馈", ID_GAME_FEEDBACK, 225, 341, 183);
    Button(view, L"应用金钱", ID_APPLY_MONEY, 24, 386, 112);
    Button(view, L"清除作弊状态及次数", ID_CLEAR_CHEATS, 146, 386, 155);
    Button(view, L"刷新游戏数据", ID_RESYNC, 310, 386, 125);
    Label(view, L"作弊状态：未使用 · 次数 0", ID_CHEAT_STATUS, 24, 422, 254);
    Label(view, L"Steam 版（离线）", ID_GAME_VERSION, 288, 422, 149);

    Stats(view, L"六位女友的进度", RIGHT_COLUMN, 4, RELATIONSHIP_FIRST, ARRAYSIZE(relationshipStats));
    Button(view, L"同步进度", ID_SYNC_RELATIONSHIP, 478, 198, 126);
    Group(view, L"准备约会", RIGHT_COLUMN, 237, GROUP_WIDTH, 79);
    Label(view, L"选择约会对象", 0, 478, 264, 104);
    date = Combo(view, ID_RELATIONSHIP_DATE, 588, 264, 152);
    for (index = 0; index < (int)ARRAYSIZE(relationshipStats); ++index)
        SendMessageW(date, CB_ADDSTRING, 0, (LPARAM)relationshipStats[index].name);
    SendMessageW(date, CB_SETCURSEL, 0, 0);
    Button(view, L"准备约会", ID_PREPARE_DATE, 752, 263, 128);
}

static void BuildPlayerControls(UiPage *view)
{
    Label(view, L"当前主角：CJ", 0, 24, 9, 412);
    Stats(view, L"主角属性与技能", LEFT_COLUMN, 37, PED_ARMOR, ARRAYSIZE(playerStats));
    Stats(view, L"通缉和个人统计", LEFT_COLUMN, 365, PED_WANTED, ARRAYSIZE(otherPlayerStats));

    Group(view, L"主角伤害防护", RIGHT_COLUMN, 4, GROUP_WIDTH, 134);
    Check(view, L"锁定主角伤害防护", ID_PLAYER_DAMAGE_LOCK, 478, 27, 218);
    Check(view, L"主角防爆", ID_PLAYER_DAMAGE_EXPLOSION, 478, 55, 96);
    Check(view, L"主角防撞", ID_PLAYER_DAMAGE_COLLISION, 582, 55, 96);
    Check(view, L"主角防弹", ID_PLAYER_DAMAGE_BULLET, 686, 55, 96);
    Check(view, L"主角防火（伤害）", ID_PLAYER_DAMAGE_FIRE, 478, 83, 204);
    Check(view, L"主角防火（秘籍）", ID_CHEAT_FIRE, 478, 108, 202);
    Check(view, L"锁定防火秘籍", ID_CHEAT_FIRE_LOCK, 700, 108, 172);
    Stats(view, L"主角移动", RIGHT_COLUMN, 146, PED_SPEED_X, ARRAYSIZE(playerMotion));
    Button(view, L"停止所有移动（冻结主角）", ID_STOP_PED_SPEED, 478, 287, 218);
    DirectionPanel(view, RIGHT_COLUMN, 320, FALSE);
}

static void BuildWeaponControls(UiPage *view)
{
    int slot;
    HWND combo;

    Group(view, L"武器槽、弹药与锁定", LEFT_COLUMN, 4, UI_PAGE_WIDTH - 24, 367);
    Label(view, L"武器类别", 0, 24, 25, 113);
    Label(view, L"武器或物品", 0, 141, 25, 270);
    Label(view, L"携带数量", 0, 435, 25, 109);
    Label(view, L"弹匣装填量", 0, 560, 25, 110);
    Label(view, L"锁定武器槽", 0, 674, 25, 108);
    for (slot = 0; slot < WEAPON_SLOT_COUNT; ++slot) {
        int y = 51 + slot * 28, id = ID_WEAPON_FIRST + slot * 5;
        int option;

        Label(view, weaponSpecs[slot].category, 0, 24, y, 112);
        combo = Combo(view, id, 141, y, 279);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"无");
        for (option = 0; weaponSpecs[slot].options[option]; ++option) {
            LRESULT item = SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)weaponSpecs[slot].options[option]);
            if (item >= 0)
                SendMessageW(combo, CB_SETITEMDATA, item, weaponSpecs[slot].gameIds[option]);
        }
        SendMessageW(combo, CB_SETCURSEL, slot == 1 ? 1 : 0, 0);
        if (weaponSpecs[slot].hasAmmo) {
            Number(view, id + 1, 435, y, 101, slot == 1 ? 120 : 0, FALSE);
            Number(view, id + 2, 560, y, 99, slot == 1 ? 30 : 0, FALSE);
        } else {
            Label(view, L"不适用", 0, 435, y, 101);
            Label(view, L"不适用", 0, 560, y, 99);
        }
        Check(view, L"锁定", id + 3, 682, y, 82);
        Button(view, L"应用", id + 4, 786, y - 1, 94);
    }

    Group(view, L"当前武器与熟练度", LEFT_COLUMN, 377, UI_PAGE_WIDTH - 24, 103);
    Check(view, L"给予指虎并锁定", ID_BRASS_KNUCKLES, 24, 400, 164);
    Check(view, L"锁定弹匣弹药", ID_LOADED_LOCK, 192, 400, 155);
    Label(view, L"当前武器槽", 0, 362, 400, 96);
    combo = Combo(view, ID_CURRENT_SLOT, 468, 400, 132);
    for (slot = SLOT_UNARMED; slot <= SLOT_DETONATOR; ++slot) {
        LPCWSTR name = slot == SLOT_UNARMED ? L"空手／指虎" :
                       slot == SLOT_DETONATOR ? L"引爆器" : weaponSpecs[slot - 1].category;
        LRESULT item = SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)name);
        if (item >= 0)
            SendMessageW(combo, CB_SETITEMDATA, item, slot);
    }
    SendMessageW(combo, CB_SETCURSEL, states[1].currentWeaponSlot, 0);
    Label(view, L"当前弹匣 30", ID_CURRENT_LOADED, 614, 400, 132);
    Label(view, L"当前携带 120", ID_CURRENT_TOTAL, 753, 400, 132);
    Button(view, L"补满弹药", ID_AMMO_REFILL, 24, 436, 122);
    Button(view, L"详细武器熟练度…", ID_WEAPON_LEVELS, 155, 436, 164);
    Label(view, L"平均熟练度 30%", ID_WEAPON_AVERAGE, 331, 437, 145);
    Label(view, L"当前武器：手枪", ID_CURRENT_WEAPON, 484, 437, 245);
    Button(view, L"读取武器", ID_READ_WEAPONS, 752, 436, 128);
}

static void BuildCheatSwitches(UiPage *view)
{
    int index;
    Group(view, L"游戏开关与锁定", LEFT_COLUMN, 4, UI_PAGE_WIDTH - 24, 383);
    for (index = 0; index < CHEAT_COUNT + 10; ++index) {
        int x = index % 2 ? 476 : 24;
        int y = 26 + index / 2 * 22;
        int onId = index == 13 ? ID_CHEAT_FIRE : ID_CHEAT_FIRST + index * 2;
        int lockId = index == 13 ? ID_CHEAT_FIRE_LOCK : onId + 1;
        HWND enabled = Check(view, index < CHEAT_COUNT ? cheatNames[index] : L"（预留）", onId, x, y, 244);
        HWND locked = Check(view, L"锁定", lockId, x + 254, y, 116);
        if (index >= CHEAT_COUNT) {
            EnableWindow(enabled, FALSE);
            EnableWindow(locked, FALSE);
        }
    }
    Group(view, L"秘籍行为", LEFT_COLUMN, 397, UI_PAGE_WIDTH - 24, 62);
    Check(view, L"使用秘籍后自动清除作弊状态", ID_SAFE_CHEATS, 24, 421, 299);
    Check(view, L"Hot Coffee（脚本）", ID_COFFEE, 354, 421, 280);
}

static void UpdateColorLabels(HWND pane, const DenseState *state)
{
    WCHAR text[48];
    HWND primary = GetDlgItem(pane, ID_COLOR_PRIMARY);
    HWND secondary = GetDlgItem(pane, ID_COLOR_SECONDARY);
    wsprintfW(text, L"车身主色 %03d…", state->colors[0]);
    SetWindowTextW(primary, text);
    wsprintfW(text, L"车身副色 %03d…", state->colors[1]);
    SetWindowTextW(secondary, text);
    InvalidateRect(primary, NULL, TRUE);
    InvalidateRect(secondary, NULL, TRUE);
}

static void DrawColor(const DenseState *state, const DRAWITEMSTRUCT *item)
{
    int which = item->CtlID == ID_COLOR_PRIMARY ? 0 : 1;
    int patchSize;
    RECT rect = item->rcItem, swatch, label;
    UINT flags = DFCS_BUTTONPUSH;
    WCHAR text[48];
    HBRUSH brush;

    if (item->itemState & ODS_SELECTED)
        flags |= DFCS_PUSHED;
    DrawFrameControl(item->hDC, &rect, DFC_BUTTON, flags);
    InflateRect(&rect, -6, -5);
    swatch = rect;
    patchSize = min(rect.bottom - rect.top, (rect.right - rect.left) / 4);
    swatch.right = swatch.left + patchSize * 2;
    brush = CreateSolidBrush(UiVehicleColor(state->colors[which]));
    if (brush) {
        FillRect(item->hDC, &swatch, brush);
        DeleteObject(brush);
    }
    FrameRect(item->hDC, &swatch, GetSysColorBrush(COLOR_3DSHADOW));
    GetWindowTextW(item->hwndItem, text, ARRAYSIZE(text));
    label = rect;
    label.left = swatch.right + 5;
    SetTextColor(item->hDC, GetSysColor(COLOR_BTNTEXT));
    SetBkMode(item->hDC, TRANSPARENT);
    DrawTextW(item->hDC, text, -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (item->itemState & ODS_FOCUS)
        DrawFocusRect(item->hDC, &rect);
}

static LRESULT CALLBACK DenseSectionProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lParam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
    } else if (message == WM_HSCROLL) {
        LRESULT result = SendMessageW(GetParent(window), message, wParam, lParam);
        CommitLiveScroll(window, (HWND)lParam, LOWORD(wParam));
        return result;
    } else if (message == WM_COMMAND) {
        return SendMessageW(GetParent(window), message, wParam, lParam);
    } else if (message == WM_DRAWITEM && (wParam == ID_COLOR_PRIMARY || wParam == ID_COLOR_SECONDARY)) {
        DrawColor((const DenseState *)GetWindowLongPtrW(window, GWLP_USERDATA), (const DRAWITEMSTRUCT *)lParam);
        return TRUE;
    } else if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN) {
        SetTextColor((HDC)wParam, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static BOOL RegisterSection(void)
{
    WNDCLASSW wc = { 0 };
    HINSTANCE instance = GetModuleHandleW(NULL);
    if (GetClassInfoW(instance, sectionClass, &wc))
        return TRUE;
    wc.hInstance = instance;
    wc.lpfnWndProc = DenseSectionProc;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = sectionClass;
    return RegisterClassW(&wc) != 0;
}

void BuildDensePage(UiPage *page)
{
    static LPCWSTR const sectionNames[2][SECTION_COUNT] = {
        { L"载具与配色", L"运动与朝向", L"世界与女友进度" },
        { L"属性与防护", L"武器与弹药", L"游戏开关" }
    };
    static void (*const builders[2][SECTION_COUNT])(UiPage *) = {
        { BuildVehicleControls, BuildVehicleMotion, BuildWorld },
        { BuildPlayerControls, BuildWeaponControls, BuildCheatSwitches }
    };
    DenseState *state = &states[page->index];
    int section;

    if (!RegisterSection()) {
        page->failed = TRUE;
        return;
    }
    ZeroMemory(state, sizeof(*state));
    state->loadedLockSlot = -1;
    state->currentWeaponSlot = 2;
    state->requestedWeaponSlot = -1;
    state->model = 562;
    state->colors[0] = 36;
    state->colors[1] = 1;
    if (page->index == 0)
        UiVehicleDefaultColors(state->model, &state->colors[0], &state->colors[1]);
    state->money = 5000;
    lstrcpyW(state->plate, L"GTASA CC");
    for (section = 0; section < (int)ARRAYSIZE(state->proficiency); ++section)
        state->proficiency[section] = 300;
    for (section = 0; section < (int)ARRAYSIZE(state->relationship); ++section)
        state->relationship[section] = relationshipStats[section].initial;
    state->weapons[1].type = 22;
    state->weapons[1].total = 120;
    state->weapons[1].loaded = 30;
    state->spawnerReady = TRUE;

    for (section = 0; section < SECTION_COUNT; ++section) {
        HWND radio = UiControl(page, WC_BUTTONW, sectionNames[page->index][section],
                               BS_AUTORADIOBUTTON | BS_PUSHLIKE | WS_TABSTOP | (section ? 0 : WS_GROUP),
                               0, ID_SECTION_FIRST + section, 12 + section * 175, 6, 163, 27);
        SetChecked(radio, section == 0);
    }
    for (section = 0; section < SECTION_COUNT; ++section) {
        UiPage view = *page;
        view.window = CreateWindowExW(WS_EX_CONTROLPARENT, sectionClass, sectionNames[page->index][section],
                                     WS_CHILD | WS_CLIPSIBLINGS | WS_GROUP,
                                     0, UiScale(page, SECTION_TOP), UiScale(page, UI_PAGE_WIDTH),
                                     UiScale(page, UI_PAGE_HEIGHT - SECTION_TOP), page->window,
                                     (HMENU)(INT_PTR)(ID_PANE_FIRST + section), GetModuleHandleW(NULL), state);
        if (!view.window) {
            page->failed = TRUE;
            return;
        }
        state->pane[section] = view.window;
        builders[page->index][section](&view);
        if (view.failed) {
            page->failed = TRUE;
            return;
        }
    }
    if (page->index == 0)
        UpdateColorLabels(state->pane[0], state);
    ShowWindow(state->pane[0], SW_SHOWNA);
}

void DensePageScroll(HWND slider)
{
    int id = GetDlgCtrlID(slider);
    if (id >= ID_STAT_FIRST && id < ID_STAT_FIRST + STAT_SLOT_COUNT * 4 && (id - ID_STAT_FIRST) % 4 == 0) {
        HWND pane = GetParent(slider);
        int value = (int)SendMessageW(slider, TBM_GETPOS, 0, 0);
        WriteStatText(pane, (id - ID_STAT_FIRST) / 4, GetDlgItem(pane, id + 1), value);
        if (id == StatId(WORLD_TIME))
            UpdateClockLabel(pane, value);
    }
}

static void SetTriple(HWND pane, int first, int x, int y, int z)
{
    SetStat(pane, first, x);
    SetStat(pane, first + 1, y);
    SetStat(pane, first + 2, z);
}

static void SelectSection(UiPage *page, DenseState *state, int section)
{
    int option;
    HWND current;
    for (option = 0; option < SECTION_COUNT; ++option)
        SetChecked(GetDlgItem(page->window, ID_SECTION_FIRST + option), option == section);
    if (section == state->activePane)
        return;
    current = state->pane[state->activePane];
    if (IsChild(current, GetFocus()))
        SetFocus(GetDlgItem(page->window, ID_SECTION_FIRST + section));
    ShowWindow(current, SW_HIDE);
    ShowWindow(state->pane[section], SW_SHOWNA);
    state->activePane = section;
}

static void ApplyDirection(UiPage *page, DenseState *state, HWND pane, BOOL kick)
{
    BOOL vehicle = page->index == 0;
    int controlId = vehicle ? ID_VEHICLE_DIRECTION : ID_PED_DIRECTION;
    int selection, speed;
    double radians;
    WCHAR status[100];

    if (kick)
        controlId = vehicle ? ID_VEHICLE_KICK_DIRECTION : ID_PED_KICK_DIRECTION;
    selection = (int)SendDlgItemMessageW(pane, controlId, CB_GETCURSEL, 0, 0);
    if (selection < 0 || selection >= (int)ARRAYSIZE(compass))
        return;
    speed = kick ? ReadNumber(GetDlgItem(pane, controlId + 1), 0, 0, 400) : 0;
    if (kick)
        SetNumber(GetDlgItem(pane, controlId + 1), speed);
    if (kick ? SetLiveField(page, vehicle ? GAME_VEHICLE_KICK : GAME_PLAYER_KICK, selection * 45, speed) :
               SetLiveField(page, vehicle ? GAME_VEHICLE_HEADING : GAME_PLAYER_HEADING, 0, selection * 45))
        return;
    state->heading = selection * 45;
    if (kick) {
        radians = selection * 0.7853981633974483;
        SetTriple(pane, vehicle ? VEH_SPEED_X : PED_SPEED_X,
                  (int)(speed * cos(radians)), (int)(speed * sin(radians)), 0);
        wsprintfW(status, L"离线 %s朝%s起步，速度 %d%%", vehicle ? L"载具" : L"主角", compass[selection], speed);
    } else {
        wsprintfW(status, L"离线 %s朝向已设为%s", vehicle ? L"载具" : L"主角", compass[selection]);
    }
    UiSetStatus(page, status);
}

static BOOL SetModelSelection(HWND combo, int model)
{
    int index, count = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    for (index = 0; index < count; ++index) {
        if (SendMessageW(combo, CB_GETITEMDATA, index, 0) == model) {
            SendMessageW(combo, CB_SETCURSEL, index, 0);
            return TRUE;
        }
    }
    return FALSE;
}

static void UpdateCurrentModel(HWND pane, int model)
{
    WCHAR label[96];
    LPCWSTR name = UiVehicleName(model);
    if (name) {
        wsprintfW(label, L"当前载具：%.60s", name);
        SetDlgItemTextW(pane, ID_CURRENT_MODEL, label);
    }
}

static BOOL NormalizePlate(HWND edit, WCHAR plate[9])
{
    WCHAR text[9];
    int count = GetWindowTextW(edit, text, ARRAYSIZE(text)), index;
    BOOL hasText = FALSE;
    if (!count)
        return FALSE;
    for (index = 0; index < count; ++index) {
        WCHAR c = text[index];
        if (c >= L'a' && c <= L'z')
            c -= L'a' - L'A';
        if ((c < L'A' || c > L'Z') && (c < L'0' || c > L'9') && c != L' ')
            return FALSE;
        if (c != L' ')
            hasText = TRUE;
        plate[index] = c;
    }
    if (!hasText)
        return FALSE;
    plate[count] = L'\0';
    SetWindowTextW(edit, plate);
    return TRUE;
}

static int ComboValue(HWND combo)
{
    LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    return index < 0 ? -1 : (int)SendMessageW(combo, CB_GETITEMDATA, index, 0);
}

static void ChangeLabel(HWND pane, int id, LPCWSTR text)
{
    WCHAR previous[96] = { 0 };
    GetDlgItemTextW(pane, id, previous, ARRAYSIZE(previous));
    if (wcscmp(previous, text) != 0)
        SetDlgItemTextW(pane, id, text);
}

static LPCWSTR CurrentWeaponName(const DenseState *state)
{
    int slot = state->currentWeaponSlot - 1, option;
    if (state->currentWeaponSlot == SLOT_UNARMED)
        return state->unarmedType == 1 ? L"指虎" : L"空手";
    if (state->currentWeaponSlot == SLOT_DETONATOR)
        return L"引爆器";
    if (slot < 0 || slot >= WEAPON_SLOT_COUNT || !state->weapons[slot].type)
        return L"无";
    for (option = 0; weaponSpecs[slot].options[option]; ++option) {
        if (weaponSpecs[slot].gameIds[option] == state->weapons[slot].type)
            return weaponSpecs[slot].options[option];
    }
    return L"未知武器";
}

static void UpdateCurrentWeapon(HWND pane, const DenseState *state)
{
    WCHAR label[96];
    int slot = state->currentWeaponSlot - 1;
    wsprintfW(label, L"当前武器：%s", CurrentWeaponName(state));
    ChangeLabel(pane, ID_CURRENT_WEAPON, label);
    if (slot >= 0 && slot < WEAPON_SLOT_COUNT && state->weapons[slot].type && weaponSpecs[slot].hasAmmo) {
        wsprintfW(label, L"当前弹匣 %d", state->weapons[slot].loaded);
        ChangeLabel(pane, ID_CURRENT_LOADED, label);
        wsprintfW(label, L"当前携带 %d", state->weapons[slot].total);
        ChangeLabel(pane, ID_CURRENT_TOTAL, label);
    } else {
        ChangeLabel(pane, ID_CURRENT_LOADED, L"当前弹匣 —");
        ChangeLabel(pane, ID_CURRENT_TOTAL, L"当前携带 —");
    }
}

static void ApplyWeapon(HWND pane, DenseState *state, int slot)
{
    int id = ID_WEAPON_FIRST + slot * 5;
    WeaponSetting *setting = &state->weapons[slot];
    int current = ComboValue(GetDlgItem(pane, id));
    setting->type = current > 0 ? current : 0;
    if (weaponSpecs[slot].hasAmmo) {
        HWND total = GetDlgItem(pane, id + 1), loaded = GetDlgItem(pane, id + 2);
        setting->total = current > 0 ? ReadNumber(total, setting->total, 0, 99999) : 0;
        setting->loaded = current > 0 ? ReadNumber(loaded, setting->loaded, 0, setting->total) : 0;
        SetNumber(total, setting->total);
        SetNumber(loaded, setting->loaded);
    }
    UpdateCurrentWeapon(pane, state);
}

static void UpdateWeaponAction(HWND pane, int slot, const WeaponSetting *setting)
{
    LPCWSTR text = setting->applying ? L"正在应用…" : setting->dirty ? L"应用修改" : L"应用";
    ChangeLabel(pane, ID_WEAPON_FIRST + slot * 5 + 4, text);
}

static void RefillAmmo(HWND pane, DenseState *state)
{
    int slot;
    for (slot = 0; slot < WEAPON_SLOT_COUNT; ++slot) {
        int id = ID_WEAPON_FIRST + slot * 5;
        if (weaponSpecs[slot].hasAmmo && SendDlgItemMessageW(pane, id, CB_GETCURSEL, 0, 0) > 0) {
            SetNumber(GetDlgItem(pane, id + 1), 99999);
            SetNumber(GetDlgItem(pane, id + 2), 100);
            ApplyWeapon(pane, state, slot);
        }
    }
}

static void UpdateWeaponAverage(HWND pane, const DenseState *state)
{
    int slot, sum = 0;
    WCHAR label[64];
    for (slot = 0; slot < (int)ARRAYSIZE(state->proficiency); ++slot)
        sum += state->proficiency[slot];
    wsprintfW(label, L"平均熟练度 %d%%", sum / ((int)ARRAYSIZE(state->proficiency) * 10));
    ChangeLabel(pane, ID_WEAPON_AVERAGE, label);
}

static GameResult QueueWeaponField(GameField field, int slot, int value, int lock)
{
    return lock < 0 ? GameSetField(field, slot, value) : GameLockField(field, slot, value, lock != 0);
}

static BOOL ReadWeaponRequest(HWND pane, int slot, GameWeapon *weapon)
{
    int id = ID_WEAPON_FIRST + slot * 5;
    int type = ComboValue(GetDlgItem(pane, id));
    if (type < 0 || type > 46)
        return FALSE;
    weapon->type = (unsigned)type;
    weapon->total = weapon->loaded = 0;
    if (type && weaponSpecs[slot].hasAmmo) {
        weapon->total = (unsigned)ReadNumber(GetDlgItem(pane, id + 1), 0, 0, 99999);
        weapon->loaded = (unsigned)ReadNumber(GetDlgItem(pane, id + 2), 0, 0, (int)weapon->total);
    }
    return TRUE;
}

static GameResult QueueWeaponApply(HWND pane, int slot)
{
    GameWeapon weapon;
    return ReadWeaponRequest(pane, slot, &weapon) ? GameSetWeapon(slot, &weapon) : GAME_INVALID;
}

static GameResult QueueWeaponLock(HWND pane, int slot, BOOL enabled)
{
    GameWeapon weapon;
    GameResult result;
    if (!ReadWeaponRequest(pane, slot, &weapon))
        return GAME_INVALID;
    result = GameLockField(GAME_WEAPON_TYPE, slot, weapon.type, enabled);
    if ((result != GAME_OK && result != GAME_QUEUED) || !weaponSpecs[slot].hasAmmo)
        return result;
    result = MergeGameResult(result, GameLockField(GAME_WEAPON_TOTAL, slot, weapon.total, enabled));
    if (states[1].loadedLockSlot == slot && Checked(GetDlgItem(pane, ID_LOADED_LOCK)))
        enabled = TRUE;
    return MergeGameResult(result, GameLockField(GAME_WEAPON_LOADED, slot, weapon.loaded, enabled));
}

static GameResult QueueRefill(HWND pane, const DenseState *state)
{
    GameResult result = GAME_OK;
    int slot;
    for (slot = 0; slot < WEAPON_SLOT_COUNT; ++slot) {
        int id = ID_WEAPON_FIRST + slot * 5;
        if (weaponSpecs[slot].hasAmmo && state->weapons[slot].type > 0) {
            int lock = Checked(GetDlgItem(pane, id + 3)) ? 1 : -1;
            result = MergeGameResult(result, QueueWeaponField(GAME_WEAPON_TOTAL, slot, 99999, lock));
            if (state->loadedLockSlot == slot && Checked(GetDlgItem(pane, ID_LOADED_LOCK)))
                lock = 1;
            result = MergeGameResult(result, QueueWeaponField(GAME_WEAPON_LOADED, slot, 100, lock));
        }
    }
    return result;
}

static const FlagBinding *FindFlag(int page, int id)
{
    const FlagBinding *flags = page == 0 ? vehicleFlags : playerFlags;
    size_t count = page == 0 ? ARRAYSIZE(vehicleFlags) : ARRAYSIZE(playerFlags), index;
    for (index = 0; index < count; ++index) {
        if (flags[index].id == id)
            return &flags[index];
    }
    return NULL;
}

static BOOL DisplayCheckedResult(UiPage *page, HWND control, GameResult result)
{
    if (!DisplayGameResult(page, result))
        return FALSE;
    if (result != GAME_QUEUED && result != GAME_OK)
        SetChecked(control, !Checked(control));
    return TRUE;
}

static GameResult QueueProofs(UiPage *page, HWND pane, BOOL locked)
{
    const FlagBinding *flags = page->index == 0 ? vehicleFlags : playerFlags;
    GameResult result = GAME_OK;
    int proof;
    for (proof = GAME_PROOF_EXPLOSION; proof <= GAME_PROOF_FIRE; ++proof)
        result = MergeGameResult(result, GameLockField(flags[proof].field, flags[proof].index,
                                                       Checked(GetDlgItem(pane, flags[proof].id)), locked));
    return result;
}

static BOOL QueueLiveProtection(UiPage *page, HWND pane, int id, HWND control)
{
    const FlagBinding *binding = FindFlag(page->index, id);
    int master = page->index == 0 ? ID_VEHICLE_DAMAGE_LOCK : ID_PLAYER_DAMAGE_LOCK;
    BOOL value = Checked(control);
    GameResult result;

    if (!GameIsConnected())
        return FALSE;
    if (id == master)
        return DisplayCheckedResult(page, control, QueueProofs(page, pane, value));
    if (!binding)
        return FALSE;
    if ((binding->field == GAME_PLAYER_PROTECTION || binding->field == GAME_VEHICLE_PROTECTION) &&
        Checked(GetDlgItem(pane, master))) {
        result = GameLockField(binding->field, binding->index, value, TRUE);
    } else {
        result = GameSetField(binding->field, binding->index, value);
    }
    return DisplayCheckedResult(page, control, result);
}

static void UpdateCheatStatus(int count)
{
    WCHAR label[64];
    wsprintfW(label, L"作弊状态：%s · 次数 %d", count ? L"已使用" : L"未使用", count);
    SetDlgItemTextW(states[0].pane[2], ID_CHEAT_STATUS, label);
}

static void RecordCheat(const DenseState *player)
{
    if (!Checked(GetDlgItem(player->pane[2], ID_SAFE_CHEATS)) && states[0].cheatsUsed < 9999999)
        ++states[0].cheatsUsed;
    UpdateCheatStatus(states[0].cheatsUsed);
}

static void ReadDensePage(UiPage *page, BOOL newer);
static void ReadWeaponSlots(DenseState *state, BOOL newer);

static GameResult QueuePaintLock(HWND pane, const DenseState *state)
{
    BOOL carry = Checked(GetDlgItem(pane, ID_COLOR_LOCK));
    BOOL primary = Checked(GetDlgItem(pane, ID_PRIMARY_LOCK));
    BOOL secondary = Checked(GetDlgItem(pane, ID_SECONDARY_LOCK));
    GameResult first = GameLockField(GAME_VEHICLE_PRIMARY, 0, state->colors[0], primary || (carry && !secondary));
    GameResult second = GameLockField(GAME_VEHICLE_SECONDARY, 0, state->colors[1], secondary || (carry && !primary));
    return MergeGameResult(first, second);
}

static BOOL QueueLivePaint(UiPage *page, HWND pane, const DenseState *state, int primary, int secondary)
{
    BOOL carry, lockPrimary, lockSecondary;
    GameResult result = GAME_OK;
    if (!GameIsConnected())
        return FALSE;
    carry = Checked(GetDlgItem(pane, ID_COLOR_LOCK));
    lockPrimary = Checked(GetDlgItem(pane, ID_PRIMARY_LOCK));
    lockSecondary = Checked(GetDlgItem(pane, ID_SECONDARY_LOCK));
    if (primary != state->colors[0])
        result = lockPrimary || (carry && !lockSecondary) ?
                 GameLockField(GAME_VEHICLE_PRIMARY, 0, primary, TRUE) : GameSetField(GAME_VEHICLE_PRIMARY, 0, primary);
    if (secondary != state->colors[1])
        result = MergeGameResult(result, lockSecondary || (carry && !lockPrimary) ?
                 GameLockField(GAME_VEHICLE_SECONDARY, 0, secondary, TRUE) : GameSetField(GAME_VEHICLE_SECONDARY, 0, secondary));
    return DisplayGameResult(page, result);
}

static BOOL VehicleCommand(UiPage *page, DenseState *state, HWND pane, int id)
{
    HWND owner = GetAncestor(page->window, GA_ROOT);
    int selected, model, slot, primary, secondary;
    WCHAR text[100], modelName[81], plate[9];

    switch (id) {
    case ID_COLOR_PRIMARY:
    case ID_COLOR_SECONDARY:
        primary = state->colors[0];
        secondary = state->colors[1];
        if (UiPickVehicleColors(owner, &primary, &secondary, id == ID_COLOR_SECONDARY) &&
            !QueueLivePaint(page, pane, state, primary, secondary)) {
            state->colors[0] = primary;
            state->colors[1] = secondary;
            UpdateColorLabels(pane, state);
            wsprintfW(text, L"离线车身颜色已应用 · 主色 %03d · 副色 %03d", state->colors[0], state->colors[1]);
            UiSetStatus(page, text);
        }
        return TRUE;
    case ID_COLOR_LOCK:
    case ID_PRIMARY_LOCK:
    case ID_SECONDARY_LOCK:
        if (GameIsConnected())
            DisplayCheckedResult(page, GetDlgItem(pane, id), QueuePaintLock(pane, state));
        return TRUE;
    case ID_GAME_FEEDBACK:
        if (GameIsConnected())
            DisplayCheckedResult(page, GetDlgItem(pane, id), GameSetFeedback(Checked(GetDlgItem(pane, id))));
        return TRUE;
    case ID_DONT_BURN:
        {
            HWND explode = GetDlgItem(pane, ID_DONT_EXPLODE);
            BOOL burnProtection = Checked(GetDlgItem(pane, ID_DONT_BURN));
            if (GameIsConnected()) {
                GameResult result = GameSetField(GAME_VEHICLE_DONT_BURN, 0, burnProtection);
                if (result == GAME_QUEUED || result == GAME_OK)
                    result = MergeGameResult(result, GameSetField(GAME_VEHICLE_DONT_EXPLODE, 0,
                                                                  burnProtection || state->preventExplosion));
                if (DisplayCheckedResult(page, GetDlgItem(pane, id), result)) {
                    burnProtection = Checked(GetDlgItem(pane, id));
                    SetChecked(explode, burnProtection || state->preventExplosion);
                    EnableWindow(explode, !burnProtection);
                    return TRUE;
                }
            }
            SetChecked(explode, burnProtection || state->preventExplosion);
            EnableWindow(explode, !burnProtection);
        }
        return TRUE;
    case ID_DONT_EXPLODE:
        if (QueueLiveProtection(page, pane, id, GetDlgItem(pane, id)))
            return TRUE;
        state->preventExplosion = Checked(GetDlgItem(pane, ID_DONT_EXPLODE));
        return TRUE;
    case ID_VEHICLE_DOOR_LOCK:
        if (GameIsConnected()) {
            HWND control = GetDlgItem(pane, id);
            selected = (int)SendDlgItemMessageW(pane, ID_VEHICLE_DOORS, CB_GETCURSEL, 0, 0);
            DisplayCheckedResult(page, control, GameLockField(GAME_VEHICLE_DOORS, 0, selected, Checked(control)));
        }
        return TRUE;
    case ID_PICK_MODEL:
        selected = (int)SendDlgItemMessageW(pane, ID_MODEL, CB_GETCURSEL, 0, 0);
        model = selected >= 0 ? (int)SendDlgItemMessageW(pane, ID_MODEL, CB_GETITEMDATA, selected, 0) : state->model;
        if (UiChooseVehicle(owner, &model, modelName, ARRAYSIZE(modelName)))
            SetModelSelection(GetDlgItem(pane, ID_MODEL), model);
        return TRUE;
    case ID_CHECK_SPAWNER:
        if (SetLiveField(page, GAME_VEHICLE_SPAWNER, 0, 1))
            return TRUE;
        state->spawnerReady = TRUE;
        SetDlgItemTextW(pane, ID_SPAWNER_READY, L"生成器状态：就绪（离线）");
        UiSetStatus(page, L"离线生成器已准备。");
        return TRUE;
    case ID_SPAWNER_AUTO:
        if (GameIsConnected() && DisplayCheckedResult(page, GetDlgItem(pane, id), GameSetField(
                GAME_VEHICLE_SPAWNER, 0, Checked(GetDlgItem(pane, id)))))
            return TRUE;
        state->spawnerReady = Checked(GetDlgItem(pane, id));
        SetDlgItemTextW(pane, ID_SPAWNER_READY, state->spawnerReady ?
                        L"生成器状态：就绪（离线）" : L"生成器状态：等待手动准备（离线）");
        return TRUE;
    case ID_SPAWN_MODEL:
        selected = (int)SendDlgItemMessageW(pane, ID_MODEL, CB_GETCURSEL, 0, 0);
        model = selected >= 0 ? (int)SendDlgItemMessageW(pane, ID_MODEL, CB_GETITEMDATA, selected, 0) : 0;
        if (selected >= 0 && SetLiveField(page, GAME_VEHICLE_SPAWN, 0, model))
            return TRUE;
        if (!state->spawnerReady) {
            UiSetStatus(page, L"请先准备离线生成器。");
        } else if (selected >= 0) {
            BOOL primaryLocked = Checked(GetDlgItem(pane, ID_PRIMARY_LOCK));
            BOOL secondaryLocked = Checked(GetDlgItem(pane, ID_SECONDARY_LOCK));
            BOOL carryColors = Checked(GetDlgItem(pane, ID_COLOR_LOCK));
            state->model = model;
            if (UiVehicleDefaultColors(state->model, &primary, &secondary)) {
                if (!carryColors || (!primaryLocked && secondaryLocked))
                    state->colors[0] = primary;
                if (!carryColors || (!secondaryLocked && primaryLocked))
                    state->colors[1] = secondary;
                UpdateColorLabels(pane, state);
            }
            UpdateCurrentModel(pane, state->model);
            SetStat(pane, VEH_HEALTH, vehicleStats[VEH_HEALTH].initial);
            SetTriple(state->pane[1], VEH_SPEED_X, 0, 0, 0);
            SetTriple(state->pane[1], VEH_SPIN_X, 0, 0, 0);
            UiSetStatus(page, L"已生成示例载具并更新当前载具。");
        }
        return TRUE;
    case ID_APPLY_PLATE:
    case ID_PLATE_LOCK:
        if (id == ID_PLATE_LOCK && !Checked(GetDlgItem(pane, ID_PLATE_LOCK))) {
            if (GameIsConnected())
                DisplayCheckedResult(page, GetDlgItem(pane, id), GameLockPlate(state->plate, FALSE));
            return TRUE;
        }
        if (!NormalizePlate(GetDlgItem(pane, ID_PLATE), plate)) {
            if (id == ID_PLATE_LOCK)
                SetChecked(GetDlgItem(pane, ID_PLATE_LOCK), FALSE);
            UiSetStatus(page, L"车牌需要 1 至 8 个英文字母、数字或空格。");
            SetFocus(GetDlgItem(pane, ID_PLATE));
            return TRUE;
        }
        lstrcpyW(state->plate, plate);
        if (GameIsConnected()) {
            GameResult result = id == ID_PLATE_LOCK ? GameLockPlate(plate, TRUE) : GameSetPlate(plate);
            if (id == ID_PLATE_LOCK ? DisplayCheckedResult(page, GetDlgItem(pane, id), result) :
                                     DisplayGameResult(page, result))
                return TRUE;
        }
        wsprintfW(text, L"离线车牌已应用为「%s」", plate);
        UiSetStatus(page, text);
        return TRUE;
    case ID_REPAIR_VEHICLE:
        if (SetLiveField(page, GAME_VEHICLE_ENGINE, 0, 100))
            return TRUE;
        SetStat(pane, VEH_HEALTH, 100);
        UiSetStatus(page, L"示例载具耐久已恢复。");
        return TRUE;
    case ID_STOP_VEHICLE_SPEED:
    case ID_STOP_VEHICLE_SPIN:
    case ID_FREEZE_VEHICLE:
    case ID_FLIP_VEHICLE:
        if (GameIsConnected()) {
            GameResult result = GAME_OK;
            if (id == ID_FLIP_VEHICLE) {
                result = GameSetField(GAME_VEHICLE_FLIP, 0, 1);
            } else {
                if (id == ID_STOP_VEHICLE_SPEED || id == ID_FREEZE_VEHICLE)
                    result = GameSetField(GAME_VEHICLE_SPEED, GAME_AXIS_ALL, 0);
                if (id == ID_STOP_VEHICLE_SPIN || id == ID_FREEZE_VEHICLE)
                    result = MergeGameResult(result, GameSetField(GAME_VEHICLE_SPIN, GAME_AXIS_ALL, 0));
            }
            if (DisplayGameResult(page, result))
                return TRUE;
        }
        if (id == ID_STOP_VEHICLE_SPEED || id == ID_FREEZE_VEHICLE)
            SetTriple(pane, VEH_SPEED_X, 0, 0, 0);
        if (id == ID_STOP_VEHICLE_SPIN || id == ID_FREEZE_VEHICLE || id == ID_FLIP_VEHICLE)
            SetTriple(pane, VEH_SPIN_X, 0, 0, 0);
        if (id == ID_FLIP_VEHICLE) {
            state->upsideDown = !state->upsideDown;
            UiSetStatus(page, state->upsideDown ? L"示例载具已翻转至倒置。" : L"示例载具已翻回正面。");
        } else {
            UiSetStatus(page, L"已清零所选载具运动值。");
        }
        return TRUE;
    case ID_VEHICLE_DIRECTION_APPLY:
    case ID_VEHICLE_KICK_APPLY:
        ApplyDirection(page, state, pane, id == ID_VEHICLE_KICK_APPLY);
        return TRUE;
    case ID_CLOCK_STOP:
    case ID_CLOCK_REAL:
    case ID_CLOCK_NORMAL:
        selected = id == ID_CLOCK_STOP ? 0 : id == ID_CLOCK_REAL ? 6 : 100;
        if (!SetLiveField(page, GAME_CLOCK_SPEED, 0, selected))
            SetStat(pane, WORLD_CLOCK_SPEED, selected);
        return TRUE;
    case ID_GAME_SPEED_NORMAL:
        if (!SetLiveField(page, GAME_WORLD_SPEED, 0, 100))
            SetStat(pane, WORLD_GAME_SPEED, 100);
        return TRUE;
    case ID_APPLY_WEATHER:
    case ID_WEATHER_LOCK:
        selected = (int)SendDlgItemMessageW(pane, ID_WEATHER, CB_GETCURSEL, 0, 0);
        if (selected < 0 || selected >= (int)ARRAYSIZE(weatherNames))
            return TRUE;
        if (GameIsConnected()) {
            BOOL locked = Checked(GetDlgItem(pane, ID_WEATHER_LOCK));
            GameResult result = id == ID_WEATHER_LOCK || locked ? GameLockField(GAME_WEATHER, 0, selected, locked) :
                                                                  GameSetField(GAME_WEATHER, 0, selected);
            if (id == ID_WEATHER_LOCK ? DisplayCheckedResult(page, GetDlgItem(pane, id), result) :
                                       DisplayGameResult(page, result))
                return TRUE;
        }
        if (id == ID_WEATHER_LOCK && !Checked(GetDlgItem(pane, id)))
            return TRUE;
        if (selected >= 0 && selected < (int)ARRAYSIZE(weatherNames)) {
            state->weather = selected;
            wsprintfW(text, L"离线天气已设为「%s」", weatherNames[selected]);
            UiSetStatus(page, text);
        }
        return TRUE;
    case ID_APPLY_MONEY:
        selected = ReadStat(pane, WORLD_MONEY, GetStatSpec(0, WORLD_MONEY));
        if (SetLiveField(page, GAME_PLAYER_MONEY, 0, selected))
            return TRUE;
        state->money = selected;
        wsprintfW(text, L"离线金钱已更新为 %d", state->money);
        UiSetStatus(page, text);
        return TRUE;
    case ID_CLEAR_CHEATS:
        if (SetLiveField(page, GAME_PLAYER_CHEATED, 0, 0))
            return TRUE;
        state->cheatsUsed = 0;
        UpdateCheatStatus(0);
        UiSetStatus(page, L"离线作弊状态与使用次数已清除。");
        return TRUE;
    case ID_RESYNC:
        if (GameIsConnected()) {
            UiRefreshGame(page);
            return TRUE;
        }
        SetStat(pane, WORLD_MONEY, state->money);
        SendDlgItemMessageW(pane, ID_WEATHER, CB_SETCURSEL, state->weather, 0);
        for (slot = 0; slot < (int)ARRAYSIZE(state->relationship); ++slot)
            SetStat(pane, RELATIONSHIP_FIRST + slot, state->relationship[slot]);
        UiSetStatus(page, L"已读取示例会话中的世界与女友进度。");
        return TRUE;
    case ID_SYNC_RELATIONSHIP:
        if (GameIsConnected()) {
            GameResult result = GAME_OK;
            for (slot = 0; slot < (int)ARRAYSIZE(state->relationship); ++slot) {
                int stat = RELATIONSHIP_FIRST + slot;
                result = MergeGameResult(result, QueueStat(page, pane, stat, ReadStat(pane, stat, &relationshipStats[slot]), -1));
            }
            if (DisplayGameResult(page, result))
                return TRUE;
        }
        for (slot = 0; slot < (int)ARRAYSIZE(state->relationship); ++slot)
            state->relationship[slot] = ReadStat(pane, RELATIONSHIP_FIRST + slot, &relationshipStats[slot]);
        UiSetStatus(page, L"六位女友的进度已同步到示例会话。");
        return TRUE;
    case ID_PREPARE_DATE:
        selected = (int)SendDlgItemMessageW(pane, ID_RELATIONSHIP_DATE, CB_GETCURSEL, 0, 0);
        if (selected < 0 || selected >= (int)ARRAYSIZE(relationshipStats))
            return TRUE;
        if (!GameIsConnected() || !DisplayGameResult(page, GameRunCommand(79 + selected, 17, L"0")))
            UiSetStatus(page, L"连接经典 Steam 版游戏后可准备所选约会。");
        return TRUE;
    }
    return QueueLiveProtection(page, pane, id, GetDlgItem(pane, id));
}

static BOOL PlayerCommand(UiPage *page, DenseState *state, HWND pane, int id, int code, HWND control)
{
    if (id == ID_CURRENT_SLOT && code == CBN_SELCHANGE) {
        int rawSlot = ComboValue(control);
        if (rawSlot < SLOT_UNARMED || rawSlot > SLOT_DETONATOR)
            return TRUE;
        if (GameIsConnected()) {
            GameResult result = GameSetField(GAME_WEAPON_SLOT, 0, rawSlot);
            if (DisplayGameResult(page, result)) {
                if (result == GAME_QUEUED || result == GAME_OK)
                    state->requestedWeaponSlot = rawSlot;
                else if (!SetModelSelection(control, state->currentWeaponSlot))
                    SendMessageW(control, CB_SETCURSEL, (WPARAM)-1, 0);
                return TRUE;
            }
        }
        state->currentWeaponSlot = rawSlot;
        UpdateCurrentWeapon(pane, state);
        return TRUE;
    }
    if (id >= ID_WEAPON_FIRST && id < ID_WEAPON_FIRST + WEAPON_SLOT_COUNT * 5 && GameIsConnected()) {
        int field = (id - ID_WEAPON_FIRST) % 5;
        int slot = (id - ID_WEAPON_FIRST) / 5;
        if (!state->weaponRendering && ((field == 0 && code == CBN_SELCHANGE) ||
                                        ((field == 1 || field == 2) && code == EN_CHANGE))) {
            WeaponSetting *setting = &state->weapons[slot];
            ++setting->editRevision;
            setting->dirty = TRUE;
            UpdateWeaponAction(pane, slot, setting);
            return TRUE;
        }
    }
    if (code != BN_CLICKED)
        return FALSE;
    if (id >= ID_CHEAT_FIRST && id < ID_CHEAT_FIRST + CHEAT_COUNT * 2) {
        int index = (id - ID_CHEAT_FIRST) / 2;
        BOOL lockControl = (id - ID_CHEAT_FIRST) % 2 != 0;
        BOOL value = Checked(control);
        if (GameIsConnected()) {
            GameResult result;
            if (lockControl) {
                result = GameLockField(GAME_PLAYER_CHEAT, index, Checked(GetDlgItem(pane, id - 1)), value);
            } else if (index >= 20) {
                result = GameSetFeature(index - 20, value);
            } else if (Checked(GetDlgItem(pane, id + 1))) {
                result = GameLockField(GAME_PLAYER_CHEAT, index, value, TRUE);
            } else {
                result = GameSetField(GAME_PLAYER_CHEAT, index, value);
            }
            if (DisplayCheckedResult(page, control, result))
                return TRUE;
        }
        if (!lockControl && value)
            RecordCheat(state);
        return TRUE;
    }
    if (id >= ID_WEAPON_FIRST && id < ID_WEAPON_FIRST + WEAPON_SLOT_COUNT * 5) {
        int field = (id - ID_WEAPON_FIRST) % 5;
        int slot = (id - ID_WEAPON_FIRST) / 5;
        if (GameIsConnected() && (field == 3 || field == 4)) {
            WeaponSetting *setting = &state->weapons[slot];
            GameResult result;
            if (field == 4 && setting->applying)
                return TRUE;
            result = field == 3 ? QueueWeaponLock(pane, slot, Checked(control)) : QueueWeaponApply(pane, slot);
            if (field == 4 && (result == GAME_OK || result == GAME_QUEUED)) {
                setting->submittedRevision = setting->editRevision;
                setting->applying = TRUE;
                UpdateWeaponAction(pane, slot, setting);
            }
            if (field == 3 ? DisplayCheckedResult(page, control, result) : DisplayGameResult(page, result))
                return TRUE;
        }
        if (field == 4 || (field == 3 && Checked(control))) {
            WCHAR status[80];
            ApplyWeapon(pane, state, slot);
            wsprintfW(status, L"已应用离线%s槽。", weaponSpecs[slot].category);
            UiSetStatus(page, status);
            return TRUE;
        }
    }
    switch (id) {
    case ID_CHEAT_FIRE:
    case ID_CHEAT_FIRE_LOCK:
        if (GameIsConnected()) {
            BOOL locked = Checked(GetDlgItem(pane, ID_CHEAT_FIRE_LOCK));
            GameResult result = id == ID_CHEAT_FIRE_LOCK || locked ?
                                GameLockField(GAME_PLAYER_CHEAT, 13, Checked(GetDlgItem(pane, ID_CHEAT_FIRE)), locked) :
                                GameSetField(GAME_PLAYER_CHEAT, 13, Checked(control));
            if (DisplayCheckedResult(page, control, result)) {
                SetChecked(GetDlgItem(state->pane[0], id), Checked(control));
                SetChecked(GetDlgItem(state->pane[2], id), Checked(control));
                return TRUE;
            }
        }
        SetChecked(GetDlgItem(state->pane[0], id), Checked(control));
        SetChecked(GetDlgItem(state->pane[2], id), Checked(control));
        if (id == ID_CHEAT_FIRE && Checked(control))
            RecordCheat(state);
        return TRUE;
    case ID_STOP_PED_SPEED:
        if (SetLiveField(page, GAME_PLAYER_SPEED, GAME_AXIS_ALL, 0))
            return TRUE;
        SetTriple(pane, PED_SPEED_X, 0, 0, 0);
        UiSetStatus(page, L"示例主角的各轴速度已清零。");
        return TRUE;
    case ID_PED_DIRECTION_APPLY:
    case ID_PED_KICK_APPLY:
        ApplyDirection(page, state, pane, id == ID_PED_KICK_APPLY);
        return TRUE;
    case ID_AMMO_REFILL:
        if (GameIsConnected() && DisplayGameResult(page, QueueRefill(pane, state)))
            return TRUE;
        RefillAmmo(pane, state);
        UiSetStatus(page, L"已补满已选择武器的示例弹药。");
        return TRUE;
    case ID_READ_WEAPONS:
        if (GameIsConnected()) {
            int skill;
            ReadWeaponSlots(state, TRUE);
            for (skill = 0; skill < UI_WEAPON_SKILLS; ++skill)
                ReadLiveField(GAME_WEAPON_SKILL, skill);
            UiSetStatus(page, L"已请求读取武器和弹药，未应用的修改会保留。");
        } else {
            UiSetStatus(page, GameResultText(GAME_OFFLINE));
        }
        return TRUE;
    case ID_WEAPON_LEVELS:
        {
            int proposed[UI_WEAPON_SKILLS], skill;
            GameResult result = GAME_OK;
            CopyMemory(proposed, state->proficiency, sizeof(proposed));
            if (!UiEditWeaponProficiency(GetAncestor(page->window, GA_ROOT), proposed))
                return TRUE;
            if (GameIsConnected()) {
                for (skill = 0; skill < (int)ARRAYSIZE(proposed); ++skill) {
                    if (proposed[skill] != state->proficiency[skill])
                        result = MergeGameResult(result, GameSetField(GAME_WEAPON_SKILL, skill, proposed[skill]));
                }
                if (DisplayGameResult(page, result))
                    return TRUE;
            }
            CopyMemory(state->proficiency, proposed, sizeof(proposed));
            UpdateWeaponAverage(pane, state);
            UiSetStatus(page, L"详细武器熟练度已更新。");
        }
        return TRUE;
    case ID_COFFEE:
        if (GameIsConnected() && DisplayCheckedResult(page, control, GameSetCoffee(Checked(control))))
            return TRUE;
        if (Checked(control))
            RecordCheat(state);
        return TRUE;
    case ID_BRASS_KNUCKLES:
        if (GameIsConnected())
            DisplayCheckedResult(page, control, GameLockField(GAME_WEAPON_TYPE, -1, 1, Checked(control)));
        return TRUE;
    case ID_LOADED_LOCK:
        if (GameIsConnected()) {
            BOOL enabled = Checked(control);
            int slot = enabled ? state->currentWeaponSlot - 1 : state->loadedLockSlot;
            if (slot < 0 || slot >= WEAPON_SLOT_COUNT || !weaponSpecs[slot].hasAmmo) {
                if (enabled)
                    DisplayCheckedResult(page, control, GAME_INVALID);
            } else {
                int weapon = ID_WEAPON_FIRST + slot * 5;
                int value = ReadNumber(GetDlgItem(pane, weapon + 2), 0, 0, 99999);
                if (!enabled && Checked(GetDlgItem(pane, weapon + 3))) {
                    state->loadedLockSlot = -1;
                } else {
                    GameResult result = GameLockField(GAME_WEAPON_LOADED, slot, value, enabled);
                    if (result == GAME_OK || result == GAME_QUEUED)
                        state->loadedLockSlot = slot;
                    DisplayCheckedResult(page, control, result);
                }
            }
        }
        return TRUE;
    }
    return QueueLiveProtection(page, pane, id, control);
}

BOOL DensePageCommand(UiPage *page, int id, int code, HWND control)
{
    DenseState *state = &states[page->index];
    HWND pane = GetParent(control);

    if (id >= ID_SECTION_FIRST && id < ID_SECTION_FIRST + SECTION_COUNT && code == BN_CLICKED) {
        SelectSection(page, state, id - ID_SECTION_FIRST);
        return TRUE;
    }
    if (page->index == 0 && id == ID_WORLD_DAY && code == EN_KILLFOCUS) {
        int day = ReadNumber(control, state->gameDay, 0, 99999);
        SetNumber(control, day);
        if (day != state->gameDay && !SetLiveField(page, GAME_CLOCK_DAY, 0, day))
            state->gameDay = day;
        UpdateClockLabel(pane, (int)SendDlgItemMessageW(pane, StatId(WORLD_TIME), TBM_GETPOS, 0, 0));
        return TRUE;
    }
    if (id >= ID_STAT_FIRST && id < ID_STAT_FIRST + STAT_SLOT_COUNT * 4) {
        int slot = (id - ID_STAT_FIRST) / 4, field = (id - ID_STAT_FIRST) % 4;
        const StatSpec *spec = GetStatSpec(page->index, slot);
        if (spec && field == 1 && code == EN_KILLFOCUS) {
            int previous = (int)SendDlgItemMessageW(pane, StatId(slot), TBM_GETPOS, 0, 0);
            int value = ReadStat(pane, slot, spec);
            SetStat(pane, slot, value);
            if (value != previous && GameIsConnected())
                DisplayGameResult(page, QueueStat(page, pane, slot, value, -1));
            return TRUE;
        }
        if (spec && field == 2 && code == BN_CLICKED) {
            if (!GameIsConnected() || !DisplayGameResult(page, QueueStat(page, pane, slot, spec->actionValue, -1)))
                SetStat(pane, slot, spec->actionValue);
            return TRUE;
        }
        if (spec && field == 3 && code == BN_CLICKED && GameIsConnected()) {
            DisplayCheckedResult(page, control, QueueStat(page, pane, slot, ReadStat(pane, slot, spec), Checked(control)));
            return TRUE;
        }
    }
    if (page->index == 0) {
        if (id == ID_VEHICLE_DOORS && code == CBN_SELCHANGE && GameIsConnected()) {
            int position = (int)SendMessageW(control, CB_GETCURSEL, 0, 0);
            if (position >= 0) {
                BOOL locked = Checked(GetDlgItem(pane, ID_VEHICLE_DOOR_LOCK));
                DisplayGameResult(page, locked ? GameLockField(GAME_VEHICLE_DOORS, 0, position, TRUE) :
                                                GameSetField(GAME_VEHICLE_DOORS, 0, position));
            }
            return TRUE;
        }
        return code == BN_CLICKED && VehicleCommand(page, state, pane, id);
    }
    return PlayerCommand(page, state, pane, id, code, control);
}

static ULONGLONG WeaponReadBit(GameField field, int index)
{
    int part;
    if (field == GAME_WEAPON_SLOT)
        return 1;
    if (field == GAME_WEAPON_TYPE && index == -1)
        return 2;
    if (index < 0 || index >= WEAPON_SLOT_COUNT)
        return 0;
    switch (field) {
    case GAME_WEAPON_TYPE: part = 0; break;
    case GAME_WEAPON_TOTAL: part = 1; break;
    case GAME_WEAPON_LOADED: part = 2; break;
    default: return 0;
    }
    return (ULONGLONG)1 << (2 + index * 3 + part);
}

static BOOL ReadWeaponField(DenseState *state, GameField field, int index, BOOL newer)
{
    ULONGLONG bit = WeaponReadBit(field, index);
    if (!bit)
        return FALSE;
    if (state->weaponReadsPending & bit) {
        if (newer && !(state->weaponReadAgain & bit) && GameReadField(field, index) == GAME_QUEUED)
            state->weaponReadAgain |= bit;
        return TRUE;
    }
    if (!GameSupportsField(field, index, FALSE) || GameReadField(field, index) != GAME_QUEUED)
        return FALSE;
    state->weaponReadsPending |= bit;
    return TRUE;
}

static void ReadWeaponSlots(DenseState *state, BOOL newer)
{
    int index;
    ReadWeaponField(state, GAME_WEAPON_SLOT, 0, newer);
    ReadWeaponField(state, GAME_WEAPON_TYPE, -1, newer);
    for (index = 0; index < WEAPON_SLOT_COUNT; ++index) {
        ReadWeaponField(state, GAME_WEAPON_TYPE, index, newer);
        if (weaponSpecs[index].hasAmmo) {
            ReadWeaponField(state, GAME_WEAPON_TOTAL, index, newer);
            ReadWeaponField(state, GAME_WEAPON_LOADED, index, newer);
        }
    }
}

static BOOL AcceptWeaponRead(DenseState *state, GameField field, int index)
{
    ULONGLONG bit = WeaponReadBit(field, index);
    if (state->weaponReadAgain & bit) {
        state->weaponReadAgain &= ~bit;
        return FALSE;
    }
    state->weaponReadsPending &= ~bit;
    return TRUE;
}

static void ReadDensePage(UiPage *page, BOOL newer)
{
    const FlagBinding *flags = page->index == 0 ? vehicleFlags : playerFlags;
    int count = page->index == 0 ? (int)ARRAYSIZE(vehicleFlags) : (int)ARRAYSIZE(playerFlags);
    int index;

    for (index = 0; index < STAT_SLOT_COUNT; ++index) {
        const GameBinding *binding = &gameStats[page->index][index];
        if (binding->field != GAME_FIELD_NONE && binding->field != GAME_MISSION_TIMER)
            ReadLiveField(binding->field, binding->index);
    }
    for (index = 0; index < count; ++index)
        ReadLiveField(flags[index].field, flags[index].index);
    if (page->index == 0) {
        static const GameField fields[] = {
            GAME_VEHICLE_MODEL, GAME_VEHICLE_PRIMARY, GAME_VEHICLE_SECONDARY, GAME_VEHICLE_DOORS,
            GAME_VEHICLE_HEADING, GAME_VEHICLE_SPAWNER, GAME_WEATHER, GAME_CLOCK_DAY
        };
        for (index = 0; index < (int)ARRAYSIZE(fields); ++index)
            ReadLiveField(fields[index], 0);
        ReadLiveField(GAME_PLAYER_CHEATED, 1);
    } else {
        ReadLiveField(GAME_PLAYER_HEADING, 0);
        ReadLiveField(GAME_PLAYER_COFFEE, 0);
        ReadWeaponSlots(&states[1], newer);
        for (index = 0; index < UI_WEAPON_SKILLS; ++index)
            ReadLiveField(GAME_WEAPON_SKILL, index);
        for (index = 0; index < CHEAT_COUNT; ++index)
            ReadLiveField(GAME_PLAYER_CHEAT, index);
    }
}

void DensePageGameRefresh(UiPage *page)
{
    if (GameIsConnected())
        ReadDensePage(page, TRUE);
}

void DensePageGamePoll(UiPage *page)
{
    if (page->index == 1 && states[1].activePane == 1 && GameIsConnected())
        ReadWeaponSlots(&states[1], FALSE);
}

static void ClearLiveLocks(UiPage *page, const DenseState *state)
{
    int slot;
    for (slot = 0; slot < STAT_SLOT_COUNT; ++slot)
        SetChecked(GetDlgItem(StatPane(page->index, slot), StatId(slot) + 3), FALSE);
    if (page->index == 0) {
        SetChecked(GetDlgItem(state->pane[0], ID_COLOR_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[0], ID_PRIMARY_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[0], ID_SECONDARY_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[0], ID_VEHICLE_DOOR_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[0], ID_VEHICLE_DAMAGE_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[0], ID_PLATE_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[2], ID_WEATHER_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[2], ID_GAME_FEEDBACK), FALSE);
    } else {
        SetChecked(GetDlgItem(state->pane[0], ID_PLAYER_DAMAGE_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[0], ID_CHEAT_FIRE_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[2], ID_CHEAT_FIRE_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[1], ID_LOADED_LOCK), FALSE);
        SetChecked(GetDlgItem(state->pane[1], ID_BRASS_KNUCKLES), FALSE);
        for (slot = 0; slot < WEAPON_SLOT_COUNT; ++slot)
            SetChecked(GetDlgItem(state->pane[1], ID_WEAPON_FIRST + slot * 5 + 3), FALSE);
        for (slot = 0; slot < CHEAT_COUNT; ++slot) {
            if (slot != 13)
                SetChecked(GetDlgItem(state->pane[2], ID_CHEAT_FIRST + slot * 2 + 1), FALSE);
        }
    }
}

static BOOL LiveStatEvent(UiPage *page, DenseState *state, const GameEvent *event, int value)
{
    int slot;
    for (slot = 0; slot < STAT_SLOT_COUNT; ++slot) {
        const GameBinding *binding = &gameStats[page->index][slot];
        HWND pane;
        int controlValue;
        if (binding->field != event->field || binding->index != event->index)
            continue;
        pane = StatPane(page->index, slot);
        if (event->result == GAME_OK) {
            if ((event->request != GAME_REQ_LOCK_FIELD || event->mode != 0) &&
                (event->request != GAME_REQ_GIRLFRIEND || event->mode != 0)) {
                controlValue = page->index == 0 && slot == VEH_WEIGHT ? (int)(event->value * 10.0 + 0.5) : value;
                if (GetFocus() != GetDlgItem(pane, StatId(slot) + 1))
                    SetStat(pane, slot, controlValue);
                if (page->index == 0 && slot == WORLD_MONEY)
                    state->money = value;
            }
        } else if (event->request == GAME_REQ_LOCK_FIELD || event->request == GAME_REQ_GIRLFRIEND) {
            SetChecked(GetDlgItem(pane, StatId(slot) + 3), event->mode == 0);
        }
        return TRUE;
    }
    return FALSE;
}

static BOOL LiveFlagEvent(UiPage *page, DenseState *state, const GameEvent *event)
{
    const FlagBinding *flags = page->index == 0 ? vehicleFlags : playerFlags;
    int count = page->index == 0 ? (int)ARRAYSIZE(vehicleFlags) : (int)ARRAYSIZE(playerFlags);
    int index;
    for (index = 0; index < count; ++index) {
        const FlagBinding *binding = &flags[index];
        if (binding->field == event->field && binding->index == event->index) {
            HWND pane = state->pane[0], control = GetDlgItem(pane, binding->id);
            if (event->result == GAME_OK) {
                if (event->request != GAME_REQ_LOCK_FIELD || event->mode != 0)
                    SetChecked(control, event->value != 0.0);
                if (binding->id == ID_DONT_BURN)
                    EnableWindow(GetDlgItem(pane, ID_DONT_EXPLODE), event->value == 0.0);
                if (binding->id == ID_DONT_EXPLODE && !Checked(GetDlgItem(pane, ID_DONT_BURN)))
                    state->preventExplosion = event->value != 0.0;
            } else if (event->request == GAME_REQ_SET_FIELD) {
                SetChecked(control, event->value == 0.0);
            } else if (event->request == GAME_REQ_LOCK_FIELD) {
                SetChecked(GetDlgItem(pane, page->index == 0 ? ID_VEHICLE_DAMAGE_LOCK : ID_PLAYER_DAMAGE_LOCK),
                           event->mode == 0);
            }
            return TRUE;
        }
    }
    return FALSE;
}

static void UpdateLiveWeaponField(DenseState *state, GameField field, int index, int value)
{
    HWND pane = state->pane[1];
    int id;
    WeaponSetting *setting;
    if (field == GAME_WEAPON_SKILL) {
        if (index >= 0 && index < UI_WEAPON_SKILLS) {
            state->proficiency[index] = value;
            UpdateWeaponAverage(pane, state);
        }
    } else if (field == GAME_WEAPON_TYPE && index == -1) {
        state->unarmedType = value;
        SetChecked(GetDlgItem(pane, ID_BRASS_KNUCKLES), value != 0);
    } else if (index >= 0 && index < WEAPON_SLOT_COUNT) {
        WCHAR text[16];
        id = ID_WEAPON_FIRST + index * 5;
        setting = &state->weapons[index];
        state->weaponRendering = TRUE;
        if (field == GAME_WEAPON_TYPE) {
            HWND combo = GetDlgItem(pane, id);
            setting->type = value;
            if (!setting->dirty && !SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0) &&
                ComboValue(combo) != value && !SetModelSelection(combo, value))
                SendMessageW(combo, CB_SETCURSEL, (WPARAM)-1, 0);
        } else if (field == GAME_WEAPON_TOTAL) {
            setting->total = value;
            if (weaponSpecs[index].hasAmmo && !setting->dirty && GetFocus() != GetDlgItem(pane, id + 1)) {
                wsprintfW(text, L"%d", value);
                ChangeLabel(pane, id + 1, text);
            }
        } else if (field == GAME_WEAPON_LOADED) {
            setting->loaded = value;
            if (weaponSpecs[index].hasAmmo && !setting->dirty && GetFocus() != GetDlgItem(pane, id + 2)) {
                wsprintfW(text, L"%d", value);
                ChangeLabel(pane, id + 2, text);
            }
        }
        state->weaponRendering = FALSE;
    }
}

static void ApplyCurrentWeaponRead(DenseState *state, const GameEvent *event, int value)
{
    HWND pane = state->pane[1];
    HWND combo = GetDlgItem(pane, ID_CURRENT_SLOT);
    if (event->request == GAME_REQ_SET_FIELD) {
        if (state->requestedWeaponSlot >= 0 && value != state->requestedWeaponSlot)
            return;
        state->requestedWeaponSlot = -1;
    } else if (state->requestedWeaponSlot >= 0) {
        return;
    }
    if (event->result == GAME_OK && value >= SLOT_UNARMED && value <= SLOT_DETONATOR)
        state->currentWeaponSlot = value;
    else if (event->request == GAME_REQ_READ_FIELD && event->result == GAME_NOT_READY)
        state->currentWeaponSlot = -1;
    if (!SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0)) {
        if (state->currentWeaponSlot < 0)
            SendMessageW(combo, CB_SETCURSEL, (WPARAM)-1, 0);
        else if (ComboValue(combo) != state->currentWeaponSlot)
            SetModelSelection(combo, state->currentWeaponSlot);
    }
    UpdateCurrentWeapon(pane, state);
}

static void ShowLiveCheat(DenseState *state, int index, BOOL checked, BOOL lock)
{
    if (index == 13) {
        int id = lock ? ID_CHEAT_FIRE_LOCK : ID_CHEAT_FIRE;
        SetChecked(GetDlgItem(state->pane[0], id), checked);
        SetChecked(GetDlgItem(state->pane[2], id), checked);
    } else if (index >= 0 && index < CHEAT_COUNT) {
        SetChecked(GetDlgItem(state->pane[2], ID_CHEAT_FIRST + index * 2 + (lock ? 1 : 0)), checked);
    }
}

static BOOL ReadPlayerEvent(DenseState *state, const GameEvent *event, int value)
{
    if (event->field == GAME_PLAYER_COFFEE) {
        if (event->result == GAME_OK)
            SetChecked(GetDlgItem(state->pane[2], ID_COFFEE), value != 0);
        return TRUE;
    }
    if (event->field == GAME_PLAYER_CHEAT) {
        if (event->result == GAME_OK) {
            if (event->request != GAME_REQ_LOCK_FIELD || event->mode)
                ShowLiveCheat(state, event->index, value != 0, FALSE);
        } else if (event->request == GAME_REQ_LOCK_FIELD) {
            ShowLiveCheat(state, event->index, event->mode == 0, TRUE);
        } else if (event->request == GAME_REQ_SET_FIELD) {
            ShowLiveCheat(state, event->index, value == 0, FALSE);
        }
        if (event->request == GAME_REQ_SET_FIELD && event->result == GAME_OK && value &&
            Checked(GetDlgItem(state->pane[2], ID_SAFE_CHEATS)))
            GameSetField(GAME_PLAYER_CHEATED, 0, 0);
        return TRUE;
    }
    if (event->field == GAME_WEAPON_SLOT || event->field == GAME_WEAPON_SKILL || event->field == GAME_WEAPON_TYPE ||
        event->field == GAME_WEAPON_TOTAL || event->field == GAME_WEAPON_LOADED) {
        if (event->request == GAME_REQ_READ_FIELD && !AcceptWeaponRead(state, event->field, event->index))
            return TRUE;
        if (event->field == GAME_WEAPON_SLOT) {
            ApplyCurrentWeaponRead(state, event, value);
        } else if (event->result == GAME_OK && (event->request != GAME_REQ_LOCK_FIELD || event->mode)) {
            if (event->field == GAME_WEAPON_TYPE && event->index == -1 && event->request == GAME_REQ_READ_FIELD)
                state->unarmedType = value;
            else
                UpdateLiveWeaponField(state, event->field, event->index, value);
            UpdateCurrentWeapon(state->pane[1], state);
        }
        if (event->result != GAME_OK && event->request == GAME_REQ_LOCK_FIELD) {
            if (event->index == -1) {
                SetChecked(GetDlgItem(state->pane[1], ID_BRASS_KNUCKLES), event->mode == 0);
            } else if (event->index >= 0 && event->index < WEAPON_SLOT_COUNT) {
                SetChecked(GetDlgItem(state->pane[1], ID_WEAPON_FIRST + event->index * 5 + 3), event->mode == 0);
            }
        }
        if (event->field == GAME_WEAPON_LOADED && event->request == GAME_REQ_LOCK_FIELD &&
            state->loadedLockSlot == event->index) {
            HWND loaded = GetDlgItem(state->pane[1], ID_LOADED_LOCK);
            if (event->result == GAME_OK) {
                if (!event->mode)
                    state->loadedLockSlot = -1;
            } else if (Checked(loaded) == (event->mode != 0)) {
                SetChecked(loaded, !event->mode);
                if (event->mode)
                    state->loadedLockSlot = -1;
            }
        }
        return TRUE;
    }
    if (event->field == GAME_PLAYER_SPEED && event->index == GAME_AXIS_ALL) {
        if (event->result == GAME_OK)
            SetTriple(state->pane[0], PED_SPEED_X, value, value, value);
        return TRUE;
    }
    if (event->field == GAME_PLAYER_HEADING || event->field == GAME_PLAYER_KICK) {
        if (event->result == GAME_OK) {
            state->heading = event->field == GAME_PLAYER_KICK ? event->index : value;
            SendDlgItemMessageW(state->pane[0], ID_PED_DIRECTION, CB_SETCURSEL, (state->heading + 22) / 45 % 8, 0);
            if (event->field == GAME_PLAYER_KICK) {
                ReadLiveField(GAME_PLAYER_SPEED, GAME_AXIS_X);
                ReadLiveField(GAME_PLAYER_SPEED, GAME_AXIS_Y);
                ReadLiveField(GAME_PLAYER_SPEED, GAME_AXIS_Z);
            }
        }
        return TRUE;
    }
    return FALSE;
}

static BOOL ReadVehicleWorldEvent(DenseState *state, const GameEvent *event, int value)
{
    HWND vehiclePane = state->pane[0], worldPane = state->pane[2];
    if (event->result != GAME_OK) {
        if (event->request == GAME_REQ_LOCK_FIELD && event->field == GAME_VEHICLE_DOORS)
            SetChecked(GetDlgItem(vehiclePane, ID_VEHICLE_DOOR_LOCK), event->mode == 0);
        if (event->request == GAME_REQ_LOCK_FIELD && event->field == GAME_WEATHER)
            SetChecked(GetDlgItem(worldPane, ID_WEATHER_LOCK), event->mode == 0);
    }
    switch (event->field) {
    case GAME_VEHICLE_MODEL:
        if (event->result == GAME_OK) {
            state->model = value;
            if (value == 0)
                SetDlgItemTextW(vehiclePane, ID_CURRENT_MODEL, L"当前载具：无");
            else
                UpdateCurrentModel(vehiclePane, value);
        } else if (event->request == GAME_REQ_READ_FIELD) {
            SetDlgItemTextW(vehiclePane, ID_CURRENT_MODEL, L"当前载具：不可用");
        }
        return TRUE;
    case GAME_VEHICLE_PRIMARY:
    case GAME_VEHICLE_SECONDARY:
        if (event->result == GAME_OK && (event->request != GAME_REQ_LOCK_FIELD || event->mode)) {
            state->colors[event->field == GAME_VEHICLE_PRIMARY ? 0 : 1] = value;
            UpdateColorLabels(vehiclePane, state);
        }
        return TRUE;
    case GAME_VEHICLE_DOORS:
        if (event->result == GAME_OK && (event->request != GAME_REQ_LOCK_FIELD || event->mode))
            SendDlgItemMessageW(vehiclePane, ID_VEHICLE_DOORS, CB_SETCURSEL, value != 0, 0);
        return TRUE;
    case GAME_VEHICLE_SPEED:
    case GAME_VEHICLE_SPIN:
        if (event->index != GAME_AXIS_ALL)
            return FALSE;
        if (event->result == GAME_OK)
            SetTriple(state->pane[1], event->field == GAME_VEHICLE_SPEED ? VEH_SPEED_X : VEH_SPIN_X, value, value, value);
        return TRUE;
    case GAME_VEHICLE_HEADING:
    case GAME_VEHICLE_KICK:
        if (event->result == GAME_OK) {
            state->heading = event->field == GAME_VEHICLE_KICK ? event->index : value;
            SendDlgItemMessageW(state->pane[1], ID_VEHICLE_DIRECTION, CB_SETCURSEL,
                               (state->heading + 22) / 45 % 8, 0);
            if (event->field == GAME_VEHICLE_KICK) {
                ReadLiveField(GAME_VEHICLE_SPEED, GAME_AXIS_X);
                ReadLiveField(GAME_VEHICLE_SPEED, GAME_AXIS_Y);
                ReadLiveField(GAME_VEHICLE_SPEED, GAME_AXIS_Z);
            }
        }
        return TRUE;
    case GAME_VEHICLE_SPAWNER:
        if (event->result == GAME_OK) {
            state->spawnerReady = value != 0;
            SetDlgItemTextW(vehiclePane, ID_SPAWNER_READY, state->spawnerReady ?
                            L"生成器状态：已就绪" : L"生成器状态：未就绪");
        }
        return TRUE;
    case GAME_VEHICLE_SPAWN:
        if (event->result == GAME_OK) {
            ReadLiveField(GAME_VEHICLE_MODEL, 0);
            ReadLiveField(GAME_VEHICLE_PRIMARY, 0);
            ReadLiveField(GAME_VEHICLE_SECONDARY, 0);
        }
        return TRUE;
    case GAME_VEHICLE_FLIP:
        return TRUE;
    case GAME_WEATHER:
        if (event->result == GAME_OK && (event->request != GAME_REQ_LOCK_FIELD || event->mode)) {
            state->weather = value;
            if (value >= 0 && value < (int)ARRAYSIZE(weatherNames))
                SendDlgItemMessageW(worldPane, ID_WEATHER, CB_SETCURSEL, value, 0);
        }
        return TRUE;
    case GAME_CLOCK_DAY:
        if (event->result == GAME_OK) {
            state->gameDay = value;
            if (GetFocus() != GetDlgItem(worldPane, ID_WORLD_DAY))
                SetNumber(GetDlgItem(worldPane, ID_WORLD_DAY), value);
            UpdateClockLabel(worldPane, (int)SendDlgItemMessageW(worldPane, StatId(WORLD_TIME), TBM_GETPOS, 0, 0));
        }
        return TRUE;
    case GAME_PLAYER_CHEATED:
        if (event->result == GAME_OK) {
            state->cheatsUsed = value;
            UpdateCheatStatus(value);
        }
        return TRUE;
    }
    return FALSE;
}

void DensePageGameEvent(UiPage *page, const GameEvent *event)
{
    DenseState *state = &states[page->index];
    BOOL handled = FALSE;
    int value = 0;

    if (event->value >= (double)INT_MAX - 0.5)
        value = INT_MAX;
    else if (event->value <= (double)INT_MIN + 0.5)
        value = INT_MIN;
    else if (event->value == event->value)
        value = (int)(event->value < 0 ? event->value - 0.5 : event->value + 0.5);
    if (event->request == GAME_REQ_SESSION) {
        BOOL connected = event->result == GAME_OK && value != 0;
        int slot;
        if (connected == state->liveSession)
            return;
        state->liveSession = connected;
        state->liveFailure = FALSE;
        state->loadedLockSlot = -1;
        state->requestedWeaponSlot = -1;
        state->weaponReadsPending = state->weaponReadAgain = 0;
        if (page->index == 1) {
            for (slot = 0; slot < WEAPON_SLOT_COUNT; ++slot) {
                state->weapons[slot].applying = FALSE;
                UpdateWeaponAction(state->pane[1], slot, &state->weapons[slot]);
            }
        }
        ClearLiveLocks(page, state);
        if (page->index == 0) {
            SetDlgItemTextW(state->pane[2], ID_GAME_VERSION, connected ? L"Steam 版（已连接）" : L"Steam 版（离线）");
            SetDlgItemTextW(state->pane[0], ID_SPAWNER_READY, connected ?
                            L"生成器状态：等待可用性确认" : L"生成器状态：就绪（离线）");
        }
        if (connected)
            ReadDensePage(page, FALSE);
        return;
    }
    if (!GameIsConnected())
        return;
    switch (event->request) {
    case GAME_REQ_READ_FIELD:
    case GAME_REQ_SET_FIELD:
    case GAME_REQ_LOCK_FIELD:
        if (event->field != GAME_FIELD_NONE) {
            handled = LiveStatEvent(page, state, event, value) || LiveFlagEvent(page, state, event);
            if (!handled)
                handled = page->index == 0 ? ReadVehicleWorldEvent(state, event, value) :
                                             ReadPlayerEvent(state, event, value);
        }
        break;
    case GAME_REQ_GIRLFRIEND:
        if (page->index == 0) {
            GameEvent normalized = *event;
            normalized.field = GAME_RELATIONSHIP;
            handled = LiveStatEvent(page, state, &normalized, value);
        }
        break;
    case GAME_REQ_FEATURE:
        if (page->index == 1) {
            ShowLiveCheat(state, event->index + 20, event->result == GAME_OK ? value != 0 : value == 0, FALSE);
            handled = TRUE;
        }
        break;
    case GAME_REQ_COFFEE:
        if (page->index == 1) {
            SetChecked(GetDlgItem(state->pane[2], ID_COFFEE), event->result == GAME_OK ? value != 0 : value == 0);
            handled = TRUE;
        }
        break;
    case GAME_REQ_FEEDBACK:
        if (page->index == 0) {
            SetChecked(GetDlgItem(state->pane[2], ID_GAME_FEEDBACK), event->result == GAME_OK ? value != 0 : value == 0);
            handled = TRUE;
        }
        break;
    case GAME_REQ_PLATE:
        if (page->index == 0) {
            if (event->mode != 0 && event->result != GAME_QUEUED) {
                BOOL enabled = event->result == GAME_OK ? event->mode == 1 : event->mode == 2;
                SetChecked(GetDlgItem(state->pane[0], ID_PLATE_LOCK), enabled);
            }
            handled = TRUE;
        }
        break;
    case GAME_REQ_WEAPON:
        if (page->index == 1 && event->index >= 0 && event->index < WEAPON_SLOT_COUNT) {
            int slot = event->index;
            WeaponSetting *setting = &state->weapons[slot];
            if (event->result == GAME_OK) {
                if (setting->applying && setting->submittedRevision == setting->editRevision)
                    setting->dirty = FALSE;
                UpdateLiveWeaponField(state, GAME_WEAPON_TYPE, slot, (int)event->weapon.type);
                UpdateLiveWeaponField(state, GAME_WEAPON_TOTAL, slot, (int)event->weapon.total);
                UpdateLiveWeaponField(state, GAME_WEAPON_LOADED, slot, (int)event->weapon.loaded);
                UpdateCurrentWeapon(state->pane[1], state);
            } else if (event->result != GAME_QUEUED) {
                ReadWeaponField(state, GAME_WEAPON_TYPE, slot, TRUE);
                if (weaponSpecs[slot].hasAmmo) {
                    ReadWeaponField(state, GAME_WEAPON_TOTAL, slot, TRUE);
                    ReadWeaponField(state, GAME_WEAPON_LOADED, slot, TRUE);
                }
            }
            if (event->result != GAME_QUEUED) {
                setting->applying = FALSE;
                UpdateWeaponAction(state->pane[1], slot, setting);
            }
            handled = TRUE;
        }
        break;
    }
    if (handled && event->request != GAME_REQ_READ_FIELD) {
        if (event->result != GAME_OK && event->result != GAME_QUEUED) {
            state->liveFailure = TRUE;
            if (event->request == GAME_REQ_SET_FIELD || event->request == GAME_REQ_LOCK_FIELD) {
                if (page->index == 1 && WeaponReadBit(event->field, event->index))
                    ReadWeaponField(state, event->field, event->index, TRUE);
                else
                    ReadLiveField(event->field, event->index);
            }
        }
        if (event->result != GAME_OK || !state->liveFailure)
            UiSetStatus(page, GameResultText(event->result));
    }
}
