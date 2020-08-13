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

#include "wapiti.h"

namespace crawlservpp::Data::wapiti {

	/*
	 * TOOLS.C
	 */

	/* xmalloc:
	 *   A simple wrapper around malloc who violently fail if memory cannot be
	 *   allocated, so it will never return nullptr.
	 */
	void *xmalloc(size_t size) {
		void *ptr = malloc(size);
		if (ptr == nullptr)
			throw std::runtime_error(
					"wapiti::xmalloc():"
					" out of memory"
			);
		return ptr;
	}

	/* xrealloc:
	 *   As xmalloc, this is a simple wrapper around realloc who fail on memory
	 *   error and so never return nullptr.
	 */
	void *xrealloc(void *ptr, size_t size) {
		void *newMemory = realloc(ptr, size);
		if(newMemory == nullptr)
			throw std::runtime_error(
					"wapiti::xrealloc():"
					" out of memory"
			);
		return newMemory;
	}

	/* xstrdup:
	 *   As the previous one, this is a safe version of xstrdup who fail on
	 *   allocation error.
	 */
	char *xstrdup(const char *str) {
		const size_t len = strlen(str) + 1;
		char *res = static_cast<char *>(xmalloc(sizeof(char) * len));
		memcpy(res, str, len);
		return res;
	}

	/******************************************************************************
	 * Netstring for persistent storage
	 *
	 *   This follow the format proposed by D.J. Bernstein for safe and portable
	 *   storage of string in persistent file and networks. This used for storing
	 *   strings in saved models.
	 *   We just add an additional end-of-line character to make the output files
	 *   more readable.
	 *
	 ******************************************************************************/

	/* ns_readstr:
	 *   Read a string from the given file in netstring format. The string is
	 *   returned as a newly allocated bloc of memory 0-terminated.
	 */
	char *ns_readstr(FILE *file) {
		uint32_t len;
		if (fscanf(file, "%" SCNu32 ":", &len) != 1)
			throw std::runtime_error(
					"wapiti::ns_readstr():"
					" cannot read from file"
			);
		char *buf = static_cast<char*>(xmalloc(len + 1));
		if (fread(buf, len, 1, file) != 1)
			throw std::runtime_error(
					"wapiti::ns_readstr():"
					" cannot read from file"
			);
		if (fgetc(file) != ',')
			throw std::runtime_error(
					"wapiti::ns_readstr():"
					" invalid format"
			);
		buf[len] = '\0';
		fgetc(file);
		return buf;
	}

	/*
	 * PATTERN.C
	 */

	/******************************************************************************
	 * A simple regular expression matcher
	 *
	 *   This module implement a simple regular expression matcher, it implement
	 *   just a subset of the classical regexp simple to implement but sufficient
	 *   for most usages and avoid to add a dependency to a full regexp library.
	 *
	 *   The recognized subset is quite simple. First for matching characters :
	 *       .  -> match any characters
	 *       \x -> match a character class (in uppercase, match the complement)
	 *               \d : digit       \a : alpha      \w : alpha + digit
	 *               \l : lowercase   \u : uppercase  \p : punctuation
	 *               \s : space
	 *             or escape a character
	 *       x  -> any other character match itself
	 *   And the constructs :
	 *       ^  -> at the begining of the regexp, anchor it at start of string
	 *       $  -> at the end of regexp, anchor it at end of string
	 *       *  -> match any number of repetition of the previous character
	 *       ?  -> optionally match the previous character
	 *
	 *   This subset is implemented quite efficiently using recursion. All recursive
	 *   calls are tail-call so they should be optimized by the compiler. As we do
	 *   direct interpretation, we have to backtrack so performance can be very poor
	 *   on specialy designed regexp. This is not a problem as the regexp as well as
	 *   the string is expected to be very simple here. If this is not the case, you
	 *   better have to prepare your data better.
	 ******************************************************************************/

	/* rex_matchit:
	 *   Match a single caracter at the start fo the string. The character might be
	 *   a plain char, a dot or char class.
	 */
	static bool rex_matchit(const char *ch, const char *str) {
		if (str[0] == '\0')
			return false;
		if (ch[0] == '.')
			return true;
		if (ch[0] == '\\') {
			switch (ch[1]) {
				case 'a': return  isalpha(str[0]);
				case 'd': return  isdigit(str[0]);
				case 'l': return  islower(str[0]);
				case 'p': return  ispunct(str[0]);
				case 's': return  isspace(str[0]);
				case 'u': return  isupper(str[0]);
				case 'w': return  isalnum(str[0]);
				case 'A': return !isalpha(str[0]);
				case 'D': return !isdigit(str[0]);
				case 'L': return !islower(str[0]);
				case 'P': return !ispunct(str[0]);
				case 'S': return !isspace(str[0]);
				case 'U': return !isupper(str[0]);
				case 'W': return !isalnum(str[0]);
				default: return ch[1] == str[0];
			}
		}
		return ch[0] == str[0];
	}

	/* rex_matchme:
	 *   Match a regular expresion at the start of the string. If a match is found,
	 *   is length is returned in len. The mathing is done through tail-recursion
	 *   for good performances.
	 */
	static bool rex_matchme(const char *re, const char *str, uint32_t *len) {
		// Special check for end of regexp
		if (re[0] == '\0')
			return true;
		if (re[0] == '$' && re[1] == '\0')
			return (str[0] == '\0');
		// Get first char of regexp
		const char *ch  = re;
		const char *nxt = re + 1 + (ch[0] == '\\');
		// Special check for the following construct "x**" where the first star
		// is consumed normally but lead the second (which is wrong) to be
		// interpreted as a char to mach as if it was escaped (and same for the
		// optional construct)
		if (*ch == '*' || *ch == '?')
			throw std::runtime_error(
					std::string(
							"wapiti::rex_matchme():"
							" unescaped * or ? in regexp: "
					)
					+ re
			);
		// Handle star repetition
		if (nxt[0] == '*') {
			nxt++;
			do {
				const uint32_t save = *len;
				if (rex_matchme(nxt, str, len))
					return true;
				*len = save + 1;
			} while (rex_matchit(ch, str++));
			return false;
		}
		// Handle optional
		if (nxt[0] == '?') {
			nxt++;
			if (rex_matchit(ch, str)) {
				(*len)++;
				if (rex_matchme(nxt, str + 1, len))
					return true;
				(*len)--;
			}
			return rex_matchme(nxt, str, len);
		}
		// Classical char matching
		(*len)++;
		if (rex_matchit(ch, str))
			return rex_matchme(nxt, str + 1, len);
		return false;
	}

	/* rex_match:
	 *   Match a regular expresion in the given string. If a match is found, the
	 *   position of the start of the match is returned and is len is returned in
	 *   len, else -1 is returned.
	 */
	static int32_t rex_match(const char *re, const char *str, uint32_t *len) {
		// Special case for anchor at start
		if (*re == '^') {
			*len = 0;
			if (rex_matchme(re + 1, str, len))
				return 0;
			return -1;
		}
		// And general case for any position
		int32_t pos = 0;
		do {
			*len = 0;
			if (rex_matchme(re, str + pos, len))
				return pos;
		} while (str[pos++] != '\0');
		// Matching failed
		return -1;
	}

	/*******************************************************************************
	 * Pattern handling
	 *
	 *   Patterns are the heart the data input process, they provide a way to tell
	 *   Wapiti how the interesting information can be extracted from the input
	 *   data. A pattern is simply a string who embed special commands about tokens
	 *   to extract from the input sequence. They are compiled to a special form
	 *   used during data loading.
	 *   For training, each position of a sequence hold a list of observation made
	 *   at this position, pattern give a way to specify these observations.
	 *
	 *   During sequence loading, all patterns are applied at each position to
	 *   produce a list of string representing the observations which will be in
	 *   turn transformed to numerical identifiers. This module take care of
	 *   building the string representation.
	 *
	 *   As said, a patern is a string with specific commands in the forms %c[...]
	 *   where 'c' is the command with arguments between the bracket. All commands
	 *   take at least to numerical arguments which define a token in the input
	 *   sequence. The first one is an offset from the current position and the
	 *   second one is a column number. With these two parameters, we get a string
	 *   in the input sequence on which we apply the command.
	 *
	 *   All command are specified with a character and result in a string which
	 *   will replace the command in the pattern string. If the command character is
	 *   lower case, the result is copied verbatim, if it is uppercase, the result
	 *   is copied with casing removed. The following commands are available:
	 *     'x' -- result is the token itself
	 *     't' -- test if a regular expression match the token. Result will be
	 *            either "true" or "false"
	 *     'm' -- match a regular expression on the token. Result is the first
	 *            substring matched.
	 ******************************************************************************/

	/* pat_comp:
	 *   Compile the pattern to a form more suitable to easily apply it on tokens
	 *   list during data reading. The given pattern string is interned in the
	 *   compiled pattern and will be freed with it, so you don't have to take care
	 *   of it and must not modify it after the compilation.
	 */
	pat_t *pat_comp(char *p) {
		pat_t *pat = nullptr;
		// Allocate memory for the compiled pattern, the allocation is based
		// on an over-estimation of the number of required item. As compiled
		// pattern take a neglectible amount of memory, this waste is not
		// important.
		uint32_t mitems = 0;
		for (uint32_t currentPos = 0; p[currentPos] != '\0'; currentPos++)
			if (p[currentPos] == '%')
				mitems++;
		mitems = mitems * 2 + 1;
		pat = static_cast<pat_t*>(xmalloc(sizeof(pat_t) + sizeof(pat->items[0]) * mitems));
		pat->src = p;
		// Next, we go through the pattern compiling the items as they are
		// found. Commands are parsed and put in a corresponding item, and
		// segment of char not in a command are put in a 's' item.
		uint32_t nitems = 0;
		uint32_t ntoks = 0;
		uint32_t pos = 0;
		while (p[pos] != '\0') {
			pat_item_t *item = &(pat->items[nitems++]);
			item->value = nullptr;
			if (p[pos] == '%') {
				// This is a command, so first parse its type and check
				// its a valid one. Next prepare the item.
				const char type = tolower(p[pos + 1]);
				if (type != 'x' && type != 't' && type != 'm')
					throw std::runtime_error(
							std::string(
									"wapiti::pat_comp():"
									" unknown command type: '%c'"
							)
							+ type
					);
				item->type = type;
				item->caps = (p[pos + 1] != type);
				pos += 2;
				// Next we parse the offset and column and store them in
				// the item.
				const char *at = p + pos;
				uint32_t col;
				int32_t off;
				int nch;
				item->absolute = false;
				if (sscanf(at, "[@%" SCNi32 ",%" SCNu32 "%n", &off, &col, &nch) == 2)
					item->absolute = true;
				else if (sscanf(at, "[%" SCNi32 ",%" SCNu32 "%n", &off, &col, &nch) != 2)
					throw std::runtime_error(
							std::string(
									"wapiti::pat_comp():"
									" invalid pattern: "
							)
							+ p
					);
				item->offset = off;
				item->column = col;
				ntoks = wapiti_max(ntoks, col);
				pos += nch;
				// And parse the end of the argument list, for 'x' there
				// is nothing to read but for 't' and 'm' we have to get
				// read the regexp.
				if (type == 't' || type == 'm') {
					if (p[pos] != ',' && p[pos + 1] != '"')
						throw std::runtime_error(
								std::string(
										"wapiti::pat_comp():"
										" missing arg in pattern: "
								)
								+ p
						);
					const int32_t start = (pos += 2);
					while (p[pos] != '\0') {
						if (p[pos] == '"')
							break;
						if (p[pos] == '\\' && p[pos+1] != '\0')
							pos++;
						pos++;
					}
					if (p[pos] != '"')
						throw std::runtime_error(
								std::string(
										"wapiti::pat_comp():"
										" unended argument: "
								)
								+ p
						);
					const int32_t len = pos - start;
					item->value = static_cast<char *>(xmalloc(sizeof(char) * (len + 1)));
					memcpy(item->value, p + start, len);
					item->value[len] = '\0';
					pos++;
				}
				// Just check the end of the arg list and loop.
				if (p[pos] != ']')
					throw std::runtime_error(
							std::string(
									"wapiti::pat_comp():"
									" missing end of pattern: "
							)
							+ p
					);
				pos++;
			} else {
				// No command here, so build an 's' item with the chars
				// until end of pattern or next command and put it in
				// the list.
				const int32_t start = pos;
				while (p[pos] != '\0' && p[pos] != '%')
					pos++;
				const int32_t len = pos - start;
				item->type  = 's';
				item->caps  = false;
				item->value = static_cast<char*>(xmalloc(sizeof(char) * (len + 1)));
				memcpy(item->value, p + start, len);
				item->value[len] = '\0';
			}
		}
		pat->ntoks = ntoks;
		pat->nitems = nitems;
		return pat;
	}

	/* pat_exec:
	 *   Execute a compiled pattern at position 'at' in the given tokens sequences
	 *   in order to produce an observation string. The string is returned as a
	 *   newly allocated memory block and the caller is responsible to free it when
	 *   not needed anymore.
	 */
	char *pat_exec(const pat_t *pat, const tok_t *tok, uint32_t at) {
		constexpr std::array literals{
			"_x-1",
			"_x-2",
			"_x-3",
			"_x-4",
			"_x-#",
			"_x+1",
			"_x+2",
			"_x+3",
			"_x+4",
			"_x+#",
			"true",
			"false"
		};

		constexpr auto bvalIndex{0};
		constexpr auto evalIndex{5};
		constexpr auto trueIndex{10};
		constexpr auto falseIndex{11};

		auto iLiteral{-1};
		auto literalOffset{0};

		const uint32_t T = tok->len;

		// Prepare the buffer who will hold the result
		uint32_t size = 16;
		uint32_t pos = 0;
		char *buffer = static_cast<char *>(xmalloc(sizeof(char) * size));

		// And loop over the compiled items
		for (uint32_t it = 0; it < pat->nitems; it++) {
			const pat_item_t *item = &(pat->items[it]);
			char *value = nullptr;
			uint32_t len = 0;

			// First, if needed, we retrieve the token at the referenced
			// position in the sequence. We store it in value and let the
			// command handler do what it need with it.
			if (item->type != 's') {
				int tokenpos = item->offset;
				if (item->absolute) {
					if (item->offset < 0)
						tokenpos += T;
					else
						tokenpos--;
				} else {
					tokenpos += at;
				}
				uint32_t col = item->column;
				if (tokenpos < 0)
					iLiteral = bvalIndex + wapiti_min(-tokenpos - 1, 4);
				else if (tokenpos >= static_cast<int64_t>(T))
					iLiteral = evalIndex + wapiti_min( tokenpos - T, 4);
				else if (col >= tok->cnts[tokenpos])
					throw std::runtime_error(
							"wapiti::pat_exec():"
							" missing tokens, cannot apply pattern"
				);
				else
					value = tok->toks[tokenpos][col];
			}

			// Next, we handle the command, 's' and 'x' are very simple but
			// 't' and 'm' require us to call the regexp matcher.
			if (item->type == 's') {
				value = item->value;
				len = strlen(value);
			} else if (item->type == 'x') {
				if(iLiteral > -1) {
					len = strlen(literals[iLiteral]);
				}
				else {
					len = strlen(value);
				}
			} else if (item->type == 't') {
				if(
					(iLiteral > -1 && rex_match(item->value, literals[iLiteral], &len) == -1)
					|| (iLiteral < 0 && rex_match(item->value, value, &len) == -1)
				) {
					iLiteral = falseIndex;
				}
				else {
					iLiteral = trueIndex;
				}
				len = strlen(literals[iLiteral]);
			} else if (item->type == 'm') {
				int32_t matchpos{0};
				if(iLiteral > -1) {
					matchpos = rex_match(item->value, literals[iLiteral], &len);
				}
				else {
					matchpos = rex_match(item->value, value, &len);
				}

				if (matchpos == -1)
					len = 0;

				if(iLiteral > -1) {
					literalOffset = matchpos;
				}
				else {
					value += matchpos;
				}

			}

			// And we add it to the buffer, growing it if needed. If the
			// user requested it, we also remove caps from the string.
			if (pos + len >= size - 1) {
				while (pos + len >= size - 1)
					size = size * speedOfGrowth;
				buffer = static_cast<char*>(xrealloc(buffer, sizeof(char) * size));
			}

			if(iLiteral > -1) {
				memcpy(buffer + pos, literals[iLiteral] + literalOffset, len - literalOffset);
			}
			else {
				memcpy(buffer + pos, value, len);
			}

			if (item->caps)
				for (uint32_t i = pos; i < pos + len; i++)
					buffer[i] = tolower(buffer[i]);
			pos += len;
		}

		// Adjust the result and return it.
		buffer[pos++] = '\0';
		buffer = static_cast<char*>(xrealloc(buffer, sizeof(char) * pos));

		return buffer;
	}

	/* pat_free:
	 *   Free all memory used by a compiled pattern object. Note that this will free
	 *   the pointer to the source string given to pat_comp so you must be sure to
	 *   not use this pointer again.
	 */
	void pat_free(pat_t *pat) {
		for (uint32_t it = 0; it < pat->nitems; it++)
			free(pat->items[it].value);
		free(pat->src);
		free(pat);
	}

	/*
	 * QUARK.C
	 */

	/******************************************************************************
	 * Map object
	 *
	 *   Implement quark object for mapping strings to identifiers through crit-bit
	 *   tree (also known as PATRICIA tries). In fact it only store a compressed
	 *   version of the trie to reduce memory footprint. The special trick of using
	 *   the last bit of the reference to differenciate between nodes and leafs come
	 *   from Daniel J. Bernstein implementation of crit-bit tree that can be found
	 *   on his web site.
	 *   [1] Morrison, Donald R. ; PATRICIA-Practical Algorithm To Retrieve
	 *   Information Coded in Alphanumeric, Journal of the ACM 15 (4): pp. 514--534,
	 *   1968. DOI:10.1145/321479.321481
	 *
	 *   This code is copyright 2002-2013 Thomas Lavergne and licenced under the BSD
	 *   Licence like the remaining of Wapiti.
	 ******************************************************************************/

	typedef struct node_s node_t;

	struct node_s {
		node_t   *child[2];
		uint32_t  pos;
		uint8_t   byte;
	};

	typedef struct leaf_s leaf_t;

	struct leaf_s {
		uint64_t  id;
		char      key __flexarr;
	};

	struct qrk_s {
		struct node_s *root;
		struct leaf_s ** leafs;
		bool     lock;
		uint64_t count;
		uint64_t size;
	};

	#define qrk_lf2nd(lf)  (reinterpret_cast<node_t *>(reinterpret_cast<intptr_t>(lf) |  1))
	#define qrk_nd2lf(nd)  (reinterpret_cast<leaf_t *>(reinterpret_cast<intptr_t>(nd) & ~1))
	#define qrk_isleaf(nd) (reinterpret_cast<intptr_t>(nd) & 1)

	/* qrk_new:
	 *   This initialize the object for holding a new empty trie, with some pre-
	 *   allocations. The returned object must be freed with a call to qrk_free when
	 *   not needed anymore.
	 */
	qrk_t *qrk_new(void) {
		constexpr uint64_t size{128};
		qrk_t *qrk = static_cast<qrk_t*>(xmalloc(sizeof(qrk_t)));
		qrk->root  = nullptr;
		qrk->count = 0;
		qrk->lock  = false;
		qrk->size  = size;
		qrk->leafs = static_cast<leaf_s**>(xmalloc(sizeof(leaf_t *) * size));
		return qrk;
	}

	/* qrk_free:
	 *   Release all the memory used by a qrk_t object allocated with qrk_new. This
	 *   will release all key string stored internally so all key returned by
	 *   qrk_unmap become invalid and must not be used anymore.
	 */
	void qrk_free(qrk_t *qrk) {
		const uint32_t stkmax = 1024;
		if (qrk->count != 0) {
			node_t *stk[stkmax];
			uint32_t cnt = 0;
			stk[cnt++] = reinterpret_cast<node_t*>(qrk->root);
			while (cnt != 0) {
				node_t *nd = stk[--cnt];
				if (qrk_isleaf(nd)) {
					free(qrk_nd2lf(nd));
					continue;
				}
				stk[cnt++] = nd->child[0];
				stk[cnt++] = nd->child[1];
				free(nd);
			}
		}
		free(qrk->leafs);
		free(qrk);
	}

	/* qrk_str2id:
	 *   Map a key to a uniq identifier. If the key already exist in the map, return
	 *   its identifier, else allocate a new identifier and insert the new (key,id)
	 *   pair inside the quark. This function is not thread safe and should not be
	 *   called on the same map from different thread without locking.
	 */
	uint64_t qrk_str2id(qrk_t *qrk, const char *key) {
		const uint8_t *raw = reinterpret_cast<const uint8_t *>(key);
		const size_t   len = strlen(key);
		// We first take care of the empty trie case so later we can safely
		// assume that the trie is well formed and so there is no nullptr pointers
		// in it.
		if (qrk->count == 0) {
			if (qrk->lock == true)
				return wapiti_none;
			const size_t s = sizeof(char) * (len + 1);
			leaf_t *l = static_cast<leaf_t *>(xmalloc(sizeof(leaf_t) + s));
			memcpy(l->key, key, s);
			l->id = 0;
			qrk->root = qrk_lf2nd(l);
			qrk->leafs[0] = l;
			qrk->count = 1;
			return 0;
		}
		// If the trie is not empty, we first go down the trie to the leaf like
		// if we are searching for the key. When at leaf there is two case,
		// either we have found our key or we have found another key with all
		// its critical bit identical to our one. So we search for the first
		// differing bit between them to know where we have to add the new node.
		const node_t *nd = qrk->root;
		while (!qrk_isleaf(nd)) {
			const uint8_t c = nd->pos < len ? raw[nd->pos] : 0;
			const int s = ((c | nd->byte) + 1) >> 8;
			nd = nd->child[s];
		}
		const char *bst = qrk_nd2lf(nd)->key;
		size_t pos;
		for (pos = 0; pos < len; pos++)
			if (key[pos] != bst[pos])
				break;
		uint8_t byte;
		if (pos != len)
			byte = key[pos] ^ bst[pos];
		else if (bst[pos] != '\0')
			byte = bst[pos];
		else
			return qrk_nd2lf(nd)->id;
		if (qrk->lock == true)
			return wapiti_none;
		// Now we known the two key are different and we know in which byte. It
		// remain to build the mask for the new critical bit and build the new
		// internal node and leaf.
		while (byte & (byte - 1))
			byte &= byte - 1;
		byte ^= byteMax;
		const uint8_t chr = bst[pos];
		const int side = ((chr | byte) + 1) >> 8;
		const size_t size = sizeof(char) * (len + 1);
		node_t *nx = static_cast<node_t*>(xmalloc(sizeof(node_t)));
		leaf_t *lf = static_cast<leaf_t*>(xmalloc(sizeof(leaf_t) + size));
		memcpy(lf->key, key, size);
		lf->id   = qrk->count++;
		nx->pos  = pos;
		nx->byte = byte;
		nx->child[1 - side] = qrk_lf2nd(lf);
		if (lf->id == qrk->size) {
			qrk->size *= speedOfGrowth;
			const size_t s = sizeof(leaf_t *) * qrk->size;
			qrk->leafs = static_cast<leaf_s**>(xrealloc(qrk->leafs, s));
		}
		qrk->leafs[lf->id] = lf;
		// And last thing to do: inserting the new node in the trie. We have to
		// walk down the trie again as we have to keep the ordering of nodes. So
		// we search for the good position to insert it.
		node_t **trg = &qrk->root;
		while (true) {
			node_t *n = *trg;
			if (qrk_isleaf(n) || n->pos > pos)
				break;
			if (n->pos == pos && n->byte > byte)
				break;
			const uint8_t c = n->pos < len ? raw[n->pos] : 0;
			const int s = ((c | n->byte) + 1) >> 8;
			trg = &n->child[s];
		}
		nx->child[side] = *trg;
		*trg = nx;
		return lf->id;
	}

	/* qrk_load:
	 *   Load a list of key from the given file and add them to the map. Each lines
	 *   of the file is taken as a single key and mapped to the next available id if
	 *   not already present. If all keys are single lines and the given map is
	 *   initilay empty, this will load a map exactly as saved by qrk_save.
	 */
	void qrk_load(qrk_t *qrk, FILE *file) {
		uint64_t cnt = 0;
		if (fscanf(file, "#qrk#%" SCNu64 "\n", &cnt) != 1) {
			if (ferror(file) != 0)
				throw std::runtime_error(
						"wapiti::qrk_load():"
						" cannot read from file"
				);
			throw std::runtime_error(
					"wapiti::qrk_load():"
					" invalid format"
			);
		}
		for (uint64_t n = 0; n < cnt; ++n) {
			char *str = ns_readstr(file);
			qrk_str2id(qrk, str);
			free(str);
		}
	}

	/* qrk_count:
	 *   Return the number of mappings stored in the quark.
	 */
	uint64_t qrk_count(const qrk_t *qrk) {
		return qrk->count;
	}

	/* qrk_lock:
	 *   Set the lock value of the quark and return the old one.
	 */
	bool qrk_lock(qrk_t *qrk, bool lock) {
		bool old = qrk->lock;
		qrk->lock = lock;
		return old;
	}

	/* qrk_id2str:
	 *    Retrieve the key associated to an identifier. The key is returned as a
	 *    constant string that should not be modified or freed by the caller, it is
	 *    a pointer to the internal copy of the key kept by the map object and
	 *    remain valid only for the life time of the quark, a call to qrk_free will
	 *    make this pointer invalid.
	 */
	const char *qrk_id2str(const qrk_t *qrk, uint64_t id) {
		if (id >= qrk->count)
			throw std::runtime_error(
					"wapiti::qrk_id2str():"
					" invalid identifier"
			);
		return qrk->leafs[id]->key;
	}

	/*
	 * SEQUENCE.C
	 */

	/*
	 * READER.C
	 */

	/*******************************************************************************
	 * Datafile reader
	 *
	 *   And now come the data file reader which use the previous module to parse
	 *   the input data in order to produce seq_t objects representing interned
	 *   sequences.
	 *
	 *   This is where the sequence will go through the tree steps to build seq_t
	 *   objects used internally. There is two way do do this. First the simpler is
	 *   to use the rdr_readseq function which directly read a sequence from a file
	 *   and convert it to a seq_t object transparently. This is how the training
	 *   and development data are loaded.
	 *   The second way consist of read a raw sequence with rdr_readraw and next
	 *   converting it to a seq_t object with rdr_raw2seq. This allow the caller to
	 *   keep the raw sequence and is used by the tagger to produce a clean output.
	 *
	 *   There is no public interface to the tok_t object as it is intended only for
	 *   internal use in the reader as an intermediate step to apply patterns.
	 ******************************************************************************/

	/* rdr_new:
	 *   Create a new empty reader object. If no patterns are loaded before you
	 *   start using the reader the input data are assumed to be already prepared
	 *   list of features. They must either start with a prefix 'u', 'b', or '*', or
	 *   you must set autouni to true in order to automatically add a 'u' prefix.
	 */
	rdr_t *rdr_new() {
		rdr_t *rdr = static_cast<rdr_t *>(xmalloc(sizeof(rdr_t)));
		rdr->npats = rdr->nuni = rdr->nbi = 0;
		rdr->ntoks = 0;
		rdr->pats = nullptr;
		rdr->lbl = qrk_new();
		rdr->obs = qrk_new();
		return rdr;
	}

	/* rdr_free:
	 *   Free all memory used by a reader object including the quark database, so
	 *   any string returned by them must not be used after this call.
	 */
	void rdr_free(rdr_t *rdr) {
		for (uint32_t i = 0; i < rdr->npats; i++)
			pat_free(rdr->pats[i]);
		free(rdr->pats);
		qrk_free(rdr->lbl);
		qrk_free(rdr->obs);
		free(rdr);
	}

	/* rdr_readline:
		 *   Read an input line from <file>. The line can be of any size limited only by
		 *   available memory, a buffer large enough is allocated and returned. The
		 *   caller is responsible to free it. On end-of-file, nullptr is returned.
		 */
	char *rdr_readline(FILE *file) {
		if (feof(file))
			return nullptr;
		// Initialize the buffer
		uint32_t len = 0;
		uint32_t size = 16;
		char *buffer = static_cast<char *>(xmalloc(size));
		// We read the line chunk by chunk until end of line, file or error
		while (!feof(file)) {
			if (fgets(buffer + len, size - len, file) == nullptr) {
				// On nullptr return there is two possible cases, either an
				// error or the end of file
				if (ferror(file))
					throw std::runtime_error(
							"wapiti::rdr_readline():"
							" cannot read from file"
					);
				// On end of file, we must check if we have already read
				// some data or not
				if (len == 0) {
					free(buffer);
					return nullptr;
				}
				break;
			}
			// Check for end of line, if this is not the case enlarge the
			// buffer and go read more data
			len += strlen(buffer + len);
			if (len == size - 1 && buffer[len - 1] != '\n') {
				size = size * speedOfGrowth;
				buffer = static_cast<char *>(xrealloc(buffer, size));
				continue;
			}
			break;
		}
		// At this point empty line should have already catched so we just
		// remove the end of line if present and resize the buffer to fit the
		// data
		if (buffer[len - 1] == '\n')
			buffer[--len] = '\0';
		return static_cast<char *>(xrealloc(buffer, len + 1));
	}

	/* rdr_rawtok2seq:
	 *   Convert a tok_t to a seq_t object taking each tokens as a feature without
	 *   applying patterns.
	 */
	static seq_t *rdr_rawtok2seq(rdr_t *rdr, const tok_t *tok) {
		const uint32_t T = tok->len;
		uint32_t size = 0;
		for (uint32_t t = 0; t < T; t++) {
			for (uint32_t n = 0; n < tok->cnts[t]; n++) {
				const char *o = tok->toks[t][n];
				switch (o[0]) {
					case 'u': size += 1; break;
					case 'b': size += 1; break;
					case '*': size += 2; break;
					default:
						throw std::runtime_error(
								std::string(
										"wapiti::rdr_rawtok2seq():"
										" invalid feature "
								) + o
						);
				}
			}
		}
		seq_t *seq = static_cast<seq_t *>(xmalloc(sizeof(seq_t) + sizeof(pos_t) * T));
		seq->raw = static_cast<uint64_t*>(xmalloc(sizeof(uint64_t) * size));
		seq->len = T;
		uint64_t *raw = seq->raw;
		for (uint32_t t = 0; t < T; t++) {
			seq->pos[t].lbl = std::numeric_limits<uint32_t>::max();
			seq->pos[t].ucnt = 0;
			seq->pos[t].uobs = raw;
			for (uint32_t n = 0; n < tok->cnts[t]; n++) {
				if (tok->toks[t][n][0] == 'b')
					continue;
				uint64_t id = qrk_str2id(rdr->obs, tok->toks[t][n]);
				if (id != wapiti_none) {
					(*raw++) = id;
					seq->pos[t].ucnt++;
				}
			}
			seq->pos[t].bcnt = 0;
			seq->pos[t].bobs = raw;
			for (uint32_t n = 0; n < tok->cnts[t]; n++) {
				if (tok->toks[t][n][0] == 'u')
					continue;
				uint64_t id = qrk_str2id(rdr->obs, tok->toks[t][n]);
				if (id != wapiti_none) {
					(*raw++) = id;
					seq->pos[t].bcnt++;
				}
			}
		}
		// And finally, if the user specified it, populate the labels
		if (tok->lbl != nullptr) {
			for (uint32_t t = 0; t < T; t++) {
				const char *lbl = tok->lbl[t];
				uint64_t id = qrk_str2id(rdr->lbl, lbl);
				seq->pos[t].lbl = id;
			}
		}
		return seq;
	}

	/* rdr_pattok2seq:
	 *   Convert a tok_t to a seq_t object by applying the patterns of the reader.
	 */
	static seq_t *rdr_pattok2seq(rdr_t *rdr, const tok_t *tok) {
		const uint32_t T = tok->len;
		// So now the tok object is ready, we can start building the seq_t
		// object by appling patterns. First we allocate the seq_t object. The
		// sequence itself as well as the sub array are allocated in one time.
		seq_t *seq = static_cast<seq_t*>(xmalloc(sizeof(seq_t) + sizeof(pos_t) * T));
		seq->raw = static_cast<uint64_t*>(xmalloc(sizeof(uint64_t) * (rdr->nuni + rdr->nbi) * T));
		seq->len = T;
		uint64_t *tmp = seq->raw;
		for (uint32_t t = 0; t < T; t++) {
			seq->pos[t].lbl  = std::numeric_limits<uint32_t>::max();
			seq->pos[t].uobs = tmp; tmp += rdr->nuni;
			seq->pos[t].bobs = tmp; tmp += rdr->nbi;
		}
		// Next, we can build the observations list by applying the patterns on
		// the tok_t sequence.
		for (uint32_t t = 0; t < T; t++) {
			pos_t *pos = &seq->pos[t];
			pos->ucnt = 0;
			pos->bcnt = 0;
			for (uint32_t x = 0; x < rdr->npats; x++) {
				// Get the observation and map it to an identifier
				char *obs = pat_exec(rdr->pats[x], tok, t);
				uint64_t id =  qrk_str2id(rdr->obs, obs);
				if (id == wapiti_none) {
					free(obs);
					continue;
				}
				// If the observation is ok, add it to the lists
				char kind = 0;
				switch (obs[0]) {
					case 'u': kind = kind1; break;
					case 'b': kind = kind2; break;
					case '*': kind = kind3; break;
					default: break;
				}
				if (kind & 1)
					pos->uobs[pos->ucnt++] = id;
				if (kind & 2)
					pos->bobs[pos->bcnt++] = id;
				free(obs);
			}
		}
		// And finally, if the user specified it, populate the labels
		if (tok->lbl != nullptr) {
			for (uint32_t t = 0; t < T; t++) {
				const char *lbl = tok->lbl[t];
				uint64_t id = qrk_str2id(rdr->lbl, lbl);
				seq->pos[t].lbl = id;
			}
		}
		return seq;
	}

	/* rdr_raw2seq:
	 *   Convert a raw sequence to a seq_t object suitable for training or
	 *   labelling. If lbl is true, the last column is assumed to be a label and
	 *   interned also.
	 */
	seq_t *rdr_raw2seq(rdr_t *rdr, const raw_t *raw, bool lbl) {
		const uint32_t T = raw->len;
		// Allocate the tok_t object, the label array is allocated only if they
		// are requested by the user.
		tok_t *tok = static_cast<tok_t*>(xmalloc(sizeof(tok_t) + T * sizeof(char **)));
		tok->cnts = static_cast<uint32_t*>(xmalloc(sizeof(uint32_t) * T));
		tok->lbl = nullptr;
		if (lbl == true)
			tok->lbl = static_cast<char**>(xmalloc(sizeof(char *) * T));
		// We now take the raw sequence line by line and split them in list of
		// tokens. To reduce memory fragmentation, the raw line is copied and
		// his reference is kept by the first tokens, next tokens are pointer to
		// this copy.
		for (uint32_t t = 0; t < T; t++) {
			// Get a copy of the raw line skiping leading space characters
			const char *src = raw->lines[t];
			while (isspace(*src))
				src++;
			char *line = xstrdup(src);
			// Split it in tokens
			std::unique_ptr<char*> toks(new char*[strlen(line) / 2 + 1]);
			uint32_t cnt = 0;
			while (*line != '\0') {
				toks.get()[cnt++] = line;
				while (*line != '\0' && !isspace(*line))
					line++;
				if (*line == '\0')
					break;
				*line++ = '\0';
				while (*line != '\0' && isspace(*line))
					line++;
			}
			// If user specified that data are labelled, move the last token
			// to the label array.
			if (lbl == true) {
				tok->lbl[t] = toks.get()[cnt - 1];
				cnt--;
			}
			// And put the remaining tokens in the tok_t object
			tok->cnts[t] = cnt;
			tok->toks[t] = static_cast<char **>(xmalloc(sizeof(char *) * cnt));
			memcpy(tok->toks[t], toks.get(), sizeof(char *) * cnt);
		}
		tok->len = T;
		// Convert the tok_t to a seq_t
		seq_t *seq = nullptr;
		if (rdr->npats == 0)
			seq = rdr_rawtok2seq(rdr, tok);
		else
			seq = rdr_pattok2seq(rdr, tok);
		// Before returning the sequence, we have to free the tok_t
		for (uint32_t t = 0; t < T; t++) {
			if (tok->cnts[t] == 0)
				continue;
			free(tok->toks[t][0]);
			free(tok->toks[t]);
		}
		free(tok->cnts);
		if (lbl == true)
			free(tok->lbl);
		free(tok);
		return seq;
	}

	/* rdr_load:
	 *   Read from the given file a reader saved previously with rdr_save. The given
	 *   reader must be empty, comming fresh from rdr_new. Be carefull that this
	 *   function performs almost no checks on the input data, so if you modify the
	 *   reader and make a mistake, it will probably result in a crash.
	 */
	void rdr_load(rdr_t *rdr, FILE *file) {
		int autouni = 0;
		fpos_t pos;
		fgetpos(file, &pos);
		if (fscanf(file, "#rdr#%" PRIu32 "/%" PRIu32 "/%d\n",
				&rdr->npats, &rdr->ntoks, &autouni) != rdr3) {
			// This for compatibility with previous file format
			fsetpos(file, &pos);
			if (fscanf(file, "#rdr#%" PRIu32 "/%" PRIu32 "\n",
					&rdr->npats, &rdr->ntoks) != 2)
				throw std::runtime_error(
						"wapiti::rdr_load():"
						" broken file, invalid reader format"
				);
		}
		rdr->nuni = rdr->nbi = 0;
		if (rdr->npats != 0) {
			rdr->pats = static_cast<pat_t**>(xmalloc(sizeof(pat_t *) * rdr->npats));
			for (uint32_t p = 0; p < rdr->npats; p++) {
				char *pat = ns_readstr(file);
				rdr->pats[p] = pat_comp(pat);
				switch (tolower(pat[0])) {
					case 'u': rdr->nuni++; break;
					case 'b': rdr->nbi++;  break;
					case '*': rdr->nuni++;
							  rdr->nbi++;  break;
					default:
						break;
				}
			}
		}
		qrk_load(rdr->lbl, file);
		qrk_load(rdr->obs, file);
	}



	/*
	 * MODEL.C
	 */

	/*******************************************************************************
	 * Linear chain CRF model
	 *
	 *   There is three concept that must be well understand here, the labels,
	 *   observations, and features. The labels are the values predicted by the
	 *   model at each point of the sequence and denoted by Y. The observations are
	 *   the values, at each point of the sequence, given to the model in order to
	 *   predict the label and denoted by O. A feature is a test on both labels and
	 *   observations, denoted by F. In linear chain CRF there is two kinds of
	 *   features :
	 *     - unigram feature who represent a test on the observations at the current
	 *       point and the label at current point.
	 *     - bigram feature who represent a test on the observation at the current
	 *       point and two labels : the current one and the previous one.
	 *   So for each observation, there Y possible unigram features and Y*Y possible
	 *   bigram features. The kind of features used by the model for a given
	 *   observation depend on the pattern who generated it.
	 ******************************************************************************/

	/* mdl_new:
	 *   Allocate a new empty model object linked with the given reader. The model
	 *   have to be synchronized before starting training or labelling. If you not
	 *   provide a reader (as it will loaded from file for example) you must be sure
	 *   to set one in the model before any attempts to synchronize it.
	 */
	mdl_t *mdl_new(rdr_t *rdr) {
		mdl_t *mdl = static_cast<mdl_t *>(xmalloc(sizeof(mdl_t)));
		mdl->nlbl   = mdl->nobs  = mdl->nftr = 0;
		mdl->kind   = nullptr;
		mdl->uoff   = mdl->boff  = nullptr;
		mdl->theta  = nullptr;
		mdl->reader = rdr;
		return mdl;
	}

	/* mdl_free:
	 *   Free all memory used by a model object inculding the reader and datasets
	 *   loaded in the model.
	 */
	void mdl_free(mdl_t *mdl) {
		free(mdl->kind);
		free(mdl->uoff);
		free(mdl->boff);
		if (mdl->theta != nullptr)
			xvm_free(mdl->theta);
		if (mdl->reader != nullptr)
			rdr_free(mdl->reader);
		free(mdl);
	}

	/* mdl_sync:
	 *   Synchronize the model with its reader. As the model is just a placeholder
	 *   for features weights and interned sequences, it know very few about the
	 *   labels and observations, all the informations are kept in the reader. A
	 *   sync will get the labels and observations count as well as the observation
	 *   kind from the reader and build internal structures representing the model.
	 *
	 *   If the model was already synchronized before, there is an existing model
	 *   incompatible with the new one to be created. In this case there is two
	 *   possibility :
	 *     - If only new observations was added, the weights of the old ones remain
	 *       valid and are kept as they form a probably good starting point for
	 *       training the new model, the new observation get a 0 weight ;
	 *     - If new labels was added, the old model are trully meaningless so we
	 *       have to fully discard them and build a new empty model.
	 *   In any case, you must never change existing labels or observations, if this
	 *   happen, you need to create a new model and destroy this one.
	 *
	 *   After synchronization, the labels and observations databases are locked to
	 *   prevent new one to be created. You must unlock them explicitly if needed.
	 *   This reduce the risk of mistakes.
	 */
	void mdl_sync(mdl_t *mdl) {
		const uint32_t Y = qrk_count(mdl->reader->lbl);
		const uint64_t O = qrk_count(mdl->reader->obs);
		// If model is already synchronized, do nothing and just return
		if (mdl->nlbl == Y && mdl->nobs == O)
			return;
		if (Y == 0 || O == 0)
			throw std::runtime_error(
					"wapiti::mdl_sync():"
					" cannot synchronize an empty model"
			);
		// If new labels was added, we have to discard all the model. In this
		// case we also display a warning as this is probably not expected by
		// the user. If only new observations was added, we will try to expand
		// the model.
		uint64_t oldF = mdl->nftr;
		uint64_t oldO = mdl->nobs;
		if (mdl->nlbl != Y && mdl->nlbl != 0)
			throw std::runtime_error(
					"wapiti::mdl_sync():"
					" labels count changed"
			);
		mdl->nlbl = Y;
		mdl->nobs = O;

		// Allocate the observations datastructure. If the model is empty or
		// discarded, a new one iscreated, else the old one is expanded.
		char     *kind = static_cast<char *>(xrealloc(mdl->kind, sizeof(char    ) * O));
		uint64_t *uoff = static_cast<uint64_t *>(xrealloc(mdl->uoff, sizeof(uint64_t) * O));
		uint64_t *boff = static_cast<uint64_t *>(xrealloc(mdl->boff, sizeof(uint64_t) * O));
		mdl->kind = kind;
		mdl->uoff = uoff;
		mdl->boff = boff;

		// Now, we can setup the features. For each new observations we fill the
		// kind and offsets arrays and count total number of features as well.
		uint64_t F = oldF;
		for (uint64_t o = oldO; o < O; o++) {
			const char *obs = qrk_id2str(mdl->reader->obs, o);
			switch (obs[0]) {
				case 'u': kind[o] = kind1; break;
				case 'b': kind[o] = kind2; break;
				case '*': kind[o] = kind3; break;
				default: break;
			}
			if (kind[o] & 1)
				uoff[o] = F, F += Y;
			if (kind[o] & 2)
				boff[o] = F, F += Y * Y;
		}
		mdl->nftr = F;
		// We can finally grow the features weights vector itself. We set all
		// the new features to 0.0 but don't touch the old ones.
		// This is a bit tricky as aligned malloc cannot be simply grown so we
		// have to allocate a new vector and copy old values ourself.
		if (oldF != 0) {
			double *newTheta = xvm_new(F);
			for (uint64_t f = 0; f < oldF; f++)
				newTheta[f] = mdl->theta[f];
			xvm_free(mdl->theta);
			mdl->theta = newTheta;
		} else {
			mdl->theta = xvm_new(F);
		}
		for (uint64_t f = oldF; f < F; f++)
			mdl->theta[f] = 0.0;

		// And lock the databases
		qrk_lock(mdl->reader->lbl, true);
		qrk_lock(mdl->reader->obs, true);
	}

	/* mdl_load:
	 *   Read back a previously saved model to continue training or start labeling.
	 *   The returned model is synced and the quarks are locked. You must give to
	 *   this function an empty model fresh from mdl_new.
	 */
	void mdl_load(mdl_t *mdl, FILE *file) {
		/*
		 * fix of possible locale bug
		 * 	see https://github.com/kermitt2/grobid/issues/43
		 * 	for more information
		 */
		const auto * previousLocale{std::setlocale(LC_ALL, "C")};
		/*
		 * end of fix
		 */

		uint64_t nact = 0;
		int type;
		if (fscanf(file, "#mdl#%d#%" SCNu64 "\n", &type, &nact) == 2) {
			mdl->type = type;
		} else {
			rewind(file);
			if (fscanf(file, "#mdl#%" SCNu64 "\n", &nact) == 1)
				mdl->type = 0;
			else
				throw std::runtime_error(
						"wapiti::mdl_load():"
						" invalid model format"
				);
		}
		rdr_load(mdl->reader, file);
		mdl_sync(mdl);
		for (uint64_t i = 0; i < nact; i++) {
			uint64_t f;
			double v;
			if (fscanf(file, "%" SCNu64 "=%la\n", &f, &v) != 2)
				throw std::runtime_error(
						"wapiti::mdl_load():"
						" invalid model format"
				);
			mdl->theta[f] = v;
		}

		/*
		 * fix of possible locale bug
		 * 	see https://github.com/kermitt2/grobid/issues/43
		 * 	for more information
		 */
		std::setlocale(LC_ALL, previousLocale);
		/*
		 * end of fix
		 */
	}

	/*
	 * GRADIENT.C
	 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

	/******************************************************************************
	 * Linear-chain CRF gradient computation
	 *
	 *   This section is responsible for computing the gradient of the
	 *   log-likelihood function to optimize over a single sequence.
	 *
	 *   There is two version of this code, one using dense matrix and one with
	 *   sparse matrix. The sparse version use the fact that for L1 regularized
	 *   trainers, the bigrams scores will be very sparse so there is a way to
	 *   reduce the amount of computation needed in the forward backward at the
	 *   price of a more complex implementation. Due to the fact that using a sparse
	 *   matrix have a cost, this implementation is slower on L2 regularized models
	 *   and on lighty L1-regularized models, this is why there is also a classical
	 *   dense version of the algorithm used for example by the L-BFGS trainer.
	 *
	 *   The sparse matrix implementation is a bit tricky because we need to store
	 *   all values in sequences in order to use the vector exponential who gives
	 *   also a lot of performance improvement on vector able machine.
	 *   We need four arrays noted <val>, <off>, <idx>, and <yp>. For each positions
	 *   t, <off>[t] value indicate where the non-zero values for t starts in <val>.
	 *   The other arrays gives the y and yp indices of these values. The easier one
	 *   to retrieve is yp, the yp indice for value at <val>[<off>[t] + n] is stored
	 *   at the same position in <yp>.
	 *   The y are more difficult: the indice y are stored with n between <idx>[y-1]
	 *   and <idx>[y]. It may seems inefective but the matrix is indexed in the
	 *   other way, we go through the idx array, and for each y we get the yp and
	 *   values, so in practice it's very efficient.
	 *
	 *   This can seem too complex but we have to keep in mind that Y are generally
	 *   very low and any sparse-matrix have overhead so we have to reduce it to the
	 *   minimum in order to get a real improvment. Dedicated library are optimized
	 *   for bigger matrix where the overhead is not a so important problem.
	 *   Another problem here is cache size. The optimization process will last most
	 *   of his time in this function so it have to be well optimized and we already
	 *   need a lot of memory for other data so we have to be carefull here if we
	 *   don't want to flush the cache all the time. Sparse matrix require less
	 *   memory than dense one only if we now in advance the number of non-zero
	 *   entries, which is not the case here, so we have to use a scheme which in
	 *   the worst case use as less as possible memory.
	 ******************************************************************************/

	/* grd_fldopsi:
	 *   We first have to compute the Ψ_t(y',y,x) weights defined as
	 *       Ψ_t(y',y,x) = \exp( ∑_k θ_k f_k(y',y,x_t) )
	 *   So at position 't' in the sequence, for each couple (y',y) we have to sum
	 *   weights of all features. Only the observations present at this position
	 *   will have a non-nul weight so we can sum only on thoses. As we use only two
	 *   kind of features: unigram and bigram, we can rewrite this as
	 *       \exp (  ∑_k μ_k(y, x_t)     f_k(y, x_t)
	 *             + ∑_k λ_k(y', y, x_t) f_k(y', y, x_t) )
	 *   Where the first sum is over the unigrams features and the second is over
	 *   bigrams ones.
	 *   This allow us to compute Ψ efficiently in three steps
	 *     1/ we sum the unigrams features weights by looping over actives
	 *          unigrams observations. (we compute this sum once and use it
	 *          for each value of y')
	 *     2/ we add the bigrams features weights by looping over actives
	 *          bigrams observations (we don't have to do this for t=0 since
	 *          there is no bigrams here)
	 *     3/ we take the component-wise exponential of the resulting matrix
	 *          (this can be done efficiently with vector maths)
	 */
	void grd_fldopsi(grd_st_t *grd_st, const seq_t *seq) {
		const mdl_t *mdl = grd_st->mdl;
		const double  *x = mdl->theta;
		const uint32_t Y = mdl->nlbl;
		const uint32_t T = seq->len;
		for (uint32_t t = 0; t < T; t++) {
			const pos_t *pos = &(seq->pos[t]);
			for (uint32_t y = 0; y < Y; y++) {
				double sum = 0.0;
				for (uint32_t n = 0; n < pos->ucnt; n++) {
					const uint64_t o = pos->uobs[n];
					sum += x[mdl->uoff[o] + y];
				}
				for (uint32_t yp = 0; yp < Y; yp++)
					map(grd_st->psi, Y, Y, t, yp, y) = sum;
			}
		}
		for (uint32_t t = 1; t < T; t++) {
			const pos_t *pos = &(seq->pos[t]);
			for (uint32_t yp = 0, d = 0; yp < Y; yp++) {
				for (uint32_t y = 0; y < Y; y++, d++) {
					double sum = 0.0;
					for (uint32_t n = 0; n < pos->bcnt; n++) {
						const uint64_t o = pos->bobs[n];
						sum += x[mdl->boff[o] + d];
					}
					map(grd_st->psi, Y, Y, t, yp, y) += sum;
				}
			}
		}
		xvm_expma(
				grd_st->psi,
				grd_st->psi,
				0.0,
				static_cast<uint64_t>(T) * Y * Y
		);
	}

	/* grd_flfwdbwd:
	 *   Now, we go to the forward-backward algorithm. As this part of the code rely
	 *   on a lot of recursive sums and products of exponentials, we have to take
	 *   care of numerical problems.
	 *   First the forward recursion
	 *       | α_1(y) = Ψ_1(y,x)
	 *       | α_t(y) = ∑_{y'} α_{t-1}(y') * Ψ_t(y',y,x)
	 *   Next come the backward recursion which is very similar
	 *       | β_T(y') = 1
	 *       | β_t(y') = ∑_y β_{t+1}(y) * Ψ_{t+1}(y',y,x)
	 *   The numerical problems can appear here. To solve them we will scale the α_t
	 *   and β_t vectors so they sum to 1 but we have to keep the scaling coeficient
	 *   as we will need them later.
	 *   Now, we have to compute the nomalization factor. But, due to the scaling
	 *   performed during the forward-backward recursions, we have to compute it at
	 *   each positions and separately for unigrams and bigrams using
	 *       for unigrams: Z_θ(t) = ∑_y α_t(y) β_t(y)
	 *       for bigrams:  Z_θ(t) = ∑_y α_t(y) β_t(y) / α-scale_t
	 *   with α-scale_t the scaling factor used for the α vector at position t
	 *   in the forward recursion.
	 */
	void grd_flfwdbwd(grd_st_t *grd_st, const seq_t *seq) {
		const mdl_t *mdl = grd_st->mdl;
		const uint64_t Y = mdl->nlbl;
		const uint32_t T = seq->len;
		double  *scale        =         grd_st->scale;
		double  *unorm        =         grd_st->unorm;
		double  *bnorm        =         grd_st->bnorm;
		for (uint32_t y = 0; y < Y; y++)
			map(grd_st->alpha, Y, 0, y) = map(grd_st->psi, Y, Y, 0, 0, y);
		scale[0] = xvm_unit(row(grd_st->alpha, Y, 0), row(grd_st->alpha, Y, 0), Y);
		for (uint32_t t = 1; t < grd_st->last + 1; t++) {
			for (uint32_t y = 0; y < Y; y++) {
				double sum = 0.0;
				for (uint32_t yp = 0; yp < Y; yp++)
					sum += map(grd_st->alpha, Y, t - 1, yp) * map(grd_st->psi, Y, Y, t, yp, y);
				map(grd_st->alpha, Y, t, y) = sum;
			}
			scale[t] = xvm_unit(row(grd_st->alpha, Y, t), row(grd_st->alpha, Y, t), Y);
		}
		for (uint32_t yp = 0; yp < Y; yp++)
			map(grd_st->beta, Y, T - 1, yp) = 1.0 / Y;
		for (uint32_t t = T - 1; t > grd_st->first; t--) {
			for (uint32_t yp = 0; yp < Y; yp++) {
				double sum = 0.0;
				for (uint32_t y = 0; y < Y; y++)
					sum += map(grd_st->beta, Y, t, y) * map(grd_st->psi, Y, Y, t, yp, y);
				map(grd_st->beta, Y, t - 1, yp) = sum;
			}
			xvm_unit(row(grd_st->beta, Y, t - 1), row(grd_st->beta, Y, t - 1), Y);
		}
		for (uint32_t t = 0; t < T; t++) {
			double z = 0.0;
			for (uint32_t y = 0; y < Y; y++)
				z += map(grd_st->alpha, Y, t, y) * map(grd_st->beta, Y, t, y);
			unorm[t] = 1.0 / z;
			bnorm[t] = scale[t] / z;
		}
	}

#pragma GCC diagnostic pop

	/******************************************************************************
	 * Dataset gradient computation
	 *
	 *   This section is responsible for computing the gradient of the
	 *   log-likelihood function to optimize over the full training set.
	 *
	 *   The gradient computation is multi-threaded, you first have to call the
	 *   function 'grd_setup' to prepare the workers pool, and next you can use
	 *   'grd_gradient' to ask for the full gradient as many time as you want. Each
	 *   time the gradient is computed over the full training set, using the curent
	 *   value of the parameters and applying the regularization. If need the
	 *   pseudo-gradient can also be computed. When you have done, you have to call
	 *   'grd_cleanup' to free the allocated memory.
	 *
	 *   This require an additional vector of size <nftr> per thread after the
	 *   first, so it can take a lot of memory to compute big models on a lot of
	 *   threads. It is strongly discouraged to ask for more threads than you have
	 *   cores, or to more thread than you have memory to hold vectors.
	 ******************************************************************************/

	/* grd_stcheck:
	 *   Check that enough memory is allocated in the gradient object so that the
	 *   linear-chain codepath can be computed for a sequence of the given length.
	 */
	void grd_stcheck(grd_st_t *grd_st, uint32_t len) {
		// Check if user ask for clearing the state tracker or if he requested a
		// bigger tracker. In this case we have to free the previous allocated
		// memory.
		if (len == 0 || (len > grd_st->len && grd_st->len != 0)) {
			xvm_free(grd_st->psi);   grd_st->psi   = NULL;
			xvm_free(grd_st->alpha); grd_st->alpha = NULL;
			xvm_free(grd_st->beta);  grd_st->beta  = NULL;
			xvm_free(grd_st->unorm); grd_st->unorm = NULL;
			xvm_free(grd_st->bnorm); grd_st->bnorm = NULL;
			xvm_free(grd_st->scale); grd_st->scale = NULL;
			grd_st->len = 0;
		}
		if (len == 0 || len <= grd_st->len)
			return;
		// If we are here, we have to allocate a new state. This is simple, we
		// just have to take care of the special case for sparse mode.
		const uint32_t Y = grd_st->mdl->nlbl;
		const uint32_t T = len;
		grd_st->psi   = xvm_new(T * Y * Y);
		grd_st->alpha = xvm_new(T * Y);
		grd_st->beta  = xvm_new(T * Y);
		grd_st->scale = xvm_new(T);
		grd_st->unorm = xvm_new(T);
		grd_st->bnorm = xvm_new(T);
		grd_st->len = len;
	}

	/* grd_stnew:
	 *   Allocation memory for gradient computation state. This allocate memory for
	 *   the longest sequence present in the data set.
	 */
	grd_st_t *grd_stnew(mdl_t *mdl, double *g) {
		grd_st_t *grd_st  = static_cast<grd_st_t*>(xmalloc(sizeof(grd_st_t)));
		grd_st->mdl    = mdl;
		grd_st->len    = 0;
		grd_st->g      = g;
		grd_st->psi    = NULL;
		grd_st->psiuni = NULL;
		grd_st->psiyp  = NULL;
		grd_st->psiidx = NULL;
		grd_st->psioff = NULL;
		grd_st->alpha  = NULL;
		grd_st->beta   = NULL;
		grd_st->unorm  = NULL;
		grd_st->bnorm  = NULL;
		grd_st->scale  = NULL;
		return grd_st;
	}

	/* grd_stfree:
	 *   Free all memory used by gradient computation.
	 */
	void grd_stfree(grd_st_t *grd_st) {
		grd_stcheck(grd_st, 0);
		free(grd_st);
	}

	/*
	 * DECODER.C
	 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

	/******************************************************************************
	 * Sequence tagging
	 *
	 *   This module implement sequence tagging using a trained model and model
	 *   evaluation on devlopment set.
	 *
	 *   The viterbi can be quite intensive on the stack if you push in it long
	 *   sequence and use large labels set. It's less a problem than in gradient
	 *   computations but it can show up in particular cases. The fix is to call it
	 *   through the mth_spawn function and request enough stack space, this will be
	 *   fixed in next version.
	 ******************************************************************************/

	/* tag_expsc:
	 *   Compute the score lattice for classical Viterbi decoding. This is the same
	 *   as for the first step of the gradient computation with the exception that
	 *   we don't need to take the exponential of the scores as the Viterbi decoding
	 *   works in log-space.
	 */

	static int tag_expsc(mdl_t *mdl, const seq_t *seq, double *vpsi) {
		const double  *x = mdl->theta;
		const uint32_t Y = mdl->nlbl;
		const uint32_t T = seq->len;

		// We first have to compute the Ψ_t(y',y,x_t) weights defined as
		//   Ψ_t(y',y,x_t) = \exp( ∑_k θ_k f_k(y',y,x_t) )
		// So at position 't' in the sequence, for each couple (y',y) we have
		// to sum weights of all features.
		// This is the same than what we do for computing the gradient but, as
		// the viterbi algorithm also work in the logarithmic space, we can
		// remove the exponential.
		//
		// Only the observations present at this position will have a non-nul
		// weight so we can sum only on thoses.
		//
		// As we use only two kind of features: unigram and bigram, we can
		// rewrite this as
		//   ∑_k μ_k(y, x_t) f_k(y, x_t) + ∑_k λ_k(y', y, x_t) f_k(y', y, x_t)
		// Where the first sum is over the unigrams features and the second is
		// over bigrams ones.
		//
		// This allow us to compute Ψ efficiently in two steps
		//   1/ we sum the unigrams features weights by looping over actives
		//        unigrams observations. (we compute this sum once and use it
		//        for each value of y')
		//   2/ we add the bigrams features weights by looping over actives
		//        bigrams observations (we don't have to do this for t=0 since
		//        there is no bigrams here)
		for (uint32_t t = 0; t < T; t++) {
			const pos_t *pos = &(seq->pos[t]);
			for (uint32_t y = 0; y < Y; y++) {
				double sum = 0.0;
				for (uint32_t n = 0; n < pos->ucnt; n++) {
					const uint64_t o = pos->uobs[n];
					sum += x[mdl->uoff[o] + y];
				}
				for (uint32_t yp = 0; yp < Y; yp++)
					map(vpsi, Y, Y, t, yp, y) = sum;
			}
		}
		for (uint32_t t = 1; t < T; t++) {
			const pos_t *pos = &(seq->pos[t]);
			for (uint32_t yp = 0, d = 0; yp < Y; yp++) {
				for (uint32_t y = 0; y < Y; y++, d++) {
					double sum = 0.0;
					for (uint32_t n = 0; n < pos->bcnt; n++) {
						const uint64_t o = pos->bobs[n];
						sum += x[mdl->boff[o] + d];
					}
					map(vpsi, Y, Y, t, yp, y) += sum;
				}
			}
		}
		return 0;
	}

	/* tag_memmsc:
	 *   Compute the score for viterbi decoding of MEMM models. This use the
	 *   previous function to compute the classical score and then normalize them
	 *   relative to the previous label. This normalization must be done in linear
	 *   space, not in logarithm one.
	 */
	static int tag_memmsc(mdl_t *mdl, const seq_t *seq, double *vpsi) {
		const uint32_t Y = mdl->nlbl;
		const uint32_t T = seq->len;
		tag_expsc(mdl, seq, vpsi);
		xvm_expma(vpsi, vpsi, 0.0, T * Y * Y);
		for (uint32_t t = 0; t < T; t++) {
			for (uint32_t yp = 0; yp < Y; yp++) {
				double sum = 0.0;
				for (uint32_t y = 0; y < Y; y++)
					sum += map(vpsi, Y, Y, t, yp, y);
				for (uint32_t y = 0; y < Y; y++)
					map(vpsi, Y, Y, t, yp, y) /= sum;
			}
		}
		return 1;
	}

	/* tag_postsc:
	 *   This function compute score lattice with posteriors. This generally result
	 *   in a slightly best labelling and allow to output normalized score for the
	 *   sequence and for each labels but this is more costly as we have to perform
	 *   a full forward backward instead of just the forward pass.
	 */
	static int tag_postsc(mdl_t *mdl, const seq_t *seq, double *vpsi) {
		const uint32_t Y = mdl->nlbl;
		const uint32_t T = seq->len;
		grd_st_t *grd_st = grd_stnew(mdl, NULL);
		grd_st->first = 0;
		grd_st->last  = T - 1;
		grd_stcheck(grd_st, seq->len);

		grd_fldopsi(grd_st, seq);
		grd_flfwdbwd(grd_st, seq);

		double  *unorm        =         grd_st->unorm;
		for (uint32_t t = 0; t < T; t++) {
			for (uint32_t y = 0; y < Y; y++) {
				double e = map(grd_st->alpha, Y, t, y) * map(grd_st->beta, Y, t, y) * unorm[t];
				for (uint32_t yp = 0; yp < Y; yp++)
					map(vpsi, Y, Y, t, yp, y) = e;
			}
		}
		grd_stfree(grd_st);
		return 1;
	}

	/* tag_forced:
	 *   This function apply correction to the psi table to take account of already
	 *   known labels. If a label is known, all arcs leading or comming from other
	 *   labels at this position are NULLified and will not be selected by the
	 *   decoder.
	 */
	static void tag_forced(mdl_t *mdl, const seq_t *seq, double *vpsi, int op) {
		const uint32_t Y = mdl->nlbl;
		const uint32_t T = seq->len;
		const double v = op ? 0.0 : -HUGE_VAL;
		for (uint32_t t = 0; t < T; t++) {
			const uint32_t cyr = seq->pos[t].lbl;
			if (cyr == std::numeric_limits<uint32_t>::max())
				continue;
			if (t != 0)
				for (uint32_t y = 0; y < Y; y++)
					if (y != cyr)
						for (uint32_t yp = 0; yp < Y; yp++)
							map(vpsi, Y, Y, t, yp, y) = v;
			if (t != T - 1)
				for (uint32_t y = 0; y < Y; y++)
					if (y != cyr)
						for (uint32_t yn = 0; yn < Y; yn++)
							map(vpsi, Y, Y, t + 1, y, yn) = v;
		}
		const uint32_t yr = seq->pos[0].lbl;
		if (yr != std::numeric_limits<uint32_t>::max()) {
			for (uint32_t y = 0; y < Y; y++) {
				if (yr == y)
					continue;
				for (uint32_t yp = 0; yp < Y; yp++)
					map(vpsi, Y, Y, 0, yp, y) = v;
			}
		}
	}

	/* tag_viterbi:
	 *   This function implement the Viterbi algorithm in order to decode the most
	 *   probable sequence of labels according to the model. Some part of this code
	 *   is very similar to the computation of the gradient as expected.
	 *
	 *   And like for the gradient, the caller is responsible to ensure there is
	 *   enough stack space.
	 */
	void tag_viterbi(
			mdl_t *mdl,
			const seq_t *seq,
			uint32_t out[],
			double *sc,
			double psc[]
	) {
		const uint32_t Y = mdl->nlbl;
		const uint32_t T = seq->len;
		double   *vpsi  = xvm_new(T * Y * Y);
		uint32_t *vback = static_cast<uint32_t*>(xmalloc(sizeof(uint32_t) * T * Y));
		double *cur = static_cast<double*>(xmalloc(sizeof(double) * Y));
		double *old = static_cast<double*>(xmalloc(sizeof(double) * Y));
		// We first compute the scores for each transitions in the lattice of
		// labels.
		int op;
		if (mdl->type == 1)
			op = tag_memmsc(mdl, seq, vpsi);
		else if (mdl->opt->lblpost)
			op = tag_postsc(mdl, seq, vpsi);
		else
			op = tag_expsc(mdl, seq, vpsi);
		if (mdl->opt->force)
			tag_forced(mdl, seq, vpsi, op);
		// Now we can do the Viterbi algorithm. This is very similar to the
		// forward pass
		//   | α_1(y) = Ψ_1(y,x_1)
		//   | α_t(y) = max_{y'} α_{t-1}(y') + Ψ_t(y',y,x_t)
		// We just replace the sum by a max and as we do the computation in the
		// logarithmic space the product become a sum. (this also mean that we
		// don't have to worry about numerical problems)
		//
		// Next we have to walk backward over the α in order to find the best
		// path. In order to do this efficiently, we keep in the 'back' array
		// the indice of the y value selected by the max. This also mean that
		// we only need the current and previous value of the α vectors, not
		// the full matrix.
		for (uint32_t y = 0; y < Y; y++)
			cur[y] = map(vpsi, Y, Y, 0, 0, y);
		for (uint32_t t = 1; t < T; t++) {
			for (uint32_t y = 0; y < Y; y++)
				old[y] = cur[y];
			for (uint32_t y = 0; y < Y; y++) {
				double   bst = -HUGE_VAL;
				uint32_t idx = 0;
				for (uint32_t yp = 0; yp < Y; yp++) {
					double val = old[yp];
					if (op)
						val *= map(vpsi, Y, Y, t, yp, y);
					else
						val += map(vpsi, Y, Y, t, yp, y);
					if (val > bst) {
						bst = val;
						idx = yp;
					}
				}
				map(vback, Y, t, y) = idx;
				cur[y]        = bst;
			}
		}
		// We can now build the sequence of labels predicted by the model. For
		// this we search in the last α vector the best value. Using this index
		// as a starting point in the back-pointer array we finally can decode
		// the best sequence.
		uint32_t bst = 0;
		for (uint32_t y = 1; y < Y; y++)
			if (cur[y] > cur[bst])
				bst = y;
		if (sc != NULL)
			*sc = cur[bst];
		for (uint32_t t = T; t > 0; t--) {
			const uint32_t yp = (t != 1) ? map(vback, Y, t - 1, bst) : 0;
			const uint32_t y  = bst;
			out[t - 1] = y;
			if (psc != NULL)
				psc[t - 1] = map(vpsi, Y, Y, t - 1, yp, y);
			bst = yp;
		}
		free(old);
		free(cur);
		free(vback);
		xvm_free(vpsi);
	}

#pragma GCC diagnostic pop

	/*
	 * VMATH.C
	 */

	/* xvm_new:
	 *   Allocate a new vector suitable to be used in the SSE code paths. This
	 *   ensure that the vector size contains the need padding. You must only use
	 *   vector allocated by this function if you use the optimized code paths.
	 */
	double *xvm_new(uint64_t N) {
	#if defined(__SSE2__) && !defined(XVM_ANSI)
		if (N % sse4 != 0)
			N += sse4 - N % sse4;
		void *ptr = _mm_malloc(sizeof(double) * N, sse16);
		if (ptr == nullptr)
			throw std::runtime_error(
					"wapiti::xvm_new():"
					" out of memory"
			);
		return static_cast<double*>(ptr);
	#else
		return xmalloc(sizeof(double) * N);
	#endif
	}

	/* xvm_free:
	 *   Free a vector allocated by xvm_new.
	 */
	void xvm_free(double x[]) {
	#if defined(__SSE2__) && !defined(XVM_ANSI)
		_mm_free(x);
	#else
		free(x);
	#endif
	}

	/* xvm_scale:
	 *   Return the given vector scaled by a constant:
	 *     r = a * x
	 */
	void xvm_scale(double r[], const double x[], double a, uint64_t N) {
		for (uint64_t n = 0; n < N; n++)
			r[n] = x[n] * a;
	}

	/* xvm_unit:
	 *   Store a normalized copy of the given vector in r and return the
	 *   normalization factor.
	 */
	double xvm_unit(double r[], const double x[], uint64_t N) {
		double sum = 0.0;
		for (uint64_t n = 0; n < N; n++)
			sum += x[n];
		const double scale = 1.0 / sum;
		xvm_scale(r, x, scale, N);
		return scale;
	}

	/* vms_expma:
	 *   Compute the component-wise exponential minus <a>:
	 *       r[i] <-- e^x[i] - a
	 *
	 *   The following comments apply to the SSE2 version of this code:
	 *
	 *   Computation is done four doubles as a time by doing computation in paralell
	 *   on two vectors of two doubles using SSE2 intrisics.  If size is not a
	 *   multiple of 4, the remaining elements are computed using the stdlib exp().
	 *
	 *   The computation is done by first doing a range reduction of the argument of
	 *   the type e^x = 2^k * e^f choosing k and f so that f is in [-0.5, 0.5].
	 *   Then 2^k can be computed exactly using bit operations to build the double
	 *   result and e^f can be efficiently computed with enough precision using a
	 *   polynomial approximation.
	 *
	 *   The polynomial approximation is done with 11th order polynomial computed by
	 *   Remez algorithm with the Solya suite, instead of the more classical Pade
	 *   polynomial form cause it is better suited to parallel execution. In order
	 *   to achieve the same precision, a Pade form seems to require three less
	 *   multiplications but need a very costly division, so it will be less
	 *   efficient.
	 *
	 *   The maximum error is less than 1lsb and special cases are correctly
	 *   handled:
	 *     +inf or +oor  -->   return +inf
	 *     -inf or -oor  -->   return  0.0
	 *     qNaN or sNaN  -->   return qNaN
	 *
	 *   This code is copyright 2004-2013 Thomas Lavergne and licenced under the
	 *   BSD licence like the remaining of Wapiti.
	 */
	void xvm_expma(double r[], const double x[], double a, uint64_t N) {
	#if defined(__SSE2__) && !defined(XVM_ANSI)
	  #define xvm_vconst(v) (_mm_castsi128_pd(_mm_set1_epi64x((v))))
		assert(r != NULL && (reinterpret_cast<uintptr_t>(r) % sse16) == 0);
		assert(x != NULL && (reinterpret_cast<uintptr_t>(x) % sse16) == 0);
		const __m128i vl  = _mm_set1_epi64x(0x3ff0000000000000ULL);
		const __m128d ehi = xvm_vconst(0x4086232bdd7abcd2ULL);
		const __m128d elo = xvm_vconst(0xc086232bdd7abcd2ULL);
		const __m128d l2e = xvm_vconst(0x3ff71547652b82feULL);
		const __m128d hal = xvm_vconst(0x3fe0000000000000ULL);
		const __m128d nan = xvm_vconst(0xfff8000000000000ULL);
		const __m128d inf = xvm_vconst(0x7ff0000000000000ULL);
		const __m128d c1  = xvm_vconst(0x3fe62e4000000000ULL);
		const __m128d c2  = xvm_vconst(0x3eb7f7d1cf79abcaULL);
		const __m128d p0  = xvm_vconst(0x3feffffffffffffeULL);
		const __m128d p1  = xvm_vconst(0x3ff000000000000bULL);
		const __m128d p2  = xvm_vconst(0x3fe0000000000256ULL);
		const __m128d p3  = xvm_vconst(0x3fc5555555553a2aULL);
		const __m128d p4  = xvm_vconst(0x3fa55555554e57d3ULL);
		const __m128d p5  = xvm_vconst(0x3f81111111362f4fULL);
		const __m128d p6  = xvm_vconst(0x3f56c16c25f3bae1ULL);
		const __m128d p7  = xvm_vconst(0x3f2a019fc9310c33ULL);
		const __m128d p8  = xvm_vconst(0x3efa01825f3cb28bULL);
		const __m128d p9  = xvm_vconst(0x3ec71e2bd880fdd8ULL);
		const __m128d p10 = xvm_vconst(0x3e9299068168ac8fULL);
		const __m128d p11 = xvm_vconst(0x3e5ac52350b60b19ULL);
		const __m128d va  = _mm_set1_pd(a);
		for (uint64_t n = 0; n < N; n += sse4) {
			__m128d mn1;
			__m128d mn2;
			__m128d mi1;
			__m128d mi2;
			__m128d t1;
			__m128d t2;
			__m128d d1;
			__m128d d2;
			__m128d v1;
			__m128d v2;
			__m128d w1;
			__m128d w2;
			__m128i k1;
			__m128i k2;
			__m128d f1;
			__m128d f2;
			// Load the next four values
			__m128d x1 = _mm_load_pd(x + n    	 );
			__m128d x2 = _mm_load_pd(x + n + sse2);
			// Check for out of ranges, infinites and NaN
			mn1 = _mm_cmpneq_pd(x1, x1);	mn2 = _mm_cmpneq_pd(x2, x2);
			mi1 = _mm_cmpgt_pd(x1, ehi);	mi2 = _mm_cmpgt_pd(x2, ehi);
			x1  = _mm_max_pd(x1, elo);	x2  = _mm_max_pd(x2, elo);
			// Range reduction: we search k and f such that e^x = 2^k * e^f
			// with f in [-0.5, 0.5]
			t1  = _mm_mul_pd(x1, l2e);	t2  = _mm_mul_pd(x2, l2e);
			t1  = _mm_add_pd(t1, hal);	t2  = _mm_add_pd(t2, hal);
			k1  = _mm_cvttpd_epi32(t1);	k2  = _mm_cvttpd_epi32(t2);
			d1  = _mm_cvtepi32_pd(k1);	d2  = _mm_cvtepi32_pd(k2);
			t1  = _mm_mul_pd(d1, c1);	t2  = _mm_mul_pd(d2, c1);
			f1  = _mm_sub_pd(x1, t1);	f2  = _mm_sub_pd(x2, t2);
			t1  = _mm_mul_pd(d1, c2);	t2  = _mm_mul_pd(d2, c2);
			f1  = _mm_sub_pd(f1, t1);	f2  = _mm_sub_pd(f2, t2);
			// Evaluation of e^f using a 11th order polynom in Horner form
			v1  = _mm_mul_pd(f1, p11);	v2  = _mm_mul_pd(f2, p11);
			v1  = _mm_add_pd(v1, p10);	v2  = _mm_add_pd(v2, p10);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p9);	v2  = _mm_add_pd(v2, p9);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p8);	v2  = _mm_add_pd(v2, p8);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p7);	v2  = _mm_add_pd(v2, p7);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p6);	v2  = _mm_add_pd(v2, p6);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p5);	v2  = _mm_add_pd(v2, p5);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p4);	v2  = _mm_add_pd(v2, p4);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p3);	v2  = _mm_add_pd(v2, p3);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p2);	v2  = _mm_add_pd(v2, p2);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p1);	v2  = _mm_add_pd(v2, p1);
			v1  = _mm_mul_pd(v1, f1);	v2  = _mm_mul_pd(v2, f2);
			v1  = _mm_add_pd(v1, p0);	v2  = _mm_add_pd(v2, p0);
			// Evaluation of 2^k using bitops to achieve exact computation
			k1  = _mm_slli_epi32(k1, sse20);	k2  = _mm_slli_epi32(k2, sse20);
			k1  = _mm_shuffle_epi32(k1, sseShuffle);
			k2  = _mm_shuffle_epi32(k2, sseShuffle);
			k1  = _mm_add_epi32(k1, vl);	k2  = _mm_add_epi32(k2, vl);
			w1  = _mm_castsi128_pd(k1);	w2  = _mm_castsi128_pd(k2);
			// Return to full range to substract <a>
				v1  = _mm_mul_pd(v1, w1);	v2  = _mm_mul_pd(v2, w2);
			v1  = _mm_sub_pd(v1, va);	v2  = _mm_sub_pd(v2, va);
			// Finally apply infinite and NaN where needed
			v1  = _mm_or_pd(_mm_and_pd(mi1, inf), _mm_andnot_pd(mi1, v1));
			v2  = _mm_or_pd(_mm_and_pd(mi2, inf), _mm_andnot_pd(mi2, v2));
			v1  = _mm_or_pd(_mm_and_pd(mn1, nan), _mm_andnot_pd(mn1, v1));
			v2  = _mm_or_pd(_mm_and_pd(mn2, nan), _mm_andnot_pd(mn2, v2));
			// Store the results
			_mm_store_pd(r + n,     v1);
			_mm_store_pd(r + n + 2, v2);
		}
	#else
		for (uint64_t n = 0; n < N; n++)
			r[n] = exp(x[n]) - a;
	#endif
	}

} /* namespace crawlservpp::Data::wapiti */
