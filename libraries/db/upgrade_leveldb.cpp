#include <bts/db/exception.hpp>
#include <bts/db/upgrade_leveldb.hpp>
#include <fc/log/logger.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <boost/regex.hpp>
#include <boost/filesystem/fstream.hpp>

namespace bts { namespace db {

    upgrade_db_mapper& upgrade_db_mapper::instance()
    {
        static upgrade_db_mapper  mapper;
        return mapper;
    }

    int32_t upgrade_db_mapper::add_type( const std::string& type_name, const upgrade_db_function& function) 
    { 
        _upgrade_db_function_registry[type_name] = function;
        return 0;
    }


    // this code has no bitshares dependencies, and it
    // could be moved to fc, if fc ever adds a leveldb dependency
    void try_upgrade_db( const fc::path& dir, leveldb::DB* dbase, const char* record_type, size_t record_type_size )
    {
      size_t old_record_type_size = 0;
      std::string old_record_type;
      fc::path record_type_filename = dir / "RECORD_TYPE";
      //if no RECORD_TYPE file exists
      if ( !boost::filesystem::exists( record_type_filename ) )
      { 
        //must be original type for the database
        old_record_type = record_type;
        int last_char = old_record_type.length() - 1;
        //strip version number from current_record_name and append 0 to set old_record_type (e.g. mytype0)
        while (last_char >= 0 && isdigit(old_record_type[last_char]))
        {
          --last_char;
        }

        //upgradeable record types should always end with version number
        if( 'v' != old_record_type[last_char] )
        {
          //ilog("Database ${db} is not upgradeable",("db",dir.to_native_ansi_path()));
          return;
        }

        ++last_char;
        old_record_type[last_char] = '0';
        old_record_type.resize(last_char+1);
      }
      else //read record type from file
      {
        boost::filesystem::ifstream is(record_type_filename);
        char buffer[120];
        is.getline(buffer,120);
        old_record_type = buffer;
        is >> old_record_type_size;
      }
      if (old_record_type != record_type)
      {
        //check if upgrade function in registry
        auto upgrade_function_itr = upgrade_db_mapper::instance()._upgrade_db_function_registry.find( old_record_type );
        if (upgrade_function_itr != upgrade_db_mapper::instance()._upgrade_db_function_registry.end())
        {
          ilog("Upgrading database ${db} from ${old} to ${new}",("db",dir.preferred_string())
                                                                ("old",old_record_type)
                                                                ("new",record_type));
          //update database's RECORD_TYPE to new record type name
          boost::filesystem::ofstream os(record_type_filename);
          os << record_type << std::endl;
          os << record_type_size;
          //upgrade the database using upgrade function
          upgrade_function_itr->second(dbase);
        }
        else
        {
          elog("In ${db}, record types ${old} and ${new} do not match, but no upgrade function found!",
                   ("db",dir.preferred_string())("old",old_record_type)("new",record_type));
        }
      }
      else if (old_record_type_size == 0) //if record type file never created, create it now
      {
        boost::filesystem::ofstream os(record_type_filename);
          os << record_type << std::endl;
          os << record_type_size;
      }
      else if (old_record_type_size != record_type_size)
      {
        elog("In ${db}, record type matches ${new}, but record sizes do not match!",
                 ("db",dir.preferred_string())("new",record_type));

      }
    }
} } // namespace bts;:db
