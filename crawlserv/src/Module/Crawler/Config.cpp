/*
 * Config.cpp
 *
 * Crawling configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/crawler.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#include "Config.h"

namespace crawlservpp::Module::Crawler {

// constructor: set default values
Config::Config() {
	// crawler entries
	this->crawlerArchives = false;
	this->crawlerArchivesNames.push_back("archives.org");
	this->crawlerArchivesUrlsMemento.push_back("http://web.archive.org/web/");
	this->crawlerArchivesUrlsTimemap.push_back("http://web.archive.org/web/timemap/link/");
	this->crawlerLock = 300;
	this->crawlerLogging = Config::crawlerLoggingDefault;
	this->crawlerReCrawl = false;
	this->crawlerReCrawlStart = true;
	this->crawlerReTries = -1;
	this->crawlerRetryArchive = true;
	this->crawlerRetryHttp.push_back(503);
	this->crawlerSleepError = 5000;
	this->crawlerSleepHttp = 0;
	this->crawlerSleepIdle = 500;
	this->crawlerSleepMySql = 20;
	this->crawlerStart = "/";
	this->crawlerTiming = false;
	this->crawlerWarningsFile = false;
	this->crawlerXml = false;

	// custom entries
	this->customCountersGlobal = true;
	this->customReCrawl = true;
}

// destructor stub
Config::~Config() {}

// load crawling-specific configuration from parsed JSON document
void Config::loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) {
	// go through all array items i.e. configuration entries
	for(rapidjson::Value::ConstValueIterator i = jsonDocument.Begin(); i != jsonDocument.End(); i++) {
		if(i->IsObject()) {
			std::string cat;
			std::string name;

			// get item properties
			for(auto j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString()) {
					// check item member
					std::string itemName = j->name.GetString();
					if(itemName == "cat") {
						if(j->value.IsString()) cat = j->value.GetString();
						else warningsTo.push_back("Invalid category name ignored.");
					}
					else if(itemName == "name") {
						if(j->value.IsString()) name = j->value.GetString();
						else warningsTo.push_back("Invalid option name ignored.");
					}
					else if(itemName != "value") {
						if(!itemName.empty()) warningsTo.push_back("Unknown configuration item \'" + itemName + "\' ignored.");
						else warningsTo.push_back("Unnamed configuration item ignored.");
					}
				}
				else warningsTo.push_back("Configuration item with invalid name ignored.");
			}

			// check item properties
			if(cat.empty()) {
				warningsTo.push_back("Configuration item without category ignored");
				continue;
			}
			if(name.empty()) {
				warningsTo.push_back("Configuration item without name ignored.");
				continue;
			}

			// get item value
			bool valueFound = false;
			for(auto j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString() && std::string(j->name.GetString()) == "value") {
					// save configuration entry
					if(cat == "crawler") {
						if(name == "archives") {
							if(j->value.IsBool()) this->crawlerArchives = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "archives.names") {
							if(j->value.IsArray()) {
								this->crawlerArchivesNames.clear();
								this->crawlerArchivesNames.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerArchivesNames.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "archives.urls.memento") {
							if(j->value.IsArray()) {
								this->crawlerArchivesUrlsMemento.clear();
								this->crawlerArchivesUrlsMemento.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerArchivesUrlsMemento.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "archives.urls.timemap") {
							if(j->value.IsArray()) {
								this->crawlerArchivesUrlsTimemap.clear();
								this->crawlerArchivesUrlsTimemap.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerArchivesUrlsTimemap.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "lock") {
							if(j->value.IsUint64()) this->crawlerLock = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "logging") {
							if(j->value.IsUint()) this->crawlerLogging = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "params.blacklist") {
							if(j->value.IsArray()) {
								this->crawlerParamsBlackList.clear();
								this->crawlerParamsBlackList.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerParamsBlackList.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "params.whitelist") {
							if(j->value.IsArray()) {
								this->crawlerParamsWhiteList.clear();
								this->crawlerParamsWhiteList.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerParamsWhiteList.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.content") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListContent.clear();
								this->crawlerQueriesBlackListContent.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListContent.push_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesBlackListContent.push_back(0);
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.types") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListTypes.clear();
								this->crawlerQueriesBlackListTypes.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListTypes.push_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesBlackListTypes.push_back(0);
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.urls") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListUrls.clear();
								this->crawlerQueriesBlackListUrls.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListUrls.push_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesBlackListUrls.push_back(0);
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.links") {
							if(j->value.IsArray()) {
								this->crawlerQueriesLinks.clear();
								this->crawlerQueriesLinks.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesLinks.push_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesLinks.push_back(0);
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.content") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListContent.clear();
								this->crawlerQueriesWhiteListContent.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListContent.push_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesWhiteListContent.push_back(0);
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.types") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListTypes.clear();
								this->crawlerQueriesWhiteListTypes.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListTypes.push_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesWhiteListTypes.push_back(0);
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.urls") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListUrls.clear();
								this->crawlerQueriesWhiteListUrls.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListUrls.push_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesWhiteListUrls.push_back(0);
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl") {
							if(j->value.IsBool()) this->crawlerReCrawl = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name
								+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "recrawl.always") {
							if(j->value.IsArray()) {
								this->crawlerReCrawlAlways.clear();
								this->crawlerReCrawlAlways.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerReCrawlAlways.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl.start") {
							if(j->value.IsBool()) this->crawlerReCrawlStart = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "retries") {
							if(j->value.IsInt64()) this->crawlerReTries = j->value.GetInt64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not long).");
						}
						else if(name == "retry.archive") {
							if(j->value.IsBool()) this->crawlerRetryArchive = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "retry.http") {
							if(j->value.IsArray()) {
								this->crawlerRetryHttp.clear();
								this->crawlerRetryHttp.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->crawlerRetryHttp.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "sleep.error") {
							if(j->value.IsUint64()) this->crawlerSleepError = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.http") {
							if(j->value.IsUint64()) this->crawlerSleepHttp = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.idle") {
							if(j->value.IsUint64()) this->crawlerSleepIdle = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.mysql") {
							if(j->value.IsUint64()) this->crawlerSleepMySql = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "start") {
							if(j->value.IsString()) this->crawlerStart = Config::makeSubUrl(j->value.GetString());
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not string).");
						}
						else if(name == "timing") {
							if(j->value.IsBool()) this->crawlerTiming = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "xml") {
							if(j->value.IsBool()) this->crawlerXml = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "warnings.file") {
							if(j->value.IsBool()) this->crawlerWarningsFile = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "custom") {
						if(name == "counters") {
							if(j->value.IsArray()) {
								this->customCounters.clear();
								this->customCounters.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->customCounters.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "counters.end") {
							if(j->value.IsArray()) {
								this->customCountersEnd.clear();
								this->customCountersEnd.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsInt64()) this->customCountersEnd.push_back(k->GetInt64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "counters.global") {
							if(j->value.IsBool()) this->customCountersGlobal = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "counters.start") {
							if(j->value.IsArray()) {
								this->customCountersStart.clear();
								this->customCountersStart.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsInt64()) this->customCountersStart.push_back(k->GetInt64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "counters.step") {
							if(j->value.IsArray()) {
								this->customCountersStep.clear();
								this->customCountersStep.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsInt64()) this->customCountersStep.push_back(k->GetInt64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl") {
							if(j->value.IsBool()) this->customReCrawl = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "urls") {
							if(j->value.IsArray()) {
								this->customUrls.clear();
								this->customUrls.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->customUrls.push_back(Config::makeSubUrl(k->GetString()));
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "network") this->network.setEntry(name, j, warningsTo);
					else warningsTo.push_back("Configuration entry with unknown category \'" + cat + "\' ignored.");
					valueFound = true;
					break;
				}
			}
			if(!valueFound) warningsTo.push_back("Configuration entry without value ignored.");
		}
		else warningsTo.push_back("Configuration entry that is no object ignored.");
	}

	// check properties of archives (arrays defining the archives should have the same number of elements - one for each archive)
	unsigned long completeArchives = crawlservpp::Helper::Algos::min(this->crawlerArchivesNames.size(),
			this->crawlerArchivesUrlsMemento.size(), this->crawlerArchivesUrlsTimemap.size());	// number of complete archives
																								// (= minimum size of all arrays)
	bool incompleteArchives = false;
	if(this->crawlerArchivesNames.size() > completeArchives) {
		// remove names of incomplete archives
		this->crawlerArchivesNames.resize(completeArchives);
		incompleteArchives = true;
	}
	if(this->crawlerArchivesUrlsMemento.size() > completeArchives) {
		// remove memento URL templates of incomplete archives
		this->crawlerArchivesUrlsMemento.resize(completeArchives);
		incompleteArchives = true;
	}
	if(this->crawlerArchivesUrlsTimemap.size() > completeArchives) {
		// remove timemap URL templates of incomplete archives
		this->crawlerArchivesUrlsTimemap.resize(completeArchives);
		incompleteArchives = true;
	}
	if(incompleteArchives) {
		// warn about incomplete counters
		warningsTo.push_back("\'archives.names\', \'.urls.memento\' and \'.urls.timemap\'"
				" should have the same number of elements.");
		warningsTo.push_back("Incomplete archive(s) removed.");
	}

	// check properties of counters (arrays defining the counters should have the same number of elements - one for each counter)
	unsigned long completeCounters = crawlservpp::Helper::Algos::min(this->customCounters.size(),
			this->customCountersStart.size(), this->customCountersEnd.size(),
			this->customCountersStep.size());	// number of complete counters (= minimum size of all arrays)
	bool incompleteCounters = false;
	if(this->customCounters.size() > completeCounters) {
		// remove counter variables of incomplete counters
		this->customCounters.resize(completeCounters);
		incompleteCounters = true;
	}
	if(this->customCountersStart.size() > completeCounters) {
		// remove starting values of incomplete counters
		this->customCountersStart.resize(completeCounters);
		incompleteCounters = true;
	}
	if(this->customCountersEnd.size() > completeCounters) {
		// remove ending values of incomplete counters
		this->customCountersEnd.resize(completeCounters);
		incompleteCounters = true;
	}
	if(this->customCountersStep.size() > completeCounters) {
		// remove step values of incomplete counters
		this->customCountersStep.resize(completeCounters);
		incompleteCounters = true;
	}
	if(incompleteCounters) {
		// warn about incomplete counters
		warningsTo.push_back("\'custom.counters\', \'.start\', \'.end\' and \'.step\'"
				" should have the same number of elements.");
		warningsTo.push_back("Incomplete counter(s) removed.");
	}

	// check validity of counters (infinite counters are invalid, therefore the need to check for counter termination)
	for(unsigned long n = 1; n <= this->customCounters.size(); n++) {
		unsigned long i = n - 1;
		if((this->customCountersStep.at(i) <= 0 && this->customCountersStart.at(i) < this->customCountersEnd.at(i))
				|| (this->customCountersStep.at(i) >= 0 && this->customCountersStart.at(i) > this->customCountersEnd.at(i))) {
			this->customCounters.erase(this->customCounters.begin() + i);
			this->customCountersStart.erase(this->customCountersStart.begin() + i);
			this->customCountersEnd.erase(this->customCountersEnd.begin() + i);
			this->customCountersStep.erase(this->customCountersStep.begin() + i);
			std::ostringstream warningStrStr;
			warningStrStr << "Counter loop of counter #" << (n + 1) << " would be infinitive, counter removed.";
			warningsTo.push_back(warningStrStr.str());
			n--;
		}
	}
}

// check for sub-URL (starting with /) or curl-supported URL protocol - adds a slash in the beginning if no protocol is found
std::string Config::makeSubUrl(const std::string& source) {
	if(!source.empty()) {
		if(source.at(0) == '/') return source;

		if(source.length() > 5) {
			if(source.substr(0, 6) == "ftp://") return source;
			if(source.substr(0, 6) == "scp://") return source;
			if(source.substr(0, 6) == "smb://") return source;

			if(source.length() > 6) {
				if(source.substr(0, 7) == "http://") return source;
				if(source.substr(0, 7) == "ftps://") return source;
				if(source.substr(0, 7) == "sftp://") return source;
				if(source.substr(0, 7) == "file://") return source;
				if(source.substr(0, 7) == "dict://") return source;
				if(source.substr(0, 7) == "imap://") return source;
				if(source.substr(0, 7) == "ldap://") return source;
				if(source.substr(0, 7) == "pop3://") return source;
				if(source.substr(0, 7) == "rtmp://") return source;
				if(source.substr(0, 7) == "rtsp://") return source;
				if(source.substr(0, 7) == "smbs://") return source;
				if(source.substr(0, 7) == "smtp://") return source;
				if(source.substr(0, 7) == "tftp://") return source;

				if(source.length() > 7) {
					if(source.substr(0, 8) == "https://") return source;
					if(source.substr(0, 8) == "imaps://") return source;
					if(source.substr(0, 8) == "ldaps://") return source;
					if(source.substr(0, 8) == "pop3s://") return source;
					if(source.substr(0, 8) == "smtps://") return source;

					if(source.length() > 8) {
						if(source.substr(0, 9) == "gopher://") return source;
						if(source.substr(0, 9) == "telnet://") return source;
					}
				}
			}
		}
	}
	return "/" + source;
}

}
