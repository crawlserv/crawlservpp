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
Config::Config() : crawlerArchives(false), crawlerHTMLCanonicalCheck(false), crawlerHTMLConsistencyCheck(false), crawlerLock(300),
		crawlerLogging(Config::crawlerLoggingDefault), crawlerReCrawl(false), crawlerReCrawlStart(true), crawlerReTries(-1),
		crawlerRetryArchive(true), crawlerSleepError(5000), crawlerSleepHttp(0), crawlerSleepIdle(500), crawlerSleepMySql(20),
		crawlerStart("/"), crawlerTiming(false), crawlerUrlCaseSensitive(true), crawlerUrlChunks(5000), crawlerUrlDebug(false),
		crawlerWarningsFile(false),	crawlerXml(false), customCountersGlobal(true), customReCrawl(true) {
	this->crawlerArchivesNames.emplace_back("archives.org");
	this->crawlerArchivesUrlsMemento.emplace_back("http://web.archive.org/web/");
	this->crawlerArchivesUrlsTimemap.emplace_back("http://web.archive.org/web/timemap/link/");
	this->crawlerRetryHttp.push_back(502);
	this->crawlerRetryHttp.push_back(503);
	this->crawlerRetryHttp.push_back(504);
}

// destructor stub
Config::~Config() {}

// load crawling-specific configuration from parsed JSON document
void Config::loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) {
	// go through all array items i.e. configuration entries
	for(rapidjson::Value::ConstValueIterator i = jsonDocument.Begin(); i != jsonDocument.End(); i++) {
		if(i->IsObject()) {
			std::string cat;
			std::string name;

			// get item properties
			for(auto j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString()) {
					// check item member
					std::string itemName(j->name.GetString(), j->name.GetStringLength());
					if(itemName == "cat") {
						if(j->value.IsString()) cat = std::string(j->value.GetString(), j->value.GetStringLength());
						else warningsTo.emplace("Invalid category name ignored.");
					}
					else if(itemName == "name") {
						if(j->value.IsString()) name = std::string(j->value.GetString(), j->value.GetStringLength());
						else warningsTo.emplace("Invalid option name ignored.");
					}
					else if(itemName != "value") {
						if(!itemName.empty()) warningsTo.emplace("Unknown configuration item \'" + itemName + "\' ignored.");
						else warningsTo.emplace("Unnamed configuration item ignored.");
					}
				}
				else warningsTo.emplace("Configuration item with invalid name ignored.");
			}

			// check item properties
			if(cat.empty()) {
				warningsTo.emplace("Configuration item without category ignored");
				continue;
			}
			if(name.empty()) {
				warningsTo.emplace("Configuration item without name ignored.");
				continue;
			}

			// get item value
			bool valueFound = false;
			for(auto j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString() && std::string(j->name.GetString(), j->name.GetStringLength()) == "value") {
					// save configuration entry
					if(cat == "crawler") {
						if(name == "archives") {
							if(j->value.IsBool()) this->crawlerArchives = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "archives.names") {
							if(j->value.IsArray()) {
								this->crawlerArchivesNames.clear();
								this->crawlerArchivesNames.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->crawlerArchivesNames.emplace_back(k->GetString(), k->GetStringLength());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "archives.urls.memento") {
							if(j->value.IsArray()) {
								this->crawlerArchivesUrlsMemento.clear();
								this->crawlerArchivesUrlsMemento.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->crawlerArchivesUrlsMemento.emplace_back(k->GetString(), k->GetStringLength());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "archives.urls.timemap") {
							if(j->value.IsArray()) {
								this->crawlerArchivesUrlsTimemap.clear();
								this->crawlerArchivesUrlsTimemap.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->crawlerArchivesUrlsTimemap.emplace_back(k->GetString(), k->GetStringLength());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "html.canonical.check") {
							if(j->value.IsBool()) this->crawlerHTMLCanonicalCheck = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "html.consistency.check") {
							if(j->value.IsBool()) this->crawlerHTMLConsistencyCheck = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "lock") {
							if(j->value.IsUint64()) this->crawlerLock = j->value.GetUint64();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "logging") {
							if(j->value.IsUint()) this->crawlerLogging = j->value.GetUint();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "params.blacklist") {
							if(j->value.IsArray()) {
								this->crawlerParamsBlackList.clear();
								this->crawlerParamsBlackList.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->crawlerParamsBlackList.emplace_back(k->GetString(), k->GetStringLength());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "params.whitelist") {
							if(j->value.IsArray()) {
								this->crawlerParamsWhiteList.clear();
								this->crawlerParamsWhiteList.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->crawlerParamsWhiteList.emplace_back(k->GetString(), k->GetStringLength());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.content") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListContent.clear();
								this->crawlerQueriesBlackListContent.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListContent.emplace_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesBlackListContent.push_back(0);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.types") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListTypes.clear();
								this->crawlerQueriesBlackListTypes.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListTypes.emplace_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesBlackListTypes.push_back(0);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.urls") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListUrls.clear();
								this->crawlerQueriesBlackListUrls.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListUrls.emplace_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesBlackListUrls.push_back(0);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.links") {
							if(j->value.IsArray()) {
								this->crawlerQueriesLinks.clear();
								this->crawlerQueriesLinks.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesLinks.emplace_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesLinks.push_back(0);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.content") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListContent.clear();
								this->crawlerQueriesWhiteListContent.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListContent.emplace_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesWhiteListContent.push_back(0);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.types") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListTypes.clear();
								this->crawlerQueriesWhiteListTypes.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListTypes.emplace_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesWhiteListTypes.push_back(0);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.urls") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListUrls.clear();
								this->crawlerQueriesWhiteListUrls.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListUrls.emplace_back(k->GetUint64());
									else if(k->IsNull()) this->crawlerQueriesWhiteListUrls.push_back(0);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long or null).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl") {
							if(j->value.IsBool()) this->crawlerReCrawl = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name
								+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "recrawl.always") {
							if(j->value.IsArray()) {
								this->crawlerReCrawlAlways.clear();
								this->crawlerReCrawlAlways.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerReCrawlAlways.emplace_back(k->GetString());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl.start") {
							if(j->value.IsBool()) this->crawlerReCrawlStart = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "retries") {
							if(j->value.IsInt64()) this->crawlerReTries = j->value.GetInt64();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not long).");
						}
						else if(name == "retry.archive") {
							if(j->value.IsBool()) this->crawlerRetryArchive = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "retry.http") {
							if(j->value.IsArray()) {
								this->crawlerRetryHttp.clear();
								this->crawlerRetryHttp.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->crawlerRetryHttp.emplace_back(k->GetUint());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not array).");
						}
						else if(name == "sleep.error") {
							if(j->value.IsUint64()) this->crawlerSleepError = j->value.GetUint64();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.http") {
							if(j->value.IsUint64()) this->crawlerSleepHttp = j->value.GetUint64();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.idle") {
							if(j->value.IsUint64()) this->crawlerSleepIdle = j->value.GetUint64();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.mysql") {
							if(j->value.IsUint64()) this->crawlerSleepMySql = j->value.GetUint64();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "start") {
							if(j->value.IsString())
								this->crawlerStart =
										Config::makeSubUrl(std::string(j->value.GetString(), j->value.GetStringLength()));
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not string).");
						}
						else if(name == "timing") {
							if(j->value.IsBool()) this->crawlerTiming = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "url.case.sensitive") {
							if(j->value.IsBool()) this->crawlerUrlCaseSensitive = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "url.chunks") {
							if(j->value.IsUint64()) this->crawlerUrlChunks = j->value.GetUint64();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "url.debug") {
							if(j->value.IsBool()) this->crawlerUrlDebug = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "xml") {
							if(j->value.IsBool()) this->crawlerXml = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not bool).");
						}
						else if(name == "warnings.file") {
							if(j->value.IsBool()) this->crawlerWarningsFile = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else warningsTo.emplace("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "custom") {
						if(name == "counters") {
							if(j->value.IsArray()) {
								this->customCounters.clear();
								this->customCounters.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->customCounters.emplace_back(k->GetString(), k->GetStringLength());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "counters.end") {
							if(j->value.IsArray()) {
								this->customCountersEnd.clear();
								this->customCountersEnd.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsInt64()) this->customCountersEnd.emplace_back(k->GetInt64());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not long).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "counters.global") {
							if(j->value.IsBool()) this->customCountersGlobal = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "counters.start") {
							if(j->value.IsArray()) {
								this->customCountersStart.clear();
								this->customCountersStart.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsInt64()) this->customCountersStart.emplace_back(k->GetInt64());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not long).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "counters.step") {
							if(j->value.IsArray()) {
								this->customCountersStep.clear();
								this->customCountersStep.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsInt64()) this->customCountersStep.emplace_back(k->GetInt64());
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not long).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl") {
							if(j->value.IsBool()) this->customReCrawl = j->value.GetBool();
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "urls") {
							if(j->value.IsArray()) {
								this->customUrls.clear();
								this->customUrls.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->customUrls.emplace_back(
												Config::makeSubUrl(std::string(k->GetString(), k->GetStringLength()))
										);
									else warningsTo.emplace("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.emplace("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else warningsTo.emplace("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "network") this->network.setEntry(name, j, warningsTo);
					else warningsTo.emplace("Configuration entry with unknown category \'" + cat + "\' ignored.");
					valueFound = true;
					break;
				}
			}
			if(!valueFound) warningsTo.emplace("Configuration entry without value ignored.");
		}
		else warningsTo.emplace("Configuration entry that is no object ignored.");
	}

	// check properties of archives (arrays defining the archives should have the same number of elements - one for each archive)
	unsigned long completeArchives = Helper::Algos::min(this->crawlerArchivesNames.size(),
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
		warningsTo.emplace("\'archives.names\', \'.urls.memento\' and \'.urls.timemap\'"
				" should have the same number of elements.");
		warningsTo.emplace("Incomplete archive(s) removed.");
	}

	// check properties of counters (arrays defining the counters should have the same number of elements - one for each counter)
	unsigned long completeCounters = Helper::Algos::min(this->customCounters.size(),
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
		warningsTo.emplace("\'custom.counters\', \'.start\', \'.end\' and \'.step\'"
				" should have the same number of elements.");
		warningsTo.emplace("Incomplete counter(s) removed.");
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
			warningsTo.emplace(warningStrStr.str());
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
