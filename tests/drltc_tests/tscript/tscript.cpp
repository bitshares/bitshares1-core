
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <fc/filesystem.hpp>
#include <fc/optional.hpp>
#include <fc/string.hpp>
#include <fc/variant.hpp>
#include <fc/variant_object.hpp>

#include <fc/io/iostream.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/rpc/json_connection.hpp>

#include <bts/blockchain/genesis_config.hpp>
#include <bts/client/client.hpp>
#include <bts/net/node.hpp>
#include <bts/rpc/rpc_server.hpp>

namespace bts { namespace tscript {

class client_context
{
public:
    client_context();
    virtual ~client_context();
    
    void configure_client_from_args();
    void start();
    
    std::vector<std::string> args;
    bts::client::client_ptr client;
    fc::future<void> client_done;
    std::map<std::string, std::string> template_context;
};

typedef std::shared_ptr<client_context> client_context_ptr;

class context
{
public:
    context();
    virtual ~context();

    void set_genesis(const fc::path& genesis_json_filename);

    void create_client(const std::string& name);
    fc::path get_data_dir_for_client(const std::string& name) const;
    
    void get_clients_by_variant(const fc::variant& spec, std::vector<client_context_ptr>& result, int depth=0);
    
    fc::path basedir;
    std::vector<client_context_ptr> v_clients;
    std::map<std::string, client_context_ptr> m_name_client;
    std::map<std::string, std::vector<std::string>> m_name_client_group;
    std::map<std::string, std::string> template_context;
    fc::path genesis_json_filename;
    bts::blockchain::genesis_block_config genesis_config;
    bts::net::simulated_network_ptr sim_network;
};

typedef std::shared_ptr<context> context_ptr;

class interpreter
{
public:
    interpreter();
    virtual ~interpreter();
    
    void run_single_command(const fc::variants& cmd);
    void run_command_list(const fc::variants& cmd_list);
    void run_file(const fc::path& path);
    
    void cmd_clients(const fc::variants& args);
    void cmd_x(const fc::variants& cmd);
    
    context_ptr ctx;
};

client_context::client_context()
{
    return;
}

client_context::~client_context()
{
    return;
}

inline void copy_str_to_buffer_with_0(std::vector<char> &buf, const std::string& s)
{
    std::copy(s.c_str(), s.c_str()+s.length(), std::back_inserter(buf));
    buf.push_back('\0');
    return;
}

void client_context::configure_client_from_args()
{
    std::vector<char> buf;
    std::vector<size_t> arg_offset;
    
    std::string arg_0 = "bitshares_client";
    arg_offset.push_back(0);
    copy_str_to_buffer_with_0(buf, arg_0);
    
    int argc = (int) this->args.size() + 1;
    
    for(int i=1;i<argc;i++)
    {
        arg_offset.push_back(buf.size());
        copy_str_to_buffer_with_0(buf, this->args[i-1]);
    }

    std::vector<char*> v_argv;
    v_argv.push_back(buf.data());
    for(int i=1;i<argc;i++)
        v_argv.push_back(v_argv[0] + arg_offset[i]);

    std::cout << "client invocation:";
    for(int i=0;i<argc;i++)
    {
        std::cout << v_argv.data()[i] << " ";
    }
    std::cout << "\n";

    this->client->configure_from_command_line(argc, v_argv.data());

    return;
}

void client_context::start()
{
    this->client_done = this->client->start();
    return;
}

context::context()
{
    this->sim_network = std::make_shared<bts::net::simulated_network>("tscript");
    this->m_name_client_group["all"] = std::vector<std::string>();
    this->basedir = "tmp";
    return;
}

context::~context()
{
    return;
}

void context::set_genesis(const fc::path& genesis_json_filename)
{
    this->genesis_json_filename = genesis_json_filename;
    this->genesis_config = fc::json::from_file(genesis_json_filename).as<bts::blockchain::genesis_block_config>();
    this->template_context["genesis.timestamp"] = this->genesis_config.timestamp.to_iso_string();
    return;
}

void context::create_client(const std::string& name)
{
    client_context_ptr cc = std::make_shared<client_context>();
    cc->args.push_back("--disable-default-peers");
    cc->args.push_back("--log-commands");
    cc->args.push_back("--ulog=0");
    cc->args.push_back("--min-delegate-connection-count=0");
    cc->args.push_back("--upnp=false");
    cc->args.push_back("--genesis-config=" + this->genesis_json_filename.string());
    cc->args.push_back("--data-dir=" + this->get_data_dir_for_client(name).string());
    
    cc->client = std::make_shared<bts::client::client>("tscript", this->sim_network);
    cc->configure_client_from_args();
    cc->client->set_client_debug_name(name);
    cc->template_context["client.name"] = name;

    this->m_name_client[name] = cc;
    this->m_name_client_group["all"].push_back(name);

    return;
}

fc::path context::get_data_dir_for_client(const std::string& name) const
{
    return this->basedir / "client" / name;
}

void context::get_clients_by_variant(const fc::variant& spec, std::vector<client_context_ptr>& result, int depth)
{
    FC_ASSERT(depth < 20);
    
    if( spec.is_array() )
    {
        const fc::variants& v = spec.get_array();
        for( const fc::variant& e : v )
            this->get_clients_by_variant(e, result, depth+1);
        return;
    }

    if( spec.is_string() )
    {
        std::string target_name = spec.get_string();
        std::cout << "in spec.is_string() : target=" << target_name << "\n";
        auto p = this->m_name_client_group.find(target_name);
        if( p != this->m_name_client_group.end() )
        {
            std::cout << "   found group\n";
            for( const fc::string& e : p->second )
            {
                auto q = this->m_name_client.find(e);
                if( q != this->m_name_client.end() )
                {
                    result.push_back(q->second);
                    std::cout << "   group member " << e << " found\n";
                }
                else
                    FC_ASSERT(false, "couldn't find named client when expanding group definition");
            }
            return;
        }
        auto q = this->m_name_client.find(target_name);
        if( q != this->m_name_client.end() )
        {
            std::cout << "   found singleton\n";
            result.push_back(q->second);
            return;
        }
        FC_ASSERT(false, "couldn't find named client");
    }
    
    FC_ASSERT(false, "expected: client-spec");
}

interpreter::interpreter()
{
    this->ctx = std::make_shared<context>();
    return;
}

interpreter::~interpreter()
{
    return;
}

void interpreter::run_single_command(const fc::variants& cmd)
{
    // parse the command
    std::string action = cmd[0].get_string();
    
    if( action == "x")
    {
        this->cmd_x(cmd);
    }
    else if( action == "!clients" )
    {
        this->cmd_clients( cmd );
    }
    else if( action == "!defgroup" )
    {
        FC_ASSERT(false, "unimplemented command");
    }
    else if( action == "!include" )
    {
        FC_ASSERT(false, "unimplemented command");
    }
    else if( action == "!pragma" )
    {
        FC_ASSERT(false, "unimplemented command");
    }
    else
    {
        FC_ASSERT(false, "unknown command");
    }
    return;
}

void interpreter::run_command_list(const fc::variants& cmd_list)
{
    for(auto cmd : cmd_list)
        this->run_single_command(cmd.as<fc::variants>());
    return;
}

void interpreter::run_file(const fc::path& path)
{
    fc::variants v = fc::json::from_file<fc::variants>(path);
    this->run_command_list(v);
    return;
}

void interpreter::cmd_clients(const fc::variants& cmd)
{
    FC_ASSERT(cmd.size() >= 2);
    
    fc::variants args = cmd[1].get_array();
    
    // create clients
    for( const fc::variant& e : args )
    {
        std::string cname = e.as<std::string>();
        std::cout << "creating client " << cname << "\n";
        this->ctx->create_client(cname);
    }
    return;
}

void interpreter::cmd_x(const fc::variants& cmd)
{
    FC_ASSERT( cmd.size() >= 3 );

    std::cout << "in cmd_x\n";
    
    std::vector<bts::tscript::client_context_ptr> targets;
    
    this->ctx->get_clients_by_variant( cmd[1], targets );

    std::cout << "targets found: " << targets.size() << "\n";

    for( bts::tscript::client_context_ptr& t : targets )
    {
        std::cout << "   " << t->client->debug_get_client_name() << ":\n";

        const std::string& method_name = cmd[2].get_string();

        std::cout << "      " << method_name << "\n";
        
        // create context for command by combining template context dictionaries
        fc::mutable_variant_object effective_ctx;
        for( auto& e : this->ctx->template_context )
            effective_ctx[e.first] = e.second;
        for( auto& e : t->template_context )
            effective_ctx[e.first] = e.second;
        fc::variant_object v_effective_ctx = effective_ctx;
        
        // substitute into parameters
        fc::variants args;
        if( cmd.size() >= 4 )
            args = cmd[3].get_array();
        
        for( size_t i=0,n=args.size(); i<n; i++ )
        {
            if( args[i].is_string() )
            {
                args[i] = fc::format_string(args[i].get_string(), v_effective_ctx);
            }
            std::cout << "      " << fc::json::to_string(args[i]) << "\n";
        }
        
        fc::optional<fc::variant> result;
        fc::optional<fc::exception> exc;
        
        try
        {
            result = t->client->get_rpc_server()->direct_invoke_method(method_name, args);
            std::cout << "      result:" << fc::json::to_string(result) << "\n";
        }
        catch( const fc::exception& e )
        {
            exc = e;
            std::cout << "      " << e.to_detail_string() << "\n";
        }
        
        std::string cmp_op = "nop";
        fc::variant cmp_op_arg;
        if( cmd.size() >= 5 )
        {
            cmp_op = cmd[4].get_string();
            if( cmd.size() >= 6 )
            {
                cmp_op_arg = cmd[5];
            }
        }
        
        bool expected_exception = false;
        
        if( cmp_op == "nop" )
            ;
        else if( cmp_op == "eq" )
        {
            FC_ASSERT(result.valid());
            std::cout << "*result : " << fc::json::to_string(*result) << "\n";
            std::cout << "cmp_op_arg : " << fc::json::to_string(cmp_op_arg) << "\n";
            FC_ASSERT(((*result) == cmp_op_arg).as_bool());
        }
        else if( cmp_op == "ex" )
        {
            expected_exception = true;
        }
        else if( cmp_op == "ss" )
        {
            FC_ASSERT(result.valid());
            FC_ASSERT(cmp_op_arg.is_object());
            FC_ASSERT(result->is_object());
            
            fc::variant_object vo_result = result->get_object();
            for( auto& kv : cmp_op_arg.get_object() )
            {
                auto it = vo_result.find(kv.key());
                FC_ASSERT( it != vo_result.end() );
                FC_ASSERT( (it->value() == kv.value()).as_bool() );
            }
        }
        
        if( (!expected_exception) && (!result.valid()) )
            FC_ASSERT(false, "terminating due to unexpected exception");
    }
    std::cout << "\n";

    return;
}

void run_single_tscript(
    const fc::path& genesis_json_filename,
    const fc::path& tscript_filename
    )
{
    FC_ASSERT( fc::is_regular_file(tscript_filename) );

    // delete client directories
    fc::remove_all( "tmp/client" );

    bts::tscript::interpreter interp;
    interp.ctx->set_genesis(genesis_json_filename);
    interp.run_file(tscript_filename);
    return;
}

inline bool startswith(
    const std::string& a,
    const std::string& prefix
    )
{
    size_t m = prefix.length(), n = a.length();
    return (m <= n) && (0 == a.compare( 0, m, prefix ));
}

inline bool endswith(
    const std::string& a,
    const std::string& suffix
    )
{
    size_t m = suffix.length(), n = a.length();
    return (m <= n) && (0 == a.compare( n-m, m, suffix ));
}

void run_tscripts(
    const fc::path& genesis_json_filename,
    const fc::path& target_filename
    )
{
    if( !fc::is_directory(target_filename) )
    {
        run_single_tscript( genesis_json_filename, target_filename );
        return;
    }

    for( fc::recursive_directory_iterator it(target_filename), it_end;
         it != it_end; ++it )
    {
        fc::path filename = (*it);
        if( !fc::is_regular_file(filename) )
            continue;
        if( !endswith(filename.string(), ".tscript") )
            continue;
        run_single_tscript(genesis_json_filename, filename);
    }
    return;
}

} }

int main(int argc, char **argv, char **envp)
{
    for( int i=1; i<argc; i++ )
        bts::tscript::run_tscripts( "tmp/genesis.json", argv[i] );
    return 0;
}
