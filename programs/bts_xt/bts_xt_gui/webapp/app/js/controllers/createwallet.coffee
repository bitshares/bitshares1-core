angular.module("app").controller "CreateWalletController", ($scope, $modal, $log, RpcService, ErrorService) ->
  $scope.submitForm = (isValid) ->
    unless isValid
      ErrorService.setError "Please fill up the form below"
      return

    RpcService.request('create_wallet', ['default', $scope.wallet_password, $scope.spending_password]).then (response) ->
      if response.result == true
        window.location.href = "/"
      else
        console.log "Error in create_wallet", result
        ErrorService.setError "Cannot create wallet, the wallet may already exist"
