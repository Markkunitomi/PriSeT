#include <iostream>
#include <experimental/filesystem>
#include <fstream>
#include <regex>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <seqan/basic.h>
#include <seqan/file.h>
#include <seqan/sequence.h>
#include <seqan/stream.h>

#include "argument_parser.hpp"
#include "filter.hpp"
#include "gui.hpp"
#include "primer_cfg_type.hpp"
#include "types.hpp"
#include "utilities.hpp"

namespace fs = std::experimental::filesystem;

// g++ ../PriSeT/src/test.cpp -Wno-write-strings -std=c++17 -Wall -Wextra -lstdc++fs -o test

struct setup
{
    // TODO: make this runnable with arbitrarily located build folders
    std::string lib_dir = (fs::canonical("../PriSeT/src/tests/library")).string();
    std::string work_dir = (fs::canonical("../PriSeT/src/tests/work")).string();
    priset::io_cfg_type io_cfg{};
    priset::primer_cfg_type primer_cfg{};
    priset::TKmerLocations kmer_locations;
    priset::TKmerMap kmer_map;

    setup()
    {
        // basic init
        priset::options opt;
        int argc = 5;
        char * argv[5] = {"priset", "-l", &lib_dir[0], "-w", &work_dir[0]};
        for (auto i = 0; i < argc; ++i)
            std::cout << "arg[" << i << "] = " << argv[i] << std::endl;

        opt.parse_arguments(argc, argv, io_cfg, primer_cfg);

        // change primer and transcript length ranges
        // k1: [(1,2), (1,75)], i.e. kmer1 occurs in  sequence 1 at position 2 and 75
        // k2: [(1,5), (1,80)], i.e. kmer2 occurs in sequence 2 at positions 20 and 80
        // -k1[2]-k2[20]-------------k1[75]/k2[80]
        primer_cfg.set_primer_length_range(4, 8);
        primer_cfg.set_transcript_range(50, 800);

        priset::TLocation loc1_kmer1{1, 2};
        priset::TLocation loc2_kmer1{1, 75};
        priset::TLocation loc1_kmer2{1, 5};
        priset::TLocation loc2_kmer2{1, 80};

        // init locations
        priset::TKmerLocation::TLocationVec loc_vec1{{loc1_kmer1, loc2_kmer1}};
        priset::TKmerLocation::TLocationVec loc_vec2{{loc1_kmer2, loc2_kmer2}};
        kmer_locations.push_back(priset::TKmerLocation(1, loc_vec1));
        kmer_locations.push_back(priset::TKmerLocation(2, loc_vec2));

        kmer_map[1] = priset::TKmer{1, "AAAA", 12.0};
        kmer_map[2] = priset::TKmer{2, "ACCC", 13.0};
    }
};

void gui_test()
{
    setup up{};
    if (priset::gui::generate_app(up.io_cfg) && priset::gui::compile_app(up.io_cfg))
        std::cout << "success" << std::endl;
    else
        std::cout << "failed to generate and compile app" << std::endl;
}

void combine_test()
{
    setup s{};

    priset::TKmerPairs pairs{};
    priset::combine(s.primer_cfg, s.kmer_locations, s.kmer_map, pairs);
    priset::print_pairs(pairs, s.kmer_map);
}

void create_table_test()
{
    setup s{};
    priset::TKmerPairs pairs{};
    create_table(s.io_cfg, s.kmer_locations, pairs, s.kmer_map);
}

void lookup_sequences_test()
{
    seqan::StringSet<seqan::DnaString, seqan::Owner<seqan::ConcatDirect<>>> text;
    fs::path text_path = "/Users/troja/priset/335928/work/index/index.txt"; //io_cfg.get_index_txt_path();
    std::cout << "text_path: " << text_path << std::endl;
    seqan::open(text, text_path.string().c_str(), seqan::OPEN_RDONLY);

    std::cout << seqan::valueById(text, 0) << std::endl;
    typedef seqan::Iterator<seqan::StringSet<seqan::DnaString, seqan::Owner<seqan::ConcatDirect<>>>>::Type TStringSetIterator;
    for (TStringSetIterator it = seqan::begin(text); it != seqan::end(text); ++it)
        std::cout << *it << '\n';

}

int main()
{
    //combine_test();
    //create_table_test();
    gui_test();
    //lookup_sequences_test();
}
