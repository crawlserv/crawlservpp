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

// hard-coded debugging constant
#define MODULE_CONFIG_DEBUG

#include "../Helper/Json.hpp"
#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/ConfigItem.hpp"

#include "../_extern/rapidjson/include/rapidjson/document.h"

#ifdef MODULE_CONFIG_DEBUG
#include <algorithm>	// std::adjacent_find, std::sort
#endif

#include <array>		// std::array
#include <cctype>		// ::isascii
#include <cstdint>		// std::int16_t, std::int32_t, std::int64_t, std::uint<8|16|32|64>_t
#include <limits>		// std::numeric_limits
#include <queue>		// std::queue
#include <stdexcept>	// std::logic_error
#include <string>		// std::string
#include <string_view>	// std::string_view, std::string_view_literals
#include <vector>		// std::vector

//! Namespace for the different modules run by threads.
namespace crawlservpp::Module {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Protocols supported by the @c libcurl library.
	inline constexpr std::array protocols{
		"ftp://"sv,
		"scp://"sv,
		"smb://"sv,
		"http://"sv,
		"ftps://"sv,
		"sftp://"sv,
		"file://"sv,
		"dict://"sv,
		"imap://"sv,
		"ldap://"sv,
		"pop3://"sv,
		"rtmp://"sv,
		"rtsp://"sv,
		"smbs://"sv,
		"smtp://"sv,
		"tftp://"sv,
		"https://"sv,
		"imaps://"sv,
		"ldaps://"sv,
		"pop3s://"sv,
		"smtps://"sv,
		"gopher://"sv,
		"telnet://"sv
	};

	///@}

	/*
	 * DECLARATION
	 */

	//! Abstract class as base for module-specific configurations
	/*!
	 * To add an option to a configuration,
	 *  add a call to any of the option()
	 *  functions  (after a new call to
	 *  category(), if necessary) to the
	 *  local parseOption() implementation.
	 *  It will be called for each
	 *  configuration item to be parsed,
	 *  checking whether this item contains
	 *  the specified option and writing
	 *  it to the specified target if it
	 *  does.
	 *
	 *  \note Once a configuration entry has
	 *    been matched, it will be ignored by
	 *    all subsequent option() calls, i.e.
	 *    it cannot be parsed twice.
	 */
	class Config {
		// for convenience
		using JsonException = Helper::Json::Exception;

		using ConfigItem = Struct::ConfigItem;

		using LogQueue = std::queue<std::string>&;
		using LogPtr = std::queue<std::string> *;

	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		Config() = default;

		//! Default destructor.
		virtual ~Config() = default;

		///@}
		///@name Configuration Loader
		///@{

		// configuration loader
		void loadConfig(const std::string& configJson, LogQueue& warningsTo);

		///@}

		//! Class for configuration exceptions.
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		Config(Config&) = delete;

		//! Deleted copy assignment operator.
		Config& operator=(Config&) = delete;

		//! Deleted move constructor.
		Config(Config&&) = delete;

		//! Deleted move assignment operator.
		Config& operator=(Config&&) = delete;

		///@}

protected:
		///@name Parsing Options
		///@{

		//! Options for parsing strings.
		enum StringParsingOption {
			//! Uses a string as it is.
			Default = 0,

			//! Requires a SQL-compatible string.
			SQL,

			//! Converts a string to a sub-URL.
			SubURL,

			//! Converts a string to a URL (without the protocol).
			URL
		};
		
		//! Options for parsing @c char's.
		enum CharParsingOption {
			//! Get @c char by its numeric value.
			FromNumber = 0,

			//! Get @c char from the beginning of a string.
			/*!
			 * Also supports certain escaped characters.
			 *
			 * \sa Helper::Strings::getFirstOrEscapeChar
			 */
			FromString
		};

		///@}
		///@name Configuration Parsing
		///@{

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
		void option(const std::string& name, std::uint8_t& target);
		void option(const std::string& name, std::vector<std::uint8_t>& target);
		void option(const std::string& name, std::uint16_t& target);
		void option(const std::string& name, std::vector<std::uint16_t>& target);
		void option(const std::string& name, std::uint32_t& target);
		void option(const std::string& name, std::vector<std::uint32_t>& target);
		void option(const std::string& name, std::uint64_t& target);
		void option(const std::string& name, std::vector<std::uint64_t>& target);

		//NOLINTNEXTLINE(fuchsia-default-arguments-declarations)
		void option(const std::string& name, std::string &target, StringParsingOption opt = Default);

		//NOLINTNEXTLINE(fuchsia-default-arguments-declarations)
		void option(const std::string& name, std::vector<std::string>& target, StringParsingOption opt = Default);

		void warning(const std::string& warning);

		///@}
		///@name Module-specific Configuration Parsing
		///@{

		virtual void parseBasicOption();

		//! Parses a module-specific configuration option.
		/*!
		 * Needs to be overridden by module-specific
		 *  configuration child classes.
		 */
		virtual void parseOption() = 0;

		//! Checks the module-specific configuration after parsing.
		/*!
		 * Needs to be overridden by module-specific
		 *  configuration child classes.
		 */
		virtual void checkOptions() = 0;

		///@}

	private:
		// temporary variables for configuration parsing
		ConfigItem currentItem;			// current configuration item
		bool setCategory{false};		// category has been set
		bool foundCategory{false};		// category has been found
		bool currentCategory{false};	// category equals current category
		bool finished{false};			// item has been found
		LogPtr logPtr{nullptr};			// pointer to the logging queue

#ifdef MODULE_CONFIG_DEBUG
		bool debug{true};				// options have been debugged
		std::string categoryString;		// category to check as string
		std::vector<std::string> list;	// container to check for duplicates
#endif

		void logQueue(const std::string& entry);

		// private static helper functions
		static void makeSubUrl(std::string& s);
		static void makeUrl(std::string& s);
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Loads a configuration.
	/*!
	 * \param configJson Constant reference to a
	 *   string containing the configuration as
	 *   JSON.
	 * \param warningsTo Reference to a queue to
	 *   which warnings will be added that occur
	 *   during the parsing of the configuration,
	 *   also known as the "logging queue".
	 *
	 * \throws Module::Config::Exception if
	 *   the configuration JSON cannot be
	 *   parsed.
	 */
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
					"Module::Config::loadConfig():"
					" Could not parse configuration JSON - "
					+ std::string(e.view())
			);
		}

		if(!json.IsArray())  {
			throw Exception(
					"Module::Config::loadConfig():"
					" Invalid configuration JSON (is no array)."
			);
		}

		// parse configuration entries
		for(const auto& entry : json.GetArray()) {
			// check whether configuration item is a JSON object
			if(!entry.IsObject()) {
				warningsTo.emplace(
						"Configuration entry that is no object ignored."
				);

				return;
			}

			// go through all members of the JSON object, find its properties (name, category, value)
			bool empty{true};
			bool ignore{false};

			for(const auto& member : entry.GetObject()) {
				// check the name of the current item member
				if(member.name.IsString()) {
					const std::string memberName(
							member.name.GetString(),
							member.name.GetStringLength()
					);

					if(memberName == "cat") {
						// found the category of the configuration item
						if(member.value.IsString()) {
							this->currentItem.category = std::string(
									member.value.GetString(),
									member.value.GetStringLength()
							);
						}
						else {
							warningsTo.emplace(
									"Configuration entry with invalid category name ignored."
							);

							ignore = true;

							break;
						}
					}
					else if(memberName == "name") {
						// found the name of the configuration item
						if(member.value.IsString()) {
							this->currentItem.name = std::string(
									member.value.GetString(),
									member.value.GetStringLength()
							);
						}
						else {
							warningsTo.emplace(
									"Configuration entry with invalid option name ignored."
							);

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
						if(memberName.empty()) {
							warningsTo.emplace(
									"Unnamed configuration entry member ignored."
							);
						}
						else {
							warningsTo.emplace(
									"Unknown configuration entry member \'"
									+ memberName
									+ "\' ignored."
							);
						}

						continue;
					}
				}
				else {
					// member has an invalid name (no string)
					warningsTo.emplace(
							"Configuration entry member with invalid name ignored."
					);
				}
			}

			// check whether configuration entry should be ignored
			if(ignore) {
				continue;
			}

			// check configuration item
			if(this->currentItem.category.empty()) {
				if(this->currentItem.name != "_algo") { // ignore algorithm ID
					this->logQueue("Configuration item without category ignored");
				}

				continue;
			}

			if(this->currentItem.name.empty()) {
				this->logQueue("Configuration item without name ignored.");

				continue;
			}

			if(empty) {
				this->logQueue("Configuration entry without value ignored.");

				continue;
			}

			// parse option by child class
			this->parseBasicOption();

#ifdef MODULE_CONFIG_DEBUG
			if(this->debug) {
				// debug options
				std::sort(this->list.begin(), this->list.end());

				const auto it{
					std::adjacent_find(this->list.cbegin(), this->list.cend())
				};

				if(it != this->list.cend()) {
					throw Exception("Duplicate option \'" + *it + "\'");
				}

				this->list.clear();

				this->debug = false;
			}
#endif

			// check whether category and item were found
			if(!this->finished) {
				if(this->foundCategory) {
					this->logQueue(
							"Unknown configuration entry \'"
							+ this->currentItem.str()
							+ "\' ignored."
					);
				}
				else {
					this->logQueue(
							"Configuration entry with unknown category \'"
							+ this->currentItem.category
							+ "\' ignored."
					);
				}
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

	//! Sets the category of the subsequent configuration items to be checked for.
	/*!
	 * \param category Constant reference to a
	 *   string containing the name of the
	 *   category.
	 */
	inline void Config::category(const std::string& category) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->categoryString = category;
		}
#endif

		// check parsing state
		if(this->finished) {
			return;
		}

		// update parsing state
		this->setCategory = true;

		// compare category with current configuration item
		if(this->currentItem.category == category) {
			this->foundCategory = true;
			this->currentCategory = true;
		}
		else {
			this->currentCategory = false;
		}
	}

	//! Checks for a configuration option of type @c bool.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a boolean
	 *   variable into which the value
	 *   of the configuration entry will
	 *   be written if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, bool& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsBool()) {
			target = this->currentItem.value->GetBool();
		}
		else if(this->currentItem.value->IsNull()) {
			target = false;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not bool)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of @c bool's.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   of @c bool's into which the value of
	 *   the configuration entry will be
	 *   written if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<bool>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsBool()) {
					target.push_back(item.GetBool());
				}
				else {
					target.push_back(false);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not bool)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type @c char.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   of the type @c char into which the
	 *   value of the configuration entry
	 *   will be written if it is encountered.
	 * \param opt %Parsing options used for the
	 *   configuration option.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, char& target, CharParsingOption opt) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		switch(opt) {
		case FromNumber:
			// get from number
			if(this->currentItem.value->IsInt()) {
				const auto value{this->currentItem.value->GetInt()};

				if(value > std::numeric_limits<char>::max()) {
					this->logQueue(
							"\'"
							+ this->currentItem.str()
							+ "\' ignored because the number is too large."
					);
				}
				else {
					target = value;
				}
			}
			else if(this->currentItem.value->IsNull()) {
				target = 0;
			}
			else {
				this->logQueue(
						"\'"
						+ this->currentItem.str()
						+ "\' ignored because of wrong type (not int)."
				);
			}

			break;

		case FromString:
			// get from string
			if(this->currentItem.value->IsString()) {
				target = Helper::Strings::getFirstOrEscapeChar(
						std::string(
								this->currentItem.value->GetString(),
								this->currentItem.value->GetStringLength()
						)
				);
			}
			else {
				this->logQueue(
						"\'"
						+ this->currentItem.str()
						+ "\' ignored because of wrong type (not string)."
				);
			}

			break;

		default:
			throw Exception("Config::option(): Invalid string parsing option");
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of @c char's.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   of @c char's into which the value
	 *   of the configuration entry will
	 *   be written if it is encountered.
	 * \param opt %Parsing options used for
	 *   the configuration option.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<char>& target, CharParsingOption opt) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

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
						const auto value{item.GetInt()};

						if(value > std::numeric_limits<char>::max()) {
							this->logQueue(
								"Value in \'"
									+ this->currentItem.str()
									+ "\' ignored because the number is too large."
							);
						}
						else {
							target.push_back(value);
						}
					}
					else {
						target.push_back(0);

						if(!item.IsNull()) {
							this->logQueue(
									"Value in \'"
									+ this->currentItem.str()
									+ "\' ignored because of wrong type (not int)."
							);
						}
					}

					break;

				case FromString:
					// get from string
					if(item.IsString()) {
						target.push_back(
								Helper::Strings::getFirstOrEscapeChar(
										std::string(
												item.GetString(),
												item.GetStringLength()
										)
								)
						);

						// check for valid ASCII
						if(::isascii(target.back()) == 0) {
							this->logQueue(
									"First character of string in \'"
									+ this->currentItem.str()
									+ "\' is no ASCII character: "
									+ std::string(
											item.GetString(),
											item.GetStringLength()
									)
							);
						}
					}
					else {
						target.push_back(0);

						if(!item.IsNull()) {
							this->logQueue(
									"Value in \'"
									+ this->currentItem.str()
									+ "\' ignored because of wrong type (not string)."
							);
						}
					}

					break;

				default:
					throw Exception("Config::option(): Invalid string parsing option");
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type 16-bit integer.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   into which the value of the
	 *   configuration entry will be written
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::int16_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsInt()) {
			const auto value{this->currentItem.value->GetInt()};

			if(value > std::numeric_limits<std::int16_t>::max()) {
				this->logQueue(
						"\'"
						+ this->currentItem.str()
						+ "\' ignored because the number is too large."
				);
			}
			else {
				target = value;
			}
		}
		else if(this->currentItem.value->IsNull()) {
			target = 0;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not integer)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of 16-bit integers.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<std::int16_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsInt()) {
					const auto value{item.GetInt()};

					if(value > std::numeric_limits<std::int16_t>::max()) {
						this->logQueue(
							"Value in \'"
							+ this->currentItem.str()
							+ "\' ignored because the number is too large."
						);
					}
					else {
						target.push_back(value);
					}
				}
				else {
					target.push_back(0);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not integer)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type 32-bit integer.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   into which the value of the
	 *   configuration entry will be written
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::int32_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsInt()) {
			target = this->currentItem.value->GetInt();
		}
		else if(this->currentItem.value->IsNull()) {
			target = 0;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not integer)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of 32-bit integers.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<std::int32_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsInt()) {
					target.push_back(item.GetInt());
				}
				else {
					target.push_back(0);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not integer)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type 64-bit integer.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   into which the value of the
	 *   configuration entry will be written
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::int64_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsInt64()) {
			target = this->currentItem.value->GetInt64();
		}
		else if(this->currentItem.value->IsNull()) {
			target = 0;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not 64-bit integer)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of 64-bit integers.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<std::int64_t>& target) {
		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsInt64()) {
					target.push_back(item.GetInt64());
				}
				else {
					target.push_back(0);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not 64-bit integer)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type unsigned 8-bit integer.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   into which the value of the
	 *   configuration entry will be written
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::uint8_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsUint()) {
			const auto value{this->currentItem.value->GetUint()};

			if(value > std::numeric_limits<std::uint8_t>::max()) {
				this->logQueue(
						"\'"
						+ this->currentItem.str()
						+ "\' ignored because the number is too large."
				);
			}
			else {
				target = value;
			}
		}
		else if(this->currentItem.value->IsNull()) {
			target = 0;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not unsigned integer)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of unsigned 8-bit integers.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<std::uint8_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set");
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint()) {
					const auto value{item.GetUint()};

					if(value > std::numeric_limits<std::uint8_t>::max()) {
						this->logQueue(
							"Value in \'"
							+ this->currentItem.str()
							+ "\' ignored because the number is too large."
						);
					}
					else {
						target.push_back(value);
					}
				}
				else {
					target.push_back(0);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not unsigned integer)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type unsigned 16-bit integer.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   into which the value of the
	 *   configuration entry will be written
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::uint16_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsUint()) {
			const auto value{this->currentItem.value->GetUint()};

			if(value > std::numeric_limits<std::uint16_t>::max()) {
				this->logQueue(
						"\'"
						+ this->currentItem.str()
						+ "\' ignored because the number is too large."
				);
			}
			else {
				target = value;
			}
		}
		else if(this->currentItem.value->IsNull()) {
			target = 0;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not unsigned integer)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of unsigned 16-bit integers.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<std::uint16_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint()) {
					const auto value{item.GetUint()};

					if(value > std::numeric_limits<std::uint16_t>::max()) {
						this->logQueue(
							"Value in \'"
							+ this->currentItem.str()
							+ "\' ignored because the number is too large."
						);
					}
					else {
						target.push_back(value);
					}
				}
				else {
					target.push_back(0);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not unsigned integer)."
						);
					}
				}
			}
		}
		else  {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type unsigned 32-bit integer.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   into which the value of the
	 *   configuration entry will be written
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::uint32_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception("Module::Config::option(): No category has been set");
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsUint()) {
			target = this->currentItem.value->GetUint();
		}
		else if(this->currentItem.value->IsNull()) {
			target = 0;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not unsigned integer)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of unsigned 32-bit integers.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<std::uint32_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception("Module::Config::option(): No category has been set");
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint()) {
					target.push_back(item.GetUint());
				}
				else {
					target.push_back(0);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not unsigned integer)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type unsigned 64-bit integer.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a variable
	 *   into which the value of the
	 *   configuration entry will be written
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::uint64_t& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception("Module::Config::option(): No category has been set");
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsUint64()) {
			target = this->currentItem.value->GetUint64();
		}
		else if(this->currentItem.value->IsNull()) {
			target = 0;
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not unsigned 64-bit integer)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of unsigned 64-bit integers.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::vector<std::uint64_t>& target) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception("Module::Config::option(): No category has been set");
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

		// check value type
		if(this->currentItem.value->IsArray()) {
			// clear target and reserve memory
			target.clear();

			target.reserve(this->currentItem.value->Size());

			// check and copy array items
			for(const auto& item : this->currentItem.value->GetArray()) {
				if(item.IsUint64()) {
					target.push_back(item.GetUint64());
				}
				else {
					target.push_back(0);

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not unsigned 64-bit integer)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is finished
		this->finished = true;
	}

	//! Checks for a configuration option of type string.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a string
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 * \param opt %Parsing option for the
	 *   configuration entry.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(const std::string& name, std::string &target, StringParsingOption opt) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

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
				if(Helper::Strings::checkSQLName(str)) {
					target.swap(str);
				}
				else {
					this->logQueue(
							"\'"
							+ str
							+ "\' in \'"
							+ this->currentItem.str()
							+ "\' ignored because it contains invalid characters."
					);
				}

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
		else if(this->currentItem.value->IsNull()) {
			target.clear();
		}
		else  {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not string)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Checks for a configuration option of type array of strings.
	/*!
	 * Ignores the option and adds a
	 *  warning to the warnings queue, if
	 *  the requested type does not match
	 *  the data type in the configuration
	 *  JSON.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   option to check for.
	 * \param target Reference to a vector
	 *   into which the value of the
	 *   configuration entry will be stored
	 *   if it is encountered.
	 * \param opt %Parsing option for the
	 *   configuration entry.
	 *
	 * \throws Module::Config::Exception if
	 *   no category has been set.
	 *
	 * \sa category
	 */
	inline void Config::option(
			const std::string& name,
			std::vector<std::string>& target,
			StringParsingOption opt
	) {
#ifdef MODULE_CONFIG_DEBUG
		if(this->debug) {
			this->list.emplace_back(this->categoryString + "." + name);
		}
#endif

		// check parsing state
		if(!(this->setCategory)) {
			throw Exception(
					"Module::Config::option():"
					" No category has been set"
			);
		}

		if(this->finished || !(this->currentCategory)) {
			return;
		}

		// compare current configuration item with defined option
		if(this->currentItem.name != name) {
			return;
		}

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
						if(Helper::Strings::checkSQLName(str)) {
							target.emplace_back(str);
						}
						else {
							this->logQueue(
									"\'"
									+ str
									+ "\' in \'"
									+ this->currentItem.str()
									+ "\' ignored because it contains invalid characters."
							);
						}

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

					if(!item.IsNull()) {
						this->logQueue(
								"Value in \'"
								+ this->currentItem.str()
								+ "\' ignored because of wrong type (not string)."
						);
					}
				}
			}
		}
		else {
			this->logQueue(
					"\'"
					+ this->currentItem.str()
					+ "\' ignored because of wrong type (not array)."
			);
		}

		// current item is parsed
		this->finished = true;
	}

	//! Adds a warning to the logging queue.
	/*!
	 * \warning Should not be used outside of
	 *   Config::loadConfig!
	 *
	 * \param warning Constant reference to a
	 *   string containing the warning.
	 *
	 * \throws Module::Config::Exception if no
	 *   log queue is active.
	 */
	inline void Config::warning(const std::string& warning) {
		if(this->logPtr == nullptr) {
			throw Exception(
					"Config::warning(): No log queue is active."
					" Do not use this function outside of Config::loadConfig()!"
			);
		}

		this->logPtr->emplace(warning);
	}

	//! Parses a basic option.
	/*!
	 * Might be overridden by child classes.
	 *
	 * Can be used by abstract classes to
	 *  add additional configuration entries
	 *  without being the final implementation,
	 *  as in Network::Config.
	 *
	 * \warning Any reimplementation needs
	 *   to call parseOption() for the
	 *   configuration to work properly.
	 *
	 * \sa Network::Config::parseBasicOption
	 */
	inline void Config::parseBasicOption() {
		this->parseOption();
	}

	// add an entry to the logging queue, if available.
	inline void Config::logQueue(const std::string& entry) {
		if(this->logPtr != nullptr) {
			this->logPtr->emplace(entry);
		}
	}

	// check for sub-URL (starting with /) or libcurl-supported URL protocol
	//  adds a slash in the beginning if no protocol was found
	inline void Config::makeSubUrl(std::string& s) {
		if(!s.empty()) {
			if(s.at(0) == '/') {
				return;
			}

			for(const auto protocol : protocols) {
				if(s.length() >= protocol.length()) {
					if(s.substr(0, protocol.length()) == protocol) {
						return;
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
			auto pos{s.find("://")};

			if(pos == std::string::npos) {
				pos = 0;
			}
			else {
				pos += 3;
			}

			pos = s.find('/', pos);

			if(pos != std::string::npos) {
				return;
			}
		}

		s.append(1, '/');
	}

} /* namespace crawlservpp::Module */

#endif /* MODULE_CONFIG_HPP_ */
