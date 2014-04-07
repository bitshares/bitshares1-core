namespace bts { namespace dns {

  /**
   *  Adds the JSON-RPC methods to the connection.  This method should be overridden to add
   *  additional methods for customized DACs.
   */
  void rpc_server::register_methods( const fc::rpc::json_connection_ptr& con )
  {
     fc::rpc::json_connection* capture_con = con.get();

     con->add_method( "lookup_domain", [=]( const fc::variants& params ) -> fc::variant
     {
         // TODO use last known state even if disconnected?
         FC_ASSERT( get_client()->get_node()->is_connected() );
         FC_ASSERT( params.size() == 1 );

         auto name   = params[1].as_string();
/*
         auto value = fc::raw::unpack<fc::variant>( output.value );
         fc::json::to_pretty_string( value );

         */
         return fc::variant( fc::json::to_pretty_string(name) );
     });


     /**
      * @note: all references to the connection should be via capture_con and not the
      * con pointer passed in.  Otherwise there will be a circular self-reference that will
      * prevent the json connection from ever being freed as the lambda would capture the
      * shared pointer to the connectiona and the connection would store the lambda.
      *
      * We know the scope of the lambda is less than the scope of the connection, so
      * capture_con will always be valid.
      */
     con->add_method( "transfer_domain", [=]( const fc::variants& params ) -> fc::variant
     {
         // some commands require a connection to the network.
         FC_ASSERT( get_client()->get_node()->is_connected() );
         // some commands require the user to be logged in.
         check_login( capture_con );

         // always validate the parameter count prior to indexing into params
         FC_ASSERT( params.size() == 2 );
         auto amount = params[0].as<bts::blockchain::asset>();
         auto addr   = params[1].as_string();
         auto trx    = _client->get_wallet()->transfer( amount, addr );
         get_client()->broadcast_transaction(trx);
         return fc::variant( trx.id() );
     });
  }

} } //bts::dns
