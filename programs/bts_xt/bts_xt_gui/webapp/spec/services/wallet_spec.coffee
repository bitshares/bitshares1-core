describe "service: Wallet", ->
  Given ->
    module("app")

  Given ->
    inject ($q, @Wallet, RpcService) =>
      deferred = $q.defer()
      @rpc = spyOn(RpcService, 'request').andReturn(deferred.promise)

  describe "#create", ->
    Given ->
      @password = "123"
    When ->
      @Wallet.create(@password)
    Then ->
      expect(@rpc).toHaveBeenCalledWith('wallet_create', ['default', @password])
