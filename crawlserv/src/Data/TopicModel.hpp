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
 * TopicModel.hpp
 *
 * Topic modeller using the Hierarchical Dirichlet Process (HDP) and
 *  Latent Dirichlet Allocation (LDA) algorithms.
 *
 * The former will be used if no fixed number of topics is given,
 *  the latter will be used if a fixed number of topics is given.
 *
 * Using tomoto, the underlying C++ API of tomotopy, see:
 *  https://bab2min.github.io/tomotopy/
 *
 * If you use the HDP topic modelling algorithm, please cite:
 *
 *  Teh, Y. W., Jordan, M. I., Beal, M. J., & Blei, D. M. (2005). Sharing
 *   clusters among related groups: Hierarchical Dirichlet processes.
 *   In Advances in neural information processing systems, 1385–1392.
 *
 *  Newman, D., Asuncion, A., Smyth, P., & Welling, M. (2009). Distributed
 *   algorithms for topic models. Journal of Machine Learning Research,
 *   10 (Aug), 1801–1828.
 *
 * If you use the LDA topic modelling algorithm, please cite:
 *
 *  Blei, D. M., Ng, A. Y., & Jordan, M. I. (2003). Latent dirichlet
 *   allocation. Journal of machine Learning research, 3(Jan), 993–1022.
 *
 *  Newman, D., Asuncion, A., Smyth, P., & Welling, M. (2009). Distributed
 *   algorithms for topic models. Journal of Machine Learning Research,
 *   10 (Aug), 1801–1828.
 *
 * If you use automated topic labeling, please cite:
 *
 *  Mei, Q., Shen, X., & Zhai, C. (2007). Automatic labeling of multinomial
 *   topic models. In Proceedings of the 13th ACM SIGKDD International
 *   Conference on Knowledge Discovery and Data Mining, 490–499.
 *
 *  Created on: Feb 2, 2021
 *      Author: ans
 */

#ifndef DATA_TOPICMODEL_HPP_
#define DATA_TOPICMODEL_HPP_

#include "PickleDict.hpp"

#include "../Helper/FileSystem.hpp"
#include "../Helper/SilentInclude/tomoto.h"
#include "../Helper/Versions.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/TopicModelInfo.hpp"

#include <algorithm>		// std::accumulate, std::find_if, std::max, std::min, std::remove_if
#include <array>			// std::array
#include <cmath>			// std::log
#include <cstdint>			// std::uint8_t, std::uint64_t
#include <cstdlib>			// std::size_t
#include <fstream>			// std::ifstream, std::ofstream
#include <ios>				// std::ios
#include <limits>			// std::numeric_limits
#include <memory>			// std::make_unique, std::unique_ptr
#include <random>			// std::random_device
#include <string>			// std::string, std::to_string
#include <string_view>		// std::string_view, std::string_view_literals
#include <unordered_map>	// std::unordered_map
#include <utility>			// std::move, std::pair
#include <vector>			// std::vector

// macro for calling member functions of different kinds of (pre-compiled) models
//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DATA_TOPICMODEL_CALL(isHdp, isIdf, function, ...) \
	if(isHdp) { \
		if(isIdf) { \
			this->hdpModelIdf->function(__VA_ARGS__); \
		} \
		else { \
			this->hdpModel->function(__VA_ARGS__); \
		} \
	} \
	else { \
		if(isIdf) { \
			this->ldaModelIdf->function(__VA_ARGS__); \
		} \
		else { \
			this->ldaModel->function(__VA_ARGS__); \
		} \
	}

// macro for retrieving a value from different kinds of (pre-compiled) models
//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DATA_TOPICMODEL_RETRIEVE(x, isHdp, isIdf, function, ...) \
	if(isHdp) { \
		if(isIdf) { \
			x = this->hdpModelIdf->function(__VA_ARGS__); \
		} \
		else { \
			x = this->hdpModel->function(__VA_ARGS__); \
		} \
	} \
	else { \
		if(isIdf) { \
			x = this->ldaModelIdf->function(__VA_ARGS__); \
		} \
		else { \
			x = this->ldaModel->function(__VA_ARGS__); \
		} \
	}

// macro for returning a value from different kinds of (pre-compiled) models
//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DATA_TOPICMODEL_RETURN(isHdp, isIdf, function, ...) \
	if(isHdp) { \
		if(isIdf) { \
			return this->hdpModelIdf->function(__VA_ARGS__); \
		} \
		else { \
			return this->hdpModel->function(__VA_ARGS__); \
		} \
	} \
	else { \
		if(isIdf) { \
			return this->ldaModelIdf->function(__VA_ARGS__); \
		} \
		else { \
			return this->ldaModel->function(__VA_ARGS__); \
		} \
	}

namespace crawlservpp::Data {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The name of the HDP model.
	inline constexpr auto hdpModelName{"HDPModel"sv};

	//! The name of the LDA model.
	inline constexpr auto ldaModelName{"LDAModel"sv};

	//! The initial number of topics by default.
	inline constexpr auto defaultNumberOfInitialTopics{2};

	//! The default concentration coeficient of the Dirichlet Process for document-table.
	inline constexpr auto defaultAlpha{0.1F};

	//! The default hyperparameter for the Dirichlet distribution for topic-word.
	inline constexpr auto defaultEta{0.01F};

	//! The default concentration coefficient of the Dirichlet Process for table-topic.
	/*!
	 * Not used by LDA models, i.e. when a fixed
	 *  number of topics is set.
	 */
	inline constexpr auto defaultGamma{0.1F};

	//! The default interval for optimizing the parameters, in iterations.
	inline constexpr auto defaultOptimizationInterval{10};

	//! The beginning of a valid model file containing a LDA (or HDP) model.
	inline constexpr auto modelFileHead{"LDA\0\0"sv};

	//! The number of bytes determining the term weighting scheme in a model file.
	inline constexpr auto modelFileTermWeightingLen{5};

	//! The term weighting scheme ONE as saved in a model file.
	inline constexpr auto modelFileTermWeightingOne{"one\0\0"sv};

	//! The term weighting scheme IDF (tf-idf) as saved in a model file.
	inline constexpr auto modelFileTermWeightingIdf{"idf\0\0"sv};

	//! The tomoto file format as saved in a model file (after model head and term weighting scheme).
	inline constexpr auto modelFileType{"TPTK"sv};

	///@}

	/*
	 * DECLARATION
	 */

	//! Topic modeller.
	/*!
	 * Uses the Hierarchical Dirichlet Process (HDP) and Latent Dirichlet
	 *  Allocation (LDA) algorithms.
	 *
	 * The former will be used if no fixed number of topics is given,
	 *  the latter will be used if a fixed number of topics is given.
	 *
	 * Using tomoto, the underlying C++ API of \c tomotopy, see:
	 *  https://bab2min.github.io/tomotopy/
	 *
	 * If you use the HDP topic modelling algorithm, please cite:
	 *
	 *  Teh, Y. W., Jordan, M. I., Beal, M. J., & Blei, D. M. (2005). Sharing
	 *   clusters among related groups: Hierarchical Dirichlet processes.
	 *   In Advances in neural information processing systems, 1385–1392.
	 *
	 *  Newman, D., Asuncion, A., Smyth, P., & Welling, M. (2009). Distributed
	 *   algorithms for topic models. Journal of Machine Learning Research,
	 *   10 (Aug), 1801–1828.
	 *
	 *  If you use the LDA topic modelling algorithm, please cite:
	 *
	 *  Blei, D.M., Ng, A.Y., & Jordan, M.I. (2003). Latent dirichlet
	 *   allocation. Journal of machine Learning research, 3(Jan), 993–1022.
	 *
	 *  Newman, D., Asuncion, A., Smyth, P., & Welling, M. (2009). Distributed
	 *   algorithms for topic models. Journal of Machine Learning Research,
	 *   10 (Aug), 1801–1828.
	 */
	class TopicModel {
		// for convenience
		using TopicModelInfo = Struct::TopicModelInfo;

		using HDPModel = tomoto::HDPModel<tomoto::TermWeight::one, tomoto::RandGen>;
		using HDPModelIDF = tomoto::HDPModel<tomoto::TermWeight::idf, tomoto::RandGen>;
		using LDAModel = tomoto::LDAModel<tomoto::TermWeight::one, tomoto::RandGen>;
		using LDAModelIDF = tomoto::LDAModel<tomoto::TermWeight::idf, tomoto::RandGen>;
		using PMIExtractor = tomoto::label::PMIExtractor;
		using FoRelevance = tomoto::label::FoRelevance;

	public:
		///@name Getters
		///@{

		[[nodiscard]] std::size_t getNumberOfDocuments() const;
		[[nodiscard]] std::size_t getVocabularySize() const;
		[[nodiscard]] std::size_t getOriginalVocabularySize() const;
		[[nodiscard]] const std::vector<std::string>& getVocabulary() const;
		[[nodiscard]] std::size_t getNumberOfWords() const;
		[[nodiscard]] std::size_t getBurnInIterations() const;
		[[nodiscard]] std::size_t getIterations() const;
		[[nodiscard]] std::size_t getParameterOptimizationInterval() const;
		[[nodiscard]] std::size_t getRandomNumberGenerationSeed() const;
		[[nodiscard]] std::string_view getModelName() const;
		[[nodiscard]] std::string_view getTermWeighting() const;
		[[nodiscard]] std::size_t getDocumentId(const std::string& name) const;
		[[nodiscard]] std::vector<std::string> getRemovedWords() const;
		[[nodiscard]] std::size_t getNumberOfTopics() const;
		[[nodiscard]] std::vector<std::size_t> getTopics() const;
		[[nodiscard]] std::vector<std::pair<std::size_t, std::uint64_t>> getTopicsSorted() const;
		[[nodiscard]] double getLogLikelihoodPerWord() const;
		[[nodiscard]] double getWordEntropy() const;
		[[nodiscard]] std::vector<std::pair<std::string, float>> getTopicTopNWords(
				std::size_t topic,
				std::size_t n
		) const;
		[[nodiscard]] std::vector<std::pair<std::string, float>> getTopicTopNLabels(
				std::size_t topic,
				std::size_t n
		) const;
		[[nodiscard]] std::vector<float> getDocumentTopics(std::size_t documentId) const;
		[[nodiscard]] std::vector<std::vector<float>> getDocumentsTopics(
				const std::vector<std::vector<std::string>>& documents,
				std::size_t maxIterations,
				std::size_t numberOfWorkers
		) const;
		[[nodiscard]] TopicModelInfo getModelInfo() const;

		///@}
		///@name Setters
		///@{

		void setFixedNumberOfTopics(std::size_t k);
		void setUseIdf(bool idf);
		void setBurnInIteration(std::size_t skipIterations);
		void setWordRemoval(
			std::size_t minWordCount,
			std::size_t minCollectionFrequency,
			std::size_t minDocumentFrequency
		);
		void setInitialParameters(
				std::size_t numberOfTopics,
				float alpha,
				float eta,
				float gamma
		);
		void setParameterOptimizationInterval(std::size_t interval);
		void setRandomNumberGenerationSeed(std::size_t newSeed);
		void setLabelingOptions(
				bool activate,
				std::size_t minCf,
				std::size_t minDf,
				std::size_t minLength,
				std::size_t maxLength,
				std::size_t maxCandidates,
				float smoothing,
				float mu,
				std::size_t windowSize
		);

		///@}
		///@name Topic Modelling
		///@{

		void addDocument(
				const std::string& name,
				const std::vector<std::string>& tokens,
				std::size_t firstToken,
				std::size_t numTokens
		);
		void startTraining();
		void train(
				std::size_t iterations,
				std::size_t threads
		);
		void label(std::size_t threads);

		///@}
		///@name Load and Save
		///@{

		std::size_t load(const std::string& fileName);
		std::size_t save(const std::string& fileName, bool full) const;

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}

		//! Class for topic modelling-specific exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// models
		std::unique_ptr<HDPModel> hdpModel;
		std::unique_ptr<HDPModelIDF> hdpModelIdf;
		std::unique_ptr<LDAModel> ldaModel;
		std::unique_ptr<LDAModelIDF> ldaModelIdf;

		// document names
		std::vector<std::string> docNames;

		// state
		bool hasDocs{false};
		bool isPrepared{false};
		std::size_t workersUsed{};

		// settings
		std::size_t fixedNumberOfTopics{};
		bool isUseIdf{false};
		std::size_t numberOfInitialTopics{defaultNumberOfInitialTopics};
		float initialAlpha{defaultAlpha};
		float initialEta{defaultEta};
		float initialGamma{defaultGamma};
		std::size_t seed{std::random_device{}()};
		std::size_t minWordCf{};
		std::size_t minWordDf{};
		std::size_t removeTopNWords{};
		std::size_t optimizationInterval{defaultOptimizationInterval};
		std::string trainedWithVersion{};

		// labeling
		std::unique_ptr<FoRelevance> labeler;
		bool isLabeling{false};
		std::size_t labelingMinCf{};
		std::size_t labelingMinDf{};
		std::size_t labelingMinLength{};
		std::size_t labelingMaxLength{};
		std::size_t labelingMaxCandidates{};
		float labelingSmoothing{};
		float labelingMu{};
		std::size_t labelingWindowSize{};

		// internal helper functions
		void initModel(bool& isHdpTo, bool& isIdfTo);
		[[nodiscard]] std::string dictLookUp(tomoto::Vid wordId) const;

		void checkModel(
				const std::string& function,
				bool& isHdpTo,
				bool& isIdfTo
		) const;
		void checkNoModel(
				const std::string& function,
				const std::string& errorMsg
		) const;
		void checkTrained(const std::string& function) const;
		void checkNotTrained(
				const std::string& function,
				const std::string& errorMsg
		) const;

		[[nodiscard]] const tomoto::Dictionary& getDict(
				bool isHdp,
				bool isIdf
		) const;
		[[nodiscard]] std::size_t getLiveK(bool isIdf) const;
		[[nodiscard]] std::size_t getK(bool isHdp, bool isIdf) const;
		[[nodiscard]] bool isLiveTopic(bool isIdf, std::size_t topic) const;
		[[nodiscard]] float getGamma(bool isIdf) const;
		[[nodiscard]] std::size_t getNumberOfTables(bool isIdf) const;

		void prepareModel(bool isHdp, bool isIdf);
		void trainModel(
				bool isHdp,
				bool isIdf,
				std::size_t iterations,
				std::size_t
				threads
		);
		void loadModelInformation(
				bool isHdp,
				bool isIdf,
				const std::vector<std::uint8_t>& data
		);
		void writeModelInformation(
				bool isHdp,
				bool isIdf,
				std::vector<std::uint8_t>& dataTo
		) const;

		const tomoto::ITopicModel& getModelInterface(bool isHdf, bool isIdf) const;

		// internal static helper functions (definitions only)
		static tomoto::RawDoc createDocument(
				const std::string& name,
				const std::vector<std::string>& tokens,
				std::size_t firstToken,
				std::size_t numTokens
		);
		static void readModelFileHead(std::istream& in, const std::string& fileName);
		static void readModelFileTermWeighting(
				std::istream& in,
				const std::string& fileName,
				bool& isIdfTo
		);
		static void readModelFileType(std::istream& in, const std::string& fileName);
		static void resetStream(std::istream& in);
		static void numberFromDict(
				const PickleDict& dict,
				const std::string& key,
				std::size_t& valueTo
		);
		static void floatFromDict(
				const PickleDict& dict,
				const std::string& key,
				float& valueTo
		);
		static void stringFromDict(
				const PickleDict& dict,
				const std::string& key,
				std::string& valueTo
		);

		// internal static helper functions (constexpr and templates)
		[[nodiscard]] static constexpr std::string_view termWeightToString(bool isIdf) {
			if(isIdf) {
				return "TermWeight.IDF";
			}

			return "TermWeight.ONE";
		}

		template<typename T> [[nodiscard]] static bool bytesEqual(
				const T& bytes,
				std::string_view s
		) {
			if(bytes.size() != s.size()) {
				return false;
			}

			for(std::size_t index{}; index < bytes.size(); ++index) {
				if(bytes[index] != s[index]) {
					return false;
				}
			}

			return true;
		}

		template<typename T> [[nodiscard]] static std::string bytesToString(
				const T& bytes
		) {
			std::string result;

			for(const auto c : bytes) {
				if(c != '\0') {
					result.push_back(c);
				}
			}

			return result;
		}
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * GETTERS
	 */

	//! Gets the number of added documents after training has begun.
	/*!
	 * \returns The number of added documents.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getNumberOfDocuments() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getNumberOfDocuments", isHdp, isIdf);
		this->checkTrained("getNumberOfDocuments");

		if(!(this->isPrepared)) {
			throw Exception(
					"TopicModel::getNumberOfDocuments():"
					" The model has not been trained yet"
			);
		}

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getNumDocs);
	}

	//! Gets the number of distinct words after training has begun.
	/*!
	 * \returns The number of distinct words after stop
	 *   words have been removed.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getVocabularySize() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getVocabularySize", isHdp, isIdf);
		this->checkTrained("getVocabularySize");

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getV);
	}

	//! Gets the number of distinct words before training.
	/*!
	 * \returns The number of distinct words before stop
	 *   words have been removed.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getOriginalVocabularySize() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getOriginalVocabularySize", isHdp, isIdf);
		this->checkTrained("getOriginalVocabularySize");

		return this->getDict(isHdp, isIdf).size();
	}

	//! Gets the complete dictionary used by the model.
	/*!
	 * \note Includes words removed during training.
	 *
	 * \returns Constant reference to a vector of strings
	 *   containing the complete dictionary of the model.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline const std::vector<std::string>& TopicModel::getVocabulary() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getVocabulary", isHdp, isIdf);
		this->checkTrained("getVocabulary");

		return this->getDict(isHdp, isIdf).getRaw();
	}


	//! Gets the number of words after training has begun.
	/*!
	 * \returns The number of words after stop words
	 *   have been removed.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getNumberOfWords() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getNumberOfWords", isHdp, isIdf);
		this->checkTrained("getNumberOfWords");

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getN);
	}

	//! Get the number of skipped iterations.
	/*!
	 * \returns The number of skipped iterations before
	 *   training.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getBurnInIterations() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getBurnInIterations", isHdp, isIdf);
		this->checkTrained("getBurnInIterations");

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getBurnInIteration);
	}

	//! Get the number of training iterations performed so far.
	/*!
	 * \returns The number of iterations performed
	 *   during training.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getIterations() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getIterations", isHdp, isIdf);
		this->checkTrained("getIterations");

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getGlobalStep);
	}

	//! Gets the interval for parameter optimization, in iterations.
	/*!
	 * \returns The interval after which the parameters
	 *   of the model will be optimized, in iterations.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getParameterOptimizationInterval() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getParameterOptimizationInterval", isHdp, isIdf);
		this->checkTrained("getParameterOptimizationInterval");

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getOptimInterval);
	}

	//! Gets the seed used for random number generation.
	/*!
	 * \returns The seed used for random number
	 *   generation by the model.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getRandomNumberGenerationSeed() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getRandomNumberGenerationSeed", isHdp, isIdf);
		this->checkTrained("getRandomNumberGenerationSeed");

		return this->seed;
	}

	//! Gets the name of the current model.
	/*!
	 * \returns A view of a string containing the
	 *   name of the current model.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added or the topic modeller has been
	 *   already cleared, i.e. no model is available.
	 */
	inline std::string_view TopicModel::getModelName() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getModelName", isHdp, isIdf);

		if(isHdp) {
			return hdpModelName;
		}

		return ldaModelName;
	}

	//! Gets the term weighting mode of the current model.
	/*!
	 * \returns A view of a string containing the
	 *   term weighting mode of the current model.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added or the topic modeller has been
	 *   already cleared, i.e. no model is available.
	 */
	inline std::string_view TopicModel::getTermWeighting() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getTermWeighting", isHdp, isIdf);

		return TopicModel::termWeightToString(isIdf);
	}

	//! Gets the ID of the document with the specified name.
	/*!
	 * \param name The name of the document.
	 *
	 * \returns The ID of the document with the specified
	 *   name.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or no document with the specified
	 *   name has been added to the model.
	 */
	inline std::size_t TopicModel::getDocumentId(const std::string& name) const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getDocumentId", isHdp, isIdf);

		std::size_t id{};

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(id, isHdp, isIdf, getDocIdByUid, name);

		if(id == std::numeric_limits<std::size_t>::max()) {
			throw Exception(
					"getDocumentId():"
					" No document named '"
					+ name
					+ "' has been added to the model"
			);
		}

		return id;
	}

	//! Gets the most common (i.e. stop) words that have been removed.
	/*!
	 * \returns A vector of strings containing the
	 *   removed (s)top words. The vector is empty if no
	 *   (s)top words have been removed.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::vector<std::string> TopicModel::getRemovedWords() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getRemovedWords", isHdp, isIdf);
		this->checkTrained("getRemovedWords");

		const auto& dict{
			this->getDict(isHdp, isIdf)
		};
		const auto& size{dict.size()};
		 std::vector<std::string> removed;

		for(std::size_t wordIndex{size - this->removeTopNWords}; wordIndex < size; ++wordIndex) {
			removed.emplace_back(dict.toWord(wordIndex));
		}

		return removed;
	}

	//! Gets the number of topics.
	/*!
	 * \returns Number of topics that are alive after
	 *   training. Returns the fixed number of topics
	 *   (\c k) if it is non-zero, i.e. when the LDA
	 *   algorithm is being used.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::size_t TopicModel::getNumberOfTopics() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getNumberOfTopics", isHdp, isIdf);
		this->checkTrained("getNumberOfTopics");

		if(isHdp) {
			return this->getLiveK(isIdf);
		}

		return this->fixedNumberOfTopics;
	}

	//! Gets the IDs of the topics.
	/*!
	 * \returns Vector with the IDs of the topics that
	 *   are alive after training. Will return
	 *   \c [0,1,...k] if the number of topics is fixed,
	 *   i.e. the LDA algorithm is being used.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::vector<std::size_t> TopicModel::getTopics() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getTopics", isHdp, isIdf);
		this->checkTrained("getTopics");

		std::vector<std::size_t> topicIds;
		std::size_t maxK{};

		if(isHdp) {
			topicIds.reserve(this->getLiveK(isIdf));

			maxK = this->getK(true, isIdf);

			for(std::size_t k{}; k < maxK; ++k) {
				if(this->isLiveTopic(isIdf, k)) {
					topicIds.emplace_back(k);
				}
			}
		}
		else {
			topicIds.reserve(this->fixedNumberOfTopics);

			for(std::size_t k{}; k < this->fixedNumberOfTopics; ++k) {
				topicIds.emplace_back(k);
			}
		}

		return topicIds;
	}

	//! Gets the IDs and counts of the topics, sorted by count.
	/*!
	 * \returns Vector of pairs with the IDs and counts
	 *   of the topics, sorted by the latter.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::vector<std::pair<std::size_t, std::uint64_t>> TopicModel::getTopicsSorted() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getTopicsSorted", isHdp, isIdf);
		this->checkTrained("getTopicsSorted");

		std::vector<std::pair<std::size_t, std::uint64_t>> topics;
		std::vector<std::uint64_t> counts;

		topics.reserve(this->getK(isHdp, isIdf));

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(counts, isHdp, isIdf, getCountByTopic);

		std::size_t topicIndex{};

		for(const auto count : counts) {
			if(!isHdp || this->isLiveTopic(isIdf, topicIndex)) {
				topics.emplace_back(topicIndex, count);
			}

			++topicIndex;
		}

		std::sort(topics.begin(), topics.end(), [](const auto& a, const auto& b) {
			return a.second > b.second;
		});

		return topics;
	}

	//! Gets the log-likelihood per word.
	/*!
	 * \returns The current log-likelihood per word.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline double TopicModel::getLogLikelihoodPerWord() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getLogLikelihoodPerWord", isHdp, isIdf);
		this->checkTrained("getLogLikelihoodPerWord");

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getLLPerWord);
	}

	//! Gets the word entropy after training.
	/*!
	 * \returns The word entropy for the whole corpus.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline double TopicModel::getWordEntropy() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getWordEntropy", isHdp, isIdf);
		this->checkTrained("getWordEntropy");

		std::vector<std::uint64_t> vocabularyFrequencies;
		std::uint64_t vocabularyUsed{};

		// retrieve vocabulary frequencies
		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(vocabularyFrequencies, isHdp, isIdf, getVocabCf);
		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(vocabularyUsed, isHdp, isIdf, getV);

		// sum up for normalization
		const auto frequencySum{
			std::accumulate(
					vocabularyFrequencies.begin(),
					vocabularyFrequencies.begin() + vocabularyUsed,
					std::uint64_t{}
			)
		};

		std::vector<double> normalizedFrequencies;

		normalizedFrequencies.reserve(vocabularyUsed);

		for(
				auto it{vocabularyFrequencies.begin()};
				it < vocabularyFrequencies.begin() + vocabularyUsed;
				++it
		) {
			normalizedFrequencies.push_back(static_cast<double>(*it) / frequencySum);
		}

		return std::accumulate(
				normalizedFrequencies.begin(),
				normalizedFrequencies.end(),
				0.,
				[](double a, double b) {
					return a + b * std::log(b);
				}
		);
	}

	//! Gets the top \c N words for the specified topic.
	/*!
	 * \param topic The ID of the topic.
	 * \param n The number of top words to retrieve
	 *   from the topic, i.e. \c N.
	 *
	 * \returns A vector containing pairs of the top
	 *   \c N words of the specified topic and their
	 *   probabiities, sorted by the latter.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::vector<std::pair<std::string, float>> TopicModel::getTopicTopNWords(
			std::size_t topic,
			std::size_t n
	) const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getTopicTopNWords", isHdp, isIdf);
		this->checkTrained("getTopicTopNWords");

		std::vector<std::pair<tomoto::Vid, float>> wordIds;

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(wordIds, isHdp, isIdf, getWidsByTopicSorted, topic, n);

		std::vector<std::pair<std::string, float>> words;

		words.reserve(n);

		for(const auto& wordId : wordIds) {
			words.emplace_back(this->dictLookUp(wordId.first), wordId.second);
		}

		return words;
	}

	//! Gets the top \c N labels for the specified topic.
	/*!
	 * \param topic The ID of the topic.
	 * \param n The number of labels to retrieve
	 *   from the topic, i.e. \c N.
	 *
	 * \returns A vector containing pairs of the top
	 *   \c N labels of the specified topic and their
	 *   probabiities, sorted by the latter.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, the model has not been trained yet,
	 *   or automated topic labelling has not been
	 *   activated.
	 */
	inline std::vector<std::pair<std::string, float>> TopicModel::getTopicTopNLabels(
			std::size_t topic,
			std::size_t n
	) const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getTopicTopNLabels", isHdp, isIdf);
		this->checkTrained("getTopicTopNLabels");

		if(!(this->labeler)) {
			throw Exception(
					"getTopicTopNLabels():"
					" Automated topic labelling has not been activated"
			);
		}

		return this->labeler->getLabels(topic, n);
	}

	//! Gets the topic distribution for a specific document seen during training.
	/*!
	 * \param documentId The ID of the document, i.e. its
	 *   index (0 referencing the document added first).
	 *
	 * \returns A vector of floating-point numbers
	 *   indicating te topic distribution of the given
	 *   document.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, the model has not been trained yet, or
	 *   the given document does not exist inside the
	 *   model.
	 */
	inline std::vector<float> TopicModel::getDocumentTopics(std::size_t documentId) const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getDocumentTopics", isHdp, isIdf);
		this->checkTrained("getDocumentTopics");

		if(documentId >= this->getNumberOfDocuments()) {
			throw Exception(
					"getDocumentTopics():"
					" Could not find document #"
					+ std::to_string(documentId)
					+ "inside the model (>"
					+ std::to_string(this->getNumberOfDocuments() - 1)
					+ ")"
			);
		}

		// retrieve document
		const tomoto::DocumentBase * documentPtr{nullptr};

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(documentPtr, isHdp, isIdf, getDoc, documentId);

		// retrieve topic distribution for the document
		std::vector<float> result;

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(result, isHdp, isIdf, getTopicsByDoc, *documentPtr);

		return result;
	}

	//! Infers the topic distribution for previously unseen documents.
	/*!
	 * \param documents A constant reference to a
	 *   vector containing vectors with the processed
	 *   tokens/words of the documents to infer the
	 *   topics for.
	 *
	 * \returns A vector containing vectors of floating-
	 *   point numbers indicating the topic distribution
	 *   for each of the given documents.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline std::vector<std::vector<float>> TopicModel::getDocumentsTopics(
			const std::vector<std::vector<std::string>>& documents,
			std::size_t maxIterations,
			std::size_t numberOfWorkers
	) const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getDocumentsTopics", isHdp, isIdf);
		this->checkTrained("getDocumentsTopics");

		// create documents
		std::vector<std::unique_ptr<tomoto::DocumentBase>> docUPtrs(documents.size());
		std::size_t docIndex{};

		for(const auto& tokens : documents) {
			const auto doc{
				TopicModel::createDocument("", tokens, 0, tokens.size())
			};

			//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
			DATA_TOPICMODEL_RETRIEVE(
					docUPtrs[docIndex],
					isHdp,
					isIdf,
					makeDoc,
					doc
			);

			++docIndex;
		}

		// get C-style pointers for underlying API
		std::vector<tomoto::DocumentBase *> docPtrs;

		docPtrs.reserve(documents.size());

		for(auto& uPtr : docUPtrs) {
			docPtrs.push_back(uPtr.get());
		}

		// infer topic distributions for documents
		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_CALL(
				isHdp,
				isIdf,
				infer,
				docPtrs,
				maxIterations,
				-1.F, /* currently not used */
				numberOfWorkers,
				tomoto::ParallelScheme::default_,
				false
		);

		std::vector<std::vector<float>> result(documents.size());
		std::size_t resultIndex{};

		for(const auto * docPtr : docPtrs) {
			//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
			DATA_TOPICMODEL_RETRIEVE(
					result[resultIndex],
					isHdp,
					isIdf,
					getTopicsByDoc,
					*docPtr
			);

			++resultIndex;
		}

		return result;
	}

	//! Gets information about the model after training.
	/*!
	 * \returns A structure containing all available
	 *   information about the trained model.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or the model has not been trained yet.
	 */
	inline Struct::TopicModelInfo TopicModel::getModelInfo() const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("getModelInfo", isHdp, isIdf);
		this->checkTrained("getModelInfo");

		TopicModelInfo information;

		information.modelName = this->getModelName();
		information.modelVersion = Helper::Versions::getTomotoVersion();
		information.numberOfDocuments = this->getNumberOfDocuments();
		information.numberOfWords = this->getNumberOfWords();
		information.sizeOfVocabulary = this->getOriginalVocabularySize();
		information.sizeOfVocabularyUsed = this->getVocabularySize();
		information.wordEntropy = this->getWordEntropy();
		information.removedWords = this->getRemovedWords();
		information.numberOfIterations = this->getIterations();
		information.numberOfBurnInSteps = this->getBurnInIterations();
		information.optimizationInterval = this->getParameterOptimizationInterval();
		information.logLikelihoodPerWord = this->getLogLikelihoodPerWord();
		information.weighting = this->getTermWeighting();
		information.minCollectionFrequency = this->minWordCf;
		information.minDocumentFrequency = this->minWordDf;
		information.numberOfTopWordsToBeRemoved = this->removeTopNWords;
		information.initialAlpha = this->initialAlpha;
		information.initialEta = this->initialEta;
		information.seed = this->seed;
		information.trainedWithVersion = this->trainedWithVersion;
		information.numberOfTopics = this->getNumberOfTopics();

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(information.alpha, isHdp, isIdf, getAlpha);
		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(information.eta, isHdp, isIdf, getEta);

		if(isHdp) {
			information.numberOfInitialTopics = this->numberOfInitialTopics;
			information.gamma = this->getGamma(isIdf);
			information.initialGamma = this->initialGamma;
			information.numberOfTables = this->getNumberOfTables(isIdf);
		}
		else {
			// get alpha for each topic (LDA only)
			information.alphas.reserve(information.numberOfTopics);

			for(std::size_t topic{}; topic < information.numberOfTopics; ++topic) {
				if(isIdf) {
					information.alphas.push_back(this->ldaModelIdf->getAlpha(topic));
				}
				else {
					information.alphas.push_back(this->ldaModel->getAlpha(topic));
				}
			}
		}

		return information;
	}

	/*
	 * SETTERS
	 */

	//! Sets the fixed number of topics.
	/*!
	 * \param k The fixed number of topics, or zero for
	 *   using the HDP algorithm to determine the number
	 *   of topics from the data.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been initialized.
	 */
	inline void TopicModel::setFixedNumberOfTopics(std::size_t k) {
		this->checkNoModel(
				"setFixedNumberOfTopics",
				"Fixed number of topics cannot be set"
		);

		this->fixedNumberOfTopics = k;
	}

	//! Sets whether to use IDF term weighting.
	/*!
	 * \param idf If true, IDF term weighting is used.
	 *   If false, every term is weighted the same.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been initialized.
	 */
	inline void TopicModel::setUseIdf(bool idf) {
		this->checkNoModel(
				"setUseIdf",
				"Term weighting cannot be set to IDF"
		);

		this->isUseIdf = idf;
	}

	//! Sets the number of iterations that will be skipped at the beginnig of training.
	/*!
	 * \param skipIterations The number of iterations to
	 *   be skipped at the beginning of the training.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been traind.
	 */
	inline void TopicModel::setBurnInIteration(std::size_t skipIterations) {
		bool isHdp{false};
		bool isIdf{false};

		this->initModel(isHdp, isIdf);
		this->checkNotTrained(
				"setBurnInIteration",
				"Iterations cannot be burned"
		);

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_CALL(isHdp, isIdf, setBurnInIteration, skipIterations);
	}

	//! Sets which (un)common words to remove before training.
	/*!
	 * \param collectionFrequency The minimum number of
	 *   occurrences in the corpus required for a word
	 *   to be kept. Zero or one to not use this
	 *   criterion.
	 * \param documentFrequency The minimum number of
	 *   documents in which a word is required to occur
	 *   in order to be kept. Zero or one to not use
	 *   this criterion.
	 * \param fixedNumberOfTopWords The number of
	 *   most-occurring words that will be classified as
	 *   stop words and ignored. Zero to not define any
	 *   stop words.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been trained.
	 */
	inline void TopicModel::setWordRemoval(
			std::size_t collectionFrequency,
			std::size_t documentFrequency,
			std::size_t fixedNumberOfTopWords
	) {
		this->checkNotTrained(
				"setWordRemoval",
				"Stop word settings cannot be changed"
		);

		this->minWordCf = collectionFrequency;
		this->minWordDf = documentFrequency;
		this->removeTopNWords = fixedNumberOfTopWords;
	}

	//! Sets the initial parameters for the model.
	/*!
	 * \param numberOfInitialTopics The initial number
	 *   of topics between 2 and 32767. The number of
	 *   topics will be adjusted for the data during
	 *   training, if the HDP algorithm is used. Will be
	 *   ignored if a fixed number of topics is set, i.e.
	 *   the LDA algorithm is used.
	 * \param alpha The initial concentration coeficient
	 *   of the Dirichlet Process for document-table.
	 * \param eta The Dirichlet prior on the per-topic
	 *   word distribution.
	 * \param gamma The initial concentration coeficient
	 *   of Dirichlet Process for table-topic. Will be
	 *   ignored if LDA will be used, i.e. the number of
	 *   fixed topics is non-zero.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been initialized.
	 */
	inline void TopicModel::setInitialParameters(
			std::size_t numberOfInitialTopics,
			float alpha,
			float eta,
			float gamma
	) {
		this->checkNoModel(
				"setInitialParameters",
				"Cannot set initial parameters"
		);

		this->numberOfInitialTopics = numberOfInitialTopics;
		this->initialAlpha = alpha;
		this->initialEta = eta;
		this->initialGamma = gamma;
	}

	//! Sets the interval for parameter optimization, in iterations.
	/*!
	 * \param interval The interval after which the
	 *   parameters of the model will be optimized, in
	 *   iterations.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been initialized.
	 */
	inline void TopicModel::setParameterOptimizationInterval(std::size_t interval) {
		this->checkNoModel(
				"setParameterOptimizationInterval",
				"Cannot set parameter optimization interval"
		);

		this->optimizationInterval = interval;
	}

	//! Sets the seed for random number generation.
	/*!
	 * \param newSeed The seed used by the model for the
	 *   generation of random numbers.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been initialized.
	 */
	inline void TopicModel::setRandomNumberGenerationSeed(std::size_t newSeed) {
		this->checkNoModel(
				"setRandomNumberGenerationSeed",
				"Cannot set seed for random number generation"
		);

		this->seed = newSeed;
	}

	//! Sets the options for automated topic labeling.
	/*!
	 * Re-labels the topics if they have already been
	 *  labeled.
	 *
	 * \param activate Sets whether to activate automated
	 *   topic labeling.
	 * \param maxCandidates Sets the maximum number of
	 *   label candidates to extract from the topics.
	 * \param smoothing A small value greater than zero
	 *   for Laplace smoothing.
	 * \param mu A discriminative coefficient. Candidates
	 *   with a high score on a specific topic and with
	 *   a low score on other topics get a higher final
	 *   score when this value is larger.
	 *
	 * \sa label
	 */
	inline void TopicModel::setLabelingOptions(
			bool activate,
			std::size_t minCf,
			std::size_t minDf,
			std::size_t minLength,
			std::size_t maxLength,
			std::size_t maxCandidates,
			float smoothing,
			float mu,
			std::uint64_t windowSize
	) {
		this->isLabeling = activate;
		this->labelingMinCf = minCf;
		this->labelingMinDf = minDf;
		this->labelingMinLength = minLength;
		this->labelingMaxLength = maxLength;
		this->labelingMaxCandidates = maxCandidates;
		this->labelingSmoothing = smoothing;
		this->labelingMu = mu;
		this->labelingWindowSize = windowSize;

		// re-label if necessary
		if(this->labeler) {
			this->label(this->workersUsed);
		}
	}

	/*
	 * TOPIC MODELLING
	 */

	//! Adds a document from a tokenized corpus.
	/*!
	 * A copy of the document will be created, i.e. the
	 *  corpus can be cleared after all documents have
	 *  been added.
	 *
	 * \param name The name of the document.
	 * \param tokens Constant reference to all tokens
	 *   in the corpus.
	 * \param firstToken Index of the document's first
	 *   token.
	 * \param numTokens Number of tokens in the
	 *   document.
	 *
	 * \throws TopicModel::Exception if the model has
	 *   already been trained.
	 *
	 * \note It is recommended to stem (or lemmatize)
	 *   the words in the document before adding it to
	 *   the model.
	 */
	inline void TopicModel::addDocument(
		const std::string& name,
		const std::vector<std::string>& tokens,
		std::size_t firstToken,
		std::size_t numTokens
	) {
		bool isHdp{false};
		bool isIdf{false};

		this->initModel(isHdp, isIdf);
		this->checkNotTrained(
				"addDocument",
				"Documents cannot be added"
		);

		// add name
		this->docNames.emplace_back(name);

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_CALL(
				isHdp,
				isIdf,
				addDoc,
				TopicModel::createDocument(
						this->docNames.back(),
						tokens,
						firstToken,
						numTokens
				)
		);

		if(!(this->hasDocs)) {
			this->hasDocs = numTokens > 0;
		}
	}

	//! Starts training without performing any iteration.
	/*!
	 * Can be used to retrieve general information about
	 *  the training data afterwards.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, or an invalid word ID is encountered
	 *   while removing stop words.
	 */
	inline void TopicModel::startTraining() {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("startTraining", isHdp, isIdf);
		this->prepareModel(isHdp, isIdf);
		this->trainModel(isHdp, isIdf, 0, 1);
	}

	//! Trains the underlying HLDA model.
	/*!
	 * Training can be performed multiple times, but
	 *  after training has been started no additional
	 *  documents can be added to the model.
	 *
	 * \param iterations The number of iterations for
	 *   modelling the topics.
	 * \param threads Number of threads. One for single
	 *   threading. Zero for guessing the number of
	 *    concurrent threads supported by the hardware.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added or the topic modeller has been
	 *   cleared.
	 */
	inline void TopicModel::train(
			std::size_t iterations,
			std::size_t threads
	) {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("train", isHdp, isIdf);
		this->prepareModel(isHdp, isIdf);
		this->trainModel(isHdp, isIdf, iterations, threads);
	}

	//! Labels the resulting topics.
	/*!
	 * Does nothing, except clearing any existing
	 *  labeling, if labeling has not been activated
	 *  or has been deactivated.
	 *
	 * \param threads Number of threads. One for single
	 *   threading. Zero for guessing the number of
	 *   concurrent threads supported by the hardware.
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, the model has not been trained yet,
	 *   or the file cannot be read.
	 *
	 * \sa setLabelingOptions
	 */
	inline void TopicModel::label(std::size_t threads) {
		if(!(this->isLabeling)) {
			this->labeler.release();

			return;
		}

		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("label", isHdp, isIdf);
		this->checkTrained("label");

		this->workersUsed = threads;

		// extract topic label candidates
		PMIExtractor extractor(
				this->labelingMinCf,
				this->labelingMinDf,
				this->labelingMinLength,
				this->labelingMaxLength,
				this->labelingMaxCandidates
		);

		const auto& modelInterface{
			this->getModelInterface(isHdp, isIdf)
		};

		const auto candidates{
			extractor.extract(&modelInterface)
		};

		// create labeler
		constexpr auto LAMBDA{0.2F};

		this->labeler = std::make_unique<FoRelevance>(
				&modelInterface,
				candidates.begin(),
				candidates.end(),
				this->labelingMinDf,
				this->labelingSmoothing,
				LAMBDA, /* not used yet */
				this->labelingMu,
				this->labelingWindowSize == 0  ?
					std::numeric_limits<std::size_t>::max()
					: this->labelingWindowSize,
				threads
		);
	}

	/*
	 * LOAD AND SAVE
	 */

	//! Loads a model from a file.
	/*!
	 * Clears all previous data before trying to load
	 *  the new model, if applicable.
	 *
	 * \param fileName Name of the file to load the
	 *   model from.
	 *
	 * \returns The number of bytes read from the
	 *   model file (best guess).
	 *
	 * \throws TopicModel::Exception if the model
	 *   could not be loaded from the specified file,
	 *   e.g. because the file does not exist or the
	 *   file format is unsupported.
	 */
	inline size_t TopicModel::load(const std::string& fileName) {
		this->clear();

		bool isHdp{false};
		bool isIdf{false};

		// open the file
		std::ifstream in(fileName.c_str(), std::ios::binary);

		if(!in.is_open()) {
			throw Exception(
					"TopicModel::loadModelFrom():"
					" Could not read from '"
					+ fileName
					+ "'"
			);
		}

		// read the file head (= model type)
		TopicModel::readModelFileHead(in, fileName);

		// read and set the term weighting scheme
		TopicModel::readModelFileTermWeighting(in, fileName, isIdf);

		this->setUseIdf(isIdf);

		// read the file type
		TopicModel::readModelFileType(in, fileName);

		// return to the beginning of the file
		TopicModel::resetStream(in);

		// initialize and load the model
		std::vector<uint8_t> data;

		this->initModel(isHdp, isIdf);

		try {
			//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
			DATA_TOPICMODEL_CALL(
					isHdp,
					isIdf,
					loadModel,
					in,
					&data
			);
		}
		catch(...) {
			// if loading of the model failed, clear it and try another algorithm
			this->clear();

			if(isHdp) { /* if the algorithm was set to HDP, set it to LDA */
				this->fixedNumberOfTopics = defaultNumberOfInitialTopics;
			}

			// return to the beginning of the file
			TopicModel::resetStream(in);

			// initialize and load the model
			this->initModel(isHdp, isIdf);

			//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
			DATA_TOPICMODEL_CALL(
					isHdp,
					isIdf,
					loadModel,
					in,
					&data
			);
		}

		// get number of bytes (best guess)
		const auto bytesRead{in.tellg()};

		// close the file
		in.close();

		// retrieve additional information about the loaded model
		this->loadModelInformation(isHdp, isIdf, data);

		return bytesRead;
	}

	//! Writes the model to a file.
	/*!
	 * \param fileName Name of the file to write the
	 *   model to.
	 * \param full Sets whether to save all documents
	 *   with the model so that the training can be
	 *   continued. If false, the saved model can only
	 *   be used for topic classification.
	 *
	 * \returns The number of bytes written to the
	 *   model file (best guess).
	 *
	 * \throws TopicModel::Exception if no documents
	 *   have been added, the topic modeller has been
	 *   cleared, the model has not been trained yet,
	 *   or the file cannot be read.
	 */
	inline std::size_t TopicModel::save(const std::string& fileName, bool full) const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("saveModelTo", isHdp, isIdf);
		this->checkTrained("saveModelTo");

		// open file to write model to
		std::ofstream out(fileName.c_str(), std::ios::binary);

		if(!out.is_open()) {
			throw Exception(
					"TopicModel::saveModelTo():"
					" Could not write to '"
					+ fileName
					+ "'"
			);
		}

		// add additional information to the saved model
		std::vector<uint8_t> data;

		this->writeModelInformation(isHdp, isIdf, data);

		// write model to file
		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_CALL(isHdp, isIdf, saveModel, out, full, &data);

		// get number of written bytes (best guess)
		const auto bytesWritten{out.tellp()};

		// close file
		out.close();

		return bytesWritten;
	}

	/*
	 * CLEANUP
	 */

	//! Clears the model, resets its settings and frees memory.
	inline void TopicModel::clear() {
		this->hdpModel.release();
		this->hdpModelIdf.release();
		this->ldaModel.release();
		this->ldaModelIdf.release();

		std::vector<std::string>{}.swap(this->docNames);

		this->hasDocs = false;
		this->isPrepared = false;

		this->fixedNumberOfTopics = 0;
		this->numberOfInitialTopics = defaultNumberOfInitialTopics;
		this->initialAlpha = defaultAlpha;
		this->initialEta = defaultEta;
		this->initialGamma = defaultGamma;
		this->seed = std::random_device{}();
		this->minWordCf = 0;
		this->minWordDf = 0;
		this->removeTopNWords = 0;
		this->optimizationInterval = defaultOptimizationInterval;

		this->trainedWithVersion.clear();

		this->labeler.release();

		this->isLabeling = false;
		this->labelingMinCf = 0;
		this->labelingMinDf = 0;
		this->labelingMinLength = 0;
		this->labelingMaxLength = 0;
		this->labelingMaxCandidates = 0;
		this->labelingSmoothing = 0.F;
		this->labelingMu = 0.F;
		this->labelingWindowSize = 0;
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// initialize model
	inline void TopicModel::initModel(bool& isHdpTo, bool& isIdfTo) {
		if(
				!(this->hdpModel)
				&& !(this->hdpModelIdf)
				&& !(this->ldaModel)
				&& !(this->ldaModelIdf)
		) {
			if(this->fixedNumberOfTopics == 0) {
				if(this->isUseIdf) {
					this->hdpModelIdf = std::make_unique<HDPModelIDF>(
							this->numberOfInitialTopics,
							this->initialAlpha,
							this->initialEta,
							this->initialGamma,
							this->seed
					);
				}
				else {
					this->hdpModel = std::make_unique<HDPModel>(
							this->numberOfInitialTopics,
							this->initialAlpha,
							this->initialEta,
							this->initialGamma,
							this->seed
					);
				}
			}
			else if(this->isUseIdf) {
				this->ldaModelIdf = std::make_unique<LDAModelIDF>(
						this->fixedNumberOfTopics,
						this->initialAlpha,
						this->initialEta,
						this->seed
				);
			}
			else {
				this->ldaModel = std::make_unique<LDAModel>(
						this->fixedNumberOfTopics,
						this->initialAlpha,
						this->initialEta,
						this->seed
				);
			}
		}

		if(this->hdpModel) {
			isHdpTo = true;
			isIdfTo = false;
		}
		else if(this->hdpModelIdf) {
			isHdpTo = true;
			isIdfTo = true;
		}
		else if(this->ldaModel) {
			isHdpTo = false;
			isIdfTo = false;
		}
		else if(this->ldaModelIdf){
			isHdpTo = false;
			isIdfTo = true;
		}
		else {
			throw Exception(
					"TopicModel::initModel():"
					" No model has been loaded."
			);
		}
	}

	// look up word ID in dictionary
	inline std::string TopicModel::dictLookUp(tomoto::Vid wordId) const {
		bool isHdp{false};
		bool isIdf{false};

		this->checkModel("dictLookUp", isHdp, isIdf);
		this->checkTrained("dictLookUp");

		std::string result;

		return this->getDict(isHdp, isIdf).toWord(wordId);
	}

	// check model
	inline void TopicModel::checkModel(
			const std::string& function,
			bool& isHdpTo,
			bool& isIdfTo
	) const {
		if(this->hasDocs) {
			if(this->hdpModel) {
				isHdpTo = true;
				isIdfTo = false;

				return;
			}

			if(this->hdpModelIdf) {
				isHdpTo = true;
				isIdfTo = true;

				return;
			}

			if(this->ldaModel) {
				isHdpTo = false;
				isIdfTo = false;

				return;
			}

			if(this->ldaModelIdf) {
				isHdpTo = false;
				isIdfTo = true;

				return;
			}
		}

		throw Exception(
				"TopicModel::"
				+ function
				+	"(): No documents have been added"
					" or the model has already been cleared"
		);
	}

	// check whether model has not been initialized
	inline void TopicModel::checkNoModel(
			const std::string& function,
			const std::string& errorMsg
	) const {
		if(
				this->hdpModel
				|| this->hdpModelIdf
				|| this->ldaModel
				|| this->ldaModelIdf
		) {
			throw Exception(
					"TopicModel::"
					+ function
					+ "(): "
					+ errorMsg
					+ " after the model has been initialized"
			);
		}
	}

	// check whether training has been started
	inline void TopicModel::checkTrained(const std::string& function) const {
		if(!(this->isPrepared)) {
			throw Exception(
					"TopicModel::"
					+ function
					+ "(): The model has not yet been trained"
			);
		}
	}

	// check whether training has not yet been started
	inline void TopicModel::checkNotTrained(
			const std::string& function,
			const std::string& errorMsg
	) const {
		if(this->isPrepared) {
			throw Exception(
					"TopicModel::"
					+ function
					+ "(): "
					+ errorMsg
					+ " after the model has already been trained"
			);
		}
	}

	// get dictionary (without further checking)
	inline const tomoto::Dictionary& TopicModel::getDict(bool isHdp, bool isIdf) const {
		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(isHdp, isIdf, getVocabDict);
	}

	// get number of topics (without further checking)
	inline std::size_t TopicModel::getLiveK(bool isIdf) const {
		if(isIdf) {
			return this->hdpModelIdf->getLiveK();
		}

		return this->hdpModel->getLiveK();
	}

	// get number of topics (without further checking)
	inline std::size_t TopicModel::getK(bool isHdp, bool isIdf) const {
		if(!isHdp) {
			return this->fixedNumberOfTopics;
		}

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETURN(
				isHdp,
				isIdf,
				getK
		);
	}

	// check whether topic is alive (without additional checking)
	inline bool TopicModel::isLiveTopic(bool isIdf, std::size_t topic) const {
		if(isIdf) {
			return this->hdpModelIdf->isLiveTopic(topic);
		}

		return this->hdpModel->isLiveTopic(topic);
	}

	// get concentration coefficient of the Dirichlet Process for table-topic (without further checking)
	inline float TopicModel::getGamma(bool isIdf) const {
		if(isIdf) {
			return this->hdpModelIdf->getGamma();
		}

		return this->hdpModel->getGamma();
	}

	// get number of tables in the LDP model
	inline std::size_t TopicModel::getNumberOfTables(bool isIdf) const {
		if(isIdf) {
			return this->hdpModelIdf->getTotalTables();
		}

		return this->hdpModel->getTotalTables();
	}

	// prepare model (without further checking)
	inline void TopicModel::prepareModel(bool isHdp, bool isIdf) {
		if(!(this->isPrepared)) {
			//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
			DATA_TOPICMODEL_CALL(
					isHdp,
					isIdf,
					prepare,
					true,
					this->minWordCf,
					this->minWordDf,
					this->removeTopNWords
			);

			this->isPrepared = true;
			this->trainedWithVersion = Helper::Versions::getTomotoVersion();
		}
	}

	// train model (without further checking)
	inline void TopicModel::trainModel(bool isHdp, bool isIdf, std::size_t iterations, std::size_t threads) {
		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_CALL(
				isHdp,
				isIdf,
				train,
				iterations,
				threads,
				tomoto::ParallelScheme::default_
		);
	}

	// load model information after reading model from file
	inline void TopicModel::loadModelInformation(
			bool isHdp,
			bool isIdf,
			const std::vector<std::uint8_t>& data
	) {
		// get model information from a dictionary generated by reading Python pickle data
		PickleDict dict(data);

		TopicModel::numberFromDict(dict, "min_cf", this->minWordCf);
		TopicModel::numberFromDict(dict, "min_df", this->minWordDf);
		TopicModel::numberFromDict(dict, "rm_top", this->removeTopNWords);
		TopicModel::numberFromDict(dict, "initial_k", this->numberOfInitialTopics); /* HDP only*/
		TopicModel::numberFromDict(dict, "k", this->fixedNumberOfTopics); /* LDA only */
		TopicModel::numberFromDict(dict, "seed", this->seed);

		TopicModel::floatFromDict(dict, "alpha", this->initialAlpha);
		TopicModel::floatFromDict(dict, "eta", this->initialEta);
		TopicModel::floatFromDict(dict, "gamma", this->initialGamma); /* HDP only */

		TopicModel::stringFromDict(dict, "version", this->trainedWithVersion);

		// check whether model has been trained
		std::size_t iterations{};

		//NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
		DATA_TOPICMODEL_RETRIEVE(iterations, isHdp, isIdf, getGlobalStep);

		this->hasDocs = true;

		if(iterations > 0) {
			this->isPrepared = true;
		}
	}

	// write model information for writing the module to file
	inline void TopicModel::writeModelInformation(
			bool isHdp,
			bool isIdf,
			std::vector<std::uint8_t>& dataTo
	) const {
		// fill dictionary with model information
		PickleDict dict;

		dict.setNumber(
				"tw",
				static_cast<std::int64_t>(
						isIdf ? tomoto::TermWeight::idf : tomoto::TermWeight::one
				)
		);

		dict.setNumber("min_cf", this->minWordCf);
		dict.setNumber("min_df", this->minWordDf);
		dict.setNumber("rm_top", this->removeTopNWords);

		if(isHdp) {
			dict.setNumber("initial_k", this->numberOfInitialTopics);
		}
		else {
			dict.setNumber("k", this->fixedNumberOfTopics);
		}

		dict.setNumber("seed", this->seed);

		dict.setFloat("alpha", this->initialAlpha);
		dict.setFloat("eta", this->initialEta);

		if(isHdp) {
			dict.setFloat("gamma", this->initialGamma);
		}

		dict.setString("version", this->trainedWithVersion);

		// write dictionary as Python pickle data
		dict.writeTo(dataTo);
	}

	// get constant reference to the interface of the currently active topic model
	inline const tomoto::ITopicModel& TopicModel::getModelInterface(bool isHdp, bool isIdf) const {
		if(isHdp) {
			if(isIdf) {
				return *(this->hdpModelIdf);
			}

			return *(this->hdpModel);
		}

		if(isIdf) {
			return *(this->ldaModelIdf);
		}

		return *(this->ldaModel);
	}

	// create document for underlying API
	inline tomoto::RawDoc TopicModel::createDocument(
			const std::string& name,
			const std::vector<std::string>& tokens,
			std::size_t firstToken,
			std::size_t numTokens
	) {
		tomoto::RawDoc doc;
		const auto documentEnd{firstToken + numTokens};

		doc.rawWords.reserve(numTokens);

		for(std::size_t tokenIndex{firstToken}; tokenIndex < documentEnd; ++tokenIndex) {
			doc.rawWords.emplace_back(tokens.at(tokenIndex));
		}

		// share document name
		doc.docUid = tomoto::SharedString(name);

		return doc;
	}

	// check first bytes of the topic model file (indicating the type of the model)
	inline void TopicModel::readModelFileHead(std::istream& in, const std::string& fileName) {
		std::array<char, modelFileHead.size()> headBytes;

		in.read(headBytes.data(), modelFileHead.size());

		if(!TopicModel::bytesEqual(headBytes, modelFileHead)) {
			throw Exception(
					"TopicModel::loadModelFrom():"
					" Invalid model file or unsupported model type in '"
					+ fileName
					+ "' (first bytes do not match tomoto's LDA model format: '"
					+ TopicModel::bytesToString(headBytes)
					+ "')"
			);
		}
	}

	// check and read term weighting scheme from topic model file
	inline void TopicModel::readModelFileTermWeighting(std::istream& in, const std::string& fileName, bool& isIdfTo) {
		std::array<char, modelFileTermWeightingLen> twBytes;

		in.read(twBytes.data(), modelFileTermWeightingLen);

		if(TopicModel::bytesEqual(twBytes, modelFileTermWeightingOne)) {
			isIdfTo = false;
		}
		else if(TopicModel::bytesEqual(twBytes, modelFileTermWeightingIdf)) {
			isIdfTo = true;
		}
		else {
			throw Exception(
					"TopicModel::loadModelFrom():"
					" Invalid model file or unsupported term weighting scheme in '"
					+ fileName
					+ "' (term weighting scheme does not match 'one' or 'idf':"
					+ TopicModel::bytesToString(twBytes)
					+ "')"
			);
		}
	}

	// check file type of topic model file
	inline void TopicModel::readModelFileType(std::istream& in, const std::string& fileName) {
		std::array<char, modelFileType.size()> typeBytes;

		in.read(typeBytes.data(), modelFileType.size());

		if(!TopicModel::bytesEqual(typeBytes, modelFileType)) {
			throw Exception(
					"TopicModel::loadModelFrom():"
					" Invalid model file '"
					+ fileName
					+ "' (type does not match tomoto's model format: '"
					+ TopicModel::bytesToString(typeBytes)
					+ "')"
			);
		}
	}

	// reset an input stream and go back to its start
	inline void TopicModel::resetStream(std::istream& in) {
		in.clear();
		in.seekg(0, std::ios_base::beg);
	}

	// get number from Pickle dictionary, if available
	inline void TopicModel::numberFromDict(const PickleDict& dict, const std::string& key, std::size_t& valueTo) {
		const auto entry{
			dict.getNumber(key)
		};

		if(entry) {
			valueTo = static_cast<std::size_t>(*entry);
		}
		else {
			valueTo = 0;
		}
	}

	// get floating-point number from Pickle dictionary, if available
	inline void TopicModel::floatFromDict(const PickleDict& dict, const std::string& key, float& valueTo) {
		const auto entry{
			dict.getFloat(key)
		};

		if(entry) {
			valueTo = static_cast<float>(*entry);
		}
		else {
			valueTo = 0.F;
		}
	}

	// get string from Pickle dictionary, if available
	inline void TopicModel::stringFromDict(const PickleDict& dict, const std::string& key, std::string& valueTo) {
		auto entry{
			dict.getString(key)
		};

		if(entry) {
			valueTo = std::move(*entry);
		}
		else {
			std::string{}.swap(valueTo);
		}
	}

} /* namespace crawlservpp::Data::TopicModel */

#endif /* DATA_TOPICMODEL_HPP_ */
