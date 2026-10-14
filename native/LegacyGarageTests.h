static void legacyGarageTests(){
 DWORD savedPed=u32(va(0xc100d0));DWORD ped=(DWORD)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD info=(DWORD)VirtualAlloc(nullptr,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(ped&&info);
 *(DWORD*)va(0xc100d0)=ped;*(float*)(ped+0x540)=100;*(DWORD*)(ped+0x530)=1;
 *(BYTE*)va(0xc0fd39)=1;*(DWORD*)va(0x9e3ab4)=17;
 for(DWORD m=400;m<=611;m++){DWORD mi=info+(m-400)*128;*(DWORD*)(va(0xb12818)+m*4)=mi;*(DWORD*)(mi+60)=0;*(WORD*)(mi+74)=0;}
 *(DWORD*)va(0xcaca24)=0;*(DWORD*)va(0xcaca24+0xd0)=0x1200;
 memset((void*)va(0x9e2668),0x5a,20*256);
 for(DWORD i=0;i<17;i++){
  BYTE* h=(BYTE*)va(0x9e3ac0)+i*0xd8;memset(h,0,0xd8);
  *(float*)h=50;*(float*)(h+4)=50;*(float*)(h+12)=1;*(float*)(h+24)=1;
  *(float*)(h+28)=10;*(float*)(h+32)=10;*(float*)(h+36)=10;h[76]=h[79]=(BYTE)garageTypes[i];
  BYTE* cars=(BYTE*)va(0x9e2668)+garageRows[i]*256;memset(cars,0,256);
  for(int s=0;s<4;s++){BYTE* c=cars+s*64;*(WORD*)(c+18)=411;memset(c+20,255,30);c[50]=c[51]=1;c[58]=255;c[61]=100;c[63]=0xb2;}
  DWORD meta[24]={};BYTE before[256]={},target[256]={};assert(SteamGarageInspect(i,meta,before)==0);memcpy(target,before,256);
  target[50]=12;target[16]=15;assert(SteamGarageWrite(i,meta,before,target,0)==0);
  assert(cars[50]==12&&cars[16]==15&&cars[63]==0xb2);assert(SteamGarageWrite(i,meta,before,target,0)==7);
 }
 DWORD meta[24]={};BYTE before[256]={},target[256]={};BYTE* cars=(BYTE*)va(0x9e2668);
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);*(WORD*)(target+18)=400;
 assert(SteamGarageWrite(0,meta,before,target,1)==0&&garageWord(cars+18)==400);
 assert(cars[63]==0xb2&&garageFloat(cars)>50&&garageFloat(cars+4)>50);
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);*(WORD*)(target+18)=0;
 assert(SteamGarageWrite(0,meta,before,target,0)==0&&garageWord(cars+18)==0);
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);*(WORD*)(target+18)=400;
 assert(SteamGarageWrite(0,meta,before,target,1)==0&&garageWord(cars+18)==400); // add to empty slot
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);target[50]=126;
 *(BYTE*)va(0xc0fd39)=0;assert(SteamGarageWrite(0,meta,before,target,0)==2);*(BYTE*)va(0xc0fd39)=1;
 target[50]=127;assert(SteamGarageWrite(0,meta,before,target,0)==3);target[50]=126;
 garageFault=1;assert(SteamGarageWrite(0,meta,before,target,0)==5&&!memcmp(cars,before,256));
 garageFault=2;assert(SteamGarageWrite(0,meta,before,target,0)==6);memcpy(cars,before,256);garageFault=0;
 *(WORD*)(target+20)=1199;assert(SteamGarageWrite(0,meta,before,target,0)==3);memcpy(target,before,256);
 assert(SteamGarageLock(0,1,target)==0);BYTE saved[256];memcpy(saved,cars,256);memset(cars,0,256);
 auto run=(void(__cdecl*)())(remote+0x100);run();assert(garageWord(cars+18)==400);
 BYTE* h=(BYTE*)meta[1];h[77]=1;memset(cars,0,256);run();assert(garageWord(cars+18)==0);h[77]=0;
 *(DWORD*)va(0x9e22a4)=1;run();assert(garageWord(cars+18)==0);*(DWORD*)va(0x9e22a4)=0;
 *(DWORD*)va(0xc100d0)=0;run();assert(garageWord(cars+18)==0);*(DWORD*)va(0xc100d0)=ped;
 assert(SteamGarageLock(0,0,nullptr)==0);run();assert(garageWord(cars+18)==0);
 memcpy(cars,saved,256);
 // Slots need not be contiguous; occupied ranks must receive distinct poses.
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);
 *(WORD*)(target+18)=0;*(WORD*)(target+64+18)=411;*(WORD*)(target+128+18)=0;*(WORD*)(target+192+18)=411;
 assert(SteamGarageWrite(0,meta,before,target,1)==0);assert(garageFloat(cars+64)!=garageFloat(cars+192));
 // A bike in a narrow mixed garage goes across the front, independent of slot.
 *(float*)(h+32)=4;assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);
 *(WORD*)(target+64+18)=522;*(DWORD*)(info+(522-400)*128+60)=9;
 assert(SteamGarageWrite(0,meta,before,target,1)==0&&cars[64+60]==100&&cars[64+61]==0);
 assert(garageFloat(cars+64+4)<garageFloat(cars+192+4));
 // Hydraulics and nitro update both stored flags and the actual handling bits.
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);*(WORD*)(target+192+20)=1087;*(WORD*)(target+192+22)=1008;
 assert(SteamGarageWrite(0,meta,before,target,0)==0);assert((cars[192+16]&0xc0)==0xc0&&cars[192+59]==5);
 assert((u32((DWORD)(cars+192+12))&0xa0000)==0xa0000&&cars[192+63]==0xb2);
 for(int i=0;i<11;i++)*(DWORD*)(va(0x44b1dc)+i*4)=va(trainRestoreTable[i]);
 *(BYTE*)va(0x6f8a10)=0x55;*(BYTE*)va(0x6fdb00)=0x55;
 *(DWORD*)(info+(487-400)*128+60)=3;*(DWORD*)(info+(520-400)*128+60)=4;
 *(DWORD*)(info+(537-400)*128+60)=6;
 *(DWORD*)(info+(441-400)*128+60)=0;*(DWORD*)(info+(501-400)*128+60)=7;*(DWORD*)(info+(464-400)*128+60)=8;
 *(DWORD*)(info+(435-400)*128+60)=11;
 *(DWORD*)(info+(539-400)*128+60)=8;
 for(DWORD model:{400u,539u,435u})assert(SteamGarageModelSupported(model)==1);
 for(DWORD model:{441u,501u,464u})assert(SteamGarageModelSupported(model)==0);
 assert(SteamGarageCapabilities()==1&&SteamGarageModelSupported(487)==1&&SteamGarageModelSupported(520)==1&&SteamGarageModelSupported(537)==0);
 for(WORD model:{(WORD)487,(WORD)520}){
  assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);*(WORD*)(target+18)=model;
  memset(target+20,255,30);assert(SteamGarageWrite(0,meta,before,target,1)==0&&garageWord(cars+18)==model);
 }
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);*(WORD*)(target+18)=537;
 assert(SteamGarageWrite(0,meta,before,target,1)==9&&!memcmp(cars,before,256));
 DWORD trainAllocation=seedTrainRestore();assert(SteamGarageCapabilities()==3&&SteamGarageModelSupported(537)==1);
 assert(SteamGarageWrite(0,meta,before,target,1)==0&&garageWord(cars+18)==537);
 assert(SteamGarageInspect(0,meta,before)==0);memcpy(target,before,256);target[50]=20;
 *(DWORD*)va(0x44b1f0)=va(0x44af89);assert(SteamGarageWrite(0,meta,before,target,0)==9&&!memcmp(cars,before,256));
 assert(SteamGarageLock(0,1,target)==9);*(WORD*)(target+18)=0;assert(SteamGarageWrite(0,meta,before,target,0)==0);
 VirtualFree((void*)trainAllocation,0,MEM_RELEASE);
 *(DWORD*)va(0xc100d0)=savedPed;VirtualFree((void*)ped,0,MEM_RELEASE);VirtualFree((void*)info,0,MEM_RELEASE);
 puts("LEGACY_GARAGE=PASS 17 garages, add/replace/clear/proofs/placement, stale snapshots, pause/closed gates, rollback, game-thread locks and unrelated bytes");
}
