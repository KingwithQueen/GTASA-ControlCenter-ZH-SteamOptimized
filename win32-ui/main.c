#include "ui.h"

#include "trainer.h"

#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

enum {
    ID_MAIN_TAB = 100,
    ID_STATUS = 101,
    ID_PAGE_FIRST = 200,
    ID_NEXT_TAB = 300,
    ID_PREVIOUS_TAB = 301,
    ID_GAME_TIMER = 302,
    WM_BENCHMARK_STEP = WM_APP + 1,
    BENCHMARK_SWITCHES = 600,
    BENCHMARK_TOTAL_STEPS = BENCHMARK_SWITCHES + UI_PAGE_COUNT - 1
};

static const WCHAR mainClass[] = L"GTASAControlCenterPrototype";
static const WCHAR pageClass[] = L"GTASAControlCenterPage";
static const WCHAR title[] = L"圣安地列斯控制中心";
static const WCHAR offlineStatus[] = L"等待游戏 · 经典 Steam 版 GTA SA";
static LPCWSTR const pageNames[UI_PAGE_COUNT] = {
    L"载具与游戏", L"玩家状态", L"车库", L"游戏秘籍", L"传送地点", L"快捷键"
};

typedef struct App {
    HWND window;
    HWND tabs;
    HWND status;
    UiPage pages[UI_PAGE_COUNT];
    HFONT font;
    int dpi;
    int currentPage;
    SIZE minimumWindow;
    int exitCode;
    DWORD lastGameRefresh;

    FILE *benchmark;
    LARGE_INTEGER frequency;
    int benchmarkStep;
    int pageChildren[UI_PAGE_COUNT];
} App;

int UiScale(const UiPage *page, int value)
{
    return MulDiv(value, page->dpi, 96);
}

HWND UiControl(UiPage *page, LPCWSTR className, LPCWSTR text, DWORD style,
               DWORD exStyle, int id, int x, int y, int width, int height)
{
    HWND control = CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
                                   UiScale(page, x), UiScale(page, y),
                                   UiScale(page, width), UiScale(page, height),
                                   page->window, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (control)
        SendMessageW(control, WM_SETFONT, (WPARAM)page->font, FALSE);
    else
        page->failed = TRUE;
    return control;
}

void UiSetStatus(UiPage *page, LPCWSTR text)
{
    HWND status = GetDlgItem(GetAncestor(page->window, GA_ROOT), ID_STATUS);
    SendMessageW(status, SB_SETTEXTW, 0, (LPARAM)text);
}

static LRESULT CALLBACK PageProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    UiPage *page;

    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lParam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
        return DefWindowProcW(window, message, wParam, lParam);
    }
    page = (UiPage *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (page) {
        switch (message) {
        case WM_COMMAND:
            if (page->index < 2) {
                if (DensePageCommand(page, LOWORD(wParam), HIWORD(wParam), (HWND)lParam))
                    return 0;
            } else if (ListPageCommand(page, LOWORD(wParam), HIWORD(wParam), (HWND)lParam)) {
                return 0;
            }
            break;
        case WM_NOTIFY:
            if (page->index >= 2)
                return ListPageNotify(page, (NMHDR *)lParam);
            break;
        case WM_HSCROLL:
            if (page->index < 2 && lParam)
                DensePageScroll((HWND)lParam);
            break;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            SetTextColor((HDC)wParam, GetSysColor(COLOR_WINDOWTEXT));
            SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void LayoutPages(App *app)
{
    RECT client, tab, status;
    int index, x, y, margin, width, height;

    if (!app->tabs || !app->status || IsIconic(app->window))
        return;
    GetClientRect(app->window, &client);
    SendMessageW(app->status, WM_SIZE, 0, 0);
    GetClientRect(app->status, &status);

    margin = UiScale(&app->pages[0], 12);
    SetRect(&tab, 0, 0, client.right - 2 * margin, client.bottom - status.bottom - 2 * margin);
    SetWindowPos(app->tabs, NULL, margin, margin, tab.right, tab.bottom,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    TabCtrl_AdjustRect(app->tabs, FALSE, &tab);
    width = UiScale(&app->pages[0], UI_PAGE_WIDTH);
    height = UiScale(&app->pages[0], UI_PAGE_HEIGHT);
    x = margin + tab.left + (tab.right - tab.left - width) / 2;
    y = margin + tab.top + (tab.bottom - tab.top - height) / 2;
    for (index = 0; index < UI_PAGE_COUNT; ++index)
        SetWindowPos(app->pages[index].window, NULL, x, y, width, height,
                     SWP_NOACTIVATE | SWP_NOZORDER);
}

static void SelectPage(App *app, int index, BOOL selectTab)
{
    HWND previous, next, focus;
    HDWP positions;
    UINT flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;

    if (index < 0 || index >= UI_PAGE_COUNT || index == app->currentPage)
        return;
    previous = app->pages[app->currentPage].window;
    next = app->pages[index].window;
    focus = GetFocus();
    if (focus && IsChild(previous, focus))
        SetFocus(app->tabs);
    if (selectTab)
        TabCtrl_SetCurSel(app->tabs, index);

    app->currentPage = index;
    positions = BeginDeferWindowPos(2);
    if (positions)
        positions = DeferWindowPos(positions, previous, NULL, 0, 0, 0, 0, flags | SWP_HIDEWINDOW);
    if (positions)
        positions = DeferWindowPos(positions, next, NULL, 0, 0, 0, 0, flags | SWP_SHOWWINDOW);
    if (!positions || !EndDeferWindowPos(positions)) {
        ShowWindow(previous, SW_HIDE);
        ShowWindow(next, SW_SHOWNA);
    }
}

static BOOL BuildWindow(App *app, HINSTANCE instance)
{
    int index;
    TCITEMW item = { 0 };
    DWORD tabStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | TCS_FIXEDWIDTH;

    app->tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", tabStyle, 0, 0, 0, 0,
                                app->window, (HMENU)ID_MAIN_TAB, instance, NULL);
    app->status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                  0, 0, 0, 0, app->window, (HMENU)ID_STATUS, instance, NULL);
    if (!app->tabs || !app->status)
        return FALSE;
    SendMessageW(app->tabs, WM_SETFONT, (WPARAM)app->font, FALSE);
    SendMessageW(app->status, WM_SETFONT, (WPARAM)app->font, FALSE);
    SendMessageW(app->status, SB_SETTEXTW, 0, (LPARAM)offlineStatus);

    for (index = 0; index < UI_PAGE_COUNT; ++index) {
        UiPage *page = &app->pages[index];
        page->index = index;
        page->dpi = app->dpi;
        page->font = app->font;
        item.mask = TCIF_TEXT;
        item.pszText = (LPWSTR)pageNames[index];
        if (TabCtrl_InsertItem(app->tabs, index, &item) == -1)
            return FALSE;
        /* Let the page background paint under its transparent group boxes. */
        page->window = CreateWindowExW(WS_EX_CONTROLPARENT, pageClass, pageNames[index],
                                       WS_CHILD | WS_CLIPSIBLINGS,
                                       0, 0, 0, 0, app->window, (HMENU)(INT_PTR)(ID_PAGE_FIRST + index),
                                       instance, page);
        if (!page->window)
            return FALSE;
        /* The page is a sibling of the tab, above its background. */
        SetWindowPos(page->window, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (index < 2)
            BuildDensePage(page);
        else
            BuildListPage(page);
        if (page->failed)
            return FALSE;
    }

    TabCtrl_SetItemSize(app->tabs, UiScale(&app->pages[0], 136), UiScale(&app->pages[0], 31));
    TabCtrl_SetCurSel(app->tabs, 0);
    LayoutPages(app);
    ShowWindow(app->pages[0].window, SW_SHOWNA);
    return TRUE;
}

static BOOL CALLBACK CountChild(HWND child, LPARAM count)
{
    UNREFERENCED_PARAMETER(child);
    ++*(int *)count;
    return TRUE;
}

static double MillisecondsSince(const App *app, LARGE_INTEGER start)
{
    LARGE_INTEGER end;

    QueryPerformanceCounter(&end);
    return (end.QuadPart - start.QuadPart) * 1000.0 / app->frequency.QuadPart;
}

static double FinishPaint(App *app, LARGE_INTEGER start)
{
    RedrawWindow(app->pages[app->currentPage].window, NULL, NULL, RDW_UPDATENOW | RDW_ALLCHILDREN);
    UpdateWindow(app->tabs);
    UpdateWindow(app->status);
    UpdateWindow(app->window);
    GdiFlush();
    return MillisecondsSince(app, start);
}

static BOOL RecordBenchmark(App *app, const char *phase, int index, double milliseconds,
                            double selectionMilliseconds)
{
    return fprintf(app->benchmark, "%s\t%d\t%.6f\t%d\t%.6f\n", phase, index, milliseconds,
                   app->pageChildren[index], selectionMilliseconds) > 0;
}

static void RunBenchmarkStep(App *app)
{
    const char *phase;
    LARGE_INTEGER start;
    double selectionMilliseconds;
    int index;

    if (app->benchmarkStep == BENCHMARK_TOTAL_STEPS) {
        if (fflush(app->benchmark) != 0)
            app->exitCode = 3;
        DestroyWindow(app->window);
        return;
    }
    if (app->benchmarkStep < UI_PAGE_COUNT - 1) {
        phase = "first";
        index = app->benchmarkStep + 1;
    } else {
        phase = "warm";
        index = (app->benchmarkStep - (UI_PAGE_COUNT - 1)) % UI_PAGE_COUNT;
    }
    QueryPerformanceCounter(&start);
    SelectPage(app, index, TRUE);
    selectionMilliseconds = MillisecondsSince(app, start);
    ++app->benchmarkStep;
    if (!RecordBenchmark(app, phase, index, FinishPaint(app, start), selectionMilliseconds)
        || !PostMessageW(app->window, WM_BENCHMARK_STEP, 0, 0)) {
        app->exitCode = 3;
        DestroyWindow(app->window);
    }
}

static LRESULT CALLBACK MainProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    App *app;

    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lParam;
        app = (App *)create->lpCreateParams;
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)app);
        return DefWindowProcW(window, message, wParam, lParam);
    }
    app = (App *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (!app)
        return DefWindowProcW(window, message, wParam, lParam);

    switch (message) {
    case WM_CREATE:
        return BuildWindow(app, ((CREATESTRUCTW *)lParam)->hInstance) ? 0 : -1;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lParam;
        limits->ptMinTrackSize.x = app->minimumWindow.cx;
        limits->ptMinTrackSize.y = app->minimumWindow.cy;
        return 0;
    }
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            LayoutPages(app);
        return 0;
    case WM_SETFOCUS:
        SetFocus(app->tabs);
        return 0;
    case WM_ACTIVATEAPP:
        if (wParam && GameIsConnected()) {
            DWORD now = GetTickCount();
            if (now - app->lastGameRefresh > 1000) {
                app->lastGameRefresh = now;
                DensePageGameRefresh(&app->pages[0]);
                DensePageGameRefresh(&app->pages[1]);
            }
        }
        break;
    case WM_NOTIFY:
    {
        NMHDR *notification = (NMHDR *)lParam;
        if (notification->hwndFrom == app->tabs && notification->code == TCN_SELCHANGE)
            SelectPage(app, TabCtrl_GetCurSel(app->tabs), FALSE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_NEXT_TAB)
            SelectPage(app, (app->currentPage + 1) % UI_PAGE_COUNT, TRUE);
        else if (LOWORD(wParam) == ID_PREVIOUS_TAB)
            SelectPage(app, (app->currentPage + UI_PAGE_COUNT - 1) % UI_PAGE_COUNT, TRUE);
        return 0;
    case WM_BENCHMARK_STEP:
        if (app->benchmark)
            RunBenchmarkStep(app);
        return 0;
    case WM_TRAINER_EVENT:
    {
        GameEvent event;
        int index;
        while (GameTakeEvent(&event)) {
            if (event.request == GAME_REQ_SESSION) {
                LPCWSTR text = event.result == GAME_OK
                    ? L"已连接经典 Steam 版 GTA SA · 请在游戏中检查实际效果"
                    : GameResultText(event.result);
                SendMessageW(app->status,SB_SETTEXTW,0,(LPARAM)text);
                if (event.result == GAME_OK)
                    SetTimer(window,ID_GAME_TIMER,25,NULL);
                else
                    KillTimer(window,ID_GAME_TIMER);
            }
            for (index = 0; index < UI_PAGE_COUNT; ++index) {
                if (index < 2)
                    DensePageGameEvent(&app->pages[index],&event);
                else
                    ListPageGameEvent(&app->pages[index],&event);
            }
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == ID_GAME_TIMER) {
            ListPageGameTick(&app->pages[5]);
            return 0;
        }
        break;
    case WM_DESTROY:
        KillTimer(window,ID_GAME_TIMER);
        GameStop();
        if (app->benchmark && app->benchmarkStep < BENCHMARK_TOTAL_STEPS)
            app->exitCode = 3;
        PostQuitMessage(app->exitCode);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static BOOL RegisterWindowClasses(HINSTANCE instance)
{
    WNDCLASSW wc = { 0 };

    wc.lpfnWndProc = MainProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = mainClass;
    if (!RegisterClassW(&wc))
        return FALSE;
    wc.lpfnWndProc = PageProc;
    wc.hIcon = NULL;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = pageClass;
    return RegisterClassW(&wc) != 0;
}

static BOOL OpenBenchmark(App *app)
{
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    BOOL valid;

    if (!argv)
        return FALSE;
    if (argc == 1) {
        LocalFree(argv);
        return TRUE;
    }
    valid = argc == 4 && wcscmp(argv[1], L"--benchmark") == 0
         && wcscmp(argv[2], L"--benchmark-file") == 0
         && QueryPerformanceFrequency(&app->frequency)
         && _wfopen_s(&app->benchmark, argv[3], L"wt") == 0;
    LocalFree(argv);
    return valid;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR commandLine, int show)
{
    static ACCEL shortcuts[] = {
        { FVIRTKEY | FCONTROL, VK_TAB, ID_NEXT_TAB },
        { FVIRTKEY | FCONTROL | FSHIFT, VK_TAB, ID_PREVIOUS_TAB }
    };
    const DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    INITCOMMONCONTROLSEX common;
    LARGE_INTEGER start;
    App app = { 0 };
    MSG message;
    RECT frame;
    HACCEL accelerators;
    HDC screen;
    int index, result;

    UNREFERENCED_PARAMETER(previous);
    UNREFERENCED_PARAMETER(commandLine);
    QueryPerformanceCounter(&start);
    if (!OpenBenchmark(&app))
        return 2;

    screen = GetDC(NULL);
    app.dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : 96;
    if (screen)
        ReleaseDC(NULL, screen);
    app.font = CreateFontW(-MulDiv(9, app.dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           DEFAULT_QUALITY, DEFAULT_PITCH, L"Tahoma");
    common.dwSize = sizeof(common);
    common.dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES
                 | ICC_TREEVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES;
    SetRect(&frame, 0, 0, MulDiv(UI_PAGE_WIDTH + 38, app.dpi, 96),
             MulDiv(UI_PAGE_HEIGHT + 90, app.dpi, 96));
    AdjustWindowRectEx(&frame, style, FALSE, 0);
    app.minimumWindow.cx = frame.right - frame.left;
    app.minimumWindow.cy = frame.bottom - frame.top;

    if (app.font && InitCommonControlsEx(&common) && RegisterWindowClasses(instance))
        app.window = CreateWindowExW(0, mainClass, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                                     app.minimumWindow.cx, app.minimumWindow.cy,
                                     NULL, NULL, instance, &app);
    if (!app.window) {
        if (app.benchmark)
            fclose(app.benchmark);
        else
            MessageBoxW(NULL, L"无法创建控制中心界面。", title, MB_ICONERROR);
        if (app.font)
            DeleteObject(app.font);
        UiFreeTranslations();
        return 1;
    }

    accelerators = CreateAcceleratorTableW(shortcuts, ARRAYSIZE(shortcuts));
    ShowWindow(app.window, app.benchmark ? SW_SHOWNORMAL : show);
    if (!app.benchmark && !GameStart(app.window))
        SendMessageW(app.status,SB_SETTEXTW,0,(LPARAM)GameResultText(GAME_BACKEND_MISSING));
    if (app.benchmark) {
        double startupMilliseconds = FinishPaint(&app, start);
        for (index = 0; index < UI_PAGE_COUNT; ++index)
            EnumChildWindows(app.pages[index].window, CountChild, (LPARAM)&app.pageChildren[index]);
        if (fprintf(app.benchmark, "phase\tpage\tms\tchild_windows\tselection_ms\n") < 0
            || !RecordBenchmark(&app, "startup", 0, startupMilliseconds, 0.0)
            || !PostMessageW(app.window, WM_BENCHMARK_STEP, 0, 0)) {
            app.exitCode = 3;
            DestroyWindow(app.window);
        }
    }
    while ((result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        if ((!accelerators || !TranslateAcceleratorW(app.window, accelerators, &message))
            && !IsDialogMessageW(app.window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (accelerators)
        DestroyAcceleratorTable(accelerators);
    DeleteObject(app.font);
    UiFreeTranslations();
    if (app.benchmark && fclose(app.benchmark) != 0)
        return 3;
    return result < 0 ? 3 : (int)message.wParam;
}
