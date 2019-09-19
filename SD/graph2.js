var savedgraphs = [];
var series = [];
var series = [];
var feedlist = [];
var plotdata = [];
var response = {};

var loading = false;
var reload = false;

var showunit = true;
var showlegend = true;

var units = [
  "Volts",
  "Watts",
  "kWh",
  "Amps",
  "VA",
  "PF",
  "Hz"
  ]

var yaxis = {
    left: {min: "auto", max: "auto", used: false},
    right: {min: "auto", max: "auto", used: false}}

var colors = [
  "#ff8000",    // orange
  "#008000",    // green
  "#0000ff",    // blue
  "#ff0000",    // red
  "#808080",    // gray
  "#8000ff",    // purple
  "#ff80ff",    // Fuscia
  "#00ffff",    // light blue
  "#808000",    // puke green
  "#804000",    // brown
  "#8080ff",    // light purple
  "#c0c0c0",    // light grey
  "#408080",    // pastel green
  "#ffff00",    // yellow
  "#ff80c0"     // 
  ]

    //********************************************************************************************
    //        Graph Reset - make a fresh start
    //********************************************************************************************

$("#graph-reset").click(function(){
  view.reset();
  feedlist = [];
  response.data = [];
  graph_load_savedgraphs();
  $("#graph-name").val("");
  $("#graph-delete").hide();
  load_feed_selector();
  graph_reload();
});

    //********************************************************************************************
    //        Select period, group
    //********************************************************************************************

$('#select-period').change(function () {
    view.periodIndex = $(this).val();
    view.custom = (view.periodIndex == 0);
    graph_reload();
});

$("#group").change(function() {
    //$("#group-auto").html("auto");
    view.group = $(this).val();
    graph_reload();
});

    //********************************************************************************************
    //        Set yaxisL and yaxisR
    //********************************************************************************************

$("body").on("change","#yaxisLmin",function(){
    yaxis.left.min = $(this).val();
    graph_draw();
});

$("body").on("change","#yaxisLmax",function(){
    yaxis.left.max = $(this).val();
    graph_draw();
});

$("body").on("change","#yaxisRmin",function(){
    yaxis.right.min = $(this).val();
    graph_draw();
});

$("body").on("change","#yaxisRmax",function(){
    yaxis.right.max = $(this).val();
    graph_draw();
});
    
    //********************************************************************************************
    //        Zoom, Pan, Reload
    //********************************************************************************************    

$(".zoom").click(function () {view.zoom($(this).val()); graph_reload();});

$('.pan').click(function() {view.pan($(this).val()); graph_reload();});

$("#reload").click(function(){graph_reload();});

    //********************************************************************************************
    //        Handle cursor select range and tooltip
    //********************************************************************************************

$('#placeholder').bind("plotselected", function (event, ranges)
{
    view.custom = true;
    view.set((ranges.xaxis.from/1000)-(ranges.xaxis.from % view.interval), (ranges.xaxis.to/1000)+view.interval-(ranges.xaxis.to % view.interval));
    graph_reload();
});

$('#placeholder').bind("plothover", function (event, pos, item)
{
    $("#tooltip").remove();
    if (item) {
        tooltip(item.pageX, item.pageY, "<span style='font-size:11px'>"+item.series.label+"</span><br>"+item.datapoint[1]+
          "<br><span style='font-size:11px'>"+moment.unix(item.datapoint[0]/1000).format('H:mm ddd, MMM D')+"</span>", "#fff");
    } 
});

function tooltip(x, y, contents, bgColour)
{
    var offset = 15; // use higher values for a little spacing between `x,y` and tooltip
    var elem = $('<div id="tooltip">' + contents + '</div>').css({
        position: 'absolute',
        display: 'none',
        'font-weight':'bold',
        border: '1px solid rgb(255, 221, 221)',
        padding: '2px',
        'background-color': bgColour,
        opacity: '0.8'
    }).appendTo("body").fadeIn(200);

    var elemY = y - elem.height() - offset;
    var elemX = x - elem.width()  - offset;
    if (elemY < 0) { elemY = 0; } 
    if (elemX < 0) { elemX = 0; } 
    elem.css({
        top: elemY,
        left: elemX
    });
};

    //********************************************************************************************
    //        Handle window and graph resize
    //********************************************************************************************

$(window).resize(function(){
    sidebar_resize();
    graph_resize();
    graph_draw();
});

function graph_resize() {
    var top_offset = 0;
    var placeholder_bound = $('#placeholder_bound');
    var placeholder = $('#placeholder');
    var width = placeholder_bound.width();
    var height = width * 0.5;
    placeholder.width(width);
    placeholder_bound.height(height-top_offset);
    placeholder.height(height-top_offset);
}

    //********************************************************************************************
    //        Show/Hide/Copy CSV
    //********************************************************************************************

$("#showcsv").click(function(){
        if ($("#showcsv").val() == "show") {
            $(".csvoptions").hide();
            $("#showcsv").html("Show CSV Output");
            $("#showcsv").val("hide");
        } else {
            printcsv();
            $(".csvoptions").show();
            $("#showcsv").html("Hide CSV Output");
            $("#showcsv").val("show");
        }
    });
    
$("#csvtimeformat").change(function(){
    printcsv();
});

$("#csvnullvalues").change(function(){
    printcsv();
});

$("#copycsv").click(function(){
  var textArea = document.getElementById("csv");
  textArea.select();
  document.execCommand("Copy");
});

    //********************************************************************************************
    //        Detail Lines - color, type(line/bar), fill, stack, delta, decimals, scale 
    //********************************************************************************************
    
$("body").on("change",".linecolor",function(){
    feedlist[$(this).attr("feedindex")].color = $(this).val();
    graph_draw();
});

$("body").on("click",".line-bar",function(){
    if($(this).html() == 'Line'){
      $(this).html('Bar');
    } else {
      $(this).html('Line');
    }
    feedlist[$(this).attr("feedindex")].plottype = $(this).html();
  graph_draw();
});

$("body").on("change",".fill",function(){
    feedlist[$(this).attr("feedindex")].fill = $(this)[0].checked;
    graph_draw();
});

$("body").on("change",".stack",function(){
    feedlist[$(this).attr("feedindex")].stack = $(this)[0].checked;
    graph_draw();
});

$("body").on("click",".delta",function(){
  feedlist[$(this).attr("feedindex")].delta = $(this)[0].checked;
  graph_reload();
});

$("body").on("change",".decimalpoints",function(){
    feedlist[$(this).attr("feedindex")].dp = $(this).val();
    graph_reload();
});

$("body").on("change",".scale",function(){
    feedlist[$(this).attr("feedindex")].scale = $(this).val();
    graph_draw();
});


    //********************************************************************************************
    //        Detail Lines Table - Options/Statistics, Arrange Rows
    //********************************************************************************************

$("#sourceStatsTable").hide();
$("body").on("click",".table-top",function(){
  if($(this).html() == 'Show Options'){
    $(this).html('Show Statistics');
    $("#sourceStatsTable").hide();
    $("#sourceOptionsTable").show("fast");
  }
  else {
    $(this).html('Show Options');
    $("#sourceOptionsTable").hide();
    $("#sourceStatsTable").show("fast");
  }
});

$("body").on("click", ".move-feed", function(){
    var feedid = $(this).attr("feedid")*1;
    var curpos = parseInt(feedid);
    var moveby = parseInt($(this).attr("moveby"));
    var newpos = curpos + moveby;
    if (newpos>=0 && newpos<feedlist.length){
        newfeedlist = arrayMove(feedlist,curpos,newpos);
        graph_draw();
    }
});

function arrayMove(array,old_index, new_index){
    array.splice(new_index, 0, array.splice(old_index, 1)[0]);
    return array;
}
    
    //********************************************************************************************
    //        Init Editor?
    //********************************************************************************************
 
function graph_init_editor()
{
    graph_load_savedgraphs();
    $("#graph-name").val("");
    
    // Load user series for editor
    
    $.ajax({                                      
        url: path+"/query?show=series",
        async: false,
        dataType: "json",
        success: function(data_in) {
            series = data_in.series;
            
            seriesbyunits = {};
            for(var z in units){
              seriesbyunits[units[z]] = [];
            }
            for (var z in series) {
                series[z].id = series[z].name + series[z].unit;
                
                if(series[z].unit == "Volts" || series[z] == "Hz"){
                  seriesbyunits["Volts"].push(series[z]);
                  seriesbyunits["Hz"].push(series[z]);
                } else {
                  seriesbyunits["Watts"].push(series[z]);
                  seriesbyunits["kWh"].push(series[z]);
                  seriesbyunits["Amps"].push(series[z]);
                  seriesbyunits["VA"].push(series[z]);
                  seriesbyunits["PF"].push(series[z]);
                }
            }
            
            var out = "";
            out += "<colgroup>";
            out += "<col span='1' style='width: 70%;'>";
            out += "<col span='1' style='width: 15%;'>";
            out += "<col span='1' style='width: 15%;'>";
            out += "</colgroup>";
            
            for (var unit in seriesbyunits) {
               unitname = unit;
               if (unit=="") unitname = "undefined";
               out += "<tr class='unitheading' unit='"+unitname+"' style='background-color:#aaa; cursor:pointer'><td style='font-size:12px; padding:4px; padding-left:8px; font-weight:bold'>"+unitname+"</td><td></td><td></td></tr>";
               out += "<tbody class='unitbody' unit='"+unitname+"'>";
               for (var z in seriesbyunits[unit]) 
               {
                   out += "<tr>";
                   var name = seriesbyunits[unit][z].name;
                   if (name.length>20) {
                       name = name.substr(0,20)+"..";
                   }
                   out += "<td>"+name+"</td>";
                   out += "<td><input class='feed-select-left' feedid="+seriesbyunits[unit][z].id+" name="+seriesbyunits[unit][z].name+" units="+unit+" type='checkbox'></td>";
                   out += "<td><input class='feed-select-right' feedid="+seriesbyunits[unit][z].id+" name="+seriesbyunits[unit][z].name+" units="+unit+" type='checkbox'></td>";
                   out += "</tr>";
               }
               out += "</tbody>";
            }
            $("#series").html(out);
            
            $(".unitbody").hide();
        }
    });
    
    $(".csvoptions").hide();

    $("body").on("click",".feed-select-left",function(){
        var feedid = $(this).attr("feedid");
        var name = $(this).attr("name");
        var unit = $(this).attr("units");
        var checked = $(this)[0].checked;
        
        var loaded = false;
        for (var z in feedlist) {
           if (feedlist[z].id==feedid) {
               if (!checked) {
                   feedlist.splice(z,1);
               } else {
                   feedlist[z].yaxis = 1;
                   loaded = true;
                   $(".feed-select-right[feedid="+feedid+"]")[0].checked = false;
               }
               graph_draw();
           }
        }
        
        if (loaded==false && checked) {
          pushfeedlist(name, unit, 1);
          graph_reload();
        }
    });

    $("body").on("click",".feed-select-right",function(){
        var feedid = $(this).attr("feedid");
        var name = $(this).attr("name");
        var unit = $(this).attr("units");
        var checked = $(this)[0].checked;

        
        var loaded = false;
        for (var z in feedlist) {
           if (feedlist[z].id==feedid) {
               if (!checked) {
                   feedlist.splice(z,1);
               } else {
                   feedlist[z].yaxis = 2;
                   loaded = true;
                   $(".feed-select-left[feedid="+feedid+"]")[0].checked = false;
               }
               graph_draw();
           }
        }
        
        if (loaded==false && checked) {
          pushfeedlist(name, unit, 2);
          graph_reload();
        }
    });
    
    $("body").on("click",".unitheading",function(){
        var unit = $(this).attr("unit");
        $(".unitbody[unit='"+unit+"']").toggle();
    });

}

function pushfeedlist(name, unit, yaxis) {
    // for(z in series){
    //     if(series[z].id == feedid){
    //         var dp = (series[z].unit=="Energy") ? 3 : 1;
            feedlist.push({    id:name+unit,
                    name:name,
                    unit:unit,
                    color:assign_color(),
                    yaxis:yaxis, fill:0,
                    stack:false,
                    scale: 1.0,
                    delta:false,
                    getaverage:false,
                    dp:1,
                    plottype:'Line'});
    //     }
    // }
}

function assign_color(){
  if(feedlist.length == 0) return colors[0];
  for(i in colors){
    var result = colors[i];
    for(z in feedlist){
      if(result == feedlist[z].color){
        result = false;
        break;
      }
    }
    if(result) return result;
  }
  return "#000000";
}

    //********************************************************************************************
    //        graph_reload - asynchronous query for all graph data
    //********************************************************************************************


function graph_reload() {
    
      // This is the first part of the asynchronous load logic.
      // It's a basic semaphore that queues a request for later.
      // If there is a load in progress, set reload = true to trigger another load when the current one completes.
    
    if(loading) {
      reload = true;
      return;
    } else {
      reload = false;
    }
    loading = true;
    
    // Build the query
    
    var errorstr = "";
    var begin = periodTable[view.periodIndex].begin;
    var end = periodTable[view.periodIndex].end;
    if(view.custom){
      begin = beginDate.viewDate().format('x');
      end = endDate.viewDate().format('x');
      $('#select-period').prop({"value":0, "selectedIndex":0});
    } 
    
    var request = path+"/query?format=json&header=yes&resolution=high&missing=null" + 
                        "&begin=" + begin + 
                        "&end=" + end + 
                        "&select=[time.utc.unix";
                        
    for(var i=0; i<feedlist.length; i++){
      request += "," + feedlist[i].name;
      request += '.' + feedlist[i].unit;
      if(feedlist[i].delta){
        request += ".delta";
      }
      request += ".d"+feedlist[i].dp;
      feedlist[i].dataindex = i + 1;
    }
    
    request += "]&group=";
    if(view.group == "Daily")request+="1d";
    else if(view.group == "Hourly")request+="1h";
    else if(view.group == "Weekly")request+="1w";
    else if(view.group == "Monthly")request+="1mo";
    else if(view.group == "Yearly")request+="1y";
    else request += 'auto';
    
      // Send the query
    
    $.ajax({                                      
        url: request,
        async: true,
        dataType: "text",
        error: function(xhr){
            $("#error").html("<h4>" + xhr.status + " " + xhr.statusText+ "</h4><p>" + request + "</p>").show();
        },
        success: function(data_in){
          
            // Process query response
          
            var valid = true;
            try {
                response = JSON.parse(data_in);
                if (response.success!=undefined) valid = false;
            } catch (e) {
                valid = false;
            }
            if (!valid) errorstr += "<div class='alert alert-danger'><b>Request error</b> "+data_in+"</div>";
            
            if (errorstr!="") {
                $("#error").html(errorstr).show();
            } else {
                view.userChange = false;
                view.interval = response.data[1][0] - response.data[0][0];
                beginDate.maxDate(new Date(response.range[0]*1000));
                beginDate.date(new Date(response.range[0]*1000));
                endDate.date(new Date(response.range[1]*1000));
                view.windowTime = endDate.viewDate().diff(beginDate.viewDate(),"seconds");
                view.userChange = true;
                $("#duration").html(endDate.viewDate().from(beginDate.viewDate(), true));
                $("#request-interval").val(view.interval / 1000);
                $("#request-limitinterval").attr("checked",view.limitinterval);
                $("#interval").hide();
                if(view.group == "auto"){
                  var interval;
                  if((view.interval % 3600) == 0) interval = view.interval/3600 + 'h';
                  else if((view.interval % 60) == 0) interval = view.interval/60 + 'm';
                  else interval = view.interval + 's';
                  $("#group-auto").html("auto (" + interval + ")");
                } else {
                  $("#group-auto").html("auto");
                }
                $("#error").hide();
                
                graph_draw();
            }
            
            // This is the second part of the asynchronous load logic.
            // Change state to indicate load no longer in progress.
            // If a reload was requested during this load, start a new load.
            
        },
        
        complete: function(){
            loading = false;
            if(reload){
              graph_reload();
            }
        }
        
    });
}

    //********************************************************************************************
    //        graph_draw() - create a graph.
    //********************************************************************************************


function graph_draw()
{
    var options = {
        lines: {fill: false},
        grid: {hoverable: true, clickable: true},
        selection: { mode: "x" },
        legend: { show: false, position: "nw", toggle: true },
        toggle: { scale: "visible" },
        touch: { pan: "x", scale: "x" },
        xaxis: { 
            mode: "time",
            timezone: "browser",
            min: beginDate.viewDate().format('x'),
            max: endDate.viewDate().format('x')
        },
	      yaxes: [
	          {position: "left"},
	          {position: "right",
	           alignTicksWithAxis: 1
	          }
		    ]
    }
    
    yaxis.left.used = false;
    yaxis2used=false;
    for(z in feedlist){
      yaxis.left.used |= (feedlist[z].yaxis == 1);
      yaxis2used |= (feedlist[z].yaxis == 2);
    }
    
    if (showlegend) options.legend.show = true;
    if (yaxis.left.min!='auto' && yaxis.left.min!='') {options.yaxes[0].min = yaxis.left.min}
    if (yaxis.left.max!='auto' && yaxis.left.max!='') {options.yaxes[0].max = yaxis.left.max}
    if (yaxis.right.min!='auto' && yaxis.right.min!='') {options.yaxes[1].min = yaxis.right.min}
    if (yaxis.right.max!='auto' && yaxis.right.max!='') {options.yaxes[1].max = yaxis.right.max}
    
    plotdata = [];
    
    for (var z in feedlist) {
        if(feedlist[z].dataindex != undefined){
          var dataindex = feedlist[z].dataindex;
          var data = [];
          var scale = feedlist[z].scale;
          for(var i=0; i<response.data.length; i++){
            if(response.data[i][dataindex]!=null) {
              data.push([response.data[i][0]*1000, response.data[i][dataindex]*scale]);
            }
          }
          
          // Add series to plot
          var label = "";
          if (showunit) label += feedlist[z].unit+": ";
          label += feedlist[z].name;
          if (yaxis.left.used && yaxis2used) {
              if (feedlist[z].yaxis == 1) {label += " &#10229;"}; // Long Left Arrow
              if (feedlist[z].yaxis == 2) {label += " &#10230;"}; // Long Right Arrow 
          }
  
          var stacked = feedlist[z].stack != undefined && feedlist[z].stack;
          var plot = {label:label, data:data, yaxis:feedlist[z].yaxis, color: feedlist[z].color, stack: stacked};
          
          if (feedlist[z].plottype=='Line') plot.lines = { show: true, fill: feedlist[z].fill };
          if (feedlist[z].plottype=='Bar'){
            plot.bars = { show: true, barWidth: view.interval * 750};
          }
          plotdata.push(plot);
        }
    }
    
    if(response.data[0] != undefined){
      options.xaxis.min = beginDate.viewDate().format('x');
      options.xaxis.max = endDate.viewDate().format('x');;
    }
    
    // The big moment....  plot it!
    
    var plotobj = $.plot($('#placeholder'), plotdata, options);
    
    if(yaxis.left.used) $("#yaxisLmenu").show();
    else $("#yaxisLmenu").hide();
    if(yaxis2used) $("#yaxisRmenu").show();
    else $("#yaxisRmenu").hide();
    
    build_data_tables();
    if($("#showcsv").val() == "show"){
      printcsv();
    }
}

    //********************************************************************************************
    //        build_data_tables() - Create the table of plot elements 
    //********************************************************************************************


function build_data_tables()
{
    if(feedlist.length == 0){
        $(".data-tables").hide();
        return;
    }
    $("#sourceOptionsBody").empty();
    $("#sourceStatsBody").empty();
    for(var z in feedlist){
      if(feedlist[z].dataindex != undefined){
        var dp = feedlist[z].dp;
        var line = "";
        line += "<tr><td>";
        if (z > 0) {
            line += "<a class='move-feed' title='Move up' feedid="+z+" moveby=-1 ><i class='glyphicon glyphicon-arrow-up'></i></a>";
        }
        if (z < feedlist.length-1) {
            line += "<a class='move-feed' title='Move down' feedid="+z+" moveby=1 ><i class='glyphicon glyphicon-arrow-down'></i></a>";
        }
        line += "</td>";
        line += "<td style='text-align:left'>"+feedlist[z].unit+":"+feedlist[z].name+"</td>";
        line += "<td><input class='table-input linecolor' feedindex="+z+" style='width:50px;' type='color' value='"+feedlist[z].color+"'></td>";
        
        line += "<td><button type='button' class='table-input line-bar' feedindex="+z+">"+feedlist[z].plottype+"</button></td>";
        line += "<td style='text-align:center'><input class='fill' type='checkbox' feedindex="+z+(feedlist[z].fill?' checked':'') + " /></td>";
        line += "<td style='text-align:center'><input class='stack' type='checkbox' feedindex="+z+(feedlist[z].stack?' checked':'') + "></td>";
        line += "<td>";
        if(feedlist[z].unit == "Energy"){
          line += "<input class='delta' + feedindex="+z+" type='checkbox'";
          if(feedlist[z].delta){line += " checked"};
          line += "/>";}
        line += "</td>";
        line += "<td><input class='table-input decimalpoints' feedindex="+z+" type='number' min='0' max='3' step='1' value="+feedlist[z].dp+" style='width:50px;' /></td>";
        line += "<td><input class='table-input scale' feedindex="+z+" type='text' style='width:50px;' value="+feedlist[z].scale+" /></td>";
        line += "</tr>";
        $("#sourceOptionsBody").append(line);
        
        var stats = getStats(feedlist[z].dataindex);
        line = "<tr><td>";
        if (z > 0) {
            line += "<a class='move-feed' title='Move up' feedid="+z+" moveby=-1 ><i class='glyphicon glyphicon-arrow-up'></i></a>";
        }
        if (z < feedlist.length-1) {
            line += "<a class='move-feed' title='Move down' feedid="+z+" moveby=1 ><i class='glyphicon glyphicon-arrow-down'></i></a>";
        }
        line += "<td style='text-align:left'>"+feedlist[z].unit+":"+feedlist[z].name+"</td>";
        var quality = Math.round(100 * (1-(stats.npointsnull/stats.npoints)));
        line += "<td>"+quality+"% ("+(stats.npoints-stats.npointsnull)+"/"+stats.npoints+")</td>";
        line += "<td>"+stats.minval.toFixed(dp)+"</td>";
        line += "<td>"+stats.maxval.toFixed(dp)+"</td>";
        line += "<td>"+stats.diff.toFixed(dp)+"</td>";
        line += "<td>"+stats.mean.toFixed(dp)+"</td>";
        
        if(feedlist[z].unit == "Power"){
          line += "<td>Wh="+Math.round(stats.mean*view.windowTime/3600)+"</td>";
        }
        
        line += "/tr>"
        $("#sourceStatsBody").append(line);
      }
    }
    $(".data-tables").show();
}

//----------------------------------------------------------------------------------------
//        printcsv() - Generate the CSV data table
//----------------------------------------------------------------------------------------
function printcsv()
{
    var timeformat = $("#csvtimeformat").val();
    var nullvalues = $("#csvnullvalues").val();
    
    var csvout = "";
    var start_time = response.data[0][0];
    for(var i=0; i<response.data.length; i++){
        var line = "";
        if (timeformat=="unix") {
            line += response.data[i][0];
        } else if (timeformat=="seconds") {
            line += (response.data[i][0] - start_time);
        } else if (timeformat=="datestr") {
            line += moment(response.data[i][0]*1000).format("YYYY-MM-DD HH:mm:ss");
        }
        
        ;
        for (var f in feedlist) {
            var dataindex = feedlist[f].dataindex;
            if(response.data[i][dataindex]==null){
              if(nullvalues == "show"){
                line += ", null";
              } else if(nullvalues == "remove"){
                line = "";
                break;
              } else {
                line += ",";
              }
            } else {
              line += ", " + Number(response.data[i][dataindex]*feedlist[f].scale).toFixed(feedlist[f].dp);
            }
        }
        if(line.length > 0) csvout += line+"\n";
    }

    $("#csv").val(csvout);
}

//----------------------------------------------------------------------------------------
// Saved graph's feature
//----------------------------------------------------------------------------------------

$("#graph-select").change(function() {
    var name = $(this).val();
    $("#graph-name").val(name);
    $("#graph-delete").show();
    var index;
    for(index=0; index<savedgraphs.length; index++){
      if(name == savedgraphs[index].name)break;
    }
    
    view.custom = savedgraphs[index].view.custom;
    view.periodIndex = savedgraphs[index].periodIndex;
    view.group = savedgraphs[index].view.group;
    view.interval = savedgraphs[index].view.interval;
    view.windowTime = savedgraphs[index].view.winndowTime;

    yaxis = savedgraphs[index].yaxis;
    view.userChange = false;
    endDate.date(new Date(savedgraphs[index].endDate));
    beginDate.date(new Date(savedgraphs[index].beginDate));
    view.userChange = true;
  
    // Lookup the period literally in case the options have changed
    $('#select-period').prop({"value":0, "selectedIndex":0});
    view.periodIndex = 0;
    for(p in periodTable){
      if(savedgraphs[index].periodlabel == periodTable[p].label ){
        $('#select-period').prop({"value":periodTable[p].label , "selectedIndex":p});
        view.periodIndex = p;
        break;
      }
    }
    
    // feedlist
    feedlist = savedgraphs[index].feedlist;
    $(".feed-select-left").prop("checked",false);
    $(".feed-select-right").prop("checked",false);
    
    $("#yaxis-min").val(yaxis.left.min);
    $("#yaxis-max").val(yaxis.left.max);
    $("#y2axis-min").val(yaxis.right.min);
    $("#y2axis-max").val(yaxis.right.max);
    $('#select-period').prop({"value":0, "selectedIndex":view.periodIndex});
    var options = $("#group").children();
    for(var i=0; i<options.length; i++){
      if(options[i].value == view.group){
        $('#group').prop({"value":options[i].value, "selectedIndex":i});
      }
    }
    
    for(z in feedlist){
      var a = $("[feedid='"+feedlist[z].id+"']");
      if(a.length){
        a[feedlist[z].yaxis-1].checked=true;
      }
      else {
        feedlist.splice(z,1);
      }
    }
    graph_reload();
});

$("#graph-name").keyup(function(){
    var name = $(this).val();
    $("#graph-delete").hide();
    for(z in savedgraphs){
      if(savedgraphs[z].name == name){
        $("#graph-delete").show();
        break;
      }
    }
});

$("#graph-delete").click(function() {
    var name = $("#graph-name").val();
    for (var z in savedgraphs) {
        if (savedgraphs[z].name==name){
            graph_delete(savedgraphs[z].id);
            //$("#graph-reset").click();
            // feedlist = [];
            // graph_reload();
            $("#graph-name").val("");
            $("#graph-delete").hide();
            // load_feed_selector();
        }
    }
});

$("#graph-save").click(function() {
    var name = $("#graph-name").val();
    
    if (name==undefined || name=="") {
        alert("Please enter a name for the graph.");
        return false;
    }
    
    var graph_to_save = {
        name: name,
        view: view,
        yaxis: yaxis,
        endDate: endDate.viewDate(),
        beginDate: beginDate.viewDate(),
        periodlabel: periodTable[view.periodIndex].label,
        feedlist: feedlist
    };
    for(z in graph_to_save.feedlist){
      delete graph_to_save.feedlist[z].stats;
    }
    $.ajax({         
        method: "POST",                             
        url: path+"/graph/create?version=2",
        data: "data="+JSON.stringify(graph_to_save),
        async: true,
        dataType: "json",
        success: function(result) {
            if (!result.success) alert("ERROR: "+result.message);
            $("#graph-delete").show();
        }
    });
    graph_load_savedgraphs();
    $("#graph-select").val(name);
});

function graph_load_savedgraphs()
{
    $.ajax({                                      
        url: path+"/graph/getall?version=2",
        async: true,
        dataType: "json",
        success: function(result) {
            savedgraphs = result;
            
            var out = "<option selected=true>Select graph:</option>";
            for (var z in savedgraphs) {
               var name = savedgraphs[z].name;
               out += "<option>"+name+"</option>";
            }
            $("#graph-select").html(out);
        }
    });
}

function graph_delete(id) {
    // Save 
    $.ajax({         
        method: "POST",                             
        url: path+"/graph/delete?version=2",
        data: "id="+id,
        async: true,
        dataType: "json",
        success: function(result) {
            if (!result.success) alert("ERROR: "+result.message);
        }
    });
    graph_load_savedgraphs();
}


// ----------------------------------------------------------------------------------------
// Sidebar
// ----------------------------------------------------------------------------------------
$("#sidebar-open").click(function(){
    $("#sidebar-wrapper").css("left","250px");
    $("#sidebar-close").show();
});

$("#sidebar-close").click(function(){
    $("#sidebar-wrapper").css("left","0");
    $("#sidebar-close").hide();
});

function sidebar_resize() {
    var width = $(window).width();
    var height = $(window).height();
    $("#sidebar-wrapper").height(height-41);
    
    if (width<1024) {
        $("#sidebar-wrapper").css("left","0");
        $("#wrapper").css("padding-left","0");
        $("#sidebar-open").show();
    } else {
        $("#sidebar-wrapper").css("left","250px");
        $("#wrapper").css("padding-left","250px");
        $("#sidebar-open").hide();
        $("#sidebar-close").hide();
    }
}

// ----------------------------------------------------------------------------------------
function load_feed_selector() {
    for (var z in series) {
        var feedid = series[z].id;
        $(".feed-select-left[feedid="+feedid+"]")[0].checked = false;
        $(".feed-select-right[feedid="+feedid+"]")[0].checked = false;
    }
    
    for (var z=0; z<feedlist.length; z++) {
        var feedid = feedlist[z].id;
        var unit = feedlist[z].unit;
        if (unit=="") unit = "undefined";
        if (feedlist[z].yaxis==1) { $(".feed-select-left[feedid="+feedid+"]")[0].checked = true; $(".unitbody[unit='"+unit+"']").show(); }
        if (feedlist[z].yaxis==2) { $(".feed-select-right[feedid="+feedid+"]")[0].checked = true; $(".unitbody[unit='"+unit+"']").show(); }
    }
}

