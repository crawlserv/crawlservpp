/*
 * Config.cpp
 *
 * Analyzing configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/analyzer.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#include "Config.h"

namespace crawlservpp::Module::Analyzer {

// constructor: set default values
Config::Config() {
	// general entries
	this->generalLogging = Config::generalLoggingDefault;
	this->generalSleepMySql = 20;
	this->generalSleepWhenFinished = 1000;

	// markov-text entries
	this->markovTextDimension = 3;
	this->markovTextLength = 400;
	this->markovTextMax = 0;
	this->markovTextResultField = "text";
	this->markovTextSourcesField = "sources";
	this->markovTextSleep = 0;
	this->markovTextTiming = true;

	// markov-tweet entries
	this->markovTweetDimension = 5;
	this->markovTweetLanguage = "en_US";
	this->markovTweetLength = 140;
	this->markovTweetMax = 0;
	this->markovTweetResultField = "tweet";
	this->markovTweetSourcesField = "sources";
	this->markovTweetSleep = 0;
	this->markovTweetSpellcheck = true;
	this->markovTweetTiming = true;

	// filter by date entries
	this->filterDateEnable = false;
}

// destructor stub
Config::~Config() {}

// load analyzing-specific configuration from parsed JSON document
void Config::loadModule(const rapidjson::Document& jsonDocument,
		std::vector<std::string>& warningsTo) {
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
				if(name != "_algo") warningsTo.push_back("Configuration item without category ignored");
				continue;
			}
			if(!name.length()) {
				warningsTo.push_back("Configuration item without name ignored.");
				continue;
			}

			// get item value
			bool valueFound = false;
			for(auto j = i->MemberBegin(); j != i->MemberEnd(); ++j) {
				if(j->name.IsString() && std::string(j->name.GetString()) == "value") {
					// save configuration entry
					if(cat == "general") {
						if(name == "input.fields") {
							if(j->value.IsArray()) {
								this->generalInputFields.clear();
								this->generalInputFields.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) {
										std::string fieldName = k->GetString();
										if(crawlservpp::Helper::Strings::checkSQLName(fieldName))
											this->generalInputFields.push_back(fieldName);
										else warningsTo.push_back("\'" + fieldName + "\' in \'" + cat + "." + name
												+ "\' ignored because it contains invalid characters.");
									}
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "input.sources") {
							if(j->value.IsArray()) {
								this->generalInputSources.clear();
								this->generalInputSources.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsUint()) this->generalInputSources.push_back(k->GetUint());
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not unsigned int).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "input.tables") {
							if(j->value.IsArray()) {
								this->generalInputTables.clear();
								this->generalInputTables.reserve(j->value.Size());
								for(auto k = j->value.Begin(); k != j->value.End(); ++k) {
									if(k->IsString()) {
										std::string fieldName = k->GetString();
										if(crawlservpp::Helper::Strings::checkSQLName(fieldName))
											this->generalInputTables.push_back(fieldName);
										else warningsTo.push_back("\'" + fieldName + "\' in \'" + cat + "." + name
												+ "\' ignored because it contains invalid characters.");
									}
									else warningsTo.push_back("Value in \'" + cat + "." + name
											+ "\' ignored because of wrong type (not string).");
								}
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not array).");
						}
						else if(name == "logging") {
							if(j->value.IsUint()) this->generalLogging = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned int).");
						}
						else if(name == "result.table") {
							if(j->value.IsString()) {
								std::string tableName = j->value.GetString();
								if(crawlservpp::Helper::Strings::checkSQLName(tableName))
									this->generalResultTable = tableName;
								else warningsTo.push_back("\'" + tableName + "\' in \'" + cat + "." + name
										+ "\' ignored because it contains invalid characters.");
							}
							else warningsTo.push_back("\'" + cat + "." + name + "\' ignored because of wrong type (not string).");
						}
						else if(name == "sleep.mysql") {
							if(j->value.IsUint64()) this->generalSleepMySql = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sleep.when.finished") {
							if(j->value.IsUint64()) this->generalSleepWhenFinished = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "markov-tweet") {
						if(name == "dimension") {
							if(j->value.IsUint() && j->value.GetUint() < 256) this->markovTweetDimension = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned char).");
						}
						else if(name == "language") {
							if(j->value.IsString()) this->markovTweetLanguage = j->value.GetString();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not string).");
						}
						else if(name == "length") {
							if(j->value.IsUint64()) this->markovTweetLength = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "."
									+ name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "max") {
							if(j->value.IsUint64()) this->markovTweetMax = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "result.field") {
							if(j->value.IsString()) {
								std::string fieldName = j->value.GetString();
								if(crawlservpp::Helper::Strings::checkSQLName(fieldName))
									this->markovTweetResultField = fieldName;
								else warningsTo.push_back("\'" + fieldName + "\' in \'" + cat + "." + name
										+ "\' ignored because it contains invalid characters.");
							}
						}
						else if(name == "sleep") {
							if(j->value.IsUint64()) this->markovTweetSleep = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sources.field") {
							if(j->value.IsString()) {
								std::string fieldName = j->value.GetString();
								if(crawlservpp::Helper::Strings::checkSQLName(fieldName))
									this->markovTweetSourcesField = fieldName;
								else warningsTo.push_back("\'" + fieldName + "\' in \'" + cat + "." + name
										+ "\' ignored because it contains invalid characters.");
							}
						}
						else if(name == "spellcheck") {
							if(j->value.IsBool()) this->markovTweetSpellcheck = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "."
									+ name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "timing") {
							if(j->value.IsBool()) this->markovTweetTiming = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "."
									+ name + "\' ignored because of wrong type (not bool).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "markov-text") {
						if(name == "dimension") {
							if(j->value.IsUint() && j->value.GetUint() < 256) this->markovTextDimension = j->value.GetUint();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned char).");
						}
						else if(name == "length") {
							if(j->value.IsUint64()) this->markovTextLength = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "."
									+ name + "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "max") {
							if(j->value.IsUint64()) this->markovTextMax = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "result.field") {
							if(j->value.IsString()) {
								std::string fieldName = j->value.GetString();
								if(crawlservpp::Helper::Strings::checkSQLName(fieldName))
									this->markovTextResultField = fieldName;
								else warningsTo.push_back("\'" + fieldName + "\' in \'" + cat + "." + name
										+ "\' ignored because it contains invalid characters.");
							}
						}
						else if(name == "sleep") {
							if(j->value.IsUint64()) this->markovTextSleep = j->value.GetUint64();
							else warningsTo.push_back("\'" + cat + "." + name
									+ "\' ignored because of wrong type (not unsigned long).");
						}
						else if(name == "sources.field") {
							if(j->value.IsString()) {
								std::string fieldName = j->value.GetString();
								if(crawlservpp::Helper::Strings::checkSQLName(fieldName))
									this->markovTextSourcesField = fieldName;
								else warningsTo.push_back("\'" + fieldName + "\' in \'" + cat + "." + name
										+ "\' ignored because it contains invalid characters.");
							}
						}
						else if(name == "timing") {
							if(j->value.IsBool()) this->markovTextTiming = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "."
									+ name + "\' ignored because of wrong type (not bool).");
						}
						else warningsTo.push_back("Unknown configuration entry \'" + cat + "." + name + "\' ignored.");
					}
					else if(cat == "filter-date") {
						if(name == "enable") {
							if(j->value.IsBool()) this->filterDateEnable = j->value.GetBool();
							else warningsTo.push_back("\'" + cat + "."
									+ name + "\' ignored because of wrong type (not bool).");
						}
						else if(name == "from") {
							if(j->value.IsString()) {
								std::string fieldDate = j->value.GetString();
								if(crawlservpp::Helper::DateTime::isValidISODate(fieldDate))
									this->filterDateFrom = fieldDate;
								else warningsTo.push_back("\'" + fieldDate + "\' in \'" + cat + "." + name
										+ "\' ignored because it does not contain a valid date (YYYY-MM-DD).");
							}
						}
						else if(name == "to") {
							if(j->value.IsString()) {
								std::string fieldDate = j->value.GetString();
								if(crawlservpp::Helper::DateTime::isValidISODate(fieldDate))
									this->filterDateTo = fieldDate;
								else warningsTo.push_back("\'" + fieldDate + "\' in \'" + cat + "." + name
										+ "\' ignored because it does not contain a valid date (YYYY-MM-DD).");
							}
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

	// check properties of inputs (arrays with fields, sources and tables should have the same number of elements)
	unsigned long completeInputs = crawlservpp::Helper::Algos::min(this->generalInputFields.size(),
			this->generalInputSources.size(), this->generalInputTables.size());				// number of complete inputs
																							// (= minimum size of all arrays)
	bool incompleteInputs = false;

	if(this->generalInputFields.size() > completeInputs) {
		// remove queries of incomplete datetime queries
		this->generalInputFields.resize(completeInputs);
		incompleteInputs = true;
	}
	if(this->generalInputSources.size() > completeInputs) {
		// remove queries of incomplete datetime queries
		this->generalInputSources.resize(completeInputs);
		incompleteInputs = true;
	}
	if(this->generalInputTables.size() > completeInputs) {
		// remove sources of incomplete datetime queries
		this->generalInputTables.resize(completeInputs);
		incompleteInputs = true;
	}
	if(incompleteInputs) {
		// warn about incomplete inputs
		warningsTo.push_back("\'input.fields\', \'.sources\' and \'.tables\' should have the same number of elements.");
		warningsTo.push_back("Incomplete inputs removed.");
	}
}

}
