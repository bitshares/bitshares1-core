angular.module("app.directives").directive "focusMe", ($timeout) ->
  scope:
    trigger: "@focusMe"
  link: (scope, element) ->
    scope.$watch "trigger", ->
      $timeout ->
        element[0].focus()

angular.module("app.directives").directive "match", ->
  require: "ngModel"
  restrict: "A"
  scope:
    match: "="
  link: (scope, elem, attrs, ctrl) ->
    scope.$watch (->
      (ctrl.$pristine and angular.isUndefined(ctrl.$modelValue)) or scope.match is ctrl.$modelValue
    ), (currentValue) ->
      ctrl.$setValidity "match", currentValue