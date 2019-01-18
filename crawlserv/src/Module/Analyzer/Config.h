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

#ifndef MODULE_ANALYZER_CONFIG_H_
#define MODULE_ANALYZER_CONFIG_H_

#include "../Config.h"

#include "../../Helper/Algos.h"
#include "../../Helper/DateTime.h"
#include "../../Helper/Strings.h"

#include "../../_extern/date.h"

#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Module::Analyzer {
	class Config : public crawlservpp::Module::Config {
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
		void loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) override;
	};
}

#endif /* MODULE_ANALYZER_CONFIG_H_ */
