// Compiled ONLY into the private offline UI fixture, never the release DLL.
#ifdef OFFLINE_UI_FIXTURE
extern "C" DWORD __stdcall SteamFixtureCoffeeSupported(DWORD supported){
 if(supported>1)return 0;coffeeScriptsVerified=supported!=0;return 1;
}
static DWORD fixturePlateTexture=0;
static DWORD __cdecl fixtureCreatePlate(const char* caption,DWORD){
 memcpy((void*)(fixturePlateTexture+16),caption,9);*(DWORD*)(fixturePlateTexture+0x54)=1;
 return fixturePlateTexture;
}
static void __cdecl fixtureDestroyPlate(DWORD texture){--*(DWORD*)(texture+0x54);}
extern "C" DWORD __stdcall SteamFixtureTrain(){
 DWORD allocation=(DWORD)VirtualAlloc(nullptr,trainAllocationSize,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!allocation)return 0;auto code=trainRestoreCode(base,allocation+trainBranchOffset);
 memcpy((void*)allocation,trainRestoreTag,sizeof trainRestoreTag);
 memcpy((void*)(allocation+trainBranchOffset),code.data(),code.size());DWORD old=0;
 if(!VirtualProtect((void*)allocation,trainAllocationSize,PAGE_EXECUTE_READ,&old))return 0;
 *(DWORD*)va(0x44b1f0)=allocation+trainBranchOffset;return 1;
}
extern "C" DWORD __stdcall SteamFixtureVehicle(DWORD enable){
 DWORD ped=u32(va(0xc100d0));if(!enable){*(DWORD*)(ped+0x46c)=0;*(DWORD*)(ped+0x58c)=0;return 1;}
 DWORD car=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD trailer=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD matrices=(DWORD)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD textures=(DWORD)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!car||!trailer||!matrices||!textures)return 0;
 *(DWORD*)(ped+0x46c)=0x100;*(DWORD*)(ped+0x58c)=car;*(WORD*)(car+0x22)=411;
 *(WORD*)(trailer+0x22)=435;*(DWORD*)(car+0x4c8)=trailer;
 *(DWORD*)(car+20)=matrices;*(DWORD*)(trailer+20)=matrices+128;
 for(int n=0;n<2;n++){float* matrix=(float*)(matrices+n*128);matrix[0]=matrix[5]=matrix[10]=1;matrix[12]=matrix[13]=50;}
 *(DWORD*)(car+0x588)=textures;*(DWORD*)(textures+0x54)=2;fixturePlateTexture=textures+256;
 DWORD info=u32(va(0xb12818)+411*4);*(DWORD*)(info+0x24)=1;*(BYTE*)(info+0x31)=255;
 BYTE create[]={0x55,0x8b,0xec,0x56,0x68,4,6,0,0,0x83,0xc4,4,0x5e,0x5d,0xe9,0,0,0,0};
 DWORD d=(DWORD)&fixtureCreatePlate-va(0x732c10+sizeof create);memcpy(create+15,&d,4);memcpy((void*)va(0x732c10),create,sizeof create);
 BYTE destroy[]={0x56,0x8b,0x74,0x24,8,0x8b,0x56,0x54,0x5e,0xe9,0,0,0,0};
 d=(DWORD)&fixtureDestroyPlate-va(0x828060+sizeof destroy);memcpy(destroy+10,&d,4);memcpy((void*)va(0x828060),destroy,sizeof destroy);
 return car;
}

#ifdef WIN32_UI_BACKEND
struct FixtureFloatTriplet {
 int stat;
 float values[3];
};

static constexpr FixtureFloatTriplet fixtureNumericFloats[]={
 {25,{24.125f,75.25f,26.5f}},
 {64,{63.75f,124.5f,65.875f}},
};
static constexpr DWORD fixtureNumericInts[]={0x0f11662fu,17,0x1ac29119u};
static constexpr DWORD fixtureWantedBefore=0x12347ca1,fixtureWantedAfter=0x681254bb;
static constexpr BYTE fixtureBeating=0x9b,fixtureWantedFlags=0x80,fixtureWantedPadding=0x5e;
static DWORD fixtureWanted;

static bool seedFixturePlayerStats(DWORD ped)
{
 void* wanted=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!wanted)
  return false;
 const DWORD playerData=va(0xc100d4);
 fixtureWanted=(DWORD)wanted;
 *(DWORD*)(ped+0x480)=playerData;
 *(DWORD*)playerData=fixtureWanted;
 *(DWORD*)(fixtureWanted+0x28)=fixtureWantedBefore;
 *(DWORD*)(fixtureWanted+0x30)=fixtureWantedAfter;
 *(BYTE*)(fixtureWanted+0x1b)=fixtureBeating;
 *(BYTE*)(fixtureWanted+0x1e)=fixtureWantedFlags;
 *(BYTE*)(fixtureWanted+0x1f)=fixtureWantedPadding;

 for(const auto& slot:fixtureNumericFloats)
  memcpy((void*)va(0xc0c5a8+(slot.stat-1)*sizeof(float)),slot.values,sizeof slot.values);
 memcpy((void*)va(0xc0c048+(152-1)*sizeof(DWORD)),fixtureNumericInts,sizeof fixtureNumericInts);
 return true;
}

// Fixed fixture-only assertion for these seeded stats and their neighbors.
extern "C" __declspec(dllexport) DWORD __stdcall SteamFixturePlayerStatsIntact(DWORD initial,DWORD level)
{
 static constexpr struct {
  DWORD chaos;
  BYTE cops,cars;
  WORD roadblock;
 } pursuit[]={
  {0,0,0,0},{70,1,1,0},{200,3,2,0},{570,4,2,12},{1220,6,2,18},{2420,8,3,24},{4620,10,3,30}
 };
 if(!base||!fixtureWanted||initial>1||level>6)
  return 0;
 for(const auto& slot:fixtureNumericFloats){
  const auto* values=(const float*)va(0xc0c5a8+(slot.stat-1)*sizeof(float));
  if(values[0]!=slot.values[0]||values[2]!=slot.values[2]||(initial&&values[1]!=slot.values[1]))
   return 0;
 }
 const auto* counts=(const DWORD*)va(0xc0c048+(152-1)*sizeof(DWORD));
 const auto* wanted=(const DWORD*)fixtureWanted;
 const auto* bytes=(const BYTE*)fixtureWanted;
 WORD roadblock=0;
 memcpy(&roadblock,bytes+0x1c,sizeof roadblock);
 return counts[0]==fixtureNumericInts[0]&&counts[2]==fixtureNumericInts[2]
     &&(!initial||counts[1]==fixtureNumericInts[1])
     &&wanted[0]==pursuit[level].chaos&&bytes[0x19]==pursuit[level].cops
     &&bytes[0x1a]==pursuit[level].cars&&roadblock==pursuit[level].roadblock
     &&bytes[0x1b]==fixtureBeating&&bytes[0x1e]==fixtureWantedFlags&&bytes[0x1f]==fixtureWantedPadding
     &&wanted[0x28/sizeof(DWORD)]==fixtureWantedBefore&&wanted[0x2c/sizeof(DWORD)]==level
     &&wanted[0x30/sizeof(DWORD)]==fixtureWantedAfter;
}
#endif

extern "C" DWORD __stdcall SteamFixtureBegin(){
 game=GetCurrentProcess();gamePid=GetCurrentProcessId();remote=0;enabled=false;
 base=(DWORD)VirtualAlloc(nullptr,0xa00000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
 DWORD ped=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD info=(DWORD)VirtualAlloc(nullptr,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!base||!ped||!info)return 0;
 *(DWORD*)va(0xc100d0)=ped;*(float*)(ped+0x540)=100;*(DWORD*)(ped+0x530)=1;
#ifdef WIN32_UI_BACKEND
 if(!seedFixturePlayerStats(ped))return 0;
#endif
 DWORD matrix=(DWORD)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!matrix)return 0;
 *(DWORD*)(ped+20)=matrix;*(float*)matrix=*(float*)(matrix+20)=*(float*)(matrix+40)=1;
 for(int i=0;i<11;i++)*(DWORD*)(va(0x44b1dc)+i*4)=va(trainRestoreTable[i]);
 memcpy((void*)va(0x706300),trainCtorSignature,sizeof trainCtorSignature);
 *(BYTE*)va(0x6f8a10)=*(BYTE*)va(0x6fdb00)=0x55;
 *(BYTE*)va(0xc0fd39)=1;*(DWORD*)va(0x9e3ab4)=17;
 for(DWORD m=400;m<=611;m++){DWORD mi=info+(m-400)*128;*(DWORD*)(va(0xb12818)+m*4)=mi;*(DWORD*)(mi+60)=0;*(WORD*)(mi+74)=0;}
 *(DWORD*)(info+(522-400)*128+60)=9;*(DWORD*)va(0xcaca24)=0;*(DWORD*)va(0xcaca24+0xd0)=0x1200;
 for(int m:{449,537,538,569,570,590})*(DWORD*)(info+(m-400)*128+60)=6;
 *(DWORD*)(info+(487-400)*128+60)=3;*(DWORD*)(info+(520-400)*128+60)=4;
 for(DWORD i=0;i<17;i++){
  BYTE* h=(BYTE*)va(0x9e3ac0)+i*0xd8;
  *(float*)h=50;*(float*)(h+4)=50;*(float*)(h+12)=1;*(float*)(h+24)=1;
  *(float*)(h+28)=10;*(float*)(h+32)=10;*(float*)(h+36)=10;h[76]=h[79]=(BYTE)garageTypes[i];
  BYTE* cars=(BYTE*)va(0x9e2668)+garageRows[i]*256;
  for(int s=0;s<4;s++){BYTE* c=cars+s*64;*(WORD*)(c+18)=411;memset(c+20,255,30);c[50]=c[51]=1;c[58]=255;c[61]=100;c[63]=0xb2;}
 }
 const BYTE call[]={0xe8,0x20,0x02,0xef,0xff};memcpy((void*)va(0x54bd2b),call,5);*(BYTE*)va(0x43bf50)=0xc3;
 for(int slot=0;slot<3;slot++){
  const BYTE health[]={0xd9,0x96,0x40,0x05,0,0};const BYTE timer[]={0x80,0xb9,0x51,0x01,0,0,0};
  memcpy((void*)va(featureSites[slot]),slot<2?health:timer,slot<2?6:7);
 }
 for(int i=0;i<6;i++)memcpy((void*)(va(0xac12d0)+gfCodePositions[i]),gfScriptSignatures[i],32);
 coffeeScriptsVerified=true;for(int i=0;i<2;i++)memcpy((void*)(va(0xac12d0)+coffeePositions[i]),coffeeSignatures[i],12);
 *(DWORD*)va(0xac25dc)=1;
 BYTE engine[10]={0x55,0x8b,0xec,0x80,0x3d};DWORD global=va(0xc35fe8);memcpy(engine+5,&global,4);memcpy((void*)va(0x594cb0),engine,10);
 // The fixture engine returns without drawing; the exact release queue/ABI runs.
 const BYTE finish[]={0x5d,0xc3};memcpy((void*)va(0x594cba),finish,2);
 return (DWORD)game;
}
extern "C" DWORD __stdcall SteamFixtureRun(){if(!remote)return 0;((void(__cdecl*)())(remote+0x100))();return 1;}
extern "C" DWORD __stdcall SteamFixtureDisturbGarage(){if(!base)return 0;memset((void*)va(0x9e2668),0,256);return 1;}
#endif
