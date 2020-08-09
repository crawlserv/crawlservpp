/*
 *      Wapiti - A linear-chain CRF tool
 *
 * Copyright (c) 2009-2013  CNRS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * *** IMPORTANT NOTE ***
 *
 * This is a modified and minimialized version of Wapiti for crawlserv++,
 *  which can be found at https://github.com/crawlserv/crawlservpp.
 *
 *  No models can be trained with this version, use the full version
 *   at https://github.com/Jekub/Wapiti instead.
 *
 *  See the manual at https://wapiti.limsi.fr/manual.html for more
 *   information about the full version and its usage.
 *
 * *** END OF IMPORTANT NOTE ***
 *
 */

#ifndef wapiti_h
#define wapiti_h

/* XVM_ANSI:
 *   By uncomenting the following define, you can force wapiti to not use SSE2
 *   even if available.
 */
//#define XVM_ANSI

/*
 * INCLUDES
 */

#include <array>
#include <cassert>
#include <cinttypes>
#include <clocale>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <sys/cdefs.h>

#if defined(__SSE2__) && !defined(XVM_ANSI)
#include <emmintrin.h>
#endif

namespace crawlservpp::Data::wapiti {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	constexpr auto version{"1.5.0"sv};

	constexpr auto speedOfGrowth{1.4};

	constexpr auto byteMax{255};

	constexpr auto rdr3{3};

	constexpr auto kind1{1};

	constexpr auto kind2{2};

	constexpr auto kind3{3};

	constexpr auto sse2{2};

	constexpr auto sse4{4};

	constexpr auto sse16{16};

	constexpr auto sse20{20};

	constexpr auto sseShuffle{0x72};

	/*
	 * DECLARATION
	 */

	/*
	 * TOOLS.H
	 */

#define wapiti_none (std::numeric_limits<uint64_t>::max())

#define wapiti_min(a, b) ((a) < (b) ? (a) : (b))
#define wapiti_max(a, b) ((a) < (b) ? (b) : (a))

	void *xmalloc(size_t size);
	void *xrealloc(void *ptr, size_t size);
	char *xstrdup(const char *str);

	char *ns_readstr(FILE *file);

	/*
	 * OPTIONS.H
	 */

	/* opt_t:
	 *   This structure hold all user configurable parameter for Wapiti and is
	 *   filled with parameters from command line.
	 */
	typedef struct opt_s opt_t;
	struct opt_s {
		bool      maxent;
		char *	  model;
		bool      lblpost;
		bool      force;
		int       prec;
		bool      all;
	};

	/*
	 * QUARK.H
	 */

	typedef struct qrk_s qrk_t;

	qrk_t *qrk_new(void);
	void qrk_free(qrk_t *qrk);
	uint64_t qrk_count(const qrk_t *qrk);
	bool qrk_lock(qrk_t *qrk, bool lock);
	const char *qrk_id2str(const qrk_t *qrk, uint64_t id);
	uint64_t qrk_str2id(qrk_t *qrk, const char *key);
	void qrk_load(qrk_t *qrk, FILE *file);

	/*
	 * SEQUENCE.H
	 */

	/*******************************************************************************
	 * Sequences and Dataset objects
	 *
	 *   Sequences represent the input data feeded by the user in Wapiti either for
	 *   training or labelling. The internal form used here is very different from
	 *   the data read from files and the convertion process is done in three steps
	 *   illustrated here:
	 *         +------+     +-------+     +-------+     +-------+
	 *         | FILE | --> | raw_t | --> | tok_t | --> | seq_t |
	 *         +------+     +-------+     +-------+     +-------+
	 *   First the sequence is read as a set of lines from the input file, this
	 *   give a raw_t object. Next this set of lines is split in tokens and
	 *   eventually the last one is separated as it will become a label, this result
	 *   in a tok_t object.
	 *   The last step consist in applying all the patterns givens by the user to
	 *   extract from these tokens the observations made on the sequence in order to
	 *   build the seq_t object which can be used by the trainer and tagger.
	 *
	 *   A dataset object is just a container for a list of sequences in internal
	 *   form used to store either training or development set.
	 *
	 *   All the convertion process is driven by the reader object and, as it is
	 *   responsible for creating the objects with a quite special allocation
	 *   scheme, we just have to implement function for freeing these objects here.
	 ******************************************************************************/

	/* raw_t:
	 *   Data-structure representing a raw sequence as a set of lines read from the
	 *   input file. This is the result of the first step of the interning process.
	 *   We keep this form separate from the tokenized one as we want to be able to
	 *   output the sequence as it was read in the labelling mode.
	 *
	 *   This represent a sequence of lengths <len> and for each position 't' you
	 *   find the corresponding line at <lines>[t].
	 *
	 *   The <lines> array is allocated with data structure, and the different lines
	 *   are allocated separatly.
	 */
	typedef struct raw_s raw_t;
	struct raw_s {
		uint32_t  len;      		//   T     Sequence length
		char     *lines __flexarr;  //  [T]    Raw lines directly from file
	};

	/* tok_t:
	 *   Data-structure representing a tokenized sequence. This is the result of the
	 *   second step of the interning process after the raw sequence have been split
	 *   in tokens and eventual labels separated from the observations.
	 *
	 *   For each position 't' in the sequence of length <len>, you find at <lbl>[t]
	 *   the eventual label provided in input file, and at <toks>[t] a list of
	 *   string tokens of length <cnts>[t].
	 *
	 *   Memory allocation here is a bit special as the first token at each position
	 *   point to a memory block who hold a copy of the raw line. Each other tokens
	 *   and the label are pointer in this block. This reduce memory fragmentation.
	 */
	typedef struct tok_s tok_t;
	struct tok_s {
		uint32_t   len;     //   T     Sequence length
		char     **lbl;     //  [T]    List of labels strings
		uint32_t  *cnts;    //  [T]    Length of tokens lists
		char     **toks __flexarr;  //  [T][]  Tokens lists
	};

	/* seq_t:
	 *   Data-structure representing a sequence of length <len> in the internal form
	 *   used by the trainers and the tagger. For each position 't' in the sequence
	 *   (0 <= t < <len>) there is some observations made on the data and an
	 *   eventual label if provided in the input file.
	 *
	 *   There is two kind of features: unigrams and bigrams one, build by combining
	 *   one observation and one or two labels. At position 't', the unigrams
	 *   features are build using the list of observations from <uobs>[t] which
	 *   contains <ucnt>[t] items, and the observation at <lbl>[t]. The bigrams
	 *   features are obtained in the same way using <bobs> and <bcnt>, and have to
	 *   be combined also with <lbl>[t-1].
	 *
	 *   If the sequence is read from a file without label, as it is the case in
	 *   labelling mode, the <lbl> field will be NULL and so, the sequence cannot be
	 *   used for training.
	 *
	 *   The raw field is private and used internaly for efficient memory
	 *   allocation. This allow to allocate <lbl>, <*cnt>, and all the list in
	 *   <*obs> with the datastructure itself.
	 */

	struct pos_s {
		uint32_t  lbl;
		uint32_t  ucnt;
		uint32_t bcnt;
		uint64_t *uobs;
		uint64_t *bobs;
	};

	typedef struct pos_s pos_t;
	typedef struct seq_s seq_t;

	struct seq_s {
		uint32_t  len;
		uint64_t *raw;
		struct pos_s pos __flexarr;
	};

	/* dat_t:
	 *   Data-structure representing a full dataset: a collection of sequences ready
	 *   to be used for training or to be labelled. It keep tracks of the maximum
	 *   sequence length as the trainer need this for memory allocation. The dataset
	 *   contains <nseq> sequence stored in <seq>. These sequences are labeled only
	 *   if <lbl> is true.
	 */
	typedef struct dat_s dat_t;
	struct dat_s {
		bool       lbl;   //         True iff sequences are labelled
		uint32_t   mlen;  //         Length of the longest sequence in the set
		uint32_t   nseq;  //   S     Number of sequences in the set
		seq_t    **seq;   //  [S]    List of sequences
	};

	/*
	 * PATTERN.H
	 */

	typedef struct pat_s pat_t;
	typedef struct pat_item_s pat_item_t;

	struct pat_item_s {
			char      type;
			bool      caps;
			char     *value;
			bool      absolute;
			int32_t   offset;
			uint32_t  column;
		};

	struct pat_s {
		char     *src;
		uint32_t  ntoks;
		uint32_t  nitems;
		struct pat_item_s items __flexarr;
	};

	pat_t *pat_comp(char *p);
	char *pat_exec(const pat_t *pat, const tok_t *tok, uint32_t at);
	void pat_free(pat_t *pat);

	/*
	 * READER.H
	 */

	/* rdr_t:
	 *   The reader object who hold all informations needed to parse the input file:
	 *   the patterns and quark for labels and observations. We keep separate count
	 *   for unigrams and bigrams pattern for simpler allocation of sequences. We
	 *   also store the expected number of column in the input data to check that
	 *   pattern are appliables.
	 */
	typedef struct rdr_s rdr_t;
	struct rdr_s {
		uint32_t   npats;      //  P   Total number of patterns
		uint32_t   nuni;
		uint32_t nbi;  //      Number of unigram and bigram patterns
		uint32_t   ntoks;      //      Expected number of tokens in input
		pat_t    **pats;       // [P]  List of precompiled patterns
		qrk_t     *lbl;        //      Labels database
		qrk_t     *obs;        //      Observation database
	};

	rdr_t *rdr_new();
	void rdr_free(rdr_t *rdr);

	seq_t *rdr_raw2seq(rdr_t *rdr, const raw_t *raw, bool lbl);

	void rdr_load(rdr_t *rdr, FILE *file);

	char *rdr_readline(FILE *file);

	/*
	 * MODEL.H
	 */

	/* mdl_t:
	 *   Represent a linear-chain CRF model. The model contain both unigram and
	 *   bigram features. It is caracterized by <nlbl> the number of labels, <nobs>
	 *   the number of observations, and <nftr> the number of features.
	 *
	 *   Each observations have a corresponding entry in <kind> whose first bit is
	 *   set if the observation is unigram and second one if it is bigram. Note that
	 *   an observation can be both. An unigram observation produce Y features and a
	 *   bigram one produce Y * Y features.
	 *   The <theta> array keep all features weights. The <*off> array give for each
	 *   observations the offset in the <theta> array where the features of the
	 *   observation are stored.
	 *
	 *   The <*off> and <theta> array are initialized only when the model is
	 *   synchronized. As you can add new labels and observations after a sync, we
	 *   keep track of the old counts in <olbl> and <oblk> to detect inconsistency
	 *   and resynchronize the model if needed. In this case, if the number of
	 *   labels have not changed, the previously trained weights are kept, else they
	 *   are now meaningless so discarded.
	 */
	typedef struct mdl_s mdl_t;
	struct mdl_s {
		opt_t    *opt;     //       options for training
		int       type;    //       model type

		// Size of various model parameters
		uint32_t  nlbl;    //   Y   number of labels
		uint64_t  nobs;    //   O   number of observations
		uint64_t  nftr;    //   F   number of features

		// Informations about observations
		char     *kind;    //  [O]  observations type
		uint64_t *uoff;    //  [O]  unigram weights offset
		uint64_t *boff;    //  [O]  bigram weights offset

		// The model itself
		double   *theta;   //  [F]  features weights

		// Datasets
		rdr_t    *reader;
	};

	mdl_t *mdl_new(rdr_t *rdr);
	void mdl_free(mdl_t *mdl);
	void mdl_load(mdl_t *mdl, FILE *file);

	/*
	 * GRADIENT.H
	 */

	/* grd_st_t:
	 *   State tracker for the gradient computation. To compute the gradient we need
	 *   to perform several steps and communicate between them a lot of intermediate
	 *   values, all these temporary are stored in this object.
	 *   A tracker can be used to compute sequence of length <len> at most, before
	 *   using it you must call grd_stcheck to ensure that the tracker is big enough
	 *   for your sequence.
	 *   This tracker is used to perform single sample gradient computations or
	 *   partial gradient computation in online algorithms and for decoding with
	 *   posteriors.
	 */
	typedef struct grd_st_s grd_st_t;
	struct grd_st_s {
		mdl_t    *mdl;
		uint32_t len;     // =T        max length of sequence
		double   *g;       // [F]       vector where to put gradient updates
		double    lloss;   //           loss value for the sequence
		double   *psi;     // [T][Y][Y] the transitions scores
		double   *psiuni;  // [T][Y]    | Same as psi in sparse format
		uint32_t *psiyp;   // [T][Y][Y] |
		uint32_t *psiidx;  // [T][Y]    |
		uint32_t *psioff;  // [T]
		double   *alpha;   // [T][Y]    forward scores
		double   *beta;    // [T][Y]    backward scores
		double   *scale;   // [T]       scaling factors of forward scores
		double   *unorm;   // [T]       normalization factors for unigrams
		double   *bnorm;   // [T]       normalization factors for bigrams
		uint32_t  first;   //           first position where gradient is needed
		uint32_t  last;    //           last position where gradient is needed
	};

	grd_st_t *grd_stnew(mdl_t *mdl, double *g);
	void grd_stfree(grd_st_t *grd_st);
	void grd_stcheck(grd_st_t *grd_st, uint32_t len);

	void grd_fldopsi(grd_st_t *grd_st, const seq_t *seq);
	void grd_flfwdbwd(grd_st_t *grd_st, const seq_t *seq);

	/* grd_t:
	 *   Multi-threaded full dataset gradient computer. This is used to compute the
	 *   gradient by algorithm working on the full dataset at each iterations. It
	 *   efficiently compute it using the fact it is additive to use as many threads
	 *   as allowed.
	 */
	typedef struct grd_s grd_t;
	struct grd_s {
		mdl_t     *mdl;
		grd_st_t **grd_st;
	};

	/*
	 * DECODER.H
	 */

	void tag_viterbi(
			mdl_t *mdl,
			const seq_t *seq,
			uint32_t out[],
			double *sc,
			double psc[]
	);

	/*
	 * VMATH.H
	 */

	double *xvm_new(uint64_t N);
	void    xvm_free(double x[]);

	double xvm_unit(double r[], const double x[], uint64_t N);

	void xvm_expma(double r[], const double x[], double a, uint64_t N);


} /* namespace crawlservpp::Data::wapiti */

#endif
