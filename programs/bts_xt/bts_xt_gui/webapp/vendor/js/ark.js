var ArkAdmin = new function ($) {
    "use strict";

    /**
     * Initialise the ArkAdmin theme
     * @param charts
     */
    this.init = function (charts) {
        initControls();
        initDatePickers();
        initMenus();
        //init code highlighter
        if (typeof prettyPrint === "function"){
            prettyPrint();
        }
        initCharts(charts);
        updateContentHeight();
        $('body').resize(function (){
            updateContentHeight();
        });
    };

    /**
     * Update the height of the content to match the window size
     */
    function updateContentHeight(){
        var windowHeight = $(window).height();
        var navHeight = $('.navbar-main').height();
        $('.content').css('min-height', (windowHeight - navHeight-1) + "px");
    }

    /**
     * Init examples
     * - This method is for demo purposes only and should be removed in production code
     * - Illustrates how to initialize various plugins on the page
     */
    this.initExamples = function (){
        //Nested Lists
        if ($('#nestable').length){
            var updateOutput = function(e)
            {
                var list   = e.length ? e : $(e.target),
                    output = list.data('output');
                if (window.JSON) {
                    output.val(window.JSON.stringify(list.nestable('serialize')));//, null, 2));
                } else {
                    output.val('JSON browser support required for this demo.');
                }
            };
            // activate Nestable for each list
            $('#nestable').nestable({ group: 1 }).on('change', updateOutput);
            $('#nestable2').nestable({ group: 1 }).on('change', updateOutput);
            // output initial serialised data
            updateOutput($('#nestable').data('output', $('#nestable-output')));
            updateOutput($('#nestable2').data('output', $('#nestable2-output')));
            $('#nestable-menu').on('click', function(e)
            {
                var target = $(e.target),
                    action = target.data('action');
                if (action === 'expand-all') {
                    $('.dd').nestable('expandAll');
                }
                if (action === 'collapse-all') {
                    $('.dd').nestable('collapseAll');
                }
            });
        }

        //layout option handle
        $('#layout_options .options-handle').on('click', function(event){
            event.preventDefault();
            var open = $('#layout_options').hasClass('open');
            if (open){
                $('#layout_options').animate({
                    right: "-180px"
                }).removeClass('open');
            }else{
                $('#layout_options').animate({
                    right: "0px"
                }).addClass('open');
            }
        });
        $('#layout_options #fixed_container').on('click', function(event){
            if ($(event.target).prop('checked')){
                $('.wrapper').addClass('container');
            }else{
                $('.wrapper').removeClass('container');
            }
        });
        $('#layout_options #fixed_header').on('click', function(event){
            if ($(event.target).prop('checked')){
                $('body').addClass('fixed_header');
            }else{
                $('body').removeClass('fixed_header');
            }
        });

        //enable select box on checkbox click
        $(':checkbox').on( "click", function() {
            $(this).parent().nextAll('select').select2("enable", this.checked );
        });

        // update pie charts at random
        setInterval(function () {
            if ($('.pie-chart').length){
                $('.pie-chart').last().data('easyPieChart').update(Math.floor(100 * Math.random()));
            }
        }, 5000);

        // popover demo
        $("[data-toggle=popover]")
            .popover({container:'body'});

        // button state demo
        $('.ark-ex-loading')
            .click(function () {
                var btn = $(this);
                btn.button('loading');
                setTimeout(function () {
                    btn.button('reset');
                }, 3000);
            });

        // Calendar
        var date = new Date();
        var d = date.getDate();
        var m = date.getMonth();
        var y = date.getFullYear();

        if ($('#calendar').length){
            var calendar = $('#calendar').fullCalendar({
                header: {
                    left: 'prev,next today',
                    center: 'title',
                    right: 'month,agendaWeek,agendaDay'
                },
                selectable: true,
                editable: true,
                select: function(start, end, allDay) {
                    var title = prompt('Event Title:');
                    if (title) {
                        calendar.fullCalendar('renderEvent',
                            {
                                title: title,
                                start: start,
                                end: end,
                                allDay: allDay
                            },
                            true // make the event "stick"
                        );
                    }
                    calendar.fullCalendar('unselect');
                },
                events: [
                    {
                        title: 'All Day Event',
                        start: new Date(y, m, 1)
                    },
                    {
                        title: 'Long Event',
                        start: new Date(y, m, d-5),
                        end: new Date(y, m, d-2)
                    },
                    {
                        id: 999,
                        title: 'Repeating Event',
                        start: new Date(y, m, d-3, 16, 0),
                        allDay: false
                    },
                    {
                        id: 999,
                        title: 'Repeating Event',
                        start: new Date(y, m, d+4, 16, 0),
                        allDay: false
                    },
                    {
                        title: 'Meeting',
                        start: new Date(y, m, d, 10, 30),
                        allDay: false
                    },
                    {
                        title: 'Lunch',
                        start: new Date(y, m, d, 12, 0),
                        end: new Date(y, m, d, 14, 0),
                        allDay: false
                    },
                    {
                        title: 'Birthday Party',
                        start: new Date(y, m, d+1, 19, 0),
                        end: new Date(y, m, d+1, 22, 30),
                        allDay: false
                    },
                    {
                        title: 'Click for Google',
                        start: new Date(y, m, 28),
                        end: new Date(y, m, 29),
                        url: 'http://google.com/'
                    }
                ]
            });
        }
    };

    /**
     * Init toggle open menu functionality
     */
    function initMenus() {
        function toggleMenu($menu){
            $menu.toggleClass('open');
        }
        $(document).on('click', '.menu .menu-toggle', function (event){
            event.preventDefault();
            toggleMenu($(this).parents('.menu').first());
        });
    }

    /**
     * Init the form controls and other input functionality
     */
    function initControls() {
        // set up textarea autosize
        $('textarea').autosize();
        // set up tooltips
        $('[data-toggle="tooltip"]').tooltip();
        // set up checkbox/radiobox styles
        $("input:checkbox, input:radio").uniform();

        // set up select2
        $('.select2').select2();

        // Pie charts
        if ($('.pie-chart').length){
            $('.pie-chart').easyPieChart({
                barColor: "#428bca",
                lineWidth: 4,
                animate: 1000,
                onStart: function () {
                    if (this.options.isInit) {
                        return;
                    }
                    this.options.isInit = true;
                    var color = $(this.el).data('barColor');
                    if (color) {
                        this.options.barColor = color;
                    }
                },
                onStep: function (oldVal, newVal, crtVal) {
                    $(this.el).find('span').text(Math.floor(crtVal) + '%');
                }
            });
        }
    }

    /**
     * Init date pickers
     */
    function initDatePickers(){
        $('.datepicker').datepicker({autoclose:true});
        $('#reportrange').daterangepicker(
            {
                ranges: {
                    'Today': [moment(), moment()],
                    'Yesterday': [moment().subtract('days', 1), moment().subtract('days', 1)],
                    'Last 7 Days': [moment().subtract('days', 6), moment()],
                    'Last 30 Days': [moment().subtract('days', 29), moment()],
                    'This Month': [moment().startOf('month'), moment().endOf('month')],
                    'Last Month': [moment().subtract('month', 1).startOf('month'), moment().subtract('month', 1).endOf('month')]
                },
                startDate: moment().startOf('month'),
                endDate: moment().endOf('month')
            },
            function(start, end) {
                $('#reportrange span').html(start.format('MMMM D, YYYY') + ' - ' + end.format('MMMM D, YYYY'));
            }
        );
    }


    /**
     * Init line and bar charts
     * @param charts
     */
    function initCharts(charts) {

        var chartLines = {
            series: {
                lines: {
                    show: true,
                    lineWidth: 1,
                    fill: true,
                    fillColor: { colors: [ { opacity: 0.1 }, { opacity: 0.13 } ] }
                },
                points: {
                    show: true,
                    lineWidth: 2,
                    radius: 3
                },
                shadowSize: 0,
                stack: true
            },
            grid: {
                hoverable: true,
                clickable: true,
                tickColor: "#f9f9f9",
                borderWidth: 0
            },
            legend: {
                show: true,
                labelBoxBorderColor: "#fff"
            },
            colors: ["#a7b5c5", "#30a0eb"],
            xaxis: {
                ticks: [[1, "Jan"], [2, "Feb"], [3, "Mar"], [4, "Apr"], [5, "May"], [6, "Jun"],
                    [7, "Jul"], [8, "Aug"], [9, "Sep"], [10, "Oct"], [11, "Nov"], [12, "Dec"]],
                font: {
                    size: 12,
                    family: "Open Sans, Arial",
                    color: "#697695"
                }
            },
            yaxis: {
                ticks: 3,
                tickDecimals: 0,
                font: {size: 12, color: "#9da3a9"}
            }
        };
        // A custom label formatter used by several of the plots

        function labelFormatter(label, series) {
            return "<div style='font-size:8pt; text-align:center; padding:2px; color:white;'>" + label + "<br/>" + Math.round(series.percent) + "%</div>";
        }
        var pieOpts = {
            series: {
                pie: {
                    show: true,
                    radius: 1,
                    label: {
                        show: true,
                        radius: 3/4,
                        formatter: labelFormatter,
                        background: {
                            opacity: 0.5
                        }
                    }
                }
            },
            legend: {
                show: false
            }
        };
        var chartBars = {
            series: {
                bars: {
                    show: true,
                    lineWidth: 1,
                    fill: true,
                    fillColor: { colors: [ { opacity: 0.1 }, { opacity: 0.13 } ] }
                }
            },
            legend: {
                show: true,
                labelBoxBorderColor: "#fff"
            },
            colors: ["#30a0eb"],
            bars: {
                horizontal: false,
                barWidth: 0.7
            },
            grid: {
                hoverable: true,
                clickable: true,
                tickColor: "#f9f9f9",
                borderWidth: 0
            },
            xaxis: {
                ticks: [[1, "Jan"], [2, "Feb"], [3, "Mar"], [4, "Apr"], [5, "May"], [6, "Jun"],
                    [7, "Jul"], [8, "Aug"], [9, "Sep"], [10, "Oct"], [11, "Nov"], [12, "Dec"]],
                font: {
                    size: 12,
                    family: "Open Sans, Arial",
                    color: "#697695"
                }
            },
            yaxis: {
                ticks: 5,
                tickDecimals: 0,
                font: {size: 12, color: "#9da3a9"}
            }
        };

        function showTooltip(x, y, contents) {
            $('<div id="tooltip">' + contents + '</div>').css({
                position: 'absolute',
                display: 'none',
                top: y - 30,
                left: x - 50,
                color: "#fff",
                padding: '2px 5px',
                'border-radius': '6px',
                'background-color': '#000',
                opacity: 0.80
            }).appendTo("body").fadeIn(200);
        }

        $.each(charts, function (id, value) {
            if (!$(id).length){
                return;
            }
            var opts = null;
            switch (value.type){
                case 'bars': opts = chartBars; break;
                case 'pie': opts = pieOpts; break;
                default:
                    opts = chartLines;
            };
            var plot = $.plot($(id), value.data, opts),
                previousPoint = null;
            $(id).bind("plothover", function (event, pos, item) {
                if (item) {
                    if (previousPoint != item.dataIndex) {
                        previousPoint = item.dataIndex;

                        $("#tooltip").remove();
                        var x = item.datapoint[0].toFixed(0),
                            y = item.datapoint[1].toFixed(0),
                            month = item.series.xaxis.ticks[item.dataIndex].label;

                        showTooltip(item.pageX, item.pageY,
                            item.series.label + " of " + month + ": " + y);
                    }
                } else {
                    $("#tooltip").remove();
                    previousPoint = null;
                }
            });
        });
    }
}(jQuery);

/**
 * Ark Init method
 */
jQuery(function () {
    "use strict";

    //Pie charts data
    var data = [];

    for (var i = 0; i < 5; i++) {
        data[i] = {
            label: "Series" + (i + 1),
            data: Math.floor(Math.random() * 20) + 15
        };
    }

    var charts = {
        '#dashboardConversions': {
            data: [
                {
                    label: "Signups",
                    data: [[1, 46], [2, 53], [3, 42], [4, 45], [5, 59], [6, 35], [7, 39], [8, 45], [9, 48], [10, 57], [11, 39], [12, 68]]
                },
                {
                    label: "Visits",
                    data: [[1, 25], [2, 30], [3, 33], [4, 48], [5, 38], [6, 40], [7, 47], [8, 55], [9, 43], [10, 50], [11, 47], [12, 39]]
                }
            ]
        },
        '#dashboardRevenues': {
            type: 'bars',
            data: [
                {
                    label: "Sales",
                    data: [[1, 51231], [2, 44220], [3, 12455], [4, 24313], [5, 57523], [6, 98432], [7, 90934], [8, 78932], [9, 12367], [10, 67345], [11, 43672], [12, 74213]]
                }
            ]
        },
        '#chartPie': {
            type: 'pie',
            data: data
        }
    };

    //Init Ark Admin Theme
    ArkAdmin.init(charts);

    //@todo: Remove on production
    ArkAdmin.initExamples();
});