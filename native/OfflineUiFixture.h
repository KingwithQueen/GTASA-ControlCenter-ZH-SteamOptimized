// Compiled ONLY into the private offline UI fixture, never the release DLL.
#ifdef OFFLINE_UI_FIXTURE
extern "C" DWORD __stdcall SteamFixtureBegin(){
 game=GetCurrentProcess();gamePid=GetCurrentProcessId();remote=0;enabled=false;
 base=(DWORD)VirtualAlloc(nullptr,0xa00000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
 DWORD ped=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD info=(DWORD)VirtualAlloc(nullptr,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);if(!base||!ped||!info)return 0;
 *(DWORD*)va(0xc100d0)=ped;*(float*)(ped+0x540)=100;*(DWORD*)(ped+0x530)=1;
 *(BYTE*)va(0xc0fd39)=1;*(DWORD*)va(0x9e3ab4)=17;
 for(DWORD m=400;m<=611;m++){DWORD mi=info+(m-400)*128;*(DWORD*)(va(0xb12818)+m*4)=mi;*(DWORD*)(mi+60)=1;*(WORD*)(mi+74)=0;}
 *(DWORD*)(info+(522-400)*128+60)=9;*(DWORD*)va(0xcaca24)=0;*(DWORD*)va(0xcaca24+0xd0)=0x1200;
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
