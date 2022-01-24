/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * TopicModelling.hpp
 *
 * Topic modelling using the Hierarchical Dirichlet Process (HDP) and
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
 *  Blei, D.M., Ng, A.Y., & Jordan, M.I. (2003). Latent dirichlet
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
 *  Created on: Feb 5, 2021
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_ALGO_TOPICMODELLING_HPP_
#define MODULE_ANALYZER_ALGO_TOPICMODELLING_HPP_

#include "../Thread.hpp"

#include "../../../Data/Corpus.hpp"
#include "../../../Data/Data.hpp"
#include "../../../Data/TopicModel.hpp"
#include "../../../Helper/FileSystem.hpp"
#include "../../../Helper/Math.hpp"
#include "../../../Helper/Memory.hpp"
#include "../../../Helper/Queue.hpp"
#include "../../../Main/Database.hpp"
#include "../../../Struct/StatusSetter.hpp"
#include "../../../Struct/TableColumn.hpp"
#include "../../../Struct/TextMap.hpp"
#include "../../../Struct/ThreadOptions.hpp"
#include "../../../Struct/ThreadStatus.hpp"
#include "../../../Timer/Simple.hpp"

#include <algorithm>		// std::count_if, std::max_element, std::min, std::sort
#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint8_t, std::uint16_t, std::uint64_t
#include <iomanip>			// std::setprecision
#include <ios>				// std::fixed
#include <limits>			// std::numeric_limits
#include <locale>			// std::locale
#include <queue>			// std::queue
#include <random>			// std::random_device
#include <sstream>			// std::ostringstream
#include <string>			// std::string, std::to_string
#include <string_view>		// std::string_view, std::string_view_literals
#include <unordered_map>	// std::unordered_map
#include <unordered_set>	// std::unordered_set
#include <utility>			// std::pair
#include <vector>			// std::vector

namespace crawlservpp::Module::Analyzer::Algo {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The directory for model files.
	inline constexpr auto topicModellingDirectory{"mdl"sv};

	//! The default number of initial topics.
	/*!
	 * Will be changed according to the data if the HDP
	 *  (and not the LDA) algorithm is used, i.e. if the
	 *  number of topics is not set to be fixed.
	 */
	inline constexpr auto topicModellingDefaultNumberOfTopics{2};

	//! The default number of most-probable tokens for each detected topic.
	/*!
	 * This number of most-probable tokens for each
	 *  detected topic will be written to the
	 *  provided topic table.
	 */
	inline constexpr auto topicModellingDefaultNumberOfTopicTokens{5};

	//! The default number of burn-in iterations.
	/*!
	 * "Burned in" iterations will be skipped before
	 *  starting to train the model.
	 */
	inline constexpr auto topicModellingDefaultBurnIn{100};

	//! The default number of iterations to train the model.
	inline constexpr auto topicModellingDefaultIterations{1000};

	//! The default number of iterations to train the model at once.
	inline constexpr auto topicModellingDefaultIterationsAtOnce{25};

	//! The default number of a token's minimum frequency in the corpus.
	inline constexpr auto topicModellingDefaultMinCf{1};

	//! The default number of a token's minimum document frequency.
	inline constexpr auto topicModellingDefaultMinDf{1};

	//! The default optimization interval for the model parameters, in training iterations.
	inline constexpr auto topicModellingDefaultOptimizeEvery{10};

	//! The default number of most-common tokens to ignore.
	inline constexpr auto topicModellingDefaultRemoveTopN{0};

	//! The default number of threads for training the model.
	inline constexpr auto topicModellingDefaultNumberOfThreads{1};

	//! The default initial hyperparameter for the Dirichlet distribution for document–table.
	inline constexpr auto topicModellingDefaultAlpha{0.1F};

	//! The default threshold for topics to be included when converting a HDP to a LDA model
	inline constexpr auto topicModellingDefaultConversionThreshold{0.F};

	//! The default initial hyperparameter for the Dirichlet distribution for topic–token.
	inline constexpr auto topicModellingDefaultEta{0.01F};

	//! The default initial concentration coefficient of the Dirichlet Process for table–topic.
	/*!
	 * Will be ignored, if the LDA instead of the
	 *  HDP algorithm is used, i.e. a fixed number
	 *  of topics is set.
	 */
	inline constexpr auto topicModellingDefaultGamma{0.1F};

	//! The default number of maximum iterations to classify a document.
	inline constexpr auto topicModellingDefaultDocIterations{100};

	//! The default number of worker threads for infering the topics of articles.
	inline constexpr auto topicModellingDefaultNumberOfWorkers{0};

	//! The default number of a topic label's minimum frequency in the corpus.
	inline constexpr auto topicModellingDefaultMinLabelCf{1};

	//! The default number of a topic label's minimum document frequency.
	inline constexpr auto topicModellingDefaultMinLabelDf{1};

	//! The default minimum length of topic labels, in tokens.
	inline constexpr auto topicModellingDefaultMinLabelLength{2};

	//! The default maximum length of topic labels, in tokens.
	inline constexpr auto topicModellingDefaultMaxLabelLength{5};

	//! The default maximum number of topic label candidates to be extracted from the training data.
	inline constexpr auto topicModellingDefaultMaxLabelCandidates{10000};

	//! The default Laplace smoothing for the automated detection of topic labels.
	inline constexpr auto topicModellingDefaultLabelSmoothing{.1F};

	//! The default discriminative coefficient for the automated detection of topic labels.
	inline constexpr auto topicModellingDefaultLabelMu{.25F};

	//! The number of added/saved articles after which the progress will be updated.
	inline constexpr auto topicModellingUpdateProgressEvery{1000};

	//! The number of classified documents after which the progress will be updated.
	inline constexpr auto topicModellingUpdateProgressEveryDocs{25};

	//! The number of digits of the log-likelihood to be logged.
	inline constexpr auto topicModellingPrecisionLL{6};

	//! The number of additional columns in the target table.
	inline constexpr auto topicModellingTargetColumns{2};

	//! The number of additional columns in the topic table.
	inline constexpr auto topicModellingTopicColumns{2};

	//! The number of columns per top label.
	inline constexpr auto topicModellingColumnsPerLabel{2};

	//! The number of columns per top token.
	inline constexpr auto topicModellingColumnsPerToken{2};

	//! Precision used when testing topic probabilities for equality, in ULPs (units in the last place).
	inline constexpr auto topicModellingPrecisionUlp{5};

	//@}

	/*
	 * DECLARATION
	 */

	//! Topic Modeller.
	/*!
	 * Topic modelling using the Hierarchical Dirichlet Process (HDP) and
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
	 *  Blei, D.M., Ng, A.Y., & Jordan, M.I. (2003). Latent dirichlet
	 *   allocation. Journal of machine Learning research, 3(Jan), 993–1022.
	 *
	 *  Newman, D., Asuncion, A., Smyth, P., & Welling, M. (2009). Distributed
	 *   algorithms for topic models. Journal of Machine Learning Research,
	 *   10 (Aug), 1801–1828.
	 */
	class TopicModelling final : public Module::Analyzer::Thread {
		// for convenience
		using Exception = Module::Analyzer::Thread::Exception;

		using Corpus = Data::Corpus;
		using TopicModel = Data::TopicModel;

		using StatusSetter = Struct::StatusSetter;
		using TableColumn = Struct::TableColumn;
		using TextMap = Struct::TextMap;
		using TextMapEntry = Struct::TextMapEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using StringString = std::pair<std::string, std::string>;

	public:
		///@name Construction
		///@{

		TopicModelling(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		TopicModelling(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);

		///@}
		///@name Implemented Getter
		///@{

		std::string_view getName() const override;

		///@}
		///@name Implemented Algorithm Functions
		///@{

		void onAlgoInitTarget() override;
		void onAlgoInit() override;
		void onAlgoTick() override;
		void onAlgoPause() override;
		void onAlgoUnpause() override;
		void onAlgoClear() override;

		///@}
		///@name Implemented Configuration Functions
		///@{

		void parseAlgoOption() override;
		void checkAlgoOptions() override;
		void resetAlgo() override;

		///@}

	private:
		// algorithm options
		struct Entries {
			// general
			std::uint16_t initialNumberOfTopics{topicModellingDefaultNumberOfTopics};
			bool isNumberOfTopicsFixed{false};
			std::string topicTable;
			std::uint16_t numberOfTopicTokens{topicModellingDefaultNumberOfTopicTokens};

			// training
			std::uint64_t burnIn{topicModellingDefaultBurnIn};
			bool idf{false};
			std::uint16_t iterations{topicModellingDefaultIterations};
			std::uint16_t iterationsAtOnce{topicModellingDefaultIterationsAtOnce};
			std::uint16_t minCf{topicModellingDefaultMinCf};
			std::uint16_t minDf{topicModellingDefaultMinDf};
			std::uint16_t optimizeEvery{topicModellingDefaultOptimizeEvery};
			std::size_t removeTopN{topicModellingDefaultRemoveTopN};
			std::uint16_t threads{topicModellingDefaultNumberOfThreads};

			// model
			float alpha{topicModellingDefaultAlpha};
			float conversionThreshold{topicModellingDefaultConversionThreshold};
			bool isContinue{false};
			float eta{topicModellingDefaultEta};
			float gamma{topicModellingDefaultGamma};
			std::uint16_t docIterations{topicModellingDefaultDocIterations};
			std::string load;
			std::string save;
			bool saveFull{false};
			std::size_t seed{};
			std::uint16_t workers{topicModellingDefaultNumberOfWorkers};

			// labeling
			std::size_t labelNumber{};
			std::uint16_t labelMinCf{topicModellingDefaultMinLabelCf};
			std::uint16_t labelMinDf{topicModellingDefaultMinLabelDf};
			std::uint8_t labelMinLength{topicModellingDefaultMinLabelLength};
			std::uint8_t labelMaxLength{topicModellingDefaultMaxLabelLength};
			std::uint64_t labelMaxCandidates{topicModellingDefaultMaxLabelCandidates};
			float labelSmoothing{topicModellingDefaultLabelSmoothing};
			float labelMu{topicModellingDefaultLabelMu};
			std::uint64_t labelWindowSize{};
		} algoConfig;

		// topic model
		TopicModel model;

		// algorithm state
		Timer::Simple timer;

		bool firstTick{true};
		bool isTrained{false};

		std::size_t iteration{};

		// second target table
		std::size_t topicTable{};

		// results
		std::unordered_set<std::string> articlesDone;
		std::vector<std::pair<std::string, std::vector<float>>> results;
		std::unordered_map<
				std::size_t,
				std::vector<std::pair<std::string, float>>
		> labels;

		// algorithm functions
		void initModel();
		void getCorpus(StatusSetter& statusSetter);
		void loadModel(StatusSetter& statusSetter);
		void addArticles(StatusSetter& statusSetter);
		void startTraining(StatusSetter& statusSetter);
		void trainModel();
		void saveModel(StatusSetter& statusSetter);
		void classifyArticles(StatusSetter& statusSetter);
		void labelTopics(StatusSetter& statusSetter);
		void saveData(StatusSetter& statusSetter);

		// internal helper functions
		void initTopicTable();
		void logLoading(const std::string& name);
		void logLoad(const std::string& name, const std::string& time, std::size_t size);
		void logModelInfo();
		void updateTrainingStatus(float ll, std::size_t k);
		void logTrainingTick(float ll, std::size_t k);
		void logTrainingTime();
		void logSaving(const std::string& name, bool full);
		void logSave(const std::string& name, const std::string& time, std::size_t size);
		void finishUp();
		void classifyQueue(std::queue<std::string>& toClassify, StatusSetter& statusSetter);
		void saveArticleData(StatusSetter& statusSetter);
		void saveTopicData(StatusSetter& statusSetter);

		[[nodiscard]] Data::InsertFieldsMixed getTopicData(
				const std::string& tableName,
				const std::pair<std::size_t, std::size_t>& topic
		) const;
		[[nodiscard]] std::string getArticleTopDescription(
				const std::vector<float>& probabilities,
				const std::vector<std::size_t>& topics
		) const;
		[[nodiscard]] std::string getTopicDescription(
				std::size_t topicId
		) const;

		// static internal helper functions
		static void initKnownTopics(
				std::vector<StringString>& fieldsTo,
				std::uint16_t numberOfTopics
		);
		static void initUnknownTopics(std::vector<StringString>& fieldsTo);
		static void initArticleColumns(std::vector<StringString>& fieldsTo);

		[[nodiscard]] static std::string modelFile(const std::string& name);

		static void addTopicColumns(
				Database& db,
				const std::string& targetTableName,
				std::uint16_t numberOfTopics
		);

		[[nodiscard]] static std::queue<std::string> getArticlesToClassify(
				const TextMap& articleMap,
				std::unordered_set<std::string>& done
		);
		[[nodiscard]] static Data::InsertFieldsMixed getArticleData(
				const std::string& tableName,
				std::size_t numberOfColumns,
				const std::pair<std::string, std::vector<float>>& articleClassification,
				const std::string& top
		);

		static void getNArticlesFromQueue(
				std::size_t n,
				std::queue<std::string>& from,
				const Corpus& corpus,
				std::vector<std::string>& namesTo,
				std::vector<std::vector<std::string>>& tokensTo
		);
		static void topicsToResults(
				std::size_t n,
				const std::vector<std::string>& names,
				const std::vector<std::vector<float>>& topics,
				std::vector<std::pair<std::string, std::vector<float>>>& to
		);
	};

} /* namespace crawlservpp::Module::Analyzer::Algo */

#endif /* MODULE_ANALYZER_ALGO_TOPICMODELLING_HPP_ */
