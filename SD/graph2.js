var savedgraphs = [];
var feeds = [];
var feedlist = [];
var plotdata = [];

var reloading = false;
var reload = false;
var highRes = false;
var highResTimer;
var queryData = [];

var embed = false;
var skipmissing = 0;
var showcsv = 0;

var showmissing = false;
var showtag = true;
var showlegend = true;

var floatingtime=1;
var yaxismin="auto";
var yaxismax="auto";
var y2axismin="auto";
var y2axismax="auto";
var showtag = true;

var previousPoint = 0;

var active_histogram_feed = 0;

 $("#graph-reset").click(function(){
  view.reset();
  feedlist = [];
  queryData = [];
  graph_load_savedgraphs();
  $("#graph-name").val("");
  $(".feed-select-left").prop("checked",false);
  $(".feed-select-right").prop("checked",false);
  graph_reload();
});
    
$("#info").show();
if ($("#showtag")[0]!=undefined) $("#showtag")[0].checked = showtag;
if ($("#showlegend")[0]!=undefined) $("#showlegend")[0].checked = showlegend;

$(".zoom").click(function () {view.zoom($(this).val()); graph_reload();});
$('.pan').click(function() {view.pan($(this).val()); graph_reload();});

$('#select-time').change(function () {
    floatingtime=1; 
    view.timewindow($(this).val());
    $(this).blur();
    graph_reload();
});

$('#request-start').change(function(){
  view.custom = true;
  view.start = new Date($('#request-start').val()).getTime();
  graph_reload();
});

$('#request-end').change(function(){
  view.custom = true;
  view.end = new Date($('#request-end').val()).getTime();
  graph_reload();
});

$('#placeholder').bind("legendclick", function (event, ranges) {
  console.log(event);
});

$('#placeholder').bind("plotselected", function (event, ranges)
{
    view.custom = true; 
    floatingtime=0; 
    view.start = ranges.xaxis.from - (ranges.xaxis.from % view.interval);
    view.end = ranges.xaxis.to + view.interval - (ranges.xaxis.to % view.interval);
    graph_reload();
});

$('#placeholder').bind("plothover", function (event, pos, item) {
    if (item) {
        var z = item.dataIndex;
        if (previousPoint != item.datapoint) {
            previousPoint = item.datapoint;

            $("#tooltip").remove();
            var item_time = item.datapoint[0];
            var item_value = item.datapoint[1];

            var d = new Date(item_time);
            var days = ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"];
            var months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
            var minutes = d.getMinutes();
            if (minutes<10) minutes = "0"+minutes;
            var date = d.getHours()+":"+minutes+" "+days[d.getDay()]+", "+months[d.getMonth()]+" "+d.getDate();
            tooltip(item.pageX, item.pageY, "<span style='font-size:11px'>"+item.series.label+"</span><br>"+item_value+"<br><span style='font-size:11px'>"+date+"</span>", "#fff");
        }
    } else $("#tooltip").remove();
});

$(window).resize(function(){
    if (!embed) sidebar_resize();
    graph_resize();
    graph_draw();
});

function graph_resize() {
    var top_offset = 0;
    if (embed) top_offset = 35;
    var placeholder_bound = $('#placeholder_bound');
    var placeholder = $('#placeholder');

    var width = placeholder_bound.width();
    var height = width * 0.5;
    if (embed) height = $(window).height();

    placeholder.width(width);
    placeholder_bound.height(height-top_offset);
    placeholder.height(height-top_offset);
}

function graph_init_editor()
{
    graph_load_savedgraphs();
    $("#graph-name").val("");
    
    // Load user feeds for editor
    
    $.ajax({                                      
        url: path+"/feed/list.json",
        async: false,
        dataType: "json",
        success: function(data_in) {
            feeds = data_in;
            
            var numberoftags = 0;
            feedsbytag = {};
            for (var z in feeds) {
                if (feedsbytag[feeds[z].tag]==undefined) {
                    feedsbytag[feeds[z].tag] = [];
                    numberoftags++;
                }
                feedsbytag[feeds[z].tag].push(feeds[z]);
            }
            
            var out = "";
            out += "<colgroup>";
            out += "<col span='1' style='width: 70%;'>";
            out += "<col span='1' style='width: 15%;'>";
            out += "<col span='1' style='width: 15%;'>";
            out += "</colgroup>";
            
            for (var tag in feedsbytag) {
               tagname = tag;
               if (tag=="") tagname = "undefined";
               out += "<tr class='tagheading' tag='"+tagname+"' style='background-color:#aaa; cursor:pointer'><td style='font-size:12px; padding:4px; padding-left:8px; font-weight:bold'>"+tagname+"</td><td></td><td></td></tr>";
               out += "<tbody class='tagbody' tag='"+tagname+"'>";
               for (var z in feedsbytag[tag]) 
               {
                   out += "<tr>";
                   var name = feedsbytag[tag][z].name;
                   if (name.length>20) {
                       name = name.substr(0,20)+"..";
                   }
                   out += "<td>"+name+"</td>";
                   out += "<td><input class='feed-select-left' feedid="+feedsbytag[tag][z].id+" type='checkbox'></td>";
                   out += "<td><input class='feed-select-right' feedid="+feedsbytag[tag][z].id+" type='checkbox'></td>";
                   out += "</tr>";
               }
               out += "</tbody>";
            }
            $("#feeds").html(out);
            
            if (feeds.length>12 && numberoftags>2) {
                $(".tagbody").hide();
            }
        }
    });
    
    $("#reload").click(function(){
        graph_reload();
    });
    
    $("#showcsv").click(function(){
        if ($("#showcsv").html()=="Show CSV Output") {
            printcsv()
            showcsv = 1;
            $("#csv").show();
            $(".csvoptions").show();
            $("#showcsv").html("Hide CSV Output");
        } else {
            showcsv = 0;
            $("#csv").hide();
            $(".csvoptions").hide();
            $("#showcsv").html("Show CSV Output");
        }
    });
    
    $(".csvoptions").hide();

    $("body").on("click",".delta",function(){
        var feedid = $(this).attr("feedid");
        
        for (var z in feedlist) {
            if (feedlist[z].id==feedid) {
                feedlist[z].delta = $(this)[0].checked;
                break;
            }
        }
        graph_reload();
    });
    
    $("body").on("change",".linecolor",function(){
        var feedid = $(this).attr("feedid");
        
        for (var z in feedlist) {
            if (feedlist[z].id==feedid) {
                feedlist[z].color = $(this).val();
                break;
            }
        }
        graph_draw();
    });
    
    $("body").on("change",".fill",function(){
        var feedid = $(this).attr("feedid");
        
        for (var z in feedlist) {
            if (feedlist[z].id==feedid) {
                feedlist[z].fill = $(this)[0].checked;
                break;
            }
        }
        graph_draw();
    });

    $("body").on("click",".feed-select-left",function(){
        var feedid = $(this).attr("feedid");
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
          pushfeedlist(feedid, 1);
          graph_reload();
        }
    });

    $("body").on("click",".feed-select-right",function(){
        var feedid = $(this).attr("feedid");
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
        
        // if (loaded==false && checked) feedlist.push({id:feedid, yaxis:2, fill:0, scale: 1.0, delta:false, getaverage:false, dp:1, plottype:'lines'});
        if (loaded==false && checked) {
          pushfeedlist(feedid, 2);
          graph_reload();
        }
    });
    
    $("body").on("click",".tagheading",function(){
        var tag = $(this).attr("tag");
        $(".tagbody[tag='"+tag+"']").toggle();
    });

    $("#showlegend").click(function(){
        if ($("#showlegend")[0].checked) showlegend = true; else showlegend = false;
        graph_draw();
    });
    
    $("#showtag").click(function(){
        if ($("#showtag")[0].checked) showtag = true; else showtag = false;
        graph_draw();
    });

    $("#group").change(function() {
        view.group = $(this).val();
        graph_reload();
    });

    $("body").on("change",".decimalpoints",function(){
        var feedid = $(this).attr("feedid");
        var dp = $(this).val();
        
        for (var z in feedlist) {
            if (feedlist[z].id == feedid) {
                feedlist[z].dp = dp;
                
                graph_draw();
                break;
            }
        }
    });

    $("body").on("change",".plottype",function(){
        var feedid = $(this).attr("feedid");
        var plottype = $(this).val();
        
        for (var z in feedlist) {
            if (feedlist[z].id == feedid) {
                feedlist[z].plottype = plottype;
                
                graph_draw();
                break;
            }
        }
    });
    
    $("body").on("change","#yaxis-min",function(){
        yaxismin = $(this).val();
        graph_draw();
    });
    
    $("body").on("change","#yaxis-max",function(){
        yaxismax = $(this).val();
        graph_draw();
    });

    $("body").on("change","#y2axis-min",function(){
        y2axismin = $(this).val();
        graph_draw();
    });

    $("body").on("change","#y2axis-max",function(){
        y2axismax = $(this).val();
        graph_draw();
    });


    $("#csvtimeformat").change(function(){
        printcsv();
    });

    $("#csvnullvalues").change(function(){
        printcsv();
    });
    
    $('body').on("click",".legendColorBox",function(d){
          var country = $(this).html().toLowerCase();
          console.log(country);
    });
}

function pushfeedlist(feedid, yaxis) {
    var index = getfeedindex(feedid);
    feedlist.push({id:feedid, name:feeds[index].name, tag:feeds[index].tag, yaxis:yaxis, fill:0, scale: 1.0, delta:false, getaverage:false, dp:1, plottype:'lines'});
}

function graph_reload() {
    
      // This is the first part of the asynchronous reload logic.
      // It's a basic semaphore that queues the request for later.
      // If there is a reload in progress, set reload = true to trigger another reload when the current one completes.
    
    if(reloading) {
      reload = true;
      return;
    } else {
      reload = false;
    }
    reloading = true;
    if(highRes != undefined){
      clearTimeout(highResTimer);
    }
    
      // Build the query
    
    var errorstr = "";
    var begin = periodTable[view.period].begin;
    var end = periodTable[view.period].end;
    if(view.custom){
      begin = view.start;
      end = view.end;
      $('#select-time').prop({"value":0, "selectedIndex":0});
    } 
    
    var request = path+"/query?format=json&header=no&missing=null" + 
                        "&begin=" + begin + 
                        "&end=" + end + 
                        "&columns=[time.local.unix";
                        
    for(var i=0; i<feedlist.length; i++){
      request += "," + feedlist[i].name;
      if(feedlist[i].tag == "Energy"){
        request += ".kwh";
      }
      if(feedlist[i].delta){
        request += ".delta";
      }
      feedlist[i].dataindex = i + 1;
    }
    
    request += "]&group=";
    if(view.group == "Daily")request+="1d";
    else if(view.group == "Hourly")request+="1h";
    else if(view.group == "Weekly")request+="1w";
    else if(view.group == "Monthly")request+="1mo";
    else if(view.group == "Yearly")request+="1y";
    else {
      request += 'auto';
      if(highRes){
        request += "&resolution=high";
        highRes = false;
      } else {
        highResTimer = setTimeout(function(){highRes = true; graph_reload();}, 2500);
      }
    }
    
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
                queryData = JSON.parse(data_in);
                if (queryData.success!=undefined) valid = false;
            } catch (e) {
                valid = false;
            }
            if (!valid) errorstr += "<div class='alert alert-danger'><b>Request error</b> "+data_in+"</div>";
            
            if (errorstr!="") {
                $("#error").html(errorstr).show();
            } else {
                for(d in queryData){
                  queryData[d][0] *= 1000;
                }
                view.start = queryData[0][0];
                view.interval = queryData[1][0] - queryData[0][0];
                view.end = queryData[queryData.length-1][0] + view.interval;
                
                $("#request-start").val(requestTimeFormat(view.start));
                $("#request-start").prop("defaultValue",requestTimeFormat(view.start));
                $("#request-end").val(requestTimeFormat(view.end));
                $("#request-end").prop("defaultValue",requestTimeFormat(view.end));
                $("#request-interval").val(view.interval / 1000);
                $("#request-limitinterval").attr("checked",view.limitinterval);
                $("#interval").hide();
                if(view.group == "auto"){
                  $("#interval").html((view.interval / 1000) + "s").show();
                }
                $("#error").hide();
                
                
                
                graph_draw();
            }
            
            // This is the second part of the asynchronous reload logic.
            // Change state to indicate reload no longer in progress.
            // If a reload was requested during this reload, start a new reload.
            
            
        },
        
        complete: function(){
            reloading = false;
            if(reload){
              graph_reload();
            }
        }
        
    });
}

function requestTimeFormat(time){
  var date = new Date(time);
  var month = date.getUTCMonth()+1;
  var day = date.getUTCDate();
  return date.getUTCFullYear() + '-' + (month <= 9 ? '0': "") + month + '-' + (day <= 9 ? '0' : "") + day;
}

function graph_draw()
{
    var options = {
        lines: { fill: false },
        grid: {hoverable: true, clickable: true},
        selection: { mode: "x" },
        legend: { show: false, position: "nw", toggle: true },
        toggle: { scale: "visible" },
        touch: { pan: "x", scale: "x" },
        xaxis: { 
            mode: "time",
            //timezone: "browser",
            min: view.start,
            max: view.end + view.interval,
        },
	      yaxes: [
	          // Left Yaxis
	          {},
	          // Right Yaxis
	          {
	            alignTicksWithAxis: 1,
			        position: "right"
	          }
		    ]
    }
    
    if (showlegend) options.legend.show = true;
    if (yaxismin!='auto' && yaxismin!='') { options.yaxes[0].min = yaxismin }
    if (yaxismax!='auto' && yaxismax!='') { options.yaxes[0].max = yaxismax }
    if (y2axismin!='auto' && y2axismin!='') { options.yaxes[1].min = y2axismin }
    if (y2axismax!='auto' && y2axismax!='') { options.yaxes[1].max = y2axismax; }
   
    var lenstr = "";
    var time_in_window = (view.end - view.start) / 1000;
    var days = Math.floor(time_in_window / 86400);
    var timeLeft = time_in_window % 86400;
    if(days) lenstr += days + "d ";
    if(timeLeft){
      var hours = Math.floor(timeLeft / 3600);
      timeLeft %= 3600;
      lenstr += hours + "h ";
      if(timeLeft){
        var mins = Math.floor(timeLeft / 60);
        lenstr += mins + "m ";
        var secs = timeLeft %= 60;
        if(secs) lenstr += secs + "s";
      }
    }
    
    $("#window-info").html("<b>Window:</b> " + printdate(view.start) + " - " + printdate(view.end) + ", <b>Length: </b>" + lenstr);
    
    plotdata = [];
    var yaxisUsed = 0;
    
    for (var z in feedlist) {
        if(feedlist[z].dataindex != undefined){
          yaxisUsed |= feedlist[z].yaxis;
          var dataindex = feedlist[z].dataindex;
          var data = [];
          for(var i=0; i<queryData.length; i++){
            if(queryData[dataindex][i]!=null || !showmissing){
              data.push([queryData[i][0], queryData[i][dataindex]]);
            }
          }
          
          // Add series to plot
          var label = "";
          if (showtag) label += feedlist[z].tag+": ";
          label += feedlist[z].name;
          if (yaxisUsed == 3) {
              if (feedlist[z].yaxis == 1) {label += " &#10229;"}; // Long Left Arrow
              if (feedlist[z].yaxis == 2) {label += " &#10230;"}; // Long Right Arrow 
          }
  
          var plot = {label:label, data:data, yaxis:feedlist[z].yaxis, color: feedlist[z].color};
          
          if (feedlist[z].plottype=='lines') plot.lines = { show: true, fill: feedlist[z].fill };
          if (feedlist[z].plottype=='bars'){
            plot.bars = { show: true, barWidth: view.interval * 0.75};
          }
          plotdata.push(plot);
        }
    }
    
    if(queryData[0] != undefined){
      options.xaxis.min = view.start;
      options.xaxis.max = view.end;
    }
    $.plot($('#placeholder'), plotdata, options);
    
    if(yaxisUsed % 2) $("#y1axis-menu").show();
    else $("#y1axis-menu").hide();
    if(yaxisUsed >= 2) $("#y2axis-menu").show();
    else $("#y2axis-menu").hide();
    
    
    if (!embed) {
      
        var default_linecolor = "000";
        var out = "";
        
        for (var z in feedlist) {
            if(feedlist[z].dataindex != undefined){
              feedlist[z].stats = stats(feedlist[z].dataindex);
              var dp = feedlist[z].dp;
           
              out += "<tr>";
              out += "<td>"+feedlist[z].tag+":"+feedlist[z].name+"</td>";
              out += "<td><select class='plottype' feedid="+feedlist[z].id+" style='width:80px'>";
              
              var selected = "";
              if (feedlist[z].plottype == "lines") selected = "selected"; else selected = "";
              out += "<option value='lines' "+selected+">Lines</option>";
              if (feedlist[z].plottype == "bars") selected = "selected"; else selected = "";
              out += "<option value='bars' "+selected+">Bars</option>";
              out += "</select></td>";
              out += "<td><input class='linecolor' feedid="+feedlist[z].id+" style='width:50px' type='color' value='#"+default_linecolor+"'></td>";
              out += "<td><input class='fill' type='checkbox' feedid="+feedlist[z].id+"></td>";
              var quality = Math.round(100 * (1-(feedlist[z].stats.npointsnull/feedlist[z].stats.npoints)));
              out += "<td>"+quality+"% ("+(feedlist[z].stats.npoints-feedlist[z].stats.npointsnull)+"/"+feedlist[z].stats.npoints+")</td>";
              out += "<td>"+feedlist[z].stats.minval.toFixed(dp)+"</td>";
              out += "<td>"+feedlist[z].stats.maxval.toFixed(dp)+"</td>";
              out += "<td>"+feedlist[z].stats.diff.toFixed(dp)+"</td>";
              out += "<td>"+feedlist[z].stats.mean.toFixed(dp)+"</td>";
              out += "<td>"+feedlist[z].stats.stdev.toFixed(dp)+"</td>";
              out += "<td>"+Math.round((feedlist[z].stats.mean*time_in_window)/3600)+"</td>";
              for (var i=0; i<11; i++) out += "<option>"+i+"</option>";
              out += "</select></td>";
              out += "<td style='text-align:center'><input class='scale' feedid="+feedlist[z].id+" type='text' style='width:50px' value='1.0' /></td>";
              if(feedlist[z].tag == "Energy"){
                out += "<td style='text-align:center'><input class='delta' feedid="+feedlist[z].id+" type='checkbox'/></td>";
              } else {
                out += "<td/>";
              }
              
              out += "<td><select feedid="+feedlist[z].id+" class='decimalpoints' style='width:50px'><option>0</option><option>1</option><option>2</option><option>3</option></select></td>";
              out += "</tr>";
            }
        }
        $("#stats").html(out);
        
        for (var z in feedlist) {
            $(".decimalpoints[feedid="+feedlist[z].id+"]").val(feedlist[z].dp);
            if ($(".getaverage[feedid="+feedlist[z].id+"]")[0]!=undefined)
                $(".getaverage[feedid="+feedlist[z].id+"]")[0].checked = feedlist[z].getaverage;
            if ($(".delta[feedid="+feedlist[z].id+"]")[0]!=undefined)
                $(".delta[feedid="+feedlist[z].id+"]")[0].checked = feedlist[z].delta;
            $(".scale[feedid="+feedlist[z].id+"]").val(feedlist[z].scale);
            $(".linecolor[feedid="+feedlist[z].id+"]").val(feedlist[z].color);
            if ($(".fill[feedid="+feedlist[z].id+"]")[0]!=undefined)
                $(".fill[feedid="+feedlist[z].id+"]")[0].checked = feedlist[z].fill;
        }
        
        if (showcsv) printcsv();
    }
}

function getfeed(id) 
{
    for (var z in feeds) {
        if (feeds[z].id == id) {
            return feeds[z];
        }
    }
}

function getfeedindex(id) 
{
    for (var z in feeds) {
        if (feeds[z].id == id) {
            return z;
        }
    }
}

//----------------------------------------------------------------------------------------
// Print CSV
//----------------------------------------------------------------------------------------
function printcsv()
{
    var timeformat = $("#csvtimeformat").val();
    var nullvalues = $("#csvnullvalues").val();
    
    var csvout = "";
    var start_time = queryData[0][0];
    for(var i=0; i<queryData.length; i++){
        var line = "";
          // Different time format options for csv output
        if (timeformat=="unix") {
            line += queryData[i][0];
        } else if (timeformat=="seconds") {
            line += (queryData[i][0] - start_time);
        } else if (timeformat=="datestr") {
            // Create date time string
            var t = new Date(queryData[i][0]);
            var year = t.getUTCFullYear();
            var month = t.getUTCMonth()+1;
            if (month<10) month = "0"+month;
            var day = t.getUTCDate();
            if (day<10) day = "0"+day;
            var hours = t.getUTCHours();
            if (hours<10) hours = "0"+hours;
            var minutes = t.getUTCMinutes();
            if (minutes<10) minutes = "0"+minutes;
            var seconds = t.getUTCSeconds();
            if (seconds<10) seconds = "0"+seconds;
            
            line += year+"-"+month+"-"+day+" "+hours+":"+minutes+":"+seconds;
        }
          
        for (var f in feedlist) {
            dataindex = feedlist[f].dataindex;
            line += ", " + queryData[i][dataindex];
        }
        
        csvout += line+"\n";
    }

    $("#csv").val(csvout);
}

//----------------------------------------------------------------------------------------
// Saved graph's feature
//----------------------------------------------------------------------------------------
function graph_index_from_name(name) {
    var index = -1;
    for (var z in savedgraphs) {
        if (savedgraphs[z].name==name) index = z;
    }
    return index;
}

$("#graph-select").change(function() {
    var name = $(this).val();
    $("#graph-name").val(name);
    $("#graph-delete").show();
    var index;
    for(index=0; index<savedgraphs.length; index++){
      if(name == savedgraphs[index].name)break;
    }
    
    // view settings
    view.start = savedgraphs[index].start;
    view.end = savedgraphs[index].end;
    view.interval = savedgraphs[index].interval;
    view.custom = savedgraphs[index].custom;
    view.group = savedgraphs[index].group;
    yaxismin = savedgraphs[index].yaxismin;
    yaxismax = savedgraphs[index].yaxismax;
    y2axismin = savedgraphs[index].y2axismin || yaxismin;
    y2axismax = savedgraphs[index].y2axismax || yaxismax;
  
    // Lookup the period literally in case the options have changed
    for(p in periodTable){
      if(savedgraphs[index].periodlabel == periodTable[p].label ){
        view.period = p;
        break;
      }
      else if(periodTable[p].selected != undefined){
        view.period = p;
      }
    }
    
    // show settings
    showmissing = savedgraphs[index].showmissing;
    showtag = savedgraphs[index].showtag;
    showlegend = savedgraphs[index].showlegend;
    
    // feedlist
    feedlist = savedgraphs[index].feedlist;
    $(".feed-select-left").prop("checked",false);
    $(".feed-select-right").prop("checked",false);
    
    $("#yaxis-min").val(yaxismin);
    $("#yaxis-max").val(yaxismax);
    $("#y2axis-min").val(y2axismin);
    $("#y2axis-max").val(y2axismax);
    $("#showtag")[0].checked = showtag;
    $("#showlegend")[0].checked = showlegend;
    $('#select-time').prop({"value":0, "selectedIndex":view.period});
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
    
    if (graph_exists(name)) {
        $("#graph-delete").show(); 
    } else { 
        $("#graph-delete").hide();
    }
});

$("#graph-delete").click(function() {
    var name = $("#graph-name").val();
    
    var updateindex = graph_index_from_name(name);
    if (updateindex!=-1) {
        graph_delete(savedgraphs[updateindex].id);
        feedlist = [];
        graph_reload();
        $("#graph-name").val("");
        load_feed_selector();
    }
});

$("#graph-save").click(function() {
    var name = $("#graph-name").val();
    
    if (name==undefined || name=="") {
        alert("Please enter a name for the graph you wish to save");
        return false;
    }
    
    var graph_to_save = {
        name: name,
        start: view.start,
        end: view.end,
        group: view.group,
        periodlabel: periodTable[view.period].label,
        interval: view.interval,
        yaxismin: yaxismin,
        yaxismax: yaxismax,
        y2axismin: y2axismin,
        y2axismax: y2axismax,
        showtag: showtag,
        showlegend: showlegend,
        feedlist: JSON.parse(JSON.stringify(feedlist))
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
    for (var z in feeds) {
        var feedid = feeds[z].id;
        $(".feed-select-left[feedid="+feedid+"]")[0].checked = false;
        $(".feed-select-right[feedid="+feedid+"]")[0].checked = false;
    }
    
    for (var z=0; z<feedlist.length; z++) {
        var feedid = feedlist[z].id;
        var tag = feedlist[z].tag;
        if (tag=="") tag = "undefined";
        if (feedlist[z].yaxis==1) { $(".feed-select-left[feedid="+feedid+"]")[0].checked = true; $(".tagbody[tag='"+tag+"']").show(); }
        if (feedlist[z].yaxis==2) { $(".feed-select-right[feedid="+feedid+"]")[0].checked = true; $(".tagbody[tag='"+tag+"']").show(); }
    }
}

function printdate(timestamp)
{
    var date = new Date();
    var thisyear = date.getUTCFullYear();
    var date = new Date(timestamp);
    var year = date.getUTCFullYear();
    var datestr = date.toUTCString().substr(0, (thisyear == year) ? 11 : 16);
    datestr += ((date.getUTCHours()<10) ? " 0" : " ") + date.getUTCHours() + ":" + ((date.getUTCMinutes()<10) ? "0" : "") + date.getUTCMinutes();
    if(date.getUTCSeconds()) datestr += ":" + ((date.getUTCSeconds()<10) ? "0" : "") + date.getUTCSeconds();
    
    //var datestr = date.getHours()+":"+minutes+" "+day+" "+month;
    //if (thisyear!=year) datestr +=" "+year;
    return datestr;
};
