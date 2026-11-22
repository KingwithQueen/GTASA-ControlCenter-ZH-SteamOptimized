#ifndef GTASA_UI_H
#define GTASA_UI_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "resources.h"
#include "trainer.h"

enum { UI_PAGE_COUNT = 6, UI_PAGE_WIDTH = 920, UI_PAGE_HEIGHT = 530 };

typedef struct UiPage {
    HWND window;
    int index;
    int dpi;
    HFONT font;
    BOOL failed;
} UiPage;

int UiScale(const UiPage *page, int value);
HWND UiControl(UiPage *page, LPCWSTR className, LPCWSTR text,
               DWORD style, DWORD exStyle, int id,
               int x, int y, int width, int height);
void UiSetStatus(UiPage *page, LPCWSTR text);
void UiRefreshGame(UiPage *page);
const char *UiResourceBytes(int id, DWORD *size);
LPCWSTR UiTranslate(LPCWSTR text);
void UiFreeTranslations(void);

void BuildDensePage(UiPage *page);
BOOL DensePageCommand(UiPage *page, int id, int code, HWND control);
void DensePageScroll(HWND control);
void DensePageGameEvent(UiPage *page, const GameEvent *event);
void DensePageGameRefresh(UiPage *page);
void DensePageGamePoll(UiPage *page);

void BuildListPage(UiPage *page);
BOOL ListPageCommand(UiPage *page, int id, int code, HWND control);
LRESULT ListPageNotify(UiPage *page, NMHDR *notification);
void ListPageGameEvent(UiPage *page, const GameEvent *event);
void ListPageGameTick(UiPage *page);

#endif
