#include "ui.h"

#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

typedef struct Translation {
    LPCWSTR source;
    LPCWSTR translated;
    size_t line;
} Translation;

static WCHAR *dictionary;
static Translation *translations;
static size_t translationCount;
static BOOL translationsLoaded;

const char *UiResourceBytes(int id, DWORD *size)
{
    HMODULE module = GetModuleHandleW(NULL);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
    HGLOBAL data = resource ? LoadResource(module, resource) : NULL;

    *size = data ? SizeofResource(module, resource) : 0;
    return data ? (const char *)LockResource(data) : NULL;
}

static int CompareTranslations(const void *left, const void *right)
{
    const Translation *a = (const Translation *)left;
    const Translation *b = (const Translation *)right;
    int order = wcscmp(a->source, b->source);

    if (order || a->line == b->line)
        return order;
    return a->line > b->line ? -1 : 1;
}

static int CompareCaption(const void *key, const void *entry)
{
    return wcscmp((LPCWSTR)key, ((const Translation *)entry)->source);
}

static void LoadTranslations(void)
{
    DWORD size;
    const char *bytes = UiResourceBytes(ID_DATA_TRANSLATIONS, &size);
    WCHAR *line, *cursor;
    size_t capacity = 1, index, distinct;
    int chars;

    translationsLoaded = TRUE;
    if (!bytes || !size || size > INT_MAX)
        return;
    chars = MultiByteToWideChar(CP_UTF8, 0, bytes, (int)size, NULL, 0);
    if (!chars)
        return;
    dictionary = calloc((size_t)chars + 1, sizeof(*dictionary));
    if (!dictionary || !MultiByteToWideChar(CP_UTF8, 0, bytes, (int)size, dictionary, chars))
        return;
    line = dictionary + (dictionary[0] == 0xfeff);
    for (cursor = line; *cursor; ++cursor)
        capacity += (*cursor == L'\n');
    translations = calloc(capacity, sizeof(*translations));
    if (!translations)
        return;

    for (index = 0; line && *line; ++index) {
        WCHAR *next = wcschr(line, L'\n');
        WCHAR *end = next ? next : line + wcslen(line);
        WCHAR *separator;

        if (next)
            *next++ = L'\0';
        if (end > line && end[-1] == L'\r')
            end[-1] = L'\0';
        separator = wcschr(line, L'\t');
        if (separator && separator != line && !wcschr(separator + 1, L'\t')) {
            *separator = L'\0';
            translations[translationCount].source = line;
            translations[translationCount].translated = separator + 1;
            translations[translationCount++].line = index;
        }
        line = next;
    }

    qsort(translations, translationCount, sizeof(*translations), CompareTranslations);
    for (index = 0, distinct = 0; index < translationCount; ++index) {
        if (!distinct || wcscmp(translations[distinct - 1].source, translations[index].source))
            translations[distinct++] = translations[index];
    }
    translationCount = distinct;
}

LPCWSTR UiTranslate(LPCWSTR text)
{
    const Translation *entry;

    if (!text || !*text)
        return text;
    if (!translationsLoaded)
        LoadTranslations();
    if (!translationCount)
        return text;
    entry = bsearch(text, translations, translationCount, sizeof(*translations), CompareCaption);
    return entry ? entry->translated : text;
}

void UiFreeTranslations(void)
{
    free(translations);
    free(dictionary);
}
