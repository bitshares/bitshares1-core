servicesModule = angular.module("app.services", [])
servicesModule.factory "ErrorService", (InfoBarService) ->
  errorMessage: null
  setError: (msg) ->
    @errorMessage = msg
    InfoBarService.message = null if msg
  clear: ->
    @errorMessage = null

servicesModule.config ($httpProvider) ->
  $httpProvider.interceptors.push('myHttpInterceptor')

servicesModule.factory "myHttpInterceptor", ($q, $rootScope, ErrorService) ->
  dont_report_methods = ["open_wallet", "walletpassphrase"]

#  request: (config) ->
#    config
#
#  response: (response) ->
#    response

  responseError: (response) ->
    promise = null
    method = null
    error_msg = if response.data?.error?.message? then response.data.error.message else response.data
    if response.config? and response.config.url.match(/\/rpc$/)
      if error_msg.match(/check_wallet_is_open/)
        promise = $rootScope.open_wallet_and_repeat_request("open_wallet", response.config.data)
      if error_msg.match(/check_wallet_unlocked/)
        promise = $rootScope.open_wallet_and_repeat_request("unlock_wallet", response.config.data)
      method = response.config.data?.method
      title = if method then "RPC error calling #{method}" else "RPC error"
      error_msg = "#{title}: #{error_msg}"
    else
      error_msg = "HTTP Error: " + error_msg
    console.log "#{error_msg.substring(0, 512)} (#{response.status})", response
    method_in_dont_report_list = method and (dont_report_methods.filter (x) -> x == method).length > 0
    if !promise and !method_in_dont_report_list
      ErrorService.setError "#{error_msg.substring(0,512)} (#{response.status})"

    return (if promise then promise else $q.reject(response))
