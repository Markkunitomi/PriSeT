// ============================================================================
//                    PriSeT - The Primer Search Tool
// ============================================================================
//          Author: Marie Hoffmann <marie.hoffmann AT fu-berlin.de>
//          Manual: https://github.com/mariehoffmann/PriSeT

// Primer Settings.

#pragma once

#include <unordered_map>

#include <seqan/basic.h>

//#include "chemistry.hpp"
#include "types.hpp"

namespace priset
{

#define WORD_SIZE 64

// The number of tailing bits reserved in a KmerID for holding the integer compression of a kmer DNA sequence.
#define CODE_SIZE 52

// The number of leading bits reserved in a KmerID to store kmer lengths (= primer_max_length - primer_min_length + 1).
// Maximal possible length is 64 - CODE_SIZE.
#define PREFIX_SIZE 10

// Mask selection, i.e. 10 leading bits and rest 0 or ~(1 << 54) - 1)
#define PREFIX_SELECTOR 18428729675200069632ULL

// The minimal primer length (or a kmer). Recommended: 18.
#define PRIMER_MIN_LEN 16ULL

// The maximal primer length (or a kmer). Recommended: 22.
#define PRIMER_MAX_LEN 25ULL

/* Primer Verification */

// The minimal transcript length.
#define TRANSCRIPT_MIN_LEN 60

// The minimal transcript length.
#define TRANSCRIPT_MAX_LEN 150

// The minimal primer melting temperature. Recommended 52.
#define PRIMER_MIN_TM 52

// The maximal primer melting temperature. Recommended 58.
#define PRIMER_MAX_TM 58

// Maximal permitted temperature difference [Kelvin] of primers.
// Differences above 5 Kelvin can lead to no amplification.
#define PRIMER_DTM 5

// The lower bound for relative CG content. Recommended: 0.4.
#define CG_MIN_CONTENT .4

// The upper bound for relative CG content. Recommended: 0.6.
#define CG_MAX_CONTENT .6

// The minimal distance (bp) between two identical kmers on same reference. Recommended: 400
#define TRAP_DIST 400

// Lower kmer frequency cutoff in percentage, i.e. all kmer occurences below will be dropped.
#define FREQ_KMER_MIN_PERCENT 20

// Lower kmer pair frequency cutoff, i.e. all pair occurences below will be dropped.
#define FREQ_PAIR_MIN_PERCENT 1


struct primer_cfg_type
{
public:
    // Member types
    using float_type = float;
    
    using size_type = uint32_t;

    // interval type for primer length ranges
    using size_interval_type = typename std::pair<size_type, size_type>;

    // The taxonomic identifier type.
    using taxid_type = unsigned int;

    // The index type to identify kmer sequences.
    using kmer_ID_type = unsigned short int;

private:
    // Root taxonomic identifier below which references are sampled.
    TTaxid root_taxid{1};

    // Number of positions varying from kmer sequence, i.e. number of permitted primer errors.
    size_type E{0};

public:
    // Constructors, destructor and assignment
    // Default constructor.
    constexpr primer_cfg_type() = default;

    // Default copy constructor.
    constexpr primer_cfg_type(primer_cfg_type const &) = default;

    // Default copy construction via assignment.
    primer_cfg_type & operator=(primer_cfg_type const &) = default;

    // Move constructor.
    constexpr primer_cfg_type(primer_cfg_type && rhs) = default;

    // Move assignment.
    primer_cfg_type & operator=(primer_cfg_type && rhs) = default;

    // Init with user defined k-mer length and error number
    primer_cfg_type(size_type E_)
    {
        E = E_;
    }

    // Use default deconstructor.
    ~primer_cfg_type() = default;
    //!\}

    //
    constexpr void set_root_taxid(taxid_type taxid)
    {
        root_taxid = taxid;
    }

    constexpr taxid_type get_root_taxid() const noexcept
    {
        return root_taxid;
    }

    // Set primer error number (E parameter for genmap's mapping).
    void set_error(size_type E_)
    {
        assert(E_ < 5);
        E = E_;
    }

    // Get primer error number.
    size_type get_error() const noexcept
    {
        return E;
    }
};

}  // namespace priset
