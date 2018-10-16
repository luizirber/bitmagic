/*
Copyright(c) 2018 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

For more information please visit:  http://bitmagic.io
*/

/** \example xsample04.cpp
 
*/

/*! \file xsample04.cpp
    \brief Example: DNA substring search

*/

#include <iostream>
#include <sstream>
#include <chrono>
#include <regex>
#include <time.h>
#include <stdio.h>

#include <stdexcept>
#include <vector>
#include <chrono>
#include <map>
#include <utility>
#include <algorithm>
#include <unordered_map>

#include "bm.h"
#include "bmalgo.h"
#include "bmserial.h"
#include "bmaggregator.h"

#include "bmdbg.h"
#include "bmtimer.h"


using namespace std;

static
void show_help()
{
    std::cerr
        << "BitMagic DNA Search Sample (c) 2018" << std::endl
        << "-fa   file-name            -- input FASTA file" << std::endl
        << "-s hi|lo                   -- run substring search benchmark" << std::endl
        << "-diag                      -- run diagnostics"  << std::endl
        << "-timing                    -- collect timings"  << std::endl
      ;
}




// Arguments
//
std::string  ifa_name;
bool         is_diag = false;
bool         is_timing = false;
bool         is_bench = false;
bool         is_search = false;
bool         h_word_set = true;

static
int parse_args(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help"))
        {
            show_help();
            return 0;
        }
        
        if (arg == "-fa" || arg == "--fa")
        {
            if (i + 1 < argc)
            {
                ifa_name = argv[++i];
            }
            else
            {
                std::cerr << "Error: -fa requires file name" << std::endl;
                return 1;
            }
            continue;
        }

        if (arg == "-diag" || arg == "--diag" || arg == "-d" || arg == "--d")
            is_diag = true;
        if (arg == "-timing" || arg == "--timing" || arg == "-t" || arg == "--t")
            is_timing = true;
        if (arg == "-bench" || arg == "--bench" || arg == "-b" || arg == "--b")
            is_bench = true;
        if (arg == "-search" || arg == "--search" || arg == "-s" || arg == "--s")
        {
            is_search = true;
            if (i + 1 < argc)
            {
                std::string a = argv[i+1];
                if (a != "-")
                {
                    if (a == "l" || a == "lo")
                    {
                        h_word_set = false;
                        ++i;
                    }
                    else
                    if (a == "h" || a == "hi")
                    {
                        h_word_set = true;
                        ++i;
                    }
                }
            }
        }

    } // for i
    return 0;
}


// Global types
//
typedef std::map<std::string, unsigned>                     freq_map;
typedef std::vector<std::pair<unsigned, std::string> >      dict_vect;

typedef bm::aggregator<bm::bvector<> >  aggregator_type;

// ----------------------------------------------------------------------------

bm::chrono_taker::duration_map_type  timing_map;

// FASTA format parser
static
int load_FASTA(const std::string& fname, std::vector<char>& seq_vect)
{
    bm::chrono_taker tt1("1. Parse FASTA", 1, &timing_map);
    
    seq_vect.resize(0);
    std::ifstream fin(fname.c_str(), std::ios::in);
    if (!fin.good())
        return -1;
    
    std::string line;
    for (unsigned i = 0; std::getline(fin, line); ++i)
    {
        if (line.empty() ||
            line.front() == '>')
            continue;
        
        for (std::string::iterator it = line.begin(); it != line.end(); ++it)
            seq_vect.push_back(*it);
    } // for
    return 0;
}




class CBitSeq
{
public:
    enum { eA = 0, eC, eG, eT, eN, eEnd };
    
    CBitSeq() {}
    
    void Build(const vector<char>& sequence)
    {
        {
        bm::bvector<>::insert_iterator iA = m_Data[eA].inserter();
        bm::bvector<>::insert_iterator iC = m_Data[eC].inserter();
        bm::bvector<>::insert_iterator iG = m_Data[eG].inserter();
        bm::bvector<>::insert_iterator iT = m_Data[eT].inserter();
        bm::bvector<>::insert_iterator iN = m_Data[eN].inserter();

        for (size_t i = 0; i < sequence.size(); ++i)
        {
            unsigned pos = unsigned(i);
            switch (sequence[i])
            {
            case 'A':
                iA = pos;
                break;
            case 'C':
                iC = pos;
                break;
            case 'G':
                iG = pos;
                break;
            case 'T':
                iT = pos;
                break;
            case 'N':
                iN = pos;
                break;
            default:
                break;
            }
        }
        }
    }
    
    const bm::bvector<>& GetVector(char letter) const
    {
        switch (letter)
        {
        case 'A':
            return m_Data[eA];
        case 'C':
            return m_Data[eC];
        case 'G':
            return m_Data[eG];
        case 'T':
            return m_Data[eT];
        case 'N':
            return m_Data[eN];
        default:
            break;
        }
        throw runtime_error("Error. Invalid letter!");
    }
    
    void Find(const string& word, vector<pair<int, int>>& res)
    {
        if (word.empty())
            return;
        bm::bvector<> bv(GetVector(word[0])); // step 1: copy first vector
        
        // run series of shifts + logical ANDs
        for (size_t i = 1; i < word.size(); ++i)
        {
            bv.shift_right();
            bv &= GetVector(word[i]);
            auto any = bv.any();
            if (!any)
                break;
        }
        
        // translate results from bvector of word ends to result
        unsigned ws = unsigned(word.size()) - 1;
        TranslateResults(bv, ws, res);
    };
    
    /// This method uses horizontal aggregator with combined SHIFT+AND
    void FindAgg(const string& word, vector<pair<int, int>>& res)
    {
        if (word.empty())
            return;
        bm::bvector<> bv(GetVector(word[0])); // step 1: copy first vector
        
        // run series of shifts fused with logical ANDs using
        // re-used bm::aggregator<>
        //
        for (size_t i = 1; i < word.size(); ++i)
        {
            const bm::bvector<>& bv_mask = GetVector(word[i]);
            bool any = m_Agg.shift_right_and(bv, bv_mask);
            if (!any)
                break;
        }
        
        // translate results from bvector of word ends to result
        unsigned ws = unsigned(word.size()) - 1;
        TranslateResults(bv, ws, res);
    };
    
    /// This method uses FUSED cache bloced aggregator with combined SHIFT+AND
    void FindAggFused(const string& word, vector<pair<int, int>>& res)
    {
        if (word.empty())
            return;
        // first we setup aggregator, add a group of vectors to be processed
        m_Agg.reset();
        for (size_t i = 0; i < word.size(); ++i)
        {
            const bm::bvector<>& bv_mask = GetVector(word[i]);
            m_Agg.add(&bv_mask);
        }
        
        // now run the whole algorithm to get benefits of cache blocking
        //
        bm::bvector<> bv;
        m_Agg.combine_shift_right_and(bv);
        
        // translate results from bvector of word ends to result
        unsigned ws = unsigned(word.size()) - 1;
        TranslateResults(bv, ws, res);
    };



    void Serialize(const string& file_name)
    {
        ofstream os(file_name.c_str());
        bm::serializer<bm::bvector<> > bvs;
        BM_DECLARE_TEMP_BLOCK(tb)
        bm::bvector<>::statistics st;
        
        for (size_t i = 0; i < eEnd; ++i)
        {
            m_Data[i].optimize(tb, bm::bvector<>::opt_compress, &st);
            // allocate serialization buffer
            vector<unsigned char> v_buf(st.max_serialize_mem);
            unsigned char*  buf = v_buf.data();//new unsigned char[st.max_serialize_mem];
            // serialize to memory
            unsigned len = bvs.serialize(m_Data[i], buf, st.max_serialize_mem);
            os.write((const char*)buf, len);
        }
    }
    
protected:

    /// Translate search results vector using (word size) left shift
    ///
    void TranslateResults(const bm::bvector<>& bv,
                          unsigned left_shift,
                          vector<pair<int, int>>& res)
    {
        bm::bvector<>::enumerator en = bv.first();
        for (; en.valid(); ++en)
        {
            auto pos = *en;
            res.emplace_back(pos - left_shift, pos);
        }
    }
    
private:
    bm::bvector<>   m_Data[eEnd];
    aggregator_type m_Agg;
};

static const size_t WORD_SIZE = 28;
using THitList = vector<pair<int, int>>;

/// generate the most frequent words of specified length from the input sequence
///
static
void generate_kmers(vector<tuple<string,int>>& top_words,
                    vector<tuple<string,int>>& lo_words,
                    const vector<char>& data,
                    size_t N,
                    unsigned word_size)
{
    cout << "k-mer generation... " << endl;
    typedef multimap<int,string, greater<int> > dest_map_type;
    
    top_words.clear();
    lo_words.clear();
    
    if (data.size() < word_size)
        return;
    
    size_t end_pos = data.size() - word_size;
    size_t i = 0;
    unordered_map<string, int> words;
    while (i < end_pos)
    {
        string s(&data[i], word_size);
        if (s.find("N") == string::npos)
            words[s] += 1;
        i += word_size;
        
        if (i % 10000 == 0)
        {
            cout << "\r" << i << "/" << end_pos << flush;
        }
    }
    cout << endl << "Picking k-mer samples..." << flush;
    dest_map_type dst;
    for_each(words.begin(), words.end(), [&](const std::pair<string,int>& p) {
                 dst.emplace(p.second, p.first);
             });
 
    {
    dest_map_type::iterator it = dst.begin();
    for(size_t count = 0; count < N && it !=dst.end(); ++it,++count)
        top_words.emplace_back(it->second, it->first);
    }

    {
    dest_map_type::reverse_iterator it = dst.rbegin();
    for(size_t count = 0; count < N && it !=dst.rend(); ++it,++count)
        lo_words.emplace_back(it->second, it->first);
    }
    cout << "OK" << endl;
}

void find_word_strncmp(vector<char>& data,
                       const char* word, unsigned word_size,
                       THitList& r)
{
    r.clear();
    if (data.size() < word_size)
        return;
    
    size_t i = 0;
    size_t end_pos = data.size() - word_size;
    while (i < end_pos)
    {
        if (strncmp(&data[i], word, word_size) == 0)
        {
            r.emplace_back(i, i + word_size - 1);
        }
        ++i;
    }
}

bool hitlist_compare(const THitList& h1, const THitList& h2)
{
    if (h1.size() != h2.size())
    {
        cerr << "HitList size error! " << h1.size() << " " << h2.size() << endl;
        return false;
    }
    for (size_t i = 0; i < h1.size(); ++i)
    {
        if (h1[i].first != h2[i].first)
            return false;
        if (h1[i].second != h2[i].second)
            return false;
    }
    return true;
}




int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        show_help();
        return 1;
    }
    
    std::vector<char> seq_vect;

    try
    {
        auto ret = parse_args(argc, argv);
        if (ret != 0)
            return ret;

        if (!ifa_name.empty())
        {
            auto res = load_FASTA(ifa_name, seq_vect);
            if (res != 0)
                return res;
            std::cout << "FASTA sequence size=" << seq_vect.size() << std::endl;
        }
        
        if (is_search)
        {
            vector<tuple<string,int>> h_words;
            vector<tuple<string,int>> l_words;
            
            vector<tuple<string,int>>& words = h_word_set ? h_words : l_words;

            // generate search sets for benchmarking
            //
            generate_kmers(h_words, l_words, seq_vect, 20, WORD_SIZE);
            
            CBitSeq idx;
            {
                bm::chrono_taker tt1("2. Build DNA index", 1, &timing_map);

                idx.Build(seq_vect);
            }
            
            for (const auto& w : words)
            {
                const string& word = get<0>(w);

                THitList hits1;
                {
                    bm::chrono_taker tt1("3. Search with strncmp", 1, &timing_map);
                    find_word_strncmp(seq_vect,
                                      word.c_str(), unsigned(word.size()),
                                      hits1);
                }

                THitList hits2;
                {
                    bm::chrono_taker tt1("4. Search with bvector SHIFT+AND", 1, &timing_map);
                    idx.Find(word, hits2);
                }

                THitList hits3;
                {
                    bm::chrono_taker tt1("5. Search with aggregator SHIFT+AND", 1, &timing_map);
                    idx.FindAgg(word, hits3);
                }

                THitList hits4;
                {
                    bm::chrono_taker tt1("6. Search with aggregator fused SHIFT+AND", 1, &timing_map);
                    idx.FindAggFused(word, hits4);
                }

                // check correctness
                if (!hitlist_compare(hits1, hits2) ||
                    !hitlist_compare(hits2, hits3) ||
                    !hitlist_compare(hits2, hits4)
                   )
                {
                    cout << "Mismatch ERROR for: " <<  word << endl;
                }
                else
                {
                    cout << word << ":" << hits4.size() << " hits " << endl;
                }
            }
        }

        if (is_timing)  // print all collected timings
        {
            std::cout << std::endl << "Performance:" << std::endl;
            bm::chrono_taker::print_duration_map(timing_map, bm::chrono_taker::ct_all);
        }
    }
    catch (std::exception& ex)
    {
        std::cerr << "Error:" << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
