#include <bts/blockchain/types.hpp>
#include <bts/cli/pretty.hpp>
#include <bts/cli/print_result.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/wallet/url.hpp>

#include <boost/range/algorithm/max_element.hpp>
#include <boost/range/algorithm/min_element.hpp>

namespace bts { namespace cli {

  bts::client::client* print_result::_client = nullptr;

  print_result::print_result(bts::client::client* client)
  {
    _client = client;
    initialize();
  }

  void print_result::format_and_print(std::ostream& out, const string& method_name,
    const fc::variants& arguments, const fc::variant& result) const
  {
    try
    {
      t_command_to_function::const_iterator iter = _command_to_function.find(method_name);

      if(iter != _command_to_function.end())
        (iter->second)(out, arguments, result);
      else
      {
        // there was no custom handler for this particular command, see if the return type
        // is one we know how to pretty-print
        string result_type;

        const bts::api::method_data& method_data = _client->get_rpc_server()->get_method_data(method_name);
        result_type = method_data.return_type;

        if(result_type == "asset")
        {
          out << (string)result.as<bts::blockchain::asset>() << "\n";
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
    catch(...)
    {
      out << "unexpected exception \n";
    }
  }

  void print_result::initialize()
  {
    _command_to_function["help"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      string help_string = result.as<string>();
      out << help_string << "\n"; };

    _command_to_function["wallet_account_vote_summary"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& votes = result.as<account_vote_summary_type>();
      out << pretty_vote_summary(votes, _client); };

    _command_to_function["wallet_account_create"] = &f_wallet_account_create;

    _command_to_function["debug_list_errors"] = &f_debug_list_errors;

    _command_to_function["get_info"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& info = result.as<variant_object>();
      out << pretty_info(info, _client); };

    _command_to_function["blockchain_get_info"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& config = result.as<variant_object>();
      out << pretty_blockchain_info(config, _client); };

    _command_to_function["wallet_get_info"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& info = result.as<variant_object>();
      out << pretty_wallet_info(info, _client); };

    _command_to_function["wallet_account_transaction_history"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& transactions = result.as<vector<pretty_transaction>>();
      out << pretty_transaction_list(transactions, _client); };

    _command_to_function["wallet_market_order_list"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& market_orders = result.as<vector<market_order>>();
      out << pretty_market_orders(market_orders, _client); };

    _command_to_function["wallet_market_order_list2"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& market_orders = result.as<map<order_id_type, market_order>>();
      out << pretty_market_orders2(market_orders, _client); };

    _command_to_function["blockchain_market_list_asks"] = &f_blockchain_market_list;
    _command_to_function["blockchain_market_list_bids"] = &f_blockchain_market_list;
    _command_to_function["blockchain_market_list_shorts"] = &f_blockchain_market_list;

    _command_to_function["wallet_list_my_accounts"] = &f_wallet_list_my_accounts;

    _command_to_function["wallet_list_accounts"] = &f_wallet_list_accounts;
    _command_to_function["wallet_list_unregistered_accounts"] = &f_wallet_list_accounts;
    _command_to_function["wallet_list_favorite_accounts"] = &f_wallet_list_accounts;

    _command_to_function["wallet_account_balance"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& balances = result.as<account_balance_summary_type>();
      out << pretty_balances(balances, _client); };

    _command_to_function["wallet_transfer"]                     = &f_wallet_transfer;
    _command_to_function["wallet_transfer_from"]                = &f_wallet_transfer;
    _command_to_function["wallet_get_transaction"]              = &f_wallet_transfer;
    _command_to_function["wallet_account_register"]             = &f_wallet_transfer;
    _command_to_function["wallet_account_update_registration"]  = &f_wallet_transfer;
    _command_to_function["wallet_account_update_active_key"]    = &f_wallet_transfer;
    _command_to_function["wallet_asset_create"]                 = &f_wallet_transfer;
    _command_to_function["wallet_asset_issue"]                  = &f_wallet_transfer;
    _command_to_function["wallet_delegate_withdraw_pay"]        = &f_wallet_transfer;
    _command_to_function["wallet_market_submit_bid"]            = &f_wallet_transfer;
    _command_to_function["wallet_market_submit_ask"]            = &f_wallet_transfer;
    _command_to_function["wallet_market_submit_short"]          = &f_wallet_transfer;
    _command_to_function["wallet_market_cover"]                 = &f_wallet_transfer;
    _command_to_function["wallet_market_add_collateral"]        = &f_wallet_transfer;
    _command_to_function["wallet_market_cancel_order"]          = &f_wallet_transfer;
    _command_to_function["wallet_publish_version"]              = &f_wallet_transfer;
    _command_to_function["wallet_publish_slate"]                = &f_wallet_transfer;
    _command_to_function["wallet_publish_price_feed"]           = &f_wallet_transfer;
    _command_to_function["wallet_publish_feeds"]                = &f_wallet_transfer;
    _command_to_function["wallet_scan_transaction"]             = &f_wallet_transfer;
    _command_to_function["wallet_recover_transaction"]          = &f_wallet_transfer;

    _command_to_function["wallet_list"] = &f_wallet_list;

    _command_to_function["network_get_usage_stats"] = &f_network_get_usage_stats;

    _command_to_function["blockchain_list_delegates"] = &f_blockchain_list_delegates;
    _command_to_function["blockchain_list_active_delegates"] = &f_blockchain_list_delegates;

    _command_to_function["blockchain_list_blocks"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      const auto& block_records = result.as<vector<block_record>>();
      out << pretty_block_list(block_records, _client); };

    _command_to_function["blockchain_list_accounts"] = &f_blockchain_list_accounts;

    _command_to_function["blockchain_list_assets"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      out << pretty_asset_list(result.as<vector<asset_record>>(), _client); };

    _command_to_function["blockchain_get_proposal_votes"] = &f_blockchain_get_proposal_votes;

    _command_to_function["blockchain_get_account"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      out << pretty_account(result.as<oaccount_record>(), _client); };

    _command_to_function["blockchain_list_forks"] = &f_blockchain_list_forks;

    _command_to_function["blockchain_list_pending_transactions"] = &f_blockchain_list_pending_transactions;

    _command_to_function["blockchain_list_proposals"] = &f_blockchain_list_proposals;

    _command_to_function["blockchain_market_order_book"] = &f_blockchain_market_order_book;

    _command_to_function["blockchain_market_order_history"] = &f_blockchain_market_order_history;

    _command_to_function["blockchain_market_price_history"] = &f_blockchain_market_price_history;

    _command_to_function["blockchain_unclaimed_genesis"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      out << _client->get_chain()->to_pretty_asset(result.as<asset>()) << "\n"; };

    _command_to_function["network_list_potential_peers"] = &f_network_list_potential_peers;

    _command_to_function["wallet_set_transaction_fee"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      out << _client->get_chain()->to_pretty_asset(result.as<asset>()) << "\n"; };

    _command_to_function["blockchain_calculate_base_supply"] = [](std::ostream& out, const fc::variants& arguments, const fc::variant& result){
      out << _client->get_chain()->to_pretty_asset(result.as<asset>()) << "\n"; };

  }

  void print_result::f_wallet_account_create(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto account_key = result.as_string();
    auto account = _client->get_wallet()->get_account_for_address(public_key_type(account_key));

    if(account)
      out << "\n\nAccount created successfully. You may give the following link to others"
      " to allow them to add you as a contact and send you funds:\n" CUSTOM_URL_SCHEME ":"
      << account->name << ':' << account_key << '\n';
    else
      out << "Sorry, something went wrong when adding your account.\n";
  }

  void print_result::f_debug_list_errors(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto error_map = result.as<map<fc::time_point, fc::exception> >();
    if(error_map.empty())
      out << "No errors.\n";
    else
      for(const auto& item : error_map)
      {
      (out) << string(item.first) << " (" << fc::get_approximate_relative_time_string(item.first) << " )\n";
      (out) << item.second.to_detail_string();
      (out) << "\n";
      }
  }

  void print_result::f_blockchain_market_list(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    const auto& market_orders = result.as<vector<market_order>>();
    out << pretty_market_orders(market_orders, _client);
  }

  void print_result::f_wallet_list_my_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto accts = result.as<vector<wallet_account_record>>();

    out << std::setw(35) << std::left << "NAME (* delegate)";
    out << std::setw(64) << "KEY";
    out << std::setw(22) << "REGISTERED";
    out << std::setw(15) << "FAVORITE";
    out << std::setw(15) << "APPROVAL";
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

      if(acct.is_favorite)
        out << std::setw(15) << "YES";
      else
        out << std::setw(15) << "NO";

      out << std::setw(15) << std::to_string(acct.approved);
      if(acct.is_delegate())
        out << std::setw(25) << (acct.block_production_enabled ? "YES" : "NO");
      else
        out << std::setw(25) << "N/A";
      out << "\n";
    }
  }

  void print_result::f_wallet_list_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto accts = result.as<vector<wallet_account_record>>();

    out << std::setw(35) << std::left << "NAME (* delegate)";
    out << std::setw(64) << "KEY";
    out << std::setw(22) << "REGISTERED";
    out << std::setw(15) << "FAVORITE";
    out << std::setw(15) << "APPROVAL";
    out << "\n";

    for(const auto& acct : accts)
    {
      if(acct.is_delegate())
        out << std::setw(35) << pretty_shorten(acct.name, 33) + " *";
      else
        out << std::setw(35) << pretty_shorten(acct.name, 34);

      out << std::setw(64) << string(acct.active_key());

      if(acct.id == 0)
        out << std::setw(22) << "NO";
      else
        out << std::setw(22) << pretty_timestamp(acct.registration_date);

      if(acct.is_favorite)
        out << std::setw(15) << "YES";
      else
        out << std::setw(15) << "NO";

      out << std::setw(10) << std::to_string(acct.approved);
      out << "\n";
    }
  }

  void print_result::f_wallet_transfer(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    const auto& record = result.as<wallet_transaction_record>();
    const auto& pretty = _client->get_wallet()->to_pretty_trx(record);
    const std::vector<pretty_transaction> transactions = { pretty };
    out << pretty_transaction_list(transactions, _client);
  }

  void print_result::f_wallet_list(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto wallets = result.as<vector<string>>();
    if(wallets.empty())
      out << "No wallets found.\n";
    else
      for(const auto& wallet : wallets)
        out << wallet << "\n";
  }

  void print_result::f_network_get_usage_stats(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
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

  void print_result::f_blockchain_list_delegates(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    const auto& delegate_records = result.as<vector<account_record>>();
    out << pretty_delegate_list(delegate_records, _client);
  }

  void print_result::f_blockchain_list_accounts(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    string start = "";
    int32_t count = 25; // In CLI this is a more sane default
    if(arguments.size() > 0)
      start = arguments[0].as_string();
    if(arguments.size() > 1)
      count = arguments[1].as<int32_t>();
    print_registered_account_list(out, result.as<vector<account_record>>(), count);
  }

  void print_result::print_registered_account_list(std::ostream& out, const vector<account_record>& account_records, int32_t count)
  {
    out << std::setw(35) << std::left << "NAME (* delegate)";
    out << std::setw(64) << "KEY";
    out << std::setw(22) << "REGISTERED";
    out << std::setw(15) << "VOTES FOR";
    out << std::setw(15) << "APPROVAL";

    out << '\n';
    for(int i = 0; i < 151; ++i)
      out << '-';
    out << '\n';

    auto counter = 0;
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

        try
        {
          out << std::setw(15) << std::to_string(_client->get_wallet()->get_account_approval(acct.name));
        }
        catch(...)
        {
          out << std::setw(15) << false;
        }
      }
      else
      {
        out << std::setw(15) << "N/A";
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

  void print_result::f_blockchain_get_proposal_votes(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto votes = result.as<vector<proposal_vote>>();
    out << std::left;
    out << std::setw(15) << "DELEGATE";
    out << std::setw(22) << "TIME";
    out << std::setw(5) << "VOTE";
    out << std::setw(35) << "MESSAGE";
    out << "\n----------------------------------------------------------------";
    out << "-----------------------\n";
    for(const auto& vote : votes)
    {
      auto rec = _client->get_chain()->get_account_record(vote.id.delegate_id);
      out << std::setw(15) << pretty_shorten(rec->name, 14);
      out << std::setw(20) << pretty_timestamp(vote.timestamp);
      if(vote.vote == proposal_vote::no)
      {
        out << std::setw(5) << "NO";
      }
      else if(vote.vote == proposal_vote::yes)
      {
        out << std::setw(5) << "YES";
      }
      else
      {
        out << std::setw(5) << "??";
      }
      out << std::setw(35) << pretty_shorten(vote.message, 35);
      out << "\n";
    }
    out << "\n";
  }

  void print_result::f_blockchain_list_forks(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
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

          auto delegate_record = _client->get_chain()->get_account_record(tine.signing_delegate);
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

  void print_result::f_blockchain_list_pending_transactions(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto transactions = result.as<vector<signed_transaction>>();

    if(transactions.empty())
    {
      out << "No pending transactions.\n";
    }
    else
    {
      out << std::setw(10) << "TXN ID"
        << std::setw(10) << "SIZE"
        << std::setw(25) << "OPERATION COUNT"
        << std::setw(25) << "SIGNATURE COUNT"
        << "\n" << std::string(70, '-') << "\n";

      for(const auto& transaction : transactions)
      {
        if(FILTER_OUTPUT_FOR_TESTS)
          out << std::setw(10) << "[redacted]";
        else
          out << std::setw(10) << transaction.id().str().substr(0, 8);

        out << std::setw(10) << transaction.data_size()
          << std::setw(25) << transaction.operations.size()
          << std::setw(25) << transaction.signatures.size()
          << "\n";
      }
    }
  }

  void print_result::f_blockchain_list_proposals(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto proposals = result.as<vector<proposal_record>>();
    out << std::left;
    out << std::setw(10) << "ID";
    out << std::setw(20) << "SUBMITTED BY";
    out << std::setw(22) << "SUBMIT TIME";
    out << std::setw(15) << "TYPE";
    out << std::setw(20) << "SUBJECT";
    out << std::setw(35) << "BODY";
    out << std::setw(20) << "DATA";
    out << std::setw(10) << "RATIFIED";
    out << "\n------------------------------------------------------------";
    out << "-----------------------------------------------------------------";
    out << "------------------\n";
    for(const auto& prop : proposals)
    {
      out << std::setw(10) << prop.id;
      auto delegate_rec = _client->get_chain()->get_account_record(prop.submitting_delegate_id);
      out << std::setw(20) << pretty_shorten(delegate_rec->name, 19);
      out << std::setw(20) << pretty_timestamp(prop.submission_date);
      out << std::setw(15) << pretty_shorten(prop.proposal_type, 14);
      out << std::setw(20) << pretty_shorten(prop.subject, 19);
      out << std::setw(35) << pretty_shorten(prop.body, 34);
      out << std::setw(20) << pretty_shorten(fc::json::to_pretty_string(prop.data), 19);
      out << std::setw(10) << (prop.ratified ? "YES" : "NO");
    }
    out << "\n";
  }

  void print_result::f_blockchain_market_order_book(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    auto bids_asks = result.as<std::pair<vector<market_order>, vector<market_order>>>();
    vector<market_order> filtered_shorts;
    if(bids_asks.first.size() == 0 && bids_asks.second.size() == 0)
    {
      out << "No Orders\n";
      return;
    }
    out << std::string(18, ' ') << "BIDS (* Short Order)"
      << std::string(38, ' ') << " | "
      << std::string(34, ' ') << "ASKS"
      << std::string(34, ' ') << "\n"
      << std::left << std::setw(26) << "TOTAL"
      << std::setw(20) << "QUANTITY"
      << std::right << std::setw(30) << "PRICE"
      << " | " << std::left << std::setw(30) << "PRICE" << std::right << std::setw(23) << "QUANTITY" << std::setw(26) << "TOTAL" << "   COLLATERAL" << "\n"
      << std::string(175, '-') << "\n";

    vector<market_order>::iterator bid_itr = bids_asks.first.begin();
    auto ask_itr = bids_asks.second.begin();

    asset_id_type quote_id;
    asset_id_type base_id;

    if(bid_itr != bids_asks.first.end())
    {
      quote_id = bid_itr->get_price().quote_asset_id;
      base_id = bid_itr->get_price().base_asset_id;
    }
    if(ask_itr != bids_asks.second.end())
    {
      quote_id = ask_itr->get_price().quote_asset_id;
      base_id = ask_itr->get_price().base_asset_id;
    }

    auto quote_asset_record = _client->get_chain()->get_asset_record(quote_id);
    // fee order is the market order to convert fees from other asset classes to XTS
    bool show_fee_order_record = base_id == 0
      && !quote_asset_record->is_market_issued()
      && quote_asset_record->collected_fees > 0;

    oprice  median_price = _client->get_chain()->get_median_delegate_price(quote_id);
    auto status = _client->get_chain()->get_market_status(quote_id, base_id);
    auto max_short_price = median_price ? *median_price : (status ? status->avg_price_1h : price(0, quote_id, base_id));


    while(bid_itr != bids_asks.first.end() || ask_itr != bids_asks.second.end())
    {
      if(median_price)
      {
        while(bid_itr != bids_asks.first.end())
        {
          if(bid_itr->type == bts::blockchain::short_order)
          {
            if(bid_itr->get_price() > *median_price)
              filtered_shorts.push_back(*bid_itr++);
            else
              break;
          }
          else
            break;
        }
      }
      if(bid_itr == bids_asks.first.end())
        break;

      if(show_fee_order_record)
      {
        out << std::left << std::setw(26) << _client->get_chain()->to_pretty_asset(asset(quote_asset_record->collected_fees, quote_id))
          << std::setw(20) << " "
          << std::right << std::setw(30) << "MARKET PRICE";

        out << ' ';
        show_fee_order_record = false;
      }
      else if(bid_itr != bids_asks.first.end())
      {
        out << std::left << std::setw(26) << (bid_itr->type == bts::blockchain::bid_order ?
          _client->get_chain()->to_pretty_asset(bid_itr->get_balance())
          : _client->get_chain()->to_pretty_asset(bid_itr->get_quote_quantity()))
          << std::setw(20) << (bid_itr->type == bts::blockchain::bid_order ?
          _client->get_chain()->to_pretty_asset(bid_itr->get_quantity())
          : _client->get_chain()->to_pretty_asset(bid_itr->get_balance()))
          << std::right << std::setw(30) << (fc::to_string(_client->get_chain()->to_pretty_price_double(bid_itr->get_price())) + " " + quote_asset_record->symbol);

        if(bid_itr->type == bts::blockchain::short_order)
          out << '*';
        else
          out << ' ';

        ++bid_itr;
      }
      else
        out << std::string(77, ' ');

      out << "| ";

      while(ask_itr != bids_asks.second.end())
      {
        if(!ask_itr->collateral)
        {
          out << std::left << std::setw(30) << (fc::to_string(_client->get_chain()->to_pretty_price_double(ask_itr->get_price())) + " " + quote_asset_record->symbol)
            << std::right << std::setw(23) << _client->get_chain()->to_pretty_asset(ask_itr->get_quantity())
            << std::right << std::setw(26) << _client->get_chain()->to_pretty_asset(ask_itr->get_quote_quantity());
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
      out << std::setw(39) << "FILTERED SHORTS"
        << std::string(38, ' ') << "| "
        << std::string(34, ' ') << "MARGIN"
        << std::string(34, ' ') << "\n"
        << std::left << std::setw(26) << "TOTAL"
        << std::setw(20) << "QUANTITY"
        << std::right << std::setw(30) << "PRICE"
        << " | " << std::left << std::setw(30) << "CALL PRICE" << std::right << std::setw(23) << "QUANTITY" << std::setw(26) << "TOTAL" << "   COLLATERAL" << "\n"
        << std::string(175, '-') << "\n";

      {
        auto ask_itr = bids_asks.second.rbegin();
        auto bid_itr = filtered_shorts.begin();
        while(bid_itr != filtered_shorts.end() || ask_itr != bids_asks.second.rend())
        {
          if(bid_itr != filtered_shorts.end())
          {
            out << std::left << std::setw(26) << (bid_itr->type == bts::blockchain::bid_order ?
              _client->get_chain()->to_pretty_asset(bid_itr->get_balance())
              : _client->get_chain()->to_pretty_asset(bid_itr->get_quote_quantity()))
              << std::setw(20) << (bid_itr->type == bts::blockchain::bid_order ?
              _client->get_chain()->to_pretty_asset(bid_itr->get_quantity())
              : _client->get_chain()->to_pretty_asset(bid_itr->get_balance()))
              << std::right << std::setw(30) << (fc::to_string(_client->get_chain()->to_pretty_price_double(bid_itr->get_price())) + " " + quote_asset_record->symbol)
              << "*";
          }
          else
            out << std::string(77, ' ');

          out << "| ";

          while(ask_itr != bids_asks.second.rend() && !ask_itr->collateral)
            ++ask_itr;
          if(ask_itr != bids_asks.second.rend())
          {
            out << std::left << std::setw(30) << std::setprecision(8) << (fc::to_string(_client->get_chain()->to_pretty_price_double(ask_itr->get_price())) + " " + quote_asset_record->symbol)
              << std::right << std::setw(23) << _client->get_chain()->to_pretty_asset(ask_itr->get_quantity())
              << std::right << std::setw(26) << _client->get_chain()->to_pretty_asset(ask_itr->get_quote_quantity());
            out << "   " << _client->get_chain()->to_pretty_asset(asset(*ask_itr->collateral));
            out << "\n";
          }
          else
            out << "\n";

          if(ask_itr != bids_asks.second.rend())
            ++ask_itr;
          if(bid_itr != filtered_shorts.end())
            ++bid_itr;
        }
      }

      auto recent_average_price = _client->get_chain()->get_market_status(quote_id, base_id)->avg_price_1h;
      out << "Average Price in Recent Trades: "
        << _client->get_chain()->to_pretty_price(recent_average_price)
        << "     ";

      auto status = _client->get_chain()->get_market_status(quote_id, base_id);
      if(status)
      {
        out << "Maximum Short Price: "
          << _client->get_chain()->to_pretty_price(max_short_price)
          << "     ";

        out << "Bid Depth: " << _client->get_chain()->to_pretty_asset(asset(status->bid_depth, base_id)) << "     ";
        out << "Ask Depth: " << _client->get_chain()->to_pretty_asset(asset(status->ask_depth, base_id)) << "     ";
        out << "Min Depth: " << _client->get_chain()->to_pretty_asset(asset(BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT)) << "\n";
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

      // TODO: print insurance fund for market issued assets

    } // end call section that only applies to market issued assets vs XTS
    else
    {
      auto status = _client->get_chain()->get_market_status(quote_id, base_id);
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
  }

  void print_result::f_blockchain_market_order_history(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
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
        << std::setw(30) << _client->get_chain()->to_pretty_price(order.bid_price)
        << std::setw(25) << _client->get_chain()->to_pretty_asset(order.bid_paid)
        << std::setw(25) << _client->get_chain()->to_pretty_asset(order.bid_received)
        << std::setw(20) << _client->get_chain()->to_pretty_asset(order.bid_paid - order.ask_received)
        << std::setw(23) << pretty_timestamp(order.timestamp)
        << std::setw(37) << string(order.bid_owner)
        << "\n"
        << std::setw(7) << "Sell  "
        << std::setw(30) << _client->get_chain()->to_pretty_price(order.ask_price)
        << std::setw(25) << _client->get_chain()->to_pretty_asset(order.ask_paid)
        << std::setw(25) << _client->get_chain()->to_pretty_asset(order.ask_received)
        << std::setw(20) << _client->get_chain()->to_pretty_asset(order.ask_paid - order.bid_received)
        << std::setw(30) << pretty_timestamp(order.timestamp)
        << "   " << string(order.ask_owner)
        << "\n";
    }
  }

  void print_result::f_blockchain_market_price_history(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
  {
    market_history_points points = result.as<market_history_points>();
    if(points.empty())
    {
      out << "No price history.\n";
      return;
    }

    out << std::setw(20) << "TIME"
      << std::setw(20) << "HIGHEST BID"
      << std::setw(20) << "LOWEST ASK"
      << std::setw(20) << "OPENING PRICE"
      << std::setw(20) << "CLOSING PRICE"
      << std::setw(20) << "TRADING VOLUME"
      << std::setw(20) << "AVERAGE PRICE"
      << "\n" << std::string(140, '-') << "\n";

    for(auto point : points)
    {
      out << std::setw(20) << pretty_timestamp(point.timestamp)
        << std::setw(20) << point.highest_bid
        << std::setw(20) << point.lowest_ask
        << std::setw(20) << point.opening_price
        << std::setw(20) << point.closing_price
        << std::setw(20) << _client->get_chain()->to_pretty_asset(asset(point.volume));
      if(point.recent_average_price)
        out << std::setw(20) << *point.recent_average_price;
      else
        out << std::setw(20) << "N/A";
      out << "\n";
    }
  }

  void print_result::f_network_list_potential_peers(std::ostream& out, const fc::variants& arguments, const fc::variant& result)
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

} }

//  else
//  {
//    // there was no custom handler for this particular command, see if the return type
//    // is one we know how to pretty-print
//    string result_type;
//    try
//    {
//      const bts::api::method_data& method_data = _rpc_server->get_method_data(method_name);
//      result_type = method_data.return_type;
//
//      if(result_type == "asset")
//      {
//        *_out << (string)result.as<bts::blockchain::asset>() << "\n";
//      }
//      else if(result_type == "address")
//      {
//        *_out << (string)result.as<bts::blockchain::address>() << "\n";
//      }
//      else if(result_type == "null" || result_type == "void")
//      {
//        *_out << "OK\n";
//      }
//      else
//      {
//        *_out << fc::json::to_pretty_string(result) << "\n";
//      }
//    }
//    catch(const fc::key_not_found_exception&)
//    {
//      elog(" KEY NOT FOUND ");
//      *_out << "key not found \n";
//    }
//    catch(...)
//    {
//      *_out << "unexpected exception \n";
//    }
//  }
//
//  *_out << std::right; /* Ensure default alignment is restored */
//}
