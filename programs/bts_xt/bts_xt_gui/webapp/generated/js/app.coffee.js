(function() {
  var app;

  app = angular.module("app", ["ngResource", "ui.router", "ui.bootstrap", "app.services", "app.directives"]);

  app.config(function($stateProvider, $urlRouterProvider) {
    var blocks, createwallet, home, receive, transactions, transfer;
    $urlRouterProvider.otherwise('/home');
    home = {
      name: 'home',
      url: '/home',
      templateUrl: "home.html",
      controller: "HomeController"
    };
    receive = {
      name: 'receive',
      url: '/receive',
      templateUrl: "receive.html",
      controller: "ReceiveController"
    };
    transfer = {
      name: 'transfer',
      url: '/transfer',
      templateUrl: "transfer.html",
      controller: "TransferController"
    };
    transfer = {
      name: 'transfer',
      url: '/transfer',
      templateUrl: "transfer.html",
      controller: "TransferController"
    };
    transactions = {
      name: 'transactions',
      url: '/transactions',
      templateUrl: "transactions.html",
      controller: "TransactionsController"
    };
    blocks = {
      name: 'blocks',
      url: '/blocks',
      templateUrl: "blocks.html",
      controller: "BlocksController"
    };
    createwallet = {
      name: 'createwallet',
      url: '/createwallet',
      templateUrl: "createwallet.html",
      controller: "CreateWalletController"
    };
    return $stateProvider.state(home).state(receive).state(transfer).state(transactions).state(blocks).state(createwallet);
  });

}).call(this);

(function() {
  angular.module("app").controller("BlocksController", function($scope, $location) {});

}).call(this);

(function() {
  angular.module("app").controller("CreateWalletController", function($scope, $modal, $log, RpcService, ErrorService) {
    return $scope.submitForm = function(isValid) {
      if (!isValid) {
        ErrorService.setError("Please fill up the form below");
        return;
      }
      return RpcService.request('create_wallet', ['default', $scope.wallet_password, $scope.spending_password]).then(function(response) {
        if (response.result === true) {
          return window.location.href = "/";
        } else {
          console.log("Error in create_wallet", result);
          return ErrorService.setError("Cannot create wallet, the wallet may already exist");
        }
      });
    };
  });

}).call(this);

(function() {
  angular.module("app").controller("HomeController", function($scope, $modal, $log, RpcService) {
    var format_amount, fromat_address, load_transactions;
    $scope.transactions = [];
    $scope.balance = 0.0;
    fromat_address = function(addr) {
      var res;
      if (!addr || addr.length === 0) {
        return "-";
      }
      res = "";
      angular.forEach(addr, function(a) {
        if (res.length > 0) {
          res += ", ";
        }
        return res += a[1];
      });
      return res;
    };
    format_amount = function(delta_balance) {
      var first_asset;
      if (!delta_balance || delta_balance.length === 0) {
        return "-";
      }
      first_asset = delta_balance[0];
      if (!first_asset || first_asset.length < 2) {
        return "-";
      }
      return first_asset[1];
    };
    load_transactions = function() {
      return RpcService.request("get_transaction_history").then(function(response) {
        var count;
        $scope.transactions.splice(0, $scope.transactions.length);
        count = 0;
        return angular.forEach(response.result, function(val) {
          count += 1;
          if (count < 10) {
            return $scope.transactions.push({
              block_num: val.block_num,
              trx_num: val.trx_num,
              time: val.confirm_time,
              amount: format_amount(val.delta_balance),
              from: fromat_address(val.from),
              to: fromat_address(val.to),
              memo: val.memo
            });
          }
        });
      });
    };
    return RpcService.request("rescan_state").then(function(response) {
      return RpcService.request('getbalance').then(function(response) {
        console.log("balance: ", response.result.amount);
        $scope.balance = response.result.amount;
        return load_transactions();
      });
    });
  });

}).call(this);

(function() {
  angular.module("app").controller("OpenWalletController", function($scope, $modalInstance, RpcService, mode) {
    var open_wallet_request, unlock_wallet_request;
    console.log("OpenWalletController mode: " + mode);
    if (mode === "open_wallet") {
      $scope.title = "Open Wallet";
      $scope.password_label = "Wallet Password";
      $scope.wrong_password_msg = "Wallet cannot be opened. Please check you password";
    } else if (mode === "unlock_wallet") {
      $scope.title = "Unlock Wallet";
      $scope.password_label = "Spending Password";
      $scope.wrong_password_msg = "Wallet cannot be unlocked. Please check you password";
    }
    open_wallet_request = function() {
      return RpcService.request('wallet_open', ['walleta', $scope.password]).then(function(response) {
        if (response.result) {
          $modalInstance.close("ok");
          return $scope.cur_deferred.resolve();
        } else {
          $scope.password_validation_error();
          return $scope.cur_deferred.resolve("invalid password");
        }
      }, function(reason) {
        $scope.password_validation_error();
        return $scope.cur_deferred.reject(reason);
      });
    };
    unlock_wallet_request = function() {
      return RpcService.request('walletpassphrase', [$scope.password, 60 * 1000000]).then(function(response) {
        $modalInstance.close("ok");
        return $scope.cur_deferred.resolve();
      }, function(reason) {
        $scope.password_validation_error();
        return $scope.cur_deferred.reject(reason);
      });
    };
    $scope.has_error = false;
    $scope.ok = function() {
      if (mode === "open_wallet") {
        return open_wallet_request();
      } else {
        return unlock_wallet_request();
      }
    };
    $scope.password_validation_error = function() {
      $scope.password = "";
      return $scope.has_error = true;
    };
    $scope.cancel = function() {
      return $modalInstance.dismiss("cancel");
    };
    return $scope.$watch("password", function(newValue, oldValue) {
      if (newValue !== "") {
        return $scope.has_error = false;
      }
    });
  });

}).call(this);

(function() {
  angular.module("app").controller("ReceiveController", function($scope, $location, RpcService, InfoBarService) {
    var refresh_addresses;
    $scope.new_address_label = "";
    $scope.addresses = [];
    $scope.pk_label = "";
    $scope.pk_value = "";
    $scope.wallet_file = "";
    $scope.wallet_password = "";
    refresh_addresses = function() {
      return RpcService.request('list_receive_addresses').then(function(response) {
        $scope.addresses.splice(0, $scope.addresses.length);
        return angular.forEach(response.result, function(val) {
          return $scope.addresses.push({
            label: val[1],
            address: val[0]
          });
        });
      });
    };
    refresh_addresses();
    $scope.create_address = function() {
      return RpcService.request('getnewaddress', [$scope.new_address_label]).then(function(response) {
        $scope.new_address_label = "";
        return refresh_addresses();
      });
    };
    $scope.import_key = function() {
      return RpcService.request('import_private_key', [$scope.pk_value, $scope.pk_label]).then(function(response) {
        $scope.pk_value = "";
        $scope.pk_label = "";
        InfoBarService.message = "Your private key was successfully imported.";
        return refresh_addresses();
      });
    };
    return $scope.import_wallet = function() {
      return RpcService.request('import_wallet', [$scope.wallet_file, $scope.wallet_password]).then(function(response) {
        $scope.wallet_file = "";
        $scope.wallet_password = "";
        InfoBarService.message = "The wallet was successfully imported.";
        return refresh_addresses();
      });
    };
  });

}).call(this);

(function() {
  angular.module("app").controller("RootController", function($scope, $location, $modal, $q, $http, $rootScope, ErrorService, InfoBarService) {
    var open_wallet;
    $scope.errorService = ErrorService;
    $scope.infoBarService = InfoBarService;
    open_wallet = function(mode) {
      $rootScope.cur_deferred = $q.defer();
      $modal.open({
        templateUrl: "openwallet.html",
        controller: "OpenWalletController",
        resolve: {
          mode: function() {
            return mode;
          }
        }
      });
      return $rootScope.cur_deferred.promise;
    };
    $rootScope.open_wallet_and_repeat_request = function(mode, request_data) {
      var deferred_request;
      deferred_request = $q.defer();
      open_wallet(mode).then(function() {
        return $http({
          method: "POST",
          cache: false,
          url: '/rpc',
          data: request_data
        }).success(function(data, status, headers, config) {
          return deferred_request.resolve(data);
        }).error(function(data, status, headers, config) {
          return deferred_request.reject();
        });
      });
      return deferred_request.promise;
    };
    return $scope.wallet_action = function(mode) {
      return open_wallet(mode);
    };
  });

}).call(this);

(function() {
  angular.module("app").controller("TransactionsController", function($scope, $location, RpcService) {
    var format_amount, fromat_address;
    $scope.transactions = [];
    fromat_address = function(addr) {
      var res;
      if (!addr || addr.length === 0) {
        return "-";
      }
      res = "";
      angular.forEach(addr, function(a) {
        if (res.length > 0) {
          res += ", ";
        }
        return res += a[1];
      });
      return res;
    };
    format_amount = function(delta_balance) {
      var first_asset;
      if (!delta_balance || delta_balance.length === 0) {
        return "-";
      }
      first_asset = delta_balance[0];
      if (!first_asset || first_asset.length < 2) {
        return "-";
      }
      return first_asset[1];
    };
    $scope.load_transactions = function() {
      return RpcService.request("rescan_state").then(function(response) {
        return RpcService.request("get_transaction_history").then(function(response) {
          $scope.transactions.splice(0, $scope.transactions.length);
          return angular.forEach(response.result, function(val) {
            return $scope.transactions.push({
              block_num: val.block_num,
              trx_num: val.trx_num,
              time: val.confirm_time,
              amount: format_amount(val.delta_balance),
              from: fromat_address(val.from),
              to: fromat_address(val.to),
              memo: val.memo
            });
          });
        });
      });
    };
    $scope.load_transactions();
    return $scope.rescan = function() {
      return $scope.load_transactions();
    };
  });

}).call(this);

(function() {
  angular.module("app").controller("TransferController", function($scope, $location, $state, RpcService, InfoBarService) {
    return $scope.send = function() {
      return RpcService.request('sendtoaddress', [$scope.payto, $scope.amount, $scope.memo]).then(function(response) {
        $scope.payto = "";
        $scope.amount = "";
        $scope.memo = "";
        return InfoBarService.message = "Transaction broadcasted (" + response.result + ")";
      });
    };
  });

}).call(this);

(function() {
  angular.module("app.directives", []).directive("errorBar", function($parse) {
    return {
      restrict: "A",
      template: "<div class=\"alert alert-danger\">\n  <button type=\"button\" class=\"close\" data-dismiss=\"alert\" aria-hidden=\"true\" ng-click=\"hideErrorBar()\" >×</button>\n  <i class=\"fa fa-exclamation-circle\"></i>\n  {{errorMessage}}\n </div> ",
      link: function(scope, elem, attrs) {
        var errorMessageAttr;
        errorMessageAttr = attrs["errormessage"];
        scope.errorMessage = null;
        scope.$watch(errorMessageAttr, function(newVal) {
          scope.errorMessage = newVal;
          return scope.isErrorBarHidden = !newVal;
        });
        return scope.hideErrorBar = function() {
          scope.errorMessage = null;
          $parse(errorMessageAttr).assign(scope, null);
          return scope.isErrorBarHidden = true;
        };
      }
    };
  });

  angular.module("app.directives").directive("infoBar", function($parse) {
    return {
      restrict: "A",
      template: "<div class=\"alert alert-success\">\n  <button type=\"button\" class=\"close\" data-dismiss=\"alert\" aria-hidden=\"true\" ng-click=\"hideInfoBar()\" >×</button>\n  <i class=\"fa fa-check-circle\"></i>\n  {{infoMessage}}\n </div> ",
      link: function(scope, elem, attrs) {
        var infoMessageAttr;
        infoMessageAttr = attrs["infomessage"];
        scope.infoMessage = null;
        scope.$watch(infoMessageAttr, function(newVal) {
          scope.infoMessage = newVal;
          return scope.isInfoBarHidden = !newVal;
        });
        return scope.hideInfoBar = function() {
          scope.infoMessage = null;
          $parse(infoMessageAttr).assign(scope, null);
          return scope.isInfoBarHidden = true;
        };
      }
    };
  });

}).call(this);

(function() {
  angular.module("app.directives").directive("focusMe", function($timeout) {
    return {
      scope: {
        trigger: "@focusMe"
      },
      link: function(scope, element) {
        return scope.$watch("trigger", function() {
          return $timeout(function() {
            return element[0].focus();
          });
        });
      }
    };
  });

  angular.module("app.directives").directive("match", function() {
    return {
      require: "ngModel",
      restrict: "A",
      scope: {
        match: "="
      },
      link: function(scope, elem, attrs, ctrl) {
        return scope.$watch((function() {
          return (ctrl.$pristine && angular.isUndefined(ctrl.$modelValue)) || scope.match === ctrl.$modelValue;
        }), function(currentValue) {
          return ctrl.$setValidity("match", currentValue);
        });
      }
    };
  });

}).call(this);

(function() {
  var servicesModule;

  servicesModule = angular.module("app.services", []);

  servicesModule.factory("ErrorService", function(InfoBarService) {
    return {
      errorMessage: null,
      setError: function(msg) {
        this.errorMessage = msg;
        if (msg) {
          return InfoBarService.message = null;
        }
      },
      clear: function() {
        return this.errorMessage = null;
      }
    };
  });

  servicesModule.config(function($httpProvider) {
    return $httpProvider.interceptors.push('myHttpInterceptor');
  });

  servicesModule.factory("myHttpInterceptor", function($q, $rootScope, ErrorService) {
    var dont_report_methods;
    dont_report_methods = ["open_wallet", "walletpassphrase"];
    return {
      responseError: function(response) {
        var error_msg, method, method_in_dont_report_list, promise, title, _ref, _ref1, _ref2;
        promise = null;
        method = null;
        error_msg = ((_ref = response.data) != null ? (_ref1 = _ref.error) != null ? _ref1.message : void 0 : void 0) != null ? response.data.error.message : response.data;
        if ((response.config != null) && response.config.url.match(/\/rpc$/)) {
          if (error_msg.match(/check_wallet_is_open/)) {
            promise = $rootScope.open_wallet_and_repeat_request("open_wallet", response.config.data);
          }
          if (error_msg.match(/check_wallet_unlocked/)) {
            promise = $rootScope.open_wallet_and_repeat_request("unlock_wallet", response.config.data);
          }
          method = (_ref2 = response.config.data) != null ? _ref2.method : void 0;
          title = method ? "RPC error calling " + method : "RPC error";
          error_msg = "" + title + ": " + error_msg;
        } else {
          error_msg = "HTTP Error: " + error_msg;
        }
        console.log("" + (error_msg.substring(0, 512)) + " (" + response.status + ")", response);
        method_in_dont_report_list = method && (dont_report_methods.filter(function(x) {
          return x === method;
        })).length > 0;
        if (!promise && !method_in_dont_report_list) {
          ErrorService.setError("" + (error_msg.substring(0, 512)) + " (" + response.status + ")");
        }
        return (promise ? promise : $q.reject(response));
      }
    };
  });

}).call(this);

(function() {
  var servicesModule;

  servicesModule = angular.module("app.services");

  servicesModule.factory("InfoBarService", function() {
    return {
      message: null,
      setMessage: function(msg) {
        return this.message = msg;
      },
      clear: function() {
        return this.message = null;
      }
    };
  });

}).call(this);

(function() {
  var servicesModule;

  servicesModule = angular.module("app.services");

  servicesModule.factory("RpcService", function($http, ErrorService) {
    var RpcService;
    return RpcService = {
      request: function(method, params) {
        var http_params, promise, reqparams;
        reqparams = {
          method: method,
          params: params || []
        };
        ErrorService.setError("");
        http_params = {
          method: "POST",
          cache: false,
          url: '/rpc',
          data: {
            jsonrpc: "2.0",
            id: 1
          }
        };
        angular.extend(http_params.data, reqparams);
        promise = $http(http_params).then(function(response) {
          console.log("RpcService <" + http_params.data.method + "> response:", response);
          return response.data || response;
        });
        return promise;
      }
    };
  });

}).call(this);
