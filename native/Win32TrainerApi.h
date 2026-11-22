// Typed interface for the Win32 front end. Compiled only into its backend build.
#include "../win32-ui/trainer.h"

namespace {
constexpr DWORD ok = 0, offline = 1, notReady = 2, invalid = 3, failed = 4,
                busy = 8, unsupported = 10;
constexpr double radians = 0.017453292519943295;
struct UiOptions {
    bool rc, wheels, alarm, restart, burn, explode, pedFlight, carFlight;
    double pedLevel, carLevel;
};
UiOptions uiOptions = {};
struct VehicleIdentity { DWORD entity, matrix; WORD model; };
VehicleIdentity uiHistory[2] = {};

enum class Storage { Byte, Word, Long, Float, Bit };
struct FieldRef {
    DWORD address;
    Storage storage;
    double factor, minimum, maximum;
    BYTE mask;
};

DWORD uiPed()
{
    DWORD ped = u32(va(0xC100D0));
    return privateWritable(ped, 0x79C) ? ped : 0;
}

DWORD uiVehicle(DWORD ped)
{
    DWORD vehicle = 0;
    if (!ped)
        return 0;
    if (uiOptions.rc) {
        if (!currentVehicle(vehicle) || vehicle == ped)
            return 0;
    } else if (!(u32(ped + 0x46C) & 0x100) || !read(ped + 0x58C, &vehicle, 4)) {
        return 0;
    }
    WORD model = 0;
    return privateWritable(vehicle, 0x500) && read(vehicle + 0x22, &model, 2)
        && model >= 400 && model <= 611 ? vehicle : 0;
}

DWORD uiMatrix(DWORD entity)
{
    DWORD matrix = entity ? u32(entity + 20) : 0;
    return privateWritable(matrix, 60) ? matrix : 0;
}

FieldRef field(DWORD address, Storage storage, double minimum, double maximum,
               double factor = 1.0, BYTE mask = 0)
{
    return {address, storage, factor, minimum, maximum, mask};
}

FieldRef gameGlobal(DWORD legacy, Storage storage, double minimum, double maximum,
                    double factor = 1.0)
{
    return field(mapped(legacy, storage == Storage::Byte ? 1 : 4), storage, minimum, maximum, factor);
}

DWORD fieldValue(FieldRef ref, double* value, bool set)
{
    if (!ref.address)
        return notReady;
    if (set && (!std::isfinite(*value) || *value < ref.minimum || *value > ref.maximum))
        return invalid;
    double raw = *value * ref.factor;
    if (ref.storage == Storage::Float) {
        float data = set ? float(raw) : 0.0f;
        if (set ? !write(ref.address, &data, 4) : !read(ref.address, &data, 4))
            return failed;
        if (!set && !std::isfinite(data))
            return failed;
        if (!set)
            *value = data / ref.factor;
        return ok;
    }
    if (ref.storage == Storage::Bit) {
        BYTE data;
        if (!read(ref.address, &data, 1))
            return failed;
        if (!set) {
            *value = (data & ref.mask) != 0;
            return ok;
        }
        BYTE next = BYTE(*value ? data | ref.mask : data & ~ref.mask);
        return (next == data || write(ref.address, &next, 1)) ? ok : failed;
    }
    if (set && std::floor(raw) != raw)
        return invalid;
    DWORD data = set ? DWORD(raw) : 0;
    SIZE_T size = ref.storage == Storage::Byte ? 1 : ref.storage == Storage::Word ? 2 : 4;
    if (set ? !write(ref.address, &data, size) : !read(ref.address, &data, size))
        return failed;
    if (!set)
        *value = data / ref.factor;
    return ok;
}

DWORD verifiedField(int which, int index, double* value, bool set)
{
    constexpr DWORD cheats[] = {
        0x969171,0x969174,0x96916D,0x96916E,0x969178,0x969164,0x969173,0x96916C,0x96917F,0x969180,
        0x96915F,0x96915E,0xB7CEE4,0xB7CEE6,0x96914C,0x96917A,0x969161,0x969165,0x969153,0x969160
    };
    switch (which) {
    case GAME_PLAYER_MONEY: return fieldValue(gameGlobal(0xB7CE50,Storage::Long,0,999999999), value, set);
    case GAME_PLAYER_FAT: return fieldValue(gameGlobal(0xB793D4,Storage::Float,0,1000), value, set);
    case GAME_PLAYER_STAMINA: return fieldValue(gameGlobal(0xB793D8,Storage::Float,0,1000), value, set);
    case GAME_PLAYER_MUSCLE: return fieldValue(gameGlobal(0xB793DC,Storage::Float,0,1000), value, set);
    case GAME_PLAYER_LUNG: return fieldValue(gameGlobal(0xB791A4,Storage::Long,0,1000), value, set);
    case GAME_PLAYER_GAMBLING: return fieldValue(gameGlobal(0xB794C4,Storage::Float,0,1000), value, set);
    case GAME_PLAYER_DRIVING: return fieldValue(gameGlobal(0xB790A0,Storage::Long,0,1000), value, set);
    case GAME_PLAYER_BIKING: return fieldValue(gameGlobal(0xB791B4,Storage::Long,0,1000), value, set);
    case GAME_PLAYER_CYCLING: return fieldValue(gameGlobal(0xB791B8,Storage::Long,0,1000), value, set);
    case GAME_PLAYER_FLYING: return fieldValue(gameGlobal(0xB7919C,Storage::Long,0,1000), value, set);
    case GAME_WEAPON_SKILL:
        if (index < 0 || index >= 10)
            return invalid;
        return fieldValue(gameGlobal(0xB79494 + DWORD(index)*4,Storage::Float,0,1000), value, set);
    case GAME_WORLD_SPEED: return fieldValue(gameGlobal(0xB7CB64,Storage::Float,10,1000,0.01), value, set);
    case GAME_CLOCK_DAY: {
        DWORD status = fieldValue(gameGlobal(0xB79038,Storage::Long,0,999999), value, set);
        if (status || !set)
            return status;
        const BYTE weekday = BYTE((DWORD(*value)+3)%7+1); // May 1, 1991 was Wednesday.
        return write(mapped(0xB7014E,1),&weekday,1) ? ok : failed;
    }
    case GAME_PLAYER_CHEAT:
        if (index >= 20 && index <= 21) {
            if (set)
                return *value == 0 || *value == 1 ? SteamFeatureSet(index - 20, *value != 0) : invalid;
            *value = SteamFeatureGet(index - 20);
            return ok;
        }
        if (index < 0 || index >= int(sizeof cheats / sizeof cheats[0]))
            return invalid;
        return fieldValue(gameGlobal(cheats[index],Storage::Byte,0,1), value, set);
    case GAME_RELATIONSHIP: {
        constexpr DWORD relationship[] = {0xB79210,0xB79214,0xB79218,0xB79220,0xB7921C,0xB79224};
        if (index < 0 || index >= 6)
            return invalid;
        if (set)
            return *value >= 0 && *value <= 100 && std::floor(*value) == *value
                ? SteamGirlfriendSet(index,DWORD(*value),2) : invalid;
        return fieldValue(gameGlobal(relationship[index],Storage::Long,0,100), value, false);
    }
    case GAME_PLAYER_CHEATED:
        if (index < 0 || index > 1 || (set && *value != 0))
            return invalid;
        if (set && index == 0) {
            double zero = 0.0;
            DWORD status = fieldValue(gameGlobal(0xB79044,Storage::Long,0,0), &zero, true);
            if (status)
                return status;
        }
        return fieldValue(index ? gameGlobal(0xB79044,Storage::Long,0,0)
                                : field(va(0x9E0CB4),Storage::Byte,0,0), value, set);
    case GAME_CLOCK_MINUTES: {
        BYTE hour = 0, minute = 0;
        DWORD clockHour = mapped(0xB70153,1), clockMinute = mapped(0xB70152,1);
        if (!clockHour || !clockMinute)
            return notReady;
        if (!set) {
            if (!read(clockHour,&hour,1) || !read(clockMinute,&minute,1))
                return failed;
            *value = hour * 60 + minute;
            return ok;
        }
        if (*value < 0 || *value > 1439 || std::floor(*value) != *value)
            return invalid;
        hour = BYTE(int(*value) / 60);
        minute = BYTE(int(*value) % 60);
        return write(clockHour,&hour,1) && write(clockMinute,&minute,1) ? ok : failed;
    }
    case GAME_CLOCK_SPEED: {
        DWORD address = mapped(0xB7015C,4), milliseconds = 0;
        if (!address)
            return notReady;
        if (!set) {
            if (!read(address,&milliseconds,4))
                return failed;
            *value = milliseconds >= 3600000 ? 0.0 : milliseconds ? 100000.0 / milliseconds : 1000.0;
            return ok;
        }
        if (*value < 0 || *value > 1000)
            return invalid;
        double duration = *value > 0 ? 100000.0 / *value : 3600000.0;
        milliseconds = duration >= 3600000.0 ? 3600000 : DWORD(duration);
        return write(address,&milliseconds,4) ? ok : failed;
    }
    case GAME_WEATHER: {
        const DWORD weather[] = {0xD0D06C,0xD0D070,0xD0D068};
        if (index < 0 || index > 1)
            return invalid;
        short item = 0;
        if (!set) {
            if (!read(va(weather[index ? 2 : 0]),&item,2))
                return failed;
            *value = item;
            return ok;
        }
        if (*value < (index ? -1 : 0) || *value > 45 || std::floor(*value) != *value)
            return invalid;
        item = short(*value);
        if (index)
            return write(va(weather[2]),&item,2) ? ok : failed;
        return write(va(weather[0]),&item,2) && write(va(weather[1]),&item,2) ? ok : failed;
    }
    case GAME_PLAYER_COFFEE: {
        if (set)
            return unsupported;
        DWORD selection = SteamCoffeeGet();
        if (selection > 1)
            return unsupported;
        *value = selection == 0;
        return ok;
    }
    default: return unsupported;
    }
}

DWORD setHeading(DWORD entity, bool vehicle, double heading, bool flip)
{
    if (heading < 0 || heading > 360)
        return invalid;
    const float angle = float(heading * radians);
    if (!vehicle) {
        float directions[] = {angle, angle};
        return write(entity + 1368,directions,sizeof directions) ? ok : failed;
    }
    DWORD matrix = uiMatrix(entity);
    float axes[12] = {};
    if (!matrix || !read(matrix,axes,sizeof axes))
        return notReady;
    axes[0] = std::sin(angle);
    axes[1] = -std::cos(angle);
    axes[2] = 0.0f;
    axes[4] = std::cos(angle);
    axes[5] = std::sin(angle);
    axes[6] = 0.0f;
    if (flip) {
        axes[8] = axes[9] = 0.0f;
        axes[10] = 1.0f;
    }
    return write(matrix,axes,sizeof axes) ? ok : failed;
}

DWORD getHeading(DWORD entity, bool vehicle, double* heading)
{
    float angle = 0;
    if (vehicle) {
        float forward[2] = {};
        DWORD matrix = uiMatrix(entity);
        if (!matrix || !read(matrix + 16,forward,sizeof forward)
            || !std::isfinite(forward[0]) || !std::isfinite(forward[1]))
            return notReady;
        angle = std::atan2(forward[1],forward[0]);
    } else if (!read(entity + 1372,&angle,4) || !std::isfinite(angle)) {
        return failed;
    }
    *heading = std::fmod(double(angle) / radians + 360.0,360.0);
    return ok;
}

struct PositionMove {
    DWORD entity, matrix;
    float before[3], velocity[3], after[3];
    bool moved, stopped;
};

bool loadMove(DWORD entity, PositionMove& move)
{
    move.entity = entity;
    move.matrix = uiMatrix(entity);
    if (!move.matrix || !read(move.matrix+48,move.before,sizeof move.before)
        || !read(entity+68,move.velocity,sizeof move.velocity))
        return false;
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(move.before[axis]) || std::abs(move.before[axis]) > 20000
            || !std::isfinite(move.velocity[axis]))
            return false;
    return true;
}

void restoreMove(const PositionMove& move)
{
    float current[3] = {}, zero[3] = {};
    if (uiMatrix(move.entity) != move.matrix)
        return;
    if (move.moved && read(move.matrix+48,current,sizeof current)
        && !memcmp(current,move.after,sizeof current))
        write(move.matrix+48,move.before,sizeof move.before);
    if (move.stopped && read(move.entity+68,current,sizeof current)
        && !memcmp(current,zero,sizeof current))
        write(move.entity+68,move.velocity,sizeof move.velocity);
}

bool vehicleIdentity(DWORD vehicle,VehicleIdentity& identity)
{
    WORD model = 0;
    if (!privateWritable(vehicle,0x500) || !read(vehicle+34,&model,2) || model < 400 || model > 611)
        return false;
    DWORD matrix = uiMatrix(vehicle);
    if (!matrix)
        return false;
    identity = {vehicle,matrix,model};
    return true;
}

void trackVehicle(DWORD ped,DWORD vehicle)
{
    VehicleIdentity current = {};
    if (!vehicle && ped && !uiHistory[0].entity)
        vehicle = u32(ped+1420);
    if (!vehicleIdentity(vehicle,current))
        return;
    if (current.entity != uiHistory[0].entity || current.matrix != uiHistory[0].matrix
        || current.model != uiHistory[0].model) {
        uiHistory[1] = uiHistory[0];
        uiHistory[0] = current;
    }
}

DWORD historyAction(int command,double value)
{
    const DWORD ped = uiPed(), active = uiVehicle(ped);
    trackVehicle(ped,active);
    if (!ped)
        return notReady;
    if (command != 61 && command != 62 && (command < 65 || command > 67) && command != 70 && command != 71)
        return invalid;
    const VehicleIdentity& previous = uiHistory[command == 61 || command == 62 || command == 70 ? 0 : 1];
    VehicleIdentity expected = {};
    if (!vehicleIdentity(previous.entity,expected) || expected.matrix != previous.matrix
        || expected.model != previous.model)
        return notReady;
    if (command == 67) {
        double doors = 2.0-value;
        return fieldValue(field(expected.entity+1272,Storage::Byte,1,2),&doors,true);
    }
    if (command == 70 || command == 71) {
        double damage = 5.0;
        return fieldValue(field(expected.entity+1216,Storage::Float,0,4000),&damage,true);
    }
    if (active)
        return notReady;
    bool movePlayer = command == 61 || command == 65;
    DWORD source = movePlayer ? expected.entity : ped;
    DWORD target = movePlayer ? ped : expected.entity;
    PositionMove sourceMove = {}, targetMove = {};
    if (!loadMove(source,sourceMove) || !loadMove(target,targetMove))
        return notReady;
    if (!movePlayer && u32(target+1224))
        return unsupported;
    float direction[2] = {};
    if (!read(sourceMove.matrix+16,direction,sizeof direction)
        || !std::isfinite(direction[0]) || !std::isfinite(direction[1]))
        return notReady;
    targetMove.after[0] = sourceMove.before[0] - 2.0f * direction[1];
    targetMove.after[1] = sourceMove.before[1] + 2.0f * direction[0];
    targetMove.after[2] = sourceMove.before[2];
    if (uiPed() != ped || uiVehicle(ped) || uiMatrix(target) != targetMove.matrix)
        return notReady;
    float stopped[3] = {};
    targetMove.moved = write(targetMove.matrix+48,targetMove.after,sizeof targetMove.after);
    targetMove.stopped = targetMove.moved && write(target+68,stopped,sizeof stopped);
    if (!targetMove.stopped) {
        restoreMove(targetMove);
        return failed;
    }
    return ok;
}

bool allowedWeapon(int index, DWORD weapon)
{
    constexpr DWORD weapons[][9] = {
        {2,3,4,5,6,8,7,9}, {22,23,24}, {25,26,27}, {28,29,32}, {30,31}, {33,34},
        {35,36,37,38}, {16,17,18,39}, {41,42,43}, {14,10,11,12,13,15}, {44,45,46}
    };
    if (!weapon)
        return true;
    if (index == -1)
        return weapon == 1;
    for (DWORD candidate : weapons[index])
        if (candidate == weapon)
            return true;
    return false;
}

DWORD queueWeaponModel(int index,DWORD weapon)
{
    constexpr DWORD models[] = {
        0,331,333,334,335,336,337,338,339,341,321,322,323,324,325,326,342,343,344,0,0,0,
        346,347,348,349,350,351,352,353,355,356,372,357,358,359,360,361,362,363,364,
        365,366,367,368,369,371
    };
    if (weapon == 45) // The original catalog identifies this equipment as unstable.
        return unsupported;
    DWORD model = models[weapon];
    if (!model || !installQueue())
        return model ? (lastError ? lastError : failed) : invalid;
    DWORD entry = remote+40+(index < 0 ? 11 : index)*4, yes = 1;
    DWORD pending = u32(entry);
    if (pending && pending != model)
        return busy;
    if (!write(remote+32,&yes,4) || !write(entry,&model,4))
        return failed;
    enabled = true;
    return ok;
}

DWORD weaponField(int which, int index, double* value, bool set, DWORD ped)
{
    if (index < -1 || index > 10 || (index == -1 && which != GAME_WEAPON_TYPE))
        return invalid;
    DWORD address = ped + 1468 + index * 28;
    if (which != GAME_WEAPON_TYPE || !set) {
        DWORD offset = which == GAME_WEAPON_TYPE ? 0 : which == GAME_WEAPON_LOADED ? 8 : 12;
        return fieldValue(field(address+offset,Storage::Long,0,99999), value, set);
    }
    if (*value < 0 || *value > 46 || std::floor(*value) != *value || !allowedWeapon(index,DWORD(*value)))
        return invalid;
    DWORD data[4] = {};
    if (!read(address,data,sizeof data))
        return failed;
    const DWORD type = DWORD(*value);
    if (data[0] == type)
        return ok;
    if (type) {
        lastError = 0;
        DWORD status = queueWeaponModel(index,type);
        if (status)
            return status;
    }
    data[0] = type;
    data[1] = data[2] = 0;
    data[3] = type ? 1 : 0;
    return write(address,data,sizeof data) ? ok : failed;
}

DWORD applyWeapon(int index,DWORD type,DWORD total,DWORD loaded)
{
    if (index < -1 || index > 10 || type > 46 || !allowedWeapon(index,type)
        || total > 99999 || loaded > total || (!type && (total || loaded)))
        return invalid;
    DWORD ped = uiPed(), data[4] = {};
    if (!ped)
        return notReady;
    DWORD address = ped + 1468 + index * 28;
    if (!read(address,data,sizeof data))
        return failed;
    if (type && data[0] != type) {
        lastError = 0;
        DWORD status = queueWeaponModel(index,type);
        if (status)
            return status;
    }
    if (uiPed() != ped)
        return notReady;
    if (data[0] != type)
        data[1] = 0;
    data[0] = type;
    data[2] = loaded;
    data[3] = total;
    return write(address,data,sizeof data) ? ok : failed;
}

DWORD entityField(int which, int index, double* value, bool set)
{
    DWORD ped = uiPed();
    if (!ped)
        return notReady;
    bool car = which >= GAME_VEHICLE_MODEL && which <= GAME_VEHICLE_FLIP;
    DWORD entity = car ? uiVehicle(ped) : ped;
    if (!entity)
        return notReady;
    switch (which) {
    case GAME_PLAYER_HEALTH: {
        float ceiling = 0;
        if (set && *value <= 1000 && read(ped + 1348,&ceiling,4)
            && std::isfinite(ceiling) && *value > ceiling) {
            float newCap = float(*value);
            if (!write(ped + 1348,&newCap,4))
                return failed;
        }
        return fieldValue(field(ped+1344,Storage::Float,0,1000),value,set);
    }
    case GAME_PLAYER_ARMOR: return fieldValue(field(ped+1352,Storage::Float,0,1000),value,set);
    case GAME_PLAYER_PROTECTION: case GAME_VEHICLE_PROTECTION: {
        constexpr BYTE pedMasks[] = {128,64,4,8}, carMasks[] = {128,16,4,8};
        if (index < 0 || index > 3)
            return invalid;
        return fieldValue(field(entity+66,Storage::Bit,0,1,1,car ? carMasks[index] : pedMasks[index]),value,set);
    }
    case GAME_PLAYER_SPEED: case GAME_VEHICLE_SPEED: case GAME_VEHICLE_SPIN: {
        DWORD start = entity + (which == GAME_VEHICLE_SPIN ? 80 : 68);
        if (index == GAME_AXIS_ALL && set) {
            if (*value < -400 || *value > 400)
                return invalid;
            float speed = float(*value / 50.0);
            float values[3] = {speed,speed,speed};
            return write(start,values,sizeof values) ? ok : failed;
        }
        if (index < 0 || index > 2)
            return invalid;
        return fieldValue(field(start+index*4,Storage::Float,-400,400,0.02),value,set);
    }
    case GAME_PLAYER_HEADING: case GAME_VEHICLE_HEADING:
        return set ? setHeading(entity,car,*value,false) : getHeading(entity,car,value);
    case GAME_PLAYER_KICK: case GAME_VEHICLE_KICK: {
        if (!set || index < 0 || index > 359 || *value < -400 || *value > 400)
            return invalid;
        DWORD status = setHeading(entity,car,index,false);
        float speed = float(*value / 50.0);
        float velocity[] = {float(std::cos(index * radians))*speed,
                            float(std::sin(index * radians))*speed,0};
        return status ? status : write(entity+68,velocity,sizeof velocity) ? ok : failed;
    }
    case GAME_WEAPON_TYPE: case GAME_WEAPON_TOTAL: case GAME_WEAPON_LOADED:
        return weaponField(which,index,value,set,ped);
    case GAME_WEAPON_SLOT:
        return fieldValue(field(ped+1816,Storage::Byte,0,12),value,set);
    case GAME_VEHICLE_MODEL:
        return set ? unsupported : fieldValue(field(entity+34,Storage::Word,400,611),value,false);
    case GAME_VEHICLE_ENGINE:
        return fieldValue(field(entity+1216,Storage::Float,0,400,10),value,set);
    case GAME_VEHICLE_PRIMARY: case GAME_VEHICLE_SECONDARY:
        return fieldValue(field(entity+1076+(which == GAME_VEHICLE_SECONDARY),Storage::Byte,0,126),value,set);
    case GAME_VEHICLE_DOORS: {
        double door = *value + 1.0;
        DWORD status = fieldValue(field(entity+1272,Storage::Byte,1,2),&door,set);
        if (!status && !set)
            *value = door == 2;
        return status;
    }
    case GAME_VEHICLE_WEIGHT: {
        if (!set)
            return fieldValue(field(entity+140,Storage::Float,0.1,400,1000),value,false);
        float mass[2] = {};
        if (*value < 0.1 || *value > 400)
            return invalid;
        if (!read(entity+140,mass,sizeof mass) || !std::isfinite(mass[0])
            || !std::isfinite(mass[1]) || mass[0] <= 0 || mass[1] <= 0)
            return notReady;
        double protect = 1.0;
        DWORD status = fieldValue(field(entity+66,Storage::Bit,0,1,1,16),&protect,true);
        mass[1] = float(mass[1] * (*value * 1000 / mass[0]));
        mass[0] = float(*value * 1000);
        return status ? status : write(entity+140,mass,sizeof mass) ? ok : failed;
    }
    case GAME_VEHICLE_FLIP: {
        if (!set)
            return unsupported;
        double heading = 0;
        DWORD status = getHeading(entity,true,&heading);
        return status ? status : setHeading(entity,true,heading,true);
    }
    default: return unsupported;
    }
}

bool vehicleAuxKind(DWORD vehicle,DWORD& kind,WORD& model)
{
    if (!vehicle || !read(vehicle+34,&model,2) || model < 400 || model > 611)
        return false;
    DWORD info = u32(va(0xB12818)+model*4);
    return (garageData(info,0x4c) || privateWritable(info,0x4c))
        && read(info+60,&kind,4) && kind < 12;
}

void TickFlight(DWORD entity,double level)
{
    float speed = 0.0f;
    DWORD address = entity + 76;
    if (entity && read(address,&speed,4) && std::isfinite(speed) && speed < 0.0f) {
        float lift = float(0.03 + level * 0.02);
        write(address,&lift,4);
    }
}

void TickVehicleOptions(DWORD vehicle)
{
    DWORD kind = 0;
    WORD model = 0;
    if (!vehicle)
        return;
    if (uiOptions.alarm) {
        DWORD zero = 0;
        if (u32(vehicle+1116))
            write(vehicle+1116,&zero,4);
    }
    if (uiOptions.restart) {
        double restart = 1;
        fieldValue(field(vehicle+1064,Storage::Bit,0,1,1,0x10),&restart,true);
    }
    if (uiOptions.burn || uiOptions.explode) {
        float engine = 0.0f;
        if (read(vehicle+1216,&engine,4) && std::isfinite(engine)) {
            if (uiOptions.burn && engine < 500.0f) {
                float healthy = 1000.0f;
                write(vehicle+1216,&healthy,4);
            } else if (uiOptions.explode && engine < 250.0f
                       && vehicleAuxKind(vehicle,kind,model) && kind == 0
                       && privateWritable(vehicle+2276,4)) {
                float zero = 0.0f;
                write(vehicle+2276,&zero,4);
            }
        }
    }
    if (uiOptions.wheels && vehicleAuxKind(vehicle,kind,model)) {
        WORD zero = 0;
        DWORD damage = kind == 0 ? vehicle+1444 : garageCatalog[model-400].bike ? vehicle+1630 : 0;
        if (damage && privateWritable(damage,2) && (u8(damage) || u8(damage+1)))
            write(damage,&zero,2);
    }
}

DWORD SetUiOption(bool& option,double* value,bool set)
{
    if (!set) {
        *value = option;
        return ok;
    }
    if (*value != 0 && *value != 1)
        return invalid;
    option = *value != 0;
    return ok;
}

DWORD TickUiOptions()
{
    DWORD ped = uiPed(), vehicle = uiVehicle(ped);
    trackVehicle(ped,vehicle);
    if (!uiOptions.pedFlight && !uiOptions.carFlight && !uiOptions.wheels && !uiOptions.alarm
        && !uiOptions.restart && !uiOptions.burn && !uiOptions.explode)
        return ok;
    if (uiOptions.pedFlight && !vehicle)
        TickFlight(ped,uiOptions.pedLevel);
    if (uiOptions.carFlight)
        TickFlight(vehicle,uiOptions.carLevel);
    TickVehicleOptions(vehicle);
    return ok;
}
}

extern "C" DWORD __stdcall SteamUiField(int which, int index, double* value, BOOL set)
{
    if (!game || !base)
        return offline;
    if (!value || (set && !std::isfinite(*value)))
        return invalid;
    switch (which) {
    case GAME_FIELD_NONE:
        if (!set)
            return unsupported;
        if (index == 1) {
            uiOptions = {};
            uiHistory[0] = uiHistory[1] = {};
            return ok;
        }
        return index == 0 ? TickUiOptions() : invalid;
    case GAME_VEHICLE_RC: return SetUiOption(uiOptions.rc,value,set != FALSE);
    case GAME_VEHICLE_HISTORY: return set ? historyAction(index,*value) : unsupported;
    case GAME_VEHICLE_WHEELS: return SetUiOption(uiOptions.wheels,value,set != FALSE);
    case GAME_VEHICLE_AUTO_RESTART: return SetUiOption(uiOptions.restart,value,set != FALSE);
    case GAME_VEHICLE_DONT_BURN: return SetUiOption(uiOptions.burn,value,set != FALSE);
    case GAME_VEHICLE_DONT_EXPLODE: return SetUiOption(uiOptions.explode,value,set != FALSE);
    case GAME_VEHICLE_AUTO_ALARM:
        if (index == 1 && set) {
            DWORD vehicle = uiVehicle(uiPed()), zero = 0;
            return !vehicle ? notReady : write(vehicle+1116,&zero,4) ? ok : failed;
        }
        return SetUiOption(uiOptions.alarm,value,set != FALSE);
    case GAME_VEHICLE_FLIGHT_ASSIST: case GAME_PLAYER_FLIGHT_ASSIST: {
        bool car = which == GAME_VEHICLE_FLIGHT_ASSIST;
        if (index == 1)
            return SetUiOption(car ? uiOptions.carFlight : uiOptions.pedFlight,value,set != FALSE);
        double& level = car ? uiOptions.carLevel : uiOptions.pedLevel;
        if (set) {
            if (*value < -100 || *value > 1000 || index != 0)
                return invalid;
            level = *value;
        } else {
            *value = level;
        }
        return ok;
    }
    default: break;
    }
    if (which == GAME_VEHICLE_SPAWNER || which == GAME_VEHICLE_SPAWN) {
        if (set && which == GAME_VEHICLE_SPAWNER && *value != 0 && *value != 1)
            return invalid;
        if (!set) {
            if (which != GAME_VEHICLE_SPAWNER)
                return unsupported;
            *value = enabled;
            return ok;
        }
        if (which == GAME_VEHICLE_SPAWNER && *value == 0) {
            DWORD zero[14] = {};
            if (remote && !write(remote+32,zero,sizeof zero))
                return failed;
            enabled = false;
            return ok;
        }
        if (which == GAME_VEHICLE_SPAWN && (*value < 400 || *value > 611 || std::floor(*value) != *value))
            return invalid;
        lastError = 0;
        if (!installQueue())
            return lastError ? lastError : failed;
        DWORD yes = 1;
        if (!write(remote+32,&yes,4))
            return failed;
        enabled = true;
        if (which == GAME_VEHICLE_SPAWNER)
            return ok;
        if (u32(remote+36))
            return busy;
        DWORD model = DWORD(*value);
        return write(remote+36,&model,4) ? ok : failed;
    }
    DWORD status = verifiedField(which,index,value,set != FALSE);
    return status != unsupported ? status : entityField(which,index,value,set != FALSE);
}

extern "C" DWORD __stdcall SteamUiPosition(GamePosition* value, BOOL set)
{
    if (!game || !base)
        return offline;
    if (!value)
        return invalid;
    DWORD ped = uiPed(), vehicle = uiVehicle(ped);
    DWORD entity = vehicle ? vehicle : ped;
    PositionMove moves[3] = {};
    if (!loadMove(entity,moves[0]))
        return notReady;
    if (!set) {
        value->x = moves[0].before[0];
        value->y = moves[0].before[1];
        value->z = moves[0].before[2];
        return getHeading(entity,vehicle != 0,&value->heading);
    }
    if (!std::isfinite(value->x) || !std::isfinite(value->y) || !std::isfinite(value->z)
        || !std::isfinite(value->heading) || std::abs(value->x) > 20000
        || std::abs(value->y) > 20000 || std::abs(value->z) > 20000
        || value->heading < 0 || value->heading > 360)
        return invalid;
    int count = 1;
    DWORD trailer = 0;
    if (vehicle) {
        if (u32(ped+0x58C) == vehicle && (u32(ped+0x46C)&0x100)) {
            if (!loadMove(ped,moves[count++]))
                return notReady;
        }
        trailer = u32(vehicle+1224);
        if (trailer && trailer != vehicle && trailer != ped) {
            WORD model = 0;
            if (!privateWritable(trailer,0x500) || !read(trailer+34,&model,2)
                || model < 400 || model > 611 || !loadMove(trailer,moves[count++]))
                return notReady;
        }
    }
    if (uiPed() != ped || uiVehicle(ped) != vehicle || (vehicle && u32(vehicle+1224) != trailer))
        return notReady;
    const double target[3] = {value->x,value->y,value->z};
    const float stopped[3] = {};
    for (int index = 0; index < count; ++index) {
        PositionMove& move = moves[index];
        if (uiMatrix(move.entity) != move.matrix)
            return notReady;
        for (int axis = 0; axis < 3; ++axis) {
            double next = target[axis] + move.before[axis] - moves[0].before[axis];
            if (std::abs(next) > 20000)
                return invalid;
            move.after[axis] = float(next);
        }
    }
    DWORD status = ok;
    for (int index = 0; index < count && !status; ++index) {
        PositionMove& move = moves[index];
        move.moved = write(move.matrix+48,move.after,sizeof move.after);
        move.stopped = move.moved && write(move.entity+68,stopped,sizeof stopped);
        if (!move.stopped)
            status = failed;
    }
    if (!status)
        status = setHeading(entity,vehicle != 0,value->heading,false);
    if (status)
        for (int index = count-1; index >= 0; --index)
            restoreMove(moves[index]);
    return status;
}

extern "C" DWORD __stdcall SteamUiPlate(const wchar_t* text, DWORD count)
{
    if (!game || !base)
        return offline;
    DWORD vehicle = uiVehicle(uiPed());
    return vehicle ? SteamPlateSet(vehicle,text,count) : notReady;
}

extern "C" DWORD __stdcall SteamUiWeapon(int index,DWORD type,DWORD total,DWORD loaded)
{
    return game && base ? applyWeapon(index,type,total,loaded) : offline;
}
