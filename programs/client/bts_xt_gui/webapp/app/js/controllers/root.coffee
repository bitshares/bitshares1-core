angular.module("app").controller "RootController", ($scope, $location, $modal, $q, $http, $rootScope, ErrorService, InfoBarService) ->
  $scope.errorService = ErrorService
  $scope.infoBarService = InfoBarService

  open_wallet = (mode) ->
    $rootScope.cur_deferred = $q.defer()
    $modal.open
      templateUrl: "openwallet.html"
      controller: "OpenWalletController"
      resolve:
        mode: -> mode
    $rootScope.cur_deferred.promise

  $rootScope.open_wallet_and_repeat_request = (mode, request_data) ->
    deferred_request = $q.defer()
    #console.log "------ open_wallet_and_repeat_request #{mode} ------"
    open_wallet(mode).then ->
      #console.log "------ open_wallet_and_repeat_request #{mode} ------ repeat ---"
      $http(
        method: "POST",
        cache: false,
        url: '/rpc'
        data: request_data
      ).success((data, status, headers, config) ->
        #console.log "------ open_wallet_and_repeat_request  #{mode} ------ repeat success ---", data
        deferred_request.resolve(data)
      ).error((data, status, headers, config) ->
        deferred_request.reject()
      )
    deferred_request.promise

  $scope.wallet_action = (mode) ->
    open_wallet(mode)
