// Per-vehicle raster generation on the game thread. Never rename shared textures.
static constexpr DWORD plateOffset=0x3200;
static bool plateEngineMatched(){
 const BYTE create[]={0x55,0x8b,0xec,0x56,0x68,0x04,0x06,0,0};
 const BYTE destroy[]={0x56,0x8b,0x74,0x24,8,0x8b,0x56,0x54};
 BYTE actual[9]={};
 return read(va(0x732c10),actual,sizeof create)&&!memcmp(actual,create,sizeof create)&&
  read(va(0x828060),actual,sizeof destroy)&&!memcmp(actual,destroy,sizeof destroy);
}
static bool plateVehicle(DWORD vehicle,DWORD& ped,DWORD& model,DWORD& texture,DWORD& type){
 ped=u32(va(0xc100d0));DWORD flags=0;WORD id=0;
 if(!privateWritable(ped,0x790)||!read(ped+0x46c,&flags,4)||!(flags&0x100)||
    u32(ped+0x58c)!=vehicle||!privateWritable(vehicle,0x58c)||!read(vehicle+0x22,&id,2)||id<400||id>611)return false;
 model=id;DWORD info=u32(va(0xb12818)+model*4);
 if(!(garageData(info,0x4c)||privateWritable(info,0x4c))||!u32(info+0x24))return false;
 texture=u32(vehicle+0x588);DWORD refs=0;
 if(!privateWritable(texture,0x58)||!read(texture+0x54,&refs,4)||!refs||refs>0x7fffffff)return false;
 type=u8(info+0x31);return type<=2||type==255;
}
extern "C" DWORD __stdcall SteamPlateSet(DWORD vehicle,const wchar_t* text,DWORD length){
 if(!game||!base)return 1;if(!text||!length||length>8)return 3;
 char caption[9]={};bool any=false;
 for(DWORD i=0;i<length;i++){
  wchar_t c=text[i];if(c>=L'a'&&c<=L'z')c-=32;
  if(c!=L' '&&!(c>=L'A'&&c<=L'Z')&&!(c>=L'0'&&c<=L'9'))return 3;
  caption[i]=(char)c;any|=c!=' ';
 }
 if(!any)return 3;
 for(DWORD i=length;i<8;i++)caption[i]=' ';
 DWORD ped=0,model=0,texture=0,type=0;
 if(!plateVehicle(vehicle,ped,model,texture,type)||!plateEngineMatched())return 3;
 char current[9]={};if(!read(texture+16,current,8))return 4;
 if(!memcmp(current,caption,8))return 0;
 if(!installQueue())return lastError?lastError:4;
 DWORD state=0;if(!read(remote+plateOffset,&state,4))return 4;if(state)return 8;
 DWORD request[10]={1,vehicle,ped,model,texture,8,0,0,0,type};memcpy((BYTE*)request+24,caption,9);
 if(!write(remote+plateOffset,request,sizeof request))return 4;
 state=2;return write(remote+plateOffset,&state,4)?0:4;
}
extern "C" DWORD __stdcall SteamPlateStatus(){
 if(!game||!base)return 1;if(!remote)return 0;DWORD packet[6]={};
 if(!read(remote+plateOffset,packet,sizeof packet))return 4;
 return packet[0]&&packet[0]!=4?8:packet[5];
}
static void appendPlateCode(std::vector<BYTE>& b,DWORD allocation,DWORD start){
 const DWORD at=allocation+plateOffset;
 auto bytes=[&](std::initializer_list<BYTE> v){b.insert(b.end(),v);};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)b.push_back(BYTE(v>>(i*8)));};
 auto branch=[&](BYTE condition){bytes({0x0f,condition});SIZE_T p=b.size();word(0);return p;};
 auto patch=[&](SIZE_T p){DWORD d=(DWORD)(b.size()-p-4);memcpy(b.data()+p,&d,4);};
 auto state=[&](DWORD value){bytes({0xc7,0x05});word(at);word(value);};
 auto result=[&](DWORD value){bytes({0xc7,0x05});word(at+20);word(value);};
 auto call=[&](DWORD target){bytes({0xe8});word(target-(start+(DWORD)b.size()+4));};
 // Cancellation is acknowledged by the consumer, never reused mid-call.
 bytes({0x83,0x3d});word(at);bytes({4});auto notCancelled=branch(0x85);
 result(0);state(0);patch(notCancelled);
 bytes({0x83,0x3d});word(at);bytes({2});auto done=branch(0x85);state(3);
 result(7);std::vector<SIZE_T> stale;
 bytes({0xa1});word(va(0xc100d0));bytes({0x3b,0x05});word(at+8);stale.push_back(branch(0x85));
 bytes({0xf7,0x80,0x6c,4,0,0});word(0x100);stale.push_back(branch(0x84));
 bytes({0x8b,0xb0,0x8c,5,0,0,0x3b,0x35});word(at+4);stale.push_back(branch(0x85));
 bytes({0x0f,0xb7,0x46,0x22,0x3b,0x05});word(at+12);stale.push_back(branch(0x85));
 bytes({0x8b,0x86,0x88,5,0,0,0x3b,0x05});word(at+16);stale.push_back(branch(0x85));
 bytes({0xff,0x35});word(at+36);bytes({0x68});word(at+24);call(va(0x732c10));bytes({0x83,0xc4,8,0x85,0xc0});
 auto created=branch(0x85);result(4);auto failed=branch(0x84);patch(created);bytes({0x8b,0xf8});
 // Retain the old texture until the new raster exists and the owner still matches.
 bytes({0x83,0x3d});word(at);bytes({3});auto discard=branch(0x85);
 std::vector<SIZE_T> changed;
 bytes({0xa1});word(va(0xc100d0));bytes({0x3b,0x05});word(at+8);changed.push_back(branch(0x85));
 bytes({0xf7,0x80,0x6c,4,0,0});word(0x100);changed.push_back(branch(0x84));
 bytes({0x8b,0xb0,0x8c,5,0,0,0x3b,0x35});word(at+4);changed.push_back(branch(0x85));
 bytes({0x0f,0xb7,0x46,0x22,0x3b,0x05});word(at+12);changed.push_back(branch(0x85));
 bytes({0x8b,0x86,0x88,5,0,0,0x3b,0x05});word(at+16);changed.push_back(branch(0x85));
 bytes({0x89,0xbe,0x88,5,0,0,0xff,0x35});word(at+16);call(va(0x828060));bytes({0x83,0xc4,4});result(0);
 bytes({0xe9});SIZE_T committed=b.size();word(0);
 patch(discard);for(auto p:changed)patch(p);bytes({0x57});call(va(0x828060));bytes({0x83,0xc4,4});result(7);
 for(auto p:stale)patch(p);patch(failed);patch(committed);state(0);patch(done);
}
