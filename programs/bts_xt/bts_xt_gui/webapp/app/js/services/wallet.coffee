class Wallet

  constructor: (@q, @rpc, @error_service) ->
    console.log "---- Wallet Constructor ----"
    @wallet_name = ""

  create: (wallet_password, spending_password) ->
    @rpc.request('wallet_create', ['default', wallet_password]).then (response) =>
      if response.result == true
        window.location.href = "/"
      else
        error = "Cannot create wallet, the wallet may already exist"
        @error_service.set error
        @q.reject(error)

  get_balance: ->
    @rpc.request('wallet_get_balance').then (response) ->
      response.result.amount

  get_wallet_name: ->
    @rpc.request('wallet_get_name').then (response) =>
      @wallet_name = response.result
      console.log "---- current wallet name: ", response.result


angular.module("app").service("Wallet", ["$q", "RpcService", "ErrorService", Wallet])
