/*
 * ConfigCrawler.cpp
 *
 * Parsing configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/parser.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#include "ConfigParser.h"

// constructor: set default values
ConfigParser::ConfigParser() {
	// parser entries
	this->generalLock = 300;
	this->generalLogging = ConfigParser::generalLoggingDefault;
	this->generalNewestOnly = true;
	this->generalReParse = false;
	this->generalResetOnFinish = false;
	this->generalSleepIdle = 500;
	this->generalSleepMySql = 20;
	this->generalTiming = false;
}

// destructor stub
ConfigParser::~ConfigParser() {}

// load parsing-specific configuration from parsed JSON document
void ConfigParser::loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) {
	// go through all array items i.e. configuration entries
	for(rapidjson::Value::ConstValueIterator i = jsonDocument.Begin(); i != jsonDocument.End(); i++) {
		if(i->IsObject()) {
			std::string cat;
			std::string name;

			// get item properties
			for(rapidjson::Value::ConstMemberIterator j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString()) {
					// check item member
					std::string itemName = j->name.GetString();
					if(itemName == "cat") {
						// category of item
						if(j->value.IsString()) cat = j->value.GetString();
						else warningsTo.push_back("Invalid category name ignored.");
					}
					else if(itemName == "name") {
						// name
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
					if(cat == "general") {
						if(name == "logging") {
							if(j->value.IsUint()) this->generalLogging = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "newest.only") {
							if(j->value.IsBool()) this->generalNewestOnly = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "reparse") {
							if(j->value.IsBool()) this->generalReParse = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "reset.on.finish") {
							if(j->value.IsBool()) this->generalResetOnFinish = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "result.table") {
							if(j->value.IsString()) this->generalResultTable = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "sleep.idle") {
							if(j->value.IsUint64()) this->generalSleepIdle = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.mysql") {
							if(j->value.IsUint64()) this->generalSleepMySql = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "timing") {
							if(j->value.IsBool()) this->generalTiming = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "parser") {
						if(name == "datetime.formats") {
							if(j->value.IsArray()) {
								this->parsingDateTimeFormats.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->parsingDateTimeFormats.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "datetime.locales") {
							if(j->value.IsArray()) {
								this->parsingDateTimeLocales.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->parsingDateTimeLocales.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "datetime.queries") {
							if(j->value.IsArray()) {
								this->parsingDateTimeQueries.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->parsingDateTimeQueries.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "datetime.sources") {
							if(j->value.IsArray()) {
								this->parsingDateTimeSources.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->parsingDateTimeSources.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.delimiters") {
							if(j->value.IsArray()) {
								this->parsingFieldDelimiters.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->parsingFieldDelimiters.push_back(Strings::getFirstOrEscapeChar(k->GetString()));
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.ignore.empty") {
							if(j->value.IsArray()) {
								this->parsingFieldIgnoreEmpty.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsBool()) this->parsingFieldIgnoreEmpty.push_back(k->GetBool());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not bool).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.json") {
							if(j->value.IsArray()) {
								this->parsingFieldJSON.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsBool()) this->parsingFieldJSON.push_back(k->GetBool());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not bool).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.names") {
							if(j->value.IsArray()) {
								this->parsingFieldNames.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->parsingFieldNames.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.queries") {
							if(j->value.IsArray()) {
								this->parsingFieldQueries.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->parsingFieldQueries.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.sources") {
							if(j->value.IsArray()) {
								this->parsingFieldSources.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->parsingFieldSources.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "id.queries") {
							if(j->value.IsArray()) {
								this->parsingIdQueries.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->parsingIdQueries.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "id.sources") {
							if(j->value.IsArray()) {
								this->parsingIdSources.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->parsingIdSources.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
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

	// check properties of datetime queries (arrays defining these queries should have the same number fo elements - one for each query)
	unsigned long completeDateTimes = std::min(this->parsingDateTimeQueries.size(), this->parsingDateTimeSources.size());
																							// number of complete datetime queries
																							// (= minimum size of all property arrays)
	bool incompleteDateTimes = false;

	// EXCEPTION: the 'date/time format' property will be ignored if array is too large or set to "%F %T" if entry is missing
	if(this->parsingDateTimeFormats.size() > completeDateTimes) this->parsingDateTimeFormats.resize(completeDateTimes);
	else while(this->parsingDateTimeFormats.size() < completeDateTimes) this->parsingDateTimeFormats.push_back("%F %T");

	// ...and empty 'date/time format' properties will also be replaced by the default value "%F %T"
	for(auto i = this->parsingDateTimeFormats.begin(); i != this->parsingDateTimeFormats.end(); ++i) if(!(i->size())) *i = "%F %T";

	// EXCEPTION: the 'locales' property will be ignored if array is too large or set to "" if entry is missing
	if(this->parsingDateTimeLocales.size() > completeDateTimes) this->parsingDateTimeLocales.resize(completeDateTimes);
	else while(this->parsingDateTimeLocales.size() < completeDateTimes) this->parsingDateTimeLocales.push_back("");

	if(this->parsingDateTimeQueries.size() > completeDateTimes) {
		// remove queries of incomplete datetime queries
		this->parsingDateTimeQueries.resize(completeDateTimes);
		incompleteDateTimes = true;
	}
	if(this->parsingDateTimeSources.size() > completeDateTimes) {
		// remove sources of incomplete datetime queries
		this->parsingDateTimeSources.resize(completeDateTimes);
		incompleteDateTimes = true;
	}
	if(incompleteDateTimes) {
		// warn about incomplete datetime queries
		warningsTo.push_back("\'datetime.queries\' and \'.sources\' should have the same number of"
				" elements.");
		warningsTo.push_back("Incomplete datetime queries removed.");
	}

	// check properties of parsing fields (arrays defining these fields should have the same number of elements - one for each field)
	unsigned long completeFields = std::min(this->parsingFieldNames.size(), std::min(this->parsingFieldQueries.size(),
			this->parsingFieldSources.size())); // number of complete fields (= minimum size of all property arrays)
	bool incompleteFields = false;

	// EXCEPTION: the 'delimiter' property will be ignored if array is too large or set to '\n' if entry is missing
	if(this->parsingFieldDelimiters.size() > completeFields) this->parsingFieldDelimiters.resize(completeFields);
	else while(this->parsingFieldDelimiters.size() < completeFields) this->parsingFieldDelimiters.push_back('\n');

	// EXCEPTION: the 'ignore empty values' property will be ignored if array is too large or set to 'true' if entry is missing
	if(this->parsingFieldIgnoreEmpty.size() > completeFields) this->parsingFieldIgnoreEmpty.resize(completeFields);
	else while(this->parsingFieldIgnoreEmpty.size() < completeFields) this->parsingFieldIgnoreEmpty.push_back(true);

	// EXCEPTION: the 'save field entry as JSON' property will be ignored if array is too large or set to 'false' if entry is missing
	if(this->parsingFieldJSON.size() > completeFields) this->parsingFieldJSON.resize(completeFields);
	else while(this->parsingFieldJSON.size() < completeFields) this->parsingFieldJSON.push_back(false);

	if(this->parsingFieldNames.size() > completeFields) {
		// remove names of incomplete parsing fields
		this->parsingFieldNames.resize(completeFields);
		incompleteFields = true;
	}
	if(this->parsingFieldQueries.size() > completeFields) {
		// remove queries of incomplete parsing fields
		this->parsingFieldQueries.resize(completeFields);
		incompleteFields = true;
	}
	if(this->parsingFieldSources.size() > completeFields) {
		// remove sources of incomplete parsing fields
		this->parsingFieldSources.resize(completeFields);
		incompleteFields = true;
	}
	if(incompleteFields) {
		// warn about incomplete parsing fields
		warningsTo.push_back("\'field.names\', \'.queries\' and \'.sources\' should have the same number of elements.");
		warningsTo.push_back("Incomplete field(s) removed.");
	}

	// check properties of id queries (arrays defining these queries should have the same number of elements - one for each query)
	unsigned long completeIds = std::min(this->parsingIdQueries.size(), this->parsingIdSources.size());	// number of complete id queries
																										// (= minimum size of all arrays)
	bool incompleteIds = false;
	if(this->parsingIdQueries.size() > completeIds) {
		// remove queries of incomplete id queries
		this->parsingIdQueries.resize(completeIds);
		incompleteIds = true;
	}
	if(this->parsingIdSources.size() > completeIds) {
		// remove sources of incomplete id queries
		this->parsingIdSources.resize(completeIds);
		incompleteIds = true;
	}
	if(incompleteIds) {
		// warn about incomplete id queries
		warningsTo.push_back("\'id.queries\' and \'.sources\' should have the same number of elements.");
		warningsTo.push_back("Incomplete id queries removed.");
	}
}
