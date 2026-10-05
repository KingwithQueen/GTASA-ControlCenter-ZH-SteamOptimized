// Explicit stored-car API. Legacy global addresses remain rejected.
static constexpr DWORD garageTypes[17]={16,31,17,32,29,27,28,30,18,26,24,25,45,39,40,41,42};
static constexpr DWORD garageRows[17]={0,10,1,11,8,6,7,9,2,5,3,4,19,15,16,17,18};
static constexpr DWORD remoteSize=32768,lockOffset=0x1000,lockStride=324;
static bool installQueue();
static float garageFloat(const BYTE* p){float v=0;memcpy(&v,p,4);return v;}
static WORD garageWord(const BYTE* p){WORD v=0;memcpy(&v,p,2);return v;}
static bool privateWritable(DWORD a,DWORD n){
 MEMORY_BASIC_INFORMATION m{};if(!a||a>0xffffffff-n||!VirtualQueryEx(game,(void*)a,&m,sizeof m))return false;
 DWORD p=m.Protect&255;return m.State==MEM_COMMIT&&m.Type==MEM_PRIVATE&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
  a+n<=(uintptr_t)m.BaseAddress+m.RegionSize&&(p==PAGE_READWRITE||p==PAGE_EXECUTE_READWRITE);
}
static bool garageData(DWORD a,DWORD n){
 MEMORY_BASIC_INFORMATION m{};if(!a||a>0xffffffff-n||!VirtualQueryEx(game,(void*)a,&m,sizeof m))return false;
 DWORD p=m.Protect&255;
 return m.State==MEM_COMMIT&&m.AllocationBase==(void*)base&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
  a+n<=(uintptr_t)m.BaseAddress+m.RegionSize&&
#if !defined(UNIT_TEST) && !defined(OFFLINE_UI_FIXTURE)
  m.Type==MEM_IMAGE&&
#endif
  (p==PAGE_READWRITE||p==PAGE_EXECUTE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_WRITECOPY);
}
// Metadata is 24 DWORDs: index,garage address,storage address,ped,80-byte header.
static DWORD garageInspect(DWORD index,DWORD* meta,BYTE* cars){
 if(!game||!base)return 1;if(index>=17)return 3;
 DWORD count=0;if(!read(va(0x9e3ab4),&count,4)||count==0||count>50)return 2;
 DWORD address=0;BYTE header[80]={};
 for(DWORD i=0;i<count;i++){
  BYTE h[80]={};DWORD a=va(0x9e3ac0)+i*0xd8;
  if(!garageData(a,80)||!read(a,h,80))return 4;
  if(h[76]==garageTypes[index]){if(address)return 2;address=a;memcpy(header,h,80);}
 }
 if(!address||header[79]!=garageTypes[index]||header[77]>5)return 2;
 for(int i=0;i<15;i++){float v=garageFloat(header+i*4);if(!std::isfinite(v)||fabsf(v)>20000)return 2;}
 float w=garageFloat(header+32),h=garageFloat(header+36);
 if(w<1||h<1||w>1000||h>1000)return 2;
 DWORD storage=va(0x9e2668)+garageRows[index]*256;
 BYTE check[256]={},second[80]={};DWORD count2=0,ped=u32(va(0xc100d0));
 if(!garageData(storage,256)||!read(storage,cars,256)||!read(storage,check,256)||memcmp(cars,check,256)||
  !read(address,second,80)||memcmp(header,second,80)||!read(va(0x9e3ab4),&count2,4)||count!=count2)return 7;
 meta[0]=index;meta[1]=address;meta[2]=storage;meta[3]=ped;memcpy(meta+4,header,80);return 0;
}
static bool garagePaused(const DWORD* meta){
 const BYTE* h=(const BYTE*)(meta+4);DWORD ped=meta[3];float health=0;DWORD flags=0,state=0,area=99;
 return (u8(va(0xc0fd39))||u8(va(0xc33994)))&&!u8(va(0xc0fd3a))&&h[77]==0&&!(h[78]&2)&&
  fabsf(garageFloat(h+56))<0.001f&&garageWord(h+64)==0&&u32(meta[1]+64)==0&&
  privateWritable(ped,0x79c)&&read(ped+0x540,&health,4)&&std::isfinite(health)&&health>0&&
  read(ped+0x46c,&flags,4)&&!(flags&0x100)&&read(ped+0x530,&state,4)&&state>=1&&state<=46&&
  u8(ped+0x2f)==0&&read(va(0xbffbe4),&area,4)&&area==0&&
  !u32(va(0x9e22a4))&&!u32(va(0x9e22ac))&&!u8(va(0x9dd099))&&!u8(va(0x9dd100));
}
#include "GarageCatalog.h"
static bool garageDefaults(WORD model,DWORD& flags){
 if(model<400||model>611||!garageCatalog[model-400].parkable)return false;
 DWORD info=u32(va(0xb12818)+model*4),kind=0;WORD handling=0;
 if(!(garageData(info,0x4c)||privateWritable(info,0x4c))||!read(info+60,&kind,4)||kind<1||kind>11||(kind>=6&&kind<=8)||
  !read(info+74,&handling,2)||handling>=210)return false;
 DWORD entry=va(0xcaca24)+handling*0xe0;
 if(!garageData(entry,0xe0)||u32(entry)!=handling||!read(entry+0xd0,&flags,4))return false;
 flags&=~0x000a0000u;return true;
}
static bool garageMod(WORD model,WORD mod){
 if(mod==65535)return true;
 const auto& c=garageCatalog[model-400];for(DWORD i=0;i<c.count;i++)if(c.mods[i]==mod)return true;
 return false;
}
static bool garageBuild(const DWORD* meta,const BYTE* before,const BYTE* selected,DWORD repark,BYTE* after){
 if(repark>1)return false;memcpy(after,before,256);DWORD occupied=0,bikes=0,rank=0,bikeRank=0,carRank=0;
 for(int s=0;s<4;s++){WORD model=garageWord(selected+s*64+18);if(model){if(model<400||model>611)return false;occupied++;if(garageCatalog[model-400].bike)bikes++;}}
 for(int s=0;s<4;s++){
  const BYTE* old=before+s*64;const BYTE* input=selected+s*64;BYTE* next=after+s*64;
  WORD model=garageWord(input+18),previous=garageWord(old+18);
  if(!model){WORD zero=0;memcpy(next+18,&zero,2);continue;}
  if(model<400||model>611||(input[50]>126&&input[50]!=old[50])||(input[51]>126&&input[51]!=old[51]))return false;
  DWORD defaults=0;
  bool replaced=model!=previous;
  if(replaced&&!garageDefaults(model,defaults))return false;
  if(replaced){memcpy(next+12,&defaults,4);next[16]&=0x0f;memset(next+20,255,30);
   next[52]=next[53]=0;next[54]=2;next[55]=next[56]=next[58]=255;next[57]=next[59]=0;}
  memcpy(next+18,&model,2);next[16]=(next[16]&0xf0)|(input[16]&15);next[50]=input[50];next[51]=input[51];
  bool hydraulic=false;BYTE nitro=0;
  for(int m=0;m<15;m++){
   WORD mod=garageWord(input+20+m*2),oldMod=garageWord(old+20+m*2);
   if((replaced||mod!=oldMod)&&!garageMod(model,mod))return false;
   memcpy(next+20+m*2,&mod,2);
   hydraulic|=mod==1087; // hydraulics upgrade model, not car handling identity.
   if(mod==1008)nitro=5;if(mod==1009)nitro=2;if(mod==1010)nitro=10;
  }
  if(input[58]!=255&&input[58]>3)return false;next[58]=input[58];
  DWORD handling=0;memcpy(&handling,next+12,4);handling&=~0x000a0000u;
  if(hydraulic)handling|=0x00020000;if(nitro)handling|=0x00080000;
  memcpy(next+12,&handling,4);next[16]=(next[16]&0x3f)|(hydraulic?0x40:0)|(nitro?0x80:0);next[59]=nitro;
  if(repark||!previous){
   const BYTE* h=(const BYTE*)(meta+4);float w=garageFloat(h+32),depth=garageFloat(h+36);
   const auto& c=garageCatalog[model-400];bool across=false;
   float fx=occupied==1?0.5f:(rank%2?0.75f:0.25f),fy=occupied<=2?(c.longCar?1/3.f:0.5f):(rank/2?0.75f:0.25f);
   if(occupied==2&&bikes==1){
    if(w<4.5f){fx=c.bike?0.5f:0.25f;fy=c.bike?0.125f:(c.longCar?0.5f:0.625f);across=c.bike;}
    else{fx=c.bike?6/7.f:0.25f;fy=c.longCar?1/3.f:0.5f;}
   }else if(occupied>=3&&bikes>0&&bikes<occupied){
    if(c.bike){fx=(bikeRank+1.f)/(bikes+1.f);fy=0.125f;across=true;}
    else{fx=(carRank+1.f)/(occupied-bikes+1.f);fy=c.longCar?0.5f:0.625f;}
   }else if(w>depth&&occupied>=3){fx=(rank+1.f)/(occupied+1.f);fy=c.longCar?1/3.f:0.5f;}
   float x=garageFloat(h)+garageFloat(h+12)*w*fx+garageFloat(h+20)*depth*fy;
   float y=garageFloat(h+4)+garageFloat(h+16)*w*fx+garageFloat(h+24)*depth*fy;
   float z=garageFloat(h+8)+c.height/3;memcpy(next,&x,4);memcpy(next+4,&y,4);memcpy(next+8,&z,4);
   float dx=garageFloat(h+(across?12:20)),dy=garageFloat(h+(across?16:24)),len=sqrtf(dx*dx+dy*dy);if(len<0.9f||len>1.1f)return false;
   next[60]=(BYTE)(signed char)lroundf(dx*100/len);next[61]=(BYTE)(signed char)lroundf(dy*100/len);next[62]=0;
  }
  for(int axis=0;axis<3;axis++){float v=garageFloat(next+axis*4);if(!std::isfinite(v)||fabsf(v)>20000)return false;}
  int x=(signed char)next[60],y=(signed char)next[61],z=(signed char)next[62];int sq=x*x+y*y+z*z;
  if(sq<9000||sq>11000)return false;
  rank++;if(garageCatalog[model-400].bike)bikeRank++;else carRank++;
 }
 return true;
}
extern "C" DWORD __stdcall SteamGarageInspect(DWORD index,DWORD* metadata,BYTE* cars){
 if(!metadata||!cars)return 3;return garageInspect(index,metadata,cars);
}
#ifdef UNIT_TEST
static DWORD garageFault=0;
#endif
extern "C" DWORD __stdcall SteamGarageWrite(DWORD index,const DWORD* expected,const BYTE* before,const BYTE* selected,DWORD repark){
 if(!expected||!before||!selected)return 3;DWORD meta[24]={};BYTE old[256]={},after[256]={},check[256]={};
 DWORD status=garageInspect(index,meta,old);if(status)return status;if(!garagePaused(meta))return 2;
 if(memcmp(meta,expected,sizeof meta)||memcmp(old,before,256))return 7;
 if(!garageBuild(meta,before,selected,repark,after))return 3;
 if(!memcmp(before,after,256))return 0;
 status=garageInspect(index,meta,old);if(status||!garagePaused(meta)||memcmp(meta,expected,sizeof meta)||memcmp(old,before,256))return 7;
 bool ok=false;
#ifdef UNIT_TEST
 if(garageFault==1)memcpy((void*)meta[2],after,53);
 else if(garageFault==2){memcpy((void*)meta[2],after,256);((BYTE*)meta[2])[63]^=128;}
 else
#endif
 ok=write(meta[2],after,256);
 status=garageInspect(index,meta,check);
 if(ok&&!status&&garagePaused(meta)&&!memcmp(meta,expected,sizeof meta)&&!memcmp(check,after,256))return 0;
 if(status||!garagePaused(meta)||memcmp(meta,expected,sizeof meta))return 6;
 for(int i=0;i<256;i++)if(check[i]!=before[i]&&check[i]!=after[i])return 6;
 if(!write(meta[2],before,256)||garageInspect(index,meta,check)||memcmp(check,before,256))return 6;return 5;
}
extern "C" DWORD __stdcall SteamGarageLock(DWORD index,DWORD armed,const BYTE* selected){
 if(index>=17||armed>1)return 3;if(!game||!base)return 1;
 DWORD off=lockOffset+index*lockStride,zero=0;
 if(!armed){return !remote||write(remote+off,&zero,4)?0:4;}
 DWORD meta[24]={};BYTE before[256]={},after[256]={};DWORD status=garageInspect(index,meta,before);
 if(status)return status;if(!selected||!garagePaused(meta))return 2;
 if(!garageBuild(meta,before,selected,1,after))return 3;if(!installQueue())return 4;
 BYTE record[lockStride]={};DWORD words[5]={0,meta[1],meta[2],garageTypes[index],meta[3]};
 memcpy(record,words,20);memcpy(record+20,meta+4,48);memcpy(record+68,after,256);
 if(!write(remote+off,&zero,4)||!write(remote+off,record,sizeof record)||!write(remote+off,&armed,4))return 4;return 0;
}
static void appendGarageCode(std::vector<BYTE>& b,DWORD allocation,DWORD start){
 auto bytes=[&](std::initializer_list<BYTE> v){b.insert(b.end(),v);};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)b.push_back(BYTE(v>>(8*i)));};
 std::vector<SIZE_T> skips;auto skip=[&](){bytes({0x0f,0x85});skips.push_back(b.size());word(0);};
 bytes({0xbe});word(allocation+lockOffset);bytes({0xbb});word(17);SIZE_T loop=b.size();
 bytes({0x83,0x3e,0x01});skip();bytes({0x8b,0x46,0x04,0x8b,0x56,0x0c,0x38,0x50,0x4c});skip();
 bytes({0x38,0x50,0x4f});skip();bytes({0x80,0x78,0x4d,0x00});skip();
 bytes({0xf6,0x40,0x4e,0x02});skip();bytes({0x83,0x78,0x40,0x00});skip();bytes({0x83,0x78,0x38,0x00});skip();
 bytes({0x8b,0x15});word(va(0xc100d0));bytes({0x3b,0x56,0x10});skip();
 for(DWORD address:{0x9e22a4u,0x9e22acu}){bytes({0x83,0x3d});word(va(address));bytes({0});skip();}
 for(DWORD address:{0x9dd099u,0x9dd100u}){bytes({0x80,0x3d});word(va(address));bytes({0});skip();}
 bytes({0x56,0x53,0xfc,0x89,0xc7,0x8d,0x76,0x14,0xb9});word(12);
 bytes({0xf3,0xa7,0x5b,0x5e});skip();
 bytes({0x8b,0x7e,0x08,0x56,0x8d,0x76,0x44,0xb9});word(64);bytes({0xf3,0xa5,0x5e});
 SIZE_T end=b.size();for(SIZE_T at:skips){DWORD offset=(DWORD)(end-at-4);memcpy(b.data()+at,&offset,4);}
 bytes({0x81,0xc6});word(lockStride);bytes({0x4b,0x0f,0x85});word(start+(DWORD)loop-(start+(DWORD)b.size()+4));
}
