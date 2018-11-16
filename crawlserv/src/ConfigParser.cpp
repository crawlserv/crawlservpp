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
	this->parserDateTimeFormat = "%F %T";
	this->parserDateTimeQuery = 0;
	this->parserDateTimeSource = ConfigParser::parserSourceContent;
	this->parserIdQuery = 0;
	this->parserIdSource = ConfigParser::parserSourceUrl;
	this->parserLogging = ConfigParser::parserLoggingDefault;
	this->parserNewestOnly = true;
	this->parserReParse = false;
	this->parserResultMultiSeparator = ",";
	this->parserSleepIdle = 500;
	this->parserSleepMySql = 20;
}

// destructor stub
ConfigParser::~ConfigParser() {}

// load configuration from JSON string
bool ConfigParser::loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo) {
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
					if(cat == "parser") {
						if(name == "datetime.format") {
							if(j->value.IsString()) this->parserDateTimeFormat = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "datetime.locale") {
							if(j->value.IsString()) this->parserDateTimeLocale = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "datetime.query") {
							if(j->value.IsUint64()) this->parserDateTimeQuery = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "datetime.source") {
							if(j->value.IsUint()) this->parserDateTimeSource = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
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
						else if(name == "id.query") {
							if(j->value.IsUint64()) this->parserIdQuery = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "id.source") {
							if(j->value.IsUint()) this->parserIdSource = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not unsigned int).");
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
						else if(name == "result.multi.quotes") {
							if(j->value.IsString()) this->parserResultMultiQuotes = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "result.multi.separator") {
							if(j->value.IsString()) this->parserResultMultiSeparator = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "result.table") {
							if(j->value.IsString()) this->parserResultTable = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
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

	// check fields
	unsigned long completeFields = std::min(this->parserFieldNames.size(), std::min(this->parserFieldQueries.size(),
			this->parserFieldSources.size()));
	bool incompleteFields = false;
	if(this->parserFieldNames.size() > completeFields) {
		this->parserFieldNames.resize(completeFields);
		incompleteFields = true;
	}
	if(this->parserFieldQueries.size() > completeFields) {
		this->parserFieldQueries.resize(completeFields);
		incompleteFields = true;
	}
	if(this->parserFieldSources.size() > completeFields) {
		this->parserFieldSources.resize(completeFields);
		incompleteFields = true;
	}
	if(incompleteFields) {
		warningsTo.push_back("\'field.names\', \'.queries\' and \'.sources\' should have the same number of elements).");
		warningsTo.push_back("Incomplete field(s) at the end removed.");
	}

	return true;
}

// get error message
const std::string& ConfigParser::getErrorMessage() const {
	return this->errorMessage;
}
