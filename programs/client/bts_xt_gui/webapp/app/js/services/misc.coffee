servicesModule = angular.module("app.services")

servicesModule.factory "InfoBarService", ->
  message: null
  setMessage: (msg) ->
    @message = msg
  clear: ->
    @message = null