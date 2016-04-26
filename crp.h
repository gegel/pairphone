//crypto related procedures

//control
int SetupCall(char* name); //originate call to specified contact
void HangUp(void); //terminate call, go to idle state
void ResetCall(void); //reconnect (originator) or terminate (acceptor)
void SetPassword(char* password); //apply pre-shared passphrase
int AddContact(char* name); //pair devices during active call
int ListContact(char* mask); //show contacts from adressbook matches specified mask
int Mute(int flag); //allow/deny speech transmission
int State(int flag); //set talk flag (by VAD)
//runtime
int MakePkt(unsigned char* pkt);  //process outgoing packet before sending
int ProcessPkt(unsigned char* pkt); //process incoming packet after receiving




