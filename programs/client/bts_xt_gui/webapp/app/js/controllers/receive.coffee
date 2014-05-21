angular.module("app").controller "ReceiveController", ($scope, $location, RpcService, InfoBarService) ->
  $scope.new_address_label = ""
  $scope.addresses = []
  $scope.pk_label = ""
  $scope.pk_value = ""
  $scope.wallet_file = ""
  $scope.wallet_password = ""

  refresh_addresses = ->
    RpcService.request('wallet_list_receive_accounts').then (response) ->
      $scope.addresses.splice(0, $scope.addresses.length)
      angular.forEach response.result, (val) ->
        $scope.addresses.push({label: val[1], address: val[0]})

  refresh_addresses()

  $scope.create_address = ->
    RpcService.request('wallet_get_account', [$scope.new_address_label]).then (response) ->
      $scope.new_address_label = ""
      refresh_addresses()

  $scope.import_key = ->
    RpcService.request('wallet_import_private_key', [$scope.pk_value, $scope.pk_label]).then (response) ->
      $scope.pk_value = ""
      $scope.pk_label = ""
      InfoBarService.message = "Your private key was successfully imported."
      refresh_addresses()

  $scope.import_wallet = ->
    RpcService.request('wallet_import_bitcoin', [$scope.wallet_file,$scope.wallet_password]).then (response) ->
      $scope.wallet_file = ""
      $scope.wallet_password = ""
      InfoBarService.message = "The wallet was successfully imported."
      refresh_addresses()
