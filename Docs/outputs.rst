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
with calculations within parenthesis evaluated before being used. This means that it is essential for more complicated outputs that you use parenthesis to explicitly tell IotaWatt how it should do it's calculation. Working strictly left to right the two calculations below give different results.
    *   8 - 2 x 3 = 18
    *   8 - (2 x 3) = 8 - 6 = 2

So lets make an output channel that combines two main inputs called *main_1* 
and *main_2*. We enter the name total_power in the **Name:** box and hover 
over the |input| button of the calculator to see a list of the inputs.

.. image:: pics/selectInput.png
    :scale: 60 %
    :align: center
    :alt: Input Select Dropdown

Select main_1 from the list and it will appear in the 
calculator formula display.
Next click on the |plusKey|, then repeat the input process selecting Main_2. Our example is unambiguous and so doesn't need parenthesis.


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

Note the use of parenthesis in the calculation of 'misc' to ensure that the sum of the measured usage is subtracted from the total consumption.


Functions - MAX and MIN
-----------------------

MAX and MIN are binary operators and are used to yield the greater or lesser of the two operands they compare. They can be accessed using the FUNC button.

They work differently from traditional spreadsheet functions (which provide upper and lower limits). 

In IotaWatt A MAX B reports the Maximum of A and B. In mathematical terms MAX(A,B) would be another way of writing it. Similarly A MIN B reports the Minimum of A and B. For example:

    *   8 MAX 3 will compare 8 to 3 and returns 3
    *   Fred MAX 3 returns the value of 'Fred' if it is Greater than 3

    *   Fred MIN 0 reports the lower of the 'Fred' and Zero.
 
A practical example of when you could use MAX and MIN is if you have a Solar System and are importing electricity at night and potentially exporting it at peak sunshine. Assuming you have a CT on the Supplier's Incoming Cable called 'MAIN', disabling auto-reverse means that both positive and negative flows will be captured by IotaWatt. Assuming that +ve flows are input and -ve flows are exports then:

    *   Defining an output 'Imported' as MAIN MAX 0 will report the imported electricity (since anything positive is >0)
    *   Defining the output 'Exported' as MAIN MIN 0 will report the exported electricity (anything negative is <0)
    
    If you want your exported electricity to be reported as a positive number then use the ABS function. 




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
