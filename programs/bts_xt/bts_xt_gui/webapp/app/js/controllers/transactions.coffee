angular.module("app").controller "TransactionsController", ($scope, $location, RpcService) ->
  $scope.transactions = []

  fromat_address = (addr) ->
    return "-" if !addr or addr.length == 0
    res = ""
    angular.forEach addr, (a) ->
      res += ", " if res.length > 0
      res += a[1]
    res
  format_amount = (delta_balance) ->
    return "-" if !delta_balance or delta_balance.length == 0
    first_asset = delta_balance[0]
    return "-" if !first_asset or first_asset.length < 2
    first_asset[1]

  $scope.load_transactions = ->
    RpcService.request("rescan_state").then (response) ->
      RpcService.request("get_transaction_history").then (response) ->
        #console.log "--- transactions = ", response
        $scope.transactions.splice(0, $scope.transactions.length)
        angular.forEach response.result, (val) ->
          $scope.transactions.push
            block_num: val.block_num
            trx_num: val.trx_num
            time: val.confirm_time
            amount: format_amount(val.delta_balance)
            from: fromat_address(val.from)
            to: fromat_address(val.to)
            memo: val.memo

  $scope.load_transactions()

  $scope.rescan = ->
    $scope.load_transactions()
