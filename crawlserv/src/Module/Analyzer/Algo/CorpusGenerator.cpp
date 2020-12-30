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
 * CorpusGenerator.cpp
 *
 * This algorithm uses the built-in functionality for building text corpora from its input data.
 *
 * Additionally, it writes some basic statistics (number and length of words and sentences)
 *  to the target table.
 *
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
		std::vector<std::string> fields;
		std::vector<std::string> types;

		fields.reserve(numFields);
		types.reserve(numFields);

		fields.emplace_back("source");
		fields.emplace_back("wordcount");
		fields.emplace_back("avgwordlen");
		fields.emplace_back("medwordlen");
		fields.emplace_back("sentencecount");
		fields.emplace_back("avgsentencelen");
		fields.emplace_back("medsentencelen");

		types.emplace_back("TEXT");
		types.emplace_back("BIGINT UNSIGNED");
		types.emplace_back("FLOAT");
		types.emplace_back("FLOAT");
		types.emplace_back("BIGINT UNSIGNED");
		types.emplace_back("FLOAT");
		types.emplace_back("FLOAT");

		this->database.setTargetFields(fields, types);

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

		std::size_t corpora{0};

		const auto resultTable{
			this->getTargetTableName()
		};

		for(std::size_t n{0}; n < this->config.generalInputSources.size(); ++n) {
			if(this->addCorpus(n, statusSetter)) {
				++corpora;
			}

			if(statusSetter.change("Creating corpus statistics...")) {
				const auto& corpus{this->corpora.back()};

				// calculate token (word) lengths
				std::vector<std::size_t> tokenLengths;

				tokenLengths.reserve(corpus.getNumTokens());

				const auto& tokens{corpus.getcTokens()};

				std::for_each(tokens.cbegin(), tokens.cend(), [&tokenLengths](const auto& token) {
					tokenLengths.push_back(Helper::Utf8::length(token));
				});

				const auto avgTokenLength{
					Helper::Math::avg<float>(tokenLengths)
				};
				const auto medTokenLength{
					Helper::Math::median<float>(tokenLengths)
				};

				std::vector<std::size_t>{}.swap(tokenLengths);

				// calculate sentence lengths
				std::vector<std::size_t> sentenceLengths;
				const auto& sentenceMap{corpus.getcSentenceMap()};

				sentenceLengths.reserve(sentenceMap.size());

				std::for_each(sentenceMap.cbegin(), sentenceMap.cend(), [&sentenceLengths](const auto& sentence) {
					sentenceLengths.push_back(sentence.second);
				});

				const auto avgSentenceLength{
					Helper::Math::avg<float>(sentenceLengths)
				};
				const auto medSentenceLength{
					Helper::Math::median<float>(sentenceLengths)
				};

				std::vector<std::size_t>{}.swap(sentenceLengths);

				// write data to target table
				Data::InsertFieldsMixed data;

				data.columns_types_values.reserve(numFields);

				data.table = resultTable;

				std::string source;

				switch(this->config.generalInputSources.at(n)) {
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
				source += this->config.generalInputTables.at(n);
				source += ".";
				source += this->config.generalInputFields.at(n);

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
						"analyzed__avgwordlen",
						DataType::_double,
						DataValue(avgTokenLength)
				);

				data.columns_types_values.emplace_back(
						"analyzed__medwordlen",
						DataType::_double,
						DataValue(medTokenLength)
				);

				data.columns_types_values.emplace_back(
						"analyzed__sentencecount",
						DataType::_uint64,
						DataValue(corpus.getcSentenceMap().size())
				);

				data.columns_types_values.emplace_back(
						"analyzed__avgsentencelen",
						DataType::_double,
						DataValue(avgSentenceLength)
				);

				data.columns_types_values.emplace_back(
						"analyzed__medsentencelen",
						DataType::_double,
						DataValue(medSentenceLength)
				);

				// clear memory
				std::vector<Data::Corpus>{}.swap(this->corpora);

				// save results
				this->database.insertCustomData(data);

				this->database.updateTargetTable();
			}
		}

		/*
		 * NOTE: The status will be saved in-class and not set here, because
		 * 	the parent class will revert to the original status after initialization
		 */

		if(corpora > 0) {
			this->status = "IDLE Corpus created.";
		}
		else {
			this->status = "IDLE No corpus created.";
		}

		// algorithm has finished
		this->finished();
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

	//! Does nothing
	void CorpusGenerator::resetAlgo() {}

} /* namespace crawlservpp::Module::Analyzer::Algo */
