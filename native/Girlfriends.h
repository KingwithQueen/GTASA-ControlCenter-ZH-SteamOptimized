#include "GirlfriendSignatures.h"
static constexpr DWORD gfOffset=0x2800,gfStride=56;
static constexpr DWORD gfGlobals[6]={0x59c,0x5a0,0x5a4,0x5ac,0x5a8,0x5b0};
static constexpr DWORD gfStats[6]={252,253,254,256,255,257};
static bool girlfriendSupported(){
 if(!game||!base)return false;
 for(int i=0;i<6;i++){BYTE actual[32]={};if(!read(va(0xac12d0)+gfCodePositions[i],actual,32)||memcmp(actual,gfScriptSignatures[i],32))return false;}
 return u32(va(0xc100d0))!=0;
}
extern "C" DWORD __stdcall SteamGirlfriendSupported(){return girlfriendSupported()?1:0;}
// mode 0 disables locking, 1 sets and locks, 2 sets without locking.
extern "C" DWORD __stdcall SteamGirlfriendSet(DWORD index,DWORD value,DWORD mode){
 if(!game||!base)return 1;if(index>5||value>100||mode>2)return 3;
 DWORD zero=0,off=gfOffset+index*gfStride;
 if(mode==0)return !remote||write(remote+off,&zero,4)?0:4;
 if(!girlfriendSupported())return 3;if(!featurePause())return 2;
 if(mode==1&&!installQueue())return lastError?lastError:4;
 if(remote&&!write(remote+off,&zero,4))return 4;
 DWORD progress=va(0xac12d0)+gfGlobals[index],stat=va(0xc0c048)+gfStats[index]*4;
 DWORD old[2]={},check[2]={};
 if(!read(progress,&old[0],4)||!read(stat,&old[1],4))return 4;
 bool ok=write(progress,&value,4)&&write(stat,&value,4);
 if(!read(progress,&check[0],4)||!read(stat,&check[1],4))return 6;
 if(!ok||check[0]!=value||check[1]!=value){
  if(!girlfriendSupported()||!featurePause()||(check[0]!=old[0]&&check[0]!=value)||(check[1]!=old[1]&&check[1]!=value))return 6;
  if(!write(progress,old,4)||!write(stat,old+1,4)||!read(progress,&check[0],4)||!read(stat,&check[1],4)||memcmp(check,old,8))return 6;return 5;
 }
 if(mode==1){
  DWORD record[14]={0,value,u32(va(0xc100d0)),progress,stat,va(0xac12d0)+gfCodePositions[index]};
  memcpy(record+6,gfScriptSignatures[index],32);DWORD armed=1;
  if(!write(remote+off,record,sizeof record)||!write(remote+off,&armed,4))return 4;
 }
 return 0;
}
static void appendGirlfriendCode(std::vector<BYTE>& b,DWORD allocation,DWORD start){
 auto bytes=[&](std::initializer_list<BYTE> v){b.insert(b.end(),v);};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)b.push_back(BYTE(v>>(8*i)));};
 for(DWORD i=0;i<6;i++){
  DWORD record=allocation+gfOffset+i*gfStride;std::vector<SIZE_T> skips;
  auto skip=[&](){bytes({0x0f,0x85});skips.push_back(b.size());word(0);};
  bytes({0xbe});word(record);bytes({0x83,0x3e,1});skip();
  bytes({0xa1});word(va(0xc100d0));bytes({0x3b,0x46,8});skip();
  bytes({0x8b,0x7e,0x14,0x56,0x8d,0x76,0x18,0xb9});word(8);bytes({0xfc,0xf3,0xa7,0x5e});skip();
  bytes({0x8b,0x46,4,0x8b,0x7e,0x0c,0x89,7,0x8b,0x7e,0x10,0x89,7});
  for(SIZE_T at:skips){DWORD offset=(DWORD)(b.size()-at-4);memcpy(b.data()+at,&offset,4);}
 }
 (void)start;
}
