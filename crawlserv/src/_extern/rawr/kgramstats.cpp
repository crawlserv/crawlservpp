/*
 * based on librawr by Kelly Rauchenberger at https://github.com/hatkirby/rawr-ebooks 👌
 *
 * - cleaned up includes
 * - changed datatypes from int to int to support larger corpora
 * - added support for enabling/disabling spell-checking in different languages
 * - removed std::cout logging and implemented algorithm-defined callback functions instead (incl. progress monitoring and verbose option)
 * - added number formatting using std::locale
 * - added the possibility to interrupt the compiling process by a callback function checking whether the algorithm should keep running
 * - added time measurement functionality using crawlserv++'s own Timer::Simple class
 *
 *  */

#include "kgramstats.h"

const rawr::query rawr::wildcardQuery = {querytype::sentence};
const rawr::word rawr::blank_word = {""};

void rawr::addCorpus(std::string corpus)
{
  _corpora.push_back(corpus);
}

// runs in O(t^2) time where t is the number of tokens in the input corpus
// We consider maxK to be fairly constant
bool rawr::compile(unsigned short maxK)
{
  _maxK = maxK;

  std::vector<std::vector<token_id>> tokens;
  std::set<std::string> thashtags;
  std::set<std::string> fv_emoticons;
  
  std::ifstream fvefile("emoticons.txt");
  if (fvefile)
  {
    std::string line;
    while (getline(fvefile, line))
    {
      fv_emoticons.insert(line);
      emoticons.forms.add(line);
    }
  }
  
  fvefile.close();
  
  std::map<std::string, std::string> canonical_form;
  
  AspellConfig* spell_config = NULL;
  if(_spellcheck) {
	  spell_config = new_aspell_config();
	  aspell_config_replace(spell_config, "lang", _language.c_str());
  }

  AspellCanHaveError* possible_err = NULL;
  if(_spellcheck) {
	  possible_err = new_aspell_speller(spell_config);
	  delete_aspell_config(spell_config);
	  if (aspell_error_number(possible_err) != 0)
	    {
	      if(_log) _log("Aspell ERROR: " + std::string(aspell_error_message(possible_err)));
	      return false;
	    }
  }
  
  AspellSpeller* spell_checker = NULL;
  if(_spellcheck) {
	  spell_checker = to_aspell_speller(possible_err);
  }
  
  prefix_search emojis;
  std::ifstream emoji_file("emojis.txt");
  if (emoji_file)
  {
    while (!emoji_file.eof())
    {
      std::string rawmojis;
      getline(emoji_file, rawmojis);
      if (rawmojis.back() == '\r')
      {
        rawmojis.pop_back();
      }
    
      emojis.add(rawmojis);
    }
    
    emoji_file.close();
  }

  crawlservpp::Timer::Simple * timer = NULL;
  if(_timing && _log) timer = new crawlservpp::Timer::Simple;
  if(_setstatus) _setstatus("Tokenizing corpus...");
  if(_setprogress) _setprogress(0.f);

  int len = 0;
  for (auto c : _corpora)
  {
    len += c.length();
  }
  
  long startper = 0;
  long per = 0;
  long perprime = 0;
  for (unsigned int i = 0; i < _corpora.size(); i++)
  {
    size_t start = 0;
    size_t end = 0;
    std::vector<token_id> tkcor;

    while (end != std::string::npos)
    {
      perprime = (startper + end) * 10000 / len;
      if (perprime != per)
      {
        per = perprime;
      
        if(_isrunning && !_isrunning()) {
        	if(_timing && _log) delete timer;
        	if(_spellcheck) delete_aspell_speller(spell_checker);
        	return false;
        }
        if(_setprogress) _setprogress((float) per / 10000);
        if(per % 1000 == 0 && _verbose && _log) {
        	// log every 10%
        	std::ostringstream logStrStr;
        	logStrStr.imbue(std::locale(""));
        	logStrStr << " " << per / 100 << "%";
        	_log(logStrStr.str());
        }
      }
    
      end = _corpora[i].find_first_of(" \n", start);

      bool emoji = false;
      std::string te = _corpora[i].substr(start, (end == std::string::npos) ? std::string::npos : end - start + 1);
      std::string t = "";
    
      if (te.compare("") && te.compare(".") && te.compare(" "))
      {
        if (te.back() == ' ')
        {
          te.pop_back();
        }
        
        // Extract strings of emojis into their own tokens even if they're not space delimited
        int m = emojis.match(te);
        emoji = m > 0;
        if (m == 0) m = 1;
        t = te.substr(0,m);
        te = te.substr(m);
      
        while (!te.empty())
        {
          m = emojis.match(te);
          if (emoji == (m > 0))
          {
            if (m == 0) m = 1;
            t += te.substr(0,m);
            te = te.substr(m);
          } else {
            end = start + t.length() - 1;
            break;
          }
        }
      
        std::string tc(t);
        std::transform(tc.begin(), tc.end(), tc.begin(), ::tolower);

        size_t pst = tc.find_first_not_of("\"([*");
        size_t dst = tc.find_last_not_of("\")]*.,?!\n;:");
        std::string canonical("");
        if ((pst != std::string::npos) && (dst != std::string::npos))
        {
          canonical = std::string(tc, pst, dst - pst + 1);
        }
      
        word& w = ([&] () -> word& {
          // Hashtag freevar
          if (canonical[0] == '#')
          {
            thashtags.insert(canonical);
          
            return hashtags;
          }
        
          // Emoticon freevar
          if (emoji)
          {
            emoticons.forms.add(canonical);
          
            return emoticons;
          }
        
          if ((pst != std::string::npos) && (dst != std::string::npos))
          {
            std::string emoticon_canon(t, pst, t.find_last_not_of("\"]*\n.,?!") - pst + 1);
            if (fv_emoticons.count(emoticon_canon) == 1)
            {
              emoticons.forms.add(emoticon_canon);
          
              return emoticons;
            }
          }
        
          // Basically any other word
          if (canonical_form.count(canonical) == 0)
          {
            if (
              // Legacy freevars should be distinct from tokens containing similar words
              (canonical.find("$name$") != std::string::npos)
              // Words with no letters will be mangled by the spell checker
              || (canonical.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") == std::string::npos)
              )
            {
              canonical_form[canonical] = canonical;
              words.emplace(canonical, canonical);
            } else {
              int correct = 1;
              if(_spellcheck) correct = aspell_speller_check(spell_checker, canonical.c_str(), canonical.size());
              if (correct)
              {
                words.emplace(canonical, canonical);
                canonical_form[canonical] = canonical;
              } else {
                const AspellWordList* suggestions = aspell_speller_suggest(spell_checker, canonical.c_str(), canonical.size());
                AspellStringEnumeration* elements = aspell_word_list_elements(suggestions);
                const char* replacement = aspell_string_enumeration_next(elements);
                if (replacement != NULL)
                {
                  std::string sugrep(replacement);
                  canonical_form[canonical] = sugrep;
          
                  if (words.count(sugrep) == 0)
                  {
                    words.emplace(sugrep, sugrep);
                  }
                } else {
                  words.emplace(canonical, canonical);
                  canonical_form[canonical] = canonical;
                }
          
                delete_aspell_string_enumeration(elements);
              }
            }
          }
        
          word& tw = words.at(canonical_form.at(canonical));
          tw.forms.add(canonical);
        
          return tw;
        })();
      
        token tk(w);
        tk.raw = t;
      
        for (char c : t)
        {
          if (c == '*')
          {
            tk.delimiters[{parentype::asterisk, doublestatus::opening}]++;
          } else if (c == '[')
          {
            tk.delimiters[{parentype::square_bracket, doublestatus::opening}]++;
          } else if (c == '(')
          {
            tk.delimiters[{parentype::paren, doublestatus::opening}]++;
          } else if (c == '"')
          {
            tk.delimiters[{parentype::quote, doublestatus::opening}]++;
          } else {
            break;
          }
        }
      
        size_t backtrack = t.find_last_not_of(".,?!])\"*\n;:") + 1;
        if (backtrack != t.length())
        {
          std::string ending = t.substr(backtrack);
          std::string suffix;
          bool newline = false;
          bool terminating = false;
        
          for (char c : ending)
          {
            if ((c == '.') || (c == ',') || (c == '?') || (c == '!') || (c == ';') || (c == ':'))
            {
              suffix += c;
              terminating = true;
            
              continue;
            } else if (c == '\n')
            {
              newline = true;
              terminating = true;
              
              continue;
            }
          
            parentype pt = ([&] {
              switch (c)
              {
                case ']': return parentype::square_bracket;
                case ')': return parentype::paren;
                case '*': return parentype::asterisk;
                case '"': return parentype::quote;
              }
            })();
          
            if (tk.delimiters[{pt, doublestatus::opening}] > 0)
            {
              tk.delimiters[{pt, doublestatus::opening}]--;
              tk.delimiters[{pt, doublestatus::both}]++;
            } else {
              tk.delimiters[{pt, doublestatus::closing}]++;
            }
          }
        
          if (terminating)
          {
            if ((suffix == ",") && (!newline))
            {
              tk.suffix = suffixtype::comma;
            } else {
              tk.suffix = suffixtype::terminating;
              
              if (!newline)
              {
                w.terms.add({suffix, false});
              } else {
                w.terms.add({".", false});
              }
            }
          }
        }

        tkcor.push_back(_tokenstore.add(tk));
      }

      start = ((end > (std::string::npos - 1) ) ? std::string::npos : end + 1);
    }
    
    tokens.push_back(tkcor);
    
    startper += _corpora[i].length();
  }
  
  if(_spellcheck) delete_aspell_speller(spell_checker);

  if(_log) {
	  if(_timing) {
		  _log("tokenized corpus in " + timer->tickStr() + ".");
		  delete timer;
	  }
	  std::ostringstream logStrStr;
	  logStrStr.imbue(std::locale(""));
	  logStrStr << canonical_form.size() << " distinct forms.";
	  _log(logStrStr.str());
	  logStrStr.str("");
	  logStrStr.clear();
	  logStrStr << words.size() << " distinct words.";
	  _log(logStrStr.str());
  }
  
  // Time to condense the distribution stuff for the words
  if(_setstatus) _setstatus("Compiling token histograms...");
  for (auto& it : words)
  {
    it.second.forms.compile();
    it.second.terms.compile();
  }
  
  // Hashtag freevar is not frequency distributed
  for (auto& it : thashtags)
  {
    hashtags.forms.add(it);
  }
  
  hashtags.forms.compile();
  hashtags.terms.compile();
  
  // Compile other freevars
  emoticons.forms.compile();
  emoticons.terms.compile();

  // Compile the interned tokens.
  _tokenstore.compile();

  // kgram distribution
  if(_timing && _log) timer = new crawlservpp::Timer::Simple();
  if(_setstatus) _setstatus("Creating markov chain...");
  if(_setprogress) _setprogress(0.f);
  std::map<kgram, std::map<token_id, token_data> > tstats;

  len = 0;
  for (auto c : tokens)
  {
    len += (maxK-1) * c.size();
  }
  
  startper = 0;
  per = 0;
  perprime = 0;
  int corpid = 0;
  for (auto corpus : tokens)
  {
    for (unsigned short k=1; k<maxK; k++)
    {
      for (unsigned long i=0; i<(corpus.size() - k); i++)
      {
        perprime = (startper+i) * 10000 / len;
        if (perprime != per)
        {
          per = perprime;
      
          if(_isrunning && !_isrunning()) {
        	  if(_timing && _log) delete timer;
        	  return false;
          }
          if(_setprogress) _setprogress((float) per / 10000);
          if(per % 1000 == 0 && _verbose && _log) {
			// log every 10%
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			logStrStr << " " << per / 100 << "%";
			_log(logStrStr.str());
		  }
        }
      
        kgram prefix(corpus.begin()+i, corpus.begin()+i+k);
        token_id fid = corpus[i+k];
        const token& f = _tokenstore.get(fid);

        if (tstats[prefix].count(fid) == 0)
        {
          tstats[prefix].emplace(fid, fid);
        }

        token_data& td = tstats[prefix].at(fid);
        td.all++;
        td.corpora.insert(corpid);

        if (std::find_if(f.raw.begin(), f.raw.end(), ::islower) == f.raw.end())
        {
          td.uppercase++;
        } else if (isupper(f.raw[0]))
        {
          td.titlecase++;
        }

        const token& startTok = _tokenstore.get(std::begin(prefix)->tok);
        if (startTok.suffix == suffixtype::terminating)
        {
          kgram term_prefix(prefix);
          term_prefix.pop_front();
          term_prefix.push_front(wildcardQuery);

          if (tstats[term_prefix].count(fid) == 0)
          {
            tstats[term_prefix].emplace(fid, fid);
          }

          token_data& td2 = tstats[term_prefix].at(fid);
          td2.all++;
          td2.corpora.insert(corpid);

          if (std::find_if(f.raw.begin(), f.raw.end(), ::islower) == f.raw.end())
          {
            td2.uppercase++;
          } else if (isupper(f.raw[0]))
          {
            td2.titlecase++;
          }
        }
      }
      
      startper += corpus.size();
    }
    
    corpid++;
  }

  if(_timing && _log) _log("created Markov chain in " + timer->tickStr() + ".");

  // Condense the kgram distribution
  if(_setstatus) _setstatus("Compiling kgram distributions...");
  if(_setprogress) _setprogress(0.f);
  len = tstats.size();
  per = 0;
  perprime = 0;
  long indicator = 0;
  for (auto& it : tstats)
  {
    indicator++;
    perprime = indicator * 10000 / len;
    if (per != perprime)
    {
      per = perprime;
    
      if(_isrunning && !_isrunning()) {
    	  if(_timing && _log) delete timer;
    	  return false;
      }
      if(_setprogress) _setprogress((float) per / 10000);
      if(per % 1000 == 0 && _verbose && _log) {
		// log every 10%
		std::ostringstream logStrStr;
		logStrStr.imbue(std::locale(""));
		logStrStr << " " << per / 100 << "%";
		_log(logStrStr.str());
	  }
    }
    
    kgram klist = it.first;
    auto& probtable = it.second;
    auto& distribution = _stats[klist];
    long max = 0;
		
    for (auto& kt : probtable)
    {
      max += kt.second.all;
			
      distribution.emplace(max, kt.second);
    }
  }
  
  if(_timing && _log) {
	  _log("compiled kgram distributions in " + timer->tickStr() + ".");
	  delete timer;
  }
  if(_setprogress) _setprogress(1.f);
  _compiled = true;
  return true;
}

std::ostream& operator<<(std::ostream& os, rawr::kgram k)
{
  for (auto& q : k)
  {
    os << q << " ";
  }
  
  return os;
}

std::ostream& operator<<(std::ostream& os, rawr::query q)
{
  if (q.type == rawr::querytype::sentence)
  {
    return os << "#.#";
  } else if (q.type == rawr::querytype::literal)
  {
    return os << q.tok;
  }
  
  return os;
}

std::ostream& operator<<(std::ostream& os, rawr::token t)
{
  os << t.w.canon;
  
  if (t.suffix == rawr::suffixtype::terminating)
  {
    return os << ".";
  } else if (t.suffix == rawr::suffixtype::comma)
  {
    return os << ",";
  } else {
    return os;
  }
}

std::ostream& operator<<(std::ostream& os, rawr::terminator t)
{
  os << t.form;
  
  if (t.newline)
  {
    os << "↵";
  }
  
  return os;
}

void rawr::setSpellChecking(bool enable, std::string language) {
	_spellcheck = enable;
	_language = language;
}

void rawr::setVerbose(bool verbose) {
	_verbose = verbose;
}

void rawr::setTiming(bool timing) {
	_timing = timing;
}

void rawr::setTransformCallback(transform_callback _arg)
{
  _transform = _arg;
}

void rawr::setLogCallback(log_callback _arg) {
	_log = _arg;
}

void rawr::setIsRunningCallback(isrunning_callback arg) {
	_isrunning = arg;
}

void rawr::setSetStatusCallback(setstatus_callback arg) {
	_setstatus = arg;
}

void rawr::setSetProgressCallback(setprogress_callback arg) {
	_setprogress = arg;
}

void rawr::setMinCorpora(unsigned int _arg)
{
  _min_corpora = _arg;
}

// runs in O(n log t) time where n is the input number of sentences and t is the number of tokens in the input corpus
std::string rawr::randomSentence(unsigned int maxL) const
{
  if (!_compiled)
  {
    return "";
  }
  
  std::string result;
  kgram cur(1, wildcardQuery);
  int cuts = 0;
  std::stack<parentype> open_delimiters;
  std::set<int> used_corpora;
	
  for (;;)
  {
    if (cur.size() == _maxK)
    {
      cur.pop_front();
    }
    
    do
    {
      if ((cur.size() > 2) && (cuts > 0) && ((rand() % cuts) > 0))
      {
        cur.pop_front();
        cuts--;
      } else {
        break;
      }
    } while ((cur.size() > 2) && (cuts > 0) && ((rand() % cuts) > 0));
    
    // Gotta circumvent the last line of the input corpus
    // https://twitter.com/starla4444/status/684222271339237376
    if (_stats.count(cur) == 0)
    {
      cur = kgram(1, wildcardQuery);
    }

    auto& distribution = _stats.at(cur);
    int max = distribution.rbegin()->first;
    int r = rand() % max;
    const token_data& next = distribution.upper_bound(r)->second;
    const token& interned = _tokenstore.get(next.tok);
    std::string nextToken = interned.w.forms.next();

    // Apply user-specified transforms
    if (_transform)
    {
      nextToken = _transform(interned.w.canon, nextToken);
    }
  
    // Determine the casing of the next token. We randomly make the token all
    // caps based on the markov chain. Otherwise, we check if the previous
    // token is the end of a sentence (terminating token or a wildcard query).
    int casing = rand() % next.all;
    if (casing < next.uppercase)
    {
      std::transform(nextToken.begin(), nextToken.end(), nextToken.begin(), ::toupper);
    } else {
      bool capitalize = false;

      if (casing - next.uppercase < next.titlecase)
      {
        capitalize = true;
      } else if (cur.rbegin()->type == querytype::sentence)
      {
        if (rand() % 2 > 0)
        {
          capitalize = true;
        }
      } else {
        const token& lastTok = _tokenstore.get(cur.rbegin()->tok);

        if (lastTok.suffix == suffixtype::terminating &&
            rand() % 2 > 0)
        {
          capitalize = true;
        }
      }

      if (capitalize)
      {
        nextToken[0] = toupper(nextToken[0]);
      }
    }
    
    // Delimiters
    for (auto& dt : interned.delimiters)
    {
      if (dt.first.status == doublestatus::both)
      {
        switch (dt.first.type)
        {
          case parentype::paren: nextToken = std::string("(", dt.second) + nextToken + std::string(")", dt.second); break;
          case parentype::square_bracket: nextToken = std::string("[", dt.second) + nextToken + std::string("]", dt.second); break;
          case parentype::asterisk: nextToken = std::string("*", dt.second) + nextToken + std::string("*", dt.second); break;
          case parentype::quote: nextToken = std::string("\"", dt.second) + nextToken + std::string("\"", dt.second); break;
        }
      } else if (dt.first.status == doublestatus::opening)
      {
        for (int i=0; i<dt.second; i++)
        {
          open_delimiters.push(dt.first.type);
        }
        
        switch (dt.first.type)
        {
          case parentype::paren: nextToken = std::string("(", dt.second) + nextToken; break;
          case parentype::square_bracket: nextToken = std::string("[", dt.second) + nextToken; break;
          case parentype::asterisk: nextToken = std::string("*", dt.second) + nextToken; break;
          case parentype::quote: nextToken = std::string("\"", dt.second) + nextToken; break;
        }
      } else if (dt.first.status == doublestatus::closing)
      {
        for (int i=0; i<dt.second; i++)
        {
          while (!open_delimiters.empty() && (open_delimiters.top() != dt.first.type))
          {
            switch (open_delimiters.top())
            {
              case parentype::paren: nextToken.append(")"); break;
              case parentype::square_bracket: nextToken.append("]"); break;
              case parentype::asterisk: nextToken.append("*"); break;
              case parentype::quote: nextToken.append("\""); break;
            }
            
            open_delimiters.pop();
          }
          
          if (open_delimiters.empty())
          {
            switch (dt.first.type)
            {
              case parentype::paren: result = "(" + result; break;
              case parentype::square_bracket: result = "[" + result; break;
              case parentype::asterisk: result = "*" + result; break;
              case parentype::quote: result = "\"" + result; break;
            }
          } else {
            open_delimiters.pop();
          }
          
          switch (dt.first.type)
          {
            case parentype::paren: nextToken.append(")"); break;
            case parentype::square_bracket: nextToken.append("]"); break;
            case parentype::asterisk: nextToken.append("*"); break;
            case parentype::quote: nextToken.append("\""); break;
          }
        }
      }
    }
    
    // Terminators
    if (interned.suffix == suffixtype::terminating)
    {
      auto term = interned.w.terms.next();
      nextToken.append(term.form);
      
      if (term.newline)
      {
        nextToken.append("\n");
      } else {
        nextToken.append(" ");
      }
    } else if (interned.suffix == suffixtype::comma)
    {
      nextToken.append(", ");
    } else {
      nextToken.append(" ");
    }
    
    if (next.all == max)
    {
      // If this pick was guaranteed, increase cut chance
      cuts++;
    } else if (cuts > 0) {
      // Otherwise, decrease cut chance
      cuts /= 2;
    }
    
    if (next.corpora.size() == 1)
    {
      used_corpora.insert(*next.corpora.begin());
    }
		
    /* DEBUG */
    if(_verbose && _log) {
    	std::ostringstream logStrStr;

    	logStrStr << cur << "-> \"" << nextToken << "\" (" << next.all << "/" << max << ")" << " in corp";
		for (auto cor : next.corpora)
		{
		  logStrStr << " " << cor;
		}
		logStrStr << "; l=" << cur.size() << ",cuts=" << cuts << std::endl;
		_log(logStrStr.str());
    }

    cur.push_back(next.tok);
    result.append(nextToken);

    if ((interned.suffix == suffixtype::terminating) && ((result.length() > maxL) || (rand() % 4 == 0)))
    {
      break;
    }
  }
  
  // Ensure that enough corpora are used
  if (used_corpora.size() < _min_corpora)
  {
    return randomSentence(maxL);
  }
  
  // Remove the trailing space
  if (result.back() == ' ' || result.back() == '\n')
  {
    result.pop_back();
  }
  
  // Close any open delimiters
  while (!open_delimiters.empty())
  {
    switch (open_delimiters.top())
    {
      case parentype::paren: result.append(")"); break;
      case parentype::square_bracket: result.append("]"); break;
      case parentype::asterisk: result.append("*"); break;
      case parentype::quote: result.append("\""); break;
    }
    
    open_delimiters.pop();
  }
	
  return result;
}
