app = angular.module("app", ["ngResource", "ui.router", "ui.bootstrap", "app.services", "app.directives"])

app.config ($stateProvider, $urlRouterProvider) ->
  $urlRouterProvider.otherwise('/home')

  home =
    name: 'home'
    url: '/home'
    templateUrl: "home.html"
    controller: "HomeController"

  receive =
    name: 'receive'
    url: '/receive'
    templateUrl: "receive.html"
    controller: "ReceiveController"

  transfer =
    name: 'transfer'
    url: '/transfer'
    templateUrl: "transfer.html"
    controller: "TransferController"

  transfer =
    name: 'transfer'
    url: '/transfer'
    templateUrl: "transfer.html"
    controller: "TransferController"

  transactions =
    name: 'transactions'
    url: '/transactions'
    templateUrl: "transactions.html"
    controller: "TransactionsController"

  blocks =
    name: 'blocks'
    url: '/blocks'
    templateUrl: "blocks.html"
    controller: "BlocksController"

  createwallet =
    name: 'createwallet'
    url: '/createwallet'
    templateUrl: "createwallet.html"
    controller: "CreateWalletController"

  $stateProvider.state(home).state(receive).state(transfer)
    .state(transactions).state(blocks).state(createwallet)

