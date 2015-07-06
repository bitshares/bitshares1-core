#include <bts/blockchain/time.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/cli/print_result.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/wallet/url.hpp>

#include <boost/range/algorithm/max_element.hpp>
#include <boost/range/algorithm/min_element.hpp>

namespace bts { namespace cli {

  print_result::print_result()
  {
    initialize();
  }

  void print_result::format_and_print( const string& method_name, const fc::variants& arguments, const fc::variant& result,
                                       cptr client, std::ostream& out )const
  {
    try
    {
      t_command_to_function::const_iterator iter = _command_to_function.find( method_name);

      if( iter != _command_to_function.end())
        (iter->second)(out, arguments, result, client);
      else
      {
        // there was no custom handler for this particular command, see if the return type
        // is one we know how to pretty-print
        string result_type;

        const bts::api::method_data& method_data = client->get_rpc_server()->get_method_data( method_name);
        result_type = method_data.return_type;

        if( result_type == "asset")
        {
          out << string( result.as<bts::blockchain::asset>() ) << "\n";
        }
        else if(result_type == "address")
        {
          out << (string)result.as<bts::blockchain::address>() << "\n";
        }
        else if(result_type == "null" || result_type == "void")
        {
          out << "OK\n";
        }
        else
        {
          out << fc::json::to_pretty_string(result) << "\n";
        }
      }
    }
    catch(const fc::key_not_found_exception&)
    {
      elog(" KEY NOT FOUND ");
      out << "key not found \n";
    }
    catch(const fc::exception& e)
    {
      elog("Exception from pretty printer: ${e}", ("e", e.to_detail_string()));
    }
    catch(...)
    {
      out << "unexpected exception \n";
    }
  }

  void print_result::initialize()
  {
    _command_to_function["help"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      string help_string = result.as<string>();
      out << help_string << "\n";
    };

    _command_to_function["wallet_account_vote_summary"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& votes = result.as<account_vote_summary_type>();
      out << pretty_vote_summary(votes, client);
    };

    _command_to_function["debug_list_errors"] = &f_debug_list_errors;
    _command_to_function["blockchain_get_account_wall"] = &f_blockchain_get_account_wall;

    _command_to_function["get_info"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& info = result.as<variant_object>();
      out << pretty_info(info, client);
    };

    _command_to_function["blockchain_get_info"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& config = result.as<variant_object>();
      out << pretty_blockchain_info(config, client);
    };

    _command_to_function["wallet_get_info"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& info = result.as<variant_object>();
      out << pretty_wallet_info(info, client);
    };

    _command_to_function["wallet_account_transaction_history"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& transactions = result.as<vector<pretty_transaction>>();
      out << pretty_transaction_list( transactions, client );
    };

    _command_to_function["wallet_transaction_history_experimental"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& transactions = result.as<set<pretty_transaction_experimental>>();
      out << pretty_experimental_transaction_list( transactions, client );
    };

    _command_to_function["wallet_market_order_list"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& order_map = result.as<map<order_id_type, market_order>>();

      vector<std::pair<order_id_type, market_order>> order_list;
      order_list.reserve( order_map.size() );
      for( const auto& order_item : order_map )
          order_list.push_back( std::make_pair( order_item.first, order_item.second ) );

      // sort them in a useful order before display
      std::sort( order_list.begin(), order_list.end(),
         []( const std::pair<order_id_type, market_order>& a,
             const std::pair<order_id_type, market_order>& b ) -> bool
         {
             // sort by base_id, then quote_id, then order type, then price
             asset_id_type a_base_id  = a.second.market_index.order_price.base_asset_id;
             asset_id_type a_quote_id = a.second.market_index.order_price.quote_asset_id;
             asset_id_type b_base_id  = b.second.market_index.order_price.base_asset_id;
             asset_id_type b_quote_id = b.second.market_index.order_price.quote_asset_id;

             if( a_base_id < b_base_id )
                 return true;
             if( a_base_id > b_base_id )
                 return false;
             FC_ASSERT( a_base_id == b_base_id );          // trichotomy

             if( a_quote_id < b_quote_id )
                 return true;
             if( a_quote_id > b_quote_id )
                 return false;
             FC_ASSERT( a_quote_id == b_quote_id );        // trichotomy

             if( a.second.type < b.second.type )
                 return true;
             if( a.second.type > b.second.type )
                 return false;
             FC_ASSERT( a.second.type == b.second.type );  // trichotomy
             return (a.second.market_index < b.second.market_index);
         }
         );

      out << pretty_order_list( order_list, client );
    };

    _command_to_function["blockchain_market_list_asks"] = &f_blockchain_market_list;
    _command_to_function["blockchain_market_list_bids"] = &f_blockchain_market_list;
    _command_to_function["blockchain_market_list_shorts"] = &f_blockchain_market_short_list;

    _command_to_function["wallet_list_accounts"] = &f_wallet_list_accounts;

    _command_to_function["wallet_account_balance"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& balances = result.as<account_balance_summary_type>();
      out << pretty_balances( balances, client );
    };

    _command_to_function["wallet_account_yield"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& yield = result.as<account_balance_summary_type>();
      out << pretty_balances( yield, client );
    };

    _command_to_function["wallet_account_historic_balance"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& balances = result.as<account_balance_summary_type>();
      out << pretty_balances( balances, client );
    };

    _command_to_function["wallet_burn"]                             = &f_wallet_transfer;
    _command_to_function["wallet_account_register"]                 = &f_wallet_transfer;
    _command_to_function["wallet_account_retract"]                  = &f_wallet_transfer;
    _command_to_function["wallet_account_update_active_key"]        = &f_wallet_transfer;
    _command_to_function["wallet_account_update_registration"]      = &f_wallet_transfer;
    _command_to_function["wallet_collect_genesis_balances"]         = &f_wallet_transfer;
    _command_to_function["wallet_collect_vested_balances"]          = &f_wallet_transfer;
    _command_to_function["wallet_delegate_withdraw_pay"]            = &f_wallet_transfer;
    _command_to_function["wallet_delegate_update_signing_key"]      = &f_wallet_transfer;
    _command_to_function["wallet_get_transaction"]                  = &f_wallet_transfer;
    _command_to_function["wallet_market_add_collateral"]            = &f_wallet_transfer;
    _command_to_function["wallet_market_batch_update"]              = &f_wallet_transfer;
    _command_to_function["wallet_market_cancel_order"]              = &f_wallet_transfer;
    _command_to_function["wallet_market_cancel_orders"]             = &f_wallet_transfer;
    _command_to_function["wallet_market_cover"]                     = &f_wallet_transfer;
    _command_to_function["wallet_market_submit_ask"]                = &f_wallet_transfer;
    _command_to_function["wallet_market_submit_bid"]                = &f_wallet_transfer;
    _command_to_function["wallet_market_submit_short"]              = &f_wallet_transfer;
    _command_to_function["wallet_mia_create"]                       = &f_wallet_transfer;
    _command_to_function["wallet_multisig_deposit"]                 = &f_wallet_transfer;
    _command_to_function["wallet_publish_feeds"]                    = &f_wallet_transfer;
    _command_to_function["wallet_publish_price_feed"]               = &f_wallet_transfer;
    _command_to_function["wallet_publish_slate"]                    = &f_wallet_transfer;
    _command_to_function["wallet_publish_version"]                  = &f_wallet_transfer;
    _command_to_function["wallet_recover_titan_deposit_info"]       = &f_wallet_transfer;
    _command_to_function["wallet_release_escrow"]                   = &f_wallet_transfer;
    _command_to_function["wallet_scan_transaction"]                 = &f_wallet_transfer;
    _command_to_function["wallet_transfer"]                         = &f_wallet_transfer;
    _command_to_function["wallet_transfer_from_with_escrow"]        = &f_wallet_transfer;
    _command_to_function["wallet_uia_create"]                       = &f_wallet_transfer;
    _command_to_function["wallet_uia_collect_fees"]                 = &f_wallet_transfer;
    _command_to_function["wallet_uia_issue"]                        = &f_wallet_transfer;
    _command_to_function["wallet_uia_issue_to_addresses"]           = &f_wallet_transfer;
    _command_to_function["wallet_uia_retract_balance"]              = &f_wallet_transfer;
    _command_to_function["wallet_uia_update_active_flags"]          = &f_wallet_transfer;
    _command_to_function["wallet_uia_update_authority_permissions"] = &f_wallet_transfer;
    _command_to_function["wallet_uia_update_description"]           = &f_wallet_transfer;
    _command_to_function["wallet_uia_update_fees"]                  = &f_wallet_transfer;
    _command_to_function["wallet_uia_update_supply"]                = &f_wallet_transfer;
    _command_to_function["wallet_uia_update_whitelist"]             = &f_wallet_transfer;

    _command_to_function["wallet_list"] = &f_wallet_list;

    _command_to_function["network_get_usage_stats"] = &f_network_get_usage_stats;

    _command_to_function["blockchain_list_delegates"] = &f_blockchain_list_delegates;
    _command_to_function["blockchain_list_active_delegates"] = &f_blockchain_list_delegates;

    _command_to_function["blockchain_list_blocks"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& block_records = result.as<vector<block_record>>();
      out << pretty_block_list(block_records, client);
    };

    _command_to_function["blockchain_list_accounts"] = &f_blockchain_list_accounts;

    _command_to_function["blockchain_list_assets"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << pretty_asset_list(result.as<vector<asset_record>>(), client);
    };

    _command_to_function["blockchain_get_account"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << pretty_account(result.as<oaccount_record>(), client);
    };

    _command_to_function["blockchain_list_forks"] = &f_blockchain_list_forks;

    _command_to_function["blockchain_list_pending_transactions"] = &f_blockchain_list_pending_transactions;

    _command_to_function["blockchain_market_order_book"] = &f_blockchain_market_order_book;

    _command_to_function["blockchain_market_order_history"] = &f_blockchain_market_order_history;

    _command_to_function["blockchain_market_price_history"] = &f_blockchain_market_price_history;

    _command_to_function["blockchain_unclaimed_genesis"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << client->get_chain()->to_pretty_asset( result.as<asset>() ) << "\n";
    };

    _command_to_function["blockchain_calculate_supply"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << client->get_chain()->to_pretty_asset( result.as<asset>() ) << "\n";
    };

    _command_to_function["blockchain_calculate_debt"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << client->get_chain()->to_pretty_asset( result.as<asset>() ) << "\n";
    };

    _command_to_function["blockchain_calculate_max_supply"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << client->get_chain()->to_pretty_asset( result.as<asset>() ) << "\n";
    };

    _command_to_function["network_list_potential_peers"] = &f_network_list_potential_peers;

    _command_to_function["wallet_set_transaction_fee"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << client->get_chain()->to_pretty_asset( result.as<asset>() ) << "\n";
    };

    _command_to_function["wallet_get_transaction_fee"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      out << client->get_chain()->to_pretty_asset( result.as<asset>() ) << "\n";
    };

    _command_to_function["disk_usage"] = []( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
    {
      const auto& usage = result.as<fc::mutable_variant_object>();
      out << pretty_disk_usage( usage );
    };

    _command_to_function["blockchain_get_block"] = &f_blockchain_get_block;
  }

  void print_result::f_blockchain_get_account_wall( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
     out << std::left;
     out << std::setw( 30 )  << "AMOUNT";
     out << std::setw( 100 ) << "MESSAGE";
     out << std::setw( 30 )  << "SIGNER";
     out << "\n";
     out << pretty_line( 160 );
     out << "\n";
     auto burn_records = result.as<vector<burn_record>>();
     for( auto record : burn_records )
     {
        out << std::left;

        if( record.index.account_id >= 0 )
            out << std::setw( 30 )  << client->get_chain()->to_pretty_asset( record.amount );
        else
            out << std::setw( 30 )  << client->get_chain()->to_pretty_asset( -record.amount );

        out << std::setw( 100 ) << record.message;
        if( record.signer )
        {
            auto signer_key = record.signer_key();
            auto oaccount_rec = client->get_chain()->get_account_record( signer_key );
            if( oaccount_rec )
               out << std::setw( 30 )  << oaccount_rec->name;
            else
               out << std::setw( 30 )  << variant(public_key_type(signer_key)).as_string();
        }
        else
        {
           out << std::setw( 30 )  << "ANONYMOUS";
        }
        out << "\n";
     }
  }

  void print_result::f_debug_list_errors(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    auto error_map = result.as<map<fc::time_point, fc::exception> >();
    if(error_map.empty())
      out << "No errors.\n";
    else
      for(const auto& item : error_map)
      {
      (out) << string(item.first) << " (" << bts::blockchain::get_approximate_relative_time_string(item.first) << " )\n";
      (out) << item.second.to_detail_string();
      (out) << "\n";
      }
  }

  void print_result::f_blockchain_market_short_list(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    const auto& market_orders = result.as<vector<market_order>>();

    const string& quote_symbol = arguments[ 0 ].as_string();
    const oasset_record quote_asset_record = client->get_chain()->get_asset_record( quote_symbol );
    FC_ASSERT( quote_asset_record.valid() );
    const asset_id_type quote_id = quote_asset_record->id;

    const ostatus_record status = client->get_chain()->get_status_record( status_index{ quote_id, asset_id_type( 0 ) } );

    out << std::left;
    out << std::setw( 30 ) << "AMOUNT";
    out << std::setw( 30 ) << "COLLATERAL";
    out << std::setw( 30 ) << "INTEREST RATE";
    out << std::setw( 30 ) << "PRICE LIMIT";
    out << std::setw( 40 ) << "ID";
    out << "\n";
    out << pretty_line( 128 );
    out << "\n";

    for( const auto& order : market_orders )
    {
       out << std::setw( 30 );
       if( status.valid() && status->current_feed_price.valid() )
           out << client->get_chain()->to_pretty_asset( (order.get_balance() * *status->current_feed_price) / 2 );
       else if( status.valid() && status->last_valid_feed_price.valid() )
           out << client->get_chain()->to_pretty_asset( (order.get_balance() * *status->last_valid_feed_price) / 2 );
       else
           out << "N/A";
       out << std::setw( 30 ) << client->get_chain()->to_pretty_asset( order.get_balance() );
       out << std::setw( 30 ) << std::to_string(100 * atof(order.interest_rate->ratio_string().c_str())) + " %";
       if( order.state.limit_price.valid() )
          out << std::setw( 30 ) << client->get_chain()->to_pretty_price( *order.state.limit_price );
       else
          out << std::setw( 30 ) << "NONE";
       out << std::setw( 40 ) << variant(order.get_id()).as_string();
       out << "\n";
    }
    out << "\n";
  }

  void print_result::f_blockchain_market_list(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    const auto& order_map = result.as<vector<market_order>>();

    vector<std::pair<order_id_type, market_order>> order_list;
    order_list.reserve( order_map.size() );
    for( const auto& order_item : order_map )
        order_list.push_back( std::make_pair( order_item.get_id(), order_item ) );

    out << pretty_order_list( order_list, client );
  }

  void print_result::f_wallet_list_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    auto accts = result.as<vector<wallet_account_record>>();

    out << std::setw(35) << std::left << "NAME (* delegate)";
    out << std::setw(64) << "KEY";
    out << std::setw(22) << "REGISTERED";
    out << std::setw(25) << "BLOCK PRODUCTION ENABLED";
    out << "\n";

    for(const auto& acct : accts)
    {
      if(acct.is_delegate())
      {
        out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
      }
      else
      {
        out << std::setw(35) << pretty_shorten(acct.name, 34);
      }

      out << std::setw(64) << string(acct.active_key());

      if(acct.id == 0)
      {
        out << std::setw(22) << "NO";
      }
      else
      {
        out << std::setw(22) << pretty_timestamp(acct.registration_date);
      }

      if(acct.is_delegate())
        out << std::setw(25) << (acct.block_production_enabled ? "YES" : "NO");
      else
        out << std::setw(25) << "N/A";
      out << "\n";
    }
  }

  void print_result::f_wallet_transfer(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    const auto& record = result.as<wallet_transaction_record>();
    const auto& pretty = client->get_wallet()->to_pretty_trx(record);
    const std::vector<pretty_transaction> transactions = { pretty };
    out << pretty_transaction_list(transactions, client);
  }

  void print_result::f_wallet_list(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    auto wallets = result.as<vector<string>>();
    if(wallets.empty())
      out << "No wallets found.\n";
    else
      for(const auto& wallet : wallets)
        out << wallet << "\n";
  }

  void print_result::f_network_get_usage_stats(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    auto stats = result.get_object();

    std::vector<uint32_t> usage_by_second = stats["usage_by_second"].as<std::vector<uint32_t> >();
    if(!usage_by_second.empty())
    {
      out << "last minute:\n";
      print_network_usage_graph(out, usage_by_second);
      out << "\n";
    }
    std::vector<uint32_t> usage_by_minute = stats["usage_by_minute"].as<std::vector<uint32_t> >();
    if(!usage_by_minute.empty())
    {
      out << "last hour:\n";
      print_network_usage_graph(out, usage_by_minute);
      out << "\n";
    }
    std::vector<uint32_t> usage_by_hour = stats["usage_by_hour"].as<std::vector<uint32_t> >();
    if(!usage_by_hour.empty())
    {
      out << "by hour:\n";
      print_network_usage_graph(out, usage_by_hour);
      out << "\n";
    }
  }

  void print_result::print_network_usage_graph(std::ostream& out, const std::vector<uint32_t>& usage_data)
  {
    uint32_t max_value = *boost::max_element(usage_data);
    uint32_t min_value = *boost::min_element(usage_data);
    const unsigned num_rows = 10;
    for(unsigned row = 0; row < num_rows; ++row)
    {
      uint32_t threshold = min_value + (max_value - min_value) / (num_rows - 1) * (num_rows - row - 1);
      for(unsigned column = 0; column < usage_data.size(); ++column)
        out << (usage_data[column] >= threshold ? "*" : " ");
      out << " " << threshold << " bytes/sec\n";
    }
    out << "\n";
  }

  void print_result::f_blockchain_list_delegates(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    const auto& delegate_records = result.as<vector<account_record>>();
    out << pretty_delegate_list(delegate_records, client);
  }

  void print_result::f_blockchain_list_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    string start = "";
    int32_t count = 25; // In CLI this is a more sane default
    if(arguments.size() > 0)
      start = arguments[0].as_string();
    if(arguments.size() > 1)
      count = arguments[1].as<int32_t>();
    print_registered_account_list(out, result.as<vector<account_record>>(), count, client);
  }

  void print_result::print_registered_account_list(std::ostream& out, const vector<account_record>& account_records, int32_t count, cptr client)
  {
    out << std::setw(35) << std::left << "NAME (* delegate)";
    out << std::setw(64) << "KEY";
    out << std::setw(22) << "REGISTERED";
    out << std::setw(15) << "VOTES FOR";

    out << '\n';
    for(int i = 0; i < 151; ++i)
      out << '-';
    out << '\n';

    int32_t counter = 0;
    for(const auto& acct : account_records)
    {
      if(acct.is_delegate())
      {
        out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
      }
      else
      {
        out << std::setw(35) << pretty_shorten(acct.name, 34);
      }

      out << std::setw(64) << string(acct.active_key());
      out << std::setw(22) << pretty_timestamp(acct.registration_date);

      if(acct.is_delegate())
      {
        out << std::setw(15) << acct.delegate_info->votes_for;
      }
      else
      {
        out << std::setw(15) << "N/A";
      }

      out << "\n";

      /* Count is positive b/c CLI overrides default -1 arg */
      if(counter >= count)
      {
        out << "... Use \"blockchain_list_accounts <start_name> <count>\" to see more.\n";
        return;
      }
      counter++;
    }
  }

  void print_result::f_blockchain_list_forks(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    std::map<uint32_t, std::vector<fork_record>> forks = result.as<std::map<uint32_t, std::vector<fork_record>>>();
    std::map<block_id_type, std::string> invalid_reasons; //Your reasons are invalid.

    if(forks.empty())
      out << "No forks.\n";
    else
    {
      out << std::setw(15) << "FORKED BLOCK"
        << std::setw(30) << "FORKING BLOCK ID"
        << std::setw(30) << "SIGNING DELEGATE"
        << std::setw(15) << "TXN COUNT"
        << std::setw(10) << "SIZE"
        << std::setw(20) << "TIMESTAMP"
        << std::setw(10) << "LATENCY"
        << std::setw(8) << "VALID"
        << std::setw(20) << "IN CURRENT CHAIN"
        << "\n" << std::string(158, '-') << "\n";

      for(const auto& fork : forks)
      {
        out << std::setw(15) << fork.first << "\n";

        for(const auto& tine : fork.second)
        {
          out << std::setw(45) << fc::variant(tine.block_id).as_string();

          auto delegate_record = client->get_chain()->get_account_record(tine.signing_delegate);
          if(delegate_record.valid() && delegate_record->name.size() < 29)
            out << std::setw(30) << delegate_record->name;
          else if(tine.signing_delegate > 0)
            out << std::setw(30) << std::string("Delegate ID ") + fc::variant(tine.signing_delegate).as_string();
          else
            out << std::setw(30) << "Unknown Delegate";

          out << std::setw(15) << tine.transaction_count
            << std::setw(10) << tine.size
            << std::setw(20) << pretty_timestamp(tine.timestamp)
            << std::setw(10) << tine.latency.to_seconds()
            << std::setw(8);

          if(tine.is_valid.valid()) {
            if(*tine.is_valid) {
              out << "YES";
            }
            else {
              out << "NO";
              if(tine.invalid_reason.valid())
                invalid_reasons[tine.block_id] = tine.invalid_reason->to_detail_string();
              else
                invalid_reasons[tine.block_id] = "No reason given.";
            }
          }
          else
            out << "N/A";

          out << std::setw(20);
          if(tine.is_current_fork)
            out << "YES";
          else
            out << "NO";

          out << "\n";
        }
      }

      if(invalid_reasons.size() > 0) {
        out << "REASONS FOR INVALID BLOCKS\n";

        for(const auto& excuse : invalid_reasons)
          out << fc::variant(excuse.first).as_string() << ": " << excuse.second << "\n";
      }
    }
  }

  void print_result::f_blockchain_list_pending_transactions(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    auto transactions = result.as<vector<signed_transaction>>();

    if(transactions.empty())
    {
      out << "No pending transactions.\n";
    }
    else
    {
      out << std::setw(15) << "TXN ID"
         << std::setw(20) << "EXPIRES"
        << std::setw(10) << "SIZE"
        << std::setw(25) << "OPERATION COUNT"
        << std::setw(25) << "SIGNATURE COUNT"
        << "\n" << std::string(100, '-') << "\n";

      for(const auto& transaction : transactions)
      {
        if(FILTER_OUTPUT_FOR_TESTS)
        {
          out << std::setw(15) << "<d-ign>" << string( transaction.id() ).substr( 0, 8 ) << "</d-ign>";
          out << std::setw(20) << "<d-ign>" << string( transaction.expiration ) << "</d-ign>";
        }
        else
        {
          out << std::setw(15) << string( transaction.id() ).substr( 0, 8 );
          out << std::setw(20) << string( transaction.expiration );
        }

        out << std::setw(10) << transaction.data_size()
          << std::setw(25) << transaction.operations.size()
          << std::setw(25) << transaction.signatures.size()
          << "\n";
      }
    }
  }

  void print_result::f_blockchain_market_order_book(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  { try {
    auto bids_asks = result.as<std::pair<vector<market_order>, vector<market_order>>>();

    out << std::string(29, ' ') << "BIDS (* Short)"
      << std::string(33, ' ') << " | "
      << std::string(34, ' ') << "ASKS"
      << std::string(34, ' ') << "\n"
      << std::left << std::setw(26) << "TOTAL"
      << std::setw(20) << "QUANTITY"
      << std::right << std::setw(30) << "PRICE"
      << " | " << std::left << std::setw(30) << "PRICE" << std::right << std::setw(23) << "QUANTITY" << std::setw(26) << "TOTAL" << "   COLLATERAL" << "\n"
      << std::string(175, '-') << "\n";

    const string& quote_symbol = arguments[ 0 ].as_string();
    const string& base_symbol = arguments[ 1 ].as_string();

    const oasset_record quote_asset_record = client->get_chain()->get_asset_record( quote_symbol );
    FC_ASSERT( quote_asset_record.valid() );
    const oasset_record base_asset_record = client->get_chain()->get_asset_record( base_symbol );
    FC_ASSERT( base_asset_record.valid() );

    const asset_id_type quote_id = quote_asset_record->id;
    const asset_id_type base_id = base_asset_record->id;

    vector<market_order>::iterator bid_itr = bids_asks.first.begin();
    auto ask_itr = bids_asks.second.begin();

    price feed_price;
    {
        const ostatus_record status = client->get_chain()->get_status_record( status_index{ quote_id, base_id } );
        if( status.valid() && status->last_valid_feed_price.valid() )
            feed_price = *status->last_valid_feed_price;
    }

    vector<market_order> shorts;
    if( base_id == 0 )
    {
       if (arguments.size() <= 2)
           shorts = client->blockchain_market_list_shorts(arguments[0].as_string());
       else
           shorts = client->blockchain_market_list_shorts(arguments[0].as_string(), arguments[2].as_int64());
    }

    auto status = client->get_chain()->get_status_record( status_index{ quote_id, base_id } );
    price short_execution_price( 0, quote_id, base_id );
    bool short_execution_price_valid = (status.valid() && status->current_feed_price.valid());
    // we don't declare short_execution_price as oprice here
    //    because we still want to print the short wall
    //    with a price of zero when the price is invalid

    if( short_execution_price_valid )
        short_execution_price = *status->current_feed_price;
    //share_type total_short_shares = 0;

    std::copy_if(shorts.begin(), shorts.end(), std::back_inserter(bids_asks.first), [&short_execution_price](const market_order& order) -> bool {
        if( order.state.limit_price && *order.state.limit_price < short_execution_price )
          return true;
        return false;
    });




    if( bids_asks.first.empty() && bids_asks.second.empty() && shorts.empty() )
    {
      out << "No Orders\n";
      return;
    }

    if (base_id == 0 && quote_asset_record->is_market_issued())
    {
      market_order short_wall;
      short_wall.type = blockchain::short_order;
      short_wall.state.balance = 0;
      short_wall.market_index.order_price = short_execution_price;
      short_wall.state.limit_price = short_execution_price;
      for (auto order : shorts)
      {
         if( !order.state.limit_price || *order.state.limit_price > short_execution_price )
         {
           short_wall.state.balance += order.state.balance; //order.balance;//((order.get_quantity() * short_execution_price)).amount;
           wlog( "add short order ${o} to wall ", ("o",order));
         }
         else
         {
            wlog( "filter order ${o} from wall because of limit", ("o",order));
            // short_wall.state.balance += 1; //order.state.balance; //(order.get_quantity() * order.get_price()).amount;
         }
      }

      auto pos = std::lower_bound(bids_asks.first.begin(), bids_asks.first.end(), short_wall, [](const market_order& a, const market_order& b) -> bool {
        return !(a.market_index == b.market_index) && !(a.market_index < b.market_index);
      });
      if (short_wall.state.balance != 0)
        bids_asks.first.insert(pos == bids_asks.first.end() ? bids_asks.first.begin() : pos, short_wall);
      wlog( "Short Wall ${w} ", ("w", short_wall) );
    }

    wdump( (shorts) );
    shorts.erase(std::remove_if(shorts.begin(), shorts.end(), [&short_execution_price](const market_order& short_order) -> bool {
      //Remove if the short execution price is past the price limit
      return short_order.state.limit_price &&  *short_order.state.limit_price < short_execution_price;
    }), shorts.end());
    wdump( (shorts) );


    std::sort( bids_asks.first.begin(), bids_asks.first.end(), [=]( const market_order& a, const market_order& b ) -> bool
               {
                  return a.get_price( feed_price ) > b.get_price( feed_price );
               }
             );
    std::sort( bids_asks.second.begin(), bids_asks.second.end(), [=]( const market_order& a, const market_order& b ) -> bool
               {
                  return a.get_price( feed_price ) < b.get_price( feed_price );
               }
             );

    //bid_itr may be invalidated by now... reset it.
    bid_itr = bids_asks.first.begin();

    while(bid_itr != bids_asks.first.end() || ask_itr != bids_asks.second.end())
    {
      if(bid_itr != bids_asks.first.end())
      {
        //bool short_wall = (bid_itr->get_owner() == address());
        bool is_short_order = bid_itr->type == short_order;

        if (is_short_order)
        {
          //asset quantity(bid_itr->get_quote_quantity() * (*bid_itr->state.limit_price));
          asset quantity = bid_itr->get_balance();
          quantity.amount /= 2; // 2x collateral
          FC_ASSERT( bid_itr->get_limit_price() );
          asset usd_quantity = quantity * *bid_itr->get_limit_price();
          out << std::left << std::setw(26) << client->get_chain()->to_pretty_asset( usd_quantity ) //bid_itr->get_quote_quantity( feed_price ))
              << std::setw(20) << client->get_chain()->to_pretty_asset(quantity)
              << std::right << std::setw(30) << (client->get_chain()->to_pretty_price( *bid_itr->state.limit_price, false ) + " " + quote_asset_record->symbol)
              << "*";
        }
        else
        {
             out << std::left << std::setw(26) << client->get_chain()->to_pretty_asset(bid_itr->get_balance())
                 << std::setw(20) << client->get_chain()->to_pretty_asset(bid_itr->get_quantity( feed_price ))
                 << std::right << std::setw(30) <<
                     (client->get_chain()->to_pretty_price( bid_itr->get_price( feed_price ), false ) + " " + quote_asset_record->symbol);
             if(is_short_order)
               out << "*";
             else
               out << " ";
        }
        ++bid_itr;
      }
      else
        out << std::string(77, ' ');

      out << "|";

      while(ask_itr != bids_asks.second.end())
      {
        if(!ask_itr->collateral)
        {
          auto abs_price =  ask_itr->get_price( feed_price );
          out << " ";
          out << std::left << std::setw(30) << (client->get_chain()->to_pretty_price( abs_price, false ) + " " + quote_asset_record->symbol)
   //         << std::left << std::setw(30) << (fc::to_string(client->get_chain()->to_pretty_price_double(ask_itr->get_price(feed_price))) + " " + quote_asset_record->symbol)
    //        << std::left << std::setw(30) << (fc::to_string(client->get_chain()->to_pretty_price_double(feed_price)) + " " + quote_asset_record->symbol)
            << std::right << std::setw(23) << client->get_chain()->to_pretty_asset(ask_itr->get_quantity( feed_price ))
            << std::right << std::setw(26) << client->get_chain()->to_pretty_asset(ask_itr->get_quote_quantity( feed_price ));
          ++ask_itr;
          break;
        }
        ++ask_itr;
      }
      out << "\n";
    }

    if(quote_asset_record->is_market_issued() && base_id == 0)
    {
      out << std::string(175, '-') << "\n";
      out << std::setw(39) << "SHORT WALL"
        << std::string(38, ' ') << "| "
        << std::string(34, ' ') << "MARGIN"
        << std::string(34, ' ') << "\n"
        << std::left << std::setw(26) << "TOTAL"
        << std::setw(20) << "QUANTITY"
        << std::right << std::setw(30) << "INTEREST RATE (APR)"
        << " | " << std::left << std::setw(30) << "CALL PRICE" << std::right << std::setw(23) << "QUANTITY" << std::setw(26) << "TOTAL" << "   COLLATERAL    EXPIRES" << "\n"
        << std::string(175, '-') << "\n";

      {
        const ostatus_record status = client->get_chain()->get_status_record( status_index{ quote_asset_record->id, asset_id_type( 0 ) } );

        auto ask_itr = bids_asks.second.rbegin();
        auto bid_itr = shorts.begin();
        while( ask_itr != bids_asks.second.rend() || bid_itr != shorts.end() )
        {
          if(bid_itr != shorts.end())
          {
              const auto& order = *bid_itr;
              auto collateral = order.get_balance();
              collateral.amount /= 2;

              out << std::left << std::setw( 26 );
              if( status.valid() && status->current_feed_price.valid() )
                  out << client->get_chain()->to_pretty_asset( collateral * *status->current_feed_price );
              else if( status.valid() && status->last_valid_feed_price.valid() )
                  out << client->get_chain()->to_pretty_asset( collateral * *status->last_valid_feed_price );
              else
                  out << "N/A";

              out << std::setw( 20 ) << client->get_chain()->to_pretty_asset( collateral );

              out << std::right << std::setw( 30 ) << std::to_string(100 * atof(order.interest_rate->ratio_string().c_str())) + " %";

              ++bid_itr;
          }
          else
          {
              out << string(76, ' ');
          }

          out << " | ";

          while(ask_itr != bids_asks.second.rend() && !ask_itr->collateral)
            ++ask_itr;
          if(ask_itr != bids_asks.second.rend())
          {
            out << std::left << std::setw(30) << std::setprecision(8) << (client->get_chain()->to_pretty_price( ask_itr->get_price( feed_price ), false ) + " " + quote_asset_record->symbol)
              << std::right << std::setw(23) << client->get_chain()->to_pretty_asset(ask_itr->get_quantity())
              << std::right << std::setw(26) << client->get_chain()->to_pretty_asset(ask_itr->get_quote_quantity());
            if(FILTER_OUTPUT_FOR_TESTS)
            {
                  out << std::right << std::setw(26) << fc::string( *ask_itr->expiration );
            }
            else
            {
                  out << std::right << std::setw(26) << bts::blockchain::get_approximate_relative_time_string( *ask_itr->expiration );
                  //
                  // if you want to see times as seconds since epoch (for debug purposes), comment preceding line and uncomment the following line
                  //
                  // out << std::right << std::setw(26) << ask_itr->expiration->sec_since_epoch();
            }
            out << "   " << client->get_chain()->to_pretty_asset(asset(*ask_itr->collateral));
            out << "\n";
          }
          else
            out << "\n";

          if(ask_itr != bids_asks.second.rend())
            ++ask_itr;
        }
      }

      auto status = client->get_chain()->get_status_record( status_index{ quote_id, base_id } );
      if(status)
      {
        if( ! short_execution_price_valid )
        {
          if( status->last_error && (status->last_error->code() == bts::blockchain::insufficient_feeds::code_value) )
            // this case is when launching a new BitAsset, the whole
            // market is shut down until there are adequate feeds
            out << "Warning: Market not open until feeds published   ";
          else
            // this case is when too many feeds have expired on
            // a BitAsset that has already been launched; bid/ask still
            // works but feed-dependent orders won't be processed until
            // we have a feed
            out << "Warning: Feeds expired, only bid+ask operate     ";
        }
        else
        {
          out << "Maximum Short Price: "
              << client->get_chain()->to_pretty_price(short_execution_price)
              << "     ";
        }

        if(status->last_error)
        {
          out << "Last Error:  ";
          out << status->last_error->to_string() << "\n";
          if(true || status->last_error->code() != 37005 /* insufficient funds */)
          {
            out << "Details:\n";
            out << status->last_error->to_detail_string() << "\n";
          }
        } else {
          out << "\n";
        }
      }

      // TODO: print insurance fund for market issued assets

    } // end call section that only applies to market issued assets vs XTS
    else
    {
      auto status = client->get_chain()->get_status_record( status_index{ quote_id, base_id } );
      if(status->last_error)
      {
        out << "Last Error:  ";
        out << status->last_error->to_string() << "\n";
        if(true || status->last_error->code() != 37005 /* insufficient funds */)
        {
          out << "Details:\n";
          out << status->last_error->to_detail_string() << "\n";
        }
      }
    }
  } FC_CAPTURE_AND_RETHROW( (arguments)(result) ) }

  void print_result::f_blockchain_market_order_history(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    vector<order_history_record> orders = result.as<vector<order_history_record>>();
    if(orders.empty())
    {
      out << "No Orders.\n";
      return;
    }

    out << std::setw(7) << "TYPE"
      << std::setw(30) << "PRICE"
      << std::setw(25) << "PAID"
      << std::setw(25) << "RECEIVED"
      << std::setw(20) << "FEES"
      << std::setw(30) << "TIMESTAMP"
      << "   OWNER"
      << "\n" << std::string(167, '-') << "\n";

    for(order_history_record order : orders)
    {
      out << std::setw(7) << "Buy  "
        << std::setw(30) << client->get_chain()->to_pretty_price(order.bid_index.order_price)
        << std::setw(25) << client->get_chain()->to_pretty_asset(order.bid_paid)
        << std::setw(25) << client->get_chain()->to_pretty_asset(order.bid_received)
        << std::setw(20) << client->get_chain()->to_pretty_asset(order.bid_paid - order.ask_received)
        << std::setw(23) << pretty_timestamp(order.timestamp)
        << std::setw(37) << string(order.bid_index.owner)
        << "\n"
        << std::setw(7) << "Sell  "
        << std::setw(30) << client->get_chain()->to_pretty_price(order.ask_index.order_price)
        << std::setw(25) << client->get_chain()->to_pretty_asset(order.ask_paid)
        << std::setw(25) << client->get_chain()->to_pretty_asset(order.ask_received)
        << std::setw(20) << client->get_chain()->to_pretty_asset(order.ask_paid - order.bid_received)
        << std::setw(30) << pretty_timestamp(order.timestamp)
        << "   " << string(order.ask_index.owner)
        << "\n";
    }
  }

  void print_result::f_blockchain_market_price_history(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    market_history_points points = result.as<market_history_points>();
    if(points.empty())
    {
      out << "No price history.\n";
      return;
    }

    const string& quote_symbol = arguments[ 0 ].as_string();
    const oasset_record quote_asset_record = client->get_chain()->get_asset_record( quote_symbol );
    FC_ASSERT( quote_asset_record.valid() );
    const asset_id_type quote_id = quote_asset_record->id;

    const string& base_symbol = arguments[ 1 ].as_string();
    const oasset_record base_asset_record = client->get_chain()->get_asset_record( base_symbol );
    FC_ASSERT( base_asset_record.valid() );
    const asset_id_type base_id = base_asset_record->id;

    out << std::setw(20) << "TIME"
      << std::setw(20) << "HIGHEST BID"
      << std::setw(20) << "LOWEST ASK"
      << std::setw(20) << "OPENING PRICE"
      << std::setw(20) << "CLOSING PRICE"
      << std::setw(30) << "BASE VOLUME"
      << std::setw(30) << "QUOTE VOLUME"
      << "\n" << std::string(160, '-') << "\n";

    for(const auto& point : points)
    {
      out << std::setw(20) << pretty_timestamp(point.timestamp)
        << std::setw(20) << point.highest_bid
        << std::setw(20) << point.lowest_ask
        << std::setw(20) << point.opening_price
        << std::setw(20) << point.closing_price
        << std::setw(30) << client->get_chain()->to_pretty_asset(asset(point.base_volume, base_id))
        << std::setw(30) << client->get_chain()->to_pretty_asset(asset(point.quote_volume, quote_id));
      out << "\n";
    }
  }

  void print_result::f_network_list_potential_peers(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client )
  {
    auto peers = result.as<std::vector<net::potential_peer_record>>();
    out << std::setw(25) << "ENDPOINT";
    out << std::setw(25) << "LAST SEEN";
    out << std::setw(25) << "LAST CONNECT ATTEMPT";
    out << std::setw(30) << "SUCCESSFUL CONNECT ATTEMPTS";
    out << std::setw(30) << "FAILED CONNECT ATTEMPTS";
    out << std::setw(35) << "LAST CONNECTION DISPOSITION";
    out << std::setw(30) << "LAST ERROR";

    out << "\n";
    for(const auto& peer : peers)
    {
      out << std::setw(25) << string(peer.endpoint);
      out << std::setw(25) << pretty_timestamp(peer.last_seen_time);
      out << std::setw(25) << pretty_timestamp(peer.last_connection_attempt_time);
      out << std::setw(30) << peer.number_of_successful_connection_attempts;
      out << std::setw(30) << peer.number_of_failed_connection_attempts;
      out << std::setw(35) << string(peer.last_connection_disposition);
      out << std::setw(30) << (peer.last_error ? peer.last_error->to_detail_string() : "none");

      out << "\n";
    }
  }

  void print_result::f_blockchain_get_block(std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client)
  {
    auto block = result.as<fc::mutable_variant_object>();
    if(!block["previous"].is_null() && FILTER_OUTPUT_FOR_TESTS)
      block["previous"] = "<d-ign>" + block["previous"].as_string() + "</d-ign>";
    if(!block["next_secret_hash"].is_null() && FILTER_OUTPUT_FOR_TESTS)
      block["next_secret_hash"] = "<d-ign>" + block["next_secret_hash"].as_string() + "</d-ign>";
    if(!block["delegate_signature"].is_null() && FILTER_OUTPUT_FOR_TESTS)
      block["delegate_signature"] = "<d-ign>" + block["delegate_signature"].as_string() + "</d-ign>";
    if(!block["random_seed"].is_null() && FILTER_OUTPUT_FOR_TESTS)
      block["random_seed"] = "<d-ign>" + block["random_seed"].as_string() + "</d-ign>";
    if(!block["processing_time"].is_null() && FILTER_OUTPUT_FOR_TESTS)
      block["processing_time"] = "<d-ign>" + block["processing_time"].as_string() + "</d-ign>";
    out << fc::json::to_pretty_string(block) << "\n";
  }
} }
