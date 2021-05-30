#include "CLI.h"

CLI::CLI(DefaultIO* dio) {
		this->dio=dio;
		ResponseToWebServer *RTWS = new ResponseToWebServer(dio);
		RTWS->AddCommand(new UploadCSV(dio));
		RTWS->AddCommand(new Settings(dio));
		RTWS->AddCommand(new Detect(dio));
		RTWS->AddCommand(new Results(dio));
		//commands.push_back(new UploadAnom(dio));
		commands.push_back(RTWS);
}

void CLI::start(){
	SharedState sharedState;
	//int index=-1;
	// while(index!=5){
	// 	dio->write("Welcome to the Anomaly Detection Server.\n");
	// 	dio->write("Please choose an option:\n");
	// 	for(size_t i=0;i<commands.size();i++){
	// 		string s("1.");
	// 		s[0]=((char)(i+1+'0'));
	// 		dio->write(s);
	// 		dio->write(commands[i]->description+"\n");
	// 	}
	// 	string input = dio->read();
	// 	index=input[0]-'0'-1;
	// 	if(index>=0 && index<=6)
	// 		commands[index]->execute(&sharedState);
	// }
	commands[0]->execute(&sharedState);
}


CLI::~CLI() {
	for(size_t i=0;i<commands.size();i++){
		delete commands[i];
	}
}

