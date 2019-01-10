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
	this->parserLock = 300;
	this->parserLogging = ConfigParser::parserLoggingDefault;
	this->parserNewestOnly = true;
	this->parserReParse = false;
	this->parserSleepIdle = 500;
	this->parserSleepMySql = 20;
	this->parserTiming = false;
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
					if(cat == "parser") {
						if(name == "datetime.formats") {
							if(j->value.IsArray()) {
								this->parserDateTimeFormats.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->parserDateTimeFormats.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "datetime.locales") {
							if(j->value.IsArray()) {
								this->parserDateTimeLocales.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->parserDateTimeLocales.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "datetime.queries") {
							if(j->value.IsArray()) {
								this->parserDateTimeQueries.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->parserDateTimeQueries.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "datetime.sources") {
							if(j->value.IsArray()) {
								this->parserDateTimeSources.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->parserDateTimeSources.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.delimiters") {
							if(j->value.IsArray()) {
								this->parserFieldDelimiters.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString())
										this->parserFieldDelimiters.push_back(Strings::getFirstOrEscapeChar(k->GetString()));
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.ignore.empty") {
							if(j->value.IsArray()) {
								this->parserFieldIgnoreEmpty.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsBool()) this->parserFieldIgnoreEmpty.push_back(k->GetBool());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not bool).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.json") {
							if(j->value.IsArray()) {
								this->parserFieldJSON.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsBool()) this->parserFieldJSON.push_back(k->GetBool());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not bool).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.names") {
							if(j->value.IsArray()) {
								this->parserFieldNames.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) this->parserFieldNames.push_back(k->GetString());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.queries") {
							if(j->value.IsArray()) {
								this->parserFieldQueries.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->parserFieldQueries.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "field.sources") {
							if(j->value.IsArray()) {
								this->parserFieldSources.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->parserFieldSources.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "id.queries") {
							if(j->value.IsArray()) {
								this->parserIdQueries.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint64()) this->parserIdQueries.push_back(k->GetUint64());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned long).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "id.sources") {
							if(j->value.IsArray()) {
								this->parserIdSources.clear();
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->parserIdSources.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "logging") {
							if(j->value.IsUint()) this->parserLogging = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "newest.only") {
							if(j->value.IsBool()) this->parserNewestOnly = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "reparse") {
							if(j->value.IsBool()) this->parserReParse = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "sleep.idle") {
							if(j->value.IsUint64()) this->parserSleepIdle = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.mysql") {
							if(j->value.IsUint64()) this->parserSleepMySql = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "result.table") {
							if(j->value.IsString()) this->parserResultTable = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "timing") {
							if(j->value.IsBool()) this->parserTiming = j->value.GetBool();
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

	// check properties of datetime queries (arrays defining these queries should have the same number fo elements - one for each query)
	unsigned long completeDateTimes = std::min(this->parserDateTimeQueries.size(), this->parserDateTimeSources.size());
																							// number of complete datetime queries
																							// (= minimum size of all property arrays)
	bool incompleteDateTimes = false;

	// EXCEPTION: the 'date/time format' property will be ignored if array is too large or set to "%F %T" if entry is missing
	if(this->parserDateTimeFormats.size() > completeDateTimes) this->parserDateTimeFormats.resize(completeDateTimes);
	else while(this->parserDateTimeFormats.size() < completeDateTimes) this->parserDateTimeFormats.push_back("%F %T");

	// ...and empty 'date/time format' properties will also be replaced by the default value "%F %T"
	for(auto i = this->parserDateTimeFormats.begin(); i != this->parserDateTimeFormats.end(); ++i) if(!(i->size())) *i = "%F %T";

	// EXCEPTION: the 'locales' property will be ignored if array is too large or set to "" if entry is missing
	if(this->parserDateTimeLocales.size() > completeDateTimes) this->parserDateTimeLocales.resize(completeDateTimes);
	else while(this->parserDateTimeLocales.size() < completeDateTimes) this->parserDateTimeLocales.push_back("");

	if(this->parserDateTimeQueries.size() > completeDateTimes) {
		// remove queries of incomplete datetime queries
		this->parserDateTimeQueries.resize(completeDateTimes);
		incompleteDateTimes = true;
	}
	if(this->parserDateTimeSources.size() > completeDateTimes) {
		// remove sources of incomplete datetime queries
		this->parserDateTimeSources.resize(completeDateTimes);
		incompleteDateTimes = true;
	}
	if(incompleteDateTimes) {
		// warn about incomplete datetime queries
		warningsTo.push_back("\'datetime.queries\' and \'.sources\' should have the same number of"
				" elements.");
		warningsTo.push_back("Incomplete datetime queries removed.");
	}

	// check properties of parsing fields (arrays defining these fields should have the same number of elements - one for each field)
	unsigned long completeFields = std::min(this->parserFieldNames.size(), std::min(this->parserFieldQueries.size(),
			this->parserFieldSources.size())); // number of complete fields (= minimum size of all property arrays)
	bool incompleteFields = false;

	// EXCEPTION: the 'delimiter' property will be ignored if array is too large or set to '\n' if entry is missing
	if(this->parserFieldDelimiters.size() > completeFields) this->parserFieldDelimiters.resize(completeFields);
	else while(this->parserFieldDelimiters.size() < completeFields) this->parserFieldDelimiters.push_back('\n');

	// EXCEPTION: the 'ignore empty values' property will be ignored if array is too large or set to 'true' if entry is missing
	if(this->parserFieldIgnoreEmpty.size() > completeFields) this->parserFieldIgnoreEmpty.resize(completeFields);
	else while(this->parserFieldIgnoreEmpty.size() < completeFields) this->parserFieldIgnoreEmpty.push_back(true);

	// EXCEPTION: the 'save field entry as JSON' property will be ignored if array is too large or set to 'false' if entry is missing
	if(this->parserFieldJSON.size() > completeFields) this->parserFieldJSON.resize(completeFields);
	else while(this->parserFieldJSON.size() < completeFields) this->parserFieldJSON.push_back(false);

	if(this->parserFieldNames.size() > completeFields) {
		// remove names of incomplete parsing fields
		this->parserFieldNames.resize(completeFields);
		incompleteFields = true;
	}
	if(this->parserFieldQueries.size() > completeFields) {
		// remove queries of incomplete parsing fields
		this->parserFieldQueries.resize(completeFields);
		incompleteFields = true;
	}
	if(this->parserFieldSources.size() > completeFields) {
		// remove sources of incomplete parsing fields
		this->parserFieldSources.resize(completeFields);
		incompleteFields = true;
	}
	if(incompleteFields) {
		// warn about incomplete parsing fields
		warningsTo.push_back("\'field.names\', \'.queries\' and \'.sources\' should have the same number of elements.");
		warningsTo.push_back("Incomplete field(s) removed.");
	}

	// check properties of id queries (arrays defining these queries should have the same number of elements - one for each query)
	unsigned long completeIds = std::min(this->parserIdQueries.size(), this->parserIdSources.size());	// number of complete id queries
																										// (= minimum size of all arrays)
	bool incompleteIds = false;
	if(this->parserIdQueries.size() > completeIds) {
		// remove queries of incomplete id queries
		this->parserIdQueries.resize(completeIds);
		incompleteIds = true;
	}
	if(this->parserIdSources.size() > completeIds) {
		// remove sources of incomplete id queries
		this->parserIdSources.resize(completeIds);
		incompleteIds = true;
	}
	if(incompleteIds) {
		// warn about incomplete id queries
		warningsTo.push_back("\'id.queries\' and \'.sources\' should have the same number of elements.");
		warningsTo.push_back("Incomplete id queries removed.");
	}
}
