angular.module("app").controller "TransferController", ($scope, $location, $state, RpcService, InfoBarService) ->

  $scope.send = ->
  	# $scope.memo was removed
    RpcService.request('wallet_transfer', [$scope.amount, $scope.payto, {"to_account":$scope.payto}]).then (response) ->
      #$state.go("transactions")
      $scope.payto = ""
      $scope.amount = ""
      $scope.memo = ""
      InfoBarService.message = "Transaction broadcasted (#{response.result})"
