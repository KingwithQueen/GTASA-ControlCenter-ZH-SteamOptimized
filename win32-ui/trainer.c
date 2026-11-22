#include "trainer.h"

#include <tlhelp32.h>
#include <math.h>
#include <float.h>
#include <process.h>
#include <strsafe.h>
#include <wchar.h>

enum { QUEUE_SIZE = 256, EVENT_SIZE = 512, CHEAT_LIMIT = 120, LOCK_SIZE = 96 };

typedef struct Request {
    GameEvent event;
    LONG generation;
    WCHAR text[CHEAT_LIMIT + 1];
} Request;

typedef struct FieldLock {
    GameField field;
    int index;
    double value;
} FieldLock;

typedef struct Backend {
    DWORD (WINAPI *attach)(HANDLE);
    BOOL (WINAPI *close)(HANDLE);
    DWORD (WINAPI *field)(int,int,double*,BOOL);
    DWORD (WINAPI *position)(GamePosition*,BOOL);
    DWORD (WINAPI *feature)(DWORD,DWORD);
    DWORD (WINAPI *coffee)(DWORD);
    DWORD (WINAPI *feedback)(DWORD);
    DWORD (WINAPI *feedbackMessage)(LPCWSTR,DWORD);
    DWORD (WINAPI *girlfriend)(DWORD,DWORD,DWORD);
    DWORD (WINAPI *plate)(LPCWSTR,DWORD);
    DWORD (WINAPI *plateStatus)(void);
    DWORD (WINAPI *weapon)(int,DWORD,DWORD,DWORD);
    DWORD (WINAPI *garageRead)(DWORD,DWORD*,BYTE*);
    DWORD (WINAPI *garageWrite)(DWORD,const DWORD*,const BYTE*,const BYTE*,DWORD);
    DWORD (WINAPI *garageLock)(DWORD,DWORD,const BYTE*);
} Backend;

typedef struct Runtime {
    HWND window;
    HANDLE wake, thread, process;
    CRITICAL_SECTION guard;
    volatile LONG pid, generation, stopping;
    GameResult status;
    Backend backend;
    Request requests[QUEUE_SIZE];
    GameEvent events[EVENT_SIZE];
    int reqHead, reqCount, eventHead, eventCount;
    FieldLock locks[LOCK_SIZE];
    int lockCount;
    BOOL feedbackEnabled;
    BOOL weatherLockOwned;
    DWORD rejectedPid;
    FILETIME rejectedCreation;
    Request cheat;
    DWORD cheatBegan, cheatLast;
    int cheatIndex;
    BOOL cheatPending, platePending;
    GameEvent plateEvent;
    DWORD plateBegan, plateChecked;
    WCHAR plateLock[9];
} Runtime;

static Runtime runtime;

static LONG AtomicRead(volatile LONG *storage)
{
    return InterlockedCompareExchange(storage, 0, 0);
}

BOOL GameIsConnected(void)
{
    return AtomicRead(&runtime.pid) != 0;
}

BOOL GameIsForeground(void)
{
    DWORD pid = 0, target = (DWORD)AtomicRead(&runtime.pid);
    HWND foreground = GetForegroundWindow();
    if (foreground && target)
        GetWindowThreadProcessId(foreground, &pid);
    return pid != 0 && pid == target;
}

LPCWSTR GameResultText(GameResult result)
{
    switch (result) {
    case GAME_OK: return L"已完成后端操作";
    case GAME_QUEUED: return L"已提交，等待游戏后端处理";
    case GAME_OFFLINE: return L"等待经典 Steam 版 GTA SA";
    case GAME_UNSUPPORTED: return L"此操作缺少已验证的 Steam 实现";
    case GAME_BUSY: return L"游戏操作繁忙，请稍候重试";
    case GAME_INVALID: return L"参数或当前游戏对象不符合操作条件";
    case GAME_NOT_READY: return L"游戏当前状态不允许操作，请确认已载入存档，必要时按 Esc 暂停";
    case GAME_LAYOUT_MISMATCH: return L"游戏版本或运行布局与已验证的经典 Steam 版不符";
    case GAME_BACKEND_MISSING: return L"找不到兼容的 GTASASteam.dll，请把测试包完整解压";
    default: return L"后端操作失败或结果未确认，请先在游戏中核对";
    }
}

static GameResult NativeResult(DWORD value)
{
    switch (value) {
    case 0: return GAME_OK;
    case 1: return GAME_OFFLINE;
    case 2: return GAME_NOT_READY;
    case 3: return GAME_INVALID;
    case 7: case 8: return GAME_BUSY;
    case 9: case 10: return GAME_UNSUPPORTED;
    default: return GAME_FAILED;
    }
}

static HMODULE LoadBackend(Backend *api)
{
    WCHAR path[MAX_PATH], *filename;
    DWORD chars = GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
    HMODULE module;
    FARPROC address;
    if (!chars || chars >= ARRAYSIZE(path))
        return NULL;
    filename = wcsrchr(path, L'\\');
    if (!filename || FAILED(StringCchCopyW(filename+1, ARRAYSIZE(path)-(filename+1-path),
                                             L"GTASASteam.dll")))
        return NULL;
    module = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module)
        return NULL;

#define LOAD(member, name) \
    address = GetProcAddress(module, name); \
    if (!address) goto missing; \
    CopyMemory(&api->member, &address, sizeof address)
    LOAD(attach,"SteamAttach");
    LOAD(close,"TrainerCloseHandle");
    LOAD(field,"SteamUiField");
    LOAD(position,"SteamUiPosition");
    LOAD(feature,"SteamFeatureSet");
    LOAD(coffee,"SteamCoffeeSet");
    LOAD(feedback,"SteamFeedbackEnable");
    LOAD(feedbackMessage,"SteamFeedbackMessage");
    LOAD(girlfriend,"SteamGirlfriendSet");
    LOAD(plate,"SteamUiPlate");
    LOAD(plateStatus,"SteamPlateStatus");
    LOAD(weapon,"SteamUiWeapon");
    LOAD(garageRead,"SteamGarageInspect");
    LOAD(garageWrite,"SteamGarageWrite");
    LOAD(garageLock,"SteamGarageLock");
#undef LOAD
    return module;
missing:
    FreeLibrary(module);
    ZeroMemory(api,sizeof(*api));
    return NULL;
}

static void PublishEvent(const GameEvent *event)
{
    BOOL signal;
    EnterCriticalSection(&runtime.guard);
    signal = runtime.eventCount == 0;
    if (runtime.eventCount < EVENT_SIZE) {
        runtime.events[(runtime.eventHead + runtime.eventCount) % EVENT_SIZE] = *event;
        ++runtime.eventCount;
    }
    LeaveCriticalSection(&runtime.guard);
    if (signal)
        PostMessageW(runtime.window,WM_TRAINER_EVENT,0,0);
}

BOOL GameTakeEvent(GameEvent *event)
{
    BOOL available;
    if (!runtime.thread || !event)
        return FALSE;
    EnterCriticalSection(&runtime.guard);
    available = runtime.eventCount != 0;
    if (available) {
        *event = runtime.events[runtime.eventHead];
        runtime.eventHead = (runtime.eventHead + 1) % EVENT_SIZE;
        --runtime.eventCount;
    }
    LeaveCriticalSection(&runtime.guard);
    return available;
}

static void PublishSession(GameResult status, DWORD pid)
{
    GameEvent event = { 0 };
    if (runtime.status == status && (DWORD)AtomicRead(&runtime.pid) == pid)
        return;
    runtime.status = status;
    InterlockedIncrement(&runtime.generation);
    InterlockedExchange(&runtime.pid,(LONG)pid);
    event.request = GAME_REQ_SESSION;
    event.result = status;
    event.value = pid != 0;
    PublishEvent(&event);
}

static BOOL GameExeName(LPCWSTR name)
{
    return _wcsicmp(name,L"gta-sa.exe") == 0 || _wcsicmp(name,L"gta_sa.exe") == 0
        || _wcsicmp(name,L"gtasa.exe") == 0;
}

static void FindGame(void)
{
    PROCESSENTRY32W entry = { 0 };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    GameResult status = GAME_OFFLINE;
    BOOL found;
    if (snapshot == INVALID_HANDLE_VALUE)
        return;
    entry.dwSize = sizeof entry;
    for (found = Process32FirstW(snapshot,&entry); found;
         found = Process32NextW(snapshot,&entry)) {
        HANDLE candidate;
        FILETIME created, exited, kernel, user;
        if (!GameExeName(entry.szExeFile) || entry.th32ProcessID == GetCurrentProcessId())
            continue;
        candidate = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE
                                | PROCESS_VM_OPERATION | SYNCHRONIZE, FALSE, entry.th32ProcessID);
        if (!candidate) {
            status = GAME_NOT_READY;
            continue;
        }
        if (!GetProcessTimes(candidate,&created,&exited,&kernel,&user)) {
            CloseHandle(candidate);
            continue;
        }
        if (entry.th32ProcessID == runtime.rejectedPid
            && CompareFileTime(&created,&runtime.rejectedCreation) == 0) {
            CloseHandle(candidate);
            status = GAME_LAYOUT_MISMATCH;
            continue;
        }
        if (runtime.backend.attach(candidate)) {
            double off = 0;
            runtime.process = candidate;
            runtime.backend.field(GAME_FIELD_NONE,1,&off,TRUE);
            PublishSession(GAME_OK,entry.th32ProcessID);
            CloseHandle(snapshot);
            return;
        }
        runtime.backend.close(candidate);
        runtime.rejectedPid = entry.th32ProcessID;
        runtime.rejectedCreation = created;
        status = GAME_LAYOUT_MISMATCH;
    }
    CloseHandle(snapshot);
    PublishSession(status,0);
}

static void Disconnect(void)
{
    runtime.lockCount = 0;
    runtime.feedbackEnabled = FALSE;
    runtime.plateLock[0] = L'\0';
    if (runtime.cheatPending) {
        runtime.cheat.event.result = GAME_OFFLINE;
        PublishEvent(&runtime.cheat.event);
        runtime.cheatPending = FALSE;
    }
    if (runtime.platePending) {
        runtime.plateEvent.result = GAME_OFFLINE;
        PublishEvent(&runtime.plateEvent);
        runtime.platePending = FALSE;
    }
    if (runtime.process) {
        if (runtime.weatherLockOwned && WaitForSingleObject(runtime.process,0) == WAIT_TIMEOUT) {
            double released = -1.0;
            runtime.backend.field(GAME_WEATHER,1,&released,TRUE);
        }
        runtime.backend.close(runtime.process);
        runtime.process = NULL;
    }
    runtime.weatherLockOwned = FALSE;
    PublishSession(GAME_OFFLINE,0);
}

static unsigned StoredWord(const BYTE *p)
{
    WORD value;
    CopyMemory(&value,p,sizeof value);
    return value;
}

static void PutWord(BYTE *p, unsigned value)
{
    WORD word = (WORD)value;
    CopyMemory(p,&word,sizeof word);
}

static void DecodeGarage(GameGarageSlot slots[4], const BYTE cars[256])
{
    int bay, i;
    for (bay = 0; bay < 4; ++bay) {
        const BYTE *car = cars + bay * 64;
        GameGarageSlot *slot = slots + bay;
        slot->model = StoredWord(car+18);
        slot->protection = StoredWord(car+16);
        slot->primary = car[50];
        slot->secondary = car[51];
        CopyMemory(&slot->angle,car+60,sizeof slot->angle);
        for (i = 0; i < 15; ++i)
            slot->mods[i] = StoredWord(car+20+i*2);
        for (i = 0; i < 8; ++i)
            slot->tuning[i] = car[52+i];
    }
}

static BOOL EncodeGarage(BYTE cars[256], const GameGarageSlot slots[4])
{
    int bay, i;
    for (bay = 0; bay < 4; ++bay) {
        BYTE *car = cars + bay * 64;
        const GameGarageSlot *slot = slots + bay;
        if ((slot->model && (slot->model < 400 || slot->model > 611))
            || slot->primary > 255 || slot->secondary > 255 || slot->protection > 65535)
            return FALSE;
        PutWord(car+18,slot->model);
        PutWord(car+16,slot->protection);
        car[50] = (BYTE)slot->primary;
        car[51] = (BYTE)slot->secondary;
        CopyMemory(car+60,&slot->angle,sizeof slot->angle);
        for (i = 0; i < 15; ++i) {
            if (slot->mods[i] > 65535)
                return FALSE;
            PutWord(car+20+i*2,slot->mods[i]);
        }
        for (i = 0; i < 8; ++i) {
            if (slot->tuning[i] > 255)
                return FALSE;
            car[52+i] = (BYTE)slot->tuning[i];
        }
    }
    return TRUE;
}

static DWORD ProcessGarage(GameEvent *event)
{
    DWORD meta[24], status;
    BYTE before[256], selected[256];
    if (event->request == GAME_REQ_GARAGE_LOCK && !event->mode)
        return runtime.backend.garageLock(event->index,0,NULL);
    status = runtime.backend.garageRead(event->index,meta,before);
    if (status)
        return status;
    if (event->request == GAME_REQ_GARAGE_READ) {
        DecodeGarage(event->slots,before);
        return 0;
    }
    CopyMemory(selected,before,sizeof selected);
    if (!EncodeGarage(selected,event->slots))
        return 3;
    if (event->request == GAME_REQ_GARAGE_LOCK)
        return runtime.backend.garageLock(event->index,1,selected);
    return runtime.backend.garageWrite(event->index,meta,before,selected,1);
}

static DWORD SetLock(GameField field, int index, double value, BOOL enabled)
{
    int slot;
    DWORD status;
    for (slot = 0; slot < runtime.lockCount; ++slot)
        if (runtime.locks[slot].field == field && runtime.locks[slot].index == index)
            break;
    if (!enabled) {
        if (slot < runtime.lockCount)
            runtime.locks[slot] = runtime.locks[--runtime.lockCount];
        if (field == GAME_WEATHER) {
            double release = -1.0;
            status = runtime.backend.field(field,1,&release,TRUE);
            if (!status)
                runtime.weatherLockOwned = FALSE;
            return status;
        }
        if (field == GAME_PLAYER_FLIGHT_ASSIST || field == GAME_VEHICLE_FLIGHT_ASSIST) {
            double off = 0;
            return runtime.backend.field(field,1,&off,TRUE);
        }
        return 0;
    }
    if (slot == LOCK_SIZE)
        return 8;
    if (field == GAME_WEATHER)
        status = runtime.backend.field(field,1,&value,TRUE);
    else if (field == GAME_PLAYER_FLIGHT_ASSIST || field == GAME_VEHICLE_FLIGHT_ASSIST)
        status = runtime.backend.field(field,0,&value,TRUE);
    else
        status = runtime.backend.field(field,index,&value,TRUE);
    if (status)
        return status;
    if (field == GAME_WEATHER)
        runtime.weatherLockOwned = TRUE;
    if (field == GAME_PLAYER_FLIGHT_ASSIST || field == GAME_VEHICLE_FLIGHT_ASSIST) {
        double on = 1.0;
        return runtime.backend.field(field,1,&on,TRUE);
    }
    runtime.locks[slot].field = field;
    runtime.locks[slot].index = index;
    runtime.locks[slot].value = value;
    if (slot == runtime.lockCount)
        ++runtime.lockCount;
    return 0;
}

static void TickLocks(void)
{
    int i;
    for (i = 0; i < runtime.lockCount; ++i) {
        const FieldLock *lock = runtime.locks + i;
        double current = 0, wanted = lock->value;
        if (!runtime.backend.field(lock->field,lock->index,&current,FALSE)
            && fabs(current-wanted) > 0.01)
            runtime.backend.field(lock->field,lock->index,&wanted,TRUE);
    }
    {
        double ignored = 0;
        runtime.backend.field(GAME_FIELD_NONE,0,&ignored,TRUE);
    }
}

static void TickPending(DWORD now)
{
    if (runtime.platePending) {
        DWORD status = runtime.backend.plateStatus();
        if (status != 8 || now - runtime.plateBegan >= 15000) {
            runtime.plateEvent.result = status == 8 ? GAME_NOT_READY : NativeResult(status);
            PublishEvent(&runtime.plateEvent);
            runtime.platePending = FALSE;
        }
    }
    if (!runtime.platePending && runtime.plateLock[0] && now - runtime.plateChecked >= 500) {
        runtime.plateChecked = now;
        if (runtime.backend.plateStatus() != 8)
            runtime.backend.plate(runtime.plateLock,(DWORD)wcslen(runtime.plateLock));
    }
    if (runtime.cheatPending) {
        GameResult result = GAME_QUEUED;
        if (now - runtime.cheatBegan >= 15000 || (runtime.cheatIndex && !GameIsForeground())) {
            result = GAME_NOT_READY;
        } else if (now - runtime.cheatLast >= 35 && GameIsForeground()) {
            INPUT input[2] = { 0 };
            WORD key = (WORD)runtime.cheat.text[runtime.cheatIndex];
            input[0].type = input[1].type = INPUT_KEYBOARD;
            input[0].ki.wVk = input[1].ki.wVk = key;
            input[1].ki.dwFlags = KEYEVENTF_KEYUP;
            if (!GameIsForeground() || SendInput(2,input,sizeof input[0]) != 2)
                result = GAME_FAILED;
            else if (!runtime.cheat.text[++runtime.cheatIndex])
                result = GAME_OK;
            runtime.cheatLast = now;
        }
        if (result != GAME_QUEUED) {
            runtime.cheat.event.result = result;
            PublishEvent(&runtime.cheat.event);
            runtime.cheatPending = FALSE;
        }
    }
}

#include "trainer_commands.h"

static void RunRequest(Request *request)
{
    GameEvent *event = &request->event;
    DWORD status = 10;
    if (!GameIsConnected() || request->generation != AtomicRead(&runtime.generation)) {
        event->result = GAME_OFFLINE;
        PublishEvent(event);
        return;
    }
    switch (event->request) {
    case GAME_REQ_READ_FIELD: case GAME_REQ_SET_FIELD:
        status = runtime.backend.field(event->field,event->index,&event->value,
                                       event->request == GAME_REQ_SET_FIELD);
        break;
    case GAME_REQ_LOCK_FIELD:
        status = SetLock(event->field,event->index,event->value,event->mode != 0);
        break;
    case GAME_REQ_FEATURE: status = runtime.backend.feature(event->index,event->value != 0); break;
    case GAME_REQ_COFFEE: status = runtime.backend.coffee(event->value == 0); break;
    case GAME_REQ_FEEDBACK:
        status = runtime.backend.feedback(event->value != 0);
        if (!status)
            runtime.feedbackEnabled = event->value != 0;
        break;
    case GAME_REQ_GIRLFRIEND:
        status = runtime.backend.girlfriend(event->index,(DWORD)event->value,event->mode);
        break;
    case GAME_REQ_PLATE:
        if (event->mode == 2) {
            runtime.plateLock[0] = L'\0';
            status = 0;
            break;
        }
        status = runtime.platePending ? 8 : runtime.backend.plate(request->text,(DWORD)wcslen(request->text));
        if (!status && (event->mode == 1 || runtime.plateLock[0]))
            StringCchCopyW(runtime.plateLock,ARRAYSIZE(runtime.plateLock),request->text);
        if (!status && runtime.backend.plateStatus() == 8) {
            runtime.plateEvent = *event;
            runtime.plateBegan = GetTickCount();
            runtime.platePending = TRUE;
            return;
        }
        if (!status)
            status = runtime.backend.plateStatus();
        break;
    case GAME_REQ_POSITION: case GAME_REQ_TELEPORT:
        status = runtime.backend.position(&event->position,event->request == GAME_REQ_TELEPORT);
        break;
    case GAME_REQ_GARAGE_READ: case GAME_REQ_GARAGE_WRITE: case GAME_REQ_GARAGE_LOCK:
        status = ProcessGarage(event);
        break;
    case GAME_REQ_CHEAT:
        if (!runtime.cheatPending) {
            runtime.cheat = *request;
            runtime.cheatBegan = runtime.cheatLast = GetTickCount();
            runtime.cheatIndex = 0;
            runtime.cheatPending = TRUE;
            return;
        }
        status = 8;
        break;
    case GAME_REQ_COMMAND:
        status = RunConsoleCommand(event->index,event->mode,request->text);
        break;
    case GAME_REQ_WEAPON:
        status = runtime.backend.weapon(event->index,event->weapon.type,event->weapon.total,event->weapon.loaded);
        if (!status) {
            int index;
            for (index = 0; index < runtime.lockCount; ++index) {
                FieldLock *lock = &runtime.locks[index];
                if (lock->index != event->index)
                    continue;
                switch (lock->field) {
                case GAME_WEAPON_TYPE: lock->value = event->weapon.type; break;
                case GAME_WEAPON_TOTAL: lock->value = event->weapon.total; break;
                case GAME_WEAPON_LOADED: lock->value = event->weapon.loaded; break;
                default: break;
                }
            }
        }
        break;
    default: break;
    }
    event->result = NativeResult(status);
    if (!status && runtime.feedbackEnabled
        && (event->request == GAME_REQ_COMMAND || event->request == GAME_REQ_TELEPORT)) {
        LPCWSTR message = event->request == GAME_REQ_TELEPORT
            ? L"Control Center: location updated" : L"Control Center: shortcut applied";
        runtime.backend.feedbackMessage(message,(DWORD)wcslen(message));
    }
    PublishEvent(event);
}

static BOOL TakeRequest(Request *request)
{
    BOOL have;
    EnterCriticalSection(&runtime.guard);
    have = runtime.reqCount != 0;
    if (have) {
        *request = runtime.requests[runtime.reqHead];
        runtime.reqHead = (runtime.reqHead + 1) % QUEUE_SIZE;
        --runtime.reqCount;
    }
    LeaveCriticalSection(&runtime.guard);
    return have;
}

static unsigned __stdcall TrainerThread(void *unused)
{
    HMODULE module = LoadBackend(&runtime.backend);
    DWORD lastScan = GetTickCount() - 1500, lastLock = lastScan;
    Request request;
    UNREFERENCED_PARAMETER(unused);
    if (!module) {
        PublishSession(GAME_BACKEND_MISSING,0);
        return 0;
    }
    PublishSession(GAME_OFFLINE,0);
    while (!AtomicRead(&runtime.stopping)) {
        DWORD now = GetTickCount();
        if (runtime.process && WaitForSingleObject(runtime.process,0) == WAIT_OBJECT_0)
            Disconnect();
        if (!runtime.process && now - lastScan >= 1500) {
            lastScan = now;
            FindGame();
        }
        if (runtime.process) {
            if (now - lastLock >= 120) {
                lastLock = now;
                TickLocks();
            }
            TickPending(now);
        }
        if (TakeRequest(&request))
            RunRequest(&request);
        else
            WaitForSingleObject(runtime.wake,25);
    }
    Disconnect();
    FreeLibrary(module);
    return 0;
}

BOOL GameStart(HWND window)
{
    if (runtime.thread)
        return FALSE;
    ZeroMemory(&runtime,sizeof runtime);
    runtime.status = GAME_BUSY;
    runtime.window = window;
    InitializeCriticalSection(&runtime.guard);
    runtime.wake = CreateEventW(NULL,FALSE,FALSE,NULL);
    if (runtime.wake)
        runtime.thread = (HANDLE)_beginthreadex(NULL,0,TrainerThread,NULL,0,NULL);
    if (runtime.thread)
        return TRUE;
    if (runtime.wake)
        CloseHandle(runtime.wake);
    DeleteCriticalSection(&runtime.guard);
    return FALSE;
}

void GameStop(void)
{
    if (!runtime.thread)
        return;
    InterlockedExchange(&runtime.stopping,1);
    SetEvent(runtime.wake);
    WaitForSingleObject(runtime.thread,INFINITE);
    CloseHandle(runtime.thread);
    CloseHandle(runtime.wake);
    runtime.thread = NULL;
    DeleteCriticalSection(&runtime.guard);
}

static GameResult Submit(Request *request)
{
    GameResult result = GAME_OFFLINE;
    if (!runtime.thread || !GameIsConnected())
        return result;
    EnterCriticalSection(&runtime.guard);
    if (runtime.reqCount < QUEUE_SIZE && runtime.reqCount + runtime.eventCount < EVENT_SIZE - 4) {
        request->generation = AtomicRead(&runtime.generation);
        runtime.requests[(runtime.reqHead + runtime.reqCount) % QUEUE_SIZE] = *request;
        ++runtime.reqCount;
        result = GAME_QUEUED;
    } else {
        result = GAME_BUSY;
    }
    LeaveCriticalSection(&runtime.guard);
    if (result == GAME_QUEUED)
        SetEvent(runtime.wake);
    return result;
}

static GameResult Scalar(GameRequestKind kind,GameField field,int index,double value,int mode)
{
    Request request = { 0 };
    request.event.request = kind;
    request.event.field = field;
    request.event.index = index;
    request.event.value = value;
    request.event.mode = mode;
    return Submit(&request);
}

BOOL GameSupportsField(GameField field, int index, BOOL write)
{
    switch (field) {
    case GAME_PLAYER_HEALTH: case GAME_PLAYER_ARMOR: case GAME_PLAYER_MONEY:
    case GAME_PLAYER_FAT: case GAME_PLAYER_STAMINA: case GAME_PLAYER_MUSCLE:
    case GAME_PLAYER_LUNG: case GAME_PLAYER_GAMBLING: case GAME_PLAYER_DRIVING:
    case GAME_PLAYER_BIKING: case GAME_PLAYER_CYCLING: case GAME_PLAYER_FLYING:
    case GAME_PLAYER_HEADING: case GAME_WEAPON_SLOT: case GAME_VEHICLE_ENGINE:
    case GAME_VEHICLE_WEIGHT: case GAME_VEHICLE_PRIMARY: case GAME_VEHICLE_SECONDARY:
    case GAME_VEHICLE_DOORS: case GAME_VEHICLE_HEADING: case GAME_VEHICLE_RC:
    case GAME_VEHICLE_SPAWNER: case GAME_CLOCK_MINUTES: case GAME_CLOCK_DAY:
    case GAME_CLOCK_SPEED: case GAME_WORLD_SPEED:
        return index == 0;
    case GAME_PLAYER_PROTECTION: case GAME_VEHICLE_PROTECTION: return index >= 0 && index < 4;
    case GAME_PLAYER_CHEAT: return index >= 0 && index < 22;
    case GAME_PLAYER_SPEED: case GAME_VEHICLE_SPEED: case GAME_VEHICLE_SPIN:
        return index >= 0 && index <= (write ? 3 : 2);
    case GAME_PLAYER_KICK: case GAME_VEHICLE_KICK: return write && index >= 0 && index < 360;
    case GAME_WEAPON_TYPE: return index >= -1 && index < 11;
    case GAME_WEAPON_TOTAL: case GAME_WEAPON_LOADED: return index >= 0 && index < 11;
    case GAME_WEAPON_SKILL: return index >= 0 && index < 10;
    case GAME_PLAYER_CHEATED: case GAME_WEATHER: return index >= 0 && index <= 1;
    case GAME_PLAYER_FLIGHT_ASSIST: case GAME_VEHICLE_FLIGHT_ASSIST:
    case GAME_VEHICLE_AUTO_ALARM: return index >= 0 && index <= 1;
    case GAME_VEHICLE_WHEELS: case GAME_VEHICLE_AUTO_RESTART:
    case GAME_VEHICLE_DONT_BURN: case GAME_VEHICLE_DONT_EXPLODE: return index == 0;
    case GAME_PLAYER_COFFEE: return !write && index == 0;
    case GAME_VEHICLE_MODEL: return !write && index == 0;
    case GAME_VEHICLE_FLIP: case GAME_VEHICLE_SPAWN: return write && index == 0;
    case GAME_RELATIONSHIP: return index >= 0 && index < 6;
    default: return FALSE;
    }
}

GameResult GameReadField(GameField field,int index)
{
    return GameSupportsField(field,index,FALSE)
        ? Scalar(GAME_REQ_READ_FIELD,field,index,0,0) : GAME_UNSUPPORTED;
}
GameResult GameSetField(GameField field,int index,double value)
{
    return GameSupportsField(field,index,TRUE)
        ? Scalar(GAME_REQ_SET_FIELD,field,index,value,0) : GAME_UNSUPPORTED;
}
GameResult GameLockField(GameField field,int index,double value,BOOL enabled)
{
    return GameSupportsField(field,index,TRUE) && GameSupportsField(field,index,FALSE)
        ? Scalar(GAME_REQ_LOCK_FIELD,field,index,value,enabled) : GAME_UNSUPPORTED;
}
GameResult GameSetFeature(int feature,BOOL enabled)
{
    if (feature < 0 || feature > 1)
        return GAME_INVALID;
    return Scalar(GAME_REQ_FEATURE,GAME_PLAYER_CHEAT,feature,enabled != FALSE,0);
}
GameResult GameSetCoffee(BOOL enabled)
{
    return Scalar(GAME_REQ_COFFEE,GAME_FIELD_NONE,0,enabled != FALSE,0);
}
GameResult GameSetFeedback(BOOL enabled)
{
    return Scalar(GAME_REQ_FEEDBACK,GAME_FIELD_NONE,0,enabled != FALSE,0);
}
GameResult GameSetGirlfriend(int index,int progress,int mode)
{
    if (index < 0 || index > 5 || progress < 0 || progress > 100 || mode < 0 || mode > 2)
        return GAME_INVALID;
    return Scalar(GAME_REQ_GIRLFRIEND,GAME_RELATIONSHIP,index,progress,mode);
}

static GameResult QueuePlate(LPCWSTR plate,int mode)
{
    Request request = { 0 };
    if (mode != 2 && (!plate || !*plate || wcslen(plate) > 8))
        return GAME_INVALID;
    request.event.request = GAME_REQ_PLATE;
    request.event.mode = mode;
    if (mode != 2)
        StringCchCopyW(request.text,ARRAYSIZE(request.text),plate);
    return Submit(&request);
}
GameResult GameSetPlate(LPCWSTR plate)
{
    return QueuePlate(plate,0);
}
GameResult GameLockPlate(LPCWSTR plate,BOOL enabled)
{
    return QueuePlate(plate,enabled ? 1 : 2);
}
GameResult GameSetWeapon(int index,const GameWeapon *weapon)
{
    Request request = { 0 };
    if (!weapon || index < -1 || index > 10 || weapon->type > 46
        || weapon->total > 99999 || weapon->loaded > weapon->total)
        return GAME_INVALID;
    request.event.request = GAME_REQ_WEAPON;
    request.event.index = index;
    request.event.weapon = *weapon;
    return Submit(&request);
}
GameResult GameReadPosition(void)
{
    return Scalar(GAME_REQ_POSITION,GAME_FIELD_NONE,0,0,0);
}
GameResult GameTeleport(const GamePosition *position)
{
    Request request = { 0 };
    if (!position)
        return GAME_INVALID;
    request.event.request = GAME_REQ_TELEPORT;
    request.event.position = *position;
    return Submit(&request);
}
static GameResult QueueGarage(GameRequestKind kind,int index,BOOL enabled,const GameGarageSlot slots[4])
{
    Request request = { 0 };
    if (index < 0 || index > 16 || (enabled && !slots))
        return GAME_INVALID;
    request.event.request = kind;
    request.event.index = index;
    request.event.mode = enabled;
    if (slots)
        CopyMemory(request.event.slots,slots,sizeof request.event.slots);
    return Submit(&request);
}
GameResult GameGarageRead(int index)
{
    return QueueGarage(GAME_REQ_GARAGE_READ,index,FALSE,NULL);
}
GameResult GameGarageWrite(int index,const GameGarageSlot slots[4])
{
    return QueueGarage(GAME_REQ_GARAGE_WRITE,index,TRUE,slots);
}
GameResult GameGarageLock(int index,BOOL enabled,const GameGarageSlot slots[4])
{
    return QueueGarage(GAME_REQ_GARAGE_LOCK,index,enabled,slots);
}
GameResult GameQueueCheat(LPCWSTR text)
{
    Request request = { 0 };
    int count = 0;
    if (!text)
        return GAME_INVALID;
    for (; *text; ++text) {
        WCHAR ch = *text;
        if (ch == L' ' || ch == L'\t')
            continue;
        if (ch >= L'a' && ch <= L'z')
            ch -= L'a' - L'A';
        if (ch < L'A' || ch > L'Z' || count >= CHEAT_LIMIT)
            return GAME_INVALID;
        request.text[count++] = ch;
    }
    if (!count)
        return GAME_INVALID;
    request.event.request = GAME_REQ_CHEAT;
    return Submit(&request);
}
GameResult GameRunCommand(int consoleId,int parameterPage,LPCWSTR raw)
{
    Request request = { 0 };
    if (consoleId < 0 || consoleId > 129 ||
        (raw && FAILED(StringCchCopyW(request.text,ARRAYSIZE(request.text),raw))))
        return GAME_INVALID;
    request.event.request = GAME_REQ_COMMAND;
    request.event.index = consoleId;
    request.event.mode = parameterPage;
    return Submit(&request);
}
