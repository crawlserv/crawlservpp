/*
 * Config.hpp
 *
 * Abstract class as base for module-specific configuration classes.
 *
 *  Created on: Jan 7, 2019
 *      Author: ans
 */

#ifndef MODULE_CONFIG_HPP_
#define MODULE_CONFIG_HPP_

#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/ConfigItem.hpp"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <limits>
#include <queue>
#include <string>
#include <vector>

namespace crawlservpp::Module {

	/*
	 * DECLARATION
	 */

	class Config {
		// for convenience
		typedef Struct::ConfigItem ConfigItem;

		typedef std::queue<std::string>& LogQueue;
		typedef std::queue<std::string> * LogPtr;

	public:
		Config();
		virtual ~Config();

		// configuration loader
		void loadConfig(const std::string& configJson, LogQueue& log);

		// sub-class for configuration exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

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
		void option(const std::string& name, short& target);
		void option(const std::string& name, std::vector<short>& target);
		void option(const std::string& name, int& target);
		void option(const std::string& name, std::vector<int>& target);
		void option(const std::string& name, long& target);
		void option(const std::string& name, std::vector<long>& target);
		void option(const std::string& name, unsigned char& target);
		void option(const std::string& name, std::vector<unsigned char>& target);
		void option(const std::string& name, unsigned short& target);
		void option(const std::string& name, std::vector<unsigned short>& target);
		void option(const std::string& name, unsigned int& target);
		void option(const std::string& name, std::vector<unsigned int>& target);
		void option(const std::string& name, unsigned long& target);
		void option(const std::string& name, std::vector<unsigned long>& target);
		void option(const std::string& name, std::string &target, StringParsingOption opt = Default);
		void option(const std::string& name, std::vector<std::string>& target, StringParsingOption opt = Default);
		void warning(const std::string& warning);

	protected:
		// module-specific parsing functions
		virtual void parseOption() = 0;
		virtual void checkOptions() = 0;

	private:
		// temporary variables for configuration parsing
		ConfigItem currentItem;			// current configuration item
		bool setCategory;				// category has been set
		bool foundCategory;				// category has been found
		bool finished;					// item has been found
		LogPtr logPtr;					// pointer to logging queue

		// private helper function
		static void makeSubUrl(std::string& s);

	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor stub
	inline Config::Config() :	setCategory(false),
								foundCategory(false),
								finished(false),
								logPtr(nullptr) {}

	// destructor stub
	inline Config::~Config() {}

	// load configuration
	inline void Config::loadConfig(const std::string& configJson, std::queue<std::string>& warningsTo) {
		// save pointer to logging queue
		this->logPtr = &warningsTo;

		// parse JSON
		rapidjson::Document json;
		if(json.Parse(configJson.c_str()).HasParseError())
			throw Config::Exception("Module::Config::loadConfig(): Could not parse configuration JSON.");

		if(!json.IsArray())
			throw Config::Exception("Module::Config::loadConfig(): Invalid configuration JSON (is no array).");

		// parse configuration entries
		for(auto entry = json.Begin(); entry != json.End(); entry++) {
			// check whether configuration item is a JSON object
			if(!(entry->IsObject())) {
				warningsTo.emplace("Configuration entry that is no object ignored.");
				return;
			}

			// go through all members of the JSON object and find its properties (name, category, value)
			bool empty = true;
			bool ignore = false;
			for(auto member = entry->MemberBegin(); member != entry->MemberEnd(); ++member) {
				// check the name of the current item member
				if(member->name.IsString()) {
					std::string memberName(
							member->name.GetString(), member->name.GetStringLength()
					);

					if(memberName == "cat") {
						// found the category of the configuration item
						if(member->value.IsString()) {
							this->currentItem.category =
									member->value.GetString(), member->value.GetStringLength();
						}
						else {
							warningsTo.emplace("Configuration entry with invalid category name ignored.");
							ignore = true;
							break;
						}
					}
					else if(memberName == "name") {
						// found the name of the configuration item
						if(member->value.IsString()) {
							this->currentItem.name = std::string(
									member->value.GetString(), member->value.GetStringLength()
							);
						}
						else {
							warningsTo.emplace("Configuration entry with invalid option name ignored.");
							ignore = true;
							break;
						}

					}
					else if(memberName == "value") {
						// found the value of the configuration item
						this->currentItem.value = &(member->value);
						empty = false;
					}
					else {
						// found an unknown member of the configuration member
						if(memberName.empty()) {
							warningsTo.emplace("Unnamed configuration entry member ignored.");
						}
						else
							warningsTo.emplace(
									"Unknown configuration entry member \'" + memberName + "\' ignored."
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
			this->parseOption();

			// check whether category and item were found
			if(!this->finished) {
				if(this->foundCategory)
					this->logPtr->emplace(
							"Unknown configuration entry"
							" \'" + this->currentItem.str() + "\' ignored."
					);
				else
					this->logPtr->emplace(
							"Configuration entry with unknown category"
							" \'" + this->currentItem.category + "\' ignored."
					);
			}

			// update parsing status
			this->currentItem = ConfigItem();
			this->foundCategory = false;
			this->finished = false;
		}

		// check options
		this->checkOptions();
	}

	// set category of configuration items to parse
	inline void Config::category(const std::string& category) {
		// check parsing state
		if(this->finished)
			return;

		// update parsing state
		this->setCategory = true;

		// compare category with current configuration item
		this->foundCategory = this->currentItem.category == category;
	}

	// check for a configuration option (bool)
	inline void Config::option(const std::string& name, bool& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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

	// check for a configuration option (array of bools)
	inline void Config::option(const std::string& name, std::vector<bool>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsBool())
					target.push_back(i->GetBool());
				else {
					target.push_back(false);

					if(!(i->IsNull()) && this->logPtr)
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

	// check for a configuration option (char)
	inline void Config::option(const std::string& name, char& target, CharParsingOption opt) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		switch(opt) {
		case FromNumber:
			// get from number
			if(this->currentItem.value->IsInt()) {
				int value = this->currentItem.value->GetInt();

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
						std::string(this->currentItem.value->GetString(),
						this->currentItem.value->GetStringLength())
				);
			else
				this->logPtr->emplace(
						"\'" + this->currentItem.str() + "\'"
						" ignored because of wrong type (not string)."
				);
			break;

		default:
			throw Config::Exception("Config::option(): Invalid string parsing option");
		}

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of chars)
	inline void Config::option(const std::string& name, std::vector<char>& target, CharParsingOption opt) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				switch(opt) {

				case FromNumber:
					// get from number
					if(i->IsInt()) {
						int value = i->GetInt();

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

						if(!(i->IsNull()) && this->logPtr)
							this->logPtr->emplace(
									"Value in \'" + this->currentItem.str() + "\'"
									" ignored because of wrong type (not int)."
							);
					}
					break;

				case FromString:
					// get from string
					if(i->IsString())
						target.push_back(
								Helper::Strings::getFirstOrEscapeChar(
										std::string(i->GetString(), i->GetStringLength())
								)
						);
					else {
						target.push_back(0);

						if(!(i->IsNull()) && this->logPtr)
							this->logPtr->emplace(
									"Value in \'" + this->currentItem.str() + "\'"
									" ignored because of wrong type (not string)."
							);
					}
					break;

				default:
					throw Config::Exception("Config::option(): Invalid string parsing option");
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

	// check for a configuration option (short)
	inline void Config::option(const std::string& name, short& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsInt()) {
			int value = this->currentItem.value->GetInt();

			if(value > std::numeric_limits<short>::max())
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

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of shorts)
	inline void Config::option(const std::string& name, std::vector<short>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsInt()) {
					int value = i->GetInt();

					if(value > std::numeric_limits<short>::max())
						this->logPtr->emplace(
							"Value in \'" + this->currentItem.str() + "\'"
							" ignored because the number is too large."
						);
					else
						target.push_back(value);
				}
				else {
					target.push_back(0);

					if(!(i->IsNull()) && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not int)."
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

	// check for a configuration option (int)
	inline void Config::option(const std::string& name, int& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
					" ignored because of wrong type (not int)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of ints)
	inline void Config::option(const std::string& name, std::vector<int>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsInt())
					target.push_back(i->GetInt());
				else {
					target.push_back(0);

					if(!(i->IsNull()) && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not int)."
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

	// check for a configuration option (long)
	inline void Config::option(const std::string& name, long& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
					" ignored because of wrong type (not long)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of longs)
	inline void Config::option(const std::string& name, std::vector<long>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsInt64())
					target.push_back(i->GetInt64());
				else {
					target.push_back(0);

					if(!(i->IsNull()) && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not long)."
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

	// check for a configuration option (unsigned char)
	inline void Config::option(const std::string& name, unsigned char& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
					" ignored because of wrong type (not unsigned int)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of unsigned chars)
	inline void Config::option(const std::string& name, std::vector<unsigned char>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsUint()) {
					unsigned int value = i->GetUint();

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

					if(!(i->IsNull()) && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned int)."
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

	// check for a configuration option (unsigned short)
	inline void Config::option(const std::string& name, unsigned short& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsUint()) {
			unsigned int value = this->currentItem.value->GetUint();

			if(value > std::numeric_limits<unsigned short>::max())
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
					" ignored because of wrong type (not unsigned int)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of unsigned shorts)
	inline void Config::option(const std::string& name, std::vector<unsigned short>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsUint()) {
					unsigned int value = i->GetUint();

					if(value > std::numeric_limits<unsigned short>::max())
						this->logPtr->emplace(
							"Value in \'" + this->currentItem.str() + "\'"
							" ignored because the number is too large."
						);
					else
						target.push_back(value);
				}
				else {
					target.push_back(0);

					if(!(i->IsNull()) && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned int)."
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

	// check for a configuration option (unsigned int)
	inline void Config::option(const std::string& name, unsigned int& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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

	// check for a configuration option (array of unsigned ints)
	inline void Config::option(const std::string& name, std::vector<unsigned int>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsUint())
					target.push_back(i->GetUint());
				else {
					target.push_back(0);

					if(!(i->IsNull()) && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned int)."
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

	// check for a configuration option (unsigned long)
	inline void Config::option(const std::string& name, unsigned long& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
					" ignored because of wrong type (not unsigned int)."
			);

		// current item is parsed
		this->finished = true;
	}

	// check for a configuration option (array of unsigned longs)
	inline void Config::option(const std::string& name, std::vector<unsigned long>& target) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsUint64())
					target.push_back(i->GetUint64());
				else {
					target.push_back(0);

					if(!(i->IsNull()) && this->logPtr)
						this->logPtr->emplace(
								"Value in \'" + this->currentItem.str() + "\'"
								" ignored because of wrong type (not unsigned long)."
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

	// check for a configuration option (string)
	inline void Config::option(const std::string& name, std::string &target, StringParsingOption opt) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
			return;

		// compare current configuration item with defined option
		if(this->currentItem.name != name)
			return;

		// check value type
		if(this->currentItem.value->IsString()) {
			std::string str(
					this->currentItem.value->GetString(), this->currentItem.value->GetStringLength()
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

			default:
				throw Config::Exception("Config::option(): Invalid string parsing option");
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

	// check for a configuration option (array of strings)
	inline void Config::option(const std::string& name, std::vector<std::string>& target, StringParsingOption opt) {
		// check parsing state
		if(!(this->setCategory))
			throw Config::Exception("Module::Config::option(): No category has been set");
		if(this->finished || !(this->foundCategory))
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
			for(auto i = this->currentItem.value->Begin(); i != this->currentItem.value->End(); ++i) {
				if(i->IsString()) {
					std::string str(i->GetString(), i->GetStringLength());

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

					default:
						throw Config::Exception("Config::option(): Invalid string parsing option");
					}
				}
				else {
					target.emplace_back();

					if(!(i->IsNull()) && this->logPtr)
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
	}

	// check for sub-URL (starting with /) or curl-supported URL protocol
	//  adds a slash in the beginning if no protocol was found
	inline void Config::makeSubUrl(std::string& s) {
		if(!s.empty()) {
			if(s.at(0) == '/')
				return;

			if(s.length() > 5) {
				if(s.substr(0, 6) == "ftp://") return;
				if(s.substr(0, 6) == "scp://") return;
				if(s.substr(0, 6) == "smb://") return;

				if(s.length() > 6) {
					if(s.substr(0, 7) == "http://") return;
					if(s.substr(0, 7) == "ftps://") return;
					if(s.substr(0, 7) == "sftp://") return;
					if(s.substr(0, 7) == "file://") return;
					if(s.substr(0, 7) == "dict://") return;
					if(s.substr(0, 7) == "imap://") return;
					if(s.substr(0, 7) == "ldap://") return;
					if(s.substr(0, 7) == "pop3://") return;
					if(s.substr(0, 7) == "rtmp://") return;
					if(s.substr(0, 7) == "rtsp://") return;
					if(s.substr(0, 7) == "smbs://") return;
					if(s.substr(0, 7) == "smtp://") return;
					if(s.substr(0, 7) == "tftp://") return;

					if(s.length() > 7) {
						if(s.substr(0, 8) == "https://") return;
						if(s.substr(0, 8) == "imaps://") return;
						if(s.substr(0, 8) == "ldaps://") return;
						if(s.substr(0, 8) == "pop3s://") return;
						if(s.substr(0, 8) == "smtps://") return;

						if(s.length() > 8) {
							if(s.substr(0, 9) == "gopher://") return;
							if(s.substr(0, 9) == "telnet://") return;
						}
					}
				}
			}
		}

		s.insert(0, "/");
	}

} /* crawlservpp::Module */

#endif /* MODULE_CONFIG_HPP_ */
