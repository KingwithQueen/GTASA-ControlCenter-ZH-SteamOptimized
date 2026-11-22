#ifndef GTASA_UI_TRAINER_H
#define GTASA_UI_TRAINER_H

#include <windows.h>

typedef enum GameResult {
    GAME_OK,
    GAME_QUEUED,
    GAME_OFFLINE,
    GAME_UNSUPPORTED,
    GAME_BUSY,
    GAME_INVALID,
    GAME_NOT_READY,
    GAME_FAILED,
    GAME_LAYOUT_MISMATCH,
    GAME_BACKEND_MISSING
} GameResult;

typedef enum GameField {
    GAME_FIELD_NONE,
    GAME_PLAYER_HEALTH, GAME_PLAYER_ARMOR, GAME_PLAYER_MONEY, GAME_PLAYER_WANTED,
    GAME_PLAYER_FAT, GAME_PLAYER_STAMINA, GAME_PLAYER_MUSCLE, GAME_PLAYER_LUNG,
    GAME_PLAYER_GAMBLING, GAME_PLAYER_DRIVING, GAME_PLAYER_BIKING, GAME_PLAYER_CYCLING,
    GAME_PLAYER_FLYING, GAME_PLAYER_RESPECT, GAME_PLAYER_APPEAL,
    GAME_PLAYER_PROTECTION, GAME_PLAYER_CHEAT, GAME_PLAYER_SPEED,
    GAME_PLAYER_FLIGHT_ASSIST, GAME_PLAYER_HEADING, GAME_PLAYER_KICK,
    GAME_WEAPON_TYPE, GAME_WEAPON_TOTAL, GAME_WEAPON_LOADED, GAME_WEAPON_SKILL,
    GAME_WEAPON_SLOT, GAME_PLAYER_CHEATED,
    GAME_VEHICLE_MODEL, GAME_VEHICLE_ENGINE, GAME_VEHICLE_WEIGHT, GAME_VEHICLE_FLIGHT_ASSIST,
    GAME_VEHICLE_PRIMARY, GAME_VEHICLE_SECONDARY, GAME_VEHICLE_PROTECTION, GAME_VEHICLE_DOORS,
    GAME_VEHICLE_SPEED, GAME_VEHICLE_SPIN, GAME_VEHICLE_HEADING, GAME_VEHICLE_KICK,
    GAME_VEHICLE_WHEELS, GAME_VEHICLE_AUTO_ALARM, GAME_VEHICLE_AUTO_RESTART,
    GAME_VEHICLE_DONT_BURN, GAME_VEHICLE_DONT_EXPLODE, GAME_VEHICLE_RC,
    GAME_VEHICLE_FLIP, GAME_VEHICLE_SPAWN, GAME_VEHICLE_SPAWNER,
    GAME_WEATHER, GAME_CLOCK_MINUTES, GAME_CLOCK_DAY, GAME_CLOCK_SPEED, GAME_WORLD_SPEED,
    GAME_MISSION_TIMER, GAME_CRIMINALS, GAME_RELATIONSHIP, GAME_PLAYER_COFFEE,
    GAME_VEHICLE_HISTORY,
    GAME_FIELD_COUNT
} GameField;

enum {
    GAME_AXIS_X, GAME_AXIS_Y, GAME_AXIS_Z, GAME_AXIS_ALL
};

enum {
    GAME_PROOF_EXPLOSION, GAME_PROOF_COLLISION, GAME_PROOF_BULLET, GAME_PROOF_FIRE
};

typedef struct GamePosition {
    double x, y, z, heading;
} GamePosition;

typedef struct GameGarageSlot {
    unsigned model, primary, secondary, protection;
    DWORD angle;
    unsigned mods[15];
    unsigned tuning[8];
} GameGarageSlot;

typedef struct GameWeapon {
    unsigned type, total, loaded;
} GameWeapon;

typedef enum GameRequestKind {
    GAME_REQ_SESSION, GAME_REQ_READ_FIELD, GAME_REQ_SET_FIELD, GAME_REQ_LOCK_FIELD,
    GAME_REQ_FEATURE, GAME_REQ_COFFEE, GAME_REQ_FEEDBACK, GAME_REQ_GIRLFRIEND, GAME_REQ_PLATE,
    GAME_REQ_POSITION, GAME_REQ_TELEPORT, GAME_REQ_GARAGE_READ, GAME_REQ_GARAGE_WRITE,
    GAME_REQ_GARAGE_LOCK, GAME_REQ_CHEAT, GAME_REQ_COMMAND, GAME_REQ_WEAPON
} GameRequestKind;

typedef struct GameEvent {
    GameRequestKind request;
    GameResult result;
    GameField field;
    int index, mode;
    double value;
    GamePosition position;
    GameWeapon weapon;
    GameGarageSlot slots[4];
} GameEvent;

#define WM_TRAINER_EVENT (WM_APP + 20)

BOOL GameStart(HWND window);
void GameStop(void);
BOOL GameTakeEvent(GameEvent *event);
BOOL GameIsConnected(void);
BOOL GameIsForeground(void);
LPCWSTR GameResultText(GameResult result);
BOOL GameSupportsField(GameField field, int index, BOOL write);

GameResult GameReadField(GameField field, int index);
GameResult GameSetField(GameField field, int index, double value);
GameResult GameLockField(GameField field, int index, double value, BOOL enabled);
GameResult GameSetFeature(int feature, BOOL enabled);
GameResult GameSetCoffee(BOOL enabled);
GameResult GameSetFeedback(BOOL enabled);
GameResult GameSetGirlfriend(int index, int progress, int mode);
GameResult GameSetPlate(LPCWSTR plate);
GameResult GameLockPlate(LPCWSTR plate, BOOL enabled);
GameResult GameSetWeapon(int index, const GameWeapon *weapon);
GameResult GameReadPosition(void);
GameResult GameTeleport(const GamePosition *position);
GameResult GameGarageRead(int index);
GameResult GameGarageWrite(int index, const GameGarageSlot slots[4]);
GameResult GameGarageLock(int index, BOOL enabled, const GameGarageSlot slots[4]);
GameResult GameQueueCheat(LPCWSTR text);
GameResult GameRunCommand(int consoleId, int parameterPage, LPCWSTR raw);

#endif
