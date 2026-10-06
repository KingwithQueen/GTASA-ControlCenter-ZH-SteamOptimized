// Classic Steam CStoredCar::RestoreCar train branch. No DLL callbacks in emitted code.
#pragma once
static constexpr char trainRestoreTag[]="GTASA_STEAM_TRAIN_RESTORE_V1";
static constexpr DWORD trainBranchOffset=0x40,trainAllocationSize=4096,trainSwitchIndex=5;
static constexpr DWORD trainRestoreTable[11]={0x44adf6,0x44ae28,0x44ae5a,0x44ae8c,0x44af5e,
 0x44af89,0x44af89,0x44af89,0x44aebe,0x44aeff,0x44af33};
static constexpr BYTE trainCtorSignature[]={0x55,0x8b,0xec,0x6a,0xff,0x68,0x58,0x88,0x89,0,0x64,0xa1};
static DWORD trainAddress(DWORD image,DWORD preferred){return image+preferred-0x400000;}
static std::vector<BYTE> trainRestoreCode(DWORD image,DWORD start){
 std::vector<BYTE> b;
 auto bytes=[&](std::initializer_list<BYTE> v){b.insert(b.end(),v);};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)b.push_back(BYTE(v>>(8*i)));};
 auto jump=[&](BYTE op,DWORD target){bytes({op});word(trainAddress(image,target)-(start+(DWORD)b.size()+4));};
 // Match the original allocator and SEH frame; allocation failure uses its null return.
 bytes({0x68});word(0x6ac);jump(0xe8,0x71d270);bytes({0x83,0xc4,4,0x85,0xc0,0x0f,0x84});
 word(trainAddress(image,0x44b1c8)-(start+(DWORD)b.size()+4));
 bytes({0x89,0x45,0xe8,0xc7,0x45,0xfc,0xff,0xff,0xff,0xff,0x0f,0xb7,0x4f,0x12,0x6a,1,0x51,0x8b,0xc8});
 jump(0xe8,0x706300);
 // Parked trains use the game's derailed-car physics, with no track/consist linkage.
 bytes({0x66,0x81,0x88,0xb8,5,0,0,0,1,0x81,0x60,0x40});word(~0x20000u);
 jump(0xe9,0x44afb8);return b;
}
