#include "trainer.h"

#include <stdio.h>
#include <stdlib.h>
#include <tlhelp32.h>
#include <wchar.h>

#if !defined(_M_IX86)
#error This test must use the x86 Windows ABI.
#endif

enum { EVENT_TIMEOUT = 6000, TARGET_TIMEOUT = 20000, MAX_OBSERVED_EVENTS = 16 };

typedef struct SessionLog {
    GameResult statuses[MAX_OBSERVED_EVENTS];
    int count;
    int notifications;
    BOOL invalid;
} SessionLog;

static const WCHAR observerClass[] = L"GtasaTrainerSessionTest";
static HANDLE targetProcess, targetStop;

static BOOL StopTarget(void)
{
    DWORD exitCode = 1;
    BOOL clean = TRUE;
    if (targetStop)
        SetEvent(targetStop);
    if (targetProcess) {
        clean = WaitForSingleObject(targetProcess, 2500) == WAIT_OBJECT_0 &&
                GetExitCodeProcess(targetProcess, &exitCode) && exitCode == 0;
        if (WaitForSingleObject(targetProcess, 0) == WAIT_TIMEOUT)
            TerminateProcess(targetProcess, 3);
        CloseHandle(targetProcess);
        targetProcess = NULL;
    }
    if (targetStop) {
        CloseHandle(targetStop);
        targetStop = NULL;
    }
    return clean;
}

static void Finish(void)
{
    GameStop();
    StopTarget();
}

static void Require(int condition, const char *expression, int line)
{
    if (!condition) {
        fprintf(stderr, "Trainer session test line %d: %s\n", line, expression);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(expression) Require((expression), #expression, __LINE__)

static BOOL GameNameExists(void)
{
    PROCESSENTRY32W entry = { 0 };
    HANDLE processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    BOOL found;
    CHECK(processes != INVALID_HANDLE_VALUE);
    entry.dwSize = sizeof(entry);
    for (found = Process32FirstW(processes, &entry); found;
         found = Process32NextW(processes, &entry)) {
        if (_wcsicmp(entry.szExeFile, L"gta_sa.exe") == 0 ||
            _wcsicmp(entry.szExeFile, L"gta-sa.exe") == 0 ||
            _wcsicmp(entry.szExeFile, L"gtasa.exe") == 0) {
            CloseHandle(processes);
            return TRUE;
        }
    }
    CloseHandle(processes);
    return FALSE;
}

static LRESULT CALLBACK ObserveSession(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW *create = (const CREATESTRUCTW *)lParam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
    }
    if (message == WM_TRAINER_EVENT) {
        SessionLog *log = (SessionLog *)GetWindowLongPtrW(window, GWLP_USERDATA);
        GameEvent event;
        ++log->notifications;
        while (GameTakeEvent(&event)) {
            if (event.request != GAME_REQ_SESSION || event.value != 0.0 ||
                log->count == MAX_OBSERVED_EVENTS) {
                log->invalid = TRUE;
            } else {
                log->statuses[log->count++] = event.result;
            }
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static HWND NewObserver(SessionLog *log)
{
    HWND window = CreateWindowExW(0, observerClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                  NULL, GetModuleHandleW(NULL), log);
    CHECK(window != NULL);
    return window;
}

static void DrainMessages(void)
{
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
        DispatchMessageW(&message);
}

static void ExpectSession(SessionLog *log, int index, GameResult expected)
{
    DWORD begin = GetTickCount();
    for (;;) {
        DWORD elapsed = GetTickCount() - begin;
        DrainMessages();
        CHECK(!log->invalid);
        CHECK(!GameIsConnected() && !GameIsForeground());
        if (log->count > index)
            break;
        CHECK(elapsed < EVENT_TIMEOUT);
        MsgWaitForMultipleObjects(0, NULL, FALSE, EVENT_TIMEOUT - elapsed, QS_POSTMESSAGE);
    }
    if (log->statuses[index] != expected)
        fprintf(stderr, "Session event %d was %d, expected %d.\n", index,
                (int)log->statuses[index], (int)expected);
    CHECK(log->statuses[index] == expected);
    CHECK(log->notifications > 0);
}

static void StartTarget(const WCHAR *path)
{
    WCHAR eventName[100], command[MAX_PATH + 125];
    STARTUPINFOW start = { 0 };
    PROCESS_INFORMATION process;

    swprintf_s(eventName, ARRAYSIZE(eventName), L"Local\\GtasaTrainerWrongTarget-%lu-%lu",
               GetCurrentProcessId(), GetTickCount());
    SetLastError(ERROR_SUCCESS);
    targetStop = CreateEventW(NULL, TRUE, FALSE, eventName);
    CHECK(targetStop && GetLastError() != ERROR_ALREADY_EXISTS);
    CHECK(swprintf_s(command, ARRAYSIZE(command), L"\"%ls\" --inert %ls", path, eventName) > 0);
    start.cb = sizeof(start);
    CHECK(CreateProcessW(path, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                         &start, &process));
    targetProcess = process.hProcess;
    CloseHandle(process.hThread);
}

static int RunTarget(const WCHAR *eventName)
{
    HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, eventName);
    DWORD result;
    if (!stop)
        return EXIT_FAILURE;
    result = WaitForSingleObject(stop, TARGET_TIMEOUT);
    CloseHandle(stop);
    return result == WAIT_OBJECT_0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void VerifyOfflineRequests(void)
{
    GamePosition coordinates = { 1.0, 2.0, 3.0, 90.0 };
    GameEvent event;
    CHECK(GameReadField(GAME_PLAYER_HEALTH, 0) == GAME_OFFLINE);
    CHECK(GameSetField(GAME_PLAYER_PROTECTION, GAME_PROOF_FIRE, 1) == GAME_OFFLINE);
    CHECK(GameTeleport(&coordinates) == GAME_OFFLINE);
    CHECK(!GameTakeEvent(&event));
}

static void StopSession(HWND window)
{
    GameEvent event;
    GameStop();
    CHECK(!GameTakeEvent(&event));
    CHECK(!GameIsConnected() && !GameIsForeground());
    DrainMessages();
    CHECK(DestroyWindow(window));
}

static void WaitForPendingNotification(HWND window)
{
    MSG message;
    DWORD begin = GetTickCount();
    while (!PeekMessageW(&message, window, WM_TRAINER_EVENT, WM_TRAINER_EVENT, PM_NOREMOVE)) {
        DWORD elapsed = GetTickCount() - begin;
        CHECK(elapsed < EVENT_TIMEOUT);
        MsgWaitForMultipleObjects(0, NULL, FALSE, EVENT_TIMEOUT - elapsed, QS_POSTMESSAGE);
    }
}

static void TestProduction(const WCHAR *targetPath)
{
    SessionLog initial = { 0 }, restarted = { 0 };
    HWND first = NewObserver(&initial);
    HWND second;

    CHECK(GameStart(first));
    CHECK(!GameStart(first));
    ExpectSession(&initial, 0, GAME_OFFLINE);
    VerifyOfflineRequests();
    StartTarget(targetPath);
    ExpectSession(&initial, 1, GAME_LAYOUT_MISMATCH);
    VerifyOfflineRequests();
    CHECK(StopTarget());
    ExpectSession(&initial, 2, GAME_OFFLINE);
    CHECK(initial.count == 3);
    VerifyOfflineRequests();
    StartTarget(targetPath);
    WaitForPendingNotification(first);
    StopSession(first);
    CHECK(initial.count == 3 && !initial.invalid);
    CHECK(StopTarget());

    second = NewObserver(&restarted);
    CHECK(GameStart(second));
    ExpectSession(&restarted, 0, GAME_OFFLINE);
    CHECK(restarted.count == 1);
    VerifyOfflineRequests();
    StopSession(second);
    GameStop();
    puts("PASS: live message-only window received offline, fingerprint mismatch, and offline again.");
    puts("PASS: game requests remained offline, and adapter stop/restart produced a fresh session.");
}

static void TestMissingBackend(void)
{
    SessionLog missing = { 0 };
    HWND window = NewObserver(&missing);
    CHECK(GameStart(window));
    ExpectSession(&missing, 0, GAME_BACKEND_MISSING);
    CHECK(missing.count == 1);
    VerifyOfflineRequests();
    StopSession(window);
    puts("PASS: absent app-local backend reported through WM_TRAINER_EVENT.");
}

int wmain(int argc, WCHAR **argv)
{
    WNDCLASSW windowClass = { 0 };
    if (argc == 3 && wcscmp(argv[1], L"--inert") == 0)
        return RunTarget(argv[2]);
    if (argc != 2) {
        fputs("Usage: trainer_session_test.exe <absolute inert gta_sa.exe path | --without-backend>\n",
              stderr);
        return EXIT_FAILURE;
    }
    if (GameNameExists()) {
        fputs("Close other GTA SA processes before running the adapter session test.\n", stderr);
        return EXIT_FAILURE;
    }
    CHECK(atexit(Finish) == 0);
    windowClass.lpfnWndProc = ObserveSession;
    windowClass.hInstance = GetModuleHandleW(NULL);
    windowClass.lpszClassName = observerClass;
    CHECK(RegisterClassW(&windowClass));
    if (wcscmp(argv[1], L"--without-backend") == 0)
        TestMissingBackend();
    else
        TestProduction(argv[1]);
    return EXIT_SUCCESS;
}
