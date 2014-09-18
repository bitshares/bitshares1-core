#pragma once

#include <fc/filesystem.hpp>

#include <WebUpdates.hpp>

class update_utility
{
    fc::path _manifest_path;
    WebUpdateManifest _manifest;

public:
    void open_manifest(fc::path path);
    void write_manifest();

    WebUpdateManifest& manifest() { return _manifest; }

    void pack_web(fc::path path, std::string output_file);
    void sign_update(WebUpdateManifest::UpdateDetails& update, fc::path update_package, bts::blockchain::private_key_type signing_key);
};
