describe "service: Wallet", ->

  beforeEach -> module("app")

  beforeEach inject ($q, $rootScope, @Wallet, RpcService, ErrorService) ->
    @rootScope = $rootScope
    @deferred = $q.defer()
    @rpc = spyOn(RpcService, 'request').andReturn(@deferred.promise)
    @error = spyOn(ErrorService, 'set')

  describe "#create", ->

    it "should process correct rpc response", ->
      @Wallet.create 'test'
      @deferred.resolve {result: true}
      @rootScope.$apply()
      expect(@rpc).toHaveBeenCalledWith('wallet_create', ['default', 'test'])

    it "should process erorrneous rpc response", ->
      @Wallet.create 'test'
      @deferred.resolve {result: false}
      @rootScope.$apply()
      expect(@rpc).toHaveBeenCalledWith('wallet_create', ['default', 'test'])
      expect(@error).toHaveBeenCalledWith('Cannot create wallet, the wallet may already exist')

