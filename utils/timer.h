/*******************************************
   Author        : Jun Zhang
   Email         : ewalker.zj@gmail.com
   Last Modified : 2017-05-05 14:02
   Filename      : timer.h
   Description   : 
*******************************************/
#include<time.h>
#include<string>

using std::string;

timespec diff(timespec start, timespec end){
	timespec diffval;
	if((end.tv_nsec - start.tv_nsec) < 0){
		diffval.tv_sec = end.tv_sec - start.tv_sec - 1;
		diffval.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
	}
	else{
		diffval.tv_sec = end.tv_sec - start.tv_sec;
		diffval.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	
	return diffval;
}

string curr_time(){
	time_t now_time = time(NULL);
	tm* local;
	local = localtime(&now_time);
	char buf[128] = {0};
	strftime(buf, 64, "%Y-%m-%d %H:%M:%S", local);
	string st = buf;
	return st;
}


