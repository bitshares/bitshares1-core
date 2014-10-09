#include "update_utility.hpp"

#include <fc/io/raw.hpp>
#include <fc/io/json.hpp>
#include <fc/io/fstream.hpp>
#include <fc/compress/lzma.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/filesystem/fstream.hpp>

#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

void update_utility::open_manifest(fc::path path)
{
    _manifest_path = path;

    if (fc::exists(path))
        _manifest = fc::json::from_file(path).as<WebUpdateManifest>();
    else
        write_manifest();
}

void update_utility::write_manifest()
{
    fc::json::save_to_file(fc::variant(_manifest), _manifest_path, false);
}

void update_utility::pack_web(fc::path path, string output_file)
{
    vector<pair<string, vector<char>>> packed_files;
    int packed_file_count = 0;
    for (fc::recursive_directory_iterator itr(path); itr != fc::recursive_directory_iterator(); ++itr)
    {
        string relative_path = (*itr).to_native_ansi_path().erase(0, path.to_native_ansi_path().size());
        if (relative_path[0] != '/')
            relative_path = "/" + relative_path;

        if (!fc::is_regular_file(*itr))
            continue;

        ++packed_file_count;
        cout << relative_path << endl;
        boost::filesystem::ifstream infile(*itr);
        vector<char> file;
        file.reserve(fc::file_size(*itr));

        char c = infile.get();
        while (infile) {
            file.push_back(c);
            c = infile.get();
        }
        infile.close();

        packed_files.push_back(std::make_pair(relative_path, file));
    }
    cout << endl;

    vector<char> packed_stream = fc::raw::pack(packed_files);
    vector<char> compressed_stream = fc::lzma_compress(packed_stream);

    fc::ofstream outfile(output_file);
    outfile.write(compressed_stream.data(), compressed_stream.size());
    outfile.close();
}

void update_utility::sign_update(WebUpdateManifest::UpdateDetails& update, fc::path update_package, bts::blockchain::private_key_type signing_key)
{
    fc::sha256::encoder enc;
    boost::filesystem::ifstream infile(update_package);
    char c = infile.get();
    while (infile)
    {
        enc.put(c);
        c = infile.get();
    }
    infile.close();
    std::string desc = update.signable_string();
    enc.write(desc.c_str(), desc.size());

    update.signatures.insert(signing_key.sign_compact(enc.result()));
}
