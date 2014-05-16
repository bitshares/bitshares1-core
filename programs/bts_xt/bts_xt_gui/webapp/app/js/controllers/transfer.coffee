angular.module("app").controller "TransferController", ($scope, $location, $state, RpcService, InfoBarService) ->

  $scope.send = ->
    RpcService.request('sendtoaddress', [$scope.payto,$scope.amount,$scope.memo]).then (response) ->
      #$state.go("transactions")
      $scope.payto = ""
      $scope.amount = ""
      $scope.memo = ""
      InfoBarService.message = "Transaction broadcasted (#{response.result})"
