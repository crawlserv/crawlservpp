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
 * CorpusProperties.hpp
 *
 * Corpus properties (source type, table and field).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_CORPUSPROPERTIES_HPP_
#define STRUCT_CORPUSPROPERTIES_HPP_

#include <cstdint>	// std::uint16_t
#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	//! Corpus properties containing the type, table, and column name of its source.
	struct CorpusProperties {
		///@name Properties
		///@{

		//! The type of the source from which the corpus is created (see below).
		/*!
		 * \sa Module::Analyzer::generalInputSourcesParsing,
		 *   Module::Analyzer::generalInputSourcesExtracting,
		 *   Module::Analyzer::generalInputSourcesAnalyzing,
		 *   Module::Analyzer::generalInputSourcesCrawling
		 */
		std::uint16_t sourceType{0};

		//! The name of the table from which the corpus is created.
		std::string sourceTable;

		//! The name of the table column from which the corpus is created.
		std::string sourceColumn;

		//! IDs of manipulators to change whole sentences.
		std::vector<std::uint16_t> sentenceManipulators;

		//! The models used by the sentence manipulators with the same array index.
		std::vector<std::string> sentenceModels;

		//! IDs of manipulators to change single words.
		std::vector<std::uint16_t> wordManipulators;

		//! The models used by the word manipulators with the same array index.
		std::vector<std::string> wordModels;

		//! List of savepoints.
		/*!
		 * Manipulation steps after which the result will
		 *  be stored in the database. Zero means that the
		 *  unmanipulated corpus will be stored. After
		 *  that, the numbering starts with the sentence
		 *  manipulators, and continues with the word
		 *  manipulators.
		 *
		 * Only the unmanipulated corpus will be stored
		 *  by default.
		 */
		std::vector<std::uint16_t> savePoints{0};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		CorpusProperties() = default;

		//! Constructor setting type, table and column name of the corpus source.
		/*!
		 * \param setSourceType The type of the source
		 *   from which the corpus is created (see below).
		 * \param setSourceTable Constant reference to a
		 *   string containing the name of the table from
		 *   which the corpus is created.
		 * \param setSourceColumn Constant reference to a
		 *   string containing the name of the table column
		 *   from which the corpus is created.
		 * \param setSentenceManipulators Constant reference
		 *   to a vector containing the manipulators to be
		 *   applied on each sentence of the corpus.
		 * \param setSentenceModels Constant reference to a
		 *   vector of strings, containing a model for each
		 *   sentence manipulator, or an empty string if no
		 *   model is required by the manipulator.
		 * \param setWordManipulators Constant reference to
		 *   a vector containing the manipulators to be
		 *   applied on each word of the corpus.
		 * \param setWordModels Constant reference to a
		 *   vector of strings, containing a model for each
		 *   word manipulator, or an empty string if no
		 *   model is required by the manipulator.
		 * \param setSavePoints Constant reference to a
		 *   vector containing the save points to be
		 *   generated, each of them counting from zero
		 *   for the unmanipulated corpus, followed by
		 *   the sentence manipulators, followed by the
		 *   word manipulators. For example, if one sentence
		 *   manipulator and one word manipulator are given,
		 *   a save point of @c 0 means saving the
		 *   unmanipulated corpus, @c 1 means saving the
		 *   corpus after running the sentence manipulator,
		 *   and @c 2 means saving the corpus after running
		 *   the word manipulator.
		 *
		 * \sa Module::Analyzer::generalInputSourcesParsing,
		 *   Module::Analyzer::generalInputSourcesExtracting,
		 *   Module::Analyzer::generalInputSourcesAnalyzing,
		 *   Module::Analyzer::generalInputSourcesCrawling,
		 *	 Data::Corpus::sentenceManipNone,
		 *	 Data::Corpus::sentenceManipTagger,
		 *	 Data::Corpus::wordManipNone,
		 *	 Data::Corpus::wordManipPorter2Stemmer,
		 *	 Data::Corpus::wordManipGermanStemmer
		 */
		CorpusProperties(
				std::uint16_t setSourceType,
				const std::string& setSourceTable,
				const std::string& setSourceColumn,
				const std::vector<std::uint16_t>& setSentenceManipulators,
				const std::vector<std::string>& setSentenceModels,
				const std::vector<std::uint16_t>& setWordManipulators,
				const std::vector<std::string>& setWordModels,
				const std::vector<std::uint16_t>& setSavePoints
		) : sourceType(setSourceType),
			sourceTable(setSourceTable),
			sourceColumn(setSourceColumn),
			sentenceManipulators(setSentenceManipulators),
			sentenceModels(setSentenceModels),
			wordManipulators(setWordManipulators),
			wordModels(setWordModels),
			savePoints(setSavePoints) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_CORPUSPROPERTIES_HPP_ */
