#include <vector>
#include <iostream>
#include <fstream>
#include <string>

#include <fc/filesystem.hpp>
#include <fc/compress/lzma.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/datastream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/raw.hpp>

#include <boost/filesystem/fstream.hpp>

using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::pair;

int main(int argc, char** argv) {
    if (argc != 3 || argv[1] == string("-h") || argv[1] == string("--help"))
    {
        cout << "Usage: " << argv[0] << " directory_to_pack private_key_hex\n";
        return 0;
    }

    if (!fc::is_directory(argv[1]))
    {
        cout << "Error: " << argv[1] << " is not a directory.\n";
        return 1;
    }

    fc::ecc::private_key signing_key;
    try {
        signing_key = fc::ecc::private_key::regenerate(fc::sha256(argv[2]));
    } catch (fc::exception e) {
        cout << "Error: Unable to parse " << argv[2] << " as a private key: " << e.to_string() << endl;
        return 1;
    }

    fc::path pack_dir(argv[1]);
    cout << "Now packing " << pack_dir.preferred_string()
         << " and signing. Signature may be verified with public key " << signing_key.get_public_key().to_base58() << endl;

    vector<pair<string, vector<char>>> packed_files;
    int packed_file_count = 0;
    for (fc::recursive_directory_iterator itr(pack_dir); itr != fc::recursive_directory_iterator(); ++itr)
    {
        string relative_path = (*itr).to_native_ansi_path().erase(0, pack_dir.to_native_ansi_path().size());
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

    fc::ofstream outfile("web.dat");
    outfile.write(compressed_stream.data(), compressed_stream.size());
    outfile.close();

    fc::time_point_sec timestamp = fc::time_point::now();
    for (char c : timestamp.to_non_delimited_iso_string())
        compressed_stream.push_back(c);
    fc::ecc::compact_signature stream_signature = signing_key.sign_compact(fc::sha256::hash(compressed_stream.data(), compressed_stream.size()));

    outfile.open("web.sig");
    fc::raw::pack(outfile, std::make_pair(stream_signature, timestamp));
    outfile.close();

    cout << "Finished packing " << packed_file_count << " files (" << compressed_stream.size() << " bytes)."
         << " Signature: " << fc::variant(stream_signature).as_string() << "\n";

    return 0;
}
