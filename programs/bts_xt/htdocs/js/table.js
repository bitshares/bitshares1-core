var app = angular.module('myApp', ['ngGrid']);
app.controller('MyCtrl', function ($scope) {
    $scope.filterOptions = {
        filterText: ''
    };

    setTableData = function(data) {
        var newData = [];
        for (var i = 0; i < data.length; i++) {
            newData.push({
                'Address': data[i][0],
                'Label': data[i][1]
            });
        }
        $scope.myData = newData;

        
        if (!$scope.$$phase) {
            $scope.$apply();
        }
    }

    setTableData([]);

    $scope.gridOptions = {
        enableRowSelection: false,
        enableCellSelection: true,
        data: 'myData',
        filterOptions: $scope.filterOptions,
        columnDefs: [{
            field: 'Label',
            enableCellEdit: true
        }, {
            field: 'Address',
            enableCellEdit: false
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
	    alert(JSON.stringify(evt.targetScope.row.entity));
    });

});
