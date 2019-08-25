#pragma once

#include <algorithm>
#include <bitset>
#include <cmath>
#include <fstream>
#include <functional>
#include <string>
#include <unordered_set>

#include "../submodules/genmap/src/common.hpp"
#include "../submodules/genmap/src/genmap_helper.hpp"

//#include "../submodules/sdsl/include/sdsl/bit_vectors.hpp"

#include "primer_cfg_type.hpp"
#include "types.hpp"
#include "utilities.hpp"

namespace priset
{
// TODO: kmer_location is the transformed to reference bit vector
// pre-filter and sequence fetch
// 1. filter candidates by number of occurences only independent of their chemical suitability
// 2. fetch sequence and check chemical constraints that need to hold for a single primer
void frequency_filter(priset::io_cfg_type const & io_cfg, TKLocations & locations, TKmerLocations & kmer_locations, TSeqNo const cutoff)
{
    // uniqueness indirectly preserved by (SeqNo, SeqPos) if list sorted lexicographically
    assert(length(locations));

    // load corpus
    seqan::StringSet<seqan::DnaString, seqan::Owner<seqan::ConcatDirect<>>> text;
    fs::path text_path = io_cfg.get_index_txt_path();
    std::cout << "text_path: " << text_path << std::endl;
    seqan::open(text, text_path.string().c_str(), seqan::OPEN_RDONLY);

    TKmerID kmer_ID;
    std::vector<TLocation> fwd;
    for (typename TKLocations::const_iterator it = locations.begin(); it != locations.end(); ++it)
    {
        // not enough k-mer occurences => continue
        // Note: getOccurrences and resetLimits in genmap lead to less kmers occurences than countOccurrences
        if ((it->second).first.size() < cutoff)
            continue;

        const auto & [seqNo, seqPos, K] = (it->first);

        // use symmetry and lexicographical ordering of locations to skip already seen ones
        if (it->second.first.size() && seqan::getValueI1<TSeqNo, TSeqPos>(it->second.first[0]) < seqPos)
            continue;
        // invariant: cutoff is always ≥ 2
        for (TLocation pair : it->second.first)
        {
            fwd.push_back(pair);
        }
        // encode dna sequence
        seqan::DnaString seq = seqan::valueById(text, seqNo);
        auto const & kmer_str = seqan::infixWithLength(seq, seqPos, K);

        kmer_ID = dna_encoder(kmer_str);
        // replace kmer_ID
        kmer_locations.push_back(TKmerLocation{kmer_ID, K, fwd});
        // TODO: same for reverse?
        fwd.clear();
    }
    locations.clear();
}

// helper for dna sequence extraction
template<typename TText>
TKmerID encode_wrapper(TText const & text, TSeqNo const seqNo, TSeqPos const seqPos, TKmerLength const K)
{
    seqan::DnaString seq = seqan::valueById(text, seqNo);
    auto const & kmer_str = seqan::infixWithLength(seq, seqPos, K);
    return dna_encoder(kmer_str);
};

// kmer locations by reference
void frequency_filter2(priset::io_cfg_type const & io_cfg, TKLocations & locations, TReferences & references, TKmerIDs & kmerIDs, TSeqNoMap & seqNoMap, TSeqNo const cutoff)
{
    std::cout << "Enter frequency_filter2 ...\n";
    // uniqueness indirectly preserved by (SeqNo, SeqPos) if list sorted lexicographically
    assert(length(locations));

    references.clear();
    kmerIDs.clear();

    // collect distinct sequence identifiers and maximal position of kmer occurences
    std::unordered_map<TSeqNo, TSeqPos> seqNo2maxPos;
    for (typename TKLocations::const_iterator it = locations.begin(); it != locations.end(); ++it)
    {
        std::cout << "iterate through locations\n";
        // cleanup in mapper may lead to undercounting kmer occurrences
        if ((it->second).first.size() < cutoff)
        {
            std::cout << "(it->second).first.size() = " << (it->second).first.size() << ", cutoff = " << cutoff << std::endl;
            std::cout << "continue due to first location vector size < cutoff\n";
            continue;
        }
        const auto & [seqNo, seqPos, K] = (it->first);
        // use symmetry and lexicographical ordering of locations to skip already seen ones
        // TODO: is this already shortcut fm mapper?
        if (it->second.first.size() && (seqan::getValueI1<TSeqNo, TSeqPos>(it->second.first[0]) < seqNo ||
            (seqan::getValueI1<TSeqNo, TSeqPos>(it->second.first[0]) == seqNo && seqan::getValueI2<TSeqNo, TSeqPos>(it->second.first[0]) < seqPos)))
        {
            std::cout << "continue due to first location vector size < cutoff\n";
            continue;
        }

        // update largest kmer position
        seqNo2maxPos.insert_or_assign(seqNo, std::max(seqNo2maxPos[seqNo], seqPos));
        // and also for other occurences
        for (std::vector<TLocation>::const_iterator it_loc_fwd = it->second.first.begin(); it_loc_fwd != it->second.first.end(); ++it_loc_fwd)
        {
            TSeqNo seqNo_next = seqan::getValueI1<TSeqNo, TSeqPos>(*it_loc_fwd);
            TSeqPos seqPos_next = seqan::getValueI2<TSeqNo, TSeqPos>(*it_loc_fwd);
            seqNo2maxPos.insert_or_assign(seqNo_next, std::max(seqNo2maxPos[seqNo_next], seqPos_next));
        }
    }
    std::cout << "max positions for sequences: \nseqNo\tmax(seqPos)\n";
    for (auto it = seqNo2maxPos.begin(); it != seqNo2maxPos.end(); ++it)
        std::cout << it->first << "\t" << it->second << std::endl;
    std::vector<TSeqNo> seqNos_sort;
    seqNos_sort.reserve(seqNo2maxPos.size());
    for (auto it_map = seqNo2maxPos.begin(); it_map != seqNo2maxPos.end(); ++it_map)
        seqNos_sort.push_back(it_map->first);
    std::sort(seqNos_sort.begin(), seqNos_sort.end());
    seqNoMap.clear();
    for (long unsigned i = 0; i < seqNos_sort.size(); ++i)
    {
        std::cout << "seqNoMap, insert: " << seqNos_sort[i] << " and " << i << std::endl;
        seqNoMap.insert({seqNos_sort[i], i});
        sdsl::bit_vector bv(seqNo2maxPos[seqNos_sort[i]] + 1, 0);
        references.push_back(bv);
    }
    kmerIDs.resize(references.size());

    std::cout << "h1\n";
    // load corpus for dna to 64 bit conversion
    seqan::StringSet<seqan::DnaString, seqan::Owner<seqan::ConcatDirect<>>> text;
    fs::path text_path = io_cfg.get_index_txt_path();
    seqan::open(text, text_path.string().c_str(), seqan::OPEN_RDONLY);
    std::cout << "h2\n";
    TKmerID kmerID;
    // set bits for kmers in references
    for (typename TKLocations::const_iterator it = locations.begin(); it != locations.end(); ++it)
    {
        std::cout << "h3\n";
        // cleanup in mapper may lead to undercounting kmer occurrences
        if ((it->second).first.size() < cutoff)
            continue;
        const auto & [seqNo, seqPos, K] = (it->first);
        // use symmetry and lexicographical ordering of locations to skip already seen ones
        // TODO: is this already shortcut fm mapper?
        if (it->second.first.size() && (seqan::getValueI1<TSeqNo, TSeqPos>(it->second.first[0]) < seqNo ||
            (seqan::getValueI1<TSeqNo, TSeqPos>(it->second.first[0]) == seqNo && seqan::getValueI2<TSeqNo, TSeqPos>(it->second.first[0]) < seqPos)))
        {
            std::cout << "continue due to first location vector size < cutoff\n";
            continue;
        }

        references[seqNoMap.at(seqNo)][seqPos] = 1;
        // delete this, debug only
        //std::string key = std::to_string(seqNo) + "_" + std::to_string(seqPos);
        //priset::TSeq const & seq = seq_map[key];
        //kmerID = dna_encoder(seq);
        // encode dna sequence and store
        kmerID = encode_wrapper<seqan::StringSet<seqan::DnaString, seqan::Owner<seqan::ConcatDirect<>>> >(text, seqNo, seqPos, K);
        //kmerIDs[seqNoMap.at(seqNo)].push_back(kmerID);
        // insert now unique occurrences listed in first vector (forward mappings)
        for (std::vector<TLocation>::const_iterator it_loc_fwd = it->second.first.begin(); it_loc_fwd != it->second.first.end(); ++it_loc_fwd)
        {
            TSeqNo seqNo_other = seqan::getValueI1<TSeqNo, TSeqPos>(*it_loc_fwd);
            TSeqPos seqPos_other = seqan::getValueI2<TSeqNo, TSeqPos>(*it_loc_fwd);
            auto idx = seqNoMap.at(seqNo_other); // compressed sequence id
            references[idx][seqPos_other] = 1;
            // rank computes number of 1s until given position (exclusive)
            sdsl::rank_support_v<> rb(&references[idx]);
            std::cout << "insert kmerIDs for seq " << idx << " at position " << rb(seqPos_other) << std::endl;

            kmerIDs[idx].insert(kmerIDs[idx].begin() + rb(seqPos_other), kmerID);

            std::cout << "insert kmerID = " << kmerID << " found at (" << seqNo_other << ", " << seqPos_other << " into row = " << idx << std::endl;
        }
    }
    locations.clear();
    std::cout << "Leaving frequency_filter2\n";
}

/*
 * Filter kmers based on their chemical properties regardless of their pairing.
 * Constraints that are checked: melting tempaerature, CG content
 */
void chemical_filter_single(primer_cfg_type const & primer_cfg, TKmerLocations & kmer_locations)
{
    assert(kmer_locations.size() < (1 << 24));
    auto mask = std::bitset<1 << 24>{};
    //std::bitset<1 << 18> aux_filter{};
    // Filter by melting temperature
    float Tm_min = primer_cfg.get_min_Tm();
    float Tm_max = primer_cfg.get_max_Tm();
    TKmerID kmerID;

    for (TKmerLocations::size_type i = 0; i < kmer_locations.size(); ++i)
    {

        // TODO: optimize - drop kmer when computing Tm in sequence lookup fct
        kmerID = kmer_locations[i].get_kmer_ID1();
        //std::cout << "kmerID = " << kmerID << std::endl;
        auto Tm = primer_melt_wallace(kmerID);
    //    std::cout << "Tm = " << Tm << std::endl;
        // filter by melting temperature
        if (Tm >= Tm_min && Tm <= Tm_max)
        {
            // filter by CG content
    //        std::cout << "call filter_CG ...\n";
            if (filter_CG(primer_cfg, kmerID))
            {

                // Filter if Gibb's free energy is below -6 kcal/mol
                if (filter_self_dimerization(kmerID))
                {
                    if (filter_repeats_runs(kmerID))
                        mask.set(i);
                }
            }
        }
    }

    // Delete all masked out entries (mask_i = 0).
    for (int32_t i = kmer_locations.size() - 1; i >= 0; --i)
    {
        if (!mask[i])
            kmer_locations.erase(kmer_locations.begin() + i);
    }
}

// check cross-dimerization.
void chemical_filter_pairs(/*primer_cfg_type const & primer_cfg, */TKmerPairs & kmer_pairs)
{
    assert(kmer_pairs.size() < (1 << 24));
    std::bitset<1 << 24> mask{};
    uint16_t i = 0;
    for (auto kmer_pair : kmer_pairs)
    {
        if (filter_cross_dimerization(kmer_pair.get_kmer_ID1(), kmer_pair.get_kmer_ID2()))
            mask.set(i);
        ++i;
    }
}

// post-filter candidates fulfilling chemical constraints by their relative frequency
/*void post_frequency_filter(TKmerLocations kmer_locations, TSeqNo occurrence_freq)
{

}*/

// filter k-mers by frequency and chemical properties
void pre_filter_main(io_cfg_type const & io_cfg, primer_cfg_type const & primer_cfg, TKLocations & locations, TReferences & references, TKmerIDs & kmerIDs, TSeqNoMap & seqNoMap)
{
    using TSeqNo = typename seqan::Value<typename TLocations::key_type, 1>::Type;

    // scale to be lower frequency bound for filters
    TSeqNo cutoff = primer_cfg.cutoff;
    // continue here
    std::cout << "INFO: Cut-off frequency = " << cutoff << std::endl;
    // frequency filter and sequence fetching
    //priset::io_cfg_type const & io_cfg, TKLocations & locations, TReferences & references, TKmerIDs & kmerIDs, TSeqNoMap & seqNoMap, TSeqNo const cutoff, std::unordered_map<std::string, TSeq> seq_map
    frequency_filter2(io_cfg, locations, references, kmerIDs, seqNoMap, primer_cfg.cutoff);
    std::cout << "INFO: kmers after frequency cutoff = " << kmer_locations.size() << std::endl;
    //chemical_filter_single(primer_cfg, kmer_locations);
    std::cout << "INFO: kmers after chemical filtering = " << kmer_locations.size() << std::endl;
}

// combine helper, forward window until both iterators point to same reference ID or end
void fast_forward(TKmerLocation::TLocationVec const & locations1, TKmerLocations::value_type::const_iterator & it1_loc_start, TKmerLocation::TLocationVec const & locations2, TKmerLocations::value_type::const_iterator & it2_loc_start)
{
    //std::cout << "enter fast_forward ..." << std::endl;
    while (it1_loc_start != locations1.end() && it2_loc_start != locations2.end() &&
            seqan::getValueI1<TSeqNo, TSeqPos>(*it1_loc_start) != seqan::getValueI1<TSeqNo, TSeqPos>(*it2_loc_start))
    {
        // speedup: use lower_bound/upper_bound
        while (it1_loc_start != locations1.end() && seqan::getValueI1<TSeqNo, TSeqPos>(*it1_loc_start) < seqan::getValueI1<TSeqNo, TSeqPos>(*it2_loc_start))
            ++it1_loc_start;
        while (it2_loc_start != locations2.end() && seqan::getValueI1<TSeqNo, TSeqPos>(*it2_loc_start) < seqan::getValueI1<TSeqNo, TSeqPos>(*it1_loc_start))
            ++it2_loc_start;
    }
    //std::cout << "... leaving ff" << std::endl;
}

/*
 * Helper for combine.
 *
 */

/* Combine based on suitable location distances s.t. transcript length is in permitted range.
 * Chemical suitability will be tested by a different function. First position indicates,
 * that the k-mer corresponds to a forward primer, and second position indicates reverse
 * primer, i.e. (k1, k2) != (k2, k1).
 */
void combine(primer_cfg_type const & primer_cfg, TKmerLocations const & kmer_locations, TKmerPairs & kmer_pairs)
{
    using it_loc_type = TKmerLocations::value_type::const_iterator;
    it_loc_type it1_loc_start, it1_loc_aux;
    it_loc_type it2_loc_start, it2_loc_aux;
    TKmer kmer1, kmer2;
    TKmerID kmer_ID1, kmer_ID2;
    TKmerLength K1, K2;
    for (auto it1 = kmer_locations.begin(); it1 != kmer_locations.end() && it1 != kmer_locations.end()-1; ++it1)
    {
        K1 = (*it1).get_K();
        for (auto it2 = it1+1; it2 != kmer_locations.end(); ++it2)
        {
            K2 = (*it2).get_K();
            kmer_ID1 = (*it1).get_kmer_ID();
            kmer_ID2 = (*it2).get_kmer_ID();
            assert(kmer_ID1 && kmer_ID2);

            // continue with next combination if kmer sequences do not pass cross-dimerization filter
            if (!filter_cross_dimerization(kmer_ID1, kmer_ID2))
                continue;
            // iterator to start position of current location for k-mer 1
            it1_loc_start = (*it1).locations.begin();
            // iterator to start position of current location for k-mer 2
            it2_loc_start = (*it2).locations.begin();
            // forward iterators to correspond to refer to same sequence ID or end
            fast_forward((*it1).locations, it1_loc_start, (*it2).locations, it2_loc_start);
            // no common reference sequences => forward kmer iterators
            if (it1_loc_start == (*it1).locations.end() || it2_loc_start == (*it2).locations.end())
                continue;

            // foward iterator for k-mer 1 on same sequence (loc)
            it1_loc_aux = it1_loc_start;
            // foward iterator for k-mer 2 on same sequence (loc)
            it2_loc_aux = it2_loc_start;
            // invariant after entering this loop: loc(it1) == loc(it2)
            auto seq_ID = seqan::getValueI1<TSeqNo, TSeqPos>(*it1_loc_start);
            while (it1_loc_start != (*it1).locations.end() && it1_loc_aux != (*it1).locations.end() && it2_loc_start != (*it2).locations.end() && seq_ID == seqan::getValueI1<TSeqNo, TSeqPos>(*it2_loc_aux))
            {
                // valid combination?
                auto pos_kmer1 = seqan::getValueI2<TSeqNo, TSeqPos>(*it1_loc_aux);
                if (it2_loc_aux != (*it2).locations.end())
                {
                    auto pos_kmer2 = seqan::getValueI2<TSeqNo, TSeqPos>(*it2_loc_aux);
                    auto pos_delta = (pos_kmer1 < pos_kmer2) ? pos_kmer2 - pos_kmer1 - K1: pos_kmer1 - pos_kmer2 - K2;
                    if (pos_delta >= primer_cfg.get_transcript_range().first && pos_delta <= primer_cfg.get_transcript_range().second)
                    {
                        TKmerID kmer_fwd_new = (pos_kmer1 < pos_kmer2) ? (*it1).get_kmer_ID() : (*it2).get_kmer_ID();
                        TKmerID kmer_rev_new = (pos_kmer1 < pos_kmer2) ? (*it2).get_kmer_ID() : (*it1).get_kmer_ID();
                        auto pair_location = std::make_tuple(seq_ID, std::min<TSeqPos>(pos_kmer1, pos_kmer2), std::max<TSeqPos>(pos_kmer1, pos_kmer2));

                        // extend location vector if pair combinations already in result
                        if (kmer_pairs.size() && kmer_pairs.back().get_kmer_ID1() == kmer_fwd_new && kmer_pairs.back().get_kmer_ID2() == kmer_rev_new)
                        {
                            kmer_pairs[kmer_pairs.size()-1].pair_locations.push_back(pair_location);
                        }
                        else
                        {
                            TKmerPair pair{kmer_fwd_new, kmer_rev_new, abs(primer_melt_wallace(kmer_fwd_new) - primer_melt_wallace(kmer_rev_new)), pair_location};
                            kmer_pairs.push_back(pair);
                        }
                    }
                    ++it2_loc_aux;
                }
                // all combinations tested for second k-mer
                if (it2_loc_aux == (*it2).locations.end())
                {
                    // reset to start of current sequence if next k-mer of it1 refers to same sequence, else forward it2
                    if (++it1_loc_aux == (*it1).locations.end())
                        break;
                    if (seqan::getValueI1<TSeqNo, TSeqPos>(*it1_loc_aux) == pos_kmer1)
                        it2_loc_aux = it2_loc_start;
                    else
                    {
                        it1_loc_start = it1_loc_aux;
                        fast_forward((*it1).locations, it1_loc_start, (*it2).locations, it2_loc_start);
                        it1_loc_aux = it1_loc_start;
                        it2_loc_aux = it2_loc_start;
                    }
                }
            }
        }
    }
}

// primer_cfg_type const & primer_cfg, TKmerLocations const & kmer_locations, TKmerPairs & kmer_pairs
void combine2(primer_cfg_type const & primer_cfg, TReferences const & references, TKmerIDs const & kmerIDs, TPairs & pairs)
{
    pairs.clear();
    pairs.resize(kmerIDs.size());
    uint64_t offset_min = primer_cfg.get_transcript_range().first;
    uint64_t offset_max = primer_cfg.get_transcript_range().second;
    for (uint64_t i = 0; i < references.size(); ++i)
    {
        TReference reference = references.at(i);
        auto kmerID_list = kmerIDs.at(i);

        sdsl::rank_support_v5<1, 1> r1s(reference);
        sdsl::select_support_mcl<1> s1s(reference);

        for (uint64_t r = 1; r < r1s.rank(reference->size()); ++r)
        {
            uint64_t idx = s1s.select(r); // position of r-th k-mer
            // window start position (inclusive)
            uint64_t w_begin = std::min(reference->size()-1, idx + K + offset_min + 1);
            // window end position (exclusive)
            uint64_t w_end = std::min(reference->size(), idx + K + offset_max + 1);
            //cout << "w_begin = " << w_begin << ", w_end = " << w_end << ", rank(w_begin) = " << r1s.rank(w_begin) << ", rank(w_end) = " << r1s.rank(w_end) << endl;
            cout << "number of ones in window: " << (r1s.rank(w_end) - r1s.rank(w_begin)) << std::endl;
            for (uint64_t k = 1; k <= r1s.rank(w_end) - r1s.rank(w_begin); ++k)
            {
                uint64_t r2 = r1s.rank(w_begin) + k; // relative kmer position in kmerIDs
                uint64_t idx2 = s1s.select(r2);
                if (kmerID_list.at(r) != kmerID_list.at(r2))
                {
                    pairs[i].push_back(std::make_pair(idx, idx2));
                    std::cout << "pair: (" << idx << ", " << idx2 << ") = " << "'"<< dna_decoder(kmerID_list.at(r)) << "' and '" << dna_decoder(kmerID_list.at(r2)) << "'\n";
                }
            }
        }
    }
}

primer_cfg, kmerIDs, pairs
void post_filter_main(primer_cfg_type const & primer_cfg, TKmerIDs & kmerIDs, TPairs & pairs)
{

}

}  // namespace priset
