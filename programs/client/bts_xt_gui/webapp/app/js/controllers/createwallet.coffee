angular.module("app").controller "CreateWalletController", ($scope, $modal, $log, RpcService, ErrorService, Wallet) ->
  $scope.submitForm = (isValid) ->
    if true or isValid
      Wallet.create($scope.wallet_password, $scope.spending_password).success ->
        window.location.href = "/"
    else
      ErrorService.set "Please fill up the form below"
