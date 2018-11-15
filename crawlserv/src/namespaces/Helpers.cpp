/*
 * Helpers.cpp
 *
 * Global helper functions encapsulated into one namespace.
 *
 *  Created on: Oct 12, 2018
 *      Author: ans
 */

#include "Helpers.h"

// convert microseconds to string
std::string Helpers::microsecondsToString(unsigned long long microseconds) {
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
std::string Helpers::millisecondsToString(unsigned long long milliseconds) {
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
std::string Helpers::secondsToString(unsigned long long seconds) {
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

// list files with specific extension in a directory and its subdirectories
std::vector<std::string> Helpers::listFilesInPath(const std::string& pathToDir, const std::string& fileExtension) {
	std::vector<std::string> result;

	// open path
	std::experimental::filesystem::path path(pathToDir);
	if(!std::experimental::filesystem::exists(path)) throw std::runtime_error("\'" + pathToDir + "\' does not exist");
	if(!std::experimental::filesystem::is_directory(path)) throw std::runtime_error("\'" + pathToDir + "\' is not a directory");

	// iterate through items
	for(auto& it: std::experimental::filesystem::recursive_directory_iterator(path)) {
		if(it.path().extension().string() == fileExtension) result.push_back(it.path().string());
	}

	return result;
}

// convert string to boolean value
bool Helpers::stringToBool(std::string inputString) {
	std::transform(inputString.begin(), inputString.end(), inputString.begin(), ::tolower);
	std::istringstream strStr(inputString);
	bool result;
	strStr >> std::boolalpha >> result;
	return result;
}

// replace all occurences of a string with another string
void Helpers::replaceAll(std::string& strInOut, const std::string& from, const std::string& to) {
	unsigned long startPos = 0;
	if(from.empty()) return;

	while((startPos = strInOut.find(from, startPos)) != std::string::npos) {
		strInOut.replace(startPos, from.length(), to);
		startPos += to.length();
	}
}

// convert ISO-8859-1 to UTF-8
std::string Helpers::iso88591ToUtf8(const std::string& strIn) {
	std::string strOut;
	for(auto i = strIn.begin(); i != strIn.end(); ++i) {
		uint8_t c = *i;
		if(c < 0x80) strOut.push_back(c);
		else {
			strOut.push_back(0xc0 | c >> 6);
			strOut.push_back(0x80 | (c & 0x3f));
		}
	}
	return strOut;
}

// replace invalid UTF-8 characters, return whether invalid characters occured
bool Helpers::repairUtf8(const std::string& strIn, std::string& strOut) {
	if(utf8::is_valid(strIn.begin(), strIn.end())) return false;
	utf8::replace_invalid(strIn.begin(), strIn.end(), back_inserter(strOut));
	return true;
}

// trim a string
void Helpers::trim(std::string & stringToTrim) {
	stringToTrim.erase(stringToTrim.begin(), std::find_if(stringToTrim.begin(), stringToTrim.end(), [](int ch) {
		return !std::isspace(ch);
	}));
	stringToTrim.erase(std::find_if(stringToTrim.rbegin(), stringToTrim.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), stringToTrim.end());
}

// parse Memento reply, get mementos and link to next page if one exists (also convert timestamps to YYYYMMDD HH:MM:SS)
std::string Helpers::parseMementos(std::string mementoContent, std::vector<std::string>& warningsTo,
		std::vector<Memento>& mementosTo) {
	std::string nextPage;
	Memento newMemento;
	std::ostringstream warningStrStr;
	unsigned long pos = 0;
	unsigned long end = 0;
	bool mementoStarted = false;
	bool newField = true;

	while(pos < mementoContent.length()) {
		// skip wildchars
		if(mementoContent.at(pos) == ' ' || mementoContent.at(pos) == '\r' || mementoContent.at(pos) == '\n'
				|| mementoContent.at(pos) == '\t') {
			pos++;
		}
		// parse link
		else if(mementoContent.at(pos) == '<') {
			end = mementoContent.find('>', pos + 1);
			if(end == std::string::npos) {
				warningStrStr << "No '>' after '<' for link at " << pos << ".";
				warningsTo.push_back(warningStrStr.str());
				warningStrStr.clear();
				warningStrStr.str(std::string());
				break;
			}
			if(mementoStarted) {
				// memento not finished -> warning
				if(newMemento.url.length() && newMemento.timeStamp.length()) mementosTo.push_back(newMemento);
				warningStrStr << "New memento started without finishing the old one at " << pos << ".";
				warningsTo.push_back(warningStrStr.str());
				warningStrStr.clear();
				warningStrStr.str(std::string());
			}
			mementoStarted = true;
			newMemento.url = mementoContent.substr(pos + 1, end - pos - 1);
			newMemento.timeStamp = "";
			pos = end + 1;
		}
		// parse field separator
		else if(mementoContent.at(pos) == ';') {
			newField = true;
			pos++;
		}
		// parse end of memento
		else if(mementoContent.at(pos) == ',') {
			if(mementoStarted) {
				if(newMemento.url.size() && newMemento.timeStamp.size()) mementosTo.push_back(newMemento);
				mementoStarted  = false;
			}
			pos++;
		}
		else {
			if(!newField) {
				warningStrStr << "Field seperator missing for new field at " << pos << ".";
				warningsTo.push_back(warningStrStr.str());
				warningStrStr.clear();
				warningStrStr.str(std::string());
			}
			else newField = false;
			end = mementoContent.find('=', pos + 1);
			if(end == std::string::npos) {
				end = mementoContent.find_first_of(",;");
				if(end == std::string::npos) {
					warningStrStr << "Cannot find end of field at " << pos << ".";
					warningsTo.push_back(warningStrStr.str());
					warningStrStr.clear();
					warningStrStr.str(std::string());
					break;
				}
				pos = end;
			}
			else {
				std::string fieldName = mementoContent.substr(pos, end - pos);
				unsigned long oldPos = pos;
				pos = mementoContent.find_first_of("\"\'", pos + 1);
				if(pos == std::string::npos) {
					warningStrStr << "Cannot find begin of value at " << oldPos << ".";
					warningsTo.push_back(warningStrStr.str());
					warningStrStr.clear();
					warningStrStr.str(std::string());
					pos++;
					continue;
				}
				end = mementoContent.find_first_of("\"\'", pos + 1);
				if(end == std::string::npos) {
					warningStrStr << "Cannot find end of value at " << pos << ".";
					warningsTo.push_back(warningStrStr.str());
					warningStrStr.clear();
					warningStrStr.str(std::string());
					break;
				}
				std::string fieldValue = mementoContent.substr(pos + 1, end - pos - 1);

				if(fieldName == "datetime") {
					// parse timestamp
					if(Helpers::convertLongDateToSQLTimeStamp(fieldValue)) newMemento.timeStamp = fieldValue;
					else {
						warningStrStr << "Could not convert timestamp \'" << fieldValue << "\'  at " << pos << ".";
						warningsTo.push_back(warningStrStr.str());
						warningStrStr.clear();
						warningStrStr.str(std::string());
					}
				}
				else if(fieldName == "rel") {
					if(fieldValue == "timemap" && newMemento.url.size()) {
						nextPage = newMemento.url;
						newMemento.url = "";
					}
				}
				pos = end + 1;
			}
		}
	}

	if(mementoStarted && newMemento.url.size() && newMemento.timeStamp.size()) mementosTo.push_back(newMemento);
	return nextPage;
}

// convert time stamp from WEEKDAY[short], DD MONTH[short] YYYY HH:MM:SS TIMEZONE[short] to YYYY-MM-DD HH:MM:SS
bool Helpers::convertLongDateToSQLTimeStamp(std::string& timeStamp) {
	std::istringstream in(timeStamp);
	date::sys_seconds tp;
	in >> date::parse("%a, %d %b %Y %T %Z", tp);
	if(!bool(in)) return false;
	timeStamp = date::format("%F %T", tp);
	return true;
}

// convert time stamp from YYYYMMDDHHMMSS to YYYY-MM-DD HH:MM:SS
bool Helpers::convertTimeStampToSQLTimeStamp(std::string& timeStamp) {
	std::istringstream in(timeStamp);
	date::sys_seconds tp;
	in >> date::parse("%Y%m%d%H%M%S", tp);
	if(!bool(in)) return false;
	timeStamp = date::format("%F %T", tp);
	return true;
}

// convert time stamp from YYYY-MM-DD HH:MM:SS to YYYYMMDDHHMMSS
bool Helpers::convertSQLTimeStampToTimeStamp(std::string& timeStamp) {
	std::istringstream in(timeStamp);
	date::sys_seconds tp;
	in >> date::parse("%F %T", tp);
	if(!bool(in)) return false;
	timeStamp = date::format("%Y%m%d%H%M%S", tp);
	return true;
}

// portable getch
static struct termios oldT, newT;

// portable getch
char Helpers::getch() {
#ifdef __unix
	char ch = 0;
	tcgetattr(0, &oldT);
	newT = oldT;
	newT.c_lflag &= ~ICANON;
	newT.c_lflag &= ~ECHO;
	tcsetattr(0, TCSANOW, &newT);
	ch = getchar();
	tcsetattr(0, TCSANOW, &oldT);
	return ch;
#elif
	return ::getch();
#endif
}
