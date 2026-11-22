#define WIN32_LEAN_AND_MEAN
#include "trainer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_IX86)
#error This test must use the x86 Windows ABI.
#endif

enum {
    GARAGES = 17,
    GARAGE_METADATA_WORDS = 24,
    GARAGE_SLOT_COUNT = 4,
    GARAGE_SLOT_BYTES = 64,
    GARAGE_BYTES = GARAGE_SLOT_COUNT * GARAGE_SLOT_BYTES,
    GARAGE_MODEL_ID = 18,
    GARAGE_PRIMARY_COLOR = 50,
    FEATURE_MISSION_TIMER = 1,
    SUCCESS = 0,
    DISCONNECTED = 1,
    NOT_READY = 2,
    INVALID = 3,
    STALE = 7,
    PENDING = 8,
    UNSUPPORTED = 10,
    COFFEE_UNAVAILABLE = 255
};

typedef DWORD (WINAPI *QueryFn)(void);
typedef DWORD (WINAPI *ValueFn)(DWORD);
typedef DWORD (WINAPI *AttachFn)(HANDLE);
typedef BOOL (WINAPI *CloseFn)(HANDLE);
typedef DWORD (WINAPI *FeatureSetFn)(DWORD, DWORD);
typedef DWORD (WINAPI *GirlfriendSetFn)(DWORD, DWORD, DWORD);
typedef DWORD (WINAPI *FeedbackSendFn)(const WCHAR *, DWORD);
typedef DWORD (WINAPI *GarageInspectFn)(DWORD, DWORD *, BYTE *);
typedef DWORD (WINAPI *GarageWriteFn)(DWORD, const DWORD *, const BYTE *, const BYTE *, DWORD);
typedef DWORD (WINAPI *GarageLockFn)(DWORD, DWORD, const BYTE *);
typedef DWORD (WINAPI *UiFieldFn)(int, int, double *, BOOL);
typedef DWORD (WINAPI *UiPositionFn)(GamePosition *, BOOL);
typedef DWORD (WINAPI *UiPlateFn)(const WCHAR *, DWORD);
typedef DWORD (WINAPI *UiWeaponFn)(int, DWORD, DWORD, DWORD);
typedef DWORD (WINAPI *FixtureStatsIntactFn)(DWORD, DWORD);

typedef struct SteamAbi {
    AttachFn attach;
    QueryFn status;
    CloseFn closeHandle;
    QueryFn coffeeGet;
    ValueFn coffeeSet;
    GirlfriendSetFn girlfriendSet;
    ValueFn featureGet;
    FeatureSetFn featureSet;
    ValueFn feedbackEnable;
    FeedbackSendFn feedbackSend;
    GarageInspectFn garageInspect;
    GarageWriteFn garageWrite;
    GarageLockFn garageLock;
    UiFieldFn field;
    UiPositionFn position;
    UiPlateFn plateSet;
    UiWeaponFn weapon;
    QueryFn plateStatus;
} SteamAbi;

typedef struct FixtureAbi {
    QueryFn begin;
    QueryFn run;
    ValueFn vehicle;
    ValueFn coffeeSupported;
    QueryFn disturbGarage;
    FixtureStatsIntactFn playerStatsIntact;
} FixtureAbi;

static void Require(int condition, const char *expression, int line)
{
    if (!condition) {
        fprintf(stderr, "Steam ABI test line %d: %s\n", line, expression);
        exit(EXIT_FAILURE);
    }
}

#define CHECK(expression) Require((expression), #expression, __LINE__)

static void Import(HMODULE library, const char *symbol, void *function, size_t functionSize)
{
    FARPROC address = GetProcAddress(library, symbol);
    if (!address || functionSize != sizeof(address)) {
        fprintf(stderr, "Steam ABI export missing or incompatible: %s\n", symbol);
        exit(EXIT_FAILURE);
    }
    memcpy(function, &address, sizeof(address));
}

#define IMPORT(library, interface, member, symbol) \
    Import((library), (symbol), &(interface)->member, sizeof((interface)->member))

static void ImportSteam(HMODULE library, SteamAbi *api)
{
    IMPORT(library, api, attach, "SteamAttach");
    IMPORT(library, api, status, "SteamStatus");
    IMPORT(library, api, closeHandle, "TrainerCloseHandle");
    IMPORT(library, api, coffeeGet, "SteamCoffeeGet");
    IMPORT(library, api, coffeeSet, "SteamCoffeeSet");
    IMPORT(library, api, girlfriendSet, "SteamGirlfriendSet");
    IMPORT(library, api, featureGet, "SteamFeatureGet");
    IMPORT(library, api, featureSet, "SteamFeatureSet");
    IMPORT(library, api, feedbackEnable, "SteamFeedbackEnable");
    IMPORT(library, api, feedbackSend, "SteamFeedbackMessage");
    IMPORT(library, api, garageInspect, "SteamGarageInspect");
    IMPORT(library, api, garageWrite, "SteamGarageWrite");
    IMPORT(library, api, garageLock, "SteamGarageLock");
    IMPORT(library, api, field, "SteamUiField");
    IMPORT(library, api, position, "SteamUiPosition");
    IMPORT(library, api, plateSet, "SteamUiPlate");
    IMPORT(library, api, weapon, "SteamUiWeapon");
    IMPORT(library, api, plateStatus, "SteamPlateStatus");
}

static HMODULE OpenLibrary(const WCHAR *path)
{
    HMODULE library = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!library) {
        fwprintf(stderr, L"Error %lu loading %ls\n", GetLastError(), path);
        exit(EXIT_FAILURE);
    }
    return library;
}

static void TestDisconnected(const SteamAbi *api)
{
    DWORD metadata[GARAGE_METADATA_WORDS] = { 0 };
    BYTE parked[GARAGE_BYTES] = { 0 };
    double value = 100.0;
    GamePosition position = { 0 };
    CHECK(api->coffeeGet() == COFFEE_UNAVAILABLE);
    CHECK(api->coffeeSet(0) == DISCONNECTED);
    CHECK(api->girlfriendSet(0, 75, 2) == DISCONNECTED);
    CHECK(api->featureGet(FEATURE_MISSION_TIMER) == 0);
    CHECK(api->featureSet(FEATURE_MISSION_TIMER, 1) == DISCONNECTED);
    CHECK(api->feedbackEnable(1) == DISCONNECTED);
    CHECK(api->feedbackSend(L"offline", 7) == DISCONNECTED);
    CHECK(api->garageInspect(0, metadata, parked) == DISCONNECTED);
    CHECK(api->garageWrite(0, metadata, parked, parked, 0) == DISCONNECTED);
    CHECK(api->garageLock(0, 0, NULL) == DISCONNECTED);
    CHECK(api->field(GAME_PLAYER_HEALTH, 0, &value, FALSE) == DISCONNECTED);
    CHECK(api->field(GAME_PLAYER_HEALTH, 0, &value, TRUE) == DISCONNECTED);
    CHECK(api->position(&position, FALSE) == DISCONNECTED);
    CHECK(api->position(&position, TRUE) == DISCONNECTED);
    CHECK(api->plateSet(L"UI TEST", 7) == DISCONNECTED);
    CHECK(api->weapon(1, 0, 0, 0) == DISCONNECTED);
    CHECK(api->plateStatus() == DISCONNECTED);
}

static void TestProduction(const WCHAR *path)
{
    static const char * const privateExports[] = {
        "SteamFixtureBegin", "SteamFixtureRun", "SteamFixtureVehicle", "SteamFixtureCoffeeSupported",
        "SteamFixtureTrain", "SteamFixtureDisturbGarage", "TrainerReadMemory", "TrainerWriteMemory",
        "SteamPlateSet", "_SteamFixturePlayerStatsIntact@8"
    };
    HMODULE library = OpenLibrary(path);
    HANDLE currentProcess = GetCurrentProcess(), duplicate = NULL;
    SteamAbi api;
    size_t index;

    ImportSteam(library, &api);
    for (index = 0; index < ARRAYSIZE(privateExports); ++index)
        CHECK(GetProcAddress(library, privateExports[index]) == NULL);
    CHECK(api.status() == SUCCESS);
    CHECK(DuplicateHandle(currentProcess, currentProcess, currentProcess, &duplicate,
                          0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(api.attach(duplicate) == 0);
    CHECK(api.closeHandle(duplicate));
    TestDisconnected(&api);
    CHECK(FreeLibrary(library));
    puts("PASS: typed release exports, incompatible-process refusal, offline results, private exports absent.");
}

static void TestGarages(const SteamAbi *api, const FixtureAbi *fixture)
{
    DWORD index, metadata[GARAGE_METADATA_WORDS], currentMetadata[GARAGE_METADATA_WORDS];
    BYTE before[GARAGE_BYTES], selected[GARAGE_BYTES], current[GARAGE_BYTES];

    for (index = 0; index < GARAGES; ++index) {
        CHECK(api->garageInspect(index, metadata, before) == SUCCESS);
        CHECK(metadata[0] == index);
    }
    CHECK(api->garageInspect(GARAGES, metadata, before) == INVALID);
    CHECK(api->garageInspect(0, NULL, before) == INVALID);
    CHECK(api->garageInspect(0, metadata, before) == SUCCESS);
    memcpy(selected, before, sizeof(selected));
    selected[GARAGE_PRIMARY_COLOR] = 127;
    CHECK(api->garageWrite(0, metadata, before, selected, 0) == INVALID);
    selected[GARAGE_PRIMARY_COLOR] = 126;
    CHECK(api->garageWrite(0, metadata, before, selected, 0) == SUCCESS);
    CHECK(api->garageWrite(0, metadata, before, selected, 0) == STALE);
    CHECK(api->garageInspect(0, currentMetadata, current) == SUCCESS);
    CHECK(memcmp(current, selected, sizeof(current)) == 0);

    CHECK(api->garageLock(0, 1, current) == SUCCESS);
    CHECK(fixture->disturbGarage() == 1);
    CHECK(fixture->run() == 1);
    CHECK(api->garageInspect(0, currentMetadata, current) == SUCCESS);
    /* Lock recalculates the parked pose for each occupied slot. */
    for (index = 0; index < GARAGE_BYTES; index += GARAGE_SLOT_BYTES) {
        CHECK(memcmp(current + index + GARAGE_MODEL_ID,
                     selected + index + GARAGE_MODEL_ID, sizeof(WORD)) == 0);
        CHECK(current[index + GARAGE_PRIMARY_COLOR] == selected[index + GARAGE_PRIMARY_COLOR]);
    }
    CHECK(api->garageLock(0, 0, NULL) == SUCCESS);
    CHECK(fixture->disturbGarage() == 1);
    CHECK(fixture->run() == 1);
    CHECK(api->garageInspect(0, currentMetadata, current) == SUCCESS);
    ZeroMemory(before, sizeof(before));
    CHECK(memcmp(current, before, sizeof(current)) == 0);
    puts("PASS: 17 typed garage snapshots, color validation/write, stale rejection, lock and unlock.");
}

static void ExpectField(const SteamAbi *api, GameField field, int index, double expected)
{
    double actual = -1.0;
    CHECK(api->field(field, index, &actual, FALSE) == SUCCESS);
    CHECK(fabs(actual - expected) < 0.001);
}

static void SetField(const SteamAbi *api, GameField field, int index, double value)
{
    CHECK(api->field(field, index, &value, TRUE) == SUCCESS);
}

static void ExpectPosition(const SteamAbi *api, const GamePosition *expected)
{
    GamePosition actual = { 0 };
    CHECK(api->position(&actual, FALSE) == SUCCESS);
    CHECK(fabs(actual.x - expected->x) < 0.001);
    CHECK(fabs(actual.y - expected->y) < 0.001);
    CHECK(fabs(actual.z - expected->z) < 0.001);
    CHECK(fabs(actual.heading - expected->heading) < 0.001);
}

static void TestPlayer(const SteamAbi *api)
{
    GamePosition original = { 0 };
    GamePosition target = { 2402.25, -1727.5, 12.875, 90.0 };
    GamePosition invalidPosition = target;
    double outOfRange = 1001.0, fractionalMoney = 10.5, noVehicle = 0.0;

    ExpectField(api, GAME_PLAYER_HEALTH, 0, 100.0);
    ExpectField(api, GAME_PLAYER_ARMOR, 0, 0.0);
    ExpectField(api, GAME_PLAYER_MONEY, 0, 0.0);
    CHECK(api->field(GAME_VEHICLE_PRIMARY, 0, &noVehicle, FALSE) == NOT_READY);
    CHECK(api->field(GAME_FIELD_COUNT, 0, &noVehicle, FALSE) == UNSUPPORTED);
    CHECK(api->field(GAME_PLAYER_HEALTH, 0, NULL, FALSE) == INVALID);
    CHECK(api->field(GAME_PLAYER_HEALTH, 0, &outOfRange, TRUE) == INVALID);
    CHECK(api->field(GAME_PLAYER_MONEY, 0, &fractionalMoney, TRUE) == INVALID);
    SetField(api, GAME_PLAYER_HEALTH, 0, 275.5);
    SetField(api, GAME_PLAYER_ARMOR, 0, 87.25);
    SetField(api, GAME_PLAYER_MONEY, 0, 12345.0);
    ExpectField(api, GAME_PLAYER_HEALTH, 0, 275.5);
    ExpectField(api, GAME_PLAYER_ARMOR, 0, 87.25);
    ExpectField(api, GAME_PLAYER_MONEY, 0, 12345.0);

    SetField(api, GAME_PLAYER_PROTECTION, GAME_PROOF_BULLET, 1.0);
    SetField(api, GAME_PLAYER_PROTECTION, GAME_PROOF_FIRE, 1.0);
    ExpectField(api, GAME_PLAYER_PROTECTION, GAME_PROOF_FIRE, 1.0);
    SetField(api, GAME_PLAYER_PROTECTION, GAME_PROOF_FIRE, 0.0);
    ExpectField(api, GAME_PLAYER_PROTECTION, GAME_PROOF_FIRE, 0.0);
    ExpectField(api, GAME_PLAYER_PROTECTION, GAME_PROOF_BULLET, 1.0);

    ExpectPosition(api, &original);
    CHECK(api->position(&target, TRUE) == SUCCESS);
    ExpectPosition(api, &target);
    invalidPosition.x = 20001.0;
    CHECK(api->position(&invalidPosition, TRUE) == INVALID);
    ExpectPosition(api, &target);
    CHECK(api->position(&original, TRUE) == SUCCESS);
    puts("PASS: typed health, armor, money, independent player proofs, and position/heading round trip.");
}

static void RejectField(const SteamAbi *api, GameField field, double value)
{
    CHECK(api->field(field, 0, &value, TRUE) == INVALID);
}

static void TestNumericPlayerStats(const SteamAbi *api, const FixtureAbi *fixture)
{
    int level;
    double blockedWanted = 3.0;
    CHECK(fixture->playerStatsIntact(TRUE, 0));
    ExpectField(api, GAME_PLAYER_RESPECT, 0, 124.5);
    ExpectField(api, GAME_PLAYER_APPEAL, 0, 75.25);
    ExpectField(api, GAME_CRIMINALS, 0, 17.0);
    ExpectField(api, GAME_PLAYER_WANTED, 0, 0.0);
    CHECK(fixture->playerStatsIntact(TRUE, 0));

    RejectField(api, GAME_PLAYER_RESPECT, 1000.25);
    RejectField(api, GAME_PLAYER_RESPECT, -0.25);
    RejectField(api, GAME_PLAYER_APPEAL, 1000.25);
    RejectField(api, GAME_PLAYER_APPEAL, -0.25);
    RejectField(api, GAME_CRIMINALS, -1.0);
    RejectField(api, GAME_CRIMINALS, 17.5);
    RejectField(api, GAME_CRIMINALS, 2147483648.0);
    RejectField(api, GAME_PLAYER_WANTED, -1.0);
    RejectField(api, GAME_PLAYER_WANTED, 7.0);
    RejectField(api, GAME_PLAYER_WANTED, 2.5);
    SetField(api, GAME_PLAYER_CHEAT, 0, 1.0);
    CHECK(api->field(GAME_PLAYER_WANTED, 0, &blockedWanted, TRUE) == NOT_READY);
    SetField(api, GAME_PLAYER_CHEAT, 0, 0.0);
    CHECK(fixture->playerStatsIntact(TRUE, 0));

    SetField(api, GAME_PLAYER_RESPECT, 0, 487.25);
    SetField(api, GAME_PLAYER_APPEAL, 0, 812.5);
    SetField(api, GAME_CRIMINALS, 0, 2147483647.0);
    for (level = 1; level <= 6; ++level) {
        SetField(api, GAME_PLAYER_WANTED, 0, level);
        ExpectField(api, GAME_PLAYER_WANTED, 0, level);
        CHECK(fixture->playerStatsIntact(FALSE, (DWORD)level));
    }
    SetField(api, GAME_PLAYER_WANTED, 0, 0.0);
    ExpectField(api, GAME_PLAYER_WANTED, 0, 0.0);
    ExpectField(api, GAME_PLAYER_RESPECT, 0, 487.25);
    ExpectField(api, GAME_PLAYER_APPEAL, 0, 812.5);
    ExpectField(api, GAME_CRIMINALS, 0, 2147483647.0);
    CHECK(fixture->playerStatsIntact(FALSE, 0));
    SetField(api, GAME_PLAYER_RESPECT, 0, 0.0);
    SetField(api, GAME_PLAYER_APPEAL, 0, 1000.0);
    SetField(api, GAME_CRIMINALS, 0, 0.0);
    ExpectField(api, GAME_PLAYER_RESPECT, 0, 0.0);
    ExpectField(api, GAME_PLAYER_APPEAL, 0, 1000.0);
    ExpectField(api, GAME_CRIMINALS, 0, 0.0);
    CHECK(fixture->playerStatsIntact(FALSE, 0));
    puts("PASS: numeric respect, appeal, criminal count and wanted, with read-only and adjacent-field checks.");
}

static void TestWeapons(const SteamAbi *api)
{
    static const int pistolSlot = 1;
    CHECK(api->weapon(-2, 0, 0, 0) == INVALID);
    CHECK(api->weapon(11, 0, 0, 0) == INVALID);
    CHECK(api->weapon(pistolSlot, 30, 30, 10) == INVALID);
    CHECK(api->weapon(pistolSlot, 22, 30, 31) == INVALID);
    CHECK(api->weapon(pistolSlot, 0, 30, 0) == INVALID);
    CHECK(api->weapon(pistolSlot, 0, 0, 0) == SUCCESS);
    ExpectField(api, GAME_WEAPON_TYPE, pistolSlot, 0.0);
    ExpectField(api, GAME_WEAPON_TOTAL, pistolSlot, 0.0);
    ExpectField(api, GAME_WEAPON_LOADED, pistolSlot, 0.0);
    puts("PASS: typed weapon slot rejects invalid slot/model/ammunition and accepts a cleared slot.");
}

static void TestFeatures(const SteamAbi *api, const FixtureAbi *fixture)
{
    double invalidColor = 127.0;
    GamePosition carStart = { 50.0, 50.0, 0.0, 90.0 };
    GamePosition carTarget = { 102.25, 72.5, 6.25, 185.0 };
    GamePosition pedTarget = { 52.25, 22.5, 6.25, 0.0 };

    CHECK(api->featureGet(FEATURE_MISSION_TIMER) == 0);
    CHECK(api->featureSet(FEATURE_MISSION_TIMER, 1) == SUCCESS);
    CHECK(api->featureGet(FEATURE_MISSION_TIMER) == 1);
    CHECK(api->featureSet(FEATURE_MISSION_TIMER, 0) == SUCCESS);
    CHECK(api->featureGet(FEATURE_MISSION_TIMER) == 0);
    CHECK(api->featureSet(2, 1) == INVALID);
    CHECK(api->girlfriendSet(5, 75, 2) == SUCCESS);
    CHECK(api->girlfriendSet(6, 75, 2) == INVALID);
    CHECK(api->girlfriendSet(5, 0, 0) == SUCCESS);

    CHECK(api->coffeeGet() == 1);
    CHECK(api->coffeeSet(0) == SUCCESS);
    CHECK(api->coffeeGet() == 0);
    CHECK(api->coffeeSet(1) == SUCCESS);
    CHECK(fixture->coffeeSupported(0) == 1);
    CHECK(api->coffeeGet() == COFFEE_UNAVAILABLE);
    CHECK(api->coffeeSet(0) == INVALID);
    CHECK(fixture->coffeeSupported(1) == 1);

    CHECK(api->feedbackEnable(1) == SUCCESS);
    CHECK(api->feedbackSend(L"ABI check", 9) == SUCCESS);
    CHECK(fixture->run() == 1);
    CHECK(api->feedbackEnable(0) == SUCCESS);

    CHECK(api->plateSet(L"ABI 123", 7) == NOT_READY);
    CHECK(fixture->vehicle(1) != 0);
    ExpectField(api, GAME_VEHICLE_MODEL, 0, 411.0);
    SetField(api, GAME_VEHICLE_PRIMARY, 0, 126.0);
    SetField(api, GAME_VEHICLE_SECONDARY, 0, 12.0);
    CHECK(api->field(GAME_VEHICLE_PRIMARY, 0, &invalidColor, TRUE) == INVALID);
    ExpectField(api, GAME_VEHICLE_PRIMARY, 0, 126.0);
    ExpectField(api, GAME_VEHICLE_SECONDARY, 0, 12.0);
    ExpectField(api, GAME_PLAYER_PROTECTION, GAME_PROOF_BULLET, 1.0);
    CHECK(api->plateSet(L"123456789", 9) == INVALID);
    CHECK(api->plateSet(L"ABI 123", 7) == SUCCESS);
    CHECK(api->plateStatus() == PENDING);
    CHECK(fixture->run() == 1);
    CHECK(api->plateStatus() == SUCCESS);
    ExpectPosition(api, &carStart);
    CHECK(api->position(&carTarget, TRUE) == SUCCESS);
    ExpectPosition(api, &carTarget);
    CHECK(fixture->vehicle(0) == 1);
    ExpectPosition(api, &pedTarget);
    puts("PASS: vehicle model/colors, relative player teleport, timer, relationships, feedback, and plate.");
}

static void TestFixture(const WCHAR *path)
{
    HMODULE library = OpenLibrary(path);
    SteamAbi api;
    FixtureAbi fixture;
    DWORD self;

    ImportSteam(library, &api);
    IMPORT(library, &fixture, begin, "SteamFixtureBegin");
    IMPORT(library, &fixture, run, "SteamFixtureRun");
    IMPORT(library, &fixture, vehicle, "SteamFixtureVehicle");
    IMPORT(library, &fixture, coffeeSupported, "SteamFixtureCoffeeSupported");
    IMPORT(library, &fixture, disturbGarage, "SteamFixtureDisturbGarage");
    IMPORT(library, &fixture, playerStatsIntact, "_SteamFixturePlayerStatsIntact@8");
    TestDisconnected(&api);
    self = fixture.begin();
    CHECK(self == (DWORD)(ULONG_PTR)GetCurrentProcess());
    CHECK(api.attach((HANDLE)(ULONG_PTR)self) != 0);
    TestGarages(&api, &fixture);
    TestPlayer(&api);
    TestNumericPlayerStats(&api, &fixture);
    TestWeapons(&api);
    TestFeatures(&api, &fixture);
    CHECK(FreeLibrary(library));
}

int wmain(int argc, WCHAR **argv)
{
    if (argc != 3) {
        fputs("Usage: steam_abi_test.exe <absolute release DLL path> <absolute fixture DLL path>\n",
              stderr);
        return EXIT_FAILURE;
    }
    TestProduction(argv[1]);
    TestFixture(argv[2]);
    return EXIT_SUCCESS;
}
