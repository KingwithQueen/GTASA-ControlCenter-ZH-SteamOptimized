/* Console command IDs and raw parameter units follow shortcut_commands.h. */
static DWORD WriteField(GameField field,int index,double value)
{
    return runtime.backend.field(field,index,&value,TRUE);
}

static DWORD ToggleField(GameField field,int index)
{
    double value = 0;
    DWORD status = runtime.backend.field(field,index,&value,FALSE);
    return status ? status : WriteField(field,index,value == 0);
}

static DWORD ChangeField(GameField field,int index,double amount,double minimum,double maximum)
{
    double value = 0;
    DWORD status = runtime.backend.field(field,index,&value,FALSE);
    if (status)
        return status;
    value += amount;
    return WriteField(field,index,max(minimum,min(maximum,value)));
}

static DWORD LockCurrent(GameField field,int index,BOOL enabled)
{
    double value = 0;
    DWORD status = enabled ? runtime.backend.field(field,index,&value,FALSE) : 0;
    return status ? status : SetLock(field,index,value,enabled);
}

static int ParseParameters(LPCWSTR raw,double values[4])
{
    int count = 0;
    WCHAR *end;
    if (!raw || !*raw)
        return 0;
    while (count < 4) {
        double value = wcstod(raw,&end);
        if (raw == end || !_finite(value))
            return -1;
        values[count++] = value;
        if (!*end)
            return count;
        if (*end != L';')
            return -1;
        raw = end+1;
    }
    return -1;
}

static DWORD AbsoluteSpeed(GameField field,double amount,BOOL increase)
{
    double x = 0,y = 0,heading = 0,length;
    GameField direction = field == GAME_PLAYER_SPEED ? GAME_PLAYER_HEADING : GAME_VEHICLE_HEADING;
    DWORD status = runtime.backend.field(field,0,&x,FALSE);
    if (!status)
        status = runtime.backend.field(field,1,&y,FALSE);
    if (status)
        return status;
    length = sqrt(x*x+y*y);
    if (length > 0.001) {
        x /= length;
        y /= length;
    } else {
        status = runtime.backend.field(direction,0,&heading,FALSE);
        if (status)
            return status;
        x = cos(heading * 0.017453292519943295);
        y = sin(heading * 0.017453292519943295);
    }
    amount = max(-400.0,min(400.0,amount + (increase ? length : 0.0)));
    status = WriteField(field,0,x*amount);
    return status ? status : WriteField(field,1,y*amount);
}

static DWORD Turn(GameField field,double amount)
{
    double value = 0;
    DWORD status = runtime.backend.field(field,0,&value,FALSE);
    return status ? status : WriteField(field,0,fmod(value+amount+3600.0,360.0));
}

static int WeaponSlot(int weapon)
{
    if (weapon == 1) return -1;
    if (weapon >= 2 && weapon <= 9) return 0;
    if (weapon >= 10 && weapon <= 15) return 9;
    if ((weapon >= 16 && weapon <= 18) || weapon == 39) return 7;
    if (weapon >= 22 && weapon <= 24) return 1;
    if (weapon >= 25 && weapon <= 27) return 2;
    if (weapon == 28 || weapon == 29 || weapon == 32) return 3;
    if (weapon == 30 || weapon == 31) return 4;
    if (weapon == 33 || weapon == 34) return 5;
    if (weapon >= 35 && weapon <= 38) return 6;
    if (weapon >= 41 && weapon <= 43) return 8;
    if (weapon >= 44 && weapon <= 46) return 10;
    return -2;
}

static DWORD GiveLegacyWeapon(int model,double ammo)
{
    static const int models[] = {
        0,331,333,334,335,336,337,338,339,341,321,322,323,324,325,326,342,343,344,0,0,0,
        346,347,348,349,350,351,352,353,355,356,372,357,358,359,360,361,362,363,364,
        365,366,367,368,369,371
    };
    int weapon, slot;
    for (weapon = 1; weapon < ARRAYSIZE(models) && models[weapon] != model; ++weapon) {}
    if (weapon == ARRAYSIZE(models))
        return 3;
    slot = WeaponSlot(weapon);
    if (slot < -1)
        return 10;
    return runtime.backend.weapon(slot,weapon,slot < 0 ? 1 : (DWORD)ammo,0);
}

static DWORD PrepareDate(int index)
{
    static const GamePosition destinations[] = {
        {2402.54,-1727.254,12.874,90}, {-1791.83,1193.698,24.561,90},
        {-373.616,-1441.395,25.927,90}, {-2575.641,1142.277,55.382,90},
        {-1404.344,2640.061,55.887,90}, {2037.287,2734.815,10.415,90}
    };
    GamePosition position = destinations[index];
    DWORD status = runtime.backend.position(&position,TRUE);
    if (status)
        return status;
    if (index == 1 || index == 2 || index == 4) {
        SetLock(GAME_PLAYER_FAT,0,0,FALSE);
        status = WriteField(GAME_PLAYER_FAT,0,index == 2 ? 0 : 750);
    }
    if (!status && (index == 2 || index == 3)) {
        SetLock(GAME_PLAYER_MUSCLE,0,0,FALSE);
        status = WriteField(GAME_PLAYER_MUSCLE,0,index == 2 ? 200 : 1000);
    }
    return status;
}

static DWORD RunConsoleCommand(int command,int page,LPCWSTR raw)
{
    static const int toggleCheats[] = {0,1,2,3,4,5,6,7,12,13};
    double parameters[4] = { 0 }, value, current = 0;
    int count = page == 17 ? 0 : ParseParameters(raw,parameters);
    int index;
    DWORD status = 0;
    GameField field;
    if (page != 17 && count <= 0)
        return 3;
    value = parameters[0];
    if (command >= 85 && command <= 94)
        return ToggleField(GAME_PLAYER_CHEAT,toggleCheats[command-85]);
    if (command >= 79 && command <= 84)
        return PrepareDate(command-79);
    if (command >= 116 && command <= 121)
        return ToggleField(GAME_PLAYER_CHEAT,command-102);
    if (command >= 21 && command <= 28)
        return WriteField(command < 25 ? GAME_VEHICLE_SPEED : GAME_VEHICLE_SPIN,(command-21)%4,0);
    if (command >= 30 && command <= 35)
        return WriteField(command < 33 ? GAME_VEHICLE_SPEED : GAME_VEHICLE_SPIN,(command-30)%3,value/10);
    if (command >= 36 && command <= 47)
        return ChangeField(command < 42 ? GAME_VEHICLE_SPEED : GAME_VEHICLE_SPIN,(command-36)%3,
                           ((command-36)/3)%2 ? -value/10 : value/10,-400,400);
    if (command >= 97 && command <= 100)
        return WriteField(GAME_PLAYER_SPEED,command-97,0);
    if (command >= 101 && command <= 103)
        return WriteField(GAME_PLAYER_SPEED,command-101,value/10);
    if (command >= 104 && command <= 109)
        return ChangeField(GAME_PLAYER_SPEED,(command-104)%3,command < 107 ? value/10 : -value/10,-400,400);
    switch (command) {
    case 0: return WriteField(GAME_PLAYER_ARMOR,0,value);
    case 1: return SetLock(GAME_PLAYER_ARMOR,0,value,TRUE);
    case 2: return LockCurrent(GAME_PLAYER_ARMOR,0,value != 0);
    case 3: return WriteField(GAME_PLAYER_HEALTH,0,value);
    case 4: return SetLock(GAME_PLAYER_HEALTH,0,value,TRUE);
    case 5: return LockCurrent(GAME_PLAYER_HEALTH,0,value != 0);
    case 6:
        if (count != 4)
            return 3;
        for (index = 0; index < 4 && !status; ++index)
            status = WriteField(GAME_VEHICLE_PROTECTION,index,parameters[index]);
        return status;
    case 7:
        for (index = 0; index < 4 && !status; ++index)
            status = LockCurrent(GAME_VEHICLE_PROTECTION,index,value != 0);
        return status;
    case 8: return WriteField(GAME_VEHICLE_DOORS,0,1.0-value);
    case 9: return SetLock(GAME_VEHICLE_DOORS,0,1.0,value != 0);
    case 10: return WriteField(GAME_VEHICLE_WHEELS,0,value);
    case 11: return WriteField(GAME_VEHICLE_ENGINE,0,value/10);
    case 12: return LockCurrent(GAME_VEHICLE_ENGINE,0,value != 0);
    case 13: return WriteField(GAME_VEHICLE_WEIGHT,0,value/10);
    case 14: return LockCurrent(GAME_VEHICLE_WEIGHT,0,value != 0);
    case 15:
        if (count != 4)
            return 3;
        status = WriteField(GAME_VEHICLE_PRIMARY,0,value);
        if (!status)
            status = WriteField(GAME_VEHICLE_SECONDARY,0,parameters[2]);
        if (!status && parameters[1])
            status = SetLock(GAME_VEHICLE_PRIMARY,0,value,TRUE);
        if (!status && parameters[3])
            status = SetLock(GAME_VEHICLE_SECONDARY,0,parameters[2],TRUE);
        return status;
    case 16:
        status = LockCurrent(GAME_VEHICLE_PRIMARY,0,value != 0);
        return status ? status : LockCurrent(GAME_VEHICLE_SECONDARY,0,value != 0);
    case 17: return WriteField(GAME_VEHICLE_AUTO_ALARM,0,value);
    case 18: return WriteField(GAME_VEHICLE_HEADING,0,value*45);
    case 19:
        return count == 2 && value >= 0 && value <= 7 && floor(value) == value
            ? WriteField(GAME_VEHICLE_KICK,(int)value*45,-parameters[1]/10) : 3;
    case 20: return WriteField(GAME_VEHICLE_FLIP,0,1);
    case 29:
        status = WriteField(GAME_VEHICLE_SPEED,3,0);
        return status ? status : WriteField(GAME_VEHICLE_SPIN,3,0);
    case 48:
        if (value == -92) return WriteField(GAME_CLOCK_SPEED,0,0);
        if (value == -91) return WriteField(GAME_CLOCK_SPEED,0,100.0/60);
        return WriteField(GAME_CLOCK_SPEED,0,value < 0 ? 100+value : 100+value*10);
    case 49: case 50: return ChangeField(GAME_CLOCK_SPEED,0,command == 49 ? 10 : -10,0,1000);
    case 51: return WriteField(GAME_PLAYER_CHEATED,0,0);
    case 52: return WriteField(GAME_CLOCK_SPEED,0,100.0/60);
    case 53: case 76: return WriteField(GAME_CLOCK_SPEED,0,100);
    case 54: case 55: return AbsoluteSpeed(GAME_VEHICLE_SPEED,value/10,command == 55);
    case 56: case 112:
        field = command == 56 ? GAME_VEHICLE_FLIGHT_ASSIST : GAME_PLAYER_FLIGHT_ASSIST;
        return WriteField(field,1,value);
    case 57: case 113:
        field = command == 57 ? GAME_VEHICLE_FLIGHT_ASSIST : GAME_PLAYER_FLIGHT_ASSIST;
        return WriteField(field,0,value/10);
    case 59:
        status = runtime.backend.feedback(value != 0);
        if (!status)
            runtime.feedbackEnabled = value != 0;
        return status;
    case 60: return WriteField(GAME_VEHICLE_AUTO_ALARM,1,1);
    case 61: case 62: case 65: case 66: case 67: case 70: case 71:
        return WriteField(GAME_VEHICLE_HISTORY,command,value);
    case 68: case 69: return Turn(GAME_VEHICLE_HEADING,command == 68 ? value : -value);
    case 72: return WriteField(GAME_WORLD_SPEED,0,value < 0 ? 100+value : 100+value*10);
    case 73: case 74: return ChangeField(GAME_WORLD_SPEED,0,command == 73 ? 10 : -10,10,1000);
    case 75: return WriteField(GAME_CLOCK_SPEED,0,0);
    case 95: case 96:
        status = runtime.backend.field(GAME_CLOCK_MINUTES,0,&current,FALSE);
        if (!status) {
            double next = current + (command == 95 ? 60 : -60);
            if (next < 0 || next >= 1440) {
                double day = 0;
                status = runtime.backend.field(GAME_CLOCK_DAY,0,&day,FALSE);
                if (!status && day + (next < 0 ? -1 : 1) >= 0)
                    status = WriteField(GAME_CLOCK_DAY,0,day + (next < 0 ? -1 : 1));
                else if (!status)
                    return 2;
            }
            if (!status)
                status = WriteField(GAME_CLOCK_MINUTES,0,fmod(next+1440,1440));
        }
        return status;
    case 110: case 111: return AbsoluteSpeed(GAME_PLAYER_SPEED,value/10,command == 111);
    case 114: case 115: return Turn(GAME_PLAYER_HEADING,command == 114 ? value : -value);
    case 122: return WriteField(GAME_VEHICLE_SPAWN,0,value);
    case 123: return WriteField(GAME_WEATHER,0,value);
    case 124:
        status = runtime.backend.field(GAME_WEATHER,1,&current,FALSE);
        if (status)
            return status;
        if (current >= 0)
            return SetLock(GAME_WEATHER,0,0,FALSE);
        status = runtime.backend.field(GAME_WEATHER,0,&current,FALSE);
        return status ? status : SetLock(GAME_WEATHER,0,current,TRUE);
    case 125: case 126: return ToggleField(GAME_PLAYER_CHEAT,command-105);
    case 127:
        if (count != 2 || value < 321 || value > 372 || floor(value) != value
            || parameters[1] < 0 || parameters[1] > 99999)
            return 3;
        return GiveLegacyWeapon((int)value,parameters[1]);
    case 128:
        status = runtime.backend.field(GAME_WEAPON_SLOT,0,&current,FALSE);
        return status ? status : current <= 11 ? WriteField(GAME_WEAPON_TYPE,(int)current-1,0) : 10;
    case 129:
        for (index = -1; index < 11 && !status; ++index)
            status = WriteField(GAME_WEAPON_TYPE,index,0);
        return status;
    default: return 10;
    }
}
