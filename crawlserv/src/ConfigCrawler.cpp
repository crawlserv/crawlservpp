/*
 * ConfigCrawler.cpp
 *
 * Configuration for crawlers.
 *
 * WARNING:	Changing the configuration requires updating 'json/crawler.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#include "ConfigCrawler.h"

// constructor: set default values
ConfigCrawler::ConfigCrawler() {
	// crawler entries
	this->crawlerArchives = false;
	this->crawlerLock = 300;
	this->crawlerLogging = ConfigCrawler::crawlerLoggingDefault;
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

	// network entries
	this->networkConnectionsMax = 5;
	this->networkContentLengthIgnore = false;
	this->networkCookies = false;
	this->networkCookiesSession = true;
	this->networkDnsCacheTimeOut = 60;
	this->networkDnsShuffle = false;
	this->networkEncodingBr = true;
	this->networkEncodingDeflate = true;
	this->networkEncodingGZip = true;
	this->networkEncodingIdentity = true;
	this->networkEncodingTransfer = false;
	this->networkHttpVersion = ConfigCrawler::networkHttpVersionV2tls;
	this->networkLocalPort = 0;
	this->networkLocalPortRange = 1;
	this->networkProxyTunnelling = false;
	this->networkRedirect = true;
	this->networkRedirectMax = -1;
	this->networkRedirectPost301 = false;
	this->networkRedirectPost302 = false;
	this->networkRedirectPost303 = false;
	this->networkRefererAutomatic = false;
	this->networkSpeedDownLimit = 0;
	this->networkSpeedLowLimit = 0;
	this->networkSpeedLowTime = 0;
	this->networkSpeedUpLimit = 0;
	this->networkSslVerifyHost = true;
	this->networkSslVerifyPeer = true;
	this->networkSslVerifyProxyHost = true;
	this->networkSslVerifyProxyPeer = true;
	this->networkSslVerifyStatus = false;
	this->networkTcpFastOpen = false;
	this->networkTcpKeepAlive = false;
	this->networkTcpKeepAliveIdle = 60;
	this->networkTcpKeepAliveInterval = 60;
	this->networkTcpNagle = false;
	this->networkTimeOut = 300;
	this->networkTimeOutHappyEyeballs = 0;
	this->networkTimeOutRequest = 300;
	this->networkVerbose = false;
}

// destructor stub
ConfigCrawler::~ConfigCrawler() {}

// load configuration from JSON string
bool ConfigCrawler::loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo) {
	// parse JSON
	rapidjson::Document json;
	if(json.Parse(configJson.c_str()).HasParseError()) {
		this->errorMessage = "Could not parse configuration JSON.";
		return false;
	}
	if(!json.IsArray()) {
		this->errorMessage = "Invalid configuration JSON (is no array).";
		return false;
	}

	// go through all array items
	for(rapidjson::Value::ConstValueIterator i = json.Begin(); i != json.End(); i++) {
		if(i->IsObject()) {
			std::string cat;
			std::string name;

			// get item properties
			for(rapidjson::Value::ConstMemberIterator j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
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
						if(itemName.length()) warningsTo.push_back("Unknown configuration item \'" + itemName + "\' ignored.");
						else warningsTo.push_back("Unnamed configuration item ignored.");
					}
				}
				else warningsTo.push_back("Configuration item with invalid name ignored.");
			}

			// check item properties
			if(!cat.length()) {
				warningsTo.push_back("Configuration item without category ignored");
				continue;
			}
			if(!name.length()) {
				warningsTo.push_back("Configuration item without name ignored.");
				continue;
			}

			// get item value
			bool valueFound = false;
			for(rapidjson::Value::ConstMemberIterator j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString() && std::string(j->name.GetString()) == "value") {
					// save configuration entry
					if(cat == "crawler") {
						if(name == "archives") {
							if(j->value.IsBool()) this->crawlerArchives = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "lock") {
							if(j->value.IsUint64()) this->crawlerLock = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "logging") {
							if(j->value.IsUint()) this->crawlerLogging = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "params.blacklist") {
							if(j->value.IsArray()) {
								this->crawlerParamsBlackList.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerParamsBlackList.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "params.whitelist") {
							if(j->value.IsArray()) {
								this->crawlerParamsWhiteList.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerParamsWhiteList.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.content") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListContent.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListContent.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.types") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListTypes.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListTypes.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.blacklist.urls") {
							if(j->value.IsArray()) {
								this->crawlerQueriesBlackListUrls.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesBlackListUrls.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.links") {
							if(j->value.IsArray()) {
								this->crawlerQueriesLinks.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesLinks.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.content") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListContent.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListContent.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.types") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListTypes.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListTypes.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "queries.whitelist.urls") {
							if(j->value.IsArray()) {
								this->crawlerQueriesWhiteListUrls.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->crawlerQueriesWhiteListUrls.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl") {
							if(j->value.IsBool()) this->crawlerReCrawl = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "recrawl.always") {
							if(j->value.IsArray()) {
								this->crawlerReCrawlAlways.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->crawlerReCrawlAlways.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "recrawl.start") {
							if(j->value.IsBool()) this->crawlerReCrawlStart = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "retries") {
							if(j->value.IsInt64()) this->crawlerReTries = j->value.GetInt64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not long).");
						}
						else if(name == "retry.archive") {
							if(j->value.IsBool()) this->crawlerRetryArchive = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "retry.http") {
							if(j->value.IsArray()) {
								this->crawlerRetryHttp.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->crawlerRetryHttp.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "sleep.error") {
							if(j->value.IsUint64()) this->crawlerSleepError = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.http") {
							if(j->value.IsUint64()) this->crawlerSleepHttp = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.idle") {
							if(j->value.IsUint64()) this->crawlerSleepIdle = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.mysql") {
							if(j->value.IsUint64()) this->crawlerSleepMySql = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "start") {
							if(j->value.IsString()) this->crawlerStart = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "timing") {
							if(j->value.IsBool()) this->crawlerTiming = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "xml") {
							if(j->value.IsBool()) this->crawlerXml = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
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
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->customUrls.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "network") {
						if(name == "connections.max") {
							if(j->value.IsUint()) this->networkConnectionsMax = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "contentlength.ignore") {
							if(j->value.IsBool()) this->networkContentLengthIgnore = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "cookies") {
							if(j->value.IsBool()) this->networkCookies = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "cookies.load") {
							if(j->value.IsString()) this->networkCookiesLoad = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "cookies.overwrite") {
							if(j->value.IsArray()) {
								this->networkCookiesOverwrite.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->networkCookiesOverwrite.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "cookies.save") {
							if(j->value.IsString()) this->networkCookiesSave = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "cookies.session") {
							if(j->value.IsBool()) this->networkCookiesSession = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "cookies.set") {
							if(j->value.IsString()) this->networkCookiesSet = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "dns.cachetimeout") {
							if(j->value.IsInt64()) this->networkDnsCacheTimeOut = j->value.GetInt64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not long).");
						}
						else if(name == "dns.doh") {
							if(j->value.IsString()) this->networkDnsDoH = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "dns.interface") {
							if(j->value.IsString()) this->networkDnsInterface = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "dns.resolves") {
							if(j->value.IsArray()) {
								this->networkDnsResolves.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->networkDnsResolves.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "dns.servers") {
							if(j->value.IsArray()) {
								this->networkDnsServers.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->networkDnsServers.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "dns.shuffle") {
							if(j->value.IsBool()) this->networkDnsShuffle = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "encoding.br") {
							if(j->value.IsBool()) this->networkEncodingBr = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "encoding.deflate") {
							if(j->value.IsBool()) this->networkEncodingDeflate = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "encoding.gzip") {
							if(j->value.IsBool()) this->networkEncodingGZip = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "encoding.identity") {
							if(j->value.IsBool()) this->networkEncodingIdentity = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "encoding.transfer") {
							if(j->value.IsBool()) this->networkEncodingTransfer = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "headers") {
							if(j->value.IsArray()) {
								this->networkHeaders.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->networkHeaders.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "http.200aliases") {
							if(j->value.IsArray()) {
								this->networkHttp200Aliases.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->networkHttp200Aliases.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "http.version") {
							if(j->value.IsInt()) this->networkHttpVersion = j->value.GetInt();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not int).");
						}
						else if(name == "local.interface") {
							if(j->value.IsString()) this->networkLocalInterface = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "local.port") {
							if(j->value.IsUint()) this->networkLocalPort = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "local.portrange") {
							if(j->value.IsUint()) this->networkLocalPortRange = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "proxy") {
							if(j->value.IsString()) this->networkProxy = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "proxy.auth") {
							if(j->value.IsString()) this->networkProxyAuth = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "proxy.headers") {
							if(j->value.IsArray()) {
								this->networkProxyHeaders.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->networkProxyHeaders.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "proxy.pre") {
							if(j->value.IsString()) this->networkProxyPre = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "proxy.tlssrp.password") {
							if(j->value.IsString()) this->networkProxyTlsSrpPassword = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "proxy.tlssrp.user") {
							if(j->value.IsString()) this->networkProxyTlsSrpUser = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "proxyy.tunnelling") {
							if(j->value.IsBool()) this->networkProxyTunnelling = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "redirect") {
							if(j->value.IsBool()) this->networkRedirect = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "redirect.max") {
							if(j->value.IsInt64()) this->networkRedirectMax = j->value.GetInt64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not long).");
						}
						else if(name == "redirect.post301") {
							if(j->value.IsBool()) this->networkRedirectPost301 = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "redirect.post302") {
							if(j->value.IsBool()) this->networkRedirectPost302 = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "redirect.post303") {
							if(j->value.IsBool()) this->networkRedirectPost303 = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "referer") {
							if(j->value.IsString()) this->networkReferer = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "referer.automatic") {
							if(j->value.IsBool()) this->networkRefererAutomatic = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "speed.downlimit") {
							if(j->value.IsUint64()) this->networkSpeedDownLimit = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "speed.lowlimit") {
							if(j->value.IsUint64()) this->networkSpeedLowLimit = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "speed.lowtime") {
							if(j->value.IsUint64()) this->networkSpeedLowTime = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "speed.uplimit") {
							if(j->value.IsUint64()) this->networkSpeedUpLimit = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "ssl.verify.host") {
							if(j->value.IsBool()) this->networkSslVerifyHost = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "ssl.verify.peer") {
							if(j->value.IsBool()) this->networkSslVerifyPeer= j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "ssl.verify.proxy.host") {
							if(j->value.IsBool()) this->networkSslVerifyProxyHost = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "ssl.verify.proxy.peer") {
							if(j->value.IsBool()) this->networkSslVerifyProxyPeer = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "ssl.verify.status") {
							if(j->value.IsBool()) this->networkSslVerifyStatus = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "tcp.fastopen") {
							if(j->value.IsBool()) this->networkTcpFastOpen = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "tcp.keepalive") {
							if(j->value.IsBool()) this->networkTcpKeepAlive = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "tcp.keepalive.idle") {
							if(j->value.IsUint64()) this->networkTcpKeepAliveIdle = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "tcp.keepalive.interval") {
							if(j->value.IsUint64()) this->networkTcpKeepAliveInterval = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "tcp.nagle") {
							if(j->value.IsBool()) this->networkTcpNagle = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "timeout") {
							if(j->value.IsUint64()) this->networkTimeOut = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "timeout.happyeyeballs") {
							if(j->value.IsUint()) this->networkTimeOutHappyEyeballs = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "timeout.request") {
							if(j->value.IsUint64()) this->networkTimeOutRequest = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "tlssrp.password") {
							if(j->value.IsString()) this->networkTlsSrpPassword = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "tlssrp.user") {
							if(j->value.IsString()) this->networkTlsSrpUser = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "useragent") {
							if(j->value.IsString()) this->networkUserAgent = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "verbose") {
							if(j->value.IsBool()) this->networkVerbose = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else warningsTo.push_back("Configuration entry with unknown category \'" + cat + "\' ignored.");
					valueFound = true;
					break;
				}
			}
			if(!valueFound) warningsTo.push_back("Configuration entry without value ignored.");
		}
		else warningsTo.push_back("Configuration entry that is no object ignored.");
	}

	// check counters
	unsigned long completeCounters = std::min(this->customCounters.size(), std::min(this->customCountersStart.size(),
			std::min(this->customCountersEnd.size(), this->customCountersStep.size())));
	bool incompleteCounters = false;
	if(this->customCounters.size() > completeCounters) {
		this->customCounters.resize(completeCounters);
		incompleteCounters = true;
	}
	if(this->customCountersStart.size() > completeCounters) {
		this->customCountersStart.resize(completeCounters);
		incompleteCounters = true;
	}
	if(this->customCountersEnd.size() > completeCounters) {
		this->customCountersEnd.resize(completeCounters);
		incompleteCounters = true;
	}
	if(this->customCountersStep.size() > completeCounters) {
		this->customCountersStep.resize(completeCounters);
		incompleteCounters = true;
	}
	if(incompleteCounters) {
		warningsTo.push_back("\'custom.counters\', \'.start\', \'.end\' and \'.step\' should have the same number of elements).");
		warningsTo.push_back("Incomplete counter(s) at the end removed.");
	}

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

	return true;
}

// get error message
const std::string& ConfigCrawler::getErrorMessage() const {
	return this->errorMessage;
}
