===============
Query API
===============

---------
Overview
---------

The IoTaWatt Query API provides access to the historical data in the datalogs 
using a restful interface that produces a table of JSON or CSV data.
The table columns can be time, IoTaWatt inputs and IoTaWatt outputs.
The table rows are datalog values grouped by a fixed time period or relative time 
periods like days, weeks, months and years.

Values can be requested for a variety of measurements such as Watts, Volts or Amps. 

The Query API provides data to the Graph+ utility.

------------
Query types
------------

There are two basic queries currently supported:

show
....
    Used to obtain a list of all inputs and outputs available to the query.

select
......
    Used to select a set of series for a particular time period and return a 
    table of values in JSON or CSV format.

-----------------
query?show
-----------------

This is the only form of the show query::

    HTTP://iotawatt.local/query?show=series

This query simply lists all of the available inputs and outputs along with their respective
primary unit of measure. The format is always JSON. 

The response lists the series names that the select query will recognize.
Here is a typical response from a simple configuration::

    {"series":[{"name":"voltage","unit":"Volts"},{"name":"mains1","unit":"Watts"},
    {"name":"mains2","unit":"Watts"},{"name":"solar","unit":"Watts"},
    {"name":"heat_pump","unit":"Watts"},{"name":"mains","unit":"Watts"},
    {"name":"used","unit":"Watts"}]}

------------
query?select
------------

select=[*series1* [, *series2* ...]]
.....................................

    Required parameter. Specifies the series to be returned in the columns of the response.
    There are three types of series that can be requested:

    time [ **.local** | .utc] [ **.iso** | .unix]
        Returns the time of the beginning of the reporting group.
        Modifiers can be used to specify local (default) or utc time,
        and iso date format(default) or unix seconds since 1/1/1970 format.

    <voltage input or output> [ **.volts** | .hz] [.d<n>]
        A voltage input or output.  A unit modifier can be used to specify:
            * .volts (default) 
            * .hz (frequency).

        The .d<n> modifier overides the default number of decimal digits.

    <power input or output> [ **.watts** | .amps | .wh | .va | .var | .varh | .pf] [.d<n>]
        A power input or output. A unit modifier can be used to specify:
            * .watts (default)
            * .amps
            * .wh (watt-hours)
            * .va (volt-ampere)
            * .var (volt-ampere-reactive)
            * .varh (volt-ampere-reactive hours)
            * .pf (power factor)

        The .d<n> modifier overides the default number of decimal digits.

    An example might be:
        `select=[time.local.unix,mains.watts.d0,solar.wh.d1]`

&begin=<time specifier>
.......................

    Required parameter. Time can be specified in a variety of relative and absolute
    formats.  See `time specifiers`_ 

&end=<time specifier>
.....................

    Required parameter. Time can be specified in a variety of relative and absolute
    formats.  See `time specifiers`_. end must be greater than begin.

&group={ **auto** | all | <n> {s | m | h | d | w | M | y}}
..........................................................

    Optional parameter.  The datalog contains measurements at 5 second (current log)
    and 1 minute (history log) intervals.  This parameter specifies how to group 
    those measurements into rows in the response table.  The simple way, useful for 
    detailed examination of short time periods, is to use some fixed number of
    seconds or minutes.  When looking at longer time periods, it can be useful to 
    group by hour, day, week etc.  IoTaWatt knows where these boundaries are 
    and will return the correct grouping taking into account daylight time changes, 
    days-in-month, and leap years.

    The default is **auto**, which selects a fixed time group to yield about 360 rows 
    (resolution=low) or 720 rows (resolution=high).

    *all* will cause all of the data in the time period to be treated as a single group.
    For most units, this will result in the average value over the entire period.
    For Wh, it will result in the total Wh for the entire period.

    To overide, specify a time unit preceeded by a multiplier, as in:
    
        * 10s (ten seconds)
        * 5m (five minutes)
        * 1h (one hour)
        * 1M (one month) *note case sensitive m=minutes, M=months*

&format={ **json** | csv}
.........................

    Optional parameter specifies the format of the query response.
    The default is **json**.
    
    :json:
        Json format where the response is a Json object enclosed in brackets {} 
        and the data table is a json array "data":[[series1,series2,..],[series1...]]
    :csv:
        Comma Separated Values table.

&header={ **no** | yes }
........................

    Optional parameter specifies if a header is to be included to describe the 
    columns (series) included in the response. Default is **no**.

    For *&format=csv*, a row is prepended to the data with a comma delimited 
    list of the series names.

    For *&format=json*, the array "labels":[series1 [,series2 ....]] is added 
    to the response.  Another array "range":[begin, end] is added where begin
    and end are the 10 digit absolute unix begin and end times of the response.

&missing={ **null** | skip | zero}
..................................

    Optional parameter specifies what to do when a missing value is encountered 
    when building a response row.

    :null:
        Use the value null.

    :zero:
        Use the value zero.

    :skip:
        Suppress the entire response row.

&resolution={ **low** | high }
..............................

    Optional parameter specifies the relative resolution of the response table 
    when *&group=auto*. The default is **low**. For more information see 
    *&group=* above.

&limit={**n** | none }
......................

    Optional parameter overides the default output limit.
    The default is 1,000 lines.

    :n:
        Maximum lines generated

    :none:
        No limit, query runs to completion
    
    Query is a blocking request. 
    The IoTaWatt does not sample power while responding to a query.
    Short queries, as issued by Graph+, are of little consequence.
    They process in a second or less.
    To avoid unintended long lapses, a limit is placed on the  number
    of lines (groups) that are returned by the query.
    To understand the time required for longer queries,
    you can experiment with a subset and scale the time up.

    If the limit is reached, output will stop with a full line.
    If the format is json and header=yes,
    the response will include an object called "limit" 
    with a value of the UTC timestamp of the next line that
    would have been produced. 
    If the format is CSV, the following message will be appended with
    the UTC timestamp of the next line that would have been produced.

    ``Limit exceeded at <UTCtime>``
    

---------------
time specifiers 
---------------

A time specifier can define a date/time in absolute or relative terms.
Three different formats are allowed:

* `Unix time`_
* `ISO time`_
* `Relative time`_

Unix time
.........

Unix time is the count of seconds or milliseconds since Jan 1, 1970.  a Unix time 
specifier is simply a 10 digit integer for seconds 
or a 13 digit integer for milliseconds.  IoTaWatt will always round the 
time to a multiple of 5 seconds.

ISO time
........

A subset of the ISO 8601 standard can be used to specify an absolute date and time. 
The supported format is:

    ``YYYY [-MM [-DD [Thh [:mm [ :ss [Z]]]]]]``

As you can see, the only thing required is the year, which must be four digits.
That is optionally followed by:

    ``-MM``
        a two digit month 01-12

    ``-DD``
        a two digit day in month 01-31

    ``Thh``
        two digit hours 00-23

    ``:mm``
        two digit minutes 00-59

    ``:ss``
        two digit seconds 00-59

    ``Z``
        indicates the time is UTC rather than local time

Some examples are:

    2018-01-01
        Start of the year 2018, equal to 2018-01-01T00:00:00 or just 2018

    2019-04-15T11:42:15
        April 15, 2019 11:42:15

Relative time
.............

Specifies a point in time relative to the current time.
Makes it possible to specify "today", "yesterday", "last week" etc.
All relative time specifiers begin with a base date or time as follows:

Relative dates all begin at 00:00:00 local IoTaWatt time.

* y - Jan 1, of the current year
* M - The first day of the current month
* w - The first day of the current week (weeks start on Sunday)
* d - The current day

Relative time.

* h - first minute and second of the current hour.
* m - First second of the current minute.
* s - The current second (rounded down to 5 second multiple).

So if "today" is 2019-04-15T16:11:42:

    +-------+---------------------------+
    | Base  |  ISO time                 |
    +=======+===========================+
    |   y   | 2019-01-01T00:00:00       |
    +-------+---------------------------+
    |   M   | 2019-04-01T00:00:00       |
    +-------+---------------------------+
    |   w   | 2019-04-14T00:00:00       |
    +-------+---------------------------+
    |   d   | 2019-04-15T00:00:00       |
    +-------+---------------------------+
    |   h   | 2019-04-15T16:00:00       |
    +-------+---------------------------+
    |   m   | 2019-04-15T16:11:00       |
    +-------+---------------------------+
    |   s   | 2019-04-15T16:11:40       |
    +-------+---------------------------+

Base time may be followed by one or more offset modifiers to add or subtract from the
base time.  The format is:

    ``{ + | -} [n] { y | M | w | d | h | m | s }``

Examples:

+-----------------------+-----------------------------------+
|   Base with modifiers |   Effective time                  |
+=======================+===================================+
|d-1d                   |00:00:00 yesterday                 |
+-----------------------+-----------------------------------+
|d-18h                  |06:00:00 yesterday                 |
+-----------------------+-----------------------------------+
|s-3h                   |Three hours ago                    |
+-----------------------+-----------------------------------+
|y-1M                   |Last December                      |
+-----------------------+-----------------------------------+
|w-1w+3d+12h            |Noon on Wednesday of last week     |
+-----------------------+-----------------------------------+
|s                      |Now                                |
+-----------------------+-----------------------------------+

By using relative time for both **begin** and **end**, relative time periods can 
be specified:

+---------------+---------------+-------------------------------+
|begin          |end            |period                         |
+===============+===============+===============================+
|d-1d           |d              |yesterday                      |
+---------------+---------------+-------------------------------+
|M-1M           |M              |Last month                     |
+---------------+---------------+-------------------------------+
|d              |s              |Today to date                  |
+---------------+---------------+-------------------------------+
|s-12h          |s              |Last 12 hours                  |
+---------------+---------------+-------------------------------+
|w-1w+2d        |w-1w+3d        |Tuesday of last week           |
+---------------+---------------+-------------------------------+
|y              |s              |Year to date                   |
+---------------+---------------+-------------------------------+

---------
Responses
---------


400 invalid query.
..................

    The query has a missing or invalid specification.  The response is 
    a json object "error":"*<error details>*".

    query::

        HTTP:// ... /query?select=[time.iso,heap_pump,misc]&begin=d-1d&end=d&group=h

    response::

        {"error":"invalid query. Invalid series: heap_pump"}

200 Success
...........

    The query succeeded and the response is sent.

    :csv:
        Response is the table of csv formatted lines.

    query::

        /query?select=[time.iso,Heat_Pump,misc]&begin=d-1d&end=d&group=h&format=csv&header=yes

    response::

        Time, Heat_Pump, misc
        2019-10-16T00:00:00, 333, 125.5
        2019-10-16T01:00:00, 332.2, 121.4
        2019-10-16T02:00:00, 446.8, 116.8
        2019-10-16T03:00:00, 416.8, 114.3
        2019-10-16T04:00:00, 415.4, 109.9
        2019-10-16T05:00:00, 582.9, 111.4
        2019-10-16T06:00:00, 711.8, 113.3
        2019-10-16T07:00:00, 783.5, 117.1
        2019-10-16T08:00:00, 619.6, 117.5
        2019-10-16T09:00:00, 333, 116.4
        2019-10-16T10:00:00, 339.8, 164.5
        2019-10-16T11:00:00, 345.1, 180.6
        2019-10-16T12:00:00, 345.6, 114.5
        2019-10-16T13:00:00, 345.3, 111.8
        2019-10-16T14:00:00, 344.3, 130.9
        2019-10-16T15:00:00, 343.4, 302.5
        2019-10-16T16:00:00, 343.1, 271.6
        2019-10-16T17:00:00, 342, 264.5
        2019-10-16T18:00:00, 342.3, 114.1
        2019-10-16T19:00:00, 343, 117
        2019-10-16T20:00:00, 342.7, 118
        2019-10-16T21:00:00, 343.9, 136
        2019-10-16T22:00:00, 344.9, 120.2
        2019-10-16T23:00:00, 345.7, 124.2``
    
    :json:
        Response is a json object.

    query::

        HTTP:// ... /query?select=[time.iso,Heat_Pump,misc]&begin=d-1d&end=d&group=h&format=json&header=yes


    response::

        {"range":[1571198400,1571284800],
        "labels":["Time","Heat_Pump","misc"],
        "data":[["2019-10-16T00:00:00",333,125.5],
        ["2019-10-16T01:00:00",332.2,121.4],
        ["2019-10-16T02:00:00",446.8,116.8],
        ["2019-10-16T03:00:00",416.8,114.3],
        ["2019-10-16T04:00:00",415.4,109.9],
        ["2019-10-16T05:00:00",582.9,111.4],
        ["2019-10-16T06:00:00",711.8,113.3],
        ["2019-10-16T07:00:00",783.5,117.1],
        ["2019-10-16T08:00:00",619.6,117.5],
        ["2019-10-16T09:00:00",333,116.4],
        ["2019-10-16T10:00:00",339.8,164.5],
        ["2019-10-16T11:00:00",345.1,180.6],
        ["2019-10-16T12:00:00",345.6,114.5],
        ["2019-10-16T13:00:00",345.3,111.8],
        ["2019-10-16T14:00:00",344.3,130.9],
        ["2019-10-16T15:00:00",343.4,302.5],
        ["2019-10-16T16:00:00",343.1,271.6],
        ["2019-10-16T17:00:00",342,264.5],
        ["2019-10-16T18:00:00",342.3,114.1],
        ["2019-10-16T19:00:00",343,117],
        ["2019-10-16T20:00:00",342.7,118],
        ["2019-10-16T21:00:00",343.9,136],
        ["2019-10-16T22:00:00",344.9,120.2],
        ["2019-10-16T23:00:00",345.7,124.2]]}
    
    




