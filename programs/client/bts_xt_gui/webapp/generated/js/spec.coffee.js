(function() {
  describe("controller: LoginController ($httpBackend.when().respond, jasmine-given, coffeescript)", function() {
    Given(function() {
      return module("app");
    });
    Given(inject(function($controller, $rootScope, $location, AuthenticationService, $httpBackend) {
      this.$httpBackend = $httpBackend;
      this.scope = $rootScope.$new();
      this.redirect = spyOn($location, 'path');
      return $controller('LoginController', {
        $scope: this.scope,
        $location: $location,
        AuthenticationService: AuthenticationService
      });
    }));
    Invariant(function() {
      this.$httpBackend.verifyNoOutstandingRequest();
      return this.$httpBackend.verifyNoOutstandingExpectation();
    });
    return describe("when a user successfully logs in", function() {
      Given(function() {
        return this.$httpBackend.whenPOST('/login', this.scope.credentials).respond(200);
      });
      When(function() {
        return this.scope.login();
      });
      When(function() {
        return this.$httpBackend.flush();
      });
      return Then("LoginController should redirect you to /home", function() {
        return expect(this.redirect).toHaveBeenCalledWith('/home');
      });
    });
  });

}).call(this);

(function() {
  describe("controller: LoginController ($httpBackend.expect().respond, vanilla jasmine, coffeescript)", function() {
    beforeEach(function() {
      return module("app");
    });
    beforeEach(inject(function($controller, $rootScope, $location, AuthenticationService, $httpBackend) {
      this.$location = $location;
      this.$httpBackend = $httpBackend;
      this.scope = $rootScope.$new();
      this.redirect = spyOn($location, 'path');
      return $controller('LoginController', {
        $scope: this.scope,
        $location: $location,
        AuthenticationService: AuthenticationService
      });
    }));
    afterEach(function() {
      this.$httpBackend.verifyNoOutstandingRequest();
      return this.$httpBackend.verifyNoOutstandingExpectation();
    });
    return describe("successfully logging in", function() {
      return it("should redirect you to /home", function() {
        this.$httpBackend.expectPOST('/login', this.scope.credentials).respond(200);
        this.scope.login();
        this.$httpBackend.flush();
        return expect(this.redirect).toHaveBeenCalledWith('/home');
      });
    });
  });

}).call(this);

(function() {
  describe("directive: shows-message-when-hovered (jasmine-given, coffeescript)", function() {
    Given(function() {
      return module("app");
    });
    Given(inject(function($rootScope, $compile) {
      this.directiveMessage = 'ralph was here';
      this.html = "<div shows-message-when-hovered message='" + this.directiveMessage + "'></div>";
      this.scope = $rootScope.$new();
      this.scope.message = this.originalMessage = 'things are looking grim';
      return this.elem = $compile(this.html)(this.scope);
    }));
    describe("when a user mouses over the element", function() {
      When(function() {
        return this.elem.triggerHandler('mouseenter');
      });
      return Then("the message on the scope is set to the message attribute of the element", function() {
        return this.scope.message === this.directiveMessage;
      });
    });
    return describe("when a users mouse leaves the element", function() {
      When(function() {
        return this.elem.triggerHandler('mouseleave');
      });
      return Then("the message is reset to the original message", function() {
        return this.scope.message === this.originalMessage;
      });
    });
  });

}).call(this);

(function() {
  describe("service: AuthenticationService", function() {
    Given(function() {
      return module("app");
    });
    Given(function() {
      var _this = this;
      return inject(function($http, AuthenticationService) {
        _this.AuthenticationService = AuthenticationService;
        _this.$httpPost = spyOn($http, 'post');
        return _this.$httpGet = spyOn($http, 'get');
      });
    });
    describe("#login", function() {
      Given(function() {
        return this.credentials = {
          name: 'Dave'
        };
      });
      When(function() {
        return this.AuthenticationService.login(this.credentials);
      });
      return Then(function() {
        return expect(this.$httpPost).toHaveBeenCalledWith('/login', this.credentials);
      });
    });
    return describe("#logout", function() {
      When(function() {
        return this.AuthenticationService.logout();
      });
      return Then(function() {
        return expect(this.$httpPost).toHaveBeenCalledWith('/logout');
      });
    });
  });

}).call(this);
