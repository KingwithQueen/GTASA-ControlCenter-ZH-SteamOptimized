static constexpr DWORD feedbackOffset=0x2a00,feedbackStride=0x200,feedbackCapacity=400;
static bool feedbackEngineMatched(){
 BYTE actual[10]={},expected[]={0x55,0x8b,0xec,0x80,0x3d,0,0,0,0,0};DWORD address=va(0xc35fe8);memcpy(expected+5,&address,4);
 return read(va(0x594cb0),actual,10)&&!memcmp(actual,expected,10);
}
extern "C" DWORD __stdcall SteamFeedbackEnable(DWORD value){
 if(!game||!base)return 1;if(value>1)return 3;
 if(value){if(!feedbackEngineMatched())return 3;if(!installQueue())return lastError?lastError:4;}
 // Cancellation stays busy until the game thread acknowledges it. Clearing
 // a slot here could let a producer replace text while the HUD still reads it.
 if(!value&&remote){DWORD cancelled=4;for(DWORD i=0;i<2;i++)if(!write(remote+feedbackOffset+i*feedbackStride,&cancelled,4))return 4;}
 return 0;
}
extern "C" DWORD __stdcall SteamFeedbackMessage(const wchar_t* text,DWORD length){
 if(!game||!base)return 1;if(!text||!length||length>180)return 3;
 if(!remote)return 2;char bytes[feedbackCapacity]={};BOOL replaced=FALSE;
 int count=WideCharToMultiByte(936,WC_NO_BEST_FIT_CHARS,text,(int)length,bytes,feedbackCapacity-1,nullptr,&replaced);
 if(count<=0||replaced)return 3;
 for(int i=0;i<count;i++)if((unsigned char)bytes[i]<32)return 3;
 for(DWORD i=0;i<2;i++){
  DWORD address=remote+feedbackOffset+i*feedbackStride,state=99;
  if(!read(address,&state,4))return 4;if(state)continue;
  // The game consumes only state 2; it retains state 3 until the engine copy
  // finishes, so a following producer cannot overwrite a text being consumed.
  state=1;if(!write(address,&state,4))return 4;
  if(!write(address+4,bytes,sizeof bytes)){state=0;write(address,&state,4);return 4;}
  state=2;return write(address,&state,4)?0:4;
 }
 return 8; // bounded queue is full; never overwrite the engine's current input.
}
static void appendFeedbackCode(std::vector<BYTE>& b,DWORD allocation,DWORD start){
 auto bytes=[&](std::initializer_list<BYTE> v){b.insert(b.end(),v);};
 auto word=[&](DWORD v){for(int i=0;i<4;i++)b.push_back(BYTE(v>>(8*i)));};
 for(DWORD i=0;i<2;i++){
  DWORD at=allocation+feedbackOffset+i*feedbackStride;
  bytes({0x83,0x3d});word(at);bytes({4,0x75,0x0a,0xc7,0x05});word(at);word(0);
  bytes({0x83,0x3d});word(at);bytes({2,0x0f,0x85});SIZE_T skip=b.size();word(0);
  bytes({0xc7,0x05});word(at);word(3);
  bytes({0x6a,0,0x6a,0,0x6a,1,0x68});word(at+4);
  bytes({0xe8});word(va(0x594cb0)-(start+(DWORD)b.size()+4));bytes({0x83,0xc4,0x10});
  bytes({0xc7,0x05});word(at);word(0);DWORD offset=(DWORD)(b.size()-skip-4);memcpy(b.data()+skip,&offset,4);
 }
}
