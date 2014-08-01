#include <bts/bitcoin/electrum.hpp>

#include <bts/wallet/exceptions.hpp>
#include <bts/blockchain/pts_address.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/base64.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

#include <boost/algorithm/string.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/foreach.hpp>

#include <boost/fusion/include/std_pair.hpp>

#include <iostream>
#include <fstream>

namespace bts { namespace bitcoin {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

struct python_dict_type_t;
struct python_dict_array_type_t;

typedef boost::variant<
  boost::recursive_wrapper<python_dict_type_t>,
  std::string,
  boost::recursive_wrapper<python_dict_array_type_t>
  > python_dict_val_t;

struct python_dict_array_type_t
{
  std::vector<python_dict_val_t> items;
};

typedef std::pair<std::string,python_dict_val_t> python_dict_elem_type_t;

struct python_dict_type_t
{
  std::vector<python_dict_elem_type_t> items;
};

} } // bts::bitcoin

BOOST_FUSION_ADAPT_STRUCT(bts::bitcoin::python_dict_type_t,
			  (std::vector<bts::bitcoin::python_dict_elem_type_t>, items))

BOOST_FUSION_ADAPT_STRUCT(bts::bitcoin::python_dict_array_type_t,
			  (std::vector<bts::bitcoin::python_dict_val_t>, items))

namespace bts { namespace bitcoin {

template <typename It>
struct parser final : qi::grammar<It, python_dict_type_t(), ascii::space_type>
{
  parser() : parser::base_type(dict)
  {
     dict %= '{' >> pairs >> '}';
     pairs = -(pair % ',') ;
     pair = key >> ':' >> val;
     key %= qstring | string;
     val %= qstring | string | array | tuple | dict;
     array %= '[' >> arrayvals >> ']';
     tuple %= '(' >> arrayvals >> ')';
     arrayvals = -(val % ',');
     string = +(~qi::char_("{[()]}:',"));
     qstring = -qi::char_('u') >> '\'' >> qcontents >> '\'';
     qcontents = *(esc | ~qi::char_("'"));
     esc = '\\' >> qi::char_("'");
  }

private:
  qi::rule<It, python_dict_type_t(), ascii::space_type> dict;
  qi::rule<It, std::vector<python_dict_elem_type_t>(), ascii::space_type> pairs;
  qi::rule<It, python_dict_elem_type_t(), ascii::space_type> pair;
  qi::rule<It, python_dict_array_type_t(), ascii::space_type> array;
  qi::rule<It, python_dict_array_type_t(), ascii::space_type> tuple;
  qi::rule<It, std::vector<python_dict_val_t>(), ascii::space_type> arrayvals;
  qi::rule<It, python_dict_val_t(), ascii::space_type> val;
  qi::rule<It, std::string(), ascii::space_type> key, string, qstring, qcontents, esc;
};

class electrumwallet
{
private:
  bool encryption;
  std::string seed, seed_version, masterkey;
  bts::bitcoin::python_dict_type_t accounts;
  std::vector<fc::ecc::private_key> privatekeys;

  void addkey_v4( int no, int n )
  {
     // stretch key
     std::string oldseed = seed;
     std::string stretched = seed;
     for( int i=0; i<100000; i++ )
     {
        if( i == 0 )
        {
           stretched = fc::sha256::hash( stretched + oldseed );
        }
        else
        {
           std::vector<char> bytes(stretched.size() / 2);
           fc::from_hex( stretched, &bytes[0], bytes.size() );
           std::string tmp(bytes.begin(), bytes.end());
           stretched = fc::sha256::hash( tmp + oldseed );
        }
     }

     // compute Hash( 'n:no:' + mpk )
     std::vector<char> mpk( masterkey.size() / 2);
     fc::from_hex( masterkey, &mpk[0], mpk.size());
     std::string mpks(mpk.begin(), mpk.end());

     fc::sha256 sequence( fc::sha256::hash( fc::sha256::hash( std::to_string( n ) + ":" + std::to_string ( no ) + ":" + mpks ) ) );
     fc::sha256 secexp( stretched );
     privatekeys.push_back( fc::ecc::private_key::generate_from_seed( secexp, sequence ) );
  }

  void derivekeys_v4( const std::string& passphrase )
  {
     for( auto &account : accounts.items )
     {
        if( account.first == "0" )
        {
           python_dict_type_t types = boost::get<python_dict_type_t>( account.second );

           if( encryption )
           {
              std::string hash = fc::sha256::hash( fc::sha256::hash( passphrase ) );
              std::vector<unsigned char> hashbytes(hash.size() / 2);
              fc::from_hex( hash, (char*)&hashbytes[0], hashbytes.size() );
              std::string decoded_seed = fc::base64_decode( seed );
              std::vector<unsigned char> plaindata(decoded_seed.size() - 16);
              if ( fc::aes_decrypt( (unsigned char*)&decoded_seed[16], decoded_seed.size() - 16,
                                    &hashbytes[0],
                                    (unsigned char*)&decoded_seed[0],
                                    &plaindata[0] ) >= 32 )
              {
                 std::string decrypted_seed(plaindata.begin(), plaindata.begin() + 32);
                 seed = decrypted_seed;
              }
              else
              {
                 FC_THROW_EXCEPTION( bts::wallet::invalid_password, "wrong password" );
              }
           }

           for( auto &type : types.items )
           {
              if( type.first == "0" || type.first == "1" )
              {
                 python_dict_array_type_t pubkeys = boost::get<python_dict_array_type_t>( type.second );
                 for( unsigned int i = 0; i < pubkeys.items.size(); i++ )
                 {
                    addkey_v4 ( (type.first == "0") ? 0 : 1, i );
                 }
              }
           }
           break;
        }
     }
  }

public:
  electrumwallet( std::string path ):encryption(false),seed_version(""),masterkey("")
  {
     std::ifstream in(path, std::ios_base::in);
     std::string storage;
     in.unsetf( std::ios::skipws );
     std::copy( std::istream_iterator<char>(in),
                std::istream_iterator<char>(),
                std::back_inserter(storage) );

     bts::bitcoin::parser<std::string::const_iterator> grammar;
     bts::bitcoin::python_dict_type_t dict;

     std::string::const_iterator iter = storage.begin();
     std::string::const_iterator end = storage.end();
     bool r = phrase_parse( iter, end, grammar, ascii::space, dict );

     if( r && iter == end )
     {
        for( auto &item : dict.items )
        {
           boost::to_lower( item.first );
           if( item.first == "use_encryption" )
           {
              auto useenc = boost::get<std::string>( item.second );
              boost::to_lower(useenc);
              if ( useenc == "true" )
                 encryption = true;
           }
           else if( item.first == "seed_version" )
           {
              seed_version = boost::get<std::string>( item.second );
           }
           else if( item.first == "seed" )
           {
              seed = boost::get<std::string>( item.second );
           }
           else if( item.first == "master_public_key" )
           {
              masterkey = boost::get<std::string>( item.second );
           }
           else if( item.first == "accounts" )
           {
              accounts.items = boost::get<bts::bitcoin::python_dict_type_t>( item.second ).items;
           }
        }
     }
     else
     {
        FC_THROW_EXCEPTION( fc::parse_error_exception, "failed to parse electrum wallet" );
     }
  }

  bool ok()
  {
     return seed_version == "4" && seed != "" && masterkey != "" && accounts.items.size() > 0;
  }

  void derivekeys( const std::string &passphrase )
  {
     if( seed_version == "4" )
     {
        try {
           derivekeys_v4( passphrase );
        }
        catch ( const fc::exception& )
        {
           //wlog( "${e}", ("e",e.to_detail_string()) );
        }
     }
     else
     {
        FC_THROW_EXCEPTION( fc::parse_error_exception, "failed to parse electrum wallet - unsupported seed_version" );
     }
  }

  std::vector<fc::ecc::private_key> keys()
  {
     return privatekeys;
  }
};

std::vector<fc::ecc::private_key> import_electrum_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{ try {
  std::vector<fc::ecc::private_key> keys;
  electrumwallet wallet( wallet_dat.to_native_ansi_path());
  if( !wallet.ok() ) FC_THROW_EXCEPTION( fc::invalid_arg_exception, "invalid electrum wallet");
  wallet.derivekeys( passphrase );
  return wallet.keys();
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

} } // bts::bitcoin
