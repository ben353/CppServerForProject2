#ifndef COMMANDS_H_
#define COMMANDS_H_

#include<iostream>
#include <string.h>

#include <fstream>
#include <vector>
//#include "timeseries.h"
#include "HybridAnomalyDetector.h"

using namespace std;

class DefaultIO{
public:
	virtual string read()=0;
	virtual void write(string text)=0;
	virtual void write(float f)=0;
	virtual void read(float* f)=0;
	virtual ~DefaultIO(){}

	// you may add additional methods here

	void readAndFile(string fileName){
		ofstream out(fileName);
		string s="";
		while((s=read())!="done\n"){
			out<<s;
		}
		out.close();
	}


};

// you may add here helper classes

struct fixdReport{
	int start;
	int end;
	string description;
	bool tp;
};

struct SharedState{
	float threshold;
	SimpleAnomalyDetector *ad;
	vector<AnomalyReport> report;
	vector<fixdReport> fixdRports;
	int testFileSize;
	SharedState(){
		threshold=0.9;
		testFileSize=0;
	}
};


// you may edit this class
class Command{
protected:
	DefaultIO* dio;

public:
	const string description;
	Command(DefaultIO* dio,const string description):dio(dio),description(description){}
	virtual void execute(SharedState* sharedState)=0;
	virtual ~Command(){}
};

// implement here your command classes

class UploadCSV:public Command{
public:
	UploadCSV(DefaultIO* dio):Command(dio,"upload a time series csv file"){}
	virtual void execute(SharedState* sharedState){
		printf("Please upload your local train CSV file.\n");
		while(dio->read()!="start\n"){}//header of http
		dio->readAndFile("anomalyTrain.csv");
		printf("Upload complete.\n");
		printf("Please upload your local test CSV file.\n");
		dio->readAndFile("anomalyTest.csv");
		printf("Upload complete.\n");
	}
};

///////////////////////////////////////////new/////////////////////////////////////////////////
class ResponseToWebServer : public Command {
public:
	vector<Command*> *commands = new vector<Command*>();
	ResponseToWebServer(DefaultIO* dio) :Command(dio,"ResponseToWebServer") {}
	virtual void execute(SharedState* sharedState){
		for (int i = 0; i < commands->size() ; i++)
		{
			commands->at(i)->execute(sharedState);
		}
		
	}
	void AddCommand(Command* c)
	{
		commands->push_back(c);
	}
	virtual ~ResponseToWebServer() 
	{
		delete commands;
	}
};
/////////////////////////////////////////////////////////////////////////////////////////////////


class Settings:public Command{
public:
	Settings(DefaultIO* dio):Command(dio,"algorithm settings"){}
	virtual void execute(SharedState* sharedState){
		string line = dio->read();
		if(!line.compare("r\n")){
			sharedState->ad = new SimpleAnomalyDetector();
		}
		else //
		{
			sharedState->ad = new HybridAnomalyDetector();
		}
	}
};

class Detect:public Command{
public:
	Detect(DefaultIO* dio):Command(dio,"detect anomalies"){}
	virtual void execute(SharedState* sharedState){
		TimeSeries train("anomalyTrain.csv");
		TimeSeries test("anomalyTest.csv");
		sharedState->testFileSize = test.getRowSize();
		//HybridAnomalyDetector ad;
		sharedState->ad->setCorrelationThreshold(sharedState->threshold);
		sharedState->ad->learnNormal(train);
		sharedState->report = sharedState->ad->detect(test);

		fixdReport fr;
		fr.start=0;
		fr.end=0;
		fr.description="";
		fr.tp=false;
		for_each(sharedState->report.begin(),sharedState->report.end(),[&fr,sharedState](AnomalyReport& ar){
			if(ar.timeStep==fr.end+1 && ar.description==fr.description)
				fr.end++;
			else{
				sharedState->fixdRports.push_back(fr);
				fr.start=ar.timeStep;
				fr.end=fr.start;
				fr.description=ar.description;
			}
		});
		sharedState->fixdRports.push_back(fr);
		sharedState->fixdRports.erase(sharedState->fixdRports.begin());

		printf("anomaly detection complete.\n");
	}
};

class Results:public Command{
public:
	Results(DefaultIO* dio):Command(dio,"display results"){}
	virtual void execute(SharedState* sharedState){
		//string send = "";
		vector<string> send;// = new vector<string>();
		//send += "hello";
		for (AnomalyReport ar : sharedState->report)
		{
			send.push_back(to_string(ar.timeStep));
			send.push_back("\t" + ar.description + "\n");
		}
		// for_each(sharedState->report.begin(),sharedState->report.end(),[this,send](AnomalyReport& ar){
		// 	send = send + to_string(ar.timeStep);
		// 	send = send + "\t"+ar.description+"\n";
		// });
		int counter = 0;
		for (int i = 0; i < send.size(); i++)
		{
			counter += send[i].length();
		}
		char str[500 + 20*counter+1];
		str[0] = '\0';
		strcat(str,"HTTP/1.1 200 OK\nDate: Thu, 25 Feb 2001 12:27:04 GMT\nServer: cppServer\nLast-Modified: Wed, 1 Jun 2001 16:05:58 GMT\nContent-Type: application/json\nAccept-Ranges: bytes\nConnection: close\n\n");
		strcat(str,"{\n\t\"vectorAnomalies\": [\n");
		//str[counter] = '\0';
		//{"cars":[ {"name":"Ford" , "price":100}, {"name":"BMW", "price":200}, {"name":"Fiat", "price":300} ]}
		//{ "Mobile": "111-111-1111" },
		for (int i = 0; i < send.size(); i++)
		{
			//strcat(str, send[i].c_str());
			printf("%s",send[i].c_str());
		}
		for (AnomalyReport ar : sharedState->report)
		{
			strcat(str,("\t\t{ \"timeStep\": \"" + to_string(ar.timeStep)+"\" , ").c_str());
			strcat(str,("\"description\": \"" + ar.description + "\" },\n").c_str());
		}
		//strcat(str,'\0');
		int temp = strlen(str);
		str[temp-2] = '\0';
		strcat(str,"\n\t]\n}");
		printf("the vector is = %s",str);
		dio->write(str);
		printf("Done.\n");
	}
};


class UploadAnom:public Command{
public:
	UploadAnom(DefaultIO* dio):Command(dio,"upload anomalies and analyze results"){}

	bool crossSection(int as,int ae,int bs, int be){
		return (ae>=bs && be>=as);
	}

	bool isTP(int start, int end,SharedState* sharedState){
		for(size_t i=0;i<sharedState->fixdRports.size();i++){
			fixdReport fr=sharedState->fixdRports[i];
			if(crossSection(start,end,fr.start,fr.end)){
				sharedState->fixdRports[i].tp=true;
				return true;
			}
		}
		return false;
	}

	virtual void execute(SharedState* sharedState){
		
		for(size_t i=0;i<sharedState->fixdRports.size();i++){
			sharedState->fixdRports[i].tp=false;
		}
		
		dio->write("Please upload your local anomalies file.\n");
		string s="";
		float TP=0,sum=0,P=0;
		while((s=dio->read())!="done\n"){
			size_t t=0;
			for(;s[t]!=',';t++);
			string st=s.substr(0,t);
			string en=s.substr(t+1,s.length());
			int start = stoi(st);
			int end = stoi(en);
			if(isTP(start,end,sharedState))
				TP++;
			sum+=end+1-start;
			P++;
		}
		dio->write("Upload complete.\n");
		float FP=0;
		for(size_t i=0;i<sharedState->fixdRports.size();i++)
			if(!sharedState->fixdRports[i].tp)
				FP++;

		float N=sharedState->testFileSize - sum;
		float tpr=((int)(1000.0*TP/P))/1000.0f;
		float fpr=((int)(1000.0*FP/N))/1000.0f;
		dio->write("True Positive Rate: ");
		dio->write(tpr);
		dio->write("\nFalse Positive Rate: ");
		dio->write(fpr);
		dio->write("\n");
	}
};

// for future implemenations
class Exit:public Command{
public:
	Exit(DefaultIO* dio):Command(dio,"exit"){}
	virtual void execute(SharedState* sharedState){
	}
};


#endif /* COMMANDS_H_ */