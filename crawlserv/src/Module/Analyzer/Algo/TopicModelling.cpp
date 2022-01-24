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
 * TopicModelling.cpp
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

#include "TopicModelling.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	TopicModelling::TopicModelling(
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
	TopicModelling::TopicModelling(Main::Database& dbBase,
							const ThreadOptions& threadOptions)
								:	Module::Analyzer::Thread(dbBase, threadOptions) {
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
	std::string_view TopicModelling::getName() const {
		return "TopicModelling";
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Initializes the target table for the algorithm.
	/*!
	 * \note When this function is called, neither the
	 *   prepared SQL statements, nor the queries have
	 *   been initialized yet.
	 */
	void TopicModelling::onAlgoInitTarget() {
		// set known target fields
		std::vector<StringString> fields;

		if(this->algoConfig.isNumberOfTopicsFixed) {
			TopicModelling::initKnownTopics(
					fields,
					this->algoConfig.initialNumberOfTopics
			);
		}
		else {
			TopicModelling::initUnknownTopics(fields);
		}

		// initialize target table
		this->database.setTargetFields(fields);

		this->database.initTargetTable(false, true);

		// initialize topic table
		this->initTopicTable();
	}

	//! Initializes the algorithm and processes its input.
	/*!
	 * \note When this function is called, both the
	 *   prepared SQL statements, and the queries have
	 *   already been initialized.
	 *
	 * \sa initQueries
	 */
	void TopicModelling::onAlgoInit() {
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

		// check sources, request text corpora and combine them into one
		this->getCorpus(statusSetter);

		if(!(this->isRunning())) {
			return;
		}

		// initialize algorithm
		this->initModel();
		this->loadModel(statusSetter);
		this->addArticles(statusSetter);
		this->startTraining(statusSetter);

		this->timer.tick();

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");

		/*
		 * NOTE: Do not set any thread status here, as the parent class
		 *        will revert to the original thread status after initialization.
		 */
	}

	//! Performs a number of training iterations, if necessary.
	void TopicModelling::onAlgoTick() {
		if(this->isTrained) {
			/* training is finished */
			this->finishUp();

			return;
		}

		if(this->firstTick) {
			/* first tick, first status */
			this->setStatusMessage("Training model...");

			this->firstTick = false;
		}

		// perform training tick
		this->trainModel();

		// update status
		const auto ll{this->model.getLogLikelihoodPerToken()};
		const auto k{this->model.getNumberOfTopics()};

		this->updateTrainingStatus(ll, k);
		this->logTrainingTick(ll, k);
	}

	//! Does nothing.
	void TopicModelling::onAlgoPause() {}

	//! Does nothing.
	void TopicModelling::onAlgoUnpause() {}

	//! Does nothing.
	void TopicModelling::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Parses a configuration option for the algorithm.
	void TopicModelling::parseAlgoOption() {
		this->category("topic");

		this->option("k", this->algoConfig.initialNumberOfTopics);
		this->option("k.fixed", this->algoConfig.isNumberOfTopicsFixed);
		this->option("table", this->algoConfig.topicTable);
		this->option("table.n", this->algoConfig.numberOfTopicTokens);

		this->category("topic-training");

		this->option("burn.in", this->algoConfig.burnIn);
		this->option("idf", this->algoConfig.idf);
		this->option("iterations", this->algoConfig.iterations);
		this->option("iterations.at.once", this->algoConfig.iterationsAtOnce);
		this->option("min.cf", this->algoConfig.minCf);
		this->option("min.df", this->algoConfig.minDf);
		this->option("optimize.every", this->algoConfig.optimizeEvery);
		this->option("remove.top.n", this->algoConfig.removeTopN);
		this->option("threads", this->algoConfig.threads);

		this->category("topic-model");

		this->option("alpha", this->algoConfig.alpha);
		this->option("continue", this->algoConfig.isContinue);
		this->option("eta", this->algoConfig.eta);
		this->option("gamma", this->algoConfig.gamma);
		this->option("iterations", this->algoConfig.docIterations);
		this->option("load", this->algoConfig.load);
		this->option("save", this->algoConfig.save);
		this->option("save.full", this->algoConfig.saveFull);
		this->option("seed", this->algoConfig.seed);
		this->option("workers", this->algoConfig.workers);

		this->category("topic-labeling");

		this->option("num", this->algoConfig.labelNumber);
		this->option("min.cf", this->algoConfig.labelMinCf);
		this->option("min.df", this->algoConfig.labelMinDf);
		this->option("min.len", this->algoConfig.labelMinLength);
		this->option("max.len", this->algoConfig.labelMaxLength);
		this->option("max.candidates", this->algoConfig.labelMaxCandidates);
		this->option("smoothing", this->algoConfig.labelSmoothing);
		this->option("mu", this->algoConfig.labelMu);
		this->option("window.size", this->algoConfig.labelWindowSize);
	}

	//! Checks the configuration options for the algorithm.
	/*!
	 * \throws Analyzer::Thread::Exception if no
	 *   topic table has been specified.
	 */
	void TopicModelling::checkAlgoOptions() {
		if(this->algoConfig.topicTable.empty()) {
			throw Exception(
					"TopicModelling::checkAlgoOptions():"
					" No topic table has been specified"
			);
		}

		if(this->algoConfig.threads != 1) {
			this->log(
					generalLoggingDefault,
					"WARNING: Training with multiple threads impedes"
					" reproducibility, even when using the same seed"
					" for random number generation!"
			);
		}

		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the algorithm.
	void TopicModelling::resetAlgo() {
		this->algoConfig = {};

		this->model.clear(true);
		this->timer.clear();

		this->firstTick = true;
		this->isTrained = false;

		this->iteration = 0;
		this->topicTable = 0;

		Helper::Memory::free(this->articlesDone);
		Helper::Memory::free(this->results);

		timer.tick();
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// set up topic model
	void TopicModelling::initModel() {
		this->model.setInitialParameters(
				this->algoConfig.initialNumberOfTopics,
				this->algoConfig.alpha,
				this->algoConfig.eta,
				this->algoConfig.gamma
		);

		if(this->algoConfig.isNumberOfTopicsFixed) {
			this->model.setFixedNumberOfTopics(this->algoConfig.initialNumberOfTopics);
		}

		this->model.setUseIdf(this->algoConfig.idf);
		this->model.setTokenRemoval(
				this->algoConfig.minCf,
				this->algoConfig.minDf,
				this->algoConfig.removeTopN
		);
		this->model.setParameterOptimizationInterval(this->algoConfig.optimizeEvery);

		if(this->algoConfig.seed == 0) {
			this->algoConfig.seed = std::random_device{}();
		}

		this->model.setRandomNumberGenerationSeed(this->algoConfig.seed);

		this->model.setLabelingOptions(
				this->algoConfig.labelNumber > 0,
				this->algoConfig.labelMinCf,
				this->algoConfig.labelMinDf,
				this->algoConfig.labelMinLength,
				this->algoConfig.labelMaxLength,
				this->algoConfig.labelMaxCandidates,
				this->algoConfig.labelSmoothing,
				this->algoConfig.labelMu,
				this->algoConfig.labelWindowSize
		);
	}

	// check sources, get text corpora and combine them into one
	void TopicModelling::getCorpus(StatusSetter& statusSetter) {
		// check your sources
		this->log(generalLoggingVerbose, "checks sources...");

		this->checkCorpusSources(statusSetter);

		// get corpora and combine them into one
		this->log(generalLoggingDefault, "gets text corpus...");

		if(!(this->addCorpora(true, statusSetter)) && this->isRunning()) {
			throw Exception("TopicModelling::getCorpus(): Corpus is empty");
		}
	}

	// load a pre-trained model, if necessary
	void TopicModelling::loadModel(StatusSetter& statusSetter) {
		if((this->algoConfig.load.empty())) {
			this->log(generalLoggingDefault, "does not load any pre-trained model.");

			return;
		}

		if(!statusSetter.change(
				"Loading pre-trained model '"
				+ this->algoConfig.load
				+ "'..."
		)) {
			return;
		}

		const auto fileName{TopicModelling::modelFile(this->algoConfig.load)};

		this->logLoading(fileName);

		Timer::Simple timer;

		const auto bytesRead{this->model.load(fileName)};

		this->isTrained = true;

		this->logLoad(fileName, timer.tickStr(), bytesRead);
	}

	// starts the training of the model
	void TopicModelling::startTraining(StatusSetter& statusSetter) {
		if(this->corpora.empty()) {
			return;
		}

		if(this->isTrained) {
			if(this->algoConfig.isContinue) {
				/* training to be continued */
				this->isTrained = false;
			}

			return;
		}

		if(!statusSetter.change("Preparing model...")) {
			return;
		}

		this->model.setBurnInIteration(this->algoConfig.burnIn);
		this->model.startTraining();
	}

	// add documents from corpus to model
	void TopicModelling::addArticles(StatusSetter& statusSetter) {
		if(this->corpora.empty() || this->isTrained) {
			return;
		}

		if(this->corpora.size() > 1) {
			throw Exception(
					"TopicModelling::addArticles():"
					" Corpora need to be combined in order"
					" to add them to the model"
			);
		}

		if(!statusSetter.change("Adding articles to the model...")) {
			return;
		}

		std::size_t articleCounter{};
		std::size_t updateCounter{};

		for(const auto& article : this->corpora[0].getcArticleMap()) {
			this->model.addDocument(
					article.value,
					this->corpora[0].getcTokens(),
					TextMapEntry::pos(article),
					TextMapEntry::length(article)
			);

			++articleCounter;
			++updateCounter;

			if(updateCounter == topicModellingUpdateProgressEvery) {
				if(
						!statusSetter.update(
								articleCounter,
								this->corpora[0].getcArticleMap().size(),
								true
						)
				) {
					return;
				}

				updateCounter = 0;
			}
		}

		statusSetter.finish();
	}

	// one training tick
	void TopicModelling::trainModel() {
		if(!(this->algoConfig.load.empty()) && !(this->algoConfig.isContinue)) {
			this->log(generalLoggingDefault, "does not continue to train loaded model.");

			return;
		}

		if(this->iteration >= this->algoConfig.iterations) {
			this->isTrained = true;

			return;
		}

		const auto iterationsToPerform{
			std::min(
					this->algoConfig.iterations - this->iteration,
					static_cast<std::size_t>(this->algoConfig.iterationsAtOnce)
			)
		};

		this->model.train(iterationsToPerform, this->algoConfig.threads);

		this->iteration += iterationsToPerform;

		if(this->iteration >= this->algoConfig.iterations) {
			this->isTrained = true;
		}
	}

	// save the trained model, if necessary
	void TopicModelling::saveModel(StatusSetter& statusSetter) {
		if(!statusSetter.change("Saving model...")) {
			return;
		}

		if(this->algoConfig.save.empty()) {
			this->log(generalLoggingDefault, "does not save the trained model.");

			return;
		}

		statusSetter.change("Saving trained model...");

		const auto fileName{TopicModelling::modelFile(this->algoConfig.save)};

		this->logSaving(fileName, this->algoConfig.saveFull);

		Timer::Simple timer;

		const auto bytesWritten{this->model.save(fileName, this->algoConfig.saveFull)};

		this->logSave(fileName, timer.tickStr(), bytesWritten);
	}

	// classify the given article
	void TopicModelling::classifyArticles(StatusSetter& statusSetter) {
		if(!statusSetter.change("Classifying articles...")) {
			return;
		}

		this->log(generalLoggingDefault, "classifying articles...");

		// classify learned articles, if possible
		this->results = this->model.getDocumentsTopics(this->articlesDone);

		// classify previously unseen articles
		std::queue<std::string> toClassify{
			TopicModelling::getArticlesToClassify(
					this->corpora.at(0).getcArticleMap(),
					this->articlesDone
			)
		};

		this->classifyQueue(toClassify, statusSetter);

		// clear corpus
		this->cleanUpCorpora();
	}

	// perform automated topic labeling
	void TopicModelling::labelTopics(StatusSetter& statusSetter) {
		if(
				this->algoConfig.labelNumber == 0
				|| !statusSetter.change("Labeling topics...")
		) {
			return;
		}

		this->log(generalLoggingDefault, "labeling topics...");

		this->model.label(this->algoConfig.workers);

		const auto topics{this->model.getTopics()};

		this->labels.clear();

		for(auto topic : topics) {
			this->labels[topic] = this->model.getTopicTopNLabels(
					topic, this->algoConfig.labelNumber
			);
		}
	}

	// save the resulting data
	void TopicModelling::saveData(StatusSetter& statusSetter) {
		if(!statusSetter.change("Saving results to database...")) {
			return;
		}

		this->saveTopicData(statusSetter);
		this->saveArticleData(statusSetter);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// initialize second target table, i.e. topic table
	void TopicModelling::initTopicTable() {
		std::vector<StringString> topicTableFields;

		topicTableFields.reserve(this->algoConfig.numberOfTopicTokens + topicModellingTopicColumns);

		topicTableFields.emplace_back("topic_id", "BIGINT UNSIGNED NOT NULL");
		topicTableFields.emplace_back("topic_count", "BIGINT UNSIGNED NOT NULL");

		for(std::size_t label{1}; label <= this->algoConfig.labelNumber; ++label) {
			topicTableFields.emplace_back("label" + std::to_string(label), "TEXT");
			topicTableFields.emplace_back("label" + std::to_string(label) + "_prob", "FLOAT");
		}

		for(std::size_t token{1}; token <= this->algoConfig.numberOfTopicTokens; ++token) {
			topicTableFields.emplace_back("token" + std::to_string(token), "TEXT");
			topicTableFields.emplace_back("token" + std::to_string(token) + "_prob", "FLOAT");
		}

		this->topicTable = this->database.addAdditionalTable(
				this->algoConfig.topicTable,
				topicTableFields,
				false,
				true
		);
	}

	// write log entry before loading a pre-trained model, if necessary
	void TopicModelling::logLoading(const std::string& name) {
		this->log(generalLoggingDefault, "loads pre-trained model from '" + name + "'...");
	}

	// write log entry after loading a pre-trained model, if necessary
	void TopicModelling::logLoad(const std::string& name, const std::string& time, std::size_t size) {
		std::ostringstream logStrStr;

		logStrStr.imbue(std::locale(""));

		logStrStr << "loaded pre-trained model from '";
		logStrStr << name;
		logStrStr << "' in ";
		logStrStr << time;
		logStrStr << " (read ";
		logStrStr << size;
		logStrStr << "B):";

		this->log(generalLoggingDefault, logStrStr.str());

		this->logModelInfo();
	}

	// log information about the model
	void TopicModelling::logModelInfo() {
		auto logEntries{this->model.getModelInfo().toQueueOfStrings()};

		Helper::Queue::reverse(logEntries);

		while(!logEntries.empty()) {
			this->log(generalLoggingDefault, logEntries.front());

			logEntries.pop();
		}
	}

	// update training status
	void TopicModelling::updateTrainingStatus(float ll, std::size_t k) {
		std::ostringstream tick;

		tick.imbue(std::locale(""));

		tick << std::fixed << std::setprecision(topicModellingPrecisionLL);

		tick << "Training model... [Iteration #";
		tick << this->iteration;
		tick << ": ll=";
		tick << ll;
		tick << ", k=";
		tick << k;
		tick << "]";

		this->setStatusMessage(tick.str());
		this->setProgress(
				static_cast<float>(this->iteration)
				/ static_cast<float>(this->algoConfig.iterations)
		);
	}

	// log training tick
	void TopicModelling::logTrainingTick(float ll, std::size_t k) {
		std::ostringstream tick;

		tick.imbue(std::locale(""));

		tick << std::fixed << std::setprecision(topicModellingPrecisionLL);

		tick << "performed training iteration #";
		tick << this->iteration;
		tick << " with log-likelihood per token: ";
		tick << ll;
		tick << ", and number of topics: ";
		tick << k;
		tick << ".";

		this->log(generalLoggingDefault, tick.str());
	}

	// log the training time
	void TopicModelling::logTrainingTime() {
		this->log(generalLoggingDefault, "trained model in " + this->timer.tickStr());
	}

	// write log entry before saving the trained model, if necessary
	void TopicModelling::logSaving(const std::string& name, bool full) {
		std::string logEntry{"saving trained model"};

		if(full) {
			logEntry += ", including all documents, ";
		}

		logEntry += " to '";
		logEntry += name;
		logEntry += "'...";

		this->log(generalLoggingDefault, logEntry);
	}

	// write log entry after saving the trained model, if necessary
	void TopicModelling::logSave(const std::string& name, const std::string& time, std::size_t size) {
		std::ostringstream logStrStr;

		logStrStr.imbue(std::locale(""));

		logStrStr << "saved trained model to '";
		logStrStr << name;
		logStrStr << "' in ";
		logStrStr << time;
		logStrStr << " (wrote ";
		logStrStr << size;
		logStrStr << "B).";

		this->log(generalLoggingDefault, logStrStr.str());
	}

	// training is finished
	void TopicModelling::finishUp() {
		StatusSetter statusSetter(
				"Finishing up...",
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

		// log time and information about the model
		this->logTrainingTime();
		this->logModelInfo();

		// finish
		this->labelTopics(statusSetter);
		this->saveModel(statusSetter);
		this->classifyArticles(statusSetter);
		this->saveData(statusSetter);

		this->finished();
	}

	// classify a queue of articles, multiple at a time
	void TopicModelling::classifyQueue(
			std::queue<std::string>& toClassify,
			StatusSetter& statusSetter
	) {
		const auto total{toClassify.size()};

		this->results.reserve(total);

		while(!toClassify.empty()) {
			std::vector<std::string> articleNames;
			std::vector<std::vector<std::string>> articles;

			TopicModelling::getNArticlesFromQueue(
					topicModellingUpdateProgressEveryDocs,
					toClassify,
					this->corpora[0],
					articleNames,
					articles
			);

			const auto topics{
				this->model.getDocumentsTopics(
						articles,
						this->algoConfig.docIterations,
						this->algoConfig.workers
				)
			};

			TopicModelling::topicsToResults(
					articles.size(),
					articleNames,
					topics,
					this->results
			);

			if(!statusSetter.update(
					total - toClassify.size(),
					total,
					true)
			) {
				return;
			}
		}

		statusSetter.finish();
	}

	// save article data to target table
	void TopicModelling::saveArticleData(StatusSetter& statusSetter) {
		if(!statusSetter.change("Saving article data...")) {
			return;
		}

		this->log(
				generalLoggingDefault,
				"saving article data to '"
				+ this->config.generalTargetTable
				+ "'..."
		);

		if(!(this->algoConfig.isNumberOfTopicsFixed)) {
			TopicModelling::addTopicColumns(
					this->database,
					this->getTargetTableName(),
					this->model.getNumberOfTopics()
			);
		}

		const auto topics{this->model.getTopics()};
		const auto resultTable{this->getTargetTableName()};
		const auto numberOfColumns{topicModellingTargetColumns + topics.size()};

		std::size_t resultCount{};
		std::size_t updateCount{};

		for(const auto& result : this->results) {
			this->database.insertCustomData(
					TopicModelling::getArticleData(
							resultTable,
							numberOfColumns,
							result,
							this->getArticleTopDescription(result.second, topics)
					)
			);

			++resultCount;
			++updateCount;

			if(updateCount == topicModellingUpdateProgressEvery) {
				if(!statusSetter.update(resultCount, this->results.size(), true)) {
					return;
				}

				updateCount = 0;
			}
		}

		this->database.updateTargetTable();

		statusSetter.finish();
	}

	// save topic data to additional table, throws TopicModelling::Exception
	void TopicModelling::saveTopicData(StatusSetter& statusSetter) {
		if(!statusSetter.change("Saving topic data...")) {
			return;
		}

		this->log(
				generalLoggingDefault,
				"saving topic data to '"
				+ this->algoConfig.topicTable
				+ "'..."
		);

		// get topics sorted
		const auto topicsSorted{this->model.getTopicsSorted()};
		const auto fullTableName{
			this->database.getAdditionalTableName(
					this->topicTable
			)
		};

		// insert topic data
		std::size_t topicCount{};

		for(const auto& topic : topicsSorted) {
			this->database.insertCustomData(
					TopicModelling::getTopicData(
							fullTableName,
							topic
					)
			);

			++topicCount;

			if(!statusSetter.update(topicCount, topicsSorted.size(), true)) {
				return;
			}
		}

		this->database.updateAdditionalTable(this->topicTable);

		statusSetter.finish();
	}

	// get topic data
	Data::InsertFieldsMixed TopicModelling::getTopicData(
			const std::string& tableName,
			const std::pair<std::size_t, std::size_t>& topic
	) const {
		Data::InsertFieldsMixed result;

		result.table = tableName;
		result.columns_types_values.reserve(
				topicModellingTopicColumns
				+ this->algoConfig.labelNumber * topicModellingColumnsPerLabel
				+ this->algoConfig.numberOfTopicTokens * topicModellingColumnsPerToken
		);

		// add topic ID and topic count
		result.columns_types_values.emplace_back(
				"analyzed__topic_id",
				Data::Type::_uint64,
				topic.first
		);
		result.columns_types_values.emplace_back(
				"analyzed__topic_count",
				Data::Type::_uint64,
				topic.second
		);

		// add top N labels
		const auto topNLabels{this->labels.find(topic.first)};
		std::size_t label{1};

		if(topNLabels != this->labels.end()) {
			for(const auto& labelPair : topNLabels->second) {
				result.columns_types_values.emplace_back(
						"analyzed__label" + std::to_string(label),
						Data::Type::_string,
						labelPair.first
				);
				result.columns_types_values.emplace_back(
						"analyzed__label" + std::to_string(label) + "_prob",
						Data::Type::_double,
						labelPair.second
				);

				++label;
			}
		}
		else {
			throw Exception(
					"TopicModelling:getTopicData():"
					" Could not get labels for topic #"
					+ std::to_string(topic.first)
			);
		}

		// add top N tokens
		const auto tokenPairs{
			this->model.getTopicTopNTokens(
					topic.first,
					this->algoConfig.numberOfTopicTokens
			)
		};
		std::size_t token{1};

		for(const auto& tokenPair : tokenPairs) {
			result.columns_types_values.emplace_back(
					"analyzed__token" + std::to_string(token),
					Data::Type::_string,
					tokenPair.first
			);
			result.columns_types_values.emplace_back(
					"analyzed__token" + std::to_string(token) + "_prob",
					Data::Type::_double,
					tokenPair.second
			);

			++token;
		}

		return result;
	}

	// get the description of the top topic(s) for the article with the given topic probabilities
	std::string TopicModelling::getArticleTopDescription(
			const std::vector<float>& probabilities,
			const std::vector<std::size_t>& topics
	) const {
		// count topics with highest probability
		const auto max{*std::max_element(probabilities.begin(), probabilities.end())};
		const auto nMax{
				static_cast<std::size_t>(
						std::count_if(
								probabilities.begin(),
								probabilities.end(),
								[&max](const auto f) {
									return Helper::Math::almostEqual(
											f,
											max,
											topicModellingPrecisionUlp
									);
								}
						)
				)
		};

		// if all topics are (almost) equal, none will be selected
		if(nMax == probabilities.size()) {
			return std::string{};
		}

		// select topics with highest probability
		std::vector<std::size_t> maxIndices;

		maxIndices.reserve(nMax);

		for(std::size_t index{}; index < probabilities.size(); ++index) {
			if(
					Helper::Math::almostEqual(
							probabilities[index],
							max,
							topicModellingPrecisionUlp
					)
			) {
				maxIndices.push_back(index);
			}
		}

		// convert to string and return
		std::string result;
		bool isAppend{false};

		for(const auto index : maxIndices) {
			if(isAppend) {
				result += " | ";
			}
			else {
				isAppend = true;
			}

			result += this->getTopicDescription(topics.at(index));
		}

		return result;
	}

	// get the description of the topic with the given topic ID
	std::string TopicModelling::getTopicDescription(std::size_t topicId) const {
		std::vector<std::pair<std::string, float>> items;

		if(this->algoConfig.labelNumber > 0) {
			// get top labels
			items = this->model.getTopicTopNLabels(
					topicId,
					1
			);
		}
		else {
			// get top tokens
			items = this->model.getTopicTopNTokens(
					topicId,
					this->algoConfig.numberOfTopicTokens
			);
		}

		// sort descending by relevance
		std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
			return a.second > b.second;
		});

		// convert to string
		std::string result;
		bool isAppend{false};

		for(const auto& item : items) {
			if(isAppend) {
				result.push_back(' ');
			}
			else {
				isAppend = true;
			}

			result += item.first;
		}

		return result;
	}


	/*
	 * INTERNAL STATIC HELPER FUNCTIONS (private)
	 */

	// initialize target fields for a known number of topics
	void TopicModelling::initKnownTopics(
			std::vector<StringString>& fieldsTo,
			std::uint16_t numberOfTopics
	) {
		fieldsTo.reserve(topicModellingTargetColumns + numberOfTopics);

		TopicModelling::initArticleColumns(fieldsTo);

		for(
				std::uint16_t topic{};
				topic < numberOfTopics;
				++topic
		) {
			fieldsTo.emplace_back("k" + std::to_string(topic), "FLOAT");
		}
	}

	// initialize target fields for an unknown number of topics (i.e. article ID only)
	void TopicModelling::initUnknownTopics(std::vector<StringString>& fieldsTo) {
		fieldsTo.reserve(topicModellingTargetColumns);

		TopicModelling::initArticleColumns(fieldsTo);
	}

	// add article field
	void TopicModelling::initArticleColumns(std::vector<StringString>& fieldsTo) {
		fieldsTo.emplace_back("article", "TEXT NOT NULL");
		fieldsTo.emplace_back("top", "TEXT DEFAULT NULL");
	}

	// creates the name for the model file to read from or write to.
	std::string TopicModelling::modelFile(const std::string& name) {
		std::string fileName{topicModellingDirectory};

		fileName.reserve(fileName.size() + 1 + name.size());
		fileName.push_back(Helper::FileSystem::getPathSeparator());

		fileName += name;

		return fileName;
	}

	// add previously unknown topic columns
	void TopicModelling::addTopicColumns(
			Database& db,
			const std::string& targetTableName,
			std::uint16_t numberOfTopics
	) {
		for(
				std::uint16_t topic{};
				topic < numberOfTopics;
				++topic
		) {
			db.addTargetColumn(
					targetTableName,
					TableColumn(
							"analyzed__k" + std::to_string(topic),
							"FLOAT"
					)
			);
		}
	}

	// get a queue of articles that still need to be classified
	std::queue<std::string> TopicModelling::getArticlesToClassify(
			const TextMap& articleMap,
			std::unordered_set<std::string>& done
	) {
		std::queue<std::string> result;

		for(const auto& article : articleMap) {
			if(done.emplace(article.value).second) {
				result.emplace(article.value);
			}
		}

		return result;
	}

	// get data for topic classifications of a specific article
	Data::InsertFieldsMixed TopicModelling::getArticleData(
			const std::string& tableName,
			std::size_t numberOfColumns,
			const std::pair<std::string, std::vector<float>>& articleClassification,
			const std::string& top
	) {
		Data::InsertFieldsMixed data;

		data.table = tableName;

		data.columns_types_values.reserve(numberOfColumns);

		data.columns_types_values.emplace_back(
				"analyzed__article",
				Data::Type::_string,
				Data::Value(articleClassification.first)
		);
		data.columns_types_values.emplace_back(
				"analyzed__top",
				Data::Type::_string,
				top.empty() ? Data::Value() : Data::Value(top)
		);

		std::size_t topic{};

		for(const auto value : articleClassification.second) {
			data.columns_types_values.emplace_back(
					"analyzed__k" + std::to_string(topic),
					Data::Type::_double,
					Data::Value(value)
			);

			++topic;
		}

		return data;
	}

	// get a specific (maximum) number of articles (their names and tokens) from a queue and a corpus
	void TopicModelling::getNArticlesFromQueue(
			std::size_t n,
			std::queue<std::string>& from,
			const Corpus& corpus,
			std::vector<std::string>& namesTo,
			std::vector<std::vector<std::string>>& tokensTo
	) {
		namesTo.reserve(n);
		tokensTo.reserve(n);

		for(std::size_t current{}; current < n; ++current) {
			if(from.empty()) {
				break;
			}

			namesTo.emplace_back(from.front());
			tokensTo.emplace_back(corpus.getTokenized(from.front()));

			from.pop();
		}
	}

	// add retrieved topics for articles to the results
	void TopicModelling::topicsToResults(
			std::size_t n,
			const std::vector<std::string>& names,
			const std::vector<std::vector<float>>& topics,
			std::vector<std::pair<std::string, std::vector<float>>>& to
	) {
		for(std::size_t current{}; current < n; ++current) {
			to.emplace_back(names[current], topics[current]);
		}
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
