angular.module("app.directives", []).directive "errorBar", ($parse) ->
  restrict: "A"
  template: """
  <div class="alert alert-danger">
    <button type="button" class="close" data-dismiss="alert" aria-hidden="true" ng-click="hideErrorBar()" >×</button>
    <i class="fa fa-exclamation-circle"></i>
    {{errorMessage}}
   </div> """
  link: (scope, elem, attrs) ->
    errorMessageAttr = attrs["errormessage"]
    scope.errorMessage = null

    scope.$watch errorMessageAttr, (newVal) ->
      scope.errorMessage = newVal
      scope.isErrorBarHidden = !newVal

    scope.hideErrorBar = ->
      scope.errorMessage = null
      $parse(errorMessageAttr).assign scope, null
      scope.isErrorBarHidden = true

angular.module("app.directives").directive "infoBar", ($parse) ->
  restrict: "A"
  template: """
  <div class="alert alert-success">
    <button type="button" class="close" data-dismiss="alert" aria-hidden="true" ng-click="hideInfoBar()" >×</button>
    <i class="fa fa-check-circle"></i>
    {{infoMessage}}
   </div> """
  link: (scope, elem, attrs) ->
    infoMessageAttr = attrs["infomessage"]
    scope.infoMessage = null

    scope.$watch infoMessageAttr, (newVal) ->
      scope.infoMessage = newVal
      scope.isInfoBarHidden = !newVal

    scope.hideInfoBar = ->
      scope.infoMessage = null
      $parse(infoMessageAttr).assign scope, null
      scope.isInfoBarHidden = true