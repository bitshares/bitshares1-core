angular.module("app").controller "OpenWalletController", ($scope, $modalInstance, RpcService, mode) ->
  console.log "OpenWalletController mode: #{mode}"
  if mode == "open_wallet"
    $scope.title = "Open Wallet"
    $scope.password_label = "Wallet Password"
    $scope.wrong_password_msg = "Wallet cannot be opened. Please check you password"
  else if mode == "unlock_wallet"
    $scope.title = "Unlock Wallet"
    $scope.password_label = "Spending Password"
    $scope.wrong_password_msg = "Wallet cannot be unlocked. Please check you password"

  open_wallet_request = ->
    RpcService.request('wallet_open', ['walleta', $scope.password]).then (response) ->
      if response.result
        $modalInstance.close("ok")
        $scope.cur_deferred.resolve()
      else
        $scope.password_validation_error()
        $scope.cur_deferred.resolve("invalid password")
    ,
    (reason) ->
      $scope.password_validation_error()
      $scope.cur_deferred.reject(reason)

  unlock_wallet_request = ->
    RpcService.request('walletpassphrase', [$scope.password, 60 * 1000000]).then (response) ->
      $modalInstance.close("ok")
      $scope.cur_deferred.resolve()
    ,
    (reason) ->
      $scope.password_validation_error()
      $scope.cur_deferred.reject(reason)

  $scope.has_error = false
  $scope.ok = ->
    if mode == "open_wallet" then open_wallet_request() else unlock_wallet_request()

  $scope.password_validation_error = ->
    $scope.password = ""
    $scope.has_error = true

  $scope.cancel = ->
    $modalInstance.dismiss "cancel"

  $scope.$watch "password", (newValue, oldValue) ->
    $scope.has_error = false if newValue != ""