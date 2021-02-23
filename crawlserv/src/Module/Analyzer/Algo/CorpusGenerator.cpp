/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * CorpusGenerator.cpp
 *
 * This algorithm uses the built-in functionality for building text
 *  corpora from its input data.
 *
 * Additionally, it writes some basic statistics (number and length of
 *  words and sentences) to the target table.
 *
 *  Created on: Mar 5, 2020
 *      Author: ans
 */

#include "CorpusGenerator.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	CorpusGenerator::CorpusGenerator(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	) : Module::Analyzer::Thread(
				dbBase,
				threadOptions,
				threadStatus
			) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	//! Starts a new algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&)
	 */
	CorpusGenerator::CorpusGenerator(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED GETTER
	 */

	//! Returns the name of the algorithm.
	/*!
	 * \returns A string view containing the
	 *   name of the implemented algorithm.
	 */
	std::string_view CorpusGenerator::getName() const {
		return "CorpusGenerator";
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Initializes the target table for the algorithm.
	/*!
	 * \note When this function is called, the prepared
	 *   SQL statements have not yet been initialized.
	 */
	void CorpusGenerator::onAlgoInitTarget() {
		// set target fields
		std::vector<StringString> fields;

		fields.reserve(corpusNumFields);

		fields.emplace_back("source", "TEXT");
		fields.emplace_back("wordcount", "BIGINT UNSIGNED");
		fields.emplace_back("avg_wordlen", "FLOAT");
		fields.emplace_back("med_wordlen", "FLOAT");
		fields.emplace_back("sd2_wordlen", "FLOAT");
		fields.emplace_back("sentencecount", "BIGINT UNSIGNED");
		fields.emplace_back("avg_sentencelen", "FLOAT");
		fields.emplace_back("med_sentencelen", "FLOAT");
		fields.emplace_back("sd2_sentencelen", "FLOAT");

		this->database.setTargetFields(fields);

		// initialize target table
		this->database.initTargetTable(true, true);
	}

	//! Generates the corpus.
	/*!
	 * \note When this function is called, the prepared
	 *   SQL statements have already been initialized.
	 */
	void CorpusGenerator::onAlgoInit() {
		StatusSetter statusSetter(
				"Initializing algorithm...",
				1.F,
				[this](const auto& status) {
					this->setStatusMessage(status);
				},
				[this](const auto progress) {
					this->setProgress(progress);
				},
				[this]() {
					return this->isRunning();
				}
		);

		// check your sources
		this->log(generalLoggingVerbose, "checks sources...");

		this->checkCorpusSources(statusSetter);

		// request text corpus
		this->log(generalLoggingDefault, "gets text corpus...");

		if(!(this->addCorpora(false, statusSetter))) {
			if(this->isRunning()) {
				throw Exception("CorpusGenerator::onAlgoInit(): Corpus is empty");
			}

			return;
		}

		// create corpus statistics
		if(!statusSetter.change("Creating corpus statistics...")) {
			return;
		}

		this->log(generalLoggingDefault, "creates corpus statistics...");

		const auto resultTable{
			this->getTargetTableName()
		};

		for(std::size_t index{}; index < this->corpora.size(); ++index) {
			const auto& corpus{this->corpora[index]};

			// calculate token (word) lengths
			std::vector<std::size_t> tokenLengths;

			tokenLengths.reserve(corpus.getNumTokens());

			const auto& tokens{corpus.getcTokens()};

			for(const auto& token : tokens) {
				if(!token.empty()) {
					tokenLengths.push_back(Helper::Utf8::length(token));
				}
			};

			const auto avgTokenLength{
				Helper::Math::avg<float>(tokenLengths)
			};
			const auto medTokenLength{
				Helper::Math::median<float>(tokenLengths)
			};
			const auto sd2TokenLength{
				Helper::Math::variance<float>(avgTokenLength, tokenLengths)
			};

			Helper::Memory::free(tokenLengths);

			// calculate sentence lengths
			std::vector<std::size_t> sentenceLengths;
			const auto& sentenceMap{corpus.getcSentenceMap()};

			sentenceLengths.reserve(sentenceMap.size());

			for(const auto& sentence : sentenceMap) {
				if(!CorpusGenerator::isSentenceEmpty(sentence, tokens)) {
					sentenceLengths.push_back(sentence.second);
				}
			}

			const auto avgSentenceLength{
				Helper::Math::avg<float>(sentenceLengths)
			};
			const auto medSentenceLength{
				Helper::Math::median<float>(sentenceLengths)
			};
			const auto sd2SentenceLength{
				Helper::Math::variance<float>(avgSentenceLength, sentenceLengths)
			};

			Helper::Memory::free(sentenceLengths);

			// write data to target table
			Data::InsertFieldsMixed data;

			data.columns_types_values.reserve(corpusNumFields);

			data.table = resultTable;

			std::string source;

			switch(this->config.generalInputSources[index]) {
			case generalInputSourcesParsing:
				source = "parsing";

				break;

			case generalInputSourcesExtracting:
				source = "extracting";

				break;

			case generalInputSourcesAnalyzing:
				source = "analyzing";

				break;

			case generalInputSourcesCrawling:
				source = "crawling";

				break;

			default:
				source = "[unknown]";
			}

			source += ".";
			source += this->config.generalInputTables.at(index);
			source += ".";
			source += this->config.generalInputFields.at(index);

			data.columns_types_values.emplace_back(
					"analyzed__source",
					DataType::_string,
					DataValue(source)
			);

			data.columns_types_values.emplace_back(
					"analyzed__wordcount",
					DataType::_uint64,
					DataValue(corpus.getNumTokens())
			);

			data.columns_types_values.emplace_back(
					"analyzed__avg_wordlen",
					DataType::_double,
					DataValue(avgTokenLength)
			);

			data.columns_types_values.emplace_back(
					"analyzed__med_wordlen",
					DataType::_double,
					DataValue(medTokenLength)
			);

			data.columns_types_values.emplace_back(
					"analyzed__sd2_wordlen",
					DataType::_double,
					DataValue(sd2TokenLength)
			);

			data.columns_types_values.emplace_back(
					"analyzed__sentencecount",
					DataType::_uint64,
					DataValue(corpus.getcSentenceMap().size())
			);

			data.columns_types_values.emplace_back(
					"analyzed__avg_sentencelen",
					DataType::_double,
					DataValue(avgSentenceLength)
			);

			data.columns_types_values.emplace_back(
					"analyzed__med_sentencelen",
					DataType::_double,
					DataValue(medSentenceLength)
			);

			data.columns_types_values.emplace_back(
					"analyzed__sd2_sentencelen",
					DataType::_double,
					DataValue(sd2SentenceLength)
			);

			Helper::Memory::free(this->corpora);

			// save results
			this->database.insertCustomData(data);

			this->database.updateTargetTable();

			if(!(this->isRunning())) {
				return;
			}
		}

		/*
		 * NOTE: The status will be saved in-class and not set here, because
		 * 	the parent class will revert to the original status after initialization
		 */

		if(this->corpora.empty()) {
			this->status = "IDLE No corpus created.";
		}
		else {
			this->status = "IDLE Corpus created.";
		}

		// algorithm has finished
		this->finished();
		this->sleep(std::numeric_limits<std::uint64_t>::max());
	}

	//! Sleeps until the thread is terminated.
	/*!
	 * The corpus has already been genererated on
	 *  initialization.
	 *
	 * \sa onAlgoInit
	 */
	void CorpusGenerator::onAlgoTick() {
		// set status
		this->setStatusMessage(this->status);

		// sleep forever (i.e. until the thread is terminated)
		this->sleep(std::numeric_limits<std::uint64_t>::max());
	}

	//! Does nothing.
	void CorpusGenerator::onAlgoPause() {}

	//! Does nothing.
	void CorpusGenerator::onAlgoUnpause() {}

	//! Does nothing.
	void CorpusGenerator::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Does nothing.
	void CorpusGenerator::parseAlgoOption() {}

	//! Does nothing.
	void CorpusGenerator::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the algorithm.
	void CorpusGenerator::resetAlgo() {
		Helper::Memory::free(this->status);
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTION (private)
	 */

	bool CorpusGenerator::isSentenceEmpty(
			const std::pair<std::size_t, std::size_t>& sentence,
			const std::vector<std::string>& tokens
	) {
		for(std::size_t index{sentence.first}; index < TextMapEntry::end(sentence); ++index) {
			if(!tokens.at(index).empty()) {
				return false;
			}
		}

		return true;
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
