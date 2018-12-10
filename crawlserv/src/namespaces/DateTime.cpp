/*
 * DateTime.cpp
 *
 * Namespace with global helper functions for date/time and time to string conversion.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#include "DateTime.h"

// convert microseconds to string
std::string DateTime::microsecondsToString(unsigned long long microseconds) {
	unsigned long long rest = microseconds;
	unsigned long days = rest / 86400000000;
	rest -= days * 86400000000;
	unsigned short hours = rest / 3600000000;
	rest -= hours * 3600000000;
	unsigned short minutes = rest / 60000000;
	rest -= minutes * 60000000;
	unsigned short seconds = rest / 1000000;
	rest -= seconds * 1000000;
	unsigned short milliseconds = rest / 1000;
	rest -= milliseconds * 1000;

	std::ostringstream resultStrStr;
	if(days) resultStrStr << days << "d ";
	if(hours) resultStrStr << hours << "h ";
	if(minutes) resultStrStr << minutes << "min ";
	if(seconds) resultStrStr << seconds << "s ";
	if(milliseconds) resultStrStr << milliseconds << "ms ";
	if(rest) resultStrStr << rest << "μs ";

	std::string resultStr = resultStrStr.str();
	if(resultStr.length()) resultStr.pop_back();
	else return "0μs";
	return resultStr;
}

// convert milliseconds to string
std::string DateTime::millisecondsToString(unsigned long long milliseconds) {
	unsigned long long rest = milliseconds;
	unsigned long days = rest / 86400000;
	rest -= days * 86400000;
	unsigned short hours = rest / 3600000;
	rest -= hours * 3600000;
	unsigned short minutes = rest / 60000;
	rest -= minutes * 60000;
	unsigned short seconds = rest / 1000;
	rest -= seconds * 1000;

	std::ostringstream resultStrStr;
	if(days) resultStrStr << days << "d ";
	if(hours) resultStrStr << hours << "h ";
	if(minutes) resultStrStr << minutes << "min ";
	if(seconds) resultStrStr << seconds << "s ";
	if(rest) resultStrStr << rest << "ms ";

	std::string resultStr = resultStrStr.str();
	if(resultStr.length()) resultStr.pop_back();
	else return "0ms";
	return resultStr;
}

// convert seconds to string
std::string DateTime::secondsToString(unsigned long long seconds) {
	unsigned long long rest = seconds;
	unsigned long days = rest / 86400;
	rest -= days * 86400;
	unsigned short hours = rest / 3600;
	rest -= hours * 3600;
	unsigned short minutes = rest / 60;
	rest -= minutes * 60;

	std::ostringstream resultStrStr;
	if(days) resultStrStr << days << "d ";
	if(hours) resultStrStr << hours << "h ";
	if(minutes) resultStrStr << minutes << "min ";
	if(rest) resultStrStr << rest << "s ";

	std::string resultStr = resultStrStr.str();
	if(resultStr.length()) resultStr.pop_back();
	else return "0s";
	return resultStr;
}

// convert time stamp from WEEKDAY[short], DD MONTH[short] YYYY HH:MM:SS TIMEZONE[short] to YYYY-MM-DD HH:MM:SS
bool DateTime::convertLongDateToSQLTimeStamp(std::string& timeStamp) {
	std::istringstream in(timeStamp);
	date::sys_seconds tp;
	in >> date::parse("%a, %d %b %Y %T %Z", tp);
	if(!bool(in)) return false;
	timeStamp = date::format("%F %T", tp);
	return true;
}

// convert time stamp from YYYYMMDDHHMMSS to YYYY-MM-DD HH:MM:SS
bool DateTime::convertTimeStampToSQLTimeStamp(std::string& timeStamp) {
	std::istringstream in(timeStamp);
	date::sys_seconds tp;
	in >> date::parse("%Y%m%d%H%M%S", tp);
	if(!bool(in)) return false;
	timeStamp = date::format("%F %T", tp);
	return true;
}

// convert time stamp from YYYY-MM-DD HH:MM:SS to YYYYMMDDHHMMSS
bool DateTime::convertSQLTimeStampToTimeStamp(std::string& timeStamp) {
	std::istringstream in(timeStamp);
	date::sys_seconds tp;
	in >> date::parse("%F %T", tp);
	if(!bool(in)) return false;
	timeStamp = date::format("%Y%m%d%H%M%S", tp);
	return true;
}
