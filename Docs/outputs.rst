=======
Outputs
=======

Outputs provide useful values that are computed from 
input channel values using a calculator like interface. 
For instance, in a typical US installation, there are two MAIN circuits, 
the sum of the two is the total power into a panel.  
Its nice to know at a glance what that total is, 
but the two mains are measured separately using two input channels.  
We need a way to add them together to display the total usage.

Hover over |Setup| and click |Outputs| in the dropdown buttons.


.. image:: pics/outputsDisplay.png
    :scale: 60 %
    :align: center
    :alt: Outputs Display

This screen will list any outputs that you have already configured, 
and allow you to click |add| to create new ones.  
You can click |edit| on existing outputs to change or delete them.  
There is no practical limit to the number of outputs that you may create.  
The only requirement is that they be uniquely named.


Adding a new Output
-------------------

So lets click |add|:

.. image:: pics/newOutput.png
    :scale: 60 %
    :align: center
    :alt: New Output

This is the *calculator* interface that IoTaWatt uses to specify 
how to calculate an output using input channel values.
A script is created that IoTaWatt uses to compute the value when 
needed.  
It works just like the simple four function calculators we are all used to, 
and using the |input| key, you can select 
input channel values to be used in the formula that you are creating. 
The resulting expression is evaluated left to right, 
with calculations within parenthesis evaluated before being used.


So lets make an output channel that combines two main inputs called *main_1* 
and *main_2*. We enter the name total_power in the **Name:** box and hover 
over the |input| button of the calculator to see a list of the inputs.

.. image:: pics/selectInput.png
    :scale: 60 %
    :align: center
    :alt: Input Select Dropdown

Select main_1 from the list and it will appear in the 
calculator formula display.
Next click on the |plusKey|, then repeat the input process selecting Main_2.


.. image:: pics/totalPowerOutput.png
    :scale: 60 %
    :align: center
    :alt: total_power output

Easy as that.  Now press |save| to return to the outputs list.  
Your new output should appear within a second or two.

.. image:: pics/outputsList.png
    :scale: 60 %
    :align: center
    :alt: outputs list

Now go back to the Channels Status screen and see that the new output channel 
is listed and indeed has a value that is the sum of the 
two inputs *main_1* and *main_2*.

.. image:: pics/outputsStatus.png
    :scale: 60 %
    :align: center
    :alt: outputs status

Some other useful outputs would be:

    *   Power used in a solar PV system, calculated by adding the 
        Solar inverter input to the (signed) Main input.
        If for instance the inverter were putting out 4500
        watts and your Main(s) indicated an outflow represented as -3100
        watts, local usage would be 1400 watts with 3100 watts exported.
        

    *   Where the Main(s) are monitored and selected circuits within the panel
        are also measured, you can create an output that shows the aggregate
        unmeasured usage by subtracting the measured inputs from the Mains as
        the *misc* output in the status display above.
        That output is defined:

.. image:: pics/miscOutput.png
    :scale: 60 %
    :align: center
    :alt: misc output

Functions - MAX and MIN
-----------------------

MAX and MIN are binary operators and are used to yield the greater or lesser of the two operands they compare. They can be accessed using the FUNC button.

They work differently from traditional spreadsheet functions (which provide upper and lower limits). 

In IotaWatt A MAX B reports the Maximum of A and B. Similarly A MIN B reports the Minimum of A and B. For example:

    *   Fred MAX 50 will compare 'Fred' to 50 and will return the value of 'Fred' if it is Greater than 50, or 50 if 'Fred' is less than 50.

    *   Fred MIN 0 reports the lower of the 'Fred' and Zero.
 
MAX and MIN are extremly useful if you have a Solar System and could import electricity at night and export it at peak sunshine.

*Example:*

    *   Connect a CT to the Supplier's Incoming Cable
    *   Define an input, call it 'MAIN'
    *   Disable auto-reverse on the input (this will mean that the both Negative and Positive Flows will be reported)
    *   Check that the CT is reporting positive when electricity is being imported and negative when electricity is being exported (if incorrect either physically re-orientate the CT or select the 'reverse' option on the input).
    *   Defining an output 'Imported' as MAIN MAX 0 will report the imported electricity (since anything positive is >0)
    *   Defining the output 'Exported' as MAIN MIN 0 will report the exported electricity (anything negative is <0)
    
    If you want your exported electricity to be reported as a positive number then use the ABS function (or multiply by -1)




.. |save| image:: pics/SaveButton.png
    :scale: 50 %
    :alt: **Save**
    
.. |Setup| image:: pics/SetupButton.png
    :scale: 60 %
    :alt: **Setup button**

.. |Outputs| image:: pics/outputsButton.png
    :scale: 60 %
    :alt: **Outputs button**

.. |plusKey| image:: pics/plusKey.png
    :scale: 50 %
    :alt: **Plus Key**

.. |input| image:: pics/inputKey.png
    :scale: 50 %
    :alt: **input Key**
    
.. |add| image:: pics/addButton.png
    :scale: 70 %
    :alt: **add button**

.. |edit| image:: pics/editButton.png
    :scale: 70 %
    :alt: **edit button**
