//*********************************************************************************************
//                   Global variables
//*********************************************************************************************

var savedgraphs = []; // JWYT - graph definitions saved on IoTaWatt for selection
var series = []; // All of the series available, loaded from initial query
var feedlist = []; // Currently selected series+units
var yaxes = []; // array of descriptors for currently active yaxes
var response = {}; // raw response from last data query

var loading = false; // semaphores used to control asynchronous data query
var reload = false;
var refreshTimer;
var selectedunitcolor = "#00a1d8"; // Color to be used for selected unit
var embed = false; // embedded graph i.e. only displays graph placeholder

var path = ""; //https://" + location.host; // used to call home
// path = "http://iotawatt.local"; // Temp: Local development (Can use the following Chrome extension to allow remote Ajax calls: https://chrome.google.com/webstore/detail/allow-cors-access-control/lhobafahddgcelffkeicbaginigeejlf?hl=en)
const queryParameters = new URLSearchParams(window.location.search);

//*********************************************************************************************
//                      Predefined reporting periods that can be selected
//*********************************************************************************************

var initialperiod = "today"; // Initial period on reset
var period = initialperiod; // Currently selected period

var periodTable = [
  { label: "custom dates", begin: "", end: "" },
  { label: "last 10 minutes", begin: "s-10m", end: "s" },
  { label: "last 30 minutes", begin: "s-30m", end: "s" },
  { label: "last 1 hour", begin: "s-1h", end: "s" },
  { label: "last 3 hours", begin: "s-3h", end: "s" },
  { label: "last 6 hours", begin: "s-6h", end: "s" },
  { label: "last 12 hours", begin: "s-12h", end: "s" },
  { label: "last 24 hours", begin: "s-24h", end: "s" },
  { label: "last 48 hours", begin: "s-48h", end: "s" },
  { label: "last 2 days to date", begin: "d-1d", end: "s" },
  { label: "last 3 days to date", begin: "d-2d", end: "s" },
  { label: "last 7 days to date", begin: "d-7d", end: "s" },
  { label: "this week to date", begin: "w", end: "s" },
  { label: "this month to date", begin: "M", end: "s" },
  { label: "this year to date", begin: "y", end: "s" },
  { label: "today", begin: "d", end: "s" },
  { label: "yesterday", begin: "d-1d", end: "d" },
  { label: "2 days ago", begin: "d-2d", end: "d-1d" },
  { label: "3 days ago", begin: "d-3d", end: "d-2d" },
  { label: "last week", begin: "w-1w", end: "w" },
  { label: "2 weeks ago", begin: "w-2w", end: "w-1w" },
  { label: "3 weeks ago", begin: "w-3w", end: "w-2w" },
  { label: "last 4 weeks", begin: "w-4w", end: "w" },
  { label: "last month", begin: "M-1M", end: "M" },
  { label: "2 months ago", begin: "M-2M", end: "M-1M" },
  { label: "3 months ago", begin: "M-3M", end: "M-2M" },
  { label: "Last 6 months", begin: "M-6M", end: "M" },
  { label: "Last 12 months", begin: "M-12M", end: "M" },
  { label: "last year", begin: "y-1y", end: "y" },
  { label: "2 years ago", begin: "y-2y", end: "y-1y" },
];

function build_period_selector() {
  $("#select-period").empty();
  for (t in periodTable) {
    $("#select-period").append(
      "<option value=" + t + ">" + periodTable[t].label + "</option>"
    );
  }
  $("#select-period option:contains(" + period + ")")
    .prop("selected", true)
    .change();
}

// Handle period select

$("#select-period").change(function() {
  periodIndex = $(this).val();
  period = periodTable[periodIndex].label;
  custom_dates = periodIndex == 0;
  refresh &= periodTable[periodIndex].end == "s";
  if (refresh) {
    $("#refresh-text").html(" Freeze");
  } else {
    $("#refresh-text").html(" Refresh");
  }
  query();
});

//*********************************************************************************************
//                      Supported units and related parameters
//*********************************************************************************************

var initialunit = "Watts"; // Initial unit on reset
var unit = initialunit; // Currently selected unit

var units = [
  { unit: "Volts", group: "V", label: "V", stat: " ", dp: 1, min: "", max: "" },
  { unit: "Watts", group: "P", label: "W", stat: "I", dp: 1, min: "", max: "" },
  { unit: "Wh", group: "P", label: "Wh", stat: "S", dp: 2, min: "", max: "" },
  { unit: "Amps", group: "P", label: "A", stat: "I", dp: 3, min: "", max: "" },
  { unit: "VA", group: "P", label: "VA", stat: "I", dp: 1, min: "", max: "" },
  { unit: "PF", group: "P", label: "PF", stat: " ", dp: 3, min: "", max: "" },
  { unit: "Hz", group: "V", label: "Hz", stat: " ", dp: 2, min: "", max: "" },
  { unit: "VAR", group: "P", label: "VAR", stat: "I", dp: 1, min: "", max: "" },
  {
    unit: "VARh",
    group: "P",
    label: "VARh",
    stat: "S",
    dp: 2,
    min: "",
    max: "",
  },
];

// Build unit select buttons

function build_units_selector() {
  var html = "";
  for (u in units) {
    html += "<button class='btn-default unit-buttons'";
    if (units[u].unit == unit) {
      html += " style='background:" + selectedunitcolor + "'";
    }
    html += ">" + units[u].unit + "</button>";
  }
  $("#units-select").html(html);
}

function unitindex(unit) {
  for (u in units) {
    if (units[u].unit == unit) return u;
  }
}

// Handle unit select button click

$("body").on("click", ".unit-buttons", function() {
  $(".unit-buttons").css("background", "white");
  $(this).css("background", selectedunitcolor);
  unit = $(this).html();
  build_source_list();
});

//*********************************************************************************************
//                      Supported groups
//*********************************************************************************************

var initialgroup = "auto"; // Initial group on reset
var group = initialgroup; // Currently selected group

var groups = [
  { group: "auto", query: "auto" },
  { group: "Hourly", query: "1h" },
  { group: "Daily", query: "1d" },
  { group: "Weekly", query: "1w" },
  { group: "Monthly", query: "1M" },
  { group: "Yearly", query: "1y" },
  { group: "all", query: "all" },
];

// build group select options

function build_group_options() {
  $("#select-group").empty();
  for (g in groups) {
    $("#select-group").append(
      "<option value='" + groups[g].group + "'>" + groups[g].group + "</option>"
    );
  }
  $("#select-group option:contains(" + group + ")")
    .prop("selected", true)
    .change();
}

// Handle group option select

$("#select-group").change(function() {
  group = $(this).val();
  query();
});

//*********************************************************************************************
//                      Colors that will be assigned to each plot series - in order of assignment
//*********************************************************************************************

var colors = [
  "#3a87fe", // med blue
  "#96d35f", // lt green
  "#e63b7a", // pottery red
  "#feb43f", // med orange
  "#be38f3", // violet
  "#01c7fc", // lt blue
  "#ff6250", // red orange
  "#e4ef65", // yellow green
  "#4e7a27", // dark green
  "#99244f", // fuchia
  "#fecb3e", // lt orange
  "#5e30eb", // dark blue
  "#ff8648", // orange
];

//*********************************************************************************************
//                      Table used to scale kilo, milli, etc with associated decimal precision
//*********************************************************************************************

var formats = [
  { max: 1, div: 0.001, prefix: "m", dp: 0 },
  { max: 10, div: 1, prefix: "", dp: 2 },
  { max: 100, div: 1, prefix: "", dp: 1 },
  { max: 1000, div: 1, prefix: "", dp: 0 },
  { max: 10000, div: 1000, prefix: "k", dp: 2 },
  { max: 100000, div: 1000, prefix: "k", dp: 1 },
  { max: 1000000, div: 1000, prefix: "k", dp: 0 },
  { max: 10000000, div: 1000000, prefix: "M", dp: 2 },
  { max: 100000000, div: 1000000, prefix: "M", dp: 1 },
  { max: 100000000000, div: 1000000, prefix: "M", dp: 0 },
];

//*********************************************************************************************
//                      context defines the current graph
//*********************************************************************************************

var userDateChange = true; // Date change would be by user
var custom_dates = false; // Period is user modified dates
var periodIndex = 0; // Index of selected period option
var periodDuration = 0; // Duration of graph period in seconds
var interval = 60; // Seconds represented by first group of response
var showLegend = true; // Show the graph legend
var showUnit = true; // Prefix series with unit: in graph legend
var refresh = false;

//********************************************************************************************
//        Graph Reset - make a fresh start
//********************************************************************************************

$("#graph-reset").click(function() {
  feedlist = [];
  response.data = [];
  $("#graph-name").val("");
  $("#graph-save").hide();
  $("#graph-delete").hide();
  $(".reset-hide").hide();
  $("#refresh-text").html(" Refresh");

  period = initialperiod;
  build_period_selector();
  unit = initialunit;
  build_units_selector();
  group = initialgroup;
  build_group_options();
  refresh = false;
  set_refresh();

  build_source_list();

  for (u in units) {
    units[u].min = "";
    units[u].max = "";
  }
  $("#showSourceOptions").click();
});

//********************************************************************************************
//        unitFormat - returns display string of val+units w/appropriate scaling and dp
//                     example 2406 Watts becomes "2.41 kW"
//********************************************************************************************

function unitFormat(val, unit) {
  if (unit == "Volts") {
    return Number(val).toFixed(1) + " V";
  }
  if (unit == "PF") {
    if (val > 0.999) {
      return Number(val).toFixed(2) + " PF";
    }
    return (Number(val).toFixed(3) + " PF").substring(1);
  }
  if (unit == "Hz") {
    return Number(val).toFixed(2) + " Hz";
  }
  for (u in units) {
    if (units[u].unit == unit) {
      if (val == 0) {
        return "0 " + units[u].label;
      }
      for (f in formats) {
        if (Math.abs(val) < formats[f].max) {
          return (
            (Number(val) / formats[f].div).toFixed(formats[f].dp) +
            " " +
            formats[f].prefix +
            units[u].label
          );
        }
      }
    }
  }
  return Number(val).toFixed(0) + " " + unit;
}

//********************************************************************************************
//        begin and end datetimepickers
//        Define the pickers, maintain min and max dates on change,
//        indicate custom period and graph-reload on change by user (explicit or implicit)
//********************************************************************************************

var widgetOpen = false;
$("#datetimepicker1").datetimepicker({
  format: "MMM D,YYYY h:mm a",
});
$("#datetimepicker2").datetimepicker({
  format: "MMM D,YYYY h:mm a",
  useCurrent: false,
});
$("#datetimepicker1").on("dp.change", function(e) {
  $("#datetimepicker2").data("DateTimePicker").minDate(e.date);
  if (userDateChange) {
    custom_dates = true;
  }
});
$("#datetimepicker2").on("dp.change", function(e) {
  $("#datetimepicker1").data("DateTimePicker").maxDate(e.date);
  if (userDateChange) {
    custom_dates = true;
  }
});
$("#datetimepicker1,#datetimepicker2").on("dp.show", function(e) {
  widgetOpen = true;
});
$("#datetimepicker1,#datetimepicker2").on("dp.hide", function(e) {
  widgetOpen = false;
  if (custom_dates) {
    query();
  }
});

var beginDate = $("#datetimepicker1").data("DateTimePicker");
var endDate = $("#datetimepicker2").data("DateTimePicker");

function set_custom_dates(begin, end) {
  userDateChange = false;
  beginDate.maxDate(new Date(end * 1000));
  beginDate.date(new Date(begin * 1000));
  endDate.minDate(new Date(begin * 1000));
  endDate.date(new Date(end * 1000));
  userDateChange = true;
}

//********************************************************************************************
//        Zoom, Pan, Reload, legend
//********************************************************************************************

$(".zoom").click(function() {
  custom_dates = true;
  var time_adj = round2group((periodDuration * (1 - $(this).val())) / 2);
  set_custom_dates(response.range[0] + time_adj, response.range[1] - time_adj);
  query();
});

$(".pan").click(function() {
  custom_dates = true;
  var time_adj = round2group(periodDuration * $(this).val());
  set_custom_dates(response.range[0] + time_adj, response.range[1] + time_adj);
  query();
});

function set_refresh() {
  if (periodTable[periodIndex].end == "s" && refresh) {
    refreshTimer = setTimeout(function() {
      query();
    }, interval * 1000);
    $("#refresh-text").html(" Freeze");
  } else {
    $("#refresh-text").html(" Refresh");
  }
}

$("#refresh").click(function() {
  if (refresh) {
    clearTimeout(refreshTimer);
    $("#refresh-text").html(" Refresh");
    refresh = false;
  } else {
    refresh = true;
    query();
  }
});

function round2group(time) {
  var round = 24 * 3600;
  if (group == "hour") round = 3600;
  else if (group == "auto") round = 5;
  return time - (time % round);
}

$("#show-legend").click(function() {
  if (showLegend) {
    showLegend = false;
    $("#show-legend").html("Show Legend");
    graph();
  } else {
    showLegend = true;
    $("#show-legend").html("Hide Legend");
    graph();
  }
});

//********************************************************************************************
//        Handle cursor select range and tooltip
//********************************************************************************************

$("#placeholder").bind("plotselected", function(event, ranges) {
  custom_dates = true;
  set_custom_dates(
    ranges.xaxis.from / 1000 - ((ranges.xaxis.from / 1000) % interval),
    ranges.xaxis.to / 1000 + interval - ((ranges.xaxis.to / 1000) % interval)
  );
  query();
});

$("#placeholder").bind("plothover", function(event, pos, item) {
  $("#tooltip").remove();
  if (item) {
    var value = item.series.data[item.dataIndex][1];
    var stackValue = item.datapoint[1];
    var dispValue = unitFormat(value, yaxes[item.series.yaxis.n - 1].unit);
    var dispStackValue =
      value == stackValue ?
      "" :
      "<br><span style='font-size:11px'>stack</span><br>" +
      unitFormat(stackValue, yaxes[item.series.yaxis.n - 1].unit);
    if (item.series.stack) {
      stackValue = "";
    }
    tooltip(
      item.pageX,
      item.pageY,
      "<span style='font-size:11px'>" +
      item.series.label +
      "</span><br>" +
      dispValue +
      dispStackValue +
      "<br><span style='font-size:11px'>" +
      moment.unix(item.datapoint[0] / 1000).format("H:mm ddd, MMM D") +
      "</span>",
      "#fff"
    );
  }
});

function tooltip(x, y, contents, bgColour) {
  var offset = 15; // use higher values for a little spacing between `x,y` and tooltip
  var elem = $('<div id="tooltip">' + contents + "</div>")
    .css({
      position: "absolute",
      display: "none",
      "font-weight": "bold",
      border: "1px solid rgb(255, 221, 221)",
      padding: "2px",
      "background-color": bgColour,
      opacity: "0.8",
    })
    .appendTo("body")
    .fadeIn(200);

  var elemY = y - elem.height() - offset;
  var elemX = x - elem.width() - offset;
  if (elemY < 0) {
    elemY = 0;
  }
  if (elemX < 0) {
    elemX = 0;
  }
  elem.css({
    top: elemY,
    left: elemX,
  });
}

//********************************************************************************************
//        Handle window and graph resize
//********************************************************************************************

$(window).resize(function() {
  if (!queryParameters.has("embed")) {
    sidebar_resize();
  }
  graph_resize();
  graph();
});

function graph_resize() {
  var top_offset = 0;
  var placeholder_bound = $("#placeholder_bound");
  var placeholder = $("#placeholder");
  var width = placeholder_bound.width();
  var height = width * 0.5;
  placeholder.width(width);
  placeholder_bound.height(height - top_offset);
  placeholder.height(height - top_offset);
}

//********************************************************************************************
//        CSV time-format, null-values, copy to clipboard, download
//********************************************************************************************

$("#csvtimeformat").change(function() {
  printcsv();
});

$("#csvnullvalues").change(function() {
  printcsv();
});

$("#copycsv").click(function() {
  var textArea = document.getElementById("csv");
  textArea.select();
  document.execCommand("Copy");
});

$("#downloadcsv").click(function() {
  var element = document.createElement("a");
  element.setAttribute(
    "href",
    "data:text/csv;charset=utf-8," + encodeURIComponent($("#csv").html())
  );
  element.setAttribute(
    "download",
    "iotawatt_" + moment().format("YYYY-MM-DD_HHmm") + ".csv"
  );
  element.style.display = "none";
  document.body.appendChild(element);
  element.click();
  document.body.removeChild(element);
});

//********************************************************************************************
//        Detail Lines - color, type(line/bar), fill, stack, decimals, scale
//********************************************************************************************

$("body").on("change", ".linecolor", function() {
  feedlist[$(this).attr("feedindex")].color = $(this).val();
  graph();
});

$("body").on("click", ".line-bar", function() {
  if ($(this).html() == "Line") {
    $(this).html("Bar");
  } else {
    $(this).html("Line");
  }
  feedlist[$(this).attr("feedindex")].plottype = $(this).html();
  graph();
});

$("body").on("change", ".fill", function() {
  feedlist[$(this).attr("feedindex")].fill = $(this)[0].checked;
  graph();
});

$("body").on("change", ".stack", function() {
  feedlist[$(this).attr("feedindex")].stack = $(this)[0].checked;
  graph();
});

$("body").on("change", ".accrue", function() {
  feedlist[$(this).attr("feedindex")].accrue = $(this)[0].checked;
  graph();
});

$("body").on("change", ".decimalpoints", function() {
  feedlist[$(this).attr("feedindex")].dp = $(this).val();
  query();
});

$("body").on("change", ".scale", function() {
  feedlist[$(this).attr("feedindex")].scale = $(this).val();
  graph();
});

//********************************************************************************************
//        Yaxes min and max
//********************************************************************************************

$("body").on("change", ".ymin", function() {
  if ($(this).val() == "auto") $(this).val("");
  var val = $(this).val();
  if (val == "" || $.isNumeric(val)) {
    units[$(this).attr("unitindex")].min = $(this).val();
  }
  graph();
});

$("body").on("change", ".ymax", function() {
  if ($(this).val() == "auto") $(this).val("");
  var val = $(this).val();
  if (val == "" || $.isNumeric(val)) {
    units[$(this).attr("unitindex")].max = $(this).val();
  }
  graph();
});

//********************************************************************************************
//        Detail Table select - CSV/Options/Statistics,Yaxis
//********************************************************************************************

$(".data-tables").hide();
$(".data-table").hide();
$(".show-tables").click(function() {
  $(".show-tables").removeClass("active");
  $(this).addClass("active");
  $(".data-table").hide();
  if ($(this).val() == "stats") {
    $("#sourceStatsTable").show();
  } else if ($(this).val() == "options") {
    $("#sourceOptionsTable").show();
  } else if ($(this).val() == "yaxes") {
    $("#yaxesTable").show();
  } else if ($(this).val() == "CSV") {
    $("#CSVgroup").show();
  }
});

$("#showSourceOptions").click();

//********************************************************************************************
//        re-arrange detail table entries
//********************************************************************************************

$("body").on("click", ".move-feed", function() {
  var feedid = $(this).attr("feedid") * 1;
  var curpos = parseInt(feedid);
  var moveby = parseInt($(this).attr("moveby"));
  var newpos = curpos + moveby;
  if (newpos >= 0 && newpos < feedlist.length) {
    newfeedlist = arrayMove(feedlist, curpos, newpos);
    graph();
  }
});

function arrayMove(array, old_index, new_index) {
  array.splice(new_index, 0, array.splice(old_index, 1)[0]);
  return array;
}

//********************************************************************************************
//        Handle series select
//********************************************************************************************

$("#source-table").on("click", ".source-table-entry", function() {
  for (var z in feedlist) {
    if (feedlist[z].unit == unit && feedlist[z].name == $(this).html()) {
      $(this).next().click();
      return;
    }
  }
  feedlist.push({
    name: $(this).html(),
    unit: unit,
    color: assign_color(),
    fill: false,
    stack: false,
    scale: 1,
    accrue: false,
    dp: units[unitindex(unit)].dp,
    plottype: "Line",
  });
  build_source_list();
  query();
});

//********************************************************************************************
//        Handle delete feed
//********************************************************************************************

$("body").on("click", ".feed-delete", function() {
  feedlist.splice($(this).attr("feedindex"), 1);
  graph();
});

//********************************************************************************************
//        Create source list table for the selected unit
//********************************************************************************************

function build_source_list() {
  var html = "<table style='width:100%';>";
  html += "<colgroup>";
  html += "<col span='1' style='width: 90%;'>";
  html += "<col span='1' style='width: 10%;'>";
  html += "</colgroup>";
  for (z in series) {
    var entryunits = series[z].units;
    if (source_type(unit) == source_type(series[z].unit)) {
      html +=
        "<tr class='source-table-row'><td class='source-table-entry'>" +
        series[z].name +
        "</td>";
      var selected = false;
      for (f in feedlist) {
        if (unit == feedlist[f].unit && series[z].name == feedlist[f].name) {
          html +=
            "<td class='feed-delete' feedindex=" +
            f +
            " style='background:" +
            feedlist[f].color +
            "'><i class='glyphicon glyphicon-trash'></i></td>";
          selected = true;
        }
      }
      if (!selected) {
        html += "<td></td>";
      }
      html += "</tr>";
    }
  }
  html += "</table>";
  $("#source-table").html(html);
}

function source_type(unit) {
  if (unit == "Volts" || unit == "Hz") return "voltage";
  return "power";
}

function assign_color() {
  if (feedlist.length == 0) return colors[0];
  for (i in colors) {
    var result = colors[i];
    for (z in feedlist) {
      if (result == feedlist[z].color) {
        result = false;
        break;
      }
    }
    if (result) return result;
  }
  return "#000000";
}

//********************************************************************************************
//        Sidebar open, close, resize
//********************************************************************************************

$("#sidebar-open").click(function() {
  $("#sidebar-wrapper").css("left", "250px");
  $("#sidebar-close").show();
});

$("#sidebar-close").click(function() {
  $("#sidebar-wrapper").css("left", "0");
  $("#sidebar-close").hide();
});

function sidebar_resize() {
  var width = $(window).width();
  var height = $(window).height();
  $("#sidebar-wrapper").height(height);

  if (width < 1024) {
    $("#sidebar-wrapper").css("left", "0");
    $("#wrapper").css("padding-left", "0");
    $("#sidebar-open").show();
  } else {
    $("#sidebar-wrapper").css("left", "250px");
    $("#wrapper").css("padding-left", "250px");
    $("#sidebar-open").hide();
    $("#sidebar-close").hide();
  }
}

//********************************************************************************************
//            query() - initiate asynchronous query for all graph data
//
//            Process the feedlist and initiate a query.
//            If there is a query in progress (loading:true), the request is delayed (reload:true)
//            and processed on completion.  There are no parameters, only the current feedlist.
//********************************************************************************************

function query() {
  clearTimeout(refreshTimer);

  // This is the first part of the asynchronous load logic.
  // It's a basic semaphore that queues a request for later.
  // If there is a load in progress, set reload = true to trigger another load when the current one completes.

  if (loading) {
    reload = true;
    return;
  }

  // Not currently loading, set loading semaphore and proceed

  loading = true;

  // Build the query

  var errorstr = "";
  var begin = periodTable[periodIndex].begin;
  var end = periodTable[periodIndex].end;
  if (custom_dates) {
    begin = beginDate.viewDate().format("x");
    end = endDate.viewDate().format("x");
    $("#select-period").prop({ value: 0, selectedIndex: 0 });
  }

  var request =
    path +
    "/query?format=json&header=yes&resolution=high&missing=null" +
    "&begin=" +
    begin +
    "&end=" +
    end +
    "&select=[time.utc.unix";

  for (var i = 0; i < feedlist.length; i++) {
    request += "," + feedlist[i].name;
    request += "." + feedlist[i].unit;
    request += ".d" + feedlist[i].dp;
    feedlist[i].dataindex = i + 1;
  }

  request += "]&group=";
  for (g in groups) {
    if (group == groups[g].group) {
      request += groups[g].query;
    }
  }

  // Send the query

  $.ajax({
    url: request,
    async: true,
    dataType: "text",
    error: function(xhr) {
      $("#error")
        .html(
          "<h4>" +
          xhr.status +
          " " +
          xhr.statusText +
          "</h4><p>" +
          xhr.responseText +
          "<br>" +
          request.substring(0, 400) +
          "</p>"
        )
        .show();
    },
    success: function(data_in) {
      // Process query response

      var valid = true;
      try {
        response = JSON.parse(data_in);
        if (response.success != undefined) valid = false;
      } catch (e) {
        valid = false;
      }
      if (!valid)
        errorstr +=
        "<div class='alert alert-danger'><b>Request error</b> " +
        data_in +
        "</div>";

      if (errorstr != "") {
        $("#error").html(errorstr).show();
      } else {
        userDateChange = false;
        beginDate.maxDate(new Date(response.range[1] * 1000));
        beginDate.date(new Date(response.range[0] * 1000));
        endDate.minDate(new Date(response.range[0] * 1000));
        endDate.date(new Date(response.range[1] * 1000));
        userDateChange = true;
        periodDuration = endDate
          .viewDate()
          .diff(beginDate.viewDate(), "seconds");
        if (response.data.length < 2) {
          interval = periodDuration;
        } else {
          interval = response.data[1][0] - response.data[0][0];
        }
        var autolabel = "auto";
        if (group == "auto") {
          if (interval % 3600 == 0) autolabel += " (" + interval / 3600 + "h)";
          else if (interval % 60 == 0) autolabel += " (" + interval / 60 + "m)";
          else autolabel += " (" + interval + "s)";
        }
        $("#select-group").children(":first").html(autolabel);
        $("#error").hide();

        graph();
      }

      // This is the second part of the asynchronous load logic.
      // Change state to indicate load no longer in progress.
      // If a reload was requested during this load, start a new load.
    },

    complete: function() {
      clearTimeout(refreshTimer);
      loading = false;
      if (reload) {
        reload = false;
        query();
      } else {
        set_refresh();
      }
    },
  });
}

//********************************************************************************************
//            graph() - Create a graph
//********************************************************************************************

function graph() {
  // Boilerplate options

  var options = {
    lines: { fill: false },
    grid: { hoverable: true, clickable: true },
    legend: { show: showLegend, position: "nw", sorted: "reverse" },
    toggle: { scale: "visible" },
    touch: { pan: "x", scale: "x" },
    xaxis: {
      mode: "time",
      timezone: "browser",
      min: beginDate.viewDate().format("x"),
      max: endDate.viewDate().format("x"),
    },
    yaxis: { alignTicksWithAxis: 1, tickFormatter: tick_format },
    yaxes: [],
  };
  if (!embed) {
    options.selection = { mode: "x" };
  }

  // Create yaxes table for active units from feedlist

  yaxes = [];
  var position = "left";
  for (z in feedlist) {
    var y;
    for (y = 0; y < yaxes.length; ++y) {
      if (feedlist[z].unit == yaxes[y].unit) {
        feedlist[z].yaxis = y + 1;
        break;
      }
    }
    if (y == yaxes.length) {
      for (u in units) {
        if (feedlist[z].unit == units[u].unit) {
          yaxes.push(units[u]);
          yaxes[yaxes.length - 1].unitIndex = u;
          yaxes[yaxes.length - 1].position = position;
          yaxes[yaxes.length - 1].min = units[u].min;
          yaxes[yaxes.length - 1].max = units[u].max;
          position = position == "left" ? "right" : "left";
          feedlist[z].yaxis = yaxes.length;
          break;
        }
      }
    }
  }

  // Add yaxes table information to plot options

  for (y in yaxes) {
    options.yaxes.push({
      position: yaxes[y].position,
      min: yaxes[y].min != "" ? yaxes[y].min : null,
      max: yaxes[y].max != "" ? yaxes[y].max : null,
    });
  }

  // Define plotdata and add feedlist items

  var plotdata = [];

  for (var z = feedlist.length - 1; z >= 0; z--) {
    if (feedlist[z].dataindex != undefined) {
      var dataindex = feedlist[z].dataindex;
      var data = [];
      var scale = feedlist[z].scale;
      var accrual = 0;
      for (var i = 0; i < response.data.length; i++) {
        if (response.data[i][dataindex] != null) {
          data.push([
            response.data[i][0] * 1000,
            response.data[i][dataindex] * scale + accrual,
          ]);
        }
        if (feedlist[z].accrue) {
          accrual += response.data[i][dataindex] * scale;
        }
      }

      var plot = {
        label: (showUnit ? feedlist[z].unit + ": " : "") + feedlist[z].name,
        data: data,
        yaxis: feedlist[z].yaxis,
        color: feedlist[z].color,
        stack: feedlist[z].stack != undefined && feedlist[z].stack,
      };

      if (feedlist[z].plottype == "Line") {
        plot.lines = { show: true, fill: feedlist[z].fill };
      }
      if (feedlist[z].plottype == "Bar") {
        plot.bars = { show: true, barWidth: interval * 750 };
        if (feedlist[z].fill) {
          plot.bars.fillColor = { colors: [{ opacity: 1 }, { opacity: 1 }] };
        }
      }
      plotdata.push(plot);
    }
  }

  // The BIG moment....  plot it!

  var plotobj = $.plot($("#placeholder"), plotdata, options);

  // Rebuild the tables to update context

  $(".data-tables").hide();
  if (feedlist.length != 0) {
    build_options_table();
    build_stats_table();
    build_yaxes_table();
    build_CSV();
    $(".data-tables").show();
  }
  build_source_list();
}

//********************************************************************************************
//        tick_format - callback to format the yaxis ticks
//********************************************************************************************

function tick_format(val, axis) {
  if (axis.ticks.length == 0) {
    yaxes[axis.n - 1].tickmin = val;
    yaxes[axis.n - 1].datamin = axis.datamin;
    yaxes[axis.n - 1].datamax = axis.datamax;
  }
  yaxes[axis.n - 1].tickmax = val;
  return unitFormat(val, yaxes[axis.n - 1].unit);
}

//********************************************************************************************
//        Create the tables for options, statistics, yaxes and CSV
//********************************************************************************************

function build_options_table() {
  $("#sourceOptionsBody").empty();
  for (var z in feedlist) {
    if (feedlist[z].dataindex != undefined) {
      var line = "<tr><td>";
      if (z > 0) {
        line +=
          "<a class='move-feed' title='Move up' feedid=" +
          z +
          " moveby=-1 ><i class='glyphicon glyphicon-arrow-up'></i></a>";
      }
      if (z < feedlist.length - 1) {
        line +=
          "<a class='move-feed' title='Move down' feedid=" +
          z +
          " moveby=1 ><i class='glyphicon glyphicon-arrow-down'></i></a>";
      }
      line += "</td>";
      line +=
        "<td><span class='feed-delete' feedindex=" +
        z +
        "><i class='glyphicon glyphicon-trash'></i></td>";
      line +=
        "<td style='text-align:left'>" +
        feedlist[z].unit +
        ":" +
        feedlist[z].name +
        "</td>";
      line +=
        "<td><input class='table-input linecolor' feedindex=" +
        z +
        " style='width:50px;' type='color' value='" +
        feedlist[z].color +
        "'></td>";

      line +=
        "<td><button type='button' class='table-input line-bar' feedindex=" +
        z +
        ">" +
        feedlist[z].plottype +
        "</button></td>";
      line +=
        "<td style='text-align:center'><input class='fill' type='checkbox' feedindex=" +
        z +
        (feedlist[z].fill ? " checked" : "") +
        " /></td>";
      line +=
        "<td style='text-align:center'><input class='stack' type='checkbox' feedindex=" +
        z +
        (feedlist[z].stack ? " checked" : "") +
        "></td>";
      if (feedlist[z].unit == "Wh" || feedlist[z].unit == "VARh") {
        line +=
          "<td style='text-align:center'><input class='accrue' type='checkbox' feedindex=" +
          z +
          (feedlist[z].accrue ? " checked" : "") +
          "></td>";
      } else {
        line += "<td></td>";
      }
      line +=
        "<td><input class='table-input decimalpoints' feedindex=" +
        z +
        " type='number' min='0' max='3' step='1' value=" +
        feedlist[z].dp +
        " style='width:50px;' /></td>";
      line +=
        "<td><input class='table-input scale' feedindex=" +
        z +
        " type='text' style='width:50px;' value=" +
        feedlist[z].scale +
        " /></td>";
      line += "</tr>";
      $("#sourceOptionsBody").append(line);
    }
  }
}

function build_stats_table() {
  $("#sourceStatsBody").empty();
  for (var z in feedlist) {
    if (feedlist[z].dataindex != undefined) {
      var dp = feedlist[z].dp;
      var stats = compute_stats(feedlist[z].dataindex, feedlist[z].scale);
      var line = "<tr><td>";
      if (z > 0) {
        line +=
          "<a class='move-feed' title='Move up' feedid=" +
          z +
          " moveby=-1 ><i class='glyphicon glyphicon-arrow-up'></i></a>";
      }
      if (z < feedlist.length - 1) {
        line +=
          "<a class='move-feed' title='Move down' feedid=" +
          z +
          " moveby=1 ><i class='glyphicon glyphicon-arrow-down'></i></a>";
      }
      line +=
        "<td><span class='feed-delete' feedindex=" +
        z +
        "><i class='glyphicon glyphicon-trash'></i></td>";
      line +=
        "<td style='text-align:left'>" +
        feedlist[z].unit +
        ":" +
        feedlist[z].name +
        "</td>";
      var quality = Math.round(100 * (1 - stats.npointsnull / stats.npoints));
      line +=
        "<td>" +
        quality +
        "% (" +
        (stats.npoints - stats.npointsnull) +
        "/" +
        stats.npoints +
        ")</td>";
      line += "<td>" + stats.minval.toFixed(dp) + "</td>";
      line += "<td>" + stats.maxval.toFixed(dp) + "</td>";
      line += "<td>" + stats.diff.toFixed(dp) + "</td>";
      line +=
        "<td>" + unitFormat(stats.mean.toFixed(dp), feedlist[z].unit) + "</td>";

      var unit = feedlist[z].unit;
      line += "<td>";
      for (u in units) {
        if (units[u].unit == unit) {
          if (units[u].stat == "S") {
            line += unitFormat(stats.sum, units[u].label);
          } else if (units[u].stat == "I") {
            line +=
              unitFormat(
                ((1 - stats.npointsnull / stats.npoints) *
                  stats.mean *
                  periodDuration) /
                3600,
                units[u].unit
              ) + "h";
          }
          break;
        }
      }
      line += "</td>";
      line += "</tr>";
      $("#sourceStatsBody").append(line);
    }
  }
}

function compute_stats(dataindex, scale) {
  var stats = {
    minval: 0,
    maxval: 0,
    sum: 0,
    npointsnull: 0,
    npoints: 0,
  };
  var i = 0;
  var val = null;
  for (var z in response.data) {
    if (response.data[z][dataindex] != null) {
      val = response.data[z][dataindex] * scale;
      if (i == 0) {
        stats.maxval = val;
        stats.minval = val;
      }
      if (val > stats.maxval) stats.maxval = val;
      if (val < stats.minval) stats.minval = val;
      stats.sum += val;
      i++;
    } else stats.npointsnull++;
    stats.npoints++;
  }
  stats.mean = stats.sum / i;
  stats.diff = stats.maxval - stats.minval;
  return stats;
}

function build_yaxes_table() {
  $("#yaxesBody").empty();
  for (y in yaxes) {
    var yaxis = yaxes[y];
    var unit = yaxis.unit;
    var min =
      units[unitindex(unit)].min == undefined ? "" : units[unitindex(unit)].min;
    var max =
      units[unitindex(unit)].max == undefined ? "" : units[unitindex(unit)].max;
    var line = "<tr>";
    line += "<td style='text-align:left'>" + yaxis.unit + "</td>";
    line +=
      "<td>" +
      unitFormat(yaxis.tickmin, unit) +
      " to " +
      unitFormat(yaxis.tickmax, unit) +
      "</td>";
    var dp = units[unitindex(unit)].dp;
    line +=
      "<td>" +
      Number(yaxis.datamin).toFixed(dp) +
      " to " +
      Number(yaxis.datamax).toFixed(dp) +
      "</td>";
    line +=
      "<td><input class='table-input ymin text-center' unitindex=" +
      unitindex(unit) +
      " style='width:50px' placeholder='auto' value=" +
      min +
      "></td>";
    line += "<td></td>";
    line +=
      "<td><input class='table-input ymax text-center' unitindex=" +
      unitindex(unit) +
      " style='width:50px' placeholder='auto' value=" +
      max +
      "></td>";
    line += "</tr>";
    $("#yaxesBody").append(line);
  }
}

function build_CSV() {
  $("#csv").empty();
  var timeformat = $("#csvtimeformat").val();
  var nullvalues = $("#csvnullvalues").val();
  var start_time = response.data[0][0];
  var accrual = [];
  for (f in feedlist) {
    accrual.push(0);
  }
  for (var i = 0; i < response.data.length; i++) {
    var line = "";
    if (timeformat == "unix") {
      line += response.data[i][0];
    } else if (timeformat == "seconds") {
      line += response.data[i][0] - start_time;
    } else if (timeformat == "datestr") {
      line += moment(response.data[i][0] * 1000).format("YYYY-MM-DD HH:mm:ss");
    }

    for (var f in feedlist) {
      var dataindex = feedlist[f].dataindex;
      var value = response.data[i][dataindex];
      if (value == null) {
        if (nullvalues == "show") {
          line += ", null";
        } else if (nullvalues == "remove") {
          line = "";
          break;
        } else {
          line += ",";
        }
      } else {
        value = value * feedlist[f].scale + accrual[f];
        line += ", " + Number(value).toFixed(feedlist[f].dp);
        if (feedlist[f].accrue) {
          accrual[f] = value;
        }
      }
    }
    if (line.length > 0) $("#csv").append(line + "\n");
  }
}

//********************************************************************************************
//        unitFormat - returns display string of val+units w/appropriate scaling and dp
//                     example 2406 Watts becomes "2.41 kW"
//********************************************************************************************

function unitFormat(val, unit) {
  if (unit == "Volts") {
    return Number(val).toFixed(1) + " V";
  }
  if (unit == "PF") {
    if (val > 0.999) {
      return Number(val).toFixed(2) + " PF";
    }
    return (Number(val).toFixed(3) + " PF").substring(1);
  }
  if (unit == "Hz") {
    return Number(val).toFixed(2) + " Hz";
  }
  for (u in units) {
    if (units[u].unit == unit) {
      if (val == 0) {
        return "0 " + units[u].label;
      }
      for (f in formats) {
        if (Math.abs(val) < formats[f].max) {
          return (
            (Number(val) / formats[f].div).toFixed(formats[f].dp) +
            " " +
            formats[f].prefix +
            units[u].label
          );
        }
      }
    }
  }
  return Number(val).toFixed(0) + " " + unit;
}

//********************************************************************************************
//            GRAPH SAVE
//********************************************************************************************

$("#graph-select").change(function() {
  loading = true;
  var name = $(this).val();
  $("graph-reset").click();

  $("#graph-name").val(name);
  $("#graph-save").show();
  $("#graph-delete").show();
  var index;
  for (index = 0; index < savedgraphs.length; index++) {
    if (name == savedgraphs[index].name) break;
  }
  if (index >= savedgraphs.length) {
    return;
  }

  $.ajax({
    url: path + "/graphs/" + savedgraphs[index].id,
    async: true,
    dataType: "json",
    success: function(result) {
      var context = result;
      period = context.period;
      build_period_selector();

      group = context.group;
      build_group_options();

      unit = context.unit;
      build_units_selector();

      userDateChange = false;
      beginDate.date(new Date(context.beginDate));
      endDate.date(new Date(context.endDate));
      userDateChange = true;

      feedlist = context.feedlist;
      refresh = false;
      if (context.refresh !== undefined) {
        refresh = context.refresh;
      }

      for (y in context.yaxes) {
        var u = unitindex(context.yaxes[y].unit);
        units[u].min = context.yaxes[y].min;
        units[u].max = context.yaxes[y].max;
      }
      loading = false;
      query();
    },
  });
});

$("#graph-save").click(function() {
  var name = $("#graph-name").val();

  if (name == undefined || name == "") {
    alert("Please enter a name for the graph.");
    return false;
  }

  var context = {
    name: name,
    period: period,
    group: group,
    unit: unit,
    endDate: endDate.viewDate(),
    beginDate: beginDate.viewDate(),
    feedlist: feedlist,
    yaxes: [],
  };
  if (refresh) {
    context.refresh = true;
  }


  for (z in context.feedlist) {
    delete context.feedlist[z].stats;
  }
  for (u in units) {
    if (units[u].min != "" || units[u].max != "") {
      context.yaxes.push({
        unit: units[u].unit,
        min: units[u].min,
        max: units[u].max,
      });
    }
  }
  $.ajax({
    method: "POST",
    url: path + "/graph/create",
    data: "data=" + JSON.stringify(context),
    async: true,
    //contentType: "text/plain",
    success: function(result) {
      if (!result.success) alert("ERROR: " + result.message);
      $("#graph-delete").show();
    },
  });
  graph_load_savedgraphs();
  $("#graph-select").val(name);
});

$("#graph-name").keyup(function() {
  var name = $(this).val();
  $("#graph-delete").hide();
  if (name == "") {
    $("#graph-save").hide();
    return;
  }
  for (z in savedgraphs) {
    if (savedgraphs[z].name == name) {
      $("#graph-delete").show();
      break;
    }
  }
  $("#graph-save").show();
});

$("#graph-delete").click(function() {
  var name = $("#graph-name").val();
  for (var z in savedgraphs) {
    if (savedgraphs[z].name == name) {
      graph_delete(savedgraphs[z].id);
      $("#graph-name").val("");
      $("#graph-delete").hide();
    }
  }
});

function graph_load_savedgraphs(callback) {
  $.ajax({
    url: path + "/graph/getallplus",
    async: true,
    dataType: "json",
    success: function(result) {
      savedgraphs = result;

      var out = "<option selected=true>Select graph:</option>";
      for (var z in savedgraphs) {
        var name = savedgraphs[z].name;
        out += "<option>" + name + "</option>";
      }
      $("#graph-select").html(out);
      if (callback) {
        callback();
      }
    },
  });
}

function graph_delete(id) {
  // Save
  $.ajax({
    method: "POST",
    url: path + "/graph/delete",
    data: "id=" + id,
    async: true,
    dataType: "json",
    success: function(result) {
      if (!result.success) alert("ERROR: " + result.message);
    },
  });
  graph_load_savedgraphs();
}

//********************************************************************************************
// Sidebar
//********************************************************************************************

$("#sidebar-open").click(function() {
  $("#sidebar-wrapper").css("left", "250px");
  $("#sidebar-close").show();
});

$("#sidebar-close").click(function() {
  $("#sidebar-wrapper").css("left", "0");
  $("#sidebar-close").hide();
});

function sidebar_resize() {
  var width = $(window).width();
  var height = $(window).height();
  $("#sidebar-wrapper").height(height - 41);

  if (width < 1024) {
    $("#sidebar-wrapper").css("left", "0");
    $("#wrapper").css("padding-left", "0");
    $("#sidebar-open").show();
  } else {
    $("#sidebar-wrapper").css("left", "250px");
    $("#wrapper").css("padding-left", "250px");
    $("#sidebar-open").hide();
    $("#sidebar-close").hide();
  }
}

//********************************************************************************************
//            INITIALIZE - Onetime startup sequence
//********************************************************************************************

function initialize() {
  moment().format(); // Initialize moment
  setTitle(); // Set the document title, async not critical
  build_units_selector();
  query_series_list();
  sidebar_resize();
  $("#graph-reset").click();
  $(".initHide").show();
  var validGraph = true;
  if (queryParameters.has("graph")) {
    graph_load_savedgraphs(() => {
      var graph = queryParameters.get("graph");
      validGraph = false;
      for (var z in savedgraphs) {
        if (graph == savedgraphs[z].name) {
          $("#graph-select").val(graph);
          $("#graph-select").change();
          validGraph = true;
          break;
        }
      }
      if (!validGraph) {
        alert("Graph " + queryParameters.get("graph") + " not found or invalid.");
      }
    })
  } else {
    graph_load_savedgraphs();
  }
  if (queryParameters.has("embed")) {
    embed = true;
    $(".embed").hide();
    $("#sidebar-close").click();
    $("#wrapper").css("padding-left", "0");
    $("#placeholder").off("plotselected");
    if (!queryParameters.has("graph")) {
      alert("embed specified with no graph parameter.");
    }
  }
  graph_resize();
}

function setTitle() {
  $.ajax({
    url: path + "/status?device=yes",
    async: true,
    dataType: "json",
    success: function(result) {
      var title = result.device.name + " " + $("#title").html();
      var title_link = "<a href='/'>" + result.device.name + "</a> " + $("#title").html();
      document.title = title;
      $("#title").html(title_link);
    },
  });
}

function query_series_list() {
  $.ajax({
    url: path + "/query?show=series",
    async: true,
    dataType: "json",
    success: function(data_in) {
      series = data_in.series;
      build_source_list();
    },
  });
}