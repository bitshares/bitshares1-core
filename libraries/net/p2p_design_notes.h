class client : public node_delegate
{
public:
  /** returns a vector of up to `limit` item hashes from the local blockchain database to the caller
   * @param from_id item_id of the last item seen.  The application should return
   *                 a block of item starting immeditaely after from_id
   */
  virtual std::vector<item_hash_t> get_item_ids(const item_id& from_id, 
                                                uint32_t limit = 2000) override; // from node_delegate

  /** Returns true if the item_id refers to a item we know about.  It could 
   * be anywhere in our blockchain database, our list of unapproved blocks, 
   * transactions or signatures
   */
  virtual bool has_item(const item_id& item_id_to_check) override; // from node_delegate
		  	  
  /** returns a single item corresponding to the item_id requested.
  * This may be called to provide block data to other peers syncing their blockchain to us,
  * to provide a newly-seen block that isn't signed and is not yet in the blockchain,
  * or to provide a transaction.
  *
  * Search through our lists of transactions and blocks and the blockchain to find the 
  * data requested.  
  *
  * @param id the id of the item that should be returned
  */
  virtual message get_item(const item_id& item_id_to_get) override; // from node_delegate
		  
  /** called when a peer system sends us an new unseen item
   *
   * If this message contains a block:
   * - we check our list of signatures_without_a_block to see if it is signed.  
   *   - If it is signed:
   *       verify that it is a potential next block.  If it is, push to blockchain, if not, throw
   *   - if it's not signed:
   *       verify that it is a potential next block.  if it is, add it to our list of unapproved_blocks.  If not , throw
   * If this is a transaction:
   * - verify that it's a valid transaction.  throw if not.
   *   add it to uncommitted_transactions
   *   if we're mining, we'll do some transaction validation and add it to our merkle tree
   * - if it's a signature.  check the signature, throw if invalid
   *   - if it signs one of our unapproved_blocks.  
   *     then push that block to the blockchain and clear any other unapproved_blocks
   *     else add to signatures_without_a_block 
   * 
   * @param message the message from a peer
   * @note this should throw if the message is invalid, will cause the originating peer to be disconnected
   */
  virtual void handle_message(const message& message_to_handle) override; // from node_delegate

  /** This is called when the node has new information about the synchronization status
   * In particular, 
   *   after a call to sync_from():
   *     the node_delegate will get a call to sync_status() with count_of_unsynchronized_items=0 if
   *     the node is already synchronized, or with count_of_unsynchronized_items > 0 if the node
   *     is out of sync.  If the node is out of sync, the node should store and track its 
   *     own _count_of_unsynchronized_items, decrementing it when it pushes a new block
   *     onto the blockchain.  When its _count_of_unsynchronized_items reaches zero, it
   *     is in sync.  The node will not provide any special notification that synchronization
   *     has been achieved.
   *     During a long sync, it is likely that the node will discover blocks generated during
   *     the sync, and will call sync_status() to update the client's count. 
   *   during normal operation:
   *     The delegate may get a sync_status call during normal operation to notify the
   *     client that it has become aout of sync.  This can happen after being disconnected
   *     from some or all peers (either we were disconnected, or there was a net split that
   *     has now reunified).  Just like the case where the sync was initiated by the client,
   *     the delegate can now expect the next count_of_unsynchronized_items to be the missing
   *     blocks required to resynch.
   *     
   *  @param item_type the type of the item we're synchronizing, will be the same as item passed to the sync_from() call
   *  @param item_count the number of items known to the node that haven't been sent to handle_message() yet.
   *                    After `item_count` more calls to handle_message(), the node will be in sync
   */
  virtual void sync_status(uint32_t item_type, uint32_t count_of_unsynchronized_items) override; // from node_delegate

  /**
   *  Call any time the number of connected peers changes.
   */
  virtual void connection_count_changed(uint32_t new_connection_count) override; // from node_delegate
}; // end class node_delegate

// peer data is stored as metadata of the connection
struct peer_data
{
  // synch stuff
  deque<item_hash_t>  ids_of_items_to_get;           // id of items in the blockchain that this peer has told us about
  uint32_t            number_of_unfetched_item_ids;  // number of items in the blockchain that follow ids_of_items_to_get but the peer hasn't yet told us their ids
  bool peer_needs_sync_items_from_us;
  bool we_need_sync_items_from_peer;
  optional<tuple<item_id, time> > item_ids_requested_from_peer; /// we check this to detect a timed-out request and in busy()

  // non-synch stuff
  set<item_id> inventory_peer_advertised_to_us;
  set<item_id> inventory_advertised_to_peer;

  map<item_id, time> items_requested_from_peer;  /// fetch from another peer if this peer disconnects
  bool busy() { return !items_requested_from_peer.empty() || item_ids_requested_from_peer; }
  bool idle() { return !busy(); }

  enum negotiation_states
  {
    disconnected,
    connected,
    hello_sent,
    hello_received,
    hello_reply_sent,
    hello_reply_received,

};

class node
{
  set<connection> connections; // fully established peers
  set<connection> negotiating_peers; // peers we're trying to connect to or are still handshaking with

  /** messages we've received and validated, but have not yet advertised to peers */
  set<item_id> _new_inventory;
private:
  queue<item_id> _items_to_fetch;

  // syncrhonization state data
  bool _synchronizing;
  item_id _last_block_in_our_chain; /// id of the last block on blockchain (updated each time we receive a valid block)
  set<block_message> _received_sync_items; // items waiting to be processed once we get their predecessors
  uint32_t _total_number_of_unfetched_items;
  // end syncrhonization state data

  set<item_hash_t> _active_requests; // requests sent out to peers already


  fetch_next_batch_of_item_ids_from_peer(peer* , item_id last_item_id_seen)
  {
    // send a fetch_blockchain_item_ids_message for the next batch of item_ids after last_item_id_seen
    // set peer_data->item_ids_requested_from_peer = make_tuple(last_item_id_seen, now());
  }

  request_item_from_peer(peer* peer_to_request_from, const item_id& item_to_request) 
  {
    peer_to_request_from->send(fetch_item_message(item_to_request));
    _active_requests.insert(item_id.item_hash);
    peer_data[peer_to_request_from]->items_requested_from_peer[item_id] = now();
  }

  fetch_requested_items_task()
  {
    // loops forever (or smarter triggering mechanism)
    // for each item in _items_to_fetch
    //   if there is a peer who isn't fetching anything for us *and* they have advertised the item we're looking for
    //      send them a fetch_item_message to request the item
    //      move it to the items_requested_from_peer list for that peer, and timestamp the request
    //      insert it into _active_requests
  }
  start_synchronizing()
  {
    // for each connection
    //   set peer's in_sync to false
    //   send a fetch_item_ids_message for the first LIMIT item after _last_block_in_our_chain
    //   set that peer's item_ids_requested_from_peer
    // start up the request_blocks_during_sync_loop() task
  }
  void request_blocks_during_sync_loop()
  {
    loop forever:
      wait until we get/valdiate a block, or get a bunch of item_ids
      for each peer:
        if !peer->busy():
          for (unsigned i = 0; i < peer->items_to_get.size(); ++i)
          {
            if (peer->items_to_get[i] not in active_requests_set AND
                peer->items_to_get[i] not in received_item_set)
            {
              request_item_from_peer(peer, items_to_get[i]);
              break;
            }
          }
  }
  void process_block_during_sync(peer* originating_peer, block_message new_block_message)
  {
    bool potential_first_block = false;
    for each peer:
      if new_block_message->block_id == peer->items_to_get[0]:
        potential_first_block = true;
        break;

    if (potential_first_block):
      try:
        _node_delegate->handle_message(new_block)// throws on invalid block
        --_total_number_of_unfetched_items;
        for each peer where peer->we_need_sync_items_from_peer:
          if new_block_message->block_id == peer->items_to_get[0]:
            peer->items_to_get.pop_front();
            if (peer->items_to_get.empty() && peer->number_of_unfetched_item_ids == 0)
              fetch_next_batch_of_item_ids_from_peer(new_block_message->block_id) // if peer returns no items, we will start receiving inventory messages from the peer
          else
            peer->disconnect(peer_sent_us_bad_data)
        originating_peer->items_requested_from_peer.erase(new_block_message->block_id)
      catch:
        originating_peer->disconnect(peer_sent_us_bad_data);
    else:
      _received_sync_items.insert(new_block_message)
  }
  
  on_trx_message()
  {
  }
  on_block_message(peer* originating_peer, block_message new_block_message)
  {
    if (originating_peer->we_need_sync_items_from_peer)
      process_block_during_sync(originating_peer, new_block_message);
    else
    {
      // the only reason we're getting a block is if we requested it from a peer's inventory
      if not in originating_peer->items_requested_from_peer:
        originating_peer->disconnect(peer_sent_us_bad_data);
        return;
      for each peer:
        remove from that peer's items_requested_from_peer // probably never happens
      try
        _node_delegate->handle_message(new_block_message);
        _new_inventory.insert(new_block_message->block_id) // advertise_new_inventory_task will take it from here
        _last_block_in_our_chain = new_block_message.block_id
      catch:
        originating_peer->disconnect(peer_sent_us_bad_data);
    }
  }

  /** the entry point where we start processing any message received from a peer */
  void on_message(peer* originating_peer, const message& received_message)
  {
    message_hash = hash(received_message);
    if (received_message.type != item_ids_inventory_message::type)
    {
      if (not in _active_requests)
      {
        originating_peer->disconnect(peer_sent_us_bad_data);
        return;
      }
      else
      {
        remove from _active_requests
        remove from originating_peer's items_requested_from_peer
      }
    }

    switch (received_message.type)
    {
    case block_message::type:
      on_block_message(originating_peer, received_message.as<block_message>());
      break;
    case item_ids_inventory_message::type:
      on_item_ids_inventory_message(received_message.as<>());
      break;

    case trx_message::type: /* FALL THROUGH */
    case signature_message::type: /* FALL THROUGH */
    default:
      {
        assert(!originating_peer->we_need_sync_items_from_peer);      
        try
        {
          _node_delegate->handle_message(received_message);
          _new_inventory.push_back(received_message);
        }
        catch (...)
        {
          originating_peer->disconnect(peer_sent_us_bad_data);
          return;
        }
      }
    }
  }
  timeout_requested_items_task()
  {
    // loop over each connction/peer
    //   if any items_requested_from_peer is older than our timeout value
    //     peer->disconnect(peer_time_out)
    //     cleanup will happen in on_connection_disconnected
  }
  advertise_new_inventory_task()
  {
    // loop over each peer
    //   if (!peer->peer_needs_sync_items_from_us)
    //     for each item in new_inventory
    //       if not in peer's inventory_peer_advertised_to_us and not in inventory_advertised_to_peer
    //         send the item_id_inventory_message to that peer (note, will send one message per item type)
    //         record the item_ids we sent in inventory_advertised_to_peer
    //   else:
    //     TODO: we do not send inventory to peers who are syncing to us,
    //           but do we need to save the latest inventory to send to them once
    //           they exit sync? 
    // clear new_inventory
  }
  on_connection_disconnected(connection&) delegate for each connection
  {
    // move any items_requested_from_peer back to our _items_to_fetch list, so we will ask another peer
    // remove the connection from our `connections` list
    // _total_number_of_unfetched_items = calculate_unsynced_block_count_from_all_peers()
  }
  on_connection_established(connection&) delegate for each connection
  {
    // peer->peer_needs_sync_items_from_us = true
    // peer->we_need_sync_items_from_peer = true
    // fetch_next_batch_of_item_ids_from_peer(peer, _last_block_in_our_chain)
  }

  /** triggered during a peer's sync, when we receive a fetch_blockchain_item_ids_message from the 
    * remote peer.
    * int total_remaining_items;
    * vector<item_hash_t> items_to_send_peer = node_delegate->get_item_ids(last_item_seen,total_remaining_items) to get the data
    * if (items_to_send_peer.empty())
    *   originating_peer->peer_needs_sync_items_from_us = false;
    * send items_to_send_peer and total_remaining_items packed into a blockchain_item_ids_inventory_message
    */
  void on_fetch_blockchain_item_ids_message(item_id last_item_seen)

  uint32_t calculate_unsynced_block_count_from_all_peers()
  {
    uint32_t max_number_of_unfetched_items = 0;
    for each peer:
      uint32_t this_peer_number_of_unfetched_items = peer->ids_of_items_to_get.size() + peer->number_of_unfetched_item_ids;
      max_number_of_unfetched_items = max(max_number_of_unfetched_items,
                                          this_peer_number_of_unfetched_items)
    return max_number_of_unfetched_items;
  }

  /** triggered during our sync, when we receive a item_ids_inventory_message from the remote peer
    * if (total_remaining_item_count == 0 and item_hashes_available.empty() and
    *     originating_peer->ids_of_items_to_get.empty() and originating_peer->number_of_unfetched_item_ids == 0)
    *   originating_peer->we_need_sync_items_from_peer = false;
    *   return
    * set peer's number_of_unfetched_item_ids = total_remaining_item_count
    * if peer's ids_of_items_to_get.empty():
    *   if item_hashes_available[0] is not ids_of_items_to_get[0] for any peer who has returned item_ids to us
    *     ask _node_delegate->has_item(item_hashes_available[0]) and pop until it returns false
    * append whatever remains to peer's ids_of_items_to_get
    * uint32_t new_number_of_unfetched_items = calculate_unsynced_block_count_from_all_peers()
    * if (new_number_of_unfetched_items != _total_number_of_unfetched_items)
    *   _node_delegate->sync_status(block_type, new_number_of_unfetched_items);
    *   _total_number_of_unfetched_items = new_number_of_unfetched_items
    * else if (new_number_of_unfetched_items == 0)
    *   _node_delegate->sync_status(block_type, 0); // tell client we're synced
    * if (total_remaining_item_count != 0)
    *   fetch_next_batch_of_item_ids_from_peer(this_peer, item_id(_last_block_in_our_chain.item_type, ids_of_items_to_get.back()))
    */
  void on_blockchain_item_ids_inventory_message(peer* originating_peer,
                                                uint32_t total_remaining_item_count,
                                                uint32_t item_type,
                                                std::vector<item_hash_t> item_hashes_available);

  /** called during normal synchronized operation when a peer sends us an
   * item_ids_inventory_message to offer us items it thinks we don't know about
   *
   * insert into peer's inventory_peer_advertised_to_us
   * for each item_id in item_hashes_available
   *   if it's not in any of our peers' inventory_advertised_to_peer and it's not in _active_requests
   *     then add it to _items_to_fetch
   */
  void on_item_ids_inventory_message(uint32_t item_type,
                                     std::vector<item_hash_t> item_hashes_available);
		  		  
  /** triggered when we receive a fetch_item_message from the remote peer
    * calls the node_delegate->get_item() to get the item's data.
    * sends the response as a message of the appropriate type
    */
  void on_fetch_item_message(const item_id& item_to_fetch);

  initiate_connection()
  {
    c
  }

  void connect_to_peers_task()
  {
    // while ((number of active peers  plus
    //         number of connections we're trying to establish < our target value) and
    //        we still know of more peers we could try connecting to)
    //   pick a peer from the potential_peer_db
    //     initiate_connection(ep)
  }

public:
  void set_delegate(node_delegate* delegate);
  /** Add endpoint to internal level_map database of potential nodes
   *   to attempt to connect to.  This database is consulted any time
   *   the number connected peers falls below the target. */
  void add_node(const fc::ip::endpoint& ep)
  {
    potential_peer_db->add(ep);
  }

  /** Attempt to connect to the specified endpoint immediately. */
  void connect_to(const fc::ip::endpoint& ep)
  {
    potential_peer_db->add(ep);
    if (too many connections)
      kill_one_connection_at_random()
    initiate_connection(ep)
  }

  /** Specifies the network interface and port upon which incoming connections should be accepted. */
  void listen_on_endpoint(const fc::ip::endpoint& ep);

  /** @return a list of peers that are currently connected. */
  std::vector<peer_status> get_connected_peers() const;

  /** Add message to outgoing inventory list, notify peers that I have a message ready.
   * item_id id_for_new_item(item_to_broadcast.type, hash(item_to_broadcast))
   * _new_inventory.insert(id_for_new_item) // advertise_new_inventory() will grab it from here
   */
  void broadcast(const message& item_to_broadcast);

  /** _last_block_in_our_chain = last_item_seen
    * start_synchronizing();
    *
    * returns immediately to caller
    * this will result in:
    *   sync_status()
    *   0 or more handle_message() calls in the blockchain_manager to provide the items (blocks)
    */
  void sync_from(const item_id& last_item_seen);
}; // end class node

int main()
{
  client().run();
}

// some message data structures we haven't really discussed much yet:
struct hello_message
{
  ip_address  local_ip_address;
  uint16_t    local_tcp_port_number;
  string      user_agent;
  int64_t     current_time;
};
struct node_connection_info
{
  ip_address  local_ip_address;
  uint16_t    local_tcp_port_number;
  ip_address  global_ip_address;
  uint16_t    global_tcp_port_number;
};
struct go_away_message
{
  uint32_t                     reason_code;
  vector<node_connection_info> try_these_instead;
};

/*
 Peer database rules:
 - each node broadcasts its timestamped addresses in a message once a day
   these messages are relayed to a few peers
 - we remember peers forever
 - we sort them according to the time they were last seen
 - when we start up, we try to connect to the most recently-connected
   peers first
 - whenever we disconnect from a peer, we update its timestamp
 - whenever we do some operation that requires us to sort the database,
   we update all connected peers with the current timestamp first
 - whenever we get list of addresses from one of our peers, we update
   our database with the provided timestamps (after some sanity-checking)
 - if we disconnect from a peer because it sent us invalid data, we 
   will blacklist it (move it to the bottom of the list, avoid advertising
   it to peers) for a certain longish amount of time.
 - if we connect to a peer and it is too busy to add us as a peer, we 
   will not try it again for some shorter amount of time
 */

// peer database contains
struct potential_peer_data
{
  vector<fc::endpoint>     peer_addresses;
  fc::timepoint            last_seen;
  enum connection_history_type { never_connected, connected_in_past, blocked };
  connection_history_type  connection_history;
};
level_db<potential_peer_data> potential_peer_db;