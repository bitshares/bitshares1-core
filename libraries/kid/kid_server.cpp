#include <bts/kid/kid_server.hpp>
#include <bts/blockchain/difficulty.hpp>
#include <bts/db/level_map.hpp>
#include <fc/filesystem.hpp>
#include <fc/network/http/server.hpp>
#include <fc/exception/exception.hpp>
#include <fc/thread/thread.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>


namespace bts { namespace kid {
   uint64_t   name_record::difficulty()const
   {
      return 1000 * bts::blockchain::difficulty( digest512() );
   }

   fc::ecc::public_key signed_name_record::get_signee()const
   {
      return fc::ecc::public_key( master_signature, digest() );
   }
   fc::ecc::public_key stored_key::get_signee()const
   {
      FC_ASSERT( encrypted_key.size() > 0 );
      auto digest = fc::sha256::hash( encrypted_key.data(), encrypted_key.size() );
      return fc::ecc::public_key( signature, digest );
   }
   fc::sha256 name_record::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }
   fc::sha256 name_record::digest512()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, *this );
      auto h512 = enc.result();
      return fc::sha256::hash( (char*)&h512, sizeof(h512) );
   }

   fc::sha256 signed_name_record::id()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   fc::sha256 block::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   fc::sha256 signed_block::id()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   void signed_block::sign( const fc::ecc::private_key& trustee_priv_key )
   {
      trustee_signature = trustee_priv_key.sign_compact( id() );
   }

   void signed_block::verify( const fc::ecc::public_key& trustee_pub_key )
   {
      FC_ASSERT( fc::ecc::public_key( trustee_signature, id() ) == trustee_pub_key )
   }

   namespace detail
   {
       class server_impl
       {
          public:
             server*                                                   _self;
             fc::http::server                                          _httpd;
             bts::db::level_map<std::string, history >                 _name_index;
             bts::db::level_map<uint32_t, signed_block>                _block_database;
             bts::db::level_map<std::string,stored_key >               _key_data;
             bts::db::level_map<fc::ripemd160, std::string>            _key_to_name;

             std::unordered_map<std::string, signed_name_record>       _pending;
       
             fc::future<void>                                          _block_gen_loop_complete;
       
             signed_block                                              _current_block;
             fc::sha256                                                _current_block_id;
             fc::ecc::private_key                                      _trustee_key;
       
             void block_generation_loop()
             {
                while( !_block_gen_loop_complete.canceled() )
                {
                   if( _pending.size() && (fc::time_point::now() - _current_block.timestamp) > fc::seconds(60) )
                   {
                      signed_block next_block;
                      next_block.number     = _current_block.number + 1;
                      auto next_diff        = _current_block.difficulty * 500 / _pending.size(); 
                      next_diff             = (_current_block.difficulty * 99 + next_diff) / 100;
                      next_block.difficulty = std::max<uint64_t>(next_diff, 1000 );
                      next_block.timestamp  = fc::time_point::now();
       
                      for( auto rec : _pending )
                      {
                         next_block.records.push_back( rec.second );
                      }
       
                      next_block.sign( _trustee_key );
                      _block_database.store(next_block.number,next_block);
       
                      for( uint32_t rec = 0; rec < next_block.records.size(); ++rec )
                      {
                         auto hist = _self->fetch_history( next_block.records[rec].name );
                         hist.updates.push_back( name_index( next_block.number, rec ) );
                         _name_index.store( next_block.records[rec].name, hist );
                      }
       
                      _current_block = next_block;
                      _current_block_id = _current_block.id();
                   }
                   fc::usleep( fc::seconds( 1 ) );
                }
             }

             void handle_request( const fc::http::request& r, const fc::http::server::response& s )
             {
                 auto pos = r.path.find( "/" );
                 auto first_dir = r.path.substr(0,pos);
                 if( first_dir == "update_record" )
                 {
                    s.set_status( fc::http::reply::RecordCreated );
                 }
                 else if( first_dir == "fetch_by_name" )
                 {
                    s.set_status( fc::http::reply::Found );
                 }
                 else if( first_dir == "fetch_by_key" )
                 {
                    s.set_status( fc::http::reply::Found );
                 }
                 else if( first_dir == "store_key" )
                 {
                    s.set_status( fc::http::reply::RecordCreated );
                 }
                 else if( first_dir == "fetch_key" )
                 {
                    s.set_status( fc::http::reply::Found );
                 }
                 else
                 {
                    s.set_status( fc::http::reply::NotFound );
                 }
             }
       };
   } // namespace detail


   server::server()
   :my( new detail::server_impl() )
   {
      my->_self = this;
      my->_block_gen_loop_complete = fc::async( [=](){ my->block_generation_loop(); } );
   }
   server::~server()
   {
      ilog( "waiting for block generation loop to exit" );
      try {
         my->_block_gen_loop_complete.cancel();
         my->_block_gen_loop_complete.wait();
      } 
      catch ( const fc::canceled_exception& e ){}
      catch ( const fc::exception& e )
      {
         wlog( "${e}", ("e",e.to_detail_string()) );
      }
   }

   void server::set_data_directory( const fc::path& dir )
   {
      my->_name_index.open( dir / "name_index" );
      my->_block_database.open( dir / "block_database" );
      my->_key_data.open( dir / "key_data" );
      my->_key_to_name.open( dir / "key_to_name" );
   }

   void server::listen( const fc::ip::endpoint& ep )
   {
      my->_httpd.on_request( [=]( const fc::http::request& r, const fc::http::server::response& s ){ my->handle_request( r, s ); } );
      my->_httpd.listen(ep);
   }

   bool server::update_record( const signed_name_record& r )
   {
      FC_ASSERT( my->_pending.size() < 20000 );
      FC_ASSERT( fc::trim_and_normalize_spaces( r.name ) == r.name );
      FC_ASSERT( fc::to_lower( r.name ) == r.name );
      FC_ASSERT( r.difficulty() >= my->_current_block.difficulty );
      FC_ASSERT( my->_current_block_id == r.prev_block_id     );

      auto pending_itr = my->_pending.find( r.name );
      FC_ASSERT( pending_itr == my->_pending.end()         );
      auto hist = fetch_history( r.name ); 
      if( hist.updates.size() == 0 )
      {
         my->_pending[r.name] = r;
         return true;
      }

      auto old_record  = fetch_record( hist.updates.back() );
      FC_ASSERT( old_record.master_key == r.get_signee() )
      my->_pending[r.name] = r;
      return true;
   }

   signed_name_record      server::fetch_record( const std::string& name )
   {
      auto pending_itr = my->_pending.find( name );
      if( pending_itr != my->_pending.end() )
         return pending_itr->second;
      auto hist= fetch_history( name ); 
      FC_ASSERT( hist.updates.size() > 0 );

      auto old_block   = fetch_block( hist.updates.back().block_num );
      return fetch_record( hist.updates.back() );
      FC_ASSERT( old_block.records.size() > hist.updates.back().record_num );
      return old_block.records[hist.updates.back().record_num];
   }

   signed_name_record      server::fetch_record( const name_index& index )
   {
      auto old_block   = fetch_block( index.block_num );
      FC_ASSERT( old_block.records.size() > index.record_num );
      return old_block.records[index.record_num];
   }

   history server::fetch_history( const std::string& name )
   {
      history hist;
      auto itr = my->_name_index.find(name);
      if( itr.valid() )
         hist = itr.value();
      return hist;
   }

   void                    server::store_key( const std::string& name, const stored_key& k )
   {
      auto cur_rec = fetch_record( name );
      FC_ASSERT( k.encrypted_key.size() < 1024*16 );
      FC_ASSERT( cur_rec.master_key == k.get_signee()  );
      my->_key_data.store( name, k );
   }

   stored_key       server::fetch_key( const std::string& name )
   {
      return my->_key_data.fetch( name );
   }

   signed_block            server::fetch_block( uint32_t block_num )
   {
      return my->_block_database.fetch( block_num );
   }

} } // namespace bts::kid
