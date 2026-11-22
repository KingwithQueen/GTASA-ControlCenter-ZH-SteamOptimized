/* Execute only while the verified single-player window has the foreground. */
#ifndef GTASA_SHORTCUT_GAME_H
#define GTASA_SHORTCUT_GAME_H

enum { MARKUP_LOCATION_COUNT = 10 };
static GamePosition markedPositions[MARKUP_LOCATION_COUNT];
static BOOL markedPositionValid[MARKUP_LOCATION_COUNT];
static int locationCursor = -1;

static GameResult TeleportToRecord(const Record *record)
{
    GamePosition position;
    if (!record->validPoint || !ReadNumber(record->field[2], &position.x)
        || !ReadNumber(record->field[3], &position.y)
        || !ReadNumber(record->field[4], &position.z)
        || !ReadNumber(record->field[5], &position.heading))
        return GAME_INVALID;
    return GameTeleport(&position);
}

static const Record *FindRecordByUid(int page, LPCWSTR uid)
{
    const RecordSet *set = RecordsFor(page);
    int index;
    for (index = 0; index < set->count; ++index)
        if (_wcsicmp(set->rows[index].uid, uid) == 0)
            return &set->rows[index];
    return NULL;
}

static BOOL ReadShortcutInteger(LPCWSTR text, long minimum, long maximum, int *value)
{
    WCHAR *end;
    long number = wcstol(text, &end, 10);
    if (end == text || *end || number < minimum || number > maximum)
        return FALSE;
    *value = (int)number;
    return TRUE;
}

static GameResult RunLocationCommand(int command, LPCWSTR parameter)
{
    const RecordSet *locations;
    GameResult result;
    int index;
    if (command == 63 || command == 64) {
        if (!ReadShortcutInteger(parameter, 1, MARKUP_LOCATION_COUNT, &index))
            return GAME_INVALID;
        --index;
        if (command == 64)
            return markedPositionValid[index] ? GameTeleport(&markedPositions[index]) : GAME_NOT_READY;
        if (positionFromEditor || pendingMarkup >= 0)
            return GAME_BUSY;
        result = GameReadPosition();
        if (result == GAME_QUEUED)
            pendingMarkup = index;
        return result;
    }
    locations = RecordsFor(PAGE_LOCATIONS);
    if (!locations->count)
        return GAME_NOT_READY;
    if (locationCursor < 0 || locationCursor >= locations->count)
        locationCursor = command == 77 ? locations->count - 1 : 0;
    index = (locationCursor + (command == 77 ? 1 : locations->count - 1)) % locations->count;
    result = TeleportToRecord(&locations->rows[index]);
    if (result == GAME_QUEUED)
        locationCursor = index;
    return result;
}

static GameResult RunShortcutRecord(const Record *row)
{
    const Record *target;
    int command;
    if (row->category == 1 || row->category == 2) {
        target = FindRecordByUid(row->category == 1 ? PAGE_CHEATS : PAGE_LOCATIONS, row->command);
        if (!target)
            return GAME_INVALID;
        return row->category == 1 ? GameQueueCheat(target->field[2]) : TeleportToRecord(target);
    }
    if (row->category != 0 || !ReadShortcutInteger(row->command, 0, 129, &command))
        return GAME_INVALID;
    if (command == 63 || command == 64 || command == 77 || command == 78)
        return RunLocationCommand(command, row->field[3]);
    return GameRunCommand(command, row->parameterPage, row->field[3]);
}

static BOOL ShortcutKeyDown(const Record *row, BYTE keys[256], BOOL hasControl, BOOL hasAlt)
{
    if (!row->enabled || row->key < 1 || row->key > 255)
        return FALSE;
    if (row->modifier == VK_CONTROL && (!hasControl || hasAlt))
        return FALSE;
    if (row->modifier == VK_MENU && (!hasAlt || hasControl))
        return FALSE;
    if (!row->modifier && ((hasControl && row->key != VK_CONTROL) || (hasAlt && row->key != VK_MENU)))
        return FALSE;
    if (row->modifier && row->modifier != VK_CONTROL && row->modifier != VK_MENU)
        return FALSE;
    if (!keys[row->key])
        keys[row->key] = (GetAsyncKeyState(row->key) & 0x8000) ? 2 : 1;
    return keys[row->key] == 2;
}

void ListPageGameTick(UiPage *page)
{
    static BYTE wasDown[MAX_ROWS];
    static BOOL wasForeground;
    static int revision;
    static DWORD lastCheck;
    const RecordSet *set;
    const Record *row;
    BYTE keys[256] = { 0 };
    BOOL first, down, hasControl, hasAlt;
    DWORD now;
    int interval, index;
    GameResult result;
    if (page->index != PAGE_SHORTCUTS || !GameIsConnected() || !GameIsForeground()) {
        wasForeground = FALSE;
        return;
    }
    now = GetTickCount();
    interval = ComboValue(GetDlgItem(page->window, ID_SHORTCUT_INTERVAL));
    if (interval < 10 || interval > 500)
        interval = 100;
    first = !wasForeground || revision != collectionRevision[PAGE_SHORTCUTS - PAGE_CHEATS];
    if (!first && now - lastCheck < (DWORD)interval)
        return;
    wasForeground = TRUE;
    lastCheck = now;
    revision = collectionRevision[PAGE_SHORTCUTS - PAGE_CHEATS];
    hasControl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    hasAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    set = RecordsFor(PAGE_SHORTCUTS);
    for (index = 0; index < set->count; ++index) {
        row = &set->rows[index];
        down = ShortcutKeyDown(row, keys, hasControl, hasAlt);
        if (!first && down && !wasDown[index] && GameIsForeground()) {
            result = RunShortcutRecord(row);
            if (result != GAME_QUEUED)
                GameFeedback(page, result);
        }
        wasDown[index] = (BYTE)down;
    }
}

static void ResetGameLocations(void)
{
    ZeroMemory(markedPositionValid, sizeof(markedPositionValid));
    locationCursor = -1;
    pendingMarkup = -1;
}

#endif
