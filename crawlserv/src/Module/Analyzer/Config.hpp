/*
 * Config.h
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

#include "../../Helper/DateTime.hpp"
#include "../../Helper/Strings.hpp"

#include <algorithm>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Module::Analyzer {

	/*
	 * DECLARATION
	 */

	class Config : public Module::Config {
	public:
		Config();
		virtual ~Config();

		// general entries
		std::vector<std::string> generalInputFields;
		std::vector<unsigned short> generalInputSources;
		static const unsigned short generalInputSourcesParsing = 0;
		static const unsigned short generalInputSourcesExtracting = 1;
		static const unsigned short generalInputSourcesAnalyzing = 2;
		static const unsigned short generalInputSourcesCrawling = 3;
		std::vector<std::string> generalInputTables;
		unsigned short generalLogging;
		static const unsigned short generalLoggingSilent = 0;
		static const unsigned short generalLoggingDefault = 1;
		static const unsigned short generalLoggingExtended = 2;
		static const unsigned short generalLoggingVerbose = 3;
		std::string generalResultTable;
		unsigned long generalSleepMySql;
		unsigned long generalSleepWhenFinished;
		unsigned long generalTimeoutTargetLock;

		// markov-text entries
		unsigned char markovTextDimension;
		unsigned long markovTextLength;
		unsigned long markovTextMax;
		std::string markovTextResultField;
		std::string markovTextSourcesField;
		unsigned long markovTextSleep;
		bool markovTextTiming;

		// markov-tweet entries
		unsigned char markovTweetDimension;
		std::string markovTweetLanguage;
		unsigned long markovTweetLength;
		unsigned long markovTweetMax;
		std::string markovTweetResultField;
		std::string markovTweetSourcesField;
		unsigned long markovTweetSleep;
		bool markovTweetSpellcheck;
		bool markovTweetTiming;

		// filter by date entries
		bool filterDateEnable;
		std::string filterDateFrom;
		std::string filterDateTo;

	protected:
		// configuration functions
		void loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) override;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set default values
	inline Config::Config() :	generalLogging(Config::generalLoggingDefault),
								generalSleepMySql(20),
								generalSleepWhenFinished(1000),
								generalTimeoutTargetLock(30),

								markovTextDimension(3),
								markovTextLength(400),
								markovTextMax(0),
								markovTextResultField("text"),
								markovTextSourcesField("sources"),
								markovTextSleep(0),
								markovTextTiming(true),

								markovTweetDimension(5),
								markovTweetLanguage("en_US"),
								markovTweetLength(140),
								markovTweetMax(0),
								markovTweetResultField("tweet"),
								markovTweetSourcesField("sources"),
								markovTweetSleep(0),
								markovTweetSpellcheck(true),
								markovTweetTiming(true),

								filterDateEnable(false) {}

	// destructor stub
	inline Config::~Config() {}

	// load analyzing-specific configuration from parsed JSON document
	inline void Config::loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) {
		// set logging queue
		this->setLog(warningsTo);

		// parse configuration entries
		for(auto entry = jsonDocument.Begin(); entry != jsonDocument.End(); entry++) {
			this->begin(entry);

			this->category("general");
			this->option("input.fields", this->generalInputFields, StringParsingOption::SQL);
			this->option("input.sources", this->generalInputSources);
			this->option("input.tables", this->generalInputTables, StringParsingOption::SQL);
			this->option("logging", this->generalLogging);
			this->option("result.table", this->generalResultTable, StringParsingOption::SQL);
			this->option("sleep.mysql", this->generalSleepMySql);
			this->option("sleep.when.finished", this->generalSleepWhenFinished);
			this->option("timeout.target.lock", this->generalTimeoutTargetLock);

			this->category("markov-tweet");
			this->option("dimension", this->markovTweetDimension);
			this->option("language", this->markovTweetLanguage);
			this->option("length", this->markovTweetLength);
			this->option("max", this->markovTweetMax);
			this->option("result.field", this->markovTweetResultField, StringParsingOption::SQL);
			this->option("sleep", this->markovTweetSleep);
			this->option("sources.field", this->markovTweetSourcesField, StringParsingOption::SQL);
			this->option("spellcheck", this->markovTweetSpellcheck);
			this->option("timing", this->markovTweetTiming);

			this->category("markov-text");
			this->option("dimension", this->markovTextDimension);
			this->option("length", this->markovTextLength);
			this->option("max", this->markovTextMax);
			this->option("result.field", this->markovTextResultField, StringParsingOption::SQL);
			this->option("sleep", this->markovTextSleep);
			this->option("sources.field", this->markovTextSourcesField, StringParsingOption::SQL);
			this->option("timing", this->markovTextTiming);

			this->category("filter-date");
			this->option("enable", this->filterDateEnable);
			this->option("from", this->filterDateFrom);
			this->option("to", this->filterDateTo);

			this->end();
		}

		// check properties of inputs (arrays with fields, sources and tables should have the same number of elements)
		unsigned long completeInputs = std::min({ // number of complete inputs (= minimum size of all arrays)
				this->generalInputFields.size(),
				this->generalInputSources.size(),
				this->generalInputTables.size()
		});

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
			warningsTo.emplace("\'input.fields\', \'.sources\' and \'.tables\' should have the same number of elements.");
			warningsTo.emplace("Incomplete inputs removed.");
		}
	}

} /* crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_CONFIG_HPP_ */
