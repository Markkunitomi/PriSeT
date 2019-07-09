// ============================================================================
//                    PriSeT - The Primer Search Tool
// ============================================================================
//          Author: Marie Hoffmann <marie.hoffmann AT fu-berlin.de>
//          Manual: https://github.com/mariehoffmann/PriSeT

#pragma once

#include <cstring>
#include <experimental/filesystem>
#include <iostream>
#include <regex>

#include <seqan/basic.h>

#include "chemistry.hpp"
#include "errors.hpp"

namespace fs = std::experimental::filesystem;

namespace priset
{

#define MAX_PATH_LENGTH 100

struct io_cfg_type
{

public:
    // Default constructor.
    io_cfg_type() = default;

    // Default copy constructor.
    io_cfg_type(io_cfg_type const &) = default;

    // Default copy construction via assignment.
    io_cfg_type & operator=(io_cfg_type const &) = default;

    // Move constructor.
    io_cfg_type(io_cfg_type && rhs) = default;

    // Move assignment.
    io_cfg_type & operator=(io_cfg_type && rhs) = default;

    // Set library and working directory paths.
    void assign(fs::path const & lib_dir_, fs::path const & work_dir_, bool const skip_idx_)
    {
        lib_dir = fs::canonical(lib_dir_);
        if (!fs::exists(work_dir_))
            fs::create_directory(work_dir_);
        work_dir = fs::canonical(work_dir_);
        skip_idx = skip_idx_;
        index_dir = work_dir;
        mapping_dir = work_dir;
        genmap_bin = fs::current_path() / "submodules/genmap/bin/genmap";
        // parse library directory and assign paths to the .acc, .fasta, and .tax files
//        std::cout << "current path: " << fs::current_path() << std::endl;

        if (!fs::exists(lib_dir))
            std::cout << "ERROR: " << LIB_DIR_ERROR << std::endl, exit(-1);
        for (auto & p : fs::directory_iterator(lib_dir))
        {
            std::cout << p << std::endl;
            if (p.path().extension().compare(ext_acc) == 0)
                acc_file = p;
            else if (p.path().extension().compare(ext_fasta) == 0)
                fasta_file = p;
            else if (p.path().extension().compare(ext_tax) == 0)
                tax_file = p;
            else if (p.path().extension().compare(ext_id) == 0)
                id_file = p;
        }
        if (!acc_file.has_filename())
            std::cout << "ERROR: Unable to locate accession file in: " << lib_dir << std::endl, exit(-1);
        std::cout << "STATUS\tSet accessions file: \t" << acc_file << std::endl;
        if (!fasta_file.has_filename())
            std::cout << "ERROR: Unable to locate fasta file in: " << lib_dir << std::endl, exit(-1);
        std::cout << "STATUS\tSet fasta file: \t" << fasta_file << std::endl;
        if (!tax_file.has_filename())
            std::cout << "ERROR: Unable to locate taxonomy file in: " << lib_dir << std::endl, exit(-1);
        std::cout << "STATUS\tSet taxonomy file: \t" << tax_file << std::endl;
        if (!id_file.has_filename())
            std::cout << "ERROR: Unable to locate id file in: " << lib_dir << std::endl, exit(-1);
        std::cout << "STATUS\tSet id file: \t" << id_file << std::endl;

        // create working directory if not existing after clearing
        if (!fs::exists(work_dir))
        {
            char cmd[50];
            sprintf(cmd, "mkdir -p %s", work_dir.c_str());
            if (system(cmd))
                std::cout << "ERROR: " << WRK_DIR_ERROR << std::endl, exit(-1);
        }

        // set output directory for FM index, will be created by genmap
        index_dir /= fs::path("/index");
        if (!skip_idx && fs::exists(index_dir))
        {
            char cmd[50];
            sprintf(cmd, "rm -r %s", index_dir.c_str());
            if (system(cmd))
                std::cout << "ERROR: Could not remove index directory " << index_dir << std::endl, exit(0);
        }
        // set output directory for FM index mapping
        mapping_dir /= fs::path("/mapping");
        std::cout << "set mapping_dir = " << mapping_dir << std::endl;

        // create table output directory
        fs::path result_path = work_dir / "table";
        if (!fs::exists(result_path))
        {
            char cmd[50];
            sprintf(cmd, "mkdir %s", result_path.c_str());
            if (system(cmd))
                std::cout << "ERROR: Creating result table directory = " << result_path << std::endl, exit(-1);
        }
        result_file = result_path / "results.csv";
        primer_info_file = result_path / "primer_info.csv";
        script_file = get_work_dir() / "app" / "app.R";
        std::cout << "STATUS\tSet R script file: " << script_file << std::endl;
        script_runner = get_work_dir() / "app" / "app_run.R";

        if (!fs::exists(script_runner))
        {
            std::ofstream ofs;
            ofs.open(script_runner);
            ofs << "library(shiny)\nrunApp(" << script_file << ")\n";
            ofs.close();
        }
    };

    // Destructor.
    ~io_cfg_type() = default;

    // Return skip_idx flag.
    bool get_skip_idx() const noexcept
    {
        return skip_idx;
    }

    // Return accession file with absolute path as filesystem::path object.
    fs::path get_acc_file() const noexcept
    {
        return acc_file;
    }

    fs::path get_id_file() const noexcept
    {
        return id_file;
    }

    // Return template file for shiny app.
    fs::path get_app_template() const noexcept
    {
        return app_template;
    }

    // Return library file with absolute path as filesystem::path object.
    fs::path get_fasta_file() const noexcept
    {
        return fasta_file;
    }

    // Return path of genmap binary
    fs::path get_genmap_binary() const noexcept
    {
        return genmap_bin;
    }

    // Return directory where FM index is stored
    fs::path get_index_dir() const noexcept
    {
        return index_dir;
    }

    // path to index plus basename without suffix, i.e. <path_to_index>/<basename>
    fs::path get_index_base_path() const noexcept
    {
        return index_dir / "index";
    }

    // path to index plus basename without suffix, i.e. <path_to_index>/<basename>
    fs::path get_index_base_path_ids() const noexcept
    {
        return index_dir / "index.ids";
    }

    // return path to concatenation of text corpus stored in two files index.txt.concat and index.txt.limits
    fs::path get_index_txt_path() const noexcept
    {
        return index_dir / "index.txt";
    }

    fs::path get_mapping_dir() const noexcept
    {
        return mapping_dir;
    }

    // Return primer info file.
    fs::path get_primer_info_file() const noexcept
    {
        return primer_info_file;
    }

    // Return file to store results in csv format.
    fs::path get_result_file() const noexcept
    {
        return result_file;
    }

    // Return path to R script file to be run in terminal.
    fs::path get_script_file() const noexcept
    {
        std::cout << "Enter get_script_file, " << script_file << std::endl;
        return script_file;
    }

    // Return path to R script starter file.
    fs::path get_script_runner() const noexcept
    {
        return script_runner;
    }

    // Return taxonomy file with absolute path as filesystem::path object.
    fs::path get_tax_file() const noexcept
    {
        return tax_file;
    }

    fs::path get_work_dir() const noexcept
    {
        return work_dir;
    }

private:
    // Directory that contains library, taxonomy, and taxid to accession files.
    fs::path lib_dir;
    // Working base directory for storing FM indices, mappings and results.
    fs::path work_dir;
    // Flag for indicating if index computation shall be skipped (because it already exists).
    bool skip_idx;

    // Taxid to accession map in csv format (set by PriSeT).
    fs::path acc_file{};
    // Sequence library file in fasta format (set by PriSeT).
    fs::path fasta_file{};
    // 1-based IDs and accession numbers.
    fs::path id_file{};
    // Taxonomy file in csv format (set by PriSeT).
    fs::path tax_file{};
    // Working subdirectory for FM index (set by PriSeT).
    fs::path index_dir;
    // Working subdirectory for FM index mappings (set by PriSeT).
    fs::path mapping_dir;
    // Path to genmap binary (set by PriSeT).
    fs::path genmap_bin;
    // Library file extensions.
    std::string ext_fasta = ".fasta";
    std::string ext_acc = ".acc";
    std::string ext_tax = ".tax";
    std::string ext_id = ".id";
    // Path to R shiny app template
    fs::path app_template = "../PriSeT/src/app_template.R";
    // R script for launching shiny app.
    fs::path script_runner;
    // Path to generated copy of R script to be run in terminal.
    fs::path script_file;
    // Path to store result tables to load in Shiny.
    fs::path result_file;
    // Path to primer info file (sequences and chemical attributes)
    fs::path primer_info_file;

};

} // namespace priset
