#include "update_utility.hpp"

#include <fc/io/raw.hpp>
#include <fc/io/json.hpp>
#include <fc/io/fstream.hpp>
#include<fc/compress/lzma.hpp>
#include <fc/reflect/variant.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <iostream>

#include <QDir>
#include <QString>

using namespace std;

int main()
{
    update_utility util;
    char option;

    cout << "Welcome to the BitShares Web Update Utility. This tool is not particularly well-written. This tool is not user friendly. It is not for users. It is for developers. Deal with it.\n\nWould you like to (p)repare a new update, or (s)ign an existing one? ";
    cin >> option;
    option = tolower(option);

    while (option != 'p' && option != 's')
    {
        cout << "Unrecognized input. Please enter either 'p' or 's': ";
        cin >> option;
        option = tolower(option);
    }
    cin.get(); // Discard the newline.

    string path;
    cout << "Enter the path to the manifest to work on. If the manifest does not exist, a new one will be created. Manifest: ";
    getline(cin, path);
    while (QFileInfo(path.c_str()).exists() && !QFileInfo(path.c_str()).isWritable())
    {
        cout << "Cannot write that manifest. Enter the path to the manifest: ";
        getline(cin, path);
    }
    util.open_manifest(path);

    if (option == 'p')
    {
        //Prepare new update.
        WebUpdateManifest::UpdateDetails update;
        if (util.manifest().updates.size()) {
            update = *util.manifest().updates.rbegin();
            ++update.patchVersion;
            if (update.patchVersion > 'z')
            {
                //Hopefully this never actually happens. :P
                ++update.minorVersion;
                update.patchVersion = 'a';
            }
        }
        string response;
        cout << "What is the major version of this update [" << (short)update.majorVersion << "]? ";
        getline(cin, response);
        if (response != "") update.majorVersion = atoi(response.c_str());
        cout << "What is the fork version of this update [" << (short)update.forkVersion << "]? ";
        getline(cin, response);
        if (response != "") update.forkVersion = atoi(response.c_str());
        cout << "What is the minor version of this update [" << (short)update.minorVersion << "]? ";
        getline(cin, response);
        if (response != "") update.minorVersion = atoi(response.c_str());
        cout << "What is the patch version of this update [" << (char)update.patchVersion << "]? ";
        getline(cin, response);
        if (response != "") update.patchVersion = tolower(response[0]);
        update.timestamp = fc::time_point::now();

        update.releaseNotes.clear();
        cout << "Now type the release notes. Newlines will be accepted as part of the input. To finish typing the release notes, enter a line containing only \":wq\".\n";
        string line;
        getline(cin, line);
        while (line != ":wq")
        {
            update.releaseNotes += line + "\n";
            getline(cin, line);
        }

        cout << "Enter the path (relative or absolute) to the web root: ";
        getline(cin, path);
        while (!QFileInfo(path.c_str()).isDir())
        {
            cout << "Unrecognized path. Enter the path to the web root: ";
            getline(cin, path);
        }
        cout << "Enter the output file name [" << QStringLiteral("%1.%2.%3-%4.pak").arg(update.majorVersion)
                                                                                   .arg(update.forkVersion)
                                                                                   .arg(update.minorVersion)
                                                                                   .arg(QChar(update.patchVersion))
                                                                                   .toStdString()
             << "]: ";
        string filename;
        getline(cin, filename);
        if (filename.empty())
            filename = QStringLiteral("%1.%2.%3-%4.pak").arg(update.majorVersion)
                                                        .arg(update.forkVersion)
                                                        .arg(update.minorVersion)
                                                        .arg(QChar(update.patchVersion))
                                                        .toStdString();
        util.pack_web(path, filename);

        cout << "Enter the full URL where the update package will be hosted: ";
        getline(cin, update.updatePackageUrl);

        cout << "OK, and did you want to sign that update too? (y/n): ";
        cin >> option;
        cin.get(); // chomp
        if (tolower(option) == 'y')
        {
            string wif;
            cout << "Enter WIF private key to sign with: ";
            getline(cin, wif);
            auto key = bts::utilities::wif_to_key(wif);
            while (!key)
            {
                cout << "Couldn't parse that key. Enter WIF key: ";
                getline(cin, wif);
                key = bts::utilities::wif_to_key(wif);
            }

            util.sign_update(update, filename, *key);
        }

        util.manifest().updates.insert(update);
    } else {
        string filename;
        cout << "Enter the update package file name: ";
        getline(cin, filename);
        while (!fc::exists(filename))
        {
            cout << "No such file. Enter the update package file name: ";
            getline(cin, filename);
        }

        string version;
        WebUpdateManifest::UpdateDetails update;
        cout << "Enter the version number of the update to sign: ";
        getline(cin, version);
        QStringList parts = QString(version.c_str()).replace('-', '.').split(".");
        while (parts.size() != 4)
        {
            cout << "Wat? Enter the version number of the update to sign: ";
            getline(cin, version);
            parts = QString(version.c_str()).replace('-', '.').split(".");
        }

        //What could possibly go wrong??
        update.majorVersion = parts[0].toInt();
        update.forkVersion = parts[1].toInt();
        update.minorVersion = parts[2].toInt();
        update.patchVersion = parts[3].toStdString()[0];
        
        if (util.manifest().updates.find(update) == util.manifest().updates.end())
        {
            cout << "That version isn't in this manifest. Seriously, can't you get anything right? I really don't need this. I'm done. Ugh.";
            return 666;
        }
        update = *util.manifest().updates.find(update);

        string wif;
        cout << "Enter WIF private key to sign with: ";
        getline(cin, wif);
        auto key = bts::utilities::wif_to_key(wif);
        while (!key)
        {
            cout << "Couldn't parse that key. Enter WIF key: ";
            getline(cin, wif);
            key = bts::utilities::wif_to_key(wif);
        }

        util.sign_update(update, filename, *key);
        util.manifest().updates.erase(update);
        util.manifest().updates.insert(update);
    }

    cout << "Writing manifest.\n";
    util.write_manifest();
}
