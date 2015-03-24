#include <bts/db/object_database.hpp>

#include <fc/io/raw.hpp>
#include <fc/container/flat.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace db {

object_database::object_database()
:_undo_db(*this)
{
   _index.resize(255);
   _undo_db.enable();

   _object_id_to_object = std::make_shared<db::level_map<object_id_type,vector<char>>>();
}

object_database::~object_database(){} 

void object_database::close()
{
   flush();

   if( _object_id_to_object->is_open() )
      _object_id_to_object->close();
}

const object* object_database::find_object( object_id_type id )const
{
   return get_index(id.space(),id.type()).find( id );
}
const object& object_database::get_object( object_id_type id )const
{
   return get_index(id.space(),id.type()).get( id );
}

const index& object_database::get_index(uint8_t space_id, uint8_t type_id)const
{
   FC_ASSERT( _index.size() > space_id, "", ("space_id",space_id)("type_id",type_id)("index.size",_index.size()) );
   const auto& tmp = _index[space_id][type_id];
   FC_ASSERT( tmp );
   return *tmp;
}
index& object_database::get_mutable_index(uint8_t space_id, uint8_t type_id)
{
   FC_ASSERT( _index.size() > space_id, "", ("space_id",space_id)("type_id",type_id)("index.size",_index.size()) );
   FC_ASSERT( _index[space_id].size() > type_id , "", ("space_id",space_id)("type_id",type_id)("index[space_id].size",_index[space_id].size()) );
   const auto& idx = _index[space_id][type_id];
   FC_ASSERT( idx, "", ("space",space_id)("type",type_id) );
   return *idx;
}

void object_database::flush()
{
   if( !_object_id_to_object->is_open() )
      return;

   vector<object_id_type> next_ids;
   for( auto& space : _index )
      for( const unique_ptr<index>& type_index : space )
         if( type_index )
         {
            type_index->inspect_all_objects([&] (const object& obj) {
               _object_id_to_object->store(obj.id, obj.pack());
            });
            next_ids.push_back( type_index->get_next_id() );
         }
   _object_id_to_object->store( object_id_type(), fc::raw::pack(next_ids) );
}

void object_database::wipe()
{
   close();
   fc::remove_all(_data_dir / "object_database" / "objects");
}

void object_database::open( const fc::path& data_dir )
{ try {
   ilog("Open object_database in ${d}", ("d", data_dir));

   _object_id_to_object->open( data_dir / "object_database" / "objects" );

   for( auto& space : _index )
   {
      for( auto& type_index : space )
      {
         if( type_index )
         {
            type_index->open( _object_id_to_object );
         }
      }
   }
   try {
   auto next_ids = fc::raw::unpack<vector<object_id_type>>( _object_id_to_object->fetch( object_id_type() ) );
   for( auto id : next_ids )
      get_mutable_index( id ).set_next_id( id );
   }
   catch ( const fc::exception& )
   {
      wlog( "unable to fetch next ids, must be new object_database" );
   }

   _data_dir = data_dir;
} FC_CAPTURE_AND_RETHROW( (data_dir) ) }


void object_database::pop_undo()
{ try {
   _undo_db.pop_commit();
} FC_CAPTURE_AND_RETHROW() }

void object_database::save_undo( const object& obj )
{
   _undo_db.on_modify( obj );
}

void object_database::save_undo_add( const object& obj )
{
   _undo_db.on_create( obj );
}

void object_database::save_undo_remove(const object& obj)
{
   _undo_db.on_remove( obj );
}

} } // namespace bts::db
