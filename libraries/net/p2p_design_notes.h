class client : public blockchain_manager
{
  // both in the syncing phase and during normal operation, blocks and signatures may arrive
  // out of order.  if they do, we need to store them until they can be paired with their
  // corresponding signature/block and added to the end of the block chain.
  unapproved_blocks; // blocks we have the data for, but no signatures
  approved_blocks; // signed blocks not yet in the block chain (we haven't seen/approved the block immediately preceding them)
  signatures_without_a_block; // signatures we received but did not yet have the blocks for.  
                             // when we put a signature here, we will have sent out a request for the block data
  uncommitted_transactions; // transactions we've seen and validated but have not been added to the block chain
  unincludable_transactions; // transactions we've seen but were not valid.  these won't be used in generating the current block,
                             // but we'll try to validate them again once get a new block.
  set<header> illegal_transactions; // 

  blockchain_database _blockchain_database;
  node _node;

  bool is_transaction_legal(transaction)
  {
    // return true if this transaction could ever exist in any block chain
  }
  bool is_transaction_valid(transaction)
  {
    // validates the transaction according to the rules for this blockchain
  }
  void add_new_transaction(transaction)
  {
    if (is_transaction_legal(transaction))
    {
      if (mining)
      {
        if (is_transaction_valid(transaction))
        {
          // add to uncommitted_transactions
        }
        else
        {
          // add to unincludable_transactions
        }
      }
      else
      {
        // add to unincludable_transactions
      }
    }
    else // not a legal transaction
    {
      // discard, never pass this on to another peer
    }
  }

  bool is_block_valid(block)
  {
    // validates the block according to the rules for this blockchain
  }
  void push_block(signed_block signed_block_to_push)
  {
    if (is_block_valid(signed_block_to_push.block))
    {
      // pushes the block onto the database
      // remove any block transactions from uncommitted_transactions and unincludable_transactions
      // does a lookup in the approved block map to see if any blocks are the next block after the block we're pushing.
      // if they are, we remove them from approved_blocks, validate and push them as well.  keep looping until we 
      // don't find any more blocks we can add to the chain
      // call add_new_transaction for all unincludable transactions(but prevent adding dupes)
    }
    else
    {
      wlog("discarding invalid block");
    }
  }
  void run()
  {
    _node.set_blockchain_manager(this);
    _node.connect();
    _node.fetch_signed_headers(_blockchain_database.get_top());
    while(1);
  }
public:
  /** returns a vector of up to 2000 headers from the local blockchain database to the caller
   * @param last_header_seen  hash of the last header seen.  The application should return
   *                          a block of headers starting immeditaely after last_header_seen
   */
  virtual signed_header_vector get_signed_headers(header last_header_seen) override; // from blockchain_manager

  /** Returns true if the header refers to a block we know about.  It could 
   * be anywhere in our blockchain database, or our list of approved or
   * unapproved blocks
   */
  virtual bool is_known_block_header(header ) override; // from blockchain_manager
		  
  /** called when the remote system sends us a list of signed headers it has seen
   * and accepted as part of the block chain.  
   *
   * This will happen during the initial
   * synch as a result of a fetch_signed_headers call.
   * 
   * When the application gets this, it will look at its unapporved_blocks
   * to see if it has the block(s) signed by this message.
   * If the blocks are missing, it will call fetch_bodies() to retreive them,
   * and store off the signature(s) in signatures_without_blocks to attach to the 
   * block when it/they come in.
   * If we already have the block in unapproved_blocks, we remove it and attach 
   * the signature.
   * If it can be attached to the end of the block chain, we call push_block(), 
   * otherwise we insert it into approved_blocks.
   * 
   * @param headers the list of signed headers that might be new to us
   */
  virtual void on_signed_header_inventory(signed_header_vector headers) override; // from blockchain_manager

  /** called when the remote system sends us a new signed header from the trustee 
   * approving a block that we will receive separately.
   *
   * When the application gets this, it will look at its unapporved_blocks
   * to see if it has the block signed by this message.
   * If the block is missing, it will call fetch_bodies() to retreive it,
   * and store off the signature in signatures_without_blocks to attach to the 
   * block when it comes in.
   * If we already have the block in unapproved_blocks, we remove it and attach 
   * the signature.
   * If it can be attached to the end of the block chain, we call push_block(), 
   * otherwise we insert it into approved_blocks.
   * 
   * @param approved_header the new signature approved by the trustee
   */
  virtual void on_trustee_approval(signed_header approved_header) override; // from blockchain_manager

		  
  /** called when the remote system sends us a list of headers it has seen
   * these could be newly-generated blocks or transactions.
   *
   * We will check our lists of transactions and blocks to see if we've ever
   * seen the corresponding body before.  If not, request it from the node
   * via fetch_body()
   *
   * @param headers the list of headers seen.
   */
  virtual void on_header_inventory(header_vector headers) override; // from blockchain_manager
		  


  /** returns a vector of bodies corresponding to the headers requested.
  * This may be called to provide block data to other peers syncing their blockchain to us,
  * to provide a newly-seen block that isn't signed and is not yet in the blockchain,
  * or to provide a transaction.
  *
  * Search through our lists of transactions and blocks (both approved and unapproved) and
  * the blockchain to find the data requested.  
  *
  * @param headers_of_bodies_requested the headers of the bodies that should be returned
  */
  virtual body_vector get_bodies(header_vector headers_of_bodies_requested) override; // from blockchain_manager
		  
  /** called when the remote system sends us a list of bodies we requested earlier
   *
   * If this body is a block:
   * - we check our list of signatures_without_a_block to see if it is signed.  
   *   - If it isn't, we store it in unapproved_blocks.
   *   - If it is, we see if it is the next block in the chain.  If it is then we call push_block() and
   *     remove the signature from signatures_without_a_block,
   *     otherwise we insert it and its signature into approved_blocks.
   * If this is a transaction:
   * - add it to uncommitted_transactions
   *   if we're mining, we'll do some transaction validation and add it to our merkle tree
   * 
   * @param bodies list of the bodies requested
   * @returns true if valid and node should broadcast, false if invalid and node layer should forget/discard
   */
  virtual bool on_requested_body(body requested_body) override; // from blockchain_manager

  /** Notifies the client that synchronization is complete.
   * The node considers us in the 'synchronizing' state as soon as we call fetch_signed_headers, 
   * and it will notify us via set_synchronization_state when we have the latest headers from
   * all connected nodes.
   * It can also kick us into the unsyncrhonized state if we lose network connection 
   * from all peers.
   */
  virtual void set_synchronization_state(bool is_synchronized) override;  // from blockchain_manager

// peer data is stored as metadata of the connection
struct peer_data
{
  set<header> inventory_peer_advertised_to_us;
  set<header> inventory_advertised_to_peer;
  map<header, time> bodies_requested_from_peer;  /// fetch from another peer if this peer disconnects
  optional<tuple<header, limit, time> > signed_headers_requested_from_peer; /// fetch from another peer if this peer disconnects or we timeout
};

class node
{
  set<connection> connections;

  /** bodies we've received and validated, but have not yet advertised to peers */
  vector<header> new_inventory;
private:
  header_list bodies_to_fetch;

  // syncrhonization state data
  bool _synchronizing;
  header _synchronization_start_header; /// header of last block on blockchain when synchronization started
  optional<tuple<header, limit> > _signed_headers_to_fetch;
  // end syncrhonization state data

  fetch_requested_bodies_task()
  {
    // loops forever (or smarter triggering mechanism)
    // for each item in bodies_to_fetch
    //   if there is a peer who isn't fetching anything for us *and* they have advertised the item we're looking for
    //      send them a fetch_bodies_msg to request the body
    //      move it to the bodies_requested_from_peer list for that peer, and timestamp the request
  }
  start_synchronizing()
  {
    // _synchronizing = true
    // 
    // for each connection
    //   set peer's in_sync to false
    //   send a fetch_signed_headers_msg for the first LIMIT headers after _synchronization_start_header
    //   set that peer's signed_headers_requested_from_peer
  }
  timeout_requested_bodies_task()
  {
    // loop over each connction/peer
    //   if any bodies_requested_from_peer or the signed_headers_requested_from_peer is older than our timeout value
    //     disconnect the peer, cleanup will happen in on_connection_disconnected
  }
  advertise_new_inventory_task()
  {
    // loop over each peer
    //   for each body in new_inventory
    //     if the connection metadata doesn't indicate that the was already notified about this body _and_ that peer doesn't already have that body (peer advertised it to us)
    //       send the header inventory message to that peer
    //       record the headers we sent in the connection metadata
    // clear new_inventory
  }
  on_connection_disconnected(connection&) delegate for each connection
  {
    // move any bodies_requested_from_peer back to our bodies_to_fetch list, so we will ask another peer
    // move any signed_headers_requested_from_peer for this peer back to our `signed_headers_to_fetch`
    // remove the connection from our `connections` list
  }
  on_connection_established(connection&) delegate for each connection
  {
    // if (_synchronizing)
    //   set peer's in_sync to false
    //   send a fetch_signed_headers_msg for first LIMIT headers after _synchronization_start_header
    //   set that peer's signed_headers_requested_from_peer
    // else if (!blockchain_manager->is_known_block_header(last_block_in_chain)) //  (last_block_in_chain was provided in initial connection message)
    //   start_synchronizing()
  }
public:
  void set_blockchain_manager(blockchain_manager*); 

  /** _synchronization_start_header = hash_of_last_header_seen
    * start_synchronizing();
    *
    * returns immediately to caller
    * later we'll call on_signed_header_inventory() in the blockchain_manager to provide the replies
    * @note the blockchain_manager may get multiple on_signed_header_inventory() calls in
    *       response to this, probably at least one per peer connected
    */
  virtual void fetch_signed_headers(header last_header_seen) = 0;
		
  /** triggered when we receive a fetch_signed_headers_msg from the remote peer
    * calls the blockchain_manager->get_signed_headers() to get the data
    * sends the response as a signed_header_inventory_msg
    */
  virtual void on_fetch_signed_headers_msg(header last_header_seen)
		  
  /** triggered when we receive a signed_header_inventory_msg from the remote peer
    * if (!headers_available.empty())
    *   call on_signed_header_inventory in the blockchain_manager
    * set the peers last_header_advertised property
    * if (headers_available.size() < LIMIT)
    *   set peer's in_sync property to true
    *   if all other peers are in sync, we're done synchronizing
    *     _synchronizing = false
    *     blockchain_manager->set_synchronization_state(true)
    * else
    *   send a fetch_signed_headers_msg for the next LIMIT headers after last_header_advertised
    *   set that peer's signed_headers_requested_from_peer
    */
  virtual void on_signed_header_inventory_msg(signed_header_vector headers_available) = 0;
		  
  /** triggered when we receive a trustee_approval_msg from the remote peer
    * call on_trustee_approval_msg in the blockchain_manager
    */
  virtual void on_trustee_approval(signed_header approved_header) = 0;
		  
  /** triggered when we receive a header_inventory_msg from the remote peer
    * call on_header_inventory in the blockchain_manager
    */
  virtual void on_header_inventory_msg(header_vector headers_available) = 0;
		  
  /** called by app.  fetches the bodies associated with the headers we just received
   * insert the headers into bodies_to_fetch, will be picked up by fetch_requested_bodies_task()
   * returns immediately to caller
   * @note later we'll call on_requested_bodies() in the blockchain_manager to provide the reply
   */
  virtual void fetch_bodies(header_vector headers_of_bodies_to_fetch) = 0;

  /** triggered when we receive a fetch_bodies_msg from the remote peer
    * calls the blockchain_manager->get_bodies() to get the block data
    * sends the response as a requested_bodies_msg (just a vector of bodies)
    */
  virtual void on_fetch_bodies_msg(header_vector) = 0;
		
  /** triggered when we receive a requested_bodies_msg from the remote peer
    * for each body in the body_vector
    *   remove the request from that peer's bodies_requested_from_peer
    *   remove the corresponding header from all peers' inventory_peer_advertised_to_us
    * 
    *   valid = blockchain_manager->on_requested_bodies()
    *   if (valid)
    *     new_inventory.push_back(this body)
    */
  virtual void on_requested_bodies_msg(body_vector) = 0;
}; // end class node

int main()
{
  client().run();
}

// some message data structures we haven't really discussed much yet:
struct hello_msg
{
  ip_address  local_ip_address;
  uint16_t    local_tcp_port_number;
  string      user_agent;
  int64_t     current_time;
  header      last_block_in_chain;  // used to see if the peer needs to sync to us
};
struct welcome_msg
{
  string      user_agent;
  int64_t     current_time;
  header      last_block_in_chain; // used to see if we need to sync to this peer
};
struct node_connection_info
{
  ip_address  local_ip_address;
  uint16_t    local_tcp_port_number;
  ip_address  global_ip_address;
  uint16_t    global_tcp_port_number;
};
struct go_away_msg
{
  uint32_t                     reason_code;
  vector<node_connection_info> try_these_instead;
};

struct header
{
  uint160t body_hash;		
};
typedef vector<header> header_vector;

struct header_inventory_msg
{
  vector<header> headers_available;
};

struct signed_header
{
  uint160t body_hash;
  uint160t trustee_signature;
};
typedef vector<signed_header> signed_header_vector;

struct signed_header_inventory_msg
{
  signed_header_vector signed_headers_available;
};
