/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Config.hpp
 *
 * Abstract class as base for module-specific configuration classes.
 *
 *  Created on: Jan 7, 2019
 *      Author: ans
 */

#ifndef MODULE_CONFIG_HPP_
#define MODULE_CONFIG_HPP_

// hardcoded debugging constant
#define MODULE_CONFIG_DEBUG

#include "../Helper/Json.hpp"
#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/ConfigItem.hpp"

#include "../_extern/rapidjson/include/rapidjson/document.h"

#ifdef MODULE_CONFIG_DEBUG
#include <algorithm>	// std::adjacent_find, std::sort
#endif

#include <cstdint>		// std::int16_t, std::int32_t, std::int64_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <limits>		// std::numeric_limits
#include <queue>		// std::queue
#include <stdexcept>	// std::logic_error
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module {

	/*
	 * DECLARATION
	 */

	class Config {
		// for convenience
		using JsonException = Helper::Json::Exception;

		using ConfigItem = Struct::ConfigItem;

		using LogQueue = std::queue<std::string>&;
		using LogPtr = std::queue<std::string> *;

	public:
		Config();
		virtual ~Config();

		// configuration loader
		void loadConfig(const std::string& configJson, LogQueue& log);

		// class for configuration exceptions
		MAIN_EXCEPTION_CLASS();

		// not moveable, not copyable
		Config(Config&) = delete;
		Config(Config&&) = delete;
		Config& operator=(Config&) = delete;
		Config& operator=(Config&&) = delete;

protected:
		// configuration parsing options
		enum StringParsingOption {
			Default = 0,	// none
			SQL,			// require SQL-compatible string
			SubURL,			// convert to sub-URL
			URL				// convert to URL (without protocol)
		};
		
		enum CharParsingOption {
			FromNumber = 0,	// get by numeric value
			FromString		// get first or escaped first character from a string
		};

		// configuration parsing functions
		void category(const std::string& category);

		void option(const std::string& name, bool& target);
		void option(const std::string& name, std::vector<bool>& target);
		void option(const std::string& name, char& target, CharParsingOption opt);
		void option(const std::string& name, std::vector<char>& target, CharParsingOption opt);
		void option(const std::string& name, std::int16_t& target);
		void option(const std::string& name, std::vector<std::int16_t>& target);
		void option(const std::string& name, std::int32_t& target);
		void option(const std::string& name, std::vector<std::int32_t>& target);
		void option(const std::string& name, std::int64_t& target);
		void option(const std::string& name, std::vector<std::int64_t>& target);
		void option(const std::string& name, unsigned char& target);
		void option(const std::string& name, std::vector<unsigned char>& target);
		void option(const std::string& name, std::uint16_t& target);
		void option(const std::string& name, std::vector<std::uint16_t>& target);
		void option(const std::string& name, std::uint32_t& target);
		void option(const std::string& name, std::vector<std::uint32_t>& target);
		void option(const std::string& name, std::uint64_t& target);
		void option(const std::string& name, std::vector<std::uint64_t>& target);
		void option(const std::string& name, std::string &target, StringParsingOption opt = Default);
		void option(const std::string& name, std::vector<std::string>& target, StringParsingOption opt = Default);

		void warning(const std::string& warning);

		// module-specific parsing functions
		virtual void parseBasicOption();
		virtual void parseOption() = 0;
		virtual void checkOptions() = 0;

	private:
		// temporary variables for configuration parsing
		ConfigItem currentItem;			// current configuration item
		bool setCategory;				// category has been set
		bool foundCategory;				// category has been found
		bool currentCategory;			// category equals current category
		bool finished;					// item has been found
		LogPtr logPtr;					// pointer to logging queue

#ifdef MODULE_CONFIG_DEBUG
		bool debug;						// options have been debugged
		std::string categoryString;		// category to check as string
		std::vector<std::string> list;	// container to check for duplicates
#endif

		// private static helper functions
		static void makeSubUrl(std::string& s);
		static void makeUrl(std::string& s);

	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor stub
	inline Config::Config() :	setCategory(false),
								foundCategory(false),
								currentCategory(false),
								finished(false),
								logPtr(nullptr)
#ifdef MODULE_CONFIG_DEBUG
								, debug(true)
#endif
	{}

	// destructor stub
	inline Config::~Config() {}

	// load configuration, throws Config::Exception
	inline void Config::loadConfig(
			const std::string& configJson,
			std::queue<std::string>& warningsTo
	) {
		// save pointer to logging queue
		this->logPtr = &warningsTo;

		// parse JSON
		rapidjson::Document json;

		try {
			json = Helper::Json::parseRapid(configJson);
		}
		catch(const JsonException& e) {
			throw Exception(
					"Module::Config::loadConfig(): Could not parse configuration JSON - "
					+ e.whatStr()
			);
		}

		if(!json.IsArray())
			throw Exception(
					"Module::Config::loadConfig(): Invalid configuration JSON (is no array)."
			);

		// parse configuration entries
		for(const auto& entry : json.GetArray()) {
			// check whether configuration item is a JSON object
			if(!entry.IsObject()) {
				warningsTo.emplace("Configuration entry that is no object ignored.");

				return;
			}

			// go through all members of the JSON object, find its properties (name, category, value)
			bool empty = true;
			bool ignore = false;

			for(const auto& member : entry.GetObject()) {
				// check the name of the current item member
				if(member.name.IsString()) {
					const std::string memberName(
							member.name.GetString(),
							member.name.GetStringLength()
					);

					if(memberName == "cat") {
						// found the category of the configuration item
						if(member.value.IsString())
							this->currentItem.category = std::string(
									member.value.GetString(),
									member.value.GetStringLength()
							);
						else {
							warningsTo.emplace("Configuration entry with invalid category name ignored.");

							ignore = true;

							break;
						}
					}
					else if(memberName == "name") {
						// found the name of the configuration item
						if(member.value.IsString())
							this->currentItem.name = std::string(
									member.value.GetString(),
									member.value.GetStringLength()
							);
						else {
							warningsTo.emplace("Configuration entry with invalid option name ignored.");

							ignore = true;

							break;
						}

					}
					else if(memberName == "value") {
						// found the value of the configuration item
						this->currentItem.value = &(member.value);

						empty = false;
					}
					else {
						// found an unknown member of the configuration member
						if(memberName.empty())
							warningsTo.emplace(
									"Unnamed configuration entry member ignored."
						);
						else
							warningsTo.emplace(
									"Unknown configuration entry member \'"
									+ memberName
									+ "\' ignored."
							);

						continue;
					}
				}
				else
					// member has an invalid name (no string)
					warningsTo.emplace("Configuration entry member with invalid name ignored.");
			}

			// check whether configuration entry should be ignored
			if(ignore)
				continue;

			// check configuration item
			if(this->currentItem.category.empty()) {
				if(this->currentItem.name != "_algo") // ignore algorithm ID
					this->logPtr->emplace("Configuration item without category ignored");

				continue;
			}
			if(this->currentItem.name.empty()) {
				this->logPtr->emplace("Configuration item without name ignored.");

				continue;
			}
			if(empty) {
				this->logPtr->emplace("Configuration entry without value ignored.");

				continue;
			}

			// parse option by child class
			this->parseBasicOption();

#ifdef MODULE_CONFIG_DEBUG
			if(this->debug) {
				// debug options
				std::sort(this->list.begin(), this->list.end());

				const auto i = std::adjacent_find(this->list.begin(), this->list.end());

				if(i != this->list.end())
					throw Exception("Duplicate option \'" + *i + "\'");

				this->list.clear();

				this->debug = false;
			}
#endif

			// check whether category and item were found
			if(!this->finished) {
				if(this->foundCategory)
					this->logPtr->emplace(
							"Unknown configuration entry \'"
							+ this->currentItem.str()
							+ "\' ignored."
					);
				else
					this->logPtr->emplace(
							"Configuration entry with unknown category \'"
							+ this->currentItem.category
							+ "\' ignored."
					);
			}

			// update parsing status
			this->currentItem = ConfigItem();
			this->foundCategory = false;
			this->currentCategory = false;
			this->finished = false;
		}

		// check options
		this->checkOptions();

		// unset logging queue
		this->logPtr = nullptr;
	}

	// set category of configuration items to parse
	inline void Config::category(const std::string& category) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->categoryString = category;
#endif

		// check parsing state
		if(this->finished)
			return;

		// update parsing state
		this->setCategory = true;

		// compare category with current configuration item
		if(this->currentItem.category == category) {
			this->foundCategory = true;
			this->currentCategory = true;
		}
		else
			this->currentCategory = false;
	}

	// check for a configuration option (bool), throws Config::Exception
	inline void Config::option(const std::string& name, bool& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsBool())
			target = this->currentItem.value->GetBool();
		else if(this->currentItem.value->IsNull())
			target = false;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not bool)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of bools), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<bool>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsBool())
					target.push_back(item.GetBool());
				else {
					target.push_back(false);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not bool)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (char), throws Config::Exception
	inline void Config::option(const std::string& name, char& target, CharParsingOption opt) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		switch(opt) {
		case FromNumber:
			// get from number
			if(this->currentItem.value->IsInt()) {
				const int value = this->currentItem.value->GetInt();

				if(value > std::numeric_limits<char>::max())
					this->logPtr->emplace(
							"\'" + this->currentItem.str() + "\'"
							" ignored because the number is too large."
					);
				else
					target = value;
			}
			else if(this->currentItem.value->IsNull())
				target = 0;
			else if(this->logPtr)
				this->logPtr->emplace(
						"\'" + this->currentItem.str() + "\'"
						" ignored because of wrong type (not int)."
				);

			break;

		case FromString:
			// get from string
			if(this->currentItem.value->IsString())
				target = Helper::Strings::getFirstOrEscapeChar(
						std::string(
								this->currentItem.value->GetString(),
								this->currentItem.value->GetStringLength()
						)
				);
			else
				this->logPtr->emplace(
						"\'" + this->currentItem.str() + "\'"
						" ignored because of wrong type (not string)."
				);

			break;

		default:
			throw Exception("Config::option(): Invalid string parsing option");
		}

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of chars), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<char>& target, CharParsingOption opt) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				switch(opt) {

				case FromNumber:
					// get from number
					if(item.IsInt()) {
						const int value = item.GetInt();

						if(value > std::numeric_limits<char>::max())
							this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because the number is too large."
							);
						else
							target.push_back(value);
					}
					else {
						target.push_back(0);

						if(!item.IsNull() && this->logPtr)
							this->logPtr->emplace(
									"Value in \'" + this->currentItem.str() + "\'"
									" ignored because of wrong type (not int)."
							);
					}

					break;

				case FromString:
					// get from string
					if(item.IsString())
						target.push_back(
								Helper::Strings::getFirstOrEscapeChar(
										std::string(
												item.GetString(),
												item.GetStringLength()
										)
								)
						);
					else {
						target.push_back(0);

						if(!item.IsNull() && this->logPtr)
							this->logPtr->emplace(
									"Value in \'"
									+ this->currentItem.str()
									+ "\' ignored because of wrong type (not string)."
							);
					}

					break;

				default:
					throw Exception("Config::option(): Invalid string parsing option");
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (16-bit integer), throws Config::Exception
	inline void Config::option(const std::string& name, std::int16_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsInt()) {
			int value = this->currentItem.value->GetInt();

			if(value > std::numeric_limits<std::int16_t>::max())
				this->logPtr->emplace(
						"\'" + this->currentItem.str() + "\'"
						" ignored because the number is too large."
				);
			else
				target = value;
		}
		else if(this->currentItem.value->IsNull())
			target = 0;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not integer)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of 16-bit integers), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<std::int16_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsInt()) {
					const int value = item.GetInt();

					if(value > std::numeric_limits<std::int16_t>::max())
						this->logPtr->emplace(
							"Value in \'" + this->currentItem.str() + "\'"
							" ignored because the number is too large."
						);
					else
						target.push_back(value);
				}
				else {
					target.push_back(0);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not integer)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (32-bit integer), throws Config::Exception
	inline void Config::option(const std::string& name, std::int32_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsInt())
			target = this->currentItem.value->GetInt();
		else if(this->currentItem.value->IsNull())
			target = 0;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not integer)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of 32-bit integers), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<std::int32_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsInt())
					target.push_back(item.GetInt());
				else {
					target.push_back(0);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not integer)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (64-bit integer), throws Config::Exception
	inline void Config::option(const std::string& name, std::int64_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsInt64())
			target = this->currentItem.value->GetInt64();
		else if(this->currentItem.value->IsNull())
			target = 0;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not 64-bit integer)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of 64-bit integers), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<std::int64_t>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsInt64())
					target.push_back(item.GetInt64());
				else {
					target.push_back(0);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not 64-bit integer)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (unsigned char), throws Config::Exception
	inline void Config::option(const std::string& name, unsigned char& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsUint()) {
			unsigned int value = this->currentItem.value->GetUint();

			if(value > std::numeric_limits<unsigned char>::max())
				this->logPtr->emplace(
						"\'" + this->currentItem.str() + "\'"
						" ignored because the number is too large."
				);
			else
				target = value;
		}
		else if(this->currentItem.value->IsNull())
			target = 0;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not unsigned integer)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of unsigned chars), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<unsigned char>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint()) {
					const unsigned int value = item.GetUint();

					if(value > std::numeric_limits<unsigned char>::max())
						this->logPtr->emplace(
							"Value in \'" + this->currentItem.str() + "\'"
							" ignored because the number is too large."
						);
					else
						target.push_back(value);
				}
				else {
					target.push_back(0);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned integer)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (unsigned 16-bit integer), throws Config::Exception
	inline void Config::option(const std::string& name, std::uint16_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsUint()) {
			const unsigned int value = this->currentItem.value->GetUint();

			if(value > std::numeric_limits<std::uint16_t>::max())
				this->logPtr->emplace(
						"\'" + this->currentItem.str() + "\'"
						" ignored because the number is too large."
				);
			else
				target = value;
		}
		else if(this->currentItem.value->IsNull())
			target = 0;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not unsigned integer)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of unsigned 16-bit intefers), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<std::uint16_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint()) {
					const unsigned int value = item.GetUint();

					if(value > std::numeric_limits<std::uint16_t>::max())
						this->logPtr->emplace(
							"Value in \'" + this->currentItem.str() + "\'"
							" ignored because the number is too large."
						);
					else
						target.push_back(value);
				}
				else {
					target.push_back(0);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned integer)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (unsigned 32-bit integer), throws Config::Exception
	inline void Config::option(const std::string& name, std::uint32_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsUint())
			target = this->currentItem.value->GetUint();
		else if(this->currentItem.value->IsNull())
			target = 0;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not unsigned int)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of unsigned 32-bit integers), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<std::uint32_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint())
					target.push_back(item.GetUint());
				else {
					target.push_back(0);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned integer)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (unsigned 64-bit integer), throws Config::Exception
	inline void Config::option(const std::string& name, std::uint64_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsUint64())
			target = this->currentItem.value->GetUint64();
		else if(this->currentItem.value->IsNull())
			target = 0;
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not unsigned 64-bit integer)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of 64-bit integers), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<std::uint64_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint64())
					target.push_back(item.GetUint64());
				else {
					target.push_back(0);

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned 64-bit integer)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is finished
		this->finished = true;
	}

	// check for a configuration option (string), throws Config::Exception
	inline void Config::option(const std::string& name, std::string &target, StringParsingOption opt) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsString()) {
			std::string str(
					this->currentItem.value->GetString(),
					this->currentItem.value->GetStringLength()
			);

			switch(opt) {
			case Default:
				// get simple string
				target.swap(str);

				break;

			case SQL:
				// check for SQL compatibility
				if(Helper::Strings::checkSQLName(str))
					target.swap(str);
				else if(this->logPtr)
					this->logPtr->emplace(
							"\'" + str + "\' in \'" + this->currentItem.str() + "\'"
							" ignored because it contains invalid characters."
					);

				break;

			case SubURL:
				// convert to sub-URL
				Config::makeSubUrl(str);

				target.swap(str);

				break;

			case URL:
				// convert to URL
				Config::makeUrl(str);

				target.swap(str);

				break;

			default:
				throw Exception("Config::option(): Invalid string parsing option");
			}
		}
		else if(this->currentItem.value->IsNull())
			target.clear();
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not string)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of strings), throws Config::Exception
	inline void Config::option(const std::string& name, std::vector<std::string>& target, StringParsingOption opt) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug)
			this->list.emplace_back(this->categoryString + "." + name);
#endif

		// check parsing state
		if(!(this->setCategory))
			throw Exception("Module::Config::option(): No category has been set");

		if(this->finished || !(this->currentCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsString()) {
					std::string str(item.GetString(), item.GetStringLength());

					switch(opt) {
					case Default:
						// get simple string
						target.emplace_back(str);

						break;

					case SQL:
						// check for SQL compatibility
						if(Helper::Strings::checkSQLName(str))
							target.emplace_back(str);
						else if(this->logPtr)
							this->logPtr->emplace(
									"\'" + str + "\' in \'" + this->currentItem.str() + "\'"
									" ignored because it contains invalid characters."
							);

						break;

					case SubURL:
						// convert to sub-URL
						Config::makeSubUrl(str);

						target.emplace_back(str);

						break;

					case URL:
						// convert to URL
						Config::makeUrl(str);

						target.emplace_back(str);

						break;

					default:
						throw Exception("Config::option(): Invalid string parsing option");
					}
				}
				else {
					target.emplace_back();

					if(!item.IsNull() && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not string)."
						);
				}
			}
		}
		else if(this->logPtr)
			this->logPtr->emplace(
					"\'" + this->currentItem.str() + "\'"
					" ignored because of wrong type (not array)."
			);

		// current item is parsed
		this->finished = true;
	}

	// log warning
	inline void Config::warning(const std::string& warning) {
		if(this->logPtr)
			this->logPtr->emplace(warning);
		else
			throw std::logic_error(
					"Config::warning(): No log queue is active."
					" Do not use this function outside of Config::loadConfig()!"
			);
	}

	// parse basic option (can be overwritten by child classes)
	inline void Config::parseBasicOption() {
		this->parseOption();
	}

	// check for sub-URL (starting with /) or cURL-supported URL protocol
	//  adds a slash in the beginning if no protocol was found
	inline void Config::makeSubUrl(std::string& s) {
		if(!s.empty()) {
			if(s.at(0) == '/')
				return;

			if(s.length() > 5) {
				if(s.substr(0, 6) == "ftp://")
					return;

				if(s.substr(0, 6) == "scp://")
					return;

				if(s.substr(0, 6) == "smb://")
					return;

				if(s.length() > 6) {
					if(s.substr(0, 7) == "http://")
						return;

					if(s.substr(0, 7) == "ftps://")
						return;

					if(s.substr(0, 7) == "sftp://")
						return;

					if(s.substr(0, 7) == "file://")
						return;

					if(s.substr(0, 7) == "dict://")
						return;

					if(s.substr(0, 7) == "imap://")
						return;

					if(s.substr(0, 7) == "ldap://")
						return;

					if(s.substr(0, 7) == "pop3://")
						return;

					if(s.substr(0, 7) == "rtmp://")
						return;

					if(s.substr(0, 7) == "rtsp://")
						return;

					if(s.substr(0, 7) == "smbs://")
						return;

					if(s.substr(0, 7) == "smtp://")
						return;

					if(s.substr(0, 7) == "tftp://")
						return;

					if(s.length() > 7) {
						if(s.substr(0, 8) == "https://")
							return;

						if(s.substr(0, 8) == "imaps://")
							return;

						if(s.substr(0, 8) == "ldaps://")
							return;

						if(s.substr(0, 8) == "pop3s://")
							return;

						if(s.substr(0, 8) == "smtps://")
							return;

						if(s.length() > 8) {
							if(s.substr(0, 9) == "gopher://")
								return;

							if(s.substr(0, 9) == "telnet://")
								return;
						}
					}
				}
			}
		}

		s.insert(0, "/");
	}

	// check for URL
	//  adds a slash in the end if none was found after protocol
	inline void Config::makeUrl(std::string& s) {
		if(!s.empty()) {
			auto pos = s.find("://");

			if(pos == std::string::npos)
				pos = 0;
			else
				pos += 3;

			pos = s.find('/', pos);

			if(pos != std::string::npos)
				return;
		}

		s.append(1, '/');
	}

} /* crawlservpp::Module */

#endif /* MODULE_CONFIG_HPP_ */
