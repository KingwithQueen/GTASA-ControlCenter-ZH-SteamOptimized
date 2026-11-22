/* Used by list_pages.c for the legacy text and INI formats. */
#ifndef GTASA_LEGACY_LIST_DATA_H
#define GTASA_LEGACY_LIST_DATA_H

#include <commdlg.h>
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>

enum {
    FIELD_LENGTH = 192, MAX_ROWS = 2048, MAX_GROUPS = 160,
    GARAGE_COUNT = 17, SLOT_COUNT = 4, GARAGE_VALUE_COUNT = 32,
    DATA_GARAGES = 5213, DATA_CHEATS = 5210, DATA_LOCATIONS = 5211, DATA_SHORTCUTS = 5212
};
enum { PAGE_GARAGES = 2, PAGE_CHEATS, PAGE_LOCATIONS, PAGE_SHORTCUTS };

typedef struct {
    WCHAR uid[37];
    WCHAR field[6][FIELD_LENGTH];
    WCHAR command[FIELD_LENGTH];
    BOOL enabled;
    int category, key, modifier, parameterPage;
    POINT point;
    BOOL validPoint;
} Record;

typedef struct {
    Record *rows;
    int count;
} RecordSet;

typedef struct {
    DWORD field[GARAGE_VALUE_COUNT];
} GarageSlot;

typedef struct {
    GarageSlot slots[SLOT_COUNT];
    BOOL locked;
} Garage;

typedef struct {
    int id, parameterPage;
    LPCWSTR caption;
} CommandSpec;

#include "shortcut_commands.h"

static const LPCWSTR dataHeaders[] = { L"GTASACheats", L"GTASAWarpLocations", L"GTASAShortcuts" };
static const LPCWSTR rootFolders[] = { L"GTA SA Cheats", L"GTA SA Locations", L"GTA SA Shortcuts" };
static const int dataResources[] = { DATA_CHEATS, DATA_LOCATIONS, DATA_SHORTCUTS };

static RecordSet collections[3];
static Garage garageData[GARAGE_COUNT], mockGarages[GARAGE_COUNT];

static RecordSet *RecordsFor(int page)
{
    return &collections[page - PAGE_CHEATS];
}

static void CopyField(WCHAR *destination, LPCWSTR source)
{
    StringCchCopyW(destination, FIELD_LENGTH, source);
}

static WCHAR *DecodeText(const char *bytes, DWORD size)
{
    WCHAR *decoded;
    int count, result;
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;

    if (!bytes || !size || size > 4 * 1024 * 1024)
        return NULL;
    if (size >= 2 && (BYTE)bytes[0] == 0xFF && (BYTE)bytes[1] == 0xFE) {
        if (size % 2 != 0)
            return NULL;
        count = (int)(size / 2) - 1;
        decoded = calloc((size_t)count + 1, sizeof(*decoded));
        if (decoded)
            memcpy(decoded, bytes + 2, count * sizeof(*decoded));
        return decoded;
    }
    if (size >= 3 && (BYTE)bytes[0] == 0xEF && (BYTE)bytes[1] == 0xBB && (BYTE)bytes[2] == 0xBF) {
        bytes += 3;
        size -= 3;
    }
    count = MultiByteToWideChar(codePage, flags, bytes, (int)size, NULL, 0);
    if (!count) {
        codePage = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(codePage, flags, bytes, (int)size, NULL, 0);
    }
    if (!count)
        return NULL;
    decoded = calloc((size_t)count + 1, sizeof(*decoded));
    if (!decoded)
        return NULL;
    result = MultiByteToWideChar(codePage, flags, bytes, (int)size, decoded, count);
    if (!result) {
        free(decoded);
        return NULL;
    }
    return decoded;
}

static WCHAR *ResourceText(int id)
{
    DWORD size = 0;
    const char *bytes = UiResourceBytes(id, &size);
    return DecodeText(bytes, size);
}

static WCHAR *ReadDataFile(LPCWSTR path)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size, read;
    char *buffer;
    WCHAR *decoded = NULL;
    if (file == INVALID_HANDLE_VALUE)
        return NULL;
    size = GetFileSize(file, NULL);
    if (size && size <= 4 * 1024 * 1024) {
        buffer = malloc(size);
        if (buffer) {
            if (ReadFile(file, buffer, size, &read, NULL) && read == size)
                decoded = DecodeText(buffer, size);
            free(buffer);
        }
    }
    CloseHandle(file);
    return decoded;
}

static WCHAR *TakeLine(WCHAR **position)
{
    WCHAR *line = *position;
    WCHAR *end;
    if (!line || !*line)
        return NULL;
    end = wcschr(line, L'\n');
    if (end) {
        *end = L'\0';
        *position = end + 1;
    } else {
        end = line + wcslen(line);
        *position = end;
    }
    if (end > line && end[-1] == L'\r')
        end[-1] = L'\0';
    return line;
}

static BOOL SplitFields(WCHAR *line, WCHAR separator, WCHAR **fields, int count)
{
    int field;
    WCHAR *next;
    for (field = 0; field + 1 < count; ++field) {
        fields[field] = line;
        next = wcschr(line, separator);
        if (!next)
            return FALSE;
        *next = L'\0';
        line = next + 1;
    }
    fields[count - 1] = line;
    return TRUE;
}

static BOOL ValidUid(LPCWSTR uid)
{
    if (wcslen(uid) != 36)
        return FALSE;
    return wcsspn(uid, L"0123456789ABCDEFabcdef-") == 36
        && uid[8] == L'-' && uid[13] == L'-' && uid[18] == L'-' && uid[23] == L'-';
}

static BOOL CreateUid(WCHAR *destination)
{
    GUID id;
    WCHAR formatted[40];
    if (FAILED(CoCreateGuid(&id)) || StringFromGUID2(&id, formatted, ARRAYSIZE(formatted)) != 39)
        return FALSE;
    return SUCCEEDED(StringCchCopyNW(destination, 37, formatted + 1, 36));
}

static BOOL ReadNumber(LPCWSTR text, double *value)
{
    LPWSTR end;
    *value = wcstod(text, &end);
    return end != text && !*end && *value >= -10000000.0 && *value <= 10000000.0;
}

static BOOL ReadPoint(LPCWSTR xText, LPCWSTR yText, POINT *point)
{
    double x, y;
    if (!ReadNumber(xText, &x) || !ReadNumber(yText, &y)
        || x < -3000.0 || x > 3000.0 || y < -3000.0 || y > 3000.0)
        return FALSE;
    point->x = (LONG)(x + (x >= 0 ? 0.5 : -0.5));
    point->y = (LONG)(y + (y >= 0 ? 0.5 : -0.5));
    return TRUE;
}

static void RecordPoint(Record *row)
{
    row->validPoint = ReadPoint(row->field[2], row->field[3], &row->point);
}

static const CommandSpec *FindCommand(int id)
{
    unsigned index;
    for (index = 0; index < ARRAYSIZE(commandSpecs); ++index)
        if (commandSpecs[index].id == id)
            return &commandSpecs[index];
    return NULL;
}

static void TranslateFolder(WCHAR *destination, LPCWSTR source, int page, BOOL translate)
{
    WCHAR copy[FIELD_LENGTH], *part, *next;
    LPCWSTR partCaption;

    if (!translate) {
        CopyField(destination, source);
        return;
    }
    CopyField(copy, source);
    part = copy;
    destination[0] = L'\0';
    while (part) {
        next = wcschr(part, L'\\');
        if (next)
            *next++ = L'\0';
        partCaption = _wcsicmp(part, rootFolders[page - PAGE_CHEATS]) == 0 ? part : UiTranslate(part);
        if (destination[0])
            StringCchCatW(destination, FIELD_LENGTH, L"\\");
        StringCchCatW(destination, FIELD_LENGTH, partCaption);
        part = next;
    }
}

static BOOL ParseRow(WCHAR *line, int page, Record *row, BOOL translate)
{
    WCHAR *field[12], *coords[4];
    int index, fields = page == PAGE_SHORTCUTS ? 12 : 4;
    if (!SplitFields(line, L'|', field, fields) || !ValidUid(field[0]))
        return FALSE;
    for (index = 1; index < fields; ++index)
        if (wcslen(field[index]) >= FIELD_LENGTH || wcschr(field[index], L'|'))
            return FALSE;
    ZeroMemory(row, sizeof(*row));
    StringCchCopyW(row->uid, ARRAYSIZE(row->uid), field[0]);
    TranslateFolder(row->field[1], field[1], page, translate);
    if (page == PAGE_CHEATS) {
        CopyField(row->field[0], translate ? UiTranslate(field[3]) : field[3]);
        CopyField(row->field[2], field[2]);
    } else {
        CopyField(row->field[0], translate ? UiTranslate(field[2]) : field[2]);
        if (page == PAGE_LOCATIONS) {
            if (!SplitFields(field[3], L';', coords, 4))
                return FALSE;
            for (index = 0; index < 4; ++index)
                CopyField(row->field[index + 2], coords[index]);
            RecordPoint(row);
        } else {
            row->enabled = _wtoi(field[3]) != 0;
            CopyField(row->field[2], field[4]);
            row->modifier = _wtoi(field[5]);
            row->key = _wtoi(field[6]);
            row->category = _wtoi(field[7]);
            CopyField(row->command, field[8]);
            row->parameterPage = _wtoi(field[9]);
            CopyField(row->field[3], field[10]);
            CopyField(row->field[4], field[11]);
        }
    }
    return row->field[0][0] != L'\0';
}

static BOOL LoadRecords(int page, WCHAR *text, BOOL translate)
{
    Record *rows;
    WCHAR *line;
    LPCWSTR heading = dataHeaders[page - PAGE_CHEATS];
    int count = 0;
    BOOL header = FALSE;
    RecordSet *set = RecordsFor(page);
    if (!text)
        return FALSE;
    rows = calloc(MAX_ROWS, sizeof(*rows));
    if (!rows)
        return FALSE;
    while ((line = TakeLine(&text)) != NULL) {
        if (!line[0] || line[0] == L'#' || line[0] == L';')
            continue;
        if (_wcsicmp(line, heading) == 0) {
            header = TRUE;
            continue;
        }
        if (!header)
            continue;
        if (_wcsnicmp(line, L"END_", 4) == 0 && _wcsicmp(line + 4, heading) == 0)
            break;
        if (count == MAX_ROWS || !ParseRow(line, page, &rows[count], translate)) {
            free(rows);
            return FALSE;
        }
        ++count;
    }
    if (!header) {
        free(rows);
        return FALSE;
    }
    free(set->rows);
    set->rows = rows;
    set->count = count;
    return TRUE;
}

static BOOL LoadGarageText(WCHAR *text, Garage *destination)
{
    Garage replacement[GARAGE_COUNT] = { 0 };
    WCHAR *line, *value, *end;
    int index, field;
    BOOL section = FALSE, loaded = FALSE;
    while ((line = TakeLine(&text)) != NULL) {
        if (line[0] == L'[')
            section = _wcsicmp(line, L"[CarTracking]") == 0;
        if (!section || _wcsnicmp(line, L"Garage", 6))
            continue;
        index = (int)wcstol(line + 6, &value, 10);
        if (value == line + 6 || *value++ != L'=' || index < 0 || index >= GARAGE_COUNT * SLOT_COUNT)
            continue;
        for (field = 0; field < GARAGE_VALUE_COUNT && *value; ++field) {
            replacement[index / SLOT_COUNT].slots[index % SLOT_COUNT].field[field] =
                wcstoul(value, &end, field < 8 ? 10 : 16);
            if (*end != L',')
                break;
            value = end + 1;
        }
        if (field < 7)
            return FALSE;
        replacement[index / SLOT_COUNT].locked =
            replacement[index / SLOT_COUNT].slots[index % SLOT_COUNT].field[0] != 0;
        loaded = TRUE;
    }
    if (loaded)
        memcpy(destination, replacement, sizeof(replacement));
    return loaded;
}

static BOOL ChooseConfigFile(UiPage *page, BOOL save, WCHAR path[MAX_PATH])
{
    OPENFILENAMEW settings = { 0 };
    settings.lStructSize = sizeof(settings);
    settings.hwndOwner = GetAncestor(page->window, GA_ROOT);
    settings.lpstrFilter = page->index == PAGE_GARAGES
        ? L"车库配置 (*.ini)\0*.ini\0所有文件 (*.*)\0*.*\0"
        : L"控制中心配置 (*.dat)\0*.dat\0所有文件 (*.*)\0*.*\0";
    settings.lpstrFile = path;
    settings.nMaxFile = MAX_PATH;
    settings.lpstrDefExt = page->index == PAGE_GARAGES ? L"ini" : L"dat";
    settings.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                     (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    path[0] = L'\0';
    return save ? GetSaveFileNameW(&settings) : GetOpenFileNameW(&settings);
}

static BOOL WriteUtf8(HANDLE file, LPCWSTR text)
{
    char line[FIELD_LENGTH * 16];
    int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, line, sizeof(line), NULL, NULL);
    DWORD written;
    return count > 0 && WriteFile(file, line, count - 1, &written, NULL)
        && written == (DWORD)(count - 1);
}

static BOOL ExportRecords(HANDLE file, int page)
{
    const RecordSet *set = RecordsFor(page);
    const Record *row;
    WCHAR line[FIELD_LENGTH * 10];
    int index;
    HRESULT formatted;
    if (FAILED(StringCchPrintfW(line, ARRAYSIZE(line), L"%ls\r\n", dataHeaders[page - PAGE_CHEATS]))
        || !WriteUtf8(file, line))
        return FALSE;
    for (index = 0; index < set->count; ++index) {
        row = &set->rows[index];
        if (page == PAGE_CHEATS)
            formatted = StringCchPrintfW(line, ARRAYSIZE(line), L"%ls|%ls|%ls|%ls\r\n",
                        row->uid, row->field[1], row->field[2], row->field[0]);
        else if (page == PAGE_LOCATIONS)
            formatted = StringCchPrintfW(line, ARRAYSIZE(line), L"%ls|%ls|%ls|%ls;%ls;%ls;%ls\r\n",
                        row->uid, row->field[1], row->field[0], row->field[2], row->field[3],
                        row->field[4], row->field[5]);
        else
            formatted = StringCchPrintfW(line, ARRAYSIZE(line),
                        L"%ls|%ls|%ls|%d|%ls|%d|%d|%d|%ls|%d|%ls|%ls\r\n",
                        row->uid, row->field[1], row->field[0], row->enabled, row->field[2],
                        row->modifier, row->key, row->category, row->command, row->parameterPage,
                        row->field[3], row->field[4]);
        if (FAILED(formatted) || !WriteUtf8(file, line))
            return FALSE;
    }
    if (FAILED(StringCchPrintfW(line, ARRAYSIZE(line), L"END_%ls\r\n",
        dataHeaders[page - PAGE_CHEATS])))
        return FALSE;
    return WriteUtf8(file, line);
}

static BOOL ExportGarage(LPCWSTR path)
{
    WCHAR line[512], number[16], key[24];
    const GarageSlot *slot;
    int garage, bay, field;
    for (garage = 0; garage < GARAGE_COUNT; ++garage) {
        for (bay = 0; bay < SLOT_COUNT; ++bay) {
            slot = &garageData[garage].slots[bay];
            StringCchPrintfW(key, ARRAYSIZE(key), L"Garage%d", garage * SLOT_COUNT + bay);
            StringCchPrintfW(line, ARRAYSIZE(line), L"%d", garageData[garage].locked != 0);
            for (field = 1; field < GARAGE_VALUE_COUNT; ++field) {
                StringCchPrintfW(number, ARRAYSIZE(number), field < 8 ? L",%lu" : L",%lX",
                                 slot->field[field]);
                StringCchCatW(line, ARRAYSIZE(line), number);
            }
            if (!WritePrivateProfileStringW(L"CarTracking", key, line, path))
                return FALSE;
        }
    }
    return TRUE;
}

static BOOL SaveConfiguration(int page, LPCWSTR path)
{
    const BYTE bom[] = { 0xEF, 0xBB, 0xBF };
    WCHAR folder[MAX_PATH], temp[MAX_PATH], *filePart;
    HANDLE file;
    DWORD written;
    BOOL success;
    if (!GetFullPathNameW(path, ARRAYSIZE(folder), folder, &filePart) || !filePart)
        return FALSE;
    *filePart = L'\0';
    if (!GetTempFileNameW(folder, L"GTA", 0, temp))
        return FALSE;
    if (page == PAGE_GARAGES && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
        success = CopyFileW(path, temp, FALSE) && ExportGarage(temp);
        if (success)
            success = MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!success)
            DeleteFileW(temp);
        return success;
    }
    file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DeleteFileW(temp);
        return FALSE;
    }
    success = page == PAGE_GARAGES || (WriteFile(file, bom, sizeof(bom), &written, NULL)
        && written == sizeof(bom) && ExportRecords(file, page) && FlushFileBuffers(file));
    CloseHandle(file);
    if (success && page == PAGE_GARAGES)
        success = ExportGarage(temp);
    if (success)
        success = MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!success)
        DeleteFileW(temp);
    return success;
}

#endif
