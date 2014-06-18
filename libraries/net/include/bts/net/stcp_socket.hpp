#pragma once
#include <fc/network/tcp_socket.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/elliptic.hpp>

namespace bts { namespace net {

/**
 *  Uses ECDH to negotiate a aes key for communicating
 *  with other nodes on the network.
 */
class stcp_socket : public virtual fc::iostream
{
  public:
    stcp_socket();
    ~stcp_socket();
    fc::tcp_socket&  get_socket() { return _sock; }
    void             accept();

    void             connect_to( const fc::ip::endpoint& remote_endpoint );
    void             bind( const fc::ip::endpoint& local_endpoint );

    virtual size_t   readsome( char* buffer, size_t max );
    virtual bool     eof()const;

    virtual size_t   writesome( const char* buffer, size_t len );
    virtual void     flush();
    virtual void     close();

    void             get( char& c ) { read( &c, 1 ); }

  private:
    void do_key_exchange();

    fc::ecc::private_key _priv_key;
    fc::array<char,8>    _buf;
    //uint32_t             _buf_len;
    fc::tcp_socket       _sock;
    fc::aes_encoder      _send_aes;
    fc::aes_decoder      _recv_aes;
};

typedef std::shared_ptr<stcp_socket> stcp_socket_ptr;

} } // bts::net
