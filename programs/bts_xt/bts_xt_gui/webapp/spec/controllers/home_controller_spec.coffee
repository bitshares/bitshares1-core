describe "controller: HomeController", ->

  beforeEach -> module("app")

  beforeEach inject ($q, $controller, @$rootScope, @$state, Wallet) ->
    @scope  = @$rootScope.$new()
    @deferred = $q.defer()
    @wallet = spyOn(Wallet, 'get_balance').andReturn(@deferred.promise)
    @controller = $controller('HomeController', {$scope: @scope, @wallet})

  it 'should should transition to #home', ->
    @$state.transitionTo('home')
    @deferred.resolve 111.11
    @$rootScope.$apply()
    expect(@scope.balance).toBe(111.11)
    expect(@$state.current.name).toBe('home')
    expect(@wallet).toHaveBeenCalled()

