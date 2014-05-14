var app = angular.module('myApp', ['ngGrid']);
app.controller('MyCtrl', function ($scope) {
    $scope.filterOptions = {
        filterText: ''
    };

    setTableData = function(data) {
        /*
        var newData = [];
        for (var i = 0; i < data.length; i++) {
            newData.push({
                'Address': data[i][0],
                'Label': data[i][1]
            });
        }*/

        //alert(JSON.stringify(data));
        
        $scope.myData = data;

        
        if (!$scope.$$phase) {
            $scope.$apply();
        }
    }

    setTableData([]);

    $scope.gridOptions = {
        enableRowSelection: true,
        enableCellSelection: false,
        data: 'myData',
        filterOptions: $scope.filterOptions,
        columnDefs: [{
            field: 'memo',
            enableCellEdit: true,
            displayName: 'Label'
        }, {
            field: 'addr',
            enableCellEdit: false,
            displayName: 'Address'
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
        set_receive_address_memo(evt.targetScope.row.entity.addr, evt.targetScope.row.entity.memo);
    });

});
