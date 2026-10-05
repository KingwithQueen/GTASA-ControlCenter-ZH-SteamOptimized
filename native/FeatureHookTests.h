static void featureHookTests(){
 DWORD ped=(DWORD)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 DWORD savedPed=u32(va(0xc100d0));assert(ped);*(DWORD*)va(0xc100d0)=ped;
 for(int slot=0;slot<3;slot++){
  const BYTE health[]={0xd9,0x96,0x40,0x05,0,0};const BYTE timer[]={0x80,0xb9,0x51,0x01,0,0,0};
  memcpy((void*)va(featureSites[slot]),slot<2?health:timer,slot<2?6:7);
 }
 *(BYTE*)va(0xc0fd39)=0;*(BYTE*)va(0xc0fd3a)=0;*(BYTE*)va(0xc33994)=0;
 assert(SteamFeatureSet(0,1)==2&&SteamFeatureGet(0)==0);
 *(BYTE*)va(0xc0fd39)=1;*(BYTE*)va(featureSites[1])=0xcc;
 assert(SteamFeatureSet(0,1)==3&&*(BYTE*)va(featureSites[0])==0xd9);
 *(BYTE*)va(featureSites[1])=0xd9;assert(SteamFeatureSet(0,1)==0&&SteamFeatureGet(0)==1);
 for(int slot=0;slot<2;slot++){
  *(BYTE*)va(featureSites[slot]+6)=0xc3;auto run=(void(__cdecl*)())va(featureSites[slot]);
  DWORD flagsBefore=0,flagsAfter=0;float health=35,result=0;double sentinel=42.125,unchanged=0;
  DWORD target=ped+0x800;
  BYTE response[12]={};DWORD responseAddress=(DWORD)response;*(float*)(target+0x540)=100;
  __asm {
   push edi
   push esi
   mov edi,responseAddress
   mov esi,target
   fld qword ptr sentinel
   fld dword ptr health
   cmp esi,esi
   pushfd
   pop flagsBefore
   call run
   pushfd
   pop flagsAfter
   fstp dword ptr result
   fstp qword ptr unchanged
   pop esi
   pop edi
  }
  assert(result==0&&*(float*)(target+0x540)==0&&unchanged==sentinel&&(flagsBefore&0x8d5)==(flagsAfter&0x8d5));
  assert(response[8]==1&&garageFloat(response)==100);
  __asm {
   push esi
   mov esi,ped
   fld dword ptr health
   call run
   fstp dword ptr result
   pop esi
  }
  assert(result==health&&*(float*)(ped+0x540)==health);
  assert(SteamFeatureSet(0,0)==0);
  __asm {
   push esi
   mov esi,target
   fld dword ptr health
   call run
   fstp dword ptr result
   pop esi
  }
  assert(result==health&&*(float*)(target+0x540)==health);assert(SteamFeatureSet(0,1)==0);
 }
 assert(SteamFeatureSet(1,1)==0);BYTE timerState[0x154]={};BYTE pauseBefore=timerState[0x151];
 // Return the zero flag produced by the pause check, as the game's JNE does.
 const BYTE returnZF[]={0x0f,0x94,0xc0,0x0f,0xb6,0xc0,0xc3};memcpy((void*)va(featureSites[2]+7),returnZF,sizeof returnZF);
 auto check=(DWORD(__thiscall*)(void*))va(featureSites[2]);
 assert(check(timerState)==0&&timerState[0x151]==pauseBefore);
 assert(SteamFeatureSet(1,0)==0&&check(timerState)==1);
 timerState[0x151]=1;assert(check(timerState)==0&&timerState[0x151]==1);
 assert(SteamFeatureSet(2,1)==3&&SteamFeatureSet(0,2)==3);
 *(DWORD*)va(0xc100d0)=savedPed;VirtualFree((void*)ped,0,MEM_RELEASE);
 puts("FEATURE_HOOKS=PASS actual x86 damage paths, player immunity, flags/x87 preservation, frozen/thawed timer checks, original pause preserved and mismatched code rejected");
}
