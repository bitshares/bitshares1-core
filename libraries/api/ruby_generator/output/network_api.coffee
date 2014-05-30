class Network

  constructor: (@q, @log, @rpc, @error_service) ->
    @log.info "---- Network API Constructor ----"


  # Attempts add or remove <node> from the peer list or try a connection to <node> once
  add_node: (node, command) ->
    @rpc.request('network_add_node', ['node', 'command']).then (response) ->
      response.result

  # Returns the number of fully-established connections to other nodes
  get_connection_count:  ->
    @rpc.request('network_get_connection_count').then (response) ->
      response.result

  # Returns data about each connected node
  get_peer_info:  ->
    @rpc.request('network_get_peer_info').then (response) ->
      response.result

  # Broadcast a previously-created signed transaction to the network
  broadcast_transaction: (transaction_to_broadcast) ->
    @rpc.request('network_broadcast_transaction', ['transaction_to_broadcast']).then (response) ->
      response.result

  # Sets advanced node parameters, used for setting up automated tests
  set_advanced_node_parameters: (params) ->
    @rpc.request('network_set_advanced_node_parameters', ['params']).then (response) ->
      response.result

  # Sets advanced node parameters, used for setting up automated tests
  get_advanced_node_parameters:  ->
    @rpc.request('network_get_advanced_node_parameters').then (response) ->
      response.result

  # Returns the time the transaction was first seen by this client
  get_transaction_propagation_data: (transaction_id) ->
    @rpc.request('network_get_transaction_propagation_data', ['transaction_id']).then (response) ->
      response.result

  # Returns the time the block was first seen by this client
  get_block_propagation_data: (block_hash) ->
    @rpc.request('network_get_block_propagation_data', ['block_hash']).then (response) ->
      response.result



angular.module("app").service("NetworkAPI", ["$q", "$log", "RpcService", "ErrorService", Network])
