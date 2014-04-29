var contacts = angular.module('contactsApp', ['ngGrid']);
contacts.controller('MyCtrl', function ($scope) {
    $scope.filterOptions = {
        filterText: ''
    };

    setContactsData = function(data) {
        var newData = [];
        for (var i = 0; i < data.length; i++) {
            newData.push({
                'Address': data[i][0],
                'Label': data[i][1]
            });
        }
        newData.unshift({
                'Address': "",
                'Label': "Type new address here -->"
            });
        $scope.myData = newData;

        
        if (!$scope.$$phase) {
            $scope.$apply();
        }
    }

    setContactsData([]);

    $scope.gridOptions = {
        enableRowSelection: false,
        enableCellSelection: true,
        enableCellEdit: true,
        data: 'myData',
        filterOptions: $scope.filterOptions,
        columnDefs: [{
            field: 'Label'
        }, {
            field: 'Address'
        }]
    };

    $scope.filterNephi = function () {
        var filterText = 'name:Nephi';
        if ($scope.filterOptions.filterText === '') {
            $scope.filterOptions.filterText = filterText;
        } else if ($scope.filterOptions.filterText === filterText) {
            $scope.filterOptions.filterText = '';
        }
    };

    $scope.$on('ngGridEventEndCellEdit', function(evt){
        if (!evt.targetScope.row) {
	        evt.targetScope.row = [];
	    }
        var address = evt.targetScope.row.entity['Address'];

        if (address != '') {
            add_send_address(address, evt.targetScope.row.entity.Label);
        }
    });

});
