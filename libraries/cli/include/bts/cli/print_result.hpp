#pragma once

#include <bts/cli/pretty.hpp>

#include <fc/variant.hpp>

#include <functional>
#include <iomanip>
#include <iostream>

namespace bts { namespace cli {

  extern bool FILTER_OUTPUT_FOR_TESTS;

  class print_result
  {
  public:
    print_result();

    void format_and_print( const string& method_name, const fc::variants& arguments, const fc::variant& result,
                           cptr client, std::ostream& out )const;

  private:
    void initialize();

    static void f_blockchain_get_account_wall( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_wallet_account_create( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_debug_list_errors( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_market_list( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_market_short_list( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_wallet_list_my_accounts( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_wallet_list_accounts( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_wallet_transfer( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_wallet_list( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_network_get_usage_stats( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_list_delegates( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_list_accounts( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_list_forks( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_list_pending_transactions( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_market_order_book( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_market_order_history( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_market_price_history( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_network_list_potential_peers( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );
    static void f_blockchain_get_block( std::ostream& out, const fc::variants& arguments, const fc::variant& result, cptr client );

    static void print_network_usage_graph( std::ostream& out, const std::vector<uint32_t>& usage_data );
    static void print_registered_account_list( std::ostream& out, const vector<account_record>& account_records, int32_t count, cptr client );

  private:
    typedef std::function<void( std::ostream&, const fc::variants&, const fc::variant&, cptr )> t_function;
    typedef std::map<std::string, t_function> t_command_to_function;

    t_command_to_function _command_to_function;
  };

} }
