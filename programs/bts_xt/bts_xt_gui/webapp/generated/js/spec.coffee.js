(function() {
  describe("service: Wallet", function() {
    Given(function() {
      return module("app");
    });
    Given(function() {
      var _this = this;
      return inject(function($q, Wallet, RpcService) {
        var deferred;
        _this.Wallet = Wallet;
        deferred = $q.defer();
        return _this.rpc = spyOn(RpcService, 'request').andReturn(deferred.promise);
      });
    });
    return describe("#create", function() {
      Given(function() {
        return this.password = "123";
      });
      When(function() {
        return this.Wallet.create(this.password);
      });
      return Then(function() {
        return expect(this.rpc).toHaveBeenCalledWith('wallet_create', ['default', this.password]);
      });
    });
  });

}).call(this);
