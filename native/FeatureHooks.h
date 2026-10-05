// Independently matched classic Steam damage stores and onscreen timer.
// Hooks remain allocated until game exit; disabling changes data only.
static constexpr DWORD featureFlags=96;
static constexpr DWORD featureSites[3]={0x4bae4b,0x4bae97,0x4504e3};
static constexpr DWORD featureCode[3]={0x3000,0x3080,0x3100};
static std::vector<BYTE> featureStub(DWORD allocation,int slot){
 std::vector<BYTE> b;DWORD start=allocation+featureCode[slot];
 auto bytes=[&](std::initializer_list<BYTE> v){b.insert(b.end(),v);};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)b.push_back(BYTE(v>>(i*8)));};
 if(slot<2){
  bytes({0x9c,0x83,0x3d});word(allocation+featureFlags);bytes({0,0x74,0x1a,0x3b,0x35});word(va(0xc100d0));
  bytes({0x74,0x12,0xdd,0xd8,0xd9,0xee,0xc6,0x47,8,1,0x50,0x8b,0x86,0x40,0x05,0,0,0x89,7,0x58});
  // EDI is CPedDamageResponse: mark zero health and report the full original
  // health damage, so the game's death-event handling agrees with the store.
  bytes({0x9d,0xd9,0x96,0x40,0x05,0,0});
 }else{
  bytes({0x83,0x3d});word(allocation+featureFlags+4);bytes({0,0x75,0x07,0x80,0xb9,0x51,0x01,0,0,0});
 }
 bytes({0xe9});word(va(featureSites[slot]+(slot<2?6:7))-(start+(DWORD)b.size()+4));return b;
}
static std::vector<BYTE> featureJump(DWORD allocation,int slot){
 std::vector<BYTE> b(slot<2?6:7,0x90);b[0]=0xe9;
 DWORD offset=allocation+featureCode[slot]-va(featureSites[slot]+5);memcpy(b.data()+1,&offset,4);return b;
}
static bool featureOriginal(int slot,const BYTE* actual){
 const BYTE health[]={0xd9,0x96,0x40,0x05,0,0};
 const BYTE timer[]={0x80,0xb9,0x51,0x01,0,0,0};
 return !memcmp(actual,slot<2?health:timer,slot<2?6:7);
}
static bool featurePause(){return u8(va(0xc0fd39))||u8(va(0xc0fd3a))||u8(va(0xc33994));}
static bool installFeature(int id){
 if(!installQueue())return false;int first=id?2:0,last=id?2:1;
 // Validate every site before changing either damage path.
 for(int slot=first;slot<=last;slot++){
  auto jump=featureJump(remote,slot),code=featureStub(remote,slot);BYTE actual[7]={};
  if(!read(va(featureSites[slot]),actual,jump.size()))return false;
  if(!memcmp(actual,jump.data(),jump.size())){
   std::vector<BYTE> check(code.size());if(!read(remote+featureCode[slot],check.data(),check.size())||check!=code){lastError=3;return false;}
  }else if(!featureOriginal(slot,actual)||!featurePause()){lastError=featureOriginal(slot,actual)?2:3;return false;}
 }
 for(int slot=first;slot<=last;slot++){
  auto jump=featureJump(remote,slot);BYTE actual[7]={};if(!read(va(featureSites[slot]),actual,jump.size()))return false;
  if(!memcmp(actual,jump.data(),jump.size()))continue;
  auto code=featureStub(remote,slot);if(!write(remote+featureCode[slot],code.data(),code.size()))return false;
  FlushInstructionCache(game,(void*)(remote+featureCode[slot]),code.size());
  DWORD previous=0,unused=0;if(!VirtualProtectEx(game,(void*)va(featureSites[slot]),jump.size(),PAGE_EXECUTE_READWRITE,&previous))return false;
  bool ok=write(va(featureSites[slot]),jump.data(),jump.size());
  bool restored=VirtualProtectEx(game,(void*)va(featureSites[slot]),jump.size(),previous,&unused)!=FALSE;
  FlushInstructionCache(game,(void*)va(featureSites[slot]),jump.size());
  BYTE check[7]={};if(!ok||!restored||!read(va(featureSites[slot]),check,jump.size())||memcmp(check,jump.data(),jump.size()))return false;
 }
 return true;
}
extern "C" DWORD __stdcall SteamFeatureSet(DWORD id,DWORD value){
 if(!game||!base)return 1;if(id>1||value>1)return 3;lastError=0;
 if(value&&!installFeature((int)id))return lastError?lastError:4;
 if(!remote)return 0;return write(remote+featureFlags+id*4,&value,4)?0:4;
}
extern "C" DWORD __stdcall SteamFeatureGet(DWORD id){
 if(!game||!base||id>1||!remote)return 0;DWORD v=0;return read(remote+featureFlags+id*4,&v,4)&&v==1?1:0;
}
