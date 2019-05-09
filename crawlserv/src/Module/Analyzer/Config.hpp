/*
 * Config.hpp
 *
 * Analyzing configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/analyzer.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_CONFIG_HPP_
#define MODULE_ANALYZER_CONFIG_HPP_

#include "../Config.hpp"

#include <algorithm>	// std::min
#include <queue>		// std::queue
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer {

	/*
	 * DECLARATION
	 */

	class Config : public Module::Config {
	public:
		Config() {};
		virtual ~Config() {};

		// configuration constants
		static const unsigned char generalInputSourcesParsing = 0;
		static const unsigned char generalInputSourcesExtracting = 1;
		static const unsigned char generalInputSourcesAnalyzing = 2;
		static const unsigned char generalInputSourcesCrawling = 3;

		static const unsigned char generalLoggingSilent = 0;
		static const unsigned char generalLoggingDefault = 1;
		static const unsigned char generalLoggingExtended = 2;
		static const unsigned char generalLoggingVerbose = 3;

		// configuration entries
		struct Entries {
			// constructor with default values
			Entries();

			// general entries
			std::vector<std::string> generalInputFields;
			std::vector<unsigned char> generalInputSources;

			std::vector<std::string> generalInputTables;
			unsigned char generalLogging;
			std::string generalResultTable;
			unsigned long generalSleepMySql;
			unsigned long generalSleepWhenFinished;
			unsigned long generalTimeoutTargetLock;

			// filter by date entries
			bool filterDateEnable;
			std::string filterDateFrom;
			std::string filterDateTo;
		} config;

	protected:
		// analyzing-specific configuration parsing
		void parseBasicOption() override;
		void checkOptions() override;

		// algo-specific configuration parsing
		virtual void parseAlgoOption() = 0;
		virtual void checkAlgoOptions() = 0;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set default values
	inline Config::Entries::Entries() :	generalLogging(Config::generalLoggingDefault),
										generalSleepMySql(20),
										generalSleepWhenFinished(5000),
										generalTimeoutTargetLock(30),

										filterDateEnable(false) {}

	// parse analyzing-specific configuration option
	inline void Config::parseBasicOption() {
		// general options
		this->category("general");
		this->option("input.fields", this->config.generalInputFields, StringParsingOption::SQL);
		this->option("input.sources", this->config.generalInputSources);
		this->option("input.tables", this->config.generalInputTables, StringParsingOption::SQL);
		this->option("logging", this->config.generalLogging);
		this->option("result.table", this->config.generalResultTable, StringParsingOption::SQL);
		this->option("sleep.mysql", this->config.generalSleepMySql);
		this->option("sleep.when.finished", this->config.generalSleepWhenFinished);
		this->option("timeout.target.lock", this->config.generalTimeoutTargetLock);

		// filter by date options
		this->category("filter-date");
		this->option("enable", this->config.filterDateEnable);
		this->option("from", this->config.filterDateFrom);
		this->option("to", this->config.filterDateTo);

		// parse algo options
		this->parseAlgoOption();
	}

	// check analyzing-specific configuration
	inline void Config::checkOptions() {
		// check properties of inputs (arrays with fields, sources and tables should have the same number of elements)
		const unsigned long completeInputs = std::min({ // number of complete inputs (= minimum size of all arrays)
				this->config.generalInputFields.size(),
				this->config.generalInputSources.size(),
				this->config.generalInputTables.size()
		});

		bool incompleteInputs = false;

		if(this->config.generalInputFields.size() > completeInputs) {
			// remove queries of incomplete datetime queries
			this->config.generalInputFields.resize(completeInputs);

			incompleteInputs = true;
		}

		if(this->config.generalInputSources.size() > completeInputs) {
			// remove queries of incomplete datetime queries
			this->config.generalInputSources.resize(completeInputs);

			incompleteInputs = true;
		}

		if(this->config.generalInputTables.size() > completeInputs) {
			// remove sources of incomplete datetime queries
			this->config.generalInputTables.resize(completeInputs);

			incompleteInputs = true;
		}

		if(incompleteInputs) {
			// warn about incomplete inputs
			this->warning(
					"\'input.fields\', \'.sources\' and \'.tables\'"
					" should have the same number of elements."
			);
			this->warning("Incomplete inputs removed.");
		}

		// check algo options
		this->checkAlgoOptions();
	}

} /* crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_CONFIG_HPP_ */
