(function() {
  describe("controller: HomeController", function() {
    beforeEach(function() {
      return module("app");
    });
    beforeEach(inject(function($q, $controller, $rootScope, $state, Wallet) {
      this.$rootScope = $rootScope;
      this.$state = $state;
      this.scope = this.$rootScope.$new();
      this.deferred = $q.defer();
      this.wallet = spyOn(Wallet, 'get_balance').andReturn(this.deferred.promise);
      return this.controller = $controller('HomeController', {
        $scope: this.scope,
        wallet: this.wallet
      });
    }));
    return it('should should transition to #home', function() {
      this.$state.transitionTo('home');
      this.deferred.resolve(111.11);
      this.$rootScope.$apply();
      expect(this.scope.balance).toBe(111.11);
      expect(this.$state.current.name).toBe('home');
      return expect(this.wallet).toHaveBeenCalled();
    });
  });

}).call(this);

(function() {
  describe("service: Wallet", function() {
    beforeEach(function() {
      return module("app");
    });
    beforeEach(inject(function($q, $rootScope, Wallet, RpcService, ErrorService) {
      this.Wallet = Wallet;
      this.rootScope = $rootScope;
      this.deferred = $q.defer();
      this.rpc = spyOn(RpcService, 'request').andReturn(this.deferred.promise);
      return this.error = spyOn(ErrorService, 'set');
    }));
    return describe("#create", function() {
      it("should process correct rpc response", function() {
        this.Wallet.create('test');
        this.deferred.resolve({
          result: true
        });
        this.rootScope.$apply();
        return expect(this.rpc).toHaveBeenCalledWith('wallet_create', ['default', 'test']);
      });
      return it("should process erorrneous rpc response", function() {
        this.Wallet.create('test');
        this.deferred.resolve({
          result: false
        });
        this.rootScope.$apply();
        expect(this.rpc).toHaveBeenCalledWith('wallet_create', ['default', 'test']);
        return expect(this.error).toHaveBeenCalledWith('Cannot create wallet, the wallet may already exist');
      });
    });
  });

}).call(this);
